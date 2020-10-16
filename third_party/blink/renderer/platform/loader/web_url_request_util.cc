// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/web_url_request_util.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>

#include "base/check.h"
#include "base/notreached.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/data_pipe_getter.mojom-blink.h"
#include "third_party/blink/public/mojom/blob/blob_registry.mojom-blink.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-blink.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_http_body.h"
#include "third_party/blink/public/platform/web_http_header_visitor.h"
#include "third_party/blink/public/platform/web_mixed_content.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

class HeaderFlattener : public WebHTTPHeaderVisitor {
 public:
  HeaderFlattener() = default;
  ~HeaderFlattener() override = default;

  void VisitHeader(const WebString& name, const WebString& value) override {
    const String wtf_name = name;
    const String wtf_value = value;

    // Skip over referrer headers found in the header map because we already
    // pulled it out as a separate parameter.
    if (EqualIgnoringASCIICase(wtf_name, "referer"))
      return;

    if (!buffer_.IsEmpty())
      buffer_.Append("\r\n");
    buffer_.Append(wtf_name);
    buffer_.Append(": ");
    buffer_.Append(wtf_value);
  }

  WebString GetBuffer() { return buffer_.ToString(); }

 private:
  StringBuilder buffer_;
};

}  // namespace

WebString GetWebURLRequestHeadersAsString(const WebURLRequest& request) {
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
      case network::mojom::blink::DataElementType::kBytes:
        http_body.AppendData(
            WebData(element.bytes(), SafeCast<size_t>(element.length())));
        break;
      case network::mojom::blink::DataElementType::kFile: {
        base::Optional<base::Time> modification_time;
        if (!element.expected_modification_time().is_null())
          modification_time = element.expected_modification_time();
        http_body.AppendFileRange(
            FilePathToWebString(element.path()), element.offset(),
            (element.length() != std::numeric_limits<uint64_t>::max())
                ? element.length()
                : -1,
            modification_time);
        break;
      }
      case network::mojom::blink::DataElementType::kDataPipe: {
        http_body.AppendDataPipe(element.CloneDataPipeGetter());
        break;
      }
      case network::mojom::blink::DataElementType::kUnknown:
      case network::mojom::blink::DataElementType::kChunkedDataPipe:
      case network::mojom::blink::DataElementType::kReadOnceStream:
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
    const WebHTTPBody& httpBody) {
  scoped_refptr<network::ResourceRequestBody> request_body =
      new network::ResourceRequestBody();
  size_t i = 0;
  WebHTTPBody::Element element;
  while (httpBody.ElementAt(i++, element)) {
    switch (element.type) {
      case HTTPBodyElementType::kTypeData:
        request_body->AppendBytes(element.data.Copy().ReleaseVector());
        break;
      case HTTPBodyElementType::kTypeFile:
        if (element.file_length == -1) {
          request_body->AppendFileRange(
              WebStringToFilePath(element.file_path), 0,
              std::numeric_limits<uint64_t>::max(),
              element.modification_time.value_or(base::Time()));
        } else {
          request_body->AppendFileRange(
              WebStringToFilePath(element.file_path),
              static_cast<uint64_t>(element.file_start),
              static_cast<uint64_t>(element.file_length),
              element.modification_time.value_or(base::Time()));
        }
        break;
      case HTTPBodyElementType::kTypeBlob: {
        DCHECK(element.optional_blob);
        mojo::Remote<mojom::blink::Blob> blob_remote(
            mojo::PendingRemote<mojom::blink::Blob>(
                std::move(element.optional_blob)));

        mojo::PendingRemote<network::mojom::blink::DataPipeGetter>
            data_pipe_getter_remote;
        blob_remote->AsDataPipeGetter(
            data_pipe_getter_remote.InitWithNewPipeAndPassReceiver());
        request_body->AppendDataPipe(
            mojo::PendingRemote<network::mojom::DataPipeGetter>(
                data_pipe_getter_remote.PassPipe(), 0u));
        break;
      }
      case HTTPBodyElementType::kTypeDataPipe: {
        // Convert the raw message pipe to
        // mojo::Remote<network::mojom::DataPipeGetter> data_pipe_getter.
        mojo::Remote<network::mojom::blink::DataPipeGetter> data_pipe_getter(
            mojo::PendingRemote<network::mojom::blink::DataPipeGetter>(
                std::move(element.data_pipe_getter)));

        // Set the cloned DataPipeGetter to the output |request_body|, while
        // keeping the original message pipe back in the input |httpBody|. This
        // way the consumer of the |httpBody| can retrieve the data pipe
        // multiple times (e.g. during redirects) until the request is finished.
        mojo::PendingRemote<network::mojom::blink::DataPipeGetter>
            cloned_getter;
        data_pipe_getter->Clone(cloned_getter.InitWithNewPipeAndPassReceiver());
        request_body->AppendDataPipe(
            mojo::PendingRemote<network::mojom::DataPipeGetter>(
                cloned_getter.PassPipe(), 0u));
        element.data_pipe_getter = data_pipe_getter.Unbind();
        break;
      }
    }
  }
  request_body->set_identifier(httpBody.Identifier());
  request_body->set_contains_sensitive_info(httpBody.ContainsPasswordData());
  return request_body;
}

mojom::blink::RequestContextType GetRequestContextTypeForWebURLRequest(
    const WebURLRequest& request) {
  return static_cast<mojom::blink::RequestContextType>(
      request.GetRequestContext());
}

network::mojom::blink::RequestDestination GetRequestDestinationForWebURLRequest(
    const WebURLRequest& request) {
  return static_cast<network::mojom::blink::RequestDestination>(
      request.GetRequestDestination());
}

WebMixedContentContextType GetMixedContentContextTypeForWebURLRequest(
    const WebURLRequest& request) {
  return WebMixedContent::ContextTypeFromRequestContext(
      request.GetRequestContext(), WebMixedContent::CheckModeForPlugin::kLax);
}

}  // namespace blink
