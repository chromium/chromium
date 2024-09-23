// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_navigation_params.h"

#include "base/uuid.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/navigation/navigation_params.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_network_provider.h"
#include "third_party/blink/renderer/platform/loader/static_data_navigation_body_loader.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

namespace blink {

WebNavigationParams::WebNavigationParams()
    : http_method(http_names::kGET),
      devtools_navigation_token(base::UnguessableToken::Create()),
      base_auction_nonce(base::Uuid::GenerateRandomV4()),
      content_settings(CreateDefaultRendererContentSettings()) {}

WebNavigationParams::~WebNavigationParams() = default;

WebNavigationParams::WebNavigationParams(
    const blink::DocumentToken& document_token,
    const base::UnguessableToken& devtools_navigation_token,
    const base::Uuid& base_auction_nonce)
    : http_method(http_names::kGET),
      document_token(document_token),
      devtools_navigation_token(devtools_navigation_token),
      base_auction_nonce(base_auction_nonce),
      content_settings(CreateDefaultRendererContentSettings()) {}

// static
std::unique_ptr<WebNavigationParams> WebNavigationParams::CreateFromInfo(
    const WebNavigationInfo& info) {
  auto result = std::make_unique<WebNavigationParams>();
  result->url = info.url_request.Url();
  result->http_method = info.url_request.HttpMethod();
  result->referrer = info.url_request.ReferrerString();
  result->referrer_policy = info.url_request.GetReferrerPolicy();
  result->http_body = info.url_request.HttpBody();
  result->http_content_type =
      info.url_request.HttpHeaderField(http_names::kContentType);
  result->requestor_origin = info.url_request.RequestorOrigin();
  result->fallback_base_url = info.requestor_base_url;
  result->frame_load_type = info.frame_load_type;
  result->is_client_redirect = info.is_client_redirect;
  result->navigation_timings.input_start = info.input_start;
  result->initiator_origin_trial_features =
      info.initiator_origin_trial_features;
  result->frame_policy = info.frame_policy;
  result->had_transient_user_activation = info.url_request.HasUserGesture();
  return result;
}

// static
std::unique_ptr<WebNavigationParams>
WebNavigationParams::CreateWithEmptyHTMLForTesting(const WebURL& base_url) {
  return CreateWithHTMLStringForTesting(base::span<const char>(), base_url);
}

// static
std::unique_ptr<WebNavigationParams>
WebNavigationParams::CreateWithHTMLStringForTesting(base::span<const char> html,
                                                    const WebURL& base_url) {
  auto result = std::make_unique<WebNavigationParams>();
  result->url = base_url;
  FillStaticResponse(result.get(), "text/html", "UTF-8", html);
  return result;
}

// static
void WebNavigationParams::FillBodyLoader(WebNavigationParams* params,
                                         base::span<const char> data) {
  params->response.SetExpectedContentLength(data.size());
  params->body_loader = StaticDataNavigationBodyLoader::CreateWithData(
      SharedBuffer::Create(data));
  params->is_static_data = true;
}

// static
void WebNavigationParams::FillBodyLoader(WebNavigationParams* params,
                                         WebData data) {
  params->response.SetExpectedContentLength(data.size());
  auto body_loader = std::make_unique<StaticDataNavigationBodyLoader>();
  params->body_loader = StaticDataNavigationBodyLoader::CreateWithData(
      scoped_refptr<SharedBuffer>(data));
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
  params->response.SetHttpStatusCode(params->http_status_code);
  FillBodyLoader(params, data);
}

// static
void WebNavigationParams::FillStaticResponse(WebNavigationParams* params,
                                             WebString mime_type,
                                             WebString text_encoding,
                                             SharedBuffer* data) {
  params->response = WebURLResponse(params->url);
  params->response.SetMimeType(mime_type);
  params->response.SetTextEncodingName(text_encoding);
  params->response.SetHttpStatusCode(params->http_status_code);
  FillBodyLoader(params, WebData(data));
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
    CrossVariantMojoRemote<network::mojom::URLLoaderFactoryInterfaceBase>
        loader_factory)
    : outer_url(outer_url),
      header_integrity(header_integrity),
      inner_url(inner_url),
      inner_response(inner_response),
      loader_factory(std::move(loader_factory)) {}

}  // namespace blink
