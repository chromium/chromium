// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/navigation_interceptor.h"

#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/webid/flags.h"
#include "content/browser/webid/identity_registry.h"
#include "content/browser/webid/request.h"
#include "content/browser/webid/webid_utils.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/webid/federated_embedder_login_request.h"
#include "content/public/browser/webid/identity_credential_source.h"
#include "content/public/common/content_client.h"
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
  if (!IsNavigationInterceptionEnabled() &&
      !IsEmbedderInitiatedLoginEnabled()) {
    return;
  }
  registry.AddThrottle(std::make_unique<NavigationInterceptor>(registry));
}

NavigationInterceptor::NavigationInterceptor(
    NavigationThrottleRegistry& registry)
    : NavigationInterceptor(
          registry,
          base::BindRepeating([](content::RenderFrameHost* rfh) -> Request* {
            return webid::Request::GetOrCreateForCurrentDocument(rfh);
          })) {}

NavigationInterceptor::NavigationInterceptor(
    NavigationThrottleRegistry& registry,
    RequestFactory request_factory)
    : content::NavigationThrottle(registry),
      request_factory_(std::move(request_factory)) {}

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
NavigationInterceptor::WillRedirectRequest() {
  return ProcessRequest();
}

NavigationThrottle::ThrottleCheckResult
NavigationInterceptor::WillProcessResponse() {
  return ProcessRequest();
}

NavigationThrottle::ThrottleCheckResult
NavigationInterceptor::ProcessRequest() {
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

  // TODO(crbug.com/498095297): Use only one header name once it is finalized.
  std::optional<std::string> intercept_header =
      headers->GetNormalizedHeader("Federation-Initiate-Request");
  if (!intercept_header) {
    intercept_header =
        headers->GetNormalizedHeader("FedCM-Intercept-Navigation");
  }

  std::optional<std::string> connection_status_header =
      headers->GetNormalizedHeader("Federation-RP-Connection-Status");

  if (!intercept_header && !connection_status_header) {
    return PROCEED;
  }

  content::RenderFrameHost* rfh = document_.AsRenderFrameHostIfValid();

  if (!rfh) {
    return PROCEED;
  }

  // We intercept if the user explicitly enabled interception, or if there is
  // an active embedder login request.
  bool has_embedder_login_request = HasEmbedderLoginRequest(rfh);
  if (!IsNavigationInterceptionEnabled() && !has_embedder_login_request) {
    return PROCEED;
  }

  if (FrameTreeNode::From(rfh)->is_on_initial_empty_document() &&
      rfh->GetLastCommittedOrigin().opaque()) {
    // Navigations out of an initial empty document with an opaque origin
    // (e.g., target="_blank" which defaults to rel="noopener") cannot support
    // FedCM because the Relying Party context is opaque.
    // An initial empty document has an opaque origin ONLY when there is no
    // opener relationship; if an opener were present (e.g., window.open or
    // rel="opener"), the origin would have been inherited from the opener
    // and would not be opaque.
    return PROCEED;
  }

  if (IdentityRegistry::FromWebContents(
          WebContents::FromRenderFrameHost(rfh)) &&
      FrameTreeNode::From(rfh)->is_on_initial_empty_document()) {
    // In pop-up windows that FedCM opens itself, such as when using the
    // Continuation API or when the user is logged out, the render frame host
    // needs to be loaded with a valid document before we can intercept
    // navigations out of it.
    return PROCEED;
  }

  if (connection_status_header) {
    // It's possible that both headers are present. In that case, if there's no
    // embedder login request, we should just proceed.
    if (has_embedder_login_request) {
      data_decoder::DataDecoder::ParseStructuredHeaderDictionaryIsolated(
          *connection_status_header,
          base::BindOnce(&NavigationInterceptor::OnConnectionStatusHeaderParsed,
                         weak_ptr_factory_.GetWeakPtr()));
    } else {
      return PROCEED;
    }
  } else if (intercept_header) {
    data_decoder::DataDecoder::ParseStructuredHeaderDictionaryIsolated(
        *intercept_header,
        base::BindOnce(&NavigationInterceptor::OnHeaderParsed,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    return PROCEED;
  }

  // TODO(http://crbug.com/455614294): Ideally, we'd like to cancel the
  // navigation early on so that the spinner stops. However, we need to
  // defer here because cancelling will destroy this object before the
  // async header parsing and token request complete.

  return DEFER;
}

void NavigationInterceptor::OnConnectionStatusHeaderParsed(
    base::expected<net::structured_headers::Dictionary, std::string> result) {
  content::RenderFrameHost* rfh = document_.AsRenderFrameHostIfValid();
  if (!rfh) {
    // The document is no longer valid, likely because the target frame has
    // navigated in the meantime.
    // Resume the deferred navigation without cancelling.
    Resume();
    return;
  }

  FederatedEmbedderLoginRequest* embedder_login_request =
      FederatedEmbedderLoginRequest::Get(WebContents::FromRenderFrameHost(rfh));
  if (!embedder_login_request) {
    Resume();
    return;
  }

  if (!result.has_value()) {
    // The header was available, but malformed.
    // Cancel the navigation because it is a developer error.
    CancelDeferredNavigation(CANCEL);
    return;
  }

  auto it = result->find("status");
  if (it != result->end() && it->second.member.size() == 1 &&
      it->second.member[0].item.is_string() &&
      it->second.member[0].item.GetString() == "connected") {
    std::optional<std::string> account_id;
    auto account_id_it = result->find("account_id");
    if (account_id_it != result->end() &&
        account_id_it->second.member.size() == 1 &&
        account_id_it->second.member[0].item.is_string()) {
      account_id = account_id_it->second.member[0].item.GetString();
    }

    // The server can send this header without embedder login request.
    if (net::SchemefulSite::IsSameSite(
            embedder_login_request->idp_origin(),
            url::Origin::Create(navigation_handle()->GetURL()))) {
      if (account_id == embedder_login_request->account_id()) {
        embedder_login_request->OnFederatedResultReceived(
            FederatedLoginResult::kSuccess);
      } else {
        embedder_login_request->OnFederatedResultReceived(
            FederatedLoginResult::kExpectedAccountNotPresent);
      }
    }
  }

  // Resume the deferred navigation without cancelling.
  Resume();
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
  auto idp_get_params_vector =
      request_builder.Build(navigation_handle()->GetURL(), *result);

  if (!idp_get_params_vector) {
    // The header was available, parsed, but contained an invalid set of
    // parameters.
    // Cancel the navigation because it is a developer error.
    CancelDeferredNavigation(CANCEL);
    return;
  }

  request_factory_.Run(rfh)->RequestToken(
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
  // The token response is not used in the navigation interception flow because
  // the IdP is expected to respond with a "redirect_to" field which is handled
  // in Request.
  // We cancel this specific navigation, assuming that the Request
  // will have already started a new navigation.
  CancelDeferredNavigation(CANCEL);
}

const char* NavigationInterceptor::GetNameForLogging() {
  return "FedCMNavigationInterceptor";
}

std::optional<std::vector<blink::mojom::IdentityProviderGetParametersPtr>>
NavigationInterceptor::RequestBuilder::Build(
    const GURL& base_url,
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

  auto config_url_str = get_string("config_url");
  if (!config_url_str) {
    return std::nullopt;
  }
  GURL config_url = base_url.Resolve(*config_url_str);
  if (!config_url.is_valid()) {
    return std::nullopt;
  }
  if (!url::IsSameOriginWith(base_url, config_url)) {
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
  idp_config->config_url = config_url;

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
