// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/local_resource_url_loader_factory.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_view_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "content/common/web_ui_loading_util.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/mime_util.h"
#include "net/http/http_response_headers.h"
#include "net/socket/socket.h"
#include "services/network/public/cpp/parsed_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/mojom/loader/local_resource_loader_config.mojom.h"
#include "ui/base/template_expressions.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "url/origin.h"

namespace content {

namespace {

std::map<url::Origin, LocalResourceURLLoaderFactory::Source>
ConvertConfigToSourcesMap(blink::mojom::LocalResourceLoaderConfigPtr config) {
  std::map<url::Origin, LocalResourceURLLoaderFactory::Source> sources;
  // TODO(https://crbug.com/384765582) This manual copy is only necessary
  // because ui::ReplaceTemplateExpressions uses an unconventional map type.
  // Remove this when that is fixed.
  for (const auto& source : config->sources) {
    const url::Origin origin = source.first;
    const blink::mojom::LocalResourceSourcePtr& mojo_source = source.second;
    const std::map<std::string, std::string> replacement_strings(
        mojo_source->replacement_strings.begin(),
        mojo_source->replacement_strings.end());
    LocalResourceURLLoaderFactory::Source local_source(
        mojo_source.Clone(), std::move(replacement_strings));
    sources.insert({std::move(origin), std::move(local_source)});
  }
  return sources;
}

// Returns the mime type of the given URL. If the mime type cannot be
// determined, returns "text/html".
std::string GetMimeType(const GURL& url) {
  std::string mime_type;
  if (net::GetMimeTypeFromFile(base::FilePath::FromASCII(url.ExtractFileName()),
                               &mime_type)) {
    return mime_type;
  }
  return "text/html";
}

// Returns the content of the response for `path` in `source` if found.
// Note: This only searches for `response_body`, not `resource_id`.
std::optional<std::string_view> FindResponseInSource(
    const LocalResourceURLLoaderFactory::Source& source,
    std::string_view path) {
  if (auto it = source.source->path_to_resource_map.find(std::string(path));
      it != source.source->path_to_resource_map.end()) {
    if (it->second->is_response_body()) {
      return it->second->get_response_body();
    }
  }
  return std::nullopt;
}

// Searches for a response in `sources`.
// Checks the source corresponding to `origin` for `url.path()`. Returns the
// response content and headers if found.
// Note: This only searches for `response_body`, not `resource_id`.
std::optional<std::pair<std::string_view, std::string_view>> FindResponse(
    const std::map<url::Origin, LocalResourceURLLoaderFactory::Source>& sources,
    const url::Origin& origin,
    const GURL& url) {
  // Check if the source corresponding to `origin` has the URL path in its
  // path_to_response_map.
  if (auto it = sources.find(origin); it != sources.end()) {
    if (auto response_opt =
            FindResponseInSource(it->second, url.path().substr(1))) {
      return std::make_pair(*response_opt,
                            std::string_view(it->second.source->headers));
    }
  }
  return std::nullopt;
}

// Sends a response with the given `content`, `headers_str`, and `mime_type` to
// `client`.
void SendResponse(mojo::PendingRemote<network::mojom::URLLoaderClient> client,
                  std::string_view content,
                  std::string_view headers_str,
                  std::string_view mime_type) {
  auto url_response_head = network::mojom::URLResponseHead::New();
  url_response_head->mime_type = std::string(mime_type);
  scoped_refptr<net::HttpResponseHeaders> headers =
      base::MakeRefCounted<net::HttpResponseHeaders>(std::string(headers_str));
  headers->SetHeader(net::HttpRequestHeaders::kContentType,
                     url_response_head->mime_type);
  url_response_head->headers = headers;

  scoped_refptr<base::RefCountedString> bytes =
      base::MakeRefCounted<base::RefCountedString>(std::string(content));

  webui::SendData(std::move(url_response_head), std::move(client), std::nullopt,
                  bytes);
}

// Returns the bytes of the resource with the given `value`.
// This function assumes that the resource value is a resource ID.
scoped_refptr<base::RefCountedMemory> GetResourceBytes(
    const blink::mojom::LocalResourceValuePtr& value) {
  // response body must be handled by `FindResponse()`.
  CHECK(value->is_resource_id());
  int resource_id = value->get_resource_id();
  return GetContentClient()->GetDataResourceBytes(resource_id);
}

}  // namespace

LocalResourceURLLoaderFactory::Source::Source(
    blink::mojom::LocalResourceSourcePtr source,
    std::map<std::string, std::string> replacement_strings)
    : source(std::move(source)),
      replacement_strings(std::move(replacement_strings)) {}

LocalResourceURLLoaderFactory::Source::Source(Source&& other) = default;
LocalResourceURLLoaderFactory::Source&
LocalResourceURLLoaderFactory::Source::operator=(Source&& other) = default;

LocalResourceURLLoaderFactory::Source::~Source() = default;

LocalResourceURLLoaderFactory::LocalResourceURLLoaderFactory(
    blink::mojom::LocalResourceLoaderConfigPtr config,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> fallback)
    : sources_(base::MakeRefCounted<
               base::RefCountedData<std::map<url::Origin, Source>>>(
          ConvertConfigToSourcesMap(std::move(config)))),
      fallback_(std::move(fallback)) {}

LocalResourceURLLoaderFactory::~LocalResourceURLLoaderFactory() = default;

bool LocalResourceURLLoaderFactory::CanServe(
    const network::ResourceRequest& request) const {
  const url::Origin origin = url::Origin::Create(request.url);

  // Check if we have a direct response for this URL.
  // This may include strings.m.js or chrome://theme/colors.css, depending on
  // how the source is configured in browser.
  if (FindResponse(sources_->data, origin, request.url).has_value()) {
    return true;
  }

  auto it = sources_->data.find(origin);
  if (it == sources_->data.end()) {
    return false;
  }

  // Get the resource ID corresponding to the URL path.
  const blink::mojom::LocalResourceSourcePtr& source = it->second.source;
  std::string_view path = request.url.path().substr(1);
  auto resource_it = source->path_to_resource_map.find(std::string(path));
  // The path-to-ID map may not have an entry for the given path. This can
  // happen for resources that are generated on-the-fly in the browser process.
  // Example: chrome://my-webui/strings.m.js
  if (resource_it == source->path_to_resource_map.end()) {
    return false;
  }

  // If the resource value is a response body, it should have been handled by
  // FindResponse() above.
  CHECK(resource_it->second->is_resource_id());
  int resource_id = resource_it->second->get_resource_id();

  // Return true if the in-process ResourceBundle has the resource for this ID.
  return GetContentClient()->HasDataResource(resource_id);
}

void LocalResourceURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> loader,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  CHECK(fallback_);
  if (!CanServe(request)) {
    fallback_->CreateLoaderAndStart(std::move(loader), request_id, options,
                                    request, std::move(client),
                                    traffic_annotation);
    return;
  }
  // Only the "chrome" scheme is supported.
  CHECK(request.url.GetScheme() == kChromeUIScheme);
  // Parallelize calls to GetResourceAndRespond across multiple threads.
  // Needs to be posted to a SequencedTaskRunner as Mojo requires a
  // SequencedTaskRunner::CurrentDefaultHandle in scope.
  base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::USER_BLOCKING, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})
      ->PostTask(FROM_HERE, base::BindOnce(GetResourceAndRespond, sources_,
                                           request, std::move(client)));
}

void LocalResourceURLLoaderFactory::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  receivers_.Add(this, std::move(receiver));
}

// static
void LocalResourceURLLoaderFactory::GetResourceAndRespond(
    const scoped_refptr<base::RefCountedData<std::map<url::Origin, Source>>>
        sources,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  const url::Origin origin = url::Origin::Create(request.url);

  // Check if we have a direct response for this URL.
  if (auto match = FindResponse(sources->data, origin, request.url)) {
    SendResponse(std::move(client), match->first, match->second,
                 GetMimeType(request.url));
    return;
  }

  auto it = sources->data.find(origin);
  CHECK(it != sources->data.end());

  const blink::mojom::LocalResourceSourcePtr& source = it->second.source;
  const std::map<std::string, std::string>& replacement_strings =
      it->second.replacement_strings;

  // Mime type.
  auto url_response_head = network::mojom::URLResponseHead::New();
  url_response_head->mime_type = GetMimeType(request.url);
  std::string mime_type = url_response_head->mime_type;

  // Other headers.
  scoped_refptr<net::HttpResponseHeaders> headers =
      base::MakeRefCounted<net::HttpResponseHeaders>(source->headers);
  headers->SetHeader(net::HttpRequestHeaders::kContentType, mime_type);
  url_response_head->headers = headers;
  url_response_head->parsed_headers = network::PopulateParsedHeaders(
      url_response_head->headers.get(), request.url);

  // Handle Range header if request.
  std::optional<net::HttpByteRange> maybe_range = std::nullopt;
  base::expected<net::HttpByteRange, webui::GetRequestedRangeError>
      range_or_error = webui::GetRequestedRange(request.headers);
  // Errors (aside from 'no Range header') should be surfaced to the client.
  if (!range_or_error.has_value() &&
      range_or_error.error() != webui::GetRequestedRangeError::kNoRanges) {
    webui::CallOnError(std::move(client),
                       net::ERR_REQUEST_RANGE_NOT_SATISFIABLE);
    return;
  }

  if (range_or_error.has_value()) {
    maybe_range = range_or_error.value();
  }

  webui::SendData(
      std::move(url_response_head), std::move(client), maybe_range,
      GetResource(request.url, source, replacement_strings, mime_type));
}

// static
scoped_refptr<base::RefCountedMemory>
LocalResourceURLLoaderFactory::GetResource(
    const GURL& url,
    const blink::mojom::LocalResourceSourcePtr& source,
    const std::map<std::string, std::string>& replacement_strings,
    const std::string& mime_type) {
  // Get resource.
  std::string_view path = url.path().substr(1);
  auto resource_it = source->path_to_resource_map.find(std::string(path));
  // CanServe should have been called before this point, which would have
  // confirmed that there exists a resource corresponding to the URL path.
  CHECK(resource_it != source->path_to_resource_map.end());
  if (resource_it->second->is_response_body()) {
    // The resource is a direct response. Note that this should already be
    // handled earlier for callers from `GetResourceAndRespond()`, so this path
    // is only for direct callers for `GetResource()`.
    return base::MakeRefCounted<base::RefCountedString>(
        resource_it->second->get_response_body());
  }

  // Load bytes using a resource ID.
  scoped_refptr<base::RefCountedMemory> raw_bytes =
      GetResourceBytes(resource_it->second);
  // CanServe should have been called before this point, which would have
  // confirmed that the ResourceBundle will return non-null for the given
  // resource ID.
  CHECK(raw_bytes);
  std::string_view bytes(base::as_string_view(*raw_bytes));

  scoped_refptr<base::RefCountedMemory> bytes_after_replacement = raw_bytes;
  if (replacement_strings.size() > 0 &&
      (mime_type == "text/html" || mime_type == "text/css" ||
       (source->should_replace_i18n_in_js && mime_type == "text/javascript"))) {
    std::string replaced_string;
    if (mime_type == "text/javascript") {
      CHECK(ui::ReplaceTemplateExpressionsInJS(bytes, replacement_strings,
                                               &replaced_string));
    } else {
      replaced_string =
          ui::ReplaceTemplateExpressions(bytes, replacement_strings);
    }
    bytes_after_replacement = base::MakeRefCounted<base::RefCountedString>(
        std::move(replaced_string));
  }

  return bytes_after_replacement;
}

}  // namespace content
