// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/federated_auth_request_impl.h"

#include <random>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/escape.h"
#include "base/strings/string_piece.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "components/url_formatter/elide_url.h"
#include "content/browser/bad_message.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webid/fake_identity_request_dialog_controller.h"
#include "content/browser/webid/flags.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/federated_identity_active_session_permission_context_delegate.h"
#include "content/public/browser/federated_identity_api_permission_context_delegate.h"
#include "content/public/browser/federated_identity_sharing_permission_context_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/page_visibility_state.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom.h"
#include "ui/accessibility/ax_mode.h"
#include "url/url_constants.h"

using blink::mojom::FederatedAuthRequestResult;
using blink::mojom::IdentityProvider;
using blink::mojom::IdentityProviderPtr;
using blink::mojom::LogoutRpsStatus;
using blink::mojom::RequestTokenStatus;
using FederatedApiPermissionStatus =
    content::FederatedIdentityApiPermissionContextDelegate::PermissionStatus;
using TokenStatus = content::FedCmRequestIdTokenStatus;
using SignInStateMatchStatus = content::FedCmSignInStateMatchStatus;
using LoginState = content::IdentityRequestAccount::LoginState;
using SignInMode = content::IdentityRequestAccount::SignInMode;

namespace content {

namespace {
static constexpr base::TimeDelta kDefaultTokenRequestDelay = base::Seconds(3);
static constexpr base::TimeDelta kMaxRejectionTime = base::Seconds(60);

// Maximum number of provider URLs in the manifest list.
// TODO(cbiesinger): Determine what the right number is.
static constexpr size_t kMaxProvidersInManifestList = 1ul;

std::string ComputeUrlEncodedTokenPostData(const std::string& client_id,
                                           const std::string& nonce,
                                           const std::string& account_id,
                                           bool is_sign_in) {
  std::string query;
  if (!client_id.empty())
    query +=
        "client_id=" + base::EscapeUrlEncodedData(client_id, /*use_plus=*/true);

  if (!nonce.empty()) {
    if (!query.empty())
      query += "&";
    query += "nonce=" + base::EscapeUrlEncodedData(nonce, /*use_plus=*/true);
  }

  if (!account_id.empty()) {
    if (!query.empty())
      query += "&";
    query += "account_id=" +
             base::EscapeUrlEncodedData(account_id, /*use_plus=*/true);
  }
  // For new users signing up, we show some disclosure text to remind them about
  // data sharing between IDP and RP. For returning users signing in, such
  // disclosure text is not necessary. This field indicates in the request
  // whether the user has been shown such disclosure text.
  std::string disclosure_text_shown = is_sign_in ? "false" : "true";
  if (!query.empty())
    query += "&disclosure_text_shown=" + disclosure_text_shown;

  return query;
}

std::string GetConsoleErrorMessage(FederatedAuthRequestResult status) {
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
    case FederatedAuthRequestResult::kErrorFetchingManifestListHttpNotFound: {
      return "The provider's FedCM manifest list file cannot be found.";
    }
    case FederatedAuthRequestResult::kErrorFetchingManifestListNoResponse: {
      return "The provider's FedCM manifest list file fetch resulted in an "
             "error response code.";
    }
    case FederatedAuthRequestResult::
        kErrorFetchingManifestListInvalidResponse: {
      return "Provider's FedCM manifest list file is invalid.";
    }
    case FederatedAuthRequestResult::kErrorManifestNotInManifestList: {
      return "Provider's FedCM manifest not listed in its manifest list.";
    }
    case FederatedAuthRequestResult::kErrorManifestListTooBig: {
      return "Provider's FedCM manifest list contains too many providers.";
    }
    case FederatedAuthRequestResult::kErrorFetchingManifestHttpNotFound: {
      return "The provider's FedCM manifest configuration cannot be found.";
    }
    case FederatedAuthRequestResult::kErrorFetchingManifestNoResponse: {
      return "The provider's FedCM manifest configuration fetch resulted in an "
             "error response code.";
    }
    case FederatedAuthRequestResult::kErrorFetchingManifestInvalidResponse: {
      return "Provider's FedCM manifest configuration is invalid.";
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
    case FederatedAuthRequestResult::kErrorCanceled: {
      return "The request has been aborted.";
    }
    case FederatedAuthRequestResult::kErrorRpPageNotVisible: {
      return "RP page is not visible.";
    }
    case FederatedAuthRequestResult::kError: {
      return "Error retrieving a token.";
    }
    case FederatedAuthRequestResult::kSuccess: {
      DCHECK(false);
      return "";
    }
  }
}

RequestTokenStatus FederatedAuthRequestResultToRequestTokenStatus(
    FederatedAuthRequestResult result) {
  // Avoids exposing to renderer detailed error messages which may leak cross
  // site information to the API call site.
  switch (result) {
    case FederatedAuthRequestResult::kSuccess: {
      return RequestTokenStatus::kSuccess;
    }
    case FederatedAuthRequestResult::kErrorTooManyRequests: {
      return RequestTokenStatus::kErrorTooManyRequests;
    }
    case FederatedAuthRequestResult::kErrorCanceled: {
      return RequestTokenStatus::kErrorCanceled;
    }
    case FederatedAuthRequestResult::kShouldEmbargo:
    case FederatedAuthRequestResult::kErrorDisabledInSettings:
    case FederatedAuthRequestResult::kErrorFetchingManifestListHttpNotFound:
    case FederatedAuthRequestResult::kErrorFetchingManifestListNoResponse:
    case FederatedAuthRequestResult::kErrorFetchingManifestListInvalidResponse:
    case FederatedAuthRequestResult::kErrorManifestNotInManifestList:
    case FederatedAuthRequestResult::kErrorManifestListTooBig:
    case FederatedAuthRequestResult::kErrorFetchingManifestHttpNotFound:
    case FederatedAuthRequestResult::kErrorFetchingManifestNoResponse:
    case FederatedAuthRequestResult::kErrorFetchingManifestInvalidResponse:
    case FederatedAuthRequestResult::kErrorFetchingClientMetadataHttpNotFound:
    case FederatedAuthRequestResult::kErrorFetchingClientMetadataNoResponse:
    case FederatedAuthRequestResult::
        kErrorFetchingClientMetadataInvalidResponse:
    case FederatedAuthRequestResult::kErrorFetchingAccountsHttpNotFound:
    case FederatedAuthRequestResult::kErrorFetchingAccountsNoResponse:
    case FederatedAuthRequestResult::kErrorFetchingAccountsInvalidResponse:
    case FederatedAuthRequestResult::kErrorFetchingIdTokenHttpNotFound:
    case FederatedAuthRequestResult::kErrorFetchingIdTokenNoResponse:
    case FederatedAuthRequestResult::kErrorFetchingIdTokenInvalidResponse:
    case FederatedAuthRequestResult::kErrorRpPageNotVisible:
    case FederatedAuthRequestResult::kError: {
      return RequestTokenStatus::kError;
    }
  }
}

IdpNetworkRequestManager::MetricsEndpointErrorCode
FederatedAuthRequestResultToMetricsEndpointErrorCode(
    blink::mojom::FederatedAuthRequestResult result) {
  switch (result) {
    case FederatedAuthRequestResult::kSuccess: {
      return IdpNetworkRequestManager::MetricsEndpointErrorCode::kNone;
    }
    case FederatedAuthRequestResult::kErrorTooManyRequests: {
      return IdpNetworkRequestManager::MetricsEndpointErrorCode::
          kTooManyRequests;
    }
    case FederatedAuthRequestResult::kErrorCanceled: {
      return IdpNetworkRequestManager::MetricsEndpointErrorCode::kErrorCanceled;
    }
    case FederatedAuthRequestResult::kErrorFetchingAccountsInvalidResponse: {
      return IdpNetworkRequestManager::MetricsEndpointErrorCode::
          kAccountsEndpointInvalidResponse;
    }
    case FederatedAuthRequestResult::kErrorFetchingIdTokenInvalidResponse: {
      return IdpNetworkRequestManager::MetricsEndpointErrorCode::
          kTokenEndpointInvalidResponse;
    }
    case FederatedAuthRequestResult::kShouldEmbargo:
    case FederatedAuthRequestResult::kErrorDisabledInSettings:
    case FederatedAuthRequestResult::kErrorRpPageNotVisible: {
      return IdpNetworkRequestManager::MetricsEndpointErrorCode::kUserFailure;
    }
    case FederatedAuthRequestResult::kErrorFetchingManifestListHttpNotFound:
    case FederatedAuthRequestResult::kErrorFetchingManifestListNoResponse:
    case FederatedAuthRequestResult::kErrorFetchingManifestHttpNotFound:
    case FederatedAuthRequestResult::kErrorFetchingManifestNoResponse:
    case FederatedAuthRequestResult::kErrorFetchingClientMetadataHttpNotFound:
    case FederatedAuthRequestResult::kErrorFetchingClientMetadataNoResponse:
    case FederatedAuthRequestResult::kErrorFetchingAccountsHttpNotFound:
    case FederatedAuthRequestResult::kErrorFetchingAccountsNoResponse:
    case FederatedAuthRequestResult::kErrorFetchingIdTokenHttpNotFound:
    case FederatedAuthRequestResult::kErrorFetchingIdTokenNoResponse: {
      return IdpNetworkRequestManager::MetricsEndpointErrorCode::
          kIdpServerUnavailable;
    }
    case FederatedAuthRequestResult::kErrorManifestNotInManifestList:
    case FederatedAuthRequestResult::kErrorManifestListTooBig: {
      return IdpNetworkRequestManager::MetricsEndpointErrorCode::kManifestError;
    }
    case FederatedAuthRequestResult::kErrorFetchingManifestListInvalidResponse:
    case FederatedAuthRequestResult::kErrorFetchingManifestInvalidResponse:
    case FederatedAuthRequestResult::
        kErrorFetchingClientMetadataInvalidResponse: {
      return IdpNetworkRequestManager::MetricsEndpointErrorCode::
          kIdpServerInvalidResponse;
    }
    case FederatedAuthRequestResult::kError: {
      return IdpNetworkRequestManager::MetricsEndpointErrorCode::kOther;
    }
  }
}

// TODO(crbug.com/1344150): Use normal distribution after sufficient data is
// collected.
base::TimeDelta GetRandomRejectionTime() {
  return kMaxRejectionTime * base::RandDouble();
}

std::string FormatUrlForDisplay(const GURL& url) {
  // We do not use url_formatter::FormatUrlForSecurityDisplay() directly because
  // our UI intentionally shows only the eTLD+1, as it makes for a shorter text
  // that is also clearer to users. The identity provider's root manifest is in
  // the root of the eTLD+1, and sign-in status within identity provider and
  // relying party can be domain-wide because it relies on cookies.
  std::string formatted_url_str =
      net::IsLocalhost(url)
          ? url.host()
          : net::registry_controlled_domains::GetDomainAndRegistry(
                url,
                net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  return base::UTF16ToUTF8(url_formatter::FormatUrlForSecurityDisplay(
      GURL(url.scheme() + "://" + formatted_url_str),
      url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS));
}

bool ShouldFailIfNotSignedInWithIdp(
    const GURL& idp_url,
    FederatedIdentitySharingPermissionContextDelegate*
        sharing_permission_delegate) {
  if (!IsFedCmIdpSigninStatusEnabled())
    return false;

  const url::Origin idp_origin = url::Origin::Create(idp_url);
  const absl::optional<bool> idp_signin_status =
      sharing_permission_delegate->GetIdpSigninStatus(idp_origin);
  if (!idp_signin_status.value_or(true))
    return true;

  return false;
}

}  // namespace

FederatedAuthRequestImpl::IdentityProviderInfo::IdentityProviderInfo() =
    default;
FederatedAuthRequestImpl::IdentityProviderInfo::~IdentityProviderInfo() =
    default;
FederatedAuthRequestImpl::IdentityProviderInfo::IdentityProviderInfo(
    const IdentityProviderInfo&) = default;

FederatedAuthRequestImpl::Endpoints::Endpoints() = default;
FederatedAuthRequestImpl::Endpoints::~Endpoints() = default;
FederatedAuthRequestImpl::Endpoints::Endpoints(const Endpoints&) = default;

FederatedAuthRequestImpl::FederatedAuthRequestImpl(
    RenderFrameHost& host,
    FederatedIdentityApiPermissionContextDelegate* api_permission_context,
    FederatedIdentityActiveSessionPermissionContextDelegate*
        active_session_permission_context,
    FederatedIdentitySharingPermissionContextDelegate*
        sharing_permission_context,
    mojo::PendingReceiver<blink::mojom::FederatedAuthRequest> receiver)
    : DocumentService(host, std::move(receiver)),
      api_permission_delegate_(api_permission_context),
      active_session_permission_delegate_(active_session_permission_context),
      sharing_permission_delegate_(sharing_permission_context),
      token_request_delay_(kDefaultTokenRequestDelay) {}

FederatedAuthRequestImpl::~FederatedAuthRequestImpl() {
  // Ensures key data members are destructed in proper order and resolves any
  // pending promise.
  if (auth_request_callback_) {
    DCHECK(!logout_callback_);
    CompleteRequestWithError(FederatedAuthRequestResult::kError,
                             TokenStatus::kUnhandledRequest,
                             /*should_delay_callback=*/false);
  }
}

// static
void FederatedAuthRequestImpl::Create(
    RenderFrameHost* host,
    mojo::PendingReceiver<blink::mojom::FederatedAuthRequest> receiver) {
  CHECK(host);

  BrowserContext* browser_context = host->GetBrowserContext();
  raw_ptr<FederatedIdentityApiPermissionContextDelegate>
      api_permission_context =
          browser_context->GetFederatedIdentityApiPermissionContext();
  raw_ptr<FederatedIdentityActiveSessionPermissionContextDelegate>
      active_session_permission_context =
          browser_context->GetFederatedIdentityActiveSessionPermissionContext();
  raw_ptr<FederatedIdentitySharingPermissionContextDelegate>
      sharing_permission_context =
          browser_context->GetFederatedIdentitySharingPermissionContext();
  if (!api_permission_context || !active_session_permission_context ||
      !sharing_permission_context) {
    return;
  }

  // FederatedAuthRequestImpl owns itself. It will self-destruct when a mojo
  // interface error occurs, the RenderFrameHost is deleted, or the
  // RenderFrameHost navigates to a new document.
  new FederatedAuthRequestImpl(*host, api_permission_context,
                               active_session_permission_context,
                               sharing_permission_context, std::move(receiver));
}

FederatedAuthRequestImpl& FederatedAuthRequestImpl::CreateForTesting(
    RenderFrameHost& host,
    FederatedIdentityApiPermissionContextDelegate* api_permission_context,
    FederatedIdentityActiveSessionPermissionContextDelegate*
        active_session_permission_context,
    FederatedIdentitySharingPermissionContextDelegate*
        sharing_permission_context,
    mojo::PendingReceiver<blink::mojom::FederatedAuthRequest> receiver) {
  return *new FederatedAuthRequestImpl(
      host, api_permission_context, active_session_permission_context,
      sharing_permission_context, std::move(receiver));
}

void FederatedAuthRequestImpl::RequestToken(
    std::vector<IdentityProviderPtr> idp_ptrs,
    bool prefer_auto_sign_in,
    bool show_iframe_requester,
    RequestTokenCallback callback) {
  if (idp_ptrs.empty()) {
    std::move(callback).Run(RequestTokenStatus::kError, "");
    return;
  }
  // It should not be possible to receive multiple IDPs when the
  // `kFedCmMultipleIdentityProviders` flag is disabled. But such a message
  // could be received from a compromised renderer.
  if (idp_ptrs.size() > 1u && !base::FeatureList::IsEnabled(
                                  features::kFedCmMultipleIdentityProviders)) {
    std::move(callback).Run(RequestTokenStatus::kError, "");
    return;
  }

  if (HasPendingRequest()) {
    fedcm_metrics_->RecordRequestTokenStatus(TokenStatus::kTooManyRequests);
    std::move(callback).Run(RequestTokenStatus::kErrorTooManyRequests, "");
    return;
  }

  auth_request_callback_ = std::move(callback);
  network_manager_ = CreateNetworkManager();
  request_dialog_controller_ = CreateDialogController();

  // TODO(crbug.com/1361642): Handle cases where not all IDPs' requests are
  // successful. Currently when multiple IDPs are specified, an accounts
  // dialog is shown only when the last IDP's request is successful.
  for (auto& idp_ptr : idp_ptrs) {
    pending_idps_.insert(idp_ptr->config_url);

    if (!fedcm_metrics_) {
      // Generate a random int for the FedCM call, to be used by the UKM events.
      std::random_device dev;
      std::mt19937 rng(dev());
      std::uniform_int_distribution<std::mt19937::result_type> uniform_dist(
          1, 1 << 30);
      // TODO(crbug.com/1307709): Handle FedCmMetrics for multiple IDPs.
      fedcm_metrics_ = std::make_unique<FedCmMetrics>(
          idp_ptr->config_url, render_frame_host().GetPageUkmSourceId(),
          uniform_dist(rng),
          /*is_disabled=*/idp_ptrs.size() > 1);
    }
    prefer_auto_sign_in_ = prefer_auto_sign_in && IsFedCmAutoSigninEnabled();
    start_time_ = base::TimeTicks::Now();

    if (!network::IsOriginPotentiallyTrustworthy(
            url::Origin::Create(idp_ptr->config_url))) {
      CompleteRequestWithError(FederatedAuthRequestResult::kError,
                               TokenStatus::kIdpNotPotentiallyTrustworthy,
                               /*should_delay_callback=*/false);
      return;
    }

    FederatedApiPermissionStatus permission_status =
        api_permission_delegate_->GetApiPermissionStatus(GetEmbeddingOrigin());

    absl::optional<TokenStatus> error_token_status;
    FederatedAuthRequestResult request_result =
        FederatedAuthRequestResult::kError;

    switch (permission_status) {
      case FederatedApiPermissionStatus::BLOCKED_VARIATIONS:
        error_token_status = TokenStatus::kDisabledInFlags;
        break;
      case FederatedApiPermissionStatus::BLOCKED_THIRD_PARTY_COOKIES_BLOCKED:
        error_token_status = TokenStatus::kThirdPartyCookiesBlocked;
        break;
      case FederatedApiPermissionStatus::BLOCKED_SETTINGS:
        error_token_status = TokenStatus::kDisabledInSettings;
        request_result = FederatedAuthRequestResult::kErrorDisabledInSettings;
        break;
      case FederatedApiPermissionStatus::BLOCKED_EMBARGO:
        error_token_status = TokenStatus::kDisabledEmbargo;
        request_result = FederatedAuthRequestResult::kErrorDisabledInSettings;
        break;
      case FederatedApiPermissionStatus::GRANTED:
        // Intentional fall-through.
        break;
      default:
        NOTREACHED();
        break;
    }

    if (error_token_status) {
      CompleteRequestWithError(request_result, *error_token_status,
                               /*should_delay_callback=*/true);
      return;
    }

    if (ShouldFailIfNotSignedInWithIdp(idp_ptr->config_url,
                                       sharing_permission_delegate_)) {
      CompleteRequestWithError(FederatedAuthRequestResult::kError,
                               TokenStatus::kNotSignedInWithIdp,
                               /*should_delay_callback=*/true);
      return;
    }

    idp_info_[idp_ptr->config_url].provider = *idp_ptr;
    idp_order_.push_back(idp_ptr->config_url);
    FetchManifest(*idp_ptr);
  }
}

void FederatedAuthRequestImpl::CancelTokenRequest() {
  if (!auth_request_callback_)
    return;

  // Dialog will be hidden by the destructor for request_dialog_controller_,
  // triggered by CompleteRequest.

  CompleteRequestWithError(FederatedAuthRequestResult::kErrorCanceled,
                           TokenStatus::kAborted,
                           /*should_delay_callback=*/false);
}

// TODO(kenrb): Depending on how this code evolves, it might make sense to
// spin session management code into its own service. The prohibition on
// making authentication requests and logout requests at the same time, while
// not problematic for any plausible use case, need not be strictly necessary
// if there is a good way to not have to resource contention between requests.
// https://crbug.com/1200581
void FederatedAuthRequestImpl::LogoutRps(
    std::vector<blink::mojom::LogoutRpsRequestPtr> logout_requests,
    LogoutRpsCallback callback) {
  if (HasPendingRequest()) {
    std::move(callback).Run(LogoutRpsStatus::kErrorTooManyRequests);
    return;
  }

  DCHECK(logout_requests_.empty());

  logout_callback_ = std::move(callback);

  if (logout_requests.empty()) {
    CompleteLogoutRequest(LogoutRpsStatus::kError);
    return;
  }

  if (base::ranges::any_of(logout_requests, [](auto& request) {
        return !request->url.is_valid();
      })) {
    bad_message::ReceivedBadMessage(render_frame_host().GetProcess(),
                                    bad_message::FARI_LOGOUT_BAD_ENDPOINT);
    CompleteLogoutRequest(LogoutRpsStatus::kError);
    return;
  }

  for (auto& request : logout_requests) {
    logout_requests_.push(std::move(request));
  }

  if (!network::IsOriginPotentiallyTrustworthy(origin())) {
    CompleteLogoutRequest(LogoutRpsStatus::kError);
    return;
  }

  network_manager_ = CreateNetworkManager();

  if (!IsFedCmIdpSignoutEnabled()) {
    CompleteLogoutRequest(LogoutRpsStatus::kError);
    return;
  }

  if (api_permission_delegate_->GetApiPermissionStatus(GetEmbeddingOrigin()) !=
      FederatedApiPermissionStatus::GRANTED) {
    CompleteLogoutRequest(LogoutRpsStatus::kError);
    return;
  }

  // TODO(kenrb): These should be parallelized rather than being dispatched
  // serially. https://crbug.com/1200581.
  DispatchOneLogout();
}

bool FederatedAuthRequestImpl::HasPendingRequest() const {
  return auth_request_callback_ || logout_callback_;
}

GURL FederatedAuthRequestImpl::ResolveManifestUrl(const IdentityProvider& idp,
                                                  const std::string& endpoint) {
  if (endpoint.empty())
    return GURL();
  GURL manifest_url =
      idp.config_url.Resolve(IdpNetworkRequestManager::kManifestFilePath);
  return manifest_url.Resolve(endpoint);
}

bool FederatedAuthRequestImpl::IsEndpointUrlValid(const IdentityProvider& idp,
                                                  const GURL& endpoint_url) {
  return url::Origin::Create(idp.config_url).IsSameOriginWith(endpoint_url);
}

void FederatedAuthRequestImpl::FetchManifest(const IdentityProvider& idp) {
  absl::optional<int> icon_ideal_size = absl::nullopt;
  absl::optional<int> icon_minimum_size = absl::nullopt;
  if (request_dialog_controller_) {
    icon_ideal_size = request_dialog_controller_->GetBrandIconIdealSize();
    icon_minimum_size = request_dialog_controller_->GetBrandIconMinimumSize();
  }

  IdpNetworkRequestManager::FetchManifestCallback manifest_callback =
      base::BindOnce(&FederatedAuthRequestImpl::OnManifestFetched,
                     weak_ptr_factory_.GetWeakPtr(), idp);
  IdpNetworkRequestManager::FetchManifestListCallback manifest_list_callback =
      base::BindOnce(&FederatedAuthRequestImpl::OnManifestListFetched,
                     weak_ptr_factory_.GetWeakPtr(), idp);

  if (IsFedCmManifestValidationEnabled()) {
    network_manager_->FetchManifestList(idp.config_url,
                                        std::move(manifest_list_callback));
  } else {
    idp_info_[idp.config_url].manifest_list_checked = true;
  }
  network_manager_->FetchManifest(idp.config_url, icon_ideal_size,
                                  icon_minimum_size,
                                  std::move(manifest_callback));
}

void FederatedAuthRequestImpl::OnManifestListFetched(
    const IdentityProvider& idp,
    IdpNetworkRequestManager::FetchStatus status,
    const std::set<GURL>& urls) {
  switch (status) {
    case IdpNetworkRequestManager::FetchStatus::kHttpNotFoundError: {
      CompleteRequestWithError(
          FederatedAuthRequestResult::kErrorFetchingManifestListHttpNotFound,
          TokenStatus::kManifestListHttpNotFound,
          /*should_delay_callback=*/true);
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kNoResponseError: {
      CompleteRequestWithError(
          FederatedAuthRequestResult::kErrorFetchingManifestListNoResponse,
          TokenStatus::kManifestListNoResponse,
          /*should_delay_callback=*/true);
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kInvalidResponseError: {
      CompleteRequestWithError(
          FederatedAuthRequestResult::kErrorFetchingManifestListInvalidResponse,
          TokenStatus::kManifestListInvalidResponse,
          /*should_delay_callback=*/true);
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kSuccess: {
      // Intentional fall-through.
    }
  }

  if (urls.size() > kMaxProvidersInManifestList) {
    CompleteRequestWithError(
        FederatedAuthRequestResult::kErrorManifestListTooBig,
        TokenStatus::kManifestListTooBig,
        /*should_delay_callback=*/true);
    return;
  }

  // The provider url from the API call:
  // navigator.credentials.get({
  //   federated: {
  //     providers: [{
  //       configURL: "https://foo.idp.example/fedcm.json",
  //       clientId: "1234"
  //     }],
  //   }
  // });
  // must match the one in the manifest list:
  // {
  //   "provider_urls": [
  //     "https://foo.idp.example/fedcm.json"
  //   ]
  // }
  bool provider_url_is_valid = (urls.count(idp.config_url) != 0);

  if (!provider_url_is_valid) {
    CompleteRequestWithError(
        FederatedAuthRequestResult::kErrorManifestNotInManifestList,
        TokenStatus::kManifestNotInManifestList,
        /*should_delay_callback=*/true);
    return;
  }

  idp_info_[idp.config_url].manifest_list_checked = true;
  if (idp_info_[idp.config_url].metadata)
    OnManifestReady(idp_info_[idp.config_url]);
}

void FederatedAuthRequestImpl::OnManifestFetched(
    const IdentityProvider& idp,
    IdpNetworkRequestManager::FetchStatus status,
    IdpNetworkRequestManager::Endpoints endpoints,
    IdentityProviderMetadata idp_metadata) {
  switch (status) {
    case IdpNetworkRequestManager::FetchStatus::kHttpNotFoundError: {
      CompleteRequestWithError(
          FederatedAuthRequestResult::kErrorFetchingManifestHttpNotFound,
          TokenStatus::kManifestHttpNotFound,
          /*should_delay_callback=*/true);
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kNoResponseError: {
      CompleteRequestWithError(
          FederatedAuthRequestResult::kErrorFetchingManifestNoResponse,
          TokenStatus::kManifestNoResponse,
          /*should_delay_callback=*/true);
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kInvalidResponseError: {
      CompleteRequestWithError(
          FederatedAuthRequestResult::kErrorFetchingManifestInvalidResponse,
          TokenStatus::kManifestInvalidResponse,
          /*should_delay_callback=*/true);
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kSuccess: {
      // Intentional fall-through.
    }
  }

  idp_info_[idp.config_url].endpoints.token =
      ResolveManifestUrl(idp, endpoints.token);
  idp_info_[idp.config_url].endpoints.accounts =
      ResolveManifestUrl(idp, endpoints.accounts);
  idp_info_[idp.config_url].endpoints.client_metadata =
      ResolveManifestUrl(idp, endpoints.client_metadata);
  // TODO(crbug.com/1307709): Fix metrics endpoint for multi IDPs.
  endpoints_.metrics = ResolveManifestUrl(idp, endpoints.metrics);
  idp_info_[idp.config_url].metadata = idp_metadata;

  if (idp_info_[idp.config_url].manifest_list_checked)
    OnManifestReady(idp_info_[idp.config_url]);
}

void FederatedAuthRequestImpl::OnManifestReady(
    const IdentityProviderInfo& idp_info) {
  bool is_token_valid =
      IsEndpointUrlValid(idp_info.provider, idp_info.endpoints.token);
  bool is_accounts_valid =
      IsEndpointUrlValid(idp_info.provider, idp_info.endpoints.accounts);
  if (!is_token_valid || !is_accounts_valid) {
    std::string message =
        "Manifest is missing or has an invalid URL for the following "
        "endpoints:\n";
    if (!is_token_valid) {
      message += "\"id_token_endpoint\"\n";
    }
    if (!is_accounts_valid) {
      message += "\"accounts_endpoint\"\n";
    }
    render_frame_host().AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError, message);
    CompleteRequestWithError(
        FederatedAuthRequestResult::kErrorFetchingManifestInvalidResponse,
        TokenStatus::kManifestInvalidResponse,
        /*should_delay_callback=*/true);
    return;
  }
  if (IsEndpointUrlValid(idp_info.provider,
                         idp_info.endpoints.client_metadata)) {
    network_manager_->FetchClientMetadata(
        idp_info.endpoints.client_metadata, idp_info.provider.client_id,
        base::BindOnce(
            &FederatedAuthRequestImpl::OnClientMetadataResponseReceived,
            weak_ptr_factory_.GetWeakPtr(),
            idp_info_[idp_info.provider.config_url]));
  } else {
    MaybeFetchAccounts(idp_info);
  }
}

void FederatedAuthRequestImpl::OnClientMetadataResponseReceived(
    const IdentityProviderInfo& idp_info,
    IdpNetworkRequestManager::FetchStatus status,
    IdpNetworkRequestManager::ClientMetadata data) {
  // TODO(yigu): Clean up the client metadata related errors for metrics and
  // console logs.
  client_metadata_ = data;
  MaybeFetchAccounts(idp_info);
}

void FederatedAuthRequestImpl::MaybeFetchAccounts(
    const IdentityProviderInfo& idp_info) {
  // Make sure that we don't fetch accounts if the IDP sign-in bit is reset to
  // false during the API call. e.g. by the login/logout HEADER.
  if (ShouldFailIfNotSignedInWithIdp(idp_info.provider.config_url,
                                     sharing_permission_delegate_)) {
    CompleteRequestWithError(FederatedAuthRequestResult::kError,
                             TokenStatus::kNotSignedInWithIdp,
                             /*should_delay_callback=*/true);
    return;
  }

  network_manager_->SendAccountsRequest(
      idp_info.endpoints.accounts, idp_info.provider.client_id,
      base::BindOnce(&FederatedAuthRequestImpl::OnAccountsResponseReceived,
                     weak_ptr_factory_.GetWeakPtr(),
                     idp_info_[idp_info.provider.config_url]));
}

void FederatedAuthRequestImpl::HandleAccountsFetchFailure(
    const GURL& idp_url,
    blink::mojom::FederatedAuthRequestResult result,
    absl::optional<TokenStatus> token_status) {
  if (!IsFedCmIdpSigninStatusEnabled()) {
    CompleteRequestWithError(result, token_status,
                             /*should_delay_callback=*/true);
    return;
  }

  const url::Origin idp_origin = url::Origin::Create(idp_url);
  const absl::optional<bool> idp_signin_status =
      sharing_permission_delegate_->GetIdpSigninStatus(idp_origin);
  // Ensures that we only fetch accounts unconditionally once.
  if (!idp_signin_status.has_value()) {
    sharing_permission_delegate_->SetIdpSigninStatus(idp_origin, false);
    CompleteRequestWithError(result, token_status,
                             /*should_delay_callback=*/true);
    return;
  }

  sharing_permission_delegate_->SetIdpSigninStatus(idp_origin, false);
  // TODO(crbug.com/1357790): we should figure out how to handle multiple IDP
  // w.r.t. showing a static failure UI. e.g. one IDP is always successful and
  // one always returns 404.
  WebContents* rp_web_contents =
      WebContents::FromRenderFrameHost(&render_frame_host());
  DCHECK(render_frame_host().GetMainFrame()->IsInPrimaryMainFrame());

  request_dialog_controller_->ShowFailureDialog(
      rp_web_contents,
      FormatUrlForDisplay(rp_web_contents->GetLastCommittedURL()),
      FormatUrlForDisplay(idp_url),
      base::BindOnce(
          &FederatedAuthRequestImpl::OnDismissFailureDialog,
          weak_ptr_factory_.GetWeakPtr(), FederatedAuthRequestResult::kError,
          TokenStatus::kNotSignedInWithIdp, /*should_delay_callback=*/true));
}

void FederatedAuthRequestImpl::OnAccountsResponseReceived(
    const IdentityProviderInfo& idp_info,
    IdpNetworkRequestManager::FetchStatus status,
    IdpNetworkRequestManager::AccountList accounts) {
  switch (status) {
    case IdpNetworkRequestManager::FetchStatus::kHttpNotFoundError: {
      HandleAccountsFetchFailure(
          idp_info.provider.config_url,
          FederatedAuthRequestResult::kErrorFetchingAccountsHttpNotFound,
          TokenStatus::kAccountsHttpNotFound);
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kNoResponseError: {
      HandleAccountsFetchFailure(
          idp_info.provider.config_url,
          FederatedAuthRequestResult::kErrorFetchingAccountsNoResponse,
          TokenStatus::kAccountsNoResponse);
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kInvalidResponseError: {
      HandleAccountsFetchFailure(
          idp_info.provider.config_url,
          FederatedAuthRequestResult::kErrorFetchingAccountsInvalidResponse,
          TokenStatus::kAccountsInvalidResponse);
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kSuccess: {
      const url::Origin idp_origin =
          url::Origin::Create(idp_info.provider.config_url);
      sharing_permission_delegate_->SetIdpSigninStatus(idp_origin, true);

      bool is_visible = (render_frame_host().IsActive() &&
                         render_frame_host().GetVisibilityState() ==
                             content::PageVisibilityState::kVisible);
      fedcm_metrics_->RecordWebContentsVisibilityUponReadyToShowDialog(
          is_visible);
      // Does not show the dialog if the user has left the page. e.g. they may
      // open a new tab before browser is ready to show the dialog.
      if (!is_visible) {
        CompleteRequestWithError(
            FederatedAuthRequestResult::kErrorRpPageNotVisible,
            TokenStatus::kRpPageNotVisible,
            /*should_delay_callback=*/true);
        return;
      }

      WebContents* rp_web_contents =
          WebContents::FromRenderFrameHost(&render_frame_host());
      DCHECK(render_frame_host().GetMainFrame()->IsInPrimaryMainFrame());

      ComputeLoginStateAndReorderAccounts(idp_info.provider, accounts);

      bool screen_reader_is_on =
          rp_web_contents->GetAccessibilityMode().has_mode(
              ui::AXMode::kScreenReader);
      // Auto signs in returning users if they have a single account and are
      // signing in.
      // TODO(yigu): Add additional controls for RP/IDP/User for this flow.
      // https://crbug.com/1236678.
      bool is_auto_sign_in = prefer_auto_sign_in_ && accounts.size() == 1 &&
                             accounts[0].login_state == LoginState::kSignIn &&
                             !screen_reader_is_on;
      // TODO(cbiesinger): Check that the URLs are valid.
      ClientIdData client_id_data{GURL(client_metadata_.terms_of_service_url),
                                  GURL(client_metadata_.privacy_policy_url)};

      show_accounts_dialog_time_ = base::TimeTicks::Now();
      fedcm_metrics_->RecordShowAccountsDialogTime(show_accounts_dialog_time_ -
                                                   start_time_);

      std::string idp_for_display =
          FormatUrlForDisplay(idp_info.provider.config_url);
      IdentityProviderData idp_data(idp_for_display, accounts,
                                    *idp_info.metadata, client_id_data);
      idp_data_.insert({idp_info.provider.config_url, idp_data});

      pending_idps_.erase(idp_info.provider.config_url);
      if (!pending_idps_.empty())
        return;

      std::string rp_url_for_display =
          FormatUrlForDisplay(rp_web_contents->GetLastCommittedURL());

      std::vector<IdentityProviderData> idp_data_for_display;
      for (const auto& idp : idp_order_) {
        if (idp_data_.count(idp))
          idp_data_for_display.push_back(idp_data_.at(idp));
      }

      request_dialog_controller_->ShowAccountsDialog(
          rp_web_contents, rp_url_for_display, idp_data_for_display,
          is_auto_sign_in ? SignInMode::kAuto : SignInMode::kExplicit,
          base::BindOnce(&FederatedAuthRequestImpl::OnAccountSelected,
                         weak_ptr_factory_.GetWeakPtr()),
          base::BindOnce(&FederatedAuthRequestImpl::OnDialogDismissed,
                         weak_ptr_factory_.GetWeakPtr()));
      return;
    }
  }
}

void FederatedAuthRequestImpl::ComputeLoginStateAndReorderAccounts(
    const IdentityProvider& idp,
    IdpNetworkRequestManager::AccountList& accounts) {
  // Populate the accounts login state.
  for (auto& account : accounts) {
    // Record when IDP and browser have different user sign-in states.
    bool idp_claimed_sign_in = account.login_state == LoginState::kSignIn;
    bool browser_observed_sign_in =
        sharing_permission_delegate_->HasSharingPermission(
            origin(), GetEmbeddingOrigin(), url::Origin::Create(idp.config_url),
            account.id);

    if (idp_claimed_sign_in == browser_observed_sign_in) {
      fedcm_metrics_->RecordSignInStateMatchStatus(
          SignInStateMatchStatus::kMatch);
    } else if (idp_claimed_sign_in) {
      fedcm_metrics_->RecordSignInStateMatchStatus(
          SignInStateMatchStatus::kIdpClaimedSignIn);
    } else {
      fedcm_metrics_->RecordSignInStateMatchStatus(
          SignInStateMatchStatus::kBrowserObservedSignIn);
    }

    // We set the login state based on the IDP response if it sends
    // back an approved_clients list. If it does not, we need to set
    // it here based on browser state.
    if (account.login_state)
      continue;
    LoginState login_state = LoginState::kSignUp;
    // Consider this a sign-in if we have seen a successful sign-up for
    // this account before.
    if (browser_observed_sign_in) {
      login_state = LoginState::kSignIn;
    }
    account.login_state = login_state;
  }

  // Now that the login states have been computed, order accounts so that the
  // returning accounts go first and the other accounts go afterwards. Since the
  // number of accounts is likely very small, sorting by login_state should be
  // fast.
  std::sort(accounts.begin(), accounts.end(), [](const auto& a, const auto& b) {
    return a.login_state < b.login_state;
  });
}

void FederatedAuthRequestImpl::OnAccountSelected(const GURL& idp_config_url,
                                                 const std::string& account_id,
                                                 bool is_sign_in) {
  DCHECK(!account_id.empty());
  const IdentityProviderInfo& idp_info = idp_info_[idp_config_url];

  // Check if the user has disabled the FedCM API after the FedCM UI is
  // displayed. This ensures that requests are not wrongfully sent to IDPs when
  // settings are changed while an existing FedCM UI is displayed. Ideally, we
  // should enforce this check before all requests but users typically won't
  // have time to disable the FedCM API in other types of requests.
  if (api_permission_delegate_->GetApiPermissionStatus(GetEmbeddingOrigin()) !=
      FederatedApiPermissionStatus::GRANTED) {
    CompleteRequestWithError(
        FederatedAuthRequestResult::kErrorDisabledInSettings,
        TokenStatus::kDisabledInSettings,
        /*should_delay_callback=*/true);
    return;
  }

  fedcm_metrics_->RecordIsSignInUser(is_sign_in);

  api_permission_delegate_->RemoveEmbargoAndResetCounts(GetEmbeddingOrigin());

  account_id_ = account_id;
  select_account_time_ = base::TimeTicks::Now();
  fedcm_metrics_->RecordContinueOnDialogTime(select_account_time_ -
                                             show_accounts_dialog_time_);

  network_manager_->SendTokenRequest(
      idp_info.endpoints.token, account_id_,
      ComputeUrlEncodedTokenPostData(idp_info.provider.client_id,
                                     idp_info.provider.nonce, account_id,
                                     is_sign_in),
      base::BindOnce(&FederatedAuthRequestImpl::OnTokenResponseReceived,
                     weak_ptr_factory_.GetWeakPtr(), idp_info.provider));
}

void FederatedAuthRequestImpl::OnDismissFailureDialog(
    blink::mojom::FederatedAuthRequestResult result,
    absl::optional<TokenStatus> token_status,
    bool should_delay_callback,
    IdentityRequestDialogController::DismissReason dismiss_reason) {
  CompleteRequest(result, token_status, /*token=*/"", should_delay_callback);
}

void FederatedAuthRequestImpl::OnDialogDismissed(
    IdentityRequestDialogController::DismissReason dismiss_reason) {
  // Clicking the close button and swiping away the account chooser are more
  // intentional than other ways of dismissing the account chooser such as
  // the virtual keyboard showing on Android.
  bool should_embargo = false;
  switch (dismiss_reason) {
    case IdentityRequestDialogController::DismissReason::CLOSE_BUTTON:
    case IdentityRequestDialogController::DismissReason::SWIPE:
      should_embargo = true;
      break;
    default:
      break;
  }

  if (should_embargo) {
    base::TimeTicks dismiss_dialog_time = base::TimeTicks::Now();
    fedcm_metrics_->RecordCancelOnDialogTime(dismiss_dialog_time -
                                             show_accounts_dialog_time_);
  }
  fedcm_metrics_->RecordCancelReason(dismiss_reason);

  if (should_embargo) {
    api_permission_delegate_->RecordDismissAndEmbargo(GetEmbeddingOrigin());
  }

  // Reject the promise immediately if the UI is dismissed without selecting
  // an account. Meanwhile, we fuzz the rejection time for other failures to
  // make it indistinguishable.
  CompleteRequestWithError(should_embargo
                               ? FederatedAuthRequestResult::kShouldEmbargo
                               : FederatedAuthRequestResult::kError,
                           should_embargo ? TokenStatus::kShouldEmbargo
                                          : TokenStatus::kNotSelectAccount,
                           /*should_delay_callback=*/false);
}

void FederatedAuthRequestImpl::OnTokenResponseReceived(
    const IdentityProvider& idp,
    IdpNetworkRequestManager::FetchStatus status,
    const std::string& id_token) {
  if (!auth_request_callback_)
    return;

  // When fetching id tokens we show a "Verify" sheet to users in case fetching
  // takes a long time due to latency etc.. In case that the fetching process is
  // fast, we still want to show the "Verify" sheet for at least
  // |token_request_delay_| seconds for better UX.
  token_response_time_ = base::TimeTicks::Now();
  base::TimeDelta fetch_time = token_response_time_ - select_account_time_;
  if (ShouldCompleteRequestImmediately() ||
      fetch_time >= token_request_delay_) {
    CompleteTokenRequest(idp, status, id_token);
    return;
  }

  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FederatedAuthRequestImpl::CompleteTokenRequest,
                     weak_ptr_factory_.GetWeakPtr(), idp, status, id_token),
      token_request_delay_ - fetch_time);
}

void FederatedAuthRequestImpl::CompleteTokenRequest(
    const IdentityProvider& idp,
    IdpNetworkRequestManager::FetchStatus status,
    const std::string& token) {
  DCHECK(!start_time_.is_null());
  switch (status) {
    case IdpNetworkRequestManager::FetchStatus::kHttpNotFoundError: {
      CompleteRequestWithError(
          FederatedAuthRequestResult::kErrorFetchingIdTokenHttpNotFound,
          TokenStatus::kIdTokenHttpNotFound,
          /*should_delay_callback=*/true);
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kNoResponseError: {
      CompleteRequestWithError(
          FederatedAuthRequestResult::kErrorFetchingIdTokenNoResponse,
          TokenStatus::kIdTokenNoResponse,
          /*should_delay_callback=*/true);
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kInvalidResponseError: {
      CompleteRequestWithError(
          FederatedAuthRequestResult::kErrorFetchingIdTokenInvalidResponse,
          TokenStatus::kIdTokenInvalidResponse,
          /*should_delay_callback=*/true);
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kSuccess: {
      // Grant sharing permission specific to *this account*.
      //
      // TODO(majidvp): But wait which account?
      //   1) The account that user selected in our UI (i.e., account_id_) or
      //   2) The one for which the IDP generated a token.
      //
      // Ideally these are one and the same but currently there is no
      // enforcement for that equality so they could be different. In the
      // future we may want to enforce that the token account (aka subject)
      // matches the user selected account. But for now these questions are
      // moot since we don't actually inspect the returned idtoken.
      // https://crbug.com/1199088
      CHECK(!account_id_.empty());
      sharing_permission_delegate_->GrantSharingPermission(
          origin(), GetEmbeddingOrigin(), url::Origin::Create(idp.config_url),
          account_id_);

      active_session_permission_delegate_->GrantActiveSession(
          origin(), url::Origin::Create(idp.config_url), account_id_);

      fedcm_metrics_->RecordTokenResponseAndTurnaroundTime(
          token_response_time_ - select_account_time_,
          token_response_time_ - start_time_);

      if (endpoints_.metrics.is_valid()) {
        network_manager_->SendSuccessfulTokenRequestMetrics(
            endpoints_.metrics, show_accounts_dialog_time_ - start_time_,
            select_account_time_ - show_accounts_dialog_time_,
            token_response_time_ - select_account_time_,
            token_response_time_ - start_time_);
      }

      CompleteRequest(FederatedAuthRequestResult::kSuccess,
                      TokenStatus::kSuccess, token,
                      /*should_delay_callback=*/false);
      return;
    }
  }
}

void FederatedAuthRequestImpl::DispatchOneLogout() {
  auto logout_request = std::move(logout_requests_.front());
  DCHECK(logout_request->url.is_valid());
  std::string account_id = logout_request->account_id;
  auto logout_origin = url::Origin::Create(logout_request->url);
  logout_requests_.pop();

  if (active_session_permission_delegate_->HasActiveSession(
          logout_origin, origin(), account_id)) {
    network_manager_->SendLogout(
        logout_request->url,
        base::BindOnce(&FederatedAuthRequestImpl::OnLogoutCompleted,
                       weak_ptr_factory_.GetWeakPtr()));
    active_session_permission_delegate_->RevokeActiveSession(
        logout_origin, origin(), account_id);
  } else {
    if (logout_requests_.empty()) {
      CompleteLogoutRequest(LogoutRpsStatus::kSuccess);
      return;
    }

    DispatchOneLogout();
  }
}

void FederatedAuthRequestImpl::OnLogoutCompleted() {
  if (logout_requests_.empty()) {
    CompleteLogoutRequest(LogoutRpsStatus::kSuccess);
    return;
  }

  DispatchOneLogout();
}

void FederatedAuthRequestImpl::CompleteRequestWithError(
    blink::mojom::FederatedAuthRequestResult result,
    absl::optional<TokenStatus> token_status,
    bool should_delay_callback) {
  CompleteRequest(result, token_status, /*token=*/"", should_delay_callback);
}

void FederatedAuthRequestImpl::CompleteRequest(
    blink::mojom::FederatedAuthRequestResult result,
    absl::optional<TokenStatus> token_status,
    const std::string& id_token,
    bool should_delay_callback) {
  DCHECK(result == FederatedAuthRequestResult::kSuccess || id_token.empty());

  if (!auth_request_callback_)
    return;

  if (token_status)
    fedcm_metrics_->RecordRequestTokenStatus(*token_status);

  if (!errors_logged_to_console_ &&
      result != FederatedAuthRequestResult::kSuccess) {
    errors_logged_to_console_ = true;

    // It would be possible to add this inspector issue on the renderer, which
    // will receive the callback. However, it is preferable to do so on the
    // browser because this is closer to the source, which means adding
    // additional metadata is easier. In addition, in the future we may only
    // need to pass a small amount of information to the renderer in the case of
    // an error, so it would be cleaner to do this by reporting the inspector
    // issue from the browser.
    AddInspectorIssue(result);
    AddConsoleErrorMessage(result);

    if (endpoints_.metrics.is_valid()) {
      network_manager_->SendFailedTokenRequestMetrics(
          endpoints_.metrics,
          FederatedAuthRequestResultToMetricsEndpointErrorCode(result));
    }
  }

  CleanUp();

  if (!should_delay_callback || ShouldCompleteRequestImmediately()) {
    errors_logged_to_console_ = false;

    RequestTokenStatus status =
        FederatedAuthRequestResultToRequestTokenStatus(result);
    std::move(auth_request_callback_).Run(status, id_token);
  } else {
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FederatedAuthRequestImpl::OnRejectRequest,
                       weak_ptr_factory_.GetWeakPtr()),
        GetRandomRejectionTime());
  }
}

void FederatedAuthRequestImpl::CleanUp() {
  request_dialog_controller_.reset();
  network_manager_.reset();
  // Given that |request_dialog_controller_| has reference to this web content
  // instance we destroy that first.
  account_id_ = std::string();
  start_time_ = base::TimeTicks();
  show_accounts_dialog_time_ = base::TimeTicks();
  select_account_time_ = base::TimeTicks();
  token_response_time_ = base::TimeTicks();
  idp_info_.clear();
  pending_idps_.clear();
  idp_order_.clear();
  idp_data_.clear();
}

void FederatedAuthRequestImpl::AddInspectorIssue(
    FederatedAuthRequestResult result) {
  DCHECK_NE(result, FederatedAuthRequestResult::kSuccess);
  auto details = blink::mojom::InspectorIssueDetails::New();
  auto federated_auth_request_details =
      blink::mojom::FederatedAuthRequestIssueDetails::New(result);
  details->federated_auth_request_details =
      std::move(federated_auth_request_details);
  render_frame_host().ReportInspectorIssue(
      blink::mojom::InspectorIssueInfo::New(
          blink::mojom::InspectorIssueCode::kFederatedAuthRequestIssue,
          std::move(details)));
}

void FederatedAuthRequestImpl::AddConsoleErrorMessage(
    FederatedAuthRequestResult result) {
  std::string message = GetConsoleErrorMessage(result);
  render_frame_host().AddMessageToConsole(
      blink::mojom::ConsoleMessageLevel::kError, message);
}

bool FederatedAuthRequestImpl::ShouldCompleteRequestImmediately() {
  return api_permission_delegate_->ShouldCompleteRequestImmediately();
}

url::Origin FederatedAuthRequestImpl::GetEmbeddingOrigin() const {
  RenderFrameHost* main_frame = render_frame_host().GetMainFrame();
  DCHECK(main_frame->IsInPrimaryMainFrame());
  return main_frame->GetLastCommittedOrigin();
}

void FederatedAuthRequestImpl::CompleteLogoutRequest(
    blink::mojom::LogoutRpsStatus status) {
  network_manager_.reset();
  base::queue<blink::mojom::LogoutRpsRequestPtr>().swap(logout_requests_);
  if (logout_callback_)
    std::move(logout_callback_).Run(status);
}

std::unique_ptr<IdpNetworkRequestManager>
FederatedAuthRequestImpl::CreateNetworkManager() {
  if (mock_network_manager_)
    return std::move(mock_network_manager_);

  return IdpNetworkRequestManager::Create(
      static_cast<RenderFrameHostImpl*>(&render_frame_host()));
}

std::unique_ptr<IdentityRequestDialogController>
FederatedAuthRequestImpl::CreateDialogController() {
  if (mock_dialog_controller_)
    return std::move(mock_dialog_controller_);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeUIForFedCM)) {
    std::string selected_account =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kUseFakeUIForFedCM);
    return std::make_unique<FakeIdentityRequestDialogController>(
        selected_account.empty()
            ? absl::nullopt
            : absl::optional<std::string>(selected_account));
  }

  return GetContentClient()->browser()->CreateIdentityRequestDialogController();
}

void FederatedAuthRequestImpl::SetTokenRequestDelayForTests(
    base::TimeDelta delay) {
  token_request_delay_ = delay;
}

void FederatedAuthRequestImpl::SetNetworkManagerForTests(
    std::unique_ptr<IdpNetworkRequestManager> manager) {
  mock_network_manager_ = std::move(manager);
}

void FederatedAuthRequestImpl::SetDialogControllerForTests(
    std::unique_ptr<IdentityRequestDialogController> controller) {
  mock_dialog_controller_ = std::move(controller);
}

void FederatedAuthRequestImpl::OnRejectRequest() {
  if (auth_request_callback_) {
    DCHECK(!logout_callback_);
    DCHECK(errors_logged_to_console_);
    CompleteRequestWithError(FederatedAuthRequestResult::kError, absl::nullopt,
                             /*should_delay_callback=*/false);
  }
}

}  // namespace content
