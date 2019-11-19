// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_navigation_params.h"

#include "third_party/blink/renderer/core/exported/web_document_loader_impl.h"
#include "third_party/blink/renderer/platform/loader/static_data_navigation_body_loader.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

namespace blink {

WebNavigationParams::WebNavigationParams()
    : http_method(http_names::kGET),
      devtools_navigation_token(base::UnguessableToken::Create()) {}

WebNavigationParams::~WebNavigationParams() = default;

WebNavigationParams::WebNavigationParams(
    const base::UnguessableToken& devtools_navigation_token)
    : http_method(http_names::kGET),
      devtools_navigation_token(devtools_navigation_token) {}

// static
std::unique_ptr<WebNavigationParams> WebNavigationParams::CreateFromInfo(
    const WebNavigationInfo& info) {
  auto result = std::make_unique<WebNavigationParams>();
  result->url = info.url_request.Url();
  result->http_method = info.url_request.HttpMethod();
  result->referrer = info.url_request.HttpHeaderField(http_names::kReferer);
  result->referrer_policy = info.url_request.GetReferrerPolicy();
  result->http_body = info.url_request.HttpBody();
  result->http_content_type =
      info.url_request.HttpHeaderField(http_names::kContentType);
  result->previews_state = info.url_request.GetPreviewsState();
  result->requestor_origin = info.url_request.RequestorOrigin();
  result->frame_load_type = info.frame_load_type;
  result->is_client_redirect = info.is_client_redirect;
  result->navigation_timings.input_start = info.input_start;
  result->initiator_origin_trial_features =
      info.initiator_origin_trial_features;
  result->ip_address_space = info.initiator_address_space;
  result->frame_policy = info.frame_policy;
  return result;
}

// static
std::unique_ptr<WebNavigationParams> WebNavigationParams::CreateWithHTMLString(
    base::span<const char> html,
    const WebURL& base_url) {
  auto result = std::make_unique<WebNavigationParams>();
  result->url = base_url;
  FillStaticResponse(result.get(), "text/html", "UTF-8", html);
  return result;
}

// static
std::unique_ptr<WebNavigationParams> WebNavigationParams::CreateForErrorPage(
    WebDocumentLoader* failed_document_loader,
    base::span<const char> html,
    const WebURL& base_url,
    const WebURL& unreachable_url,
    int error_code) {
  auto result = WebNavigationParams::CreateWithHTMLString(html, base_url);
  DCHECK(!unreachable_url.IsEmpty() || error_code != 0);
  result->unreachable_url = unreachable_url;
  result->error_code = error_code;
  static_cast<WebDocumentLoaderImpl*>(failed_document_loader)
      ->FillNavigationParamsForErrorPage(result.get());
  return result;
}

#if INSIDE_BLINK
// static
std::unique_ptr<WebNavigationParams> WebNavigationParams::CreateWithHTMLBuffer(
    scoped_refptr<SharedBuffer> buffer,
    const KURL& base_url) {
  auto result = std::make_unique<WebNavigationParams>();
  result->url = base_url;
  FillStaticResponse(result.get(), "text/html", "UTF-8",
                     base::make_span(buffer->Data(), buffer->size()));
  return result;
}
#endif

// static
void WebNavigationParams::FillBodyLoader(WebNavigationParams* params,
                                         base::span<const char> data) {
  params->response.SetExpectedContentLength(data.size());
  auto body_loader = std::make_unique<StaticDataNavigationBodyLoader>();
  body_loader->Write(data.data(), data.size());
  body_loader->Finish();
  params->body_loader = std::move(body_loader);
  params->is_static_data = true;
}

// static
void WebNavigationParams::FillBodyLoader(WebNavigationParams* params,
                                         WebData data) {
  params->response.SetExpectedContentLength(data.size());
  auto body_loader = std::make_unique<StaticDataNavigationBodyLoader>();
  scoped_refptr<SharedBuffer> buffer = data;
  if (buffer)
    body_loader->Write(*buffer);
  body_loader->Finish();
  params->body_loader = std::move(body_loader);
  params->is_static_data = true;
}

// static
void WebNavigationParams::FillStaticResponse(WebNavigationParams* params,
                                             WebString mime_type,
                                             WebString text_encoding,
                                             base::span<const char> data) {
  params->response = WebURLResponse(params->url);
  params->response.SetMimeType(mime_type);
  params->response.SetTextEncodingName(text_encoding);
  FillBodyLoader(params, data);
}

WebNavigationParams::PrefetchedSignedExchange::PrefetchedSignedExchange() =
    default;
WebNavigationParams::PrefetchedSignedExchange::~PrefetchedSignedExchange() =
    default;
WebNavigationParams::PrefetchedSignedExchange::PrefetchedSignedExchange(
    const WebURL& outer_url,
    const WebString& header_integrity,
    const WebURL& inner_url,
    const WebURLResponse& inner_response,
    mojo::ScopedMessagePipeHandle loader_factory_handle)
    : outer_url(outer_url),
      header_integrity(header_integrity),
      inner_url(inner_url),
      inner_response(inner_response),
      loader_factory_handle(std::move(loader_factory_handle)) {}

}  // namespace blink
