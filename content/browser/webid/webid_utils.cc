// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/webid_utils.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/url_formatter/elide_url.h"
#include "components/url_formatter/url_formatter.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/runtime_feature_state/runtime_feature_state_document_data.h"
#include "content/browser/webid/fedcm_metrics.h"
#include "content/browser/webid/flags.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/federated_identity_api_permission_context_delegate.h"
#include "content/public/browser/federated_identity_permission_context_delegate.h"
#include "content/public/common/web_identity.h"
#include "net/base/net_errors.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"

#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "url/origin.h"

using blink::mojom::FederatedAuthRequestResult;

namespace content::webid {

bool IsSameOriginWithAncestors(const url::Origin& origin,
                               RenderFrameHost* render_frame_host) {
  while (render_frame_host) {
    if (!origin.IsSameOriginWith(render_frame_host->GetLastCommittedOrigin())) {
      return false;
    }
    render_frame_host = render_frame_host->GetParent();
  }
  return true;
}

void SetIdpSigninStatus(content::BrowserContext* context,
                        int frame_tree_node_id,
                        const url::Origin& origin,
                        blink::mojom::IdpSigninStatus status) {
  FrameTreeNode* frame_tree_node = nullptr;
  // frame_tree_node_id may be invalid if we are loading the first frame
  // of the tab.
  if (frame_tree_node_id != FrameTreeNode::kFrameTreeNodeInvalidId) {
    frame_tree_node = FrameTreeNode::GloballyFindByID(frame_tree_node_id);
    // If the id was not kFrameTreeNodeInvalidId, but the lookup failed, we
    // ignore the load because we cannot do same-origin checks.
    if (!frame_tree_node) {
      RecordSetLoginStatusIgnoredReason(
          FedCmSetLoginStatusIgnoredReason::kFrameTreeLookupFailed);
      return;
    }
  }
  // Make sure we're same-origin with our ancestors.
  if (frame_tree_node) {
    if (frame_tree_node->IsInFencedFrameTree()) {
      RecordSetLoginStatusIgnoredReason(
          FedCmSetLoginStatusIgnoredReason::kInFencedFrame);
      return;
    }

    if (!IsSameOriginWithAncestors(origin, frame_tree_node->parent())) {
      RecordSetLoginStatusIgnoredReason(
          FedCmSetLoginStatusIgnoredReason::kCrossOrigin);
      return;
    }
  }

  auto* delegate = context->GetFederatedIdentityPermissionContext();
  if (!delegate) {
    // The embedder may not have a delegate (e.g. webview)
    return;
  }
  delegate->SetIdpSigninStatus(
      origin, status == blink::mojom::IdpSigninStatus::kSignedIn);
}

absl::optional<std::string> ComputeConsoleMessageForHttpResponseCode(
    const char* endpoint_name,
    int http_response_code) {
  // Do not add error message for OK response status.
  if (http_response_code >= 200 && http_response_code <= 299)
    return absl::nullopt;

  if (http_response_code < 0) {
    // In this case, the |response_code| represents a NET_ERROR, so we should
    // use a helper function to ensure we use a meaningful message.
    return base::StringPrintf(
        "The fetch of the %s resulted in a network error: %s", endpoint_name,
        net::ErrorToShortString(http_response_code).c_str());
  }
  // In this case, the |response_code| represents an HTTP error code, which is
  // standard and hence the number by itself should be understood.
  return base::StringPrintf(
      "When fetching the %s, a %d HTTP response code was received.",
      endpoint_name, http_response_code);
}

bool IsEndpointSameOrigin(const GURL& identity_provider_config_url,
                          const GURL& endpoint_url) {
  return url::Origin::Create(identity_provider_config_url)
      .IsSameOriginWith(endpoint_url);
}

bool ShouldFailAccountsEndpointRequestBecauseNotSignedInWithIdp(
    RenderFrameHost& host,
    const GURL& identity_provider_config_url,
    FederatedIdentityPermissionContextDelegate* permission_delegate) {
  const url::Origin idp_origin =
      url::Origin::Create(identity_provider_config_url);
  if (webid::GetIdpSigninStatusMode(host, idp_origin) ==
      FedCmIdpSigninStatusMode::DISABLED) {
    return false;
  }

  const absl::optional<bool> idp_signin_status =
      permission_delegate->GetIdpSigninStatus(idp_origin);
  return !idp_signin_status.value_or(true);
}

void UpdateIdpSigninStatusForAccountsEndpointResponse(
    RenderFrameHost& host,
    const GURL& identity_provider_config_url,
    IdpNetworkRequestManager::FetchStatus fetch_status,
    bool does_idp_have_failing_signin_status,
    FederatedIdentityPermissionContextDelegate* permission_delegate,
    FedCmMetrics* metrics) {
  url::Origin idp_origin = url::Origin::Create(identity_provider_config_url);
  if (webid::GetIdpSigninStatusMode(host, idp_origin) ==
      FedCmIdpSigninStatusMode::DISABLED) {
    return;
  }

  // Record metrics on effect of IDP sign-in status API.
  const absl::optional<bool> idp_signin_status =
      permission_delegate->GetIdpSigninStatus(idp_origin);
  metrics->RecordIdpSigninMatchStatus(idp_signin_status,
                                      fetch_status.parse_status);

  if (fetch_status.parse_status ==
      IdpNetworkRequestManager::ParseStatus::kSuccess) {
    // `does_idp_have_failing_signin_status` fails the request prior to fetching
    // the accounts endpoint for FedCmIdpSigninStatusMode::ENABLED mode but not
    // FedCmIdpSigninStatusMode::METRICS_ONLY mode. Do not set the IdP sign-in
    // status here if `does_idp_have_failing_signin_status` in
    // FedCmIdpSigninStatusMode::METRICS_ONLY mode in order to better emulate
    // FedCmIdpSigninStatusMode::ENABLED behavior.
    if (!does_idp_have_failing_signin_status) {
      permission_delegate->SetIdpSigninStatus(idp_origin, true);
    }
  } else {
    RecordIdpSignOutNetError(fetch_status.response_code);
    // Ensures that we only fetch accounts unconditionally once.
    permission_delegate->SetIdpSigninStatus(idp_origin, false);
  }
}

std::string GetConsoleErrorMessageFromResult(
    FederatedAuthRequestResult status) {
  switch (status) {
    case FederatedAuthRequestResult::kShouldEmbargo: {
      return "User declined or dismissed prompt. API exponential cool down "
             "triggered.";
    }
    case FederatedAuthRequestResult::kErrorDisabledInSettings: {
      return "Third-party sign in was disabled in browser Site Settings.";
    }
    case FederatedAuthRequestResult::kErrorTooManyRequests: {
      return "Only one navigator.credentials.get request may be outstanding at "
             "one time.";
    }
    case FederatedAuthRequestResult::kErrorFetchingWellKnownHttpNotFound: {
      return "The provider's FedCM well-known file cannot be found.";
    }
    case FederatedAuthRequestResult::kErrorFetchingWellKnownNoResponse: {
      return "The provider's FedCM well-known file fetch resulted in an "
             "error response code.";
    }
    case FederatedAuthRequestResult::kErrorFetchingWellKnownInvalidResponse: {
      return "Provider's FedCM well-known file is invalid.";
    }
    case FederatedAuthRequestResult::kErrorFetchingWellKnownListEmpty: {
      return "Provider's FedCM well-known file has no config URLs.";
    }
    case FederatedAuthRequestResult::
        kErrorFetchingWellKnownInvalidContentType: {
      return "Provider's FedCM well-known content type must be a JSON content "
             "type.";
    }
    case FederatedAuthRequestResult::kErrorConfigNotInWellKnown: {
      return "Provider's FedCM config file not listed in its well-known file.";
    }
    case FederatedAuthRequestResult::kErrorWellKnownTooBig: {
      return "Provider's FedCM well-known file contains too many config URLs.";
    }
    case FederatedAuthRequestResult::kErrorFetchingConfigHttpNotFound: {
      return "The provider's FedCM config file cannot be found.";
    }
    case FederatedAuthRequestResult::kErrorFetchingConfigNoResponse: {
      return "The provider's FedCM config file fetch resulted in an "
             "error response code.";
    }
    case FederatedAuthRequestResult::kErrorFetchingConfigInvalidResponse: {
      return "Provider's FedCM config file is invalid.";
    }
    case FederatedAuthRequestResult::kErrorFetchingConfigInvalidContentType: {
      return "Provider's FedCM config file content type must be a JSON content "
             "type.";
    }
    case FederatedAuthRequestResult::kErrorFetchingClientMetadataHttpNotFound: {
      return "The provider's client metadata endpoint cannot be found.";
    }
    case FederatedAuthRequestResult::kErrorFetchingClientMetadataNoResponse: {
      return "The provider's client metadata fetch resulted in an error "
             "response code.";
    }
    case FederatedAuthRequestResult::
        kErrorFetchingClientMetadataInvalidResponse: {
      return "Provider's client metadata is invalid.";
    }
    case FederatedAuthRequestResult::
        kErrorFetchingClientMetadataInvalidContentType: {
      return "Provider's client metadata content type must be a JSON content "
             "type.";
    }
    case FederatedAuthRequestResult::kErrorFetchingAccountsHttpNotFound: {
      return "The provider's accounts list endpoint cannot be found.";
    }
    case FederatedAuthRequestResult::kErrorFetchingAccountsNoResponse: {
      return "The provider's accounts list fetch resulted in an error response "
             "code.";
    }
    case FederatedAuthRequestResult::kErrorFetchingAccountsInvalidResponse: {
      return "Provider's accounts list is invalid. Should have received an "
             "\"accounts\" list, where each account must have at least \"id\", "
             "\"name\", and \"email\".";
    }
    case FederatedAuthRequestResult::kErrorFetchingAccountsListEmpty: {
      return "Provider's accounts list is empty.";
    }
    case FederatedAuthRequestResult::kErrorFetchingAccountsInvalidContentType: {
      return "Provider's accounts list endpoint content type must be a JSON "
             "content type.";
    }
    case FederatedAuthRequestResult::kErrorFetchingIdTokenHttpNotFound: {
      return "The provider's id token endpoint cannot be found.";
    }
    case FederatedAuthRequestResult::kErrorFetchingIdTokenNoResponse: {
      return "The provider's token fetch resulted in an error response "
             "code.";
    }
    case FederatedAuthRequestResult::kErrorFetchingIdTokenInvalidResponse: {
      return "Provider's token is invalid.";
    }
    case FederatedAuthRequestResult::kErrorFetchingIdTokenIdpErrorResponse: {
      return "Provider is unable to issue a token, but provided details on the "
             "error that occurred.";
    }
    case FederatedAuthRequestResult::
        kErrorFetchingIdTokenCrossSiteIdpErrorResponse: {
      return "Provider is unable to issue a token, but provided details on the "
             "error that occurred. The error URL must be same-site with the "
             "config URL.";
    }
    case FederatedAuthRequestResult::kErrorFetchingIdTokenInvalidContentType: {
      return "Provider's token endpoint content type must be a JSON content "
             "type.";
    }
    case FederatedAuthRequestResult::kErrorCanceled: {
      return "The request has been aborted.";
    }
    case FederatedAuthRequestResult::kErrorRpPageNotVisible: {
      return "RP page is not visible.";
    }
    case FederatedAuthRequestResult::kErrorSilentMediationFailure: {
      return "Silent mediation was requested, but the conditions to achieve it "
             "were not met.";
    }
    case FederatedAuthRequestResult::kErrorThirdPartyCookiesBlocked: {
      return "Third party cookies are blocked. Right now the Chromium "
             "implementation of FedCM API requires third party cookies and "
             "this restriction will be removed soon. In the interim, to test "
             "FedCM without third-party cookies, enable the "
             "#fedcm-without-third-party-cookies flag.";
    }
    case FederatedAuthRequestResult::kErrorNotSignedInWithIdp: {
      return "Not signed in with the identity provider.";
    }
    case FederatedAuthRequestResult::kError: {
      return "Error retrieving a token.";
    }
    case FederatedAuthRequestResult::kSuccess: {
      // Should not be called with success, as we should not add a console
      // message for success.
      DCHECK(false);
      return "";
    }
  }
}

FedCmIdpSigninStatusMode GetIdpSigninStatusMode(RenderFrameHost& host,
                                                const url::Origin& idp_origin) {
  // TODO(crbug.com/1487668): Remove this function in favor of
  // GetFedCmIdpSigninStatusFlag.
  return GetFedCmIdpSigninStatusFlag();
}

std::string FormatUrlWithDomain(const GURL& url, bool for_display) {
  // We do not use url_formatter::FormatUrlForSecurityDisplay() directly because
  // our UI intentionally shows only the eTLD+1, as it makes for a shorter text
  // that is also clearer to users. The identity provider's well-known file is
  // in the root of the eTLD+1, and sign-in status within identity provider and
  // relying party can be domain-wide because it relies on cookies.
  std::string formatted_url_str =
      net::IsLocalhost(url)
          ? url.host()
          : net::registry_controlled_domains::GetDomainAndRegistry(
                url,
                net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  if (for_display) {
    return base::UTF16ToUTF8(url_formatter::FormatUrlForSecurityDisplay(
        GURL(url.scheme() + "://" + formatted_url_str),
        url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS));
  }
  // We want defaults but we need to keep the scheme.
  url_formatter::FormatUrlTypes types =
      url_formatter::kFormatUrlOmitDefaults &
      ~(url_formatter::kFormatUrlOmitHTTP | url_formatter::kFormatUrlOmitHTTPS |
        url_formatter::kFormatUrlOmitFileScheme);
  return base::UTF16ToUTF8(url_formatter::FormatUrl(
      GURL(url.scheme() + "://" + formatted_url_str), types,
      base::UnescapeRule::SPACES, nullptr, nullptr, nullptr));
}

bool HasSharingPermissionOrIdpHasThirdPartyCookiesAccess(
    RenderFrameHost& host,
    const GURL& provider_url,
    const url::Origin& embedder_origin,
    const url::Origin& requester_origin,
    const absl::optional<std::string>& account_id,
    FederatedIdentityPermissionContextDelegate* sharing_permission_delegate,
    FederatedIdentityApiPermissionContextDelegate* api_permission_delegate) {
  bool has_access = IsFedCmExemptIdpWithThirdPartyCookiesEnabled() &&
                    api_permission_delegate->HasThirdPartyCookiesAccess(
                        host, provider_url, embedder_origin);
  return sharing_permission_delegate->HasSharingPermission(
             requester_origin, embedder_origin,
             url::Origin::Create(provider_url), account_id) ||
         has_access;
}

}  // namespace content::webid
