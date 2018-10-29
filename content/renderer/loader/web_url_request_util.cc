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
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/data_pipe_getter.mojom.h"
#include "services/network/public/mojom/request_context_frame_type.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/blob/blob_registry.mojom.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/public/platform/modules/fetch/fetch_api_request.mojom.h"
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
      return RESOURCE_TYPE_CSP_REPORT;

    // Favicon
    case blink::mojom::RequestContextType::FAVICON:
      return RESOURCE_TYPE_FAVICON;

    // Font
    case blink::mojom::RequestContextType::FONT:
      return RESOURCE_TYPE_FONT_RESOURCE;

    // Image
    case blink::mojom::RequestContextType::IMAGE:
    case blink::mojom::RequestContextType::IMAGE_SET:
      return RESOURCE_TYPE_IMAGE;

    // Media
    case blink::mojom::RequestContextType::AUDIO:
    case blink::mojom::RequestContextType::VIDEO:
      return RESOURCE_TYPE_MEDIA;

    // Object
    case blink::mojom::RequestContextType::EMBED:
    case blink::mojom::RequestContextType::OBJECT:
      return RESOURCE_TYPE_OBJECT;

    // Ping
    case blink::mojom::RequestContextType::BEACON:
    case blink::mojom::RequestContextType::PING:
      return RESOURCE_TYPE_PING;

    // Subresource of plugins
    case blink::mojom::RequestContextType::PLUGIN:
      return RESOURCE_TYPE_PLUGIN_RESOURCE;

    // Prefetch
    case blink::mojom::RequestContextType::PREFETCH:
      return RESOURCE_TYPE_PREFETCH;

    // Script
    case blink::mojom::RequestContextType::IMPORT:
    case blink::mojom::RequestContextType::SCRIPT:
      return RESOURCE_TYPE_SCRIPT;

    // Style
    case blink::mojom::RequestContextType::XSLT:
    case blink::mojom::RequestContextType::STYLE:
      return RESOURCE_TYPE_STYLESHEET;

    // Subresource
    case blink::mojom::RequestContextType::DOWNLOAD:
    case blink::mojom::RequestContextType::MANIFEST:
    case blink::mojom::RequestContextType::SUBRESOURCE:
      return RESOURCE_TYPE_SUB_RESOURCE;

    // TextTrack
    case blink::mojom::RequestContextType::TRACK:
      return RESOURCE_TYPE_MEDIA;

    // Workers
    case blink::mojom::RequestContextType::SERVICE_WORKER:
      return RESOURCE_TYPE_SERVICE_WORKER;
    case blink::mojom::RequestContextType::SHARED_WORKER:
      return RESOURCE_TYPE_SHARED_WORKER;
    case blink::mojom::RequestContextType::WORKER:
      return RESOURCE_TYPE_WORKER;

    // Unspecified
    case blink::mojom::RequestContextType::INTERNAL:
    case blink::mojom::RequestContextType::UNSPECIFIED:
      return RESOURCE_TYPE_SUB_RESOURCE;

    // XHR
    case blink::mojom::RequestContextType::EVENT_SOURCE:
    case blink::mojom::RequestContextType::FETCH:
    case blink::mojom::RequestContextType::XML_HTTP_REQUEST:
      return RESOURCE_TYPE_XHR;

    // These should be handled by the FrameType checks at the top of the
    // function.
    case blink::mojom::RequestContextType::FORM:
    case blink::mojom::RequestContextType::HYPERLINK:
    case blink::mojom::RequestContextType::LOCATION:
    case blink::mojom::RequestContextType::FRAME:
    case blink::mojom::RequestContextType::IFRAME:
      NOTREACHED();
      return RESOURCE_TYPE_SUB_RESOURCE;

    default:
      NOTREACHED();
      return RESOURCE_TYPE_SUB_RESOURCE;
  }
}

ResourceType WebURLRequestToResourceType(const WebURLRequest& request) {
  blink::mojom::RequestContextType request_context =
      request.GetRequestContext();
  if (request.GetFrameType() !=
      network::mojom::RequestContextFrameType::kNone) {
    DCHECK(request_context == blink::mojom::RequestContextType::FORM ||
           request_context == blink::mojom::RequestContextType::FRAME ||
           request_context == blink::mojom::RequestContextType::HYPERLINK ||
           request_context == blink::mojom::RequestContextType::IFRAME ||
           request_context == blink::mojom::RequestContextType::INTERNAL ||
           request_context == blink::mojom::RequestContextType::LOCATION);
    if (request.GetFrameType() ==
            network::mojom::RequestContextFrameType::kTopLevel ||
        request.GetFrameType() ==
            network::mojom::RequestContextFrameType::kAuxiliary) {
      return RESOURCE_TYPE_MAIN_FRAME;
    }
    if (request.GetFrameType() ==
        network::mojom::RequestContextFrameType::kNested)
      return RESOURCE_TYPE_SUB_FRAME;
    NOTREACHED();
    return RESOURCE_TYPE_SUB_RESOURCE;
  }
  return RequestContextToResourceType(request_context);
}

net::HttpRequestHeaders GetWebURLRequestHeaders(
    const blink::WebURLRequest& request) {
  net::HttpRequestHeaders headers;
  HttpRequestHeadersVisitor visitor(&headers);
  request.VisitHTTPHeaderFields(&visitor);
  return headers;
}

std::string GetWebURLRequestHeadersAsString(
    const blink::WebURLRequest& request) {
  HeaderFlattener flattener;
  request.VisitHTTPHeaderFields(&flattener);
  return flattener.GetBuffer();
}

int GetLoadFlagsForWebURLRequest(const WebURLRequest& request) {
  int load_flags = net::LOAD_NORMAL;

  GURL url = request.Url();
  switch (request.GetCacheMode()) {
    case FetchCacheMode::kNoStore:
      load_flags |= net::LOAD_DISABLE_CACHE;
      break;
    case FetchCacheMode::kValidateCache:
      load_flags |= net::LOAD_VALIDATE_CACHE;
      break;
    case FetchCacheMode::kBypassCache:
      load_flags |= net::LOAD_BYPASS_CACHE;
      break;
    case FetchCacheMode::kForceCache:
      load_flags |= net::LOAD_SKIP_CACHE_VALIDATION;
      break;
    case FetchCacheMode::kOnlyIfCached:
      load_flags |= net::LOAD_ONLY_FROM_CACHE | net::LOAD_SKIP_CACHE_VALIDATION;
      break;
    case FetchCacheMode::kUnspecifiedOnlyIfCachedStrict:
      load_flags |= net::LOAD_ONLY_FROM_CACHE;
      break;
    case FetchCacheMode::kDefault:
      break;
    case FetchCacheMode::kUnspecifiedForceCacheMiss:
      load_flags |= net::LOAD_ONLY_FROM_CACHE | net::LOAD_BYPASS_CACHE;
      break;
  }

  if (!request.AllowStoredCredentials()) {
    load_flags |= net::LOAD_DO_NOT_SAVE_COOKIES;
    load_flags |= net::LOAD_DO_NOT_SEND_COOKIES;
    load_flags |= net::LOAD_DO_NOT_SEND_AUTH_DATA;
  }

  if (request.GetRequestContext() == blink::mojom::RequestContextType::PREFETCH)
    load_flags |= net::LOAD_PREFETCH;

  if (request.GetExtraData()) {
    RequestExtraData* extra_data =
        static_cast<RequestExtraData*>(request.GetExtraData());
    if (extra_data->is_for_no_state_prefetch())
      load_flags |= net::LOAD_PREFETCH;
  }
  if (request.SupportsAsyncRevalidation())
    load_flags |= net::LOAD_SUPPORT_ASYNC_REVALIDATION;

  return load_flags;
}

WebHTTPBody GetWebHTTPBodyForRequestBody(
    const network::ResourceRequestBody& input) {
  return GetWebHTTPBodyForRequestBodyWithBlobPtrs(input, {});
}

WebHTTPBody GetWebHTTPBodyForRequestBodyWithBlobPtrs(
    const network::ResourceRequestBody& input,
    std::vector<blink::mojom::BlobPtrInfo> blob_ptrs) {
  WebHTTPBody http_body;
  http_body.Initialize();
  http_body.SetIdentifier(input.identifier());
  http_body.SetContainsPasswordData(input.contains_sensitive_info());
  auto blob_ptr_iter = blob_ptrs.begin();
  for (auto& element : *input.elements()) {
    switch (element.type()) {
      case network::DataElement::TYPE_BYTES:
        http_body.AppendData(WebData(element.bytes(), element.length()));
        break;
      case network::DataElement::TYPE_FILE:
        http_body.AppendFileRange(
            blink::FilePathToWebString(element.path()), element.offset(),
            (element.length() != std::numeric_limits<uint64_t>::max())
                ? element.length()
                : -1,
            element.expected_modification_time().ToDoubleT());
        break;
      case network::DataElement::TYPE_BLOB:
        if (blob_ptrs.empty()) {
          http_body.AppendBlob(WebString::FromASCII(element.blob_uuid()));
        } else {
          DCHECK(blob_ptr_iter != blob_ptrs.end());
          blink::mojom::BlobPtrInfo& blob = *blob_ptr_iter++;
          http_body.AppendBlob(WebString::FromASCII(element.blob_uuid()),
                               element.length(), blob.PassHandle());
        }
        break;
      case network::DataElement::TYPE_DATA_PIPE: {
        http_body.AppendDataPipe(
            element.CloneDataPipeGetter().PassInterface().PassHandle());
        break;
      }
      case network::DataElement::TYPE_UNKNOWN:
      case network::DataElement::TYPE_RAW_FILE:
      case network::DataElement::TYPE_CHUNKED_DATA_PIPE:
        NOTREACHED();
        break;
    }
  }
  return http_body;
}

std::vector<blink::mojom::BlobPtrInfo> GetBlobPtrsForRequestBody(
    const network::ResourceRequestBody& input) {
  std::vector<blink::mojom::BlobPtrInfo> blob_ptrs;
  blink::mojom::BlobRegistryPtr blob_registry;
  for (auto& element : *input.elements()) {
    if (element.type() == network::DataElement::TYPE_BLOB) {
      blink::mojom::BlobPtrInfo blob_ptr;
      if (!blob_registry) {
        blink::Platform::Current()->GetInterfaceProvider()->GetInterface(
            mojo::MakeRequest(&blob_registry));
      }
      blob_registry->GetBlobFromUUID(mojo::MakeRequest(&blob_ptr),
                                     element.blob_uuid());
      blob_ptrs.push_back(std::move(blob_ptr));
    }
  }
  return blob_ptrs;
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
        if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
          DCHECK(element.optional_blob_handle.is_valid());
          blink::mojom::BlobPtr blob_ptr(
              blink::mojom::BlobPtrInfo(std::move(element.optional_blob_handle),
                                        blink::mojom::Blob::Version_));

          network::mojom::DataPipeGetterPtr data_pipe_getter_ptr;
          blob_ptr->AsDataPipeGetter(MakeRequest(&data_pipe_getter_ptr));

          request_body->AppendDataPipe(std::move(data_pipe_getter_ptr));
        } else {
          request_body->AppendBlob(element.blob_uuid.Utf8(),
                                   element.blob_length);
        }
        break;
      }
      case WebHTTPBody::Element::kTypeDataPipe: {
        // Convert the raw message pipe to network::mojom::DataPipeGetterPtr.
        network::mojom::DataPipeGetterPtr data_pipe_getter;
        data_pipe_getter.Bind(network::mojom::DataPipeGetterPtrInfo(
            std::move(element.data_pipe_getter), 0u));

        // Set the cloned DataPipeGetter to the output |request_body|, while
        // keeping the original message pipe back in the input |httpBody|. This
        // way the consumer of the |httpBody| can retrieve the data pipe
        // multiple times (e.g. during redirects) until the request is finished.
        network::mojom::DataPipeGetterPtr cloned_getter;
        data_pipe_getter->Clone(mojo::MakeRequest(&cloned_getter));
        request_body->AppendDataPipe(std::move(cloned_getter));
        element.data_pipe_getter =
            data_pipe_getter.PassInterface().PassHandle();
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
  bool block_mixed_plugin_content = false;
  if (request.GetExtraData()) {
    RequestExtraData* extra_data =
        static_cast<RequestExtraData*>(request.GetExtraData());
    block_mixed_plugin_content = extra_data->block_mixed_plugin_content();
  }

  return blink::WebMixedContent::ContextTypeFromRequestContext(
      request.GetRequestContext(), block_mixed_plugin_content);
}

#undef STATIC_ASSERT_ENUM

}  // namespace content
