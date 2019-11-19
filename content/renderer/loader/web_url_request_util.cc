// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/loader/web_url_request_util.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "content/child/child_thread_impl.h"
#include "content/public/common/service_names.mojom.h"
#include "content/renderer/loader/request_extra_data.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/data_pipe_getter.mojom.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/blob/blob_registry.mojom.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_http_body.h"
#include "third_party/blink/public/platform/web_http_header_visitor.h"
#include "third_party/blink/public/platform/web_mixed_content.h"
#include "third_party/blink/public/platform/web_string.h"

using blink::mojom::FetchCacheMode;
using blink::WebData;
using blink::WebHTTPBody;
using blink::WebString;
using blink::WebURLRequest;

namespace content {

namespace {

std::string TrimLWSAndCRLF(const base::StringPiece& input) {
  base::StringPiece string = net::HttpUtil::TrimLWS(input);
  const char* begin = string.data();
  const char* end = string.data() + string.size();
  while (begin < end && (end[-1] == '\r' || end[-1] == '\n'))
    --end;
  return std::string(base::StringPiece(begin, end - begin));
}

class HttpRequestHeadersVisitor : public blink::WebHTTPHeaderVisitor {
 public:
  explicit HttpRequestHeadersVisitor(net::HttpRequestHeaders* headers)
      : headers_(headers) {}
  ~HttpRequestHeadersVisitor() override = default;

  void VisitHeader(const WebString& name, const WebString& value) override {
    std::string name_latin1 = name.Latin1();
    std::string value_latin1 = TrimLWSAndCRLF(value.Latin1());

    // Skip over referrer headers found in the header map because we already
    // pulled it out as a separate parameter.
    if (base::LowerCaseEqualsASCII(name_latin1, "referer"))
      return;

    DCHECK(net::HttpUtil::IsValidHeaderName(name_latin1)) << name_latin1;
    DCHECK(net::HttpUtil::IsValidHeaderValue(value_latin1)) << value_latin1;
    headers_->SetHeader(name_latin1, value_latin1);
  }

 private:
  net::HttpRequestHeaders* const headers_;
};

class HeaderFlattener : public blink::WebHTTPHeaderVisitor {
 public:
  HeaderFlattener() {}
  ~HeaderFlattener() override {}

  void VisitHeader(const WebString& name, const WebString& value) override {
    // Headers are latin1.
    const std::string& name_latin1 = name.Latin1();
    const std::string& value_latin1 = value.Latin1();

    // Skip over referrer headers found in the header map because we already
    // pulled it out as a separate parameter.
    if (base::LowerCaseEqualsASCII(name_latin1, "referer"))
      return;

    if (!buffer_.empty())
      buffer_.append("\r\n");
    buffer_.append(name_latin1 + ": " + value_latin1);
  }

  const std::string& GetBuffer() const {
    return buffer_;
  }

 private:
  std::string buffer_;
};

}  // namespace

ResourceType RequestContextToResourceType(
    blink::mojom::RequestContextType request_context) {
  switch (request_context) {
    // CSP report
    case blink::mojom::RequestContextType::CSP_REPORT:
      return ResourceType::kCspReport;

    // Favicon
    case blink::mojom::RequestContextType::FAVICON:
      return ResourceType::kFavicon;

    // Font
    case blink::mojom::RequestContextType::FONT:
      return ResourceType::kFontResource;

    // Image
    case blink::mojom::RequestContextType::IMAGE:
    case blink::mojom::RequestContextType::IMAGE_SET:
      return ResourceType::kImage;

    // Media
    case blink::mojom::RequestContextType::AUDIO:
    case blink::mojom::RequestContextType::VIDEO:
      return ResourceType::kMedia;

    // Object
    case blink::mojom::RequestContextType::EMBED:
    case blink::mojom::RequestContextType::OBJECT:
      return ResourceType::kObject;

    // Ping
    case blink::mojom::RequestContextType::BEACON:
    case blink::mojom::RequestContextType::PING:
      return ResourceType::kPing;

    // Subresource of plugins
    case blink::mojom::RequestContextType::PLUGIN:
      return ResourceType::kPluginResource;

    // Prefetch
    case blink::mojom::RequestContextType::PREFETCH:
      return ResourceType::kPrefetch;

    // Script
    case blink::mojom::RequestContextType::IMPORT:
    case blink::mojom::RequestContextType::SCRIPT:
      return ResourceType::kScript;

    // Style
    case blink::mojom::RequestContextType::XSLT:
    case blink::mojom::RequestContextType::STYLE:
      return ResourceType::kStylesheet;

    // Subresource
    case blink::mojom::RequestContextType::DOWNLOAD:
    case blink::mojom::RequestContextType::MANIFEST:
    case blink::mojom::RequestContextType::SUBRESOURCE:
      return ResourceType::kSubResource;

    // TextTrack
    case blink::mojom::RequestContextType::TRACK:
      return ResourceType::kMedia;

    // Workers
    case blink::mojom::RequestContextType::SERVICE_WORKER:
      return ResourceType::kServiceWorker;
    case blink::mojom::RequestContextType::SHARED_WORKER:
      return ResourceType::kSharedWorker;
    case blink::mojom::RequestContextType::WORKER:
      return ResourceType::kWorker;

    // Unspecified
    case blink::mojom::RequestContextType::INTERNAL:
    case blink::mojom::RequestContextType::UNSPECIFIED:
      return ResourceType::kSubResource;

    // XHR
    case blink::mojom::RequestContextType::EVENT_SOURCE:
    case blink::mojom::RequestContextType::FETCH:
    case blink::mojom::RequestContextType::XML_HTTP_REQUEST:
      return ResourceType::kXhr;

    // Navigation requests should not go through WebURLLoader.
    case blink::mojom::RequestContextType::FORM:
    case blink::mojom::RequestContextType::HYPERLINK:
    case blink::mojom::RequestContextType::LOCATION:
    case blink::mojom::RequestContextType::FRAME:
    case blink::mojom::RequestContextType::IFRAME:
      NOTREACHED();
      return ResourceType::kSubResource;

    default:
      NOTREACHED();
      return ResourceType::kSubResource;
  }
}

ResourceType WebURLRequestToResourceType(const WebURLRequest& request) {
  return RequestContextToResourceType(request.GetRequestContext());
}

net::HttpRequestHeaders GetWebURLRequestHeaders(
    const blink::WebURLRequest& request) {
  net::HttpRequestHeaders headers;
  HttpRequestHeadersVisitor visitor(&headers);
  request.VisitHttpHeaderFields(&visitor);
  return headers;
}

std::string GetWebURLRequestHeadersAsString(
    const blink::WebURLRequest& request) {
  HeaderFlattener flattener;
  request.VisitHttpHeaderFields(&flattener);
  return flattener.GetBuffer();
}

WebHTTPBody GetWebHTTPBodyForRequestBody(
    const network::ResourceRequestBody& input) {
  WebHTTPBody http_body;
  http_body.Initialize();
  http_body.SetIdentifier(input.identifier());
  http_body.SetContainsPasswordData(input.contains_sensitive_info());
  for (auto& element : *input.elements()) {
    switch (element.type()) {
      case network::mojom::DataElementType::kBytes:
        http_body.AppendData(WebData(element.bytes(), element.length()));
        break;
      case network::mojom::DataElementType::kFile:
        http_body.AppendFileRange(
            blink::FilePathToWebString(element.path()), element.offset(),
            (element.length() != std::numeric_limits<uint64_t>::max())
                ? element.length()
                : -1,
            element.expected_modification_time().ToDoubleT());
        break;
      case network::mojom::DataElementType::kBlob:
          http_body.AppendBlob(WebString::FromASCII(element.blob_uuid()));
        break;
      case network::mojom::DataElementType::kDataPipe: {
        http_body.AppendDataPipe(element.CloneDataPipeGetter().PassPipe());
        break;
      }
      case network::mojom::DataElementType::kUnknown:
      case network::mojom::DataElementType::kRawFile:
      case network::mojom::DataElementType::kChunkedDataPipe:
        NOTREACHED();
        break;
    }
  }
  return http_body;
}

scoped_refptr<network::ResourceRequestBody> GetRequestBodyForWebURLRequest(
    const WebURLRequest& request) {
  scoped_refptr<network::ResourceRequestBody> request_body;

  if (request.HttpBody().IsNull()) {
    return request_body;
  }

  const std::string& method = request.HttpMethod().Latin1();
  // GET and HEAD requests shouldn't have http bodies.
  DCHECK(method != "GET" && method != "HEAD");

  return GetRequestBodyForWebHTTPBody(request.HttpBody());
}

scoped_refptr<network::ResourceRequestBody> GetRequestBodyForWebHTTPBody(
    const blink::WebHTTPBody& httpBody) {
  scoped_refptr<network::ResourceRequestBody> request_body =
      new network::ResourceRequestBody();
  size_t i = 0;
  WebHTTPBody::Element element;
  while (httpBody.ElementAt(i++, element)) {
    switch (element.type) {
      case WebHTTPBody::Element::kTypeData:
        request_body->AppendBytes(element.data.Copy().ReleaseVector());
        break;
      case WebHTTPBody::Element::kTypeFile:
        if (element.file_length == -1) {
          request_body->AppendFileRange(
              blink::WebStringToFilePath(element.file_path), 0,
              std::numeric_limits<uint64_t>::max(), base::Time());
        } else {
          request_body->AppendFileRange(
              blink::WebStringToFilePath(element.file_path),
              static_cast<uint64_t>(element.file_start),
              static_cast<uint64_t>(element.file_length),
              base::Time::FromDoubleT(element.modification_time));
        }
        break;
      case WebHTTPBody::Element::kTypeBlob: {
        DCHECK(element.optional_blob_handle.is_valid());
        mojo::Remote<blink::mojom::Blob> blob_remote(
            mojo::PendingRemote<blink::mojom::Blob>(
                std::move(element.optional_blob_handle),
                blink::mojom::Blob::Version_));

        mojo::PendingRemote<network::mojom::DataPipeGetter>
            data_pipe_getter_remote;
        blob_remote->AsDataPipeGetter(
            data_pipe_getter_remote.InitWithNewPipeAndPassReceiver());

        request_body->AppendDataPipe(std::move(data_pipe_getter_remote));
        break;
      }
      case WebHTTPBody::Element::kTypeDataPipe: {
        // Convert the raw message pipe to
        // mojo::Remote<network::mojom::DataPipeGetter> data_pipe_getter.
        mojo::Remote<network::mojom::DataPipeGetter> data_pipe_getter(
            mojo::PendingRemote<network::mojom::DataPipeGetter>(
                std::move(element.data_pipe_getter), 0u));

        // Set the cloned DataPipeGetter to the output |request_body|, while
        // keeping the original message pipe back in the input |httpBody|. This
        // way the consumer of the |httpBody| can retrieve the data pipe
        // multiple times (e.g. during redirects) until the request is finished.
        mojo::PendingRemote<network::mojom::DataPipeGetter> cloned_getter;
        data_pipe_getter->Clone(cloned_getter.InitWithNewPipeAndPassReceiver());
        request_body->AppendDataPipe(std::move(cloned_getter));
        element.data_pipe_getter = data_pipe_getter.Unbind().PassPipe();
        break;
      }
    }
  }
  request_body->set_identifier(httpBody.Identifier());
  request_body->set_contains_sensitive_info(httpBody.ContainsPasswordData());
  return request_body;
}

#define STATIC_ASSERT_ENUM(a, b)                            \
  static_assert(static_cast<int>(a) == static_cast<int>(b), \
                "mismatching enums: " #a)

std::string GetFetchIntegrityForWebURLRequest(const WebURLRequest& request) {
  return request.GetFetchIntegrity().Utf8();
}

blink::mojom::RequestContextType GetRequestContextTypeForWebURLRequest(
    const WebURLRequest& request) {
  return static_cast<blink::mojom::RequestContextType>(
      request.GetRequestContext());
}

blink::WebMixedContentContextType GetMixedContentContextTypeForWebURLRequest(
    const WebURLRequest& request) {
  return blink::WebMixedContent::ContextTypeFromRequestContext(
      request.GetRequestContext(),
      /*strict_mixed_content_checking_for_plugin=*/false);
}

#undef STATIC_ASSERT_ENUM

}  // namespace content
