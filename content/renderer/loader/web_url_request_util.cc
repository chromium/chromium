// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/loader/web_url_request_util.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>

#include "base/check.h"
#include "base/notreached.h"
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
      case network::mojom::DataElementType::kFile: {
        base::Optional<base::Time> modification_time;
        if (!element.expected_modification_time().is_null())
          modification_time = element.expected_modification_time();
        http_body.AppendFileRange(
            blink::FilePathToWebString(element.path()), element.offset(),
            (element.length() != std::numeric_limits<uint64_t>::max())
                ? element.length()
                : -1,
            modification_time);
        break;
      }
      case network::mojom::DataElementType::kBlob:
          http_body.AppendBlob(WebString::FromASCII(element.blob_uuid()));
        break;
      case network::mojom::DataElementType::kDataPipe: {
        http_body.AppendDataPipe(element.CloneDataPipeGetter());
        break;
      }
      case network::mojom::DataElementType::kUnknown:
      case network::mojom::DataElementType::kChunkedDataPipe:
      case network::mojom::DataElementType::kReadOnceStream:
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
              std::numeric_limits<uint64_t>::max(),
              element.modification_time.value_or(base::Time()));
        } else {
          request_body->AppendFileRange(
              blink::WebStringToFilePath(element.file_path),
              static_cast<uint64_t>(element.file_start),
              static_cast<uint64_t>(element.file_length),
              element.modification_time.value_or(base::Time()));
        }
        break;
      case WebHTTPBody::Element::kTypeBlob: {
        DCHECK(element.optional_blob);
        mojo::Remote<blink::mojom::Blob> blob_remote(
            mojo::PendingRemote<blink::mojom::Blob>(
                std::move(element.optional_blob)));

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
                std::move(element.data_pipe_getter)));

        // Set the cloned DataPipeGetter to the output |request_body|, while
        // keeping the original message pipe back in the input |httpBody|. This
        // way the consumer of the |httpBody| can retrieve the data pipe
        // multiple times (e.g. during redirects) until the request is finished.
        mojo::PendingRemote<network::mojom::DataPipeGetter> cloned_getter;
        data_pipe_getter->Clone(cloned_getter.InitWithNewPipeAndPassReceiver());
        request_body->AppendDataPipe(std::move(cloned_getter));
        element.data_pipe_getter = data_pipe_getter.Unbind();
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

blink::mojom::RequestContextType GetRequestContextTypeForWebURLRequest(
    const WebURLRequest& request) {
  return static_cast<blink::mojom::RequestContextType>(
      request.GetRequestContext());
}

network::mojom::RequestDestination GetRequestDestinationForWebURLRequest(
    const WebURLRequest& request) {
  return static_cast<network::mojom::RequestDestination>(
      request.GetRequestDestination());
}

blink::WebMixedContentContextType GetMixedContentContextTypeForWebURLRequest(
    const WebURLRequest& request) {
  return blink::WebMixedContent::ContextTypeFromRequestContext(
      request.GetRequestContext(),
      blink::WebMixedContent::CheckModeForPlugin::kLax);
}

#undef STATIC_ASSERT_ENUM

}  // namespace content
