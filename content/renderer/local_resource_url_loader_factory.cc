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
#include "net/socket/socket.h"
#include "services/network/public/cpp/parsed_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/mojom/loader/local_resource_loader_config.mojom.h"
#include "ui/base/template_expressions.h"
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
  auto it = sources_->data.find(origin);
  // The renderer process may not have metadata for the data source. This can
  // happen if the data source isn't a WebUIDataSource, in which case the
  // browser process doesn't send metadata for it.
  // Example: chrome://theme/colors.css
  if (it == sources_->data.end()) {
    return false;
  }

  // Get the resource ID corresponding to the URL path.
  const blink::mojom::LocalResourceSourcePtr& source = it->second.source;
  std::string_view path = request.url.path_piece().substr(1);
  auto resource_it = source->path_to_resource_id_map.find(path);
  // The path-to-ID map may not have an entry for the given path. This can
  // happen for resources that are generated on-the-fly in the browser process.
  // Example: chrome://my-webui/strings.m.js
  if (resource_it == source->path_to_resource_id_map.end()) {
    return false;
  }
  int resource_id = resource_it->second;

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
  CHECK(request.url.scheme() == kChromeUIScheme);
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
  auto it = sources->data.find(origin);
  // CanServe should have been called before this point, which would have
  // confirmed that there exists a source corresponding to the URL origin.
  CHECK(it != sources->data.end());

  const blink::mojom::LocalResourceSourcePtr& source = it->second.source;
  const std::map<std::string, std::string>& replacement_strings =
      it->second.replacement_strings;

  // Get resource id.
  std::string_view path = request.url.path_piece().substr(1);
  auto resource_it = source->path_to_resource_id_map.find(path);
  // CanServe should have been called before this point, which would have
  // confirmed that there exists a resource ID corresponding to the URL path.
  CHECK(resource_it != source->path_to_resource_id_map.end());
  int resource_id = resource_it->second;

  // Load bytes.
  scoped_refptr<base::RefCountedMemory> raw_bytes =
      GetContentClient()->GetDataResourceBytes(resource_id);
  // CanServe should have been called before this point, which would have
  // confirmed that the ResourceBundle will return non-null for the given
  // resource ID.
  CHECK(raw_bytes);
  std::string_view bytes(base::as_string_view(*raw_bytes));

  auto url_response_head = network::mojom::URLResponseHead::New();

  // Mime type.
  std::string mime_type;
  if (net::GetMimeTypeFromFile(
          base::FilePath::FromASCII(request.url.ExtractFileName()),
          &mime_type)) {
    url_response_head->mime_type = mime_type;
  } else {
    url_response_head->mime_type = "text/html";
  }

  scoped_refptr<base::RefCountedMemory> bytes_after_replacement = raw_bytes;
  if (source->replacement_strings.size() > 0 &&
      (url_response_head->mime_type == "text/html" ||
       url_response_head->mime_type == "text/css" ||
       (source->should_replace_i18n_in_js &&
        url_response_head->mime_type == "text/javascript"))) {
    std::string replaced_string;
    if (url_response_head->mime_type == "text/javascript") {
      CHECK(ui::ReplaceTemplateExpressionsInJS(bytes, replacement_strings,
                                               &replaced_string));
    } else {
      replaced_string =
          ui::ReplaceTemplateExpressions(bytes, replacement_strings);
    }
    bytes_after_replacement = base::MakeRefCounted<base::RefCountedString>(
        std::move(replaced_string));
  }

  // Other headers.
  scoped_refptr<net::HttpResponseHeaders> headers =
      base::MakeRefCounted<net::HttpResponseHeaders>(source->headers);
  headers->SetHeader(net::HttpRequestHeaders::kContentType, mime_type);
  url_response_head->headers = headers;
  url_response_head->parsed_headers = network::PopulateParsedHeaders(
      url_response_head->headers.get(), request.url);

  // Handle Range header if request.
  base::expected<net::HttpByteRange, webui::GetRequestedRangeError>
      range_or_error = webui::GetRequestedRange(request.headers);
  // Errors (aside from 'no Range header') should be surfaced to the client.
  if (!range_or_error.has_value() &&
      range_or_error.error() != webui::GetRequestedRangeError::kNoRanges) {
    webui::CallOnError(std::move(client),
                       net::ERR_REQUEST_RANGE_NOT_SATISFIABLE);
    return;
  }
  std::optional<net::HttpByteRange> maybe_range =
      range_or_error.has_value() ? std::make_optional(range_or_error.value())
                                 : std::nullopt;

  webui::SendData(std::move(url_response_head), std::move(client), maybe_range,
                  bytes_after_replacement);
}

}  // namespace content
