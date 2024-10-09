// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/webid_utils.h"

#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/url_formatter/elide_url.h"
#include "components/url_formatter/url_formatter.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/webid/fedcm_metrics.h"
#include "content/browser/webid/federated_auth_request_page_data.h"
#include "content/browser/webid/flags.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/federated_identity_api_permission_context_delegate.h"
#include "content/public/browser/federated_identity_permission_context_delegate.h"
#include "content/public/browser/runtime_feature_state/runtime_feature_state_document_data.h"
#include "content/public/common/web_identity.h"
#include "net/base/net_errors.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/schemeful_site.h"
#include "net/base/url_util.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "url/origin.h"

using blink::mojom::FederatedAuthRequestResult;
using content::FedCmDisconnectStatus;

namespace content::webid {

namespace {
constexpr net::registry_controlled_domains::PrivateRegistryFilter
    kDefaultPrivateRegistryFilter =
        net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES;
}  // namespace

bool IsSameSiteWithAncestors(const url::Origin& origin,
                             RenderFrameHost* render_frame_host) {
  while (render_frame_host) {
    // Many cases are same-origin, so check that first to speed up the cases
    // where the check passes, as IsSameSite() is slower.
    if (!origin.IsSameOriginWith(render_frame_host->GetLastCommittedOrigin()) &&
        !IsSameSite(origin, render_frame_host->GetLastCommittedOrigin())) {
      return false;
    }
    render_frame_host = render_frame_host->GetParent();
  }
  return true;
}

void SetIdpSigninStatus(content::BrowserContext* context,
                        FrameTreeNodeId frame_tree_node_id,
                        const url::Origin& origin,
                        blink::mojom::IdpSigninStatus status) {
  FrameTreeNode* frame_tree_node = nullptr;
  // frame_tree_node_id may be invalid if we are loading the first frame
  // of the tab.
  if (frame_tree_node_id) {
    frame_tree_node = FrameTreeNode::GloballyFindByID(frame_tree_node_id);
    // If the id was valid, but the lookup failed, we ignore the load because we
    // cannot do same-origin checks.
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

    if (!IsSameSiteWithAncestors(origin, frame_tree_node->parent())) {
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

std::optional<std::string> ComputeConsoleMessageForHttpResponseCode(
    const char* endpoint_name,
    int http_response_code) {
  // Do not add error message for OK response status.
  if (http_response_code >= 200 && http_response_code <= 299)
    return std::nullopt;

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

bool IsSameSite(const url::Origin& origin1, const url::Origin& origin2) {
  return net::SchemefulSite(origin1) == net::SchemefulSite(origin2);
}

bool ShouldFailAccountsEndpointRequestBecauseNotSignedInWithIdp(
    RenderFrameHost& host,
    const GURL& identity_provider_config_url,
    FederatedIdentityPermissionContextDelegate* permission_delegate) {
  const url::Origin idp_origin =
      url::Origin::Create(identity_provider_config_url);
  const std::optional<bool> idp_signin_status =
      permission_delegate->GetIdpSigninStatus(idp_origin);
  return !idp_signin_status.value_or(true);
}

void UpdateIdpSigninStatusForAccountsEndpointResponse(
    RenderFrameHost& host,
    const GURL& identity_provider_config_url,
    IdpNetworkRequestManager::FetchStatus fetch_status,
    bool does_idp_have_failing_signin_status,
    FederatedIdentityPermissionContextDelegate* permission_delegate) {
  url::Origin idp_origin = url::Origin::Create(identity_provider_config_url);

  // Record metrics on effect of IDP sign-in status API.
  const std::optional<bool> idp_signin_status =
      permission_delegate->GetIdpSigninStatus(idp_origin);
  FedCmMetrics::RecordIdpSigninMatchStatus(idp_signin_status,
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
    case FederatedAuthRequestResult::kIdpNotPotentiallyTrustworthy: {
      return "The IdP is not potentially trustworthy (are you using HTTP?)";
    }
    case FederatedAuthRequestResult::kDisabledInSettings: {
      return "FedCM was disabled in browser Site Settings.";
    }
    case FederatedAuthRequestResult::kDisabledInFlags: {
      return "FedCM was disabled in flags.";
    }
    case FederatedAuthRequestResult::kTooManyRequests: {
      return "Only one navigator.credentials.get request may be outstanding at "
             "one time.";
    }
    case FederatedAuthRequestResult::kWellKnownHttpNotFound: {
      return "The provider's FedCM well-known file cannot be found.";
    }
    case FederatedAuthRequestResult::kWellKnownNoResponse: {
      return "The provider's FedCM well-known file fetch resulted in an "
             "error response code.";
    }
    case FederatedAuthRequestResult::kWellKnownInvalidResponse: {
      return "Provider's FedCM well-known file is invalid.";
    }
    case FederatedAuthRequestResult::kWellKnownListEmpty: {
      return "Provider's FedCM well-known file has no config URLs.";
    }
    case FederatedAuthRequestResult::kWellKnownInvalidContentType: {
      return "Provider's FedCM well-known content type must be a JSON content "
             "type.";
    }
    case FederatedAuthRequestResult::kConfigNotInWellKnown: {
      return "Provider's FedCM config file not listed in its well-known file.";
    }
    case FederatedAuthRequestResult::kWellKnownTooBig: {
      return "Provider's FedCM well-known file contains too many config URLs.";
    }
    case FederatedAuthRequestResult::kConfigHttpNotFound: {
      return "The provider's FedCM config file cannot be found.";
    }
    case FederatedAuthRequestResult::kConfigNoResponse: {
      return "The provider's FedCM config file fetch resulted in an "
             "error response code.";
    }
    case FederatedAuthRequestResult::kConfigInvalidResponse: {
      return "Provider's FedCM config file is invalid.";
    }
    case FederatedAuthRequestResult::kConfigInvalidContentType: {
      return "Provider's FedCM config file content type must be a JSON content "
             "type.";
    }
    case FederatedAuthRequestResult::kClientMetadataHttpNotFound: {
      return "The provider's client metadata endpoint cannot be found.";
    }
    case FederatedAuthRequestResult::kClientMetadataNoResponse: {
      return "The provider's client metadata fetch resulted in an error "
             "response code.";
    }
    case FederatedAuthRequestResult::kClientMetadataInvalidResponse: {
      return "Provider's client metadata is invalid.";
    }
    case FederatedAuthRequestResult::kClientMetadataInvalidContentType: {
      return "Provider's client metadata content type must be a JSON content "
             "type.";
    }
    case FederatedAuthRequestResult::kAccountsHttpNotFound: {
      return "The provider's accounts list endpoint cannot be found.";
    }
    case FederatedAuthRequestResult::kAccountsNoResponse: {
      return "The provider's accounts list fetch resulted in an error response "
             "code.";
    }
    case FederatedAuthRequestResult::kAccountsInvalidResponse: {
      return "Provider's accounts list is invalid. Should have received an "
             "\"accounts\" list, where each account must have at least \"id\", "
             "\"name\", and \"email\".";
    }
    case FederatedAuthRequestResult::kAccountsListEmpty: {
      return "Provider's accounts list is empty.";
    }
    case FederatedAuthRequestResult::kAccountsInvalidContentType: {
      return "Provider's accounts list endpoint content type must be a JSON "
             "content type.";
    }
    case FederatedAuthRequestResult::kIdTokenHttpNotFound: {
      return "The provider's id token endpoint cannot be found.";
    }
    case FederatedAuthRequestResult::kIdTokenNoResponse: {
      return "The provider's token fetch resulted in an error response "
             "code.";
    }
    case FederatedAuthRequestResult::kIdTokenInvalidResponse: {
      return "Provider's token is invalid.";
    }
    case FederatedAuthRequestResult::kIdTokenIdpErrorResponse: {
      return "Provider is unable to issue a token, but provided details on the "
             "error that occurred.";
    }
    case FederatedAuthRequestResult::kIdTokenCrossSiteIdpErrorResponse: {
      return "Provider is unable to issue a token, but provided details on the "
             "error that occurred. The error URL must be same-site with the "
             "config URL.";
    }
    case FederatedAuthRequestResult::kIdTokenInvalidContentType: {
      return "Provider's token endpoint content type must be a JSON content "
             "type.";
    }
    case FederatedAuthRequestResult::kCanceled: {
      return "The request has been aborted.";
    }
    case FederatedAuthRequestResult::kRpPageNotVisible: {
      return "RP page is not visible.";
    }
    case FederatedAuthRequestResult::kSilentMediationFailure: {
      return "Silent mediation was requested, but the conditions to achieve it "
             "were not met.";
    }
    case FederatedAuthRequestResult::kThirdPartyCookiesBlocked: {
      return "Third party cookies are blocked. Right now the Chromium "
             "implementation of FedCM API requires third party cookies and "
             "this restriction will be removed soon. In the interim, to test "
             "FedCM without third-party cookies, enable the "
             "#fedcm-without-third-party-cookies flag.";
    }
    case FederatedAuthRequestResult::kMissingTransientUserActivation: {
      return "FedCM active mode requires transient user activation.";
    }
    case FederatedAuthRequestResult::kReplacedByActiveMode: {
      return "The request is replaced by a new one with active mode.";
    }
    case FederatedAuthRequestResult::kNotSignedInWithIdp: {
      return "Not signed in with the identity provider.";
    }
    case FederatedAuthRequestResult::kInvalidFieldsSpecified: {
      return "Invalid 'fields' were specified in the FedCM call.";
    }
    case FederatedAuthRequestResult::kRelyingPartyOriginIsOpaque: {
      return "FedCM is not supported on an opaque origin.";
    }
    case FederatedAuthRequestResult::kTypeNotMatching: {
      return "The requested IdP type did not match the registered IdP.";
    }
    case FederatedAuthRequestResult::kError: {
      return "Error retrieving a token.";
    }
    case FederatedAuthRequestResult::kSuccess: {
      // Should not be called with success, as we should not add a console
      // message for success.
      NOTREACHED();
    }
  }
}

std::string GetDisconnectConsoleErrorMessage(
    FedCmDisconnectStatus disconnect_status_for_metrics) {
  switch (disconnect_status_for_metrics) {
    case FedCmDisconnectStatus::kSuccess: {
      NOTREACHED_IN_MIGRATION();
      return "";
    }
    case FedCmDisconnectStatus::kTooManyRequests: {
      return "There is a pending disconnect() call.";
    }
    case FedCmDisconnectStatus::kUnhandledRequest: {
      return "The disconnect request did not finish by the time the page was "
             "closed.";
    }
    case FedCmDisconnectStatus::kNoAccountToDisconnect: {
      return "There is no account to disconnect.";
    }
    case FedCmDisconnectStatus::kDisconnectUrlIsCrossOrigin: {
      return "The disconnect URL is cross origin";
    }
    case FedCmDisconnectStatus::kDisconnectFailedOnServer: {
      return "The disconnect request failed on the server";
    }
    case FedCmDisconnectStatus::kConfigHttpNotFound: {
      return "The config file cannot be found.";
    }
    case FedCmDisconnectStatus::kConfigNoResponse: {
      return "The config file returned an error response code.";
    }
    case FedCmDisconnectStatus::kConfigInvalidResponse: {
      return "The config file returned some invalid response.";
    }
    case FedCmDisconnectStatus::kDisabledInSettings: {
      return "FedCM is disabled by user settings.";
    }
    case FedCmDisconnectStatus::kDisabledInFlags: {
      return "The disconnect API is disabled by a flag.";
    }
    case FedCmDisconnectStatus::kWellKnownHttpNotFound: {
      return "The well known file cannot be found.";
    }
    case FedCmDisconnectStatus::kWellKnownNoResponse: {
      return "The well-known file returned an error response code.";
    }
    case FedCmDisconnectStatus::kWellKnownInvalidResponse: {
      return "The well-known filed returned some invalid response.";
    }
    case FedCmDisconnectStatus::kWellKnownListEmpty: {
      return "The well-known file returned an empty list.";
    }
    case FedCmDisconnectStatus::kConfigNotInWellKnown: {
      return "The config file is not in the well-known file.";
    }
    case FedCmDisconnectStatus::kWellKnownTooBig: {
      return "Provider's FedCM well-known file contains too many config URLs.";
    }
    case FedCmDisconnectStatus::kWellKnownInvalidContentType: {
      return "Provider's well-known content type must be a JSON content type.";
    }
    case FedCmDisconnectStatus::kConfigInvalidContentType: {
      return "Provider's FedCM config file content type must be a JSON content "
             "type.";
    }
    case FedCmDisconnectStatus::kIdpNotPotentiallyTrustworthy: {
      return "The provider's config file URL is not potentially trustworthy.";
    }
  }
}

FedCmIdpSigninStatusMode GetIdpSigninStatusMode(RenderFrameHost& host,
                                                const url::Origin& idp_origin) {
  // TODO(crbug.com/40283354): Remove this function in favor of
  // GetFedCmIdpSigninStatusFlag.
  return GetFedCmIdpSigninStatusFlag();
}

std::string FormatUrlForDisplay(const GURL& url) {
  // We do not use url_formatter::FormatUrlForSecurityDisplay() directly because
  // our UI intentionally shows only the eTLD+1, as it makes for a shorter text
  // that is also clearer to users. The identity provider's well-known file is
  // in the root of the eTLD+1, and sign-in status within identity provider and
  // relying party can be domain-wide because it relies on cookies.
  std::string formatted_url_str =
      net::IsLocalhost(url)
          ? url.host()
          : net::registry_controlled_domains::GetDomainAndRegistry(
                url, kDefaultPrivateRegistryFilter);
  return base::UTF16ToUTF8(url_formatter::FormatUrlForSecurityDisplay(
      GURL(url.scheme() + "://" + formatted_url_str),
      url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS));
}

bool HasSharingPermissionOrIdpHasThirdPartyCookiesAccess(
    RenderFrameHost& host,
    const GURL& provider_url,
    const url::Origin& embedder_origin,
    const url::Origin& requester_origin,
    const std::optional<std::string>& account_id,
    FederatedIdentityPermissionContextDelegate* sharing_permission_delegate,
    FederatedIdentityApiPermissionContextDelegate* api_permission_delegate) {
  if (api_permission_delegate->HasThirdPartyCookiesAccess(host, provider_url,
                                                          embedder_origin)) {
    return true;
  }
  if (account_id) {
    return sharing_permission_delegate
        ->GetLastUsedTimestamp(requester_origin, embedder_origin,
                               url::Origin::Create(provider_url), *account_id)
        .has_value();
  }
  return sharing_permission_delegate->HasSharingPermission(
      requester_origin, embedder_origin, url::Origin::Create(provider_url));
}

bool IsFedCmAuthzEnabled(RenderFrameHost& host, const url::Origin& idp_origin) {
  RuntimeFeatureStateDocumentData* rfs_document_data =
      RuntimeFeatureStateDocumentData::GetForCurrentDocument(&host);
  // If field trials or an explicit user selection disables authz, we should
  // respect that.
  std::optional<bool> is_overridden = IsFedCmAuthzOverridden();
  if (is_overridden) {
    return *is_overridden;
  }

  // Should not be null as this gets initialized when the host gets created.
  DCHECK(rfs_document_data);
  std::vector<url::Origin> third_party_origins = {idp_origin};
  // This includes origin trials.
  bool runtime_enabled =
      rfs_document_data->runtime_feature_state_read_context()
          .IsFedCmAuthzEnabled() ||
      rfs_document_data->runtime_feature_state_read_context()
          .IsFedCmAuthzEnabledForThirdParty(third_party_origins);

  bool flag_enabled = IsFedCmAuthzFlagEnabled();
  return runtime_enabled || flag_enabled;
}

FederatedAuthRequestPageData* GetPageData(Page& page) {
  return FederatedAuthRequestPageData::GetOrCreateForPage(page);
}

int GetNewSessionID() {
  return base::RandInt(1, 1 << 30);
}

}  // namespace content::webid
