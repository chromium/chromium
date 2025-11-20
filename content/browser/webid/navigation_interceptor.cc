// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/navigation_interceptor.h"

#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "content/browser/webid/flags.h"
#include "content/browser/webid/request_service.h"
#include "content/browser/webid/webid_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/structured_headers.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "url/gurl.h"

namespace content::webid {

// static
void NavigationInterceptor::MaybeCreateAndAdd(
    NavigationThrottleRegistry& registry) {
  if (!IsNavigationInterceptionEnabled()) {
    return;
  }
  registry.AddThrottle(std::make_unique<NavigationInterceptor>(registry));
}

NavigationInterceptor::NavigationInterceptor(
    NavigationThrottleRegistry& registry)
    : NavigationInterceptor(
          registry,
          base::BindRepeating(
              [](content::RenderFrameHost* rfh) -> RequestService* {
                return webid::RequestService::GetOrCreateForCurrentDocument(
                    rfh);
              })) {}

NavigationInterceptor::NavigationInterceptor(
    NavigationThrottleRegistry& registry,
    RequestServiceBuilder service_builder)
    : content::NavigationThrottle(registry),
      service_builder_(std::move(service_builder)) {}

NavigationInterceptor::~NavigationInterceptor() = default;

NavigationThrottle::ThrottleCheckResult
NavigationInterceptor::WillStartRequest() {
  // navigation_handle()->GetRenderFrameHost() points to the new RFH that the
  // navigation would commit to, but we will abort that navigation, so we want
  // to initiate the request in the current RFH for the target frame, so we look
  // that up here.
  content::RenderFrameHost* rfh = RenderFrameHost::FromID(
      navigation_handle()->GetPreviousRenderFrameHostId());
  document_ = rfh->GetWeakDocumentPtr();
  return PROCEED;
}

NavigationThrottle::ThrottleCheckResult
NavigationInterceptor::WillProcessResponse() {
  if (!document_.AsRenderFrameHostIfValid()) {
    // Some other navigation has happened in the meantime.
    return PROCEED;
  }
  if (!navigation_handle()->IsInPrimaryMainFrame()) {
    // Only top level navigations can be intercepted.
    return PROCEED;
  }

  // Only intercept user-initiated navigations because we want to use
  // active mode.
  if (!DidNavigationHandleHaveActivation(navigation_handle())) {
    return PROCEED;
  }

  auto* headers = navigation_handle()->GetResponseHeaders();

  if (!headers) {
    // IdPs need to opt-in for the interception via response header.
    return PROCEED;
  }

  std::optional<std::string> header =
      headers->GetNormalizedHeader("FedCM-Intercept-Navigation");

  if (!header) {
    return PROCEED;
  }

  content::RenderFrameHost* rfh = document_.AsRenderFrameHostIfValid();

  if (!rfh) {
    return PROCEED;
  }

  data_decoder::DataDecoder::ParseStructuredHeaderDictionaryIsolated(
      *header, base::BindOnce(&NavigationInterceptor::OnHeaderParsed,
                              weak_ptr_factory_.GetWeakPtr()));

  // TODO(http://crbug.com/455614294): Ideally, we'd like to cancel the
  // navigation early on so that the spinner stops. However, we need to
  // defer here because cancelling will destroy this object before the
  // async header parsing and token request complete.

  return DEFER;
}

void NavigationInterceptor::OnHeaderParsed(
    base::expected<net::structured_headers::Dictionary, std::string> result) {
  content::RenderFrameHost* rfh = document_.AsRenderFrameHostIfValid();
  if (!rfh) {
    // The document is no longer valid, likely because the target frame has
    // navigated in the meantime.
    // Resume the deferred navigation without cancelling.
    Resume();
    return;
  }

  if (!result.has_value()) {
    // The header was available, but malformed.
    // Cancel the navigation because it is a developer error.
    CancelDeferredNavigation(CANCEL);
    return;
  }

  RequestBuilder request_builder;
  auto idp_get_params_vector = request_builder.Build(*result);

  if (!idp_get_params_vector) {
    // The header was available, parsed, but contained an invalid set of
    // parameters.
    // Cancel the navigation because it is a developer error.
    CancelDeferredNavigation(CANCEL);
    return;
  }

  service_builder_.Run(rfh)->RequestToken(
      std::move(*idp_get_params_vector),
      password_manager::CredentialMediationRequirement::kOptional,
      navigation_handle(),
      base::BindOnce(&NavigationInterceptor::OnTokenResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void NavigationInterceptor::OnTokenResponse(
    blink::mojom::RequestTokenStatus status,
    const std::optional<GURL>& selected_identity_provider_config_url,
    std::optional<base::Value> token,
    blink::mojom::TokenErrorPtr error,
    bool is_auto_selected) {
  // TODO(http://crbug.com/455614294): expose the redirect_to URL outside
  // of the token so that either one or the other can be used.
  // TODO(http://crbug.com/455614294): consider supporting the redirect_to
  // response for non-interception use cases too.
  if (status != blink::mojom::RequestTokenStatus::kSuccess) {
    // The FedCM request failed.
    // Cancel the navigation because it is a developer error.
    CancelDeferredNavigation(CANCEL);
    return;
  }

  ResponseBuilder response_builder;
  auto params = response_builder.Build(std::move(*token));

  if (!params) {
    // The FedCM request succeeded, but the IdP returned an
    // invalid response.
    // Cancel the navigation because it is a developer error.
    CancelDeferredNavigation(CANCEL);
    return;
  }

  auto frame_tree_node_id = navigation_handle()->GetFrameTreeNodeId();

  params->frame_tree_node_id = frame_tree_node_id;

  content::WebContents* web_contents = navigation_handle()->GetWebContents();

  if (!web_contents) {
    return;
  }

  // Redirect the navigation to the URL specified in the token (which also
  // cancels the current one).
  // TODO(http://crbug.com/455614294): re-consider the security properties that
  // the redirection need to have (e.g. CSP, who is the initiator, should
  // SameSite cookies be passed, how does it relate to the history, the Referer
  // header, navigating to internal schemes, like chrome://settings, etc).
  web_contents->GetController().LoadURLWithParams(*params);
}

const char* NavigationInterceptor::GetNameForLogging() {
  return "FedCMNavigationInterceptor";
}

std::optional<std::vector<blink::mojom::IdentityProviderGetParametersPtr>>
NavigationInterceptor::RequestBuilder::Build(
    const net::structured_headers::Dictionary& dictionary) {
  auto get_string =
      [&dictionary](const std::string& key) -> std::optional<std::string> {
    auto it = dictionary.find(key);
    if (it == dictionary.end() || it->second.member.size() != 1 ||
        !it->second.member[0].item.is_string()) {
      return std::nullopt;
    }
    return it->second.member[0].item.GetString();
  };

  auto config_url = get_string("config_url");
  if (!config_url) {
    return std::nullopt;
  }

  auto client_id = get_string("client_id");
  if (!client_id) {
    return std::nullopt;
  }

  auto idp_options = blink::mojom::IdentityProviderRequestOptions::New();

  idp_options->login_hint = get_string("login_hint").value_or("");
  idp_options->domain_hint = get_string("domain_hint").value_or("");
  idp_options->params_json = get_string("params");

  auto fields_it = dictionary.find("fields");
  if (fields_it != dictionary.end()) {
    std::vector<std::string> fields;
    for (const auto& member_item : fields_it->second.member) {
      if (!member_item.item.is_string()) {
        return std::nullopt;
      }
      const std::string& field_str = member_item.item.GetString();
      fields.push_back(field_str);
    }
    idp_options->fields = fields;
  }

  blink::mojom::RpContext context = blink::mojom::RpContext::kSignIn;
  auto context_string = get_string("context");
  if (context_string) {
    if (*context_string == "signin") {
      context = blink::mojom::RpContext::kSignIn;
    } else if (*context_string == "signup") {
      context = blink::mojom::RpContext::kSignUp;
    } else if (*context_string == "use") {
      context = blink::mojom::RpContext::kUse;
    } else if (*context_string == "continue") {
      context = blink::mojom::RpContext::kContinue;
    } else {
      // Unknown context.
      return std::nullopt;
    }
  }

  auto idp_config = blink::mojom::IdentityProviderConfig::New();
  idp_config->config_url = GURL(*config_url);
  idp_config->client_id = *client_id;

  idp_options->config = std::move(idp_config);

  std::vector<blink::mojom::IdentityProviderRequestOptionsPtr>
      idp_options_vector;
  idp_options_vector.push_back(std::move(idp_options));

  auto idp_get_params = blink::mojom::IdentityProviderGetParameters::New();
  idp_get_params->providers = std::move(idp_options_vector);
  idp_get_params->context = context;

  // IdPs are not allowed to trigger passive modes via navigation interceptors.
  idp_get_params->mode = blink::mojom::RpMode::kActive;

  std::vector<blink::mojom::IdentityProviderGetParametersPtr>
      idp_get_params_vector;
  idp_get_params_vector.push_back(std::move(idp_get_params));

  return idp_get_params_vector;
}

std::optional<content::NavigationController::LoadURLParams>
NavigationInterceptor::ResponseBuilder::Build(const base::Value& response) {
  if (!response.is_dict()) {
    return std::nullopt;
  }

  const base::Value* redirect_to = response.GetDict().Find("redirect_to");
  if (!redirect_to) {
    return std::nullopt;
  }

  if (redirect_to->is_string()) {
    GURL url(redirect_to->GetString());
    if (!url.is_valid()) {
      return std::nullopt;
    }

    // TODO(http://crbug.com/455614294): does OAuth allow redirections
    // to non-secure origins, e.g. http?
    if (!url.SchemeIsHTTPOrHTTPS()) {
      return std::nullopt;
    }

    content::NavigationController::LoadURLParams navigation(url);
    navigation.transition_type = ui::PAGE_TRANSITION_LINK;
    return navigation;
  }

  // If it's not a string, it must be a dictionary to be valid.
  if (!redirect_to->is_dict()) {
    return std::nullopt;
  }

  GURL url;
  std::string method = "GET";

  const std::string* url_str = redirect_to->GetDict().FindString("url");
  if (!url_str) {
    return std::nullopt;
  }
  url = GURL(*url_str);

  if (!url.SchemeIsHTTPOrHTTPS()) {
    return std::nullopt;
  }

  const std::string* method_str = redirect_to->GetDict().FindString("method");
  if (method_str) {
    method = *method_str;
  }

  if (!url.is_valid()) {
    return std::nullopt;
  }

  content::NavigationController::LoadURLParams submission(url);
  submission.transition_type = ui::PAGE_TRANSITION_FORM_SUBMIT;

  if (method == "POST") {
    std::string body;
    const std::string* body_str = redirect_to->GetDict().FindString("body");
    if (body_str) {
      body = *body_str;
    }
    submission.post_data = network::ResourceRequestBody::CreateFromBytes(
        std::vector<uint8_t>(body.begin(), body.end()));
    submission.extra_headers =
        "Content-Type: application/x-www-form-urlencoded\r\n";
  }

  return submission;
}

}  // namespace content::webid
