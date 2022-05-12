// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/federated_auth_request_impl.h"

#include "base/callback.h"
#include "base/command_line.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_piece.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "content/browser/bad_message.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webid/fake_identity_request_dialog_controller.h"
#include "content/browser/webid/fedcm_metrics.h"
#include "content/browser/webid/flags.h"
#include "content/browser/webid/webid_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/federated_identity_active_session_permission_context_delegate.h"
#include "content/public/browser/federated_identity_api_permission_context_delegate.h"
#include "content/public/browser/federated_identity_sharing_permission_context_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom.h"
#include "ui/accessibility/ax_mode.h"
#include "url/url_constants.h"

using blink::mojom::FederatedAuthRequestResult;
using blink::mojom::LogoutRpsStatus;
using blink::mojom::LogoutStatus;
using blink::mojom::RequestIdTokenStatus;
using blink::mojom::RevokeStatus;
using FederatedApiPermissionStatus =
    content::FederatedIdentityApiPermissionContextDelegate::PermissionStatus;
using IdTokenStatus = content::FedCmRequestIdTokenStatus;
using LoginState = content::IdentityRequestAccount::LoginState;
using RevokeStatusForMetrics = content::FedCmRevokeStatus;
using SignInMode = content::IdentityRequestAccount::SignInMode;

namespace content {

namespace {
static constexpr base::TimeDelta kDefaultIdTokenRequestDelay = base::Seconds(3);
// TODO(yigu): We need to make sure the delay is greater than the time required
// for a successful flow based on `Blink.FedCm.Timing.TurnaroundTime`.
// https://crbug.com/1298316.
static constexpr base::TimeDelta kRequestRejectionDelay = base::Seconds(60);

// Maximum number of provider URLs in the manifest list.
// TODO(cbiesinger): Determine what the right number is.
static constexpr size_t kMaxProvidersInManifestList = 1ul;

std::string FormatRequestParamsWithoutScope(const std::string& client_id,
                                            const std::string& nonce,
                                            const std::string& account_id,
                                            bool is_sign_in) {
  std::string query;
  if (!client_id.empty())
    query += "client_id=" + client_id;

  if (!nonce.empty()) {
    if (!query.empty())
      query += "&";
    query += "nonce=" + nonce;
  }

  if (!account_id.empty()) {
    if (!query.empty())
      query += "&";
    query += "account_id=" + account_id;
  }
  // For returning users who are signing in instead of signing up, we do not
  // show the privacy policy and terms of service on the consent sheet. This
  // field indicates in the request that whether the user has granted consent
  // after seeing the sheet with privacy policy and terms of service.
  std::string consent_acquired = is_sign_in ? "false" : "true";
  if (!query.empty())
    query += "&consent_acquired=" + consent_acquired;
  return query;
}

std::string GetConsoleErrorMessage(FederatedAuthRequestResult status) {
  switch (status) {
    case FederatedAuthRequestResult::kApprovalDeclined: {
      return "User declined the sign-in attempt.";
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
    case FederatedAuthRequestResult::
        kErrorClientMetadataMissingPrivacyPolicyUrl: {
      return "Provider's client metadata is missing or has an invalid privacy "
             "policy url.";
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
      return "The provider's id token fetch resulted in an error response "
             "code.";
    }
    case FederatedAuthRequestResult::kErrorFetchingIdTokenInvalidResponse: {
      return "Provider's id token is invalid.";
    }
    case FederatedAuthRequestResult::kErrorFetchingIdTokenInvalidRequest: {
      return "The id token fetching request is invalid.";
    }
    case FederatedAuthRequestResult::kErrorCanceled: {
      return "The request has been aborted.";
    }
    case FederatedAuthRequestResult::kError: {
      return "Error retrieving an id token.";
    }
    case FederatedAuthRequestResult::kSuccess: {
      DCHECK(false);
      return "";
    }
  }
}

RequestIdTokenStatus FederatedAuthRequestResultToRequestIdTokenStatus(
    FederatedAuthRequestResult result) {
  // Avoids exposing to renderer detailed error messages which may leak cross
  // site information to the API call site.
  switch (result) {
    case FederatedAuthRequestResult::kSuccess: {
      return RequestIdTokenStatus::kSuccess;
    }
    case FederatedAuthRequestResult::kApprovalDeclined: {
      return RequestIdTokenStatus::kApprovalDeclined;
    }
    case FederatedAuthRequestResult::kErrorTooManyRequests: {
      return RequestIdTokenStatus::kErrorTooManyRequests;
    }
    case FederatedAuthRequestResult::kErrorCanceled: {
      return RequestIdTokenStatus::kErrorCanceled;
    }
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
        kErrorClientMetadataMissingPrivacyPolicyUrl:
    case FederatedAuthRequestResult::
        kErrorFetchingClientMetadataInvalidResponse:
    case FederatedAuthRequestResult::kErrorFetchingAccountsHttpNotFound:
    case FederatedAuthRequestResult::kErrorFetchingAccountsNoResponse:
    case FederatedAuthRequestResult::kErrorFetchingAccountsInvalidResponse:
    case FederatedAuthRequestResult::kErrorFetchingIdTokenHttpNotFound:
    case FederatedAuthRequestResult::kErrorFetchingIdTokenNoResponse:
    case FederatedAuthRequestResult::kErrorFetchingIdTokenInvalidResponse:
    case FederatedAuthRequestResult::kErrorFetchingIdTokenInvalidRequest:
    case FederatedAuthRequestResult::kError: {
      return RequestIdTokenStatus::kError;
    }
  }
}

}  // namespace

FederatedAuthRequestImpl::FederatedAuthRequestImpl(RenderFrameHostImpl* host,
                                                   const url::Origin& origin)
    : render_frame_host_(host),
      origin_(origin),
      delay_timer_(FROM_HERE,
                   kRequestRejectionDelay,
                   this,
                   &FederatedAuthRequestImpl::OnRejectRequest),
      id_token_request_delay_(kDefaultIdTokenRequestDelay) {}

FederatedAuthRequestImpl::~FederatedAuthRequestImpl() {
  // Ensures key data members are destructed in proper order and resolves any
  // pending promise.
  if (auth_request_callback_) {
    DCHECK(!revoke_callback_);
    DCHECK(!logout_callback_);
    RecordRequestIdTokenStatus(IdTokenStatus::kUnhandledRequest,
                               render_frame_host_->GetPageUkmSourceId());
    CompleteRequest(FederatedAuthRequestResult::kError, "",
                    /*should_call_callback=*/true);
  }
  if (revoke_callback_) {
    DCHECK(!auth_request_callback_);
    DCHECK(!logout_callback_);
    RecordRevokeStatus(RevokeStatusForMetrics::kUnhandledRequest,
                       render_frame_host_->GetPageUkmSourceId());
    CompleteRevokeRequest(RevokeStatus::kError,
                          /*should_call_callback=*/true);
  }
}

void FederatedAuthRequestImpl::RequestIdToken(
    const GURL& provider,
    const std::string& client_id,
    const std::string& nonce,
    bool prefer_auto_sign_in,
    blink::mojom::FederatedAuthRequest::RequestIdTokenCallback callback) {
  if (HasPendingRequest()) {
    RecordRequestIdTokenStatus(IdTokenStatus::kTooManyRequests,
                               render_frame_host_->GetPageUkmSourceId());
    std::move(callback).Run(RequestIdTokenStatus::kErrorTooManyRequests, "");
    return;
  }

  auth_request_callback_ = std::move(callback);
  provider_ = provider;
  client_id_ = client_id;
  nonce_ = nonce;
  prefer_auto_sign_in_ = prefer_auto_sign_in && IsFedCmAutoSigninEnabled();
  start_time_ = base::TimeTicks::Now();
  delay_timer_.Reset();

  if (!GetApiPermissionContext()) {
    CompleteRequest(FederatedAuthRequestResult::kError, "",
                    /*should_call_callback=*/true);
    return;
  }

  network_manager_ = CreateNetworkManager(provider);
  if (!network_manager_) {
    RecordRequestIdTokenStatus(IdTokenStatus::kNoNetworkManager,
                               render_frame_host_->GetPageUkmSourceId());
    // TODO(yigu): this is due to provider url being non-secure. We should
    // reject early in the renderer process.
    CompleteRequest(FederatedAuthRequestResult::kError, "",
                    /*should_call_callback=*/true);
    return;
  }

  FederatedApiPermissionStatus permission_status =
      GetApiPermissionContext()->GetApiPermissionStatus(origin_);

  absl::optional<IdTokenStatus> error_id_token_status;
  FederatedAuthRequestResult request_result =
      FederatedAuthRequestResult::kError;

  switch (permission_status) {
    case FederatedApiPermissionStatus::BLOCKED_VARIATIONS:
      error_id_token_status = IdTokenStatus::kDisabledInFlags;
      break;
    case FederatedApiPermissionStatus::BLOCKED_THIRD_PARTY_COOKIES_BLOCKED:
      error_id_token_status = IdTokenStatus::kThirdPartyCookiesBlocked;
      break;
    case FederatedApiPermissionStatus::BLOCKED_SETTINGS:
      error_id_token_status = IdTokenStatus::kDisabledInSettings;
      request_result = FederatedAuthRequestResult::kErrorDisabledInSettings;
      break;
    case FederatedApiPermissionStatus::BLOCKED_EMBARGO:
      error_id_token_status = IdTokenStatus::kDisabledEmbargo;
      request_result = FederatedAuthRequestResult::kErrorDisabledInSettings;
      break;
    case FederatedApiPermissionStatus::GRANTED:
      // Intentional fall-through.
      break;
    default:
      NOTREACHED();
      break;
  }

  if (error_id_token_status) {
    RecordRequestIdTokenStatus(*error_id_token_status,
                               render_frame_host_->GetPageUkmSourceId());
    CompleteRequest(request_result, "", /*should_call_callback=*/false);
    return;
  }

  request_dialog_controller_ = CreateDialogController();

  FetchManifest(kForToken);
}

void FederatedAuthRequestImpl::CancelTokenRequest() {
  if (!auth_request_callback_)
    return;

  // Dialog will be hidden by the destructor for request_dialog_controller_,
  // triggered by CompleteRequest.
  RecordRequestIdTokenStatus(IdTokenStatus::kAborted,
                             render_frame_host_->GetPageUkmSourceId());
  CompleteRequest(FederatedAuthRequestResult::kErrorCanceled, "",
                  /*should_call_callback=*/true);
}

void FederatedAuthRequestImpl::Revoke(
    const GURL& provider,
    const std::string& client_id,
    const std::string& hint,
    blink::mojom::FederatedAuthRequest::RevokeCallback callback) {
  if (HasPendingRequest()) {
    RecordRevokeStatus(RevokeStatusForMetrics::kTooManyRequests,
                       render_frame_host_->GetPageUkmSourceId());
    std::move(callback).Run(RevokeStatus::kError);
    return;
  }

  provider_ = provider;
  client_id_ = client_id;
  hint_ = hint;
  delay_timer_.Reset();
  revoke_callback_ = std::move(callback);

  if (!GetApiPermissionContext()) {
    CompleteRevokeRequest(RevokeStatus::kError,
                          /*should_call_callback=*/false);
    return;
  }

  network_manager_ = CreateNetworkManager(provider);
  if (!network_manager_) {
    RecordRevokeStatus(RevokeStatusForMetrics::kNoNetworkManager,
                       render_frame_host_->GetPageUkmSourceId());
    CompleteRevokeRequest(RevokeStatus::kError,
                          /*should_call_callback=*/false);
    return;
  }

  FederatedApiPermissionStatus permission_status =
      GetApiPermissionContext()->GetApiPermissionStatus(origin_);

  absl::optional<RevokeStatusForMetrics> error_revoke_status;
  switch (permission_status) {
    case FederatedApiPermissionStatus::BLOCKED_VARIATIONS:
      error_revoke_status = RevokeStatusForMetrics::kDisabledInFlags;
      break;
    case FederatedApiPermissionStatus::BLOCKED_THIRD_PARTY_COOKIES_BLOCKED:
      error_revoke_status = RevokeStatusForMetrics::kThirdPartyCookiesBlocked;
      break;
    case FederatedApiPermissionStatus::BLOCKED_SETTINGS:
    case FederatedApiPermissionStatus::BLOCKED_EMBARGO:
      error_revoke_status = RevokeStatusForMetrics::kDisabledInSettings;
      break;
    case FederatedApiPermissionStatus::GRANTED:
      // Intentional fall-through.
      break;
    default:
      NOTREACHED();
      break;
  }

  if (error_revoke_status) {
    RecordRevokeStatus(*error_revoke_status,
                       render_frame_host_->GetPageUkmSourceId());
    CompleteRevokeRequest(RevokeStatus::kError, /*should_call_callback=*/false);
    return;
  }

  if (!GetSharingPermissionContext() ||
      !GetSharingPermissionContext()->HasSharingPermissionForAnyAccount(
          origin_, url::Origin::Create(provider_))) {
    RecordRevokeStatus(RevokeStatusForMetrics::kNoAccountToRevoke,
                       render_frame_host_->GetPageUkmSourceId());
    CompleteRevokeRequest(RevokeStatus::kError,
                          /*should_call_callback=*/false);
    return;
  }

  FetchManifest(kForRevoke);
}

void FederatedAuthRequestImpl::Logout(
    const GURL& provider,
    const std::string& account_id,
    blink::mojom::FederatedAuthRequest::LogoutCallback callback) {
  url::Origin idp_origin(url::Origin::Create(provider));
  auto* context = GetActiveSessionPermissionContext();
  if (!context || !context->HasActiveSession(origin_, idp_origin, account_id)) {
    std::move(callback).Run(LogoutStatus::kNotLoggedIn);
    return;
  }

  if (!GetApiPermissionContext() ||
      GetApiPermissionContext()->GetApiPermissionStatus(origin_) !=
          FederatedApiPermissionStatus::GRANTED) {
    std::move(callback).Run(LogoutStatus::kNotLoggedIn);
    return;
  }

  context->RevokeActiveSession(origin_, idp_origin, account_id);
  std::move(callback).Run(LogoutStatus::kSuccess);
}

// TODO(kenrb): Depending on how this code evolves, it might make sense to
// spin session management code into its own service. The prohibition on
// making authentication requests and logout requests at the same time, while
// not problematic for any plausible use case, need not be strictly necessary
// if there is a good way to not have to resource contention between requests.
// https://crbug.com/1200581
void FederatedAuthRequestImpl::LogoutRps(
    std::vector<blink::mojom::LogoutRpsRequestPtr> logout_requests,
    blink::mojom::FederatedAuthRequest::LogoutRpsCallback callback) {
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
    bad_message::ReceivedBadMessage(render_frame_host_->GetProcess(),
                                    bad_message::FARI_LOGOUT_BAD_ENDPOINT);
    CompleteLogoutRequest(LogoutRpsStatus::kError);
    return;
  }

  for (auto& request : logout_requests) {
    logout_requests_.push(std::move(request));
  }

  network_manager_ = CreateNetworkManager(origin_.GetURL());
  if (!network_manager_ || !GetApiPermissionContext()) {
    CompleteLogoutRequest(LogoutRpsStatus::kError);
    return;
  }

  if (!IsFedCmIdpSignoutEnabled()) {
    CompleteLogoutRequest(LogoutRpsStatus::kError);
    return;
  }

  if (GetApiPermissionContext()->GetApiPermissionStatus(origin_) !=
      FederatedApiPermissionStatus::GRANTED) {
    CompleteLogoutRequest(LogoutRpsStatus::kError);
    return;
  }

  // TODO(kenrb): These should be parallelized rather than being dispatched
  // serially. https://crbug.com/1200581.
  DispatchOneLogout();
}

bool FederatedAuthRequestImpl::HasPendingRequest() const {
  return auth_request_callback_ || logout_callback_ || revoke_callback_;
}

GURL FederatedAuthRequestImpl::ResolveManifestUrl(const std::string& endpoint) {
  if (endpoint.empty())
    return GURL();
  GURL manifest_url =
      provider_.Resolve(IdpNetworkRequestManager::kManifestFilePath);
  return manifest_url.Resolve(endpoint);
}

bool FederatedAuthRequestImpl::IsEndpointUrlValid(const GURL& endpoint_url) {
  return url::Origin::Create(provider_).IsSameOriginWith(endpoint_url);
}

void FederatedAuthRequestImpl::FetchManifest(FetchManifestType type) {
  absl::optional<int> icon_ideal_size = absl::nullopt;
  absl::optional<int> icon_minimum_size = absl::nullopt;
  if (request_dialog_controller_) {
    icon_ideal_size = request_dialog_controller_->GetBrandIconIdealSize();
    icon_minimum_size = request_dialog_controller_->GetBrandIconMinimumSize();
  }

  IdpNetworkRequestManager::FetchManifestCallback manifest_callback;
  IdpNetworkRequestManager::FetchManifestListCallback manifest_list_callback;
  switch (type) {
    case kForToken: {
      manifest_callback =
          base::BindOnce(&FederatedAuthRequestImpl::OnManifestFetched,
                         weak_ptr_factory_.GetWeakPtr());
      manifest_list_callback =
          base::BindOnce(&FederatedAuthRequestImpl::OnManifestListFetched,
                         weak_ptr_factory_.GetWeakPtr());
      break;
    }
    case kForRevoke: {
      manifest_callback =
          base::BindOnce(&FederatedAuthRequestImpl::OnManifestFetchedForRevoke,
                         weak_ptr_factory_.GetWeakPtr());
      manifest_list_callback = base::BindOnce(
          &FederatedAuthRequestImpl::OnManifestListFetchedForRevoke,
          weak_ptr_factory_.GetWeakPtr());
      break;
    }
  }
  if (IsFedCmManifestValidationEnabled()) {
    network_manager_->FetchManifestList(std::move(manifest_list_callback));
  } else {
    manifest_list_checked_ = true;
  }
  // network_manager_ can be null here during tests when FetchManifestList
  // synchronously calls the callback with an error, in which case CleanUp()
  // will set the network_manager_ to null. If that happens we can safely
  // skip calling FetchManifest.
  if (network_manager_) {
    network_manager_->FetchManifest(icon_ideal_size, icon_minimum_size,
                                    std::move(manifest_callback));
  }
}

void FederatedAuthRequestImpl::OnManifestListFetched(
    IdpNetworkRequestManager::FetchStatus status,
    const std::set<GURL>& urls) {
  switch (status) {
    case IdpNetworkRequestManager::FetchStatus::kHttpNotFoundError: {
      RecordRequestIdTokenStatus(IdTokenStatus::kManifestListHttpNotFound,
                                 render_frame_host_->GetPageUkmSourceId());
      CompleteRequest(
          FederatedAuthRequestResult::kErrorFetchingManifestListHttpNotFound,
          "",
          /*should_call_callback=*/false);
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kNoResponseError: {
      RecordRequestIdTokenStatus(IdTokenStatus::kManifestListNoResponse,
                                 render_frame_host_->GetPageUkmSourceId());
      CompleteRequest(
          FederatedAuthRequestResult::kErrorFetchingManifestListNoResponse, "",
          /*should_call_callback=*/false);
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kInvalidResponseError: {
      RecordRequestIdTokenStatus(IdTokenStatus::kManifestListInvalidResponse,
                                 render_frame_host_->GetPageUkmSourceId());
      CompleteRequest(
          FederatedAuthRequestResult::kErrorFetchingManifestListInvalidResponse,
          "",
          /*should_call_callback=*/false);
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kInvalidRequestError: {
      NOTREACHED();
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kSuccess: {
      // Intentional fall-through.
    }
  }

  if (urls.size() > kMaxProvidersInManifestList) {
    RecordRequestIdTokenStatus(IdTokenStatus::kManifestListTooBig,
                               render_frame_host_->GetPageUkmSourceId());
    CompleteRequest(FederatedAuthRequestResult::kErrorManifestListTooBig, "",
                    /*should_call_callback=*/false);
    return;
  }

  // The provider url from the API call:
  // navigator.credentials.get({
  //   federated: {
  //     providers: [{
  //       url: "https://foo.idp.example",
  //       clientId: "1234"
  //     }],
  //   }
  // });
  // must match the one in the manifest list:
  // {
  //   "provider_urls": [
  //     "https://foo.idp.example"
  //   ]
  // }
  // However, it's possible for developers to append a trailing slash in one of
  // them but not in the other one especially when there's path involved.
  // Besides, for GURL without path, |provider_.spec()| will append a trailing
  // slash automatically. Therefore we relax the requirement by allowing
  // mismatch on trailing slash.
  GURL provider_url = IdpNetworkRequestManager::FixupProviderUrl(provider_);
  DCHECK_EQ(provider_url.path().back(), '/');

  bool provider_url_is_valid = (urls.count(provider_url) != 0);

  if (!provider_url_is_valid) {
    RecordRequestIdTokenStatus(IdTokenStatus::kManifestNotInManifestList,
                               render_frame_host_->GetPageUkmSourceId());
    CompleteRequest(FederatedAuthRequestResult::kErrorManifestNotInManifestList,
                    "",
                    /*should_call_callback=*/false);
    return;
  }

  manifest_list_checked_ = true;
  if (idp_metadata_)
    OnManifestReady(*idp_metadata_);
}

void FederatedAuthRequestImpl::OnManifestListFetchedForRevoke(
    IdpNetworkRequestManager::FetchStatus status,
    const std::set<GURL>& urls) {
  switch (status) {
    case IdpNetworkRequestManager::FetchStatus::kHttpNotFoundError: {
      RecordRevokeStatus(RevokeStatusForMetrics::kManifestListHttpNotFound,
                         render_frame_host_->GetPageUkmSourceId());
      CompleteRevokeRequest(RevokeStatus::kError,
                            /*should_call_callback=*/false);
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kNoResponseError: {
      RecordRevokeStatus(RevokeStatusForMetrics::kManifestListNoResponse,
                         render_frame_host_->GetPageUkmSourceId());
      CompleteRevokeRequest(RevokeStatus::kError,
                            /*should_call_callback=*/false);
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kInvalidResponseError: {
      RecordRevokeStatus(RevokeStatusForMetrics::kManifestListInvalidResponse,
                         render_frame_host_->GetPageUkmSourceId());
      CompleteRevokeRequest(RevokeStatus::kError,
                            /*should_call_callback=*/false);
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kInvalidRequestError: {
      NOTREACHED();
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kSuccess: {
      // Intentional fall-through.
    }
  }

  if (urls.size() > kMaxProvidersInManifestList) {
    RecordRevokeStatus(RevokeStatusForMetrics::kManifestListTooBig,
                       render_frame_host_->GetPageUkmSourceId());
    CompleteRevokeRequest(RevokeStatus::kError,
                          /*should_call_callback=*/false);
    return;
  }

  GURL provider_url = IdpNetworkRequestManager::FixupProviderUrl(provider_);
  if (urls.count(provider_url) == 0) {
    RecordRevokeStatus(RevokeStatusForMetrics::kManifestNotInManifestList,
                       render_frame_host_->GetPageUkmSourceId());
    CompleteRevokeRequest(RevokeStatus::kError,
                          /*should_call_callback=*/false);
    return;
  }

  manifest_list_checked_ = true;
  if (idp_metadata_)
    OnManifestReadyForRevoke(*idp_metadata_);
}

void FederatedAuthRequestImpl::OnManifestFetched(
    IdpNetworkRequestManager::FetchStatus status,
    IdpNetworkRequestManager::Endpoints endpoints,
    IdentityProviderMetadata idp_metadata) {
  switch (status) {
    case IdpNetworkRequestManager::FetchStatus::kHttpNotFoundError: {
      RecordRequestIdTokenStatus(IdTokenStatus::kManifestHttpNotFound,
                                 render_frame_host_->GetPageUkmSourceId());
      CompleteRequest(
          FederatedAuthRequestResult::kErrorFetchingManifestHttpNotFound, "",
          /*should_call_callback=*/false);
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kNoResponseError: {
      RecordRequestIdTokenStatus(IdTokenStatus::kManifestNoResponse,
                                 render_frame_host_->GetPageUkmSourceId());
      CompleteRequest(
          FederatedAuthRequestResult::kErrorFetchingManifestNoResponse, "",
          /*should_call_callback=*/false);
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kInvalidResponseError: {
      RecordRequestIdTokenStatus(IdTokenStatus::kManifestInvalidResponse,
                                 render_frame_host_->GetPageUkmSourceId());
      CompleteRequest(
          FederatedAuthRequestResult::kErrorFetchingManifestInvalidResponse, "",
          /*should_call_callback=*/false);
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kInvalidRequestError: {
      NOTREACHED();
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kSuccess: {
      // Intentional fall-through.
    }
  }

  endpoints_.token = ResolveManifestUrl(endpoints.token);
  endpoints_.accounts = ResolveManifestUrl(endpoints.accounts);
  endpoints_.client_metadata = ResolveManifestUrl(endpoints.client_metadata);
  idp_metadata_ = idp_metadata;

  if (manifest_list_checked_)
    OnManifestReady(idp_metadata);
}

void FederatedAuthRequestImpl::OnManifestReady(
    IdentityProviderMetadata idp_metadata) {
  bool is_token_valid = IsEndpointUrlValid(endpoints_.token);
  bool is_accounts_valid = IsEndpointUrlValid(endpoints_.accounts);
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
    render_frame_host_->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError, message);
    RecordRequestIdTokenStatus(IdTokenStatus::kManifestInvalidResponse,
                               render_frame_host_->GetPageUkmSourceId());
    CompleteRequest(
        FederatedAuthRequestResult::kErrorFetchingManifestInvalidResponse, "",
        /*should_call_callback=*/false);
    return;
  }
  GURL brand_icon_url = idp_metadata.brand_icon_url;
  DownloadBitmap(
      brand_icon_url, request_dialog_controller_->GetBrandIconIdealSize(),
      base::BindOnce(&FederatedAuthRequestImpl::OnBrandIconDownloaded,
                     weak_ptr_factory_.GetWeakPtr(),
                     request_dialog_controller_->GetBrandIconMinimumSize(),
                     std::move(idp_metadata)));
}

void FederatedAuthRequestImpl::OnManifestFetchedForRevoke(
    IdpNetworkRequestManager::FetchStatus status,
    IdpNetworkRequestManager::Endpoints endpoints,
    IdentityProviderMetadata idp_metadata) {
  switch (status) {
    case IdpNetworkRequestManager::FetchStatus::kHttpNotFoundError: {
      RecordRevokeStatus(RevokeStatusForMetrics::kManifestHttpNotFound,
                         render_frame_host_->GetPageUkmSourceId());
      CompleteRevokeRequest(RevokeStatus::kError,
                            /*should_call_callback=*/false);
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kNoResponseError: {
      RecordRevokeStatus(RevokeStatusForMetrics::kManifestNoResponse,
                         render_frame_host_->GetPageUkmSourceId());
      CompleteRevokeRequest(RevokeStatus::kError,
                            /*should_call_callback=*/false);
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kInvalidResponseError: {
      RecordRevokeStatus(RevokeStatusForMetrics::kManifestInvalidResponse,
                         render_frame_host_->GetPageUkmSourceId());
      CompleteRevokeRequest(RevokeStatus::kError,
                            /*should_call_callback=*/false);
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kInvalidRequestError: {
      NOTREACHED();
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kSuccess: {
      // Intentional fall-through.
    }
  }

  endpoints_.revoke = ResolveManifestUrl(endpoints.revocation);
  if (!IsEndpointUrlValid(endpoints_.revoke)) {
    render_frame_host_->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError,
        "Manifest is missing or has an invalid URL for the following required "
        "endpoint: \"revocation_endpoint\"");
    RecordRevokeStatus(RevokeStatusForMetrics::kRevokeUrlIsCrossOrigin,
                       render_frame_host_->GetPageUkmSourceId());
    CompleteRevokeRequest(RevokeStatus::kError,
                          /*should_call_callback=*/false);
    return;
  }

  idp_metadata_ = idp_metadata;
  if (manifest_list_checked_)
    OnManifestReadyForRevoke(idp_metadata);
}

void FederatedAuthRequestImpl::OnManifestReadyForRevoke(
    IdentityProviderMetadata idp_metadata) {
  network_manager_->SendRevokeRequest(
      endpoints_.revoke, client_id_, hint_,
      base::BindOnce(&FederatedAuthRequestImpl::OnRevokeResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FederatedAuthRequestImpl::OnRevokeResponse(
    IdpNetworkRequestManager::RevokeResponse response) {
  RevokeStatus status =
      response == IdpNetworkRequestManager::RevokeResponse::kSuccess
          ? RevokeStatus::kSuccess
          : RevokeStatus::kError;
  if (status == RevokeStatus::kSuccess) {
    url::Origin idp_origin{url::Origin::Create(provider_)};
    // Since the account is now deleted, revoke the permission.
    if (GetSharingPermissionContext()) {
      GetSharingPermissionContext()->RevokeSharingPermission(origin_,
                                                             idp_origin, hint_);
    }
    if (GetActiveSessionPermissionContext()) {
      GetActiveSessionPermissionContext()->RevokeActiveSession(
          origin_, idp_origin, hint_);
    }
    RecordRevokeStatus(RevokeStatusForMetrics::kSuccess,
                       render_frame_host_->GetPageUkmSourceId());
    CompleteRevokeRequest(status, /*should_call_callback=*/true);
  } else {
    RecordRevokeStatus(RevokeStatusForMetrics::kRevocationFailedOnServer,
                       render_frame_host_->GetPageUkmSourceId());
    CompleteRevokeRequest(status, /*should_call_callback=*/false);
  }
}

void FederatedAuthRequestImpl::CompleteRevokeRequest(
    RevokeStatus status,
    bool should_call_callback) {
  if (!revoke_callback_)
    return;

  network_manager_.reset();
  provider_ = GURL();
  hint_ = std::string();
  client_id_ = std::string();
  manifest_list_checked_ = false;
  idp_metadata_.reset();

  if (should_call_callback)
    std::move(revoke_callback_).Run(status);
}

void FederatedAuthRequestImpl::OnBrandIconDownloaded(
    int icon_minimum_size,
    IdentityProviderMetadata idp_metadata,
    int id,
    int http_status_code,
    const GURL& image_url,
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<gfx::Size>& sizes) {
  // For the sake of FedCM spec simplicity do not support multi-resolution .ico
  // files.
  if (bitmaps.size() == 1 && bitmaps[0].width() == bitmaps[0].height() &&
      bitmaps[0].width() >= icon_minimum_size) {
    idp_metadata.brand_icon = bitmaps[0];
  }

  if (IsEndpointUrlValid(endpoints_.client_metadata)) {
    network_manager_->FetchClientMetadata(
        endpoints_.client_metadata, client_id_,
        base::BindOnce(
            &FederatedAuthRequestImpl::OnClientMetadataResponseReceived,
            weak_ptr_factory_.GetWeakPtr(), std::move(idp_metadata)));
  } else {
    network_manager_->SendAccountsRequest(
        endpoints_.accounts, client_id_,
        base::BindOnce(&FederatedAuthRequestImpl::OnAccountsResponseReceived,
                       weak_ptr_factory_.GetWeakPtr(), idp_metadata));
  }
}

void FederatedAuthRequestImpl::OnClientMetadataResponseReceived(
    IdentityProviderMetadata idp_metadata,
    IdpNetworkRequestManager::FetchStatus status,
    IdpNetworkRequestManager::ClientMetadata data) {
  // TODO(yigu): Clean up the client metadata related errors for metrics and
  // console logs.
  client_metadata_ = data;
  network_manager_->SendAccountsRequest(
      endpoints_.accounts, client_id_,
      base::BindOnce(&FederatedAuthRequestImpl::OnAccountsResponseReceived,
                     weak_ptr_factory_.GetWeakPtr(), idp_metadata));
}

void FederatedAuthRequestImpl::DownloadBitmap(
    const GURL& icon_url,
    int ideal_icon_size,
    WebContents::ImageDownloadCallback callback) {
  if (!icon_url.is_valid()) {
    std::move(callback).Run(0 /* id */, 404 /* http_status_code */, icon_url,
                            std::vector<SkBitmap>(), std::vector<gfx::Size>());
    return;
  }

  WebContents::FromRenderFrameHost(render_frame_host_)
      ->DownloadImage(icon_url, /*is_favicon*/ false,
                      gfx::Size(ideal_icon_size, ideal_icon_size),
                      0 /* max_bitmap_size */, false /* bypass_cache */,
                      std::move(callback));
}

void FederatedAuthRequestImpl::OnAccountsResponseReceived(
    IdentityProviderMetadata idp_metadata,
    IdpNetworkRequestManager::FetchStatus status,
    IdpNetworkRequestManager::AccountList accounts) {
  switch (status) {
    case IdpNetworkRequestManager::FetchStatus::kHttpNotFoundError: {
      RecordRequestIdTokenStatus(IdTokenStatus::kAccountsHttpNotFound,
                                 render_frame_host_->GetPageUkmSourceId());
      CompleteRequest(
          FederatedAuthRequestResult::kErrorFetchingAccountsHttpNotFound, "",
          /*should_call_callback=*/false);
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kNoResponseError: {
      RecordRequestIdTokenStatus(IdTokenStatus::kAccountsNoResponse,
                                 render_frame_host_->GetPageUkmSourceId());
      CompleteRequest(
          FederatedAuthRequestResult::kErrorFetchingAccountsNoResponse, "",
          /*should_call_callback=*/false);
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kInvalidResponseError: {
      RecordRequestIdTokenStatus(IdTokenStatus::kAccountsInvalidResponse,
                                 render_frame_host_->GetPageUkmSourceId());
      CompleteRequest(
          FederatedAuthRequestResult::kErrorFetchingAccountsInvalidResponse, "",
          /*should_call_callback=*/false);
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kSuccess: {
      WebContents* rp_web_contents =
          WebContents::FromRenderFrameHost(render_frame_host_);
      bool is_visible = rp_web_contents && (rp_web_contents->GetVisibility() ==
                                            Visibility::VISIBLE);
      RecordWebContentsVisibilityUponReadyToShowDialog(is_visible);
      // Does not show the dialog if the user has left the page. e.g. they may
      // open a new tab before browser is ready to show the dialog.
      if (!is_visible) {
        CompleteRequest(FederatedAuthRequestResult::kError, "",
                        /*should_call_callback=*/false);
        return;
      }

      // Populate the accounts login state.
      for (auto& account : accounts) {
        // We set the login state based on the IDP response if it sends
        // back an approved_clients list. If it does not, we need to set
        // it here based on browser state.
        if (account.login_state)
          continue;
        LoginState login_state = LoginState::kSignUp;
        // Consider this a sign-in if we have seen a successful sign-up for
        // this account before.
        if (GetSharingPermissionContext() &&
            GetSharingPermissionContext()->HasSharingPermission(
                origin_, url::Origin::Create(provider_), account.id)) {
          login_state = LoginState::kSignIn;
        }
        account.login_state = login_state;
      }

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
      ClientIdData data{GURL(client_metadata_.terms_of_service_url),
                        GURL(client_metadata_.privacy_policy_url)};
      show_accounts_dialog_time_ = base::TimeTicks::Now();
      RecordShowAccountsDialogTime(show_accounts_dialog_time_ - start_time_,
                                   render_frame_host_->GetPageUkmSourceId());

      request_dialog_controller_->ShowAccountsDialog(
          rp_web_contents, provider_, accounts, idp_metadata, data,
          is_auto_sign_in ? SignInMode::kAuto : SignInMode::kExplicit,
          base::BindOnce(&FederatedAuthRequestImpl::OnAccountSelected,
                         weak_ptr_factory_.GetWeakPtr()));
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kInvalidRequestError: {
      NOTREACHED();
    }
  }
}

void FederatedAuthRequestImpl::OnAccountSelected(const std::string& account_id,
                                                 bool is_sign_in,
                                                 bool should_embargo) {
  // This could happen if user didn't select any accounts.
  if (account_id.empty()) {
    base::TimeTicks dismiss_dialog_time = base::TimeTicks::Now();
    RecordCancelOnDialogTime(dismiss_dialog_time - show_accounts_dialog_time_,
                             render_frame_host_->GetPageUkmSourceId());
    RecordRequestIdTokenStatus(IdTokenStatus::kNotSelectAccount,
                               render_frame_host_->GetPageUkmSourceId());

    if (should_embargo && GetApiPermissionContext()) {
      GetApiPermissionContext()->RecordDismissAndEmbargo(origin_);
    }

    CompleteRequest(FederatedAuthRequestResult::kError, "",
                    /*should_call_callback=*/false);
    return;
  }

  RecordIsSignInUser(is_sign_in);

  if (GetApiPermissionContext()) {
    GetApiPermissionContext()->RemoveEmbargoAndResetCounts(origin_);
  }

  account_id_ = account_id;
  select_account_time_ = base::TimeTicks::Now();
  RecordContinueOnDialogTime(select_account_time_ - show_accounts_dialog_time_,
                             render_frame_host_->GetPageUkmSourceId());

  network_manager_->SendTokenRequest(
      endpoints_.token, account_id_,
      FormatRequestParamsWithoutScope(client_id_, nonce_, account_id,
                                      is_sign_in),
      base::BindOnce(&FederatedAuthRequestImpl::OnTokenResponseReceived,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FederatedAuthRequestImpl::OnTokenResponseReceived(
    IdpNetworkRequestManager::FetchStatus status,
    const std::string& id_token) {
  if (!auth_request_callback_)
    return;

  // When fetching id tokens we show a "Verify" sheet to users in case fetching
  // takes a long time due to latency etc.. In case that the fetching process is
  // fast, we still want to show the "Verify" sheet for at least
  // |id_token_request_delay_| seconds for better UX.
  id_token_response_time_ = base::TimeTicks::Now();
  base::TimeDelta fetch_time = id_token_response_time_ - select_account_time_;
  if (fetch_time >= id_token_request_delay_) {
    CompleteIdTokenRequest(status, id_token);
    return;
  }

  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FederatedAuthRequestImpl::CompleteIdTokenRequest,
                     weak_ptr_factory_.GetWeakPtr(), status, id_token),
      id_token_request_delay_ - fetch_time);
}

void FederatedAuthRequestImpl::CompleteIdTokenRequest(
    IdpNetworkRequestManager::FetchStatus status,
    const std::string& id_token) {
  DCHECK(!start_time_.is_null());
  switch (status) {
    case IdpNetworkRequestManager::FetchStatus::kHttpNotFoundError: {
      RecordRequestIdTokenStatus(IdTokenStatus::kIdTokenHttpNotFound,
                                 render_frame_host_->GetPageUkmSourceId());
      CompleteRequest(
          FederatedAuthRequestResult::kErrorFetchingIdTokenHttpNotFound, "",
          /*should_call_callback=*/false);
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kNoResponseError: {
      RecordRequestIdTokenStatus(IdTokenStatus::kIdTokenNoResponse,
                                 render_frame_host_->GetPageUkmSourceId());
      CompleteRequest(
          FederatedAuthRequestResult::kErrorFetchingIdTokenNoResponse, "",
          /*should_call_callback=*/false);
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kInvalidRequestError: {
      RecordRequestIdTokenStatus(IdTokenStatus::kIdTokenInvalidRequest,
                                 render_frame_host_->GetPageUkmSourceId());
      CompleteRequest(
          FederatedAuthRequestResult::kErrorFetchingIdTokenInvalidRequest, "",
          /*should_call_callback=*/false);
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kInvalidResponseError: {
      RecordRequestIdTokenStatus(IdTokenStatus::kIdTokenInvalidResponse,
                                 render_frame_host_->GetPageUkmSourceId());
      CompleteRequest(
          FederatedAuthRequestResult::kErrorFetchingIdTokenInvalidResponse, "",
          /*should_call_callback=*/false);
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kSuccess: {
      if (GetSharingPermissionContext()) {
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
        GetSharingPermissionContext()->GrantSharingPermission(
            origin_, url::Origin::Create(provider_), account_id_);
      }

      if (GetActiveSessionPermissionContext()) {
        GetActiveSessionPermissionContext()->GrantActiveSession(
            origin_, url::Origin::Create(provider_), account_id_);
      }

      RecordIdTokenResponseAndTurnaroundTime(
          id_token_response_time_ - select_account_time_,
          id_token_response_time_ - start_time_,
          render_frame_host_->GetPageUkmSourceId());
      RecordRequestIdTokenStatus(IdTokenStatus::kSuccess,
                                 render_frame_host_->GetPageUkmSourceId());
      CompleteRequest(FederatedAuthRequestResult::kSuccess, id_token,
                      /*should_call_callback=*/true);
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

  if (!GetActiveSessionPermissionContext()) {
    CompleteLogoutRequest(LogoutRpsStatus::kError);
    return;
  }

  if (GetActiveSessionPermissionContext()->HasActiveSession(
          logout_origin, origin_, account_id)) {
    network_manager_->SendLogout(
        logout_request->url,
        base::BindOnce(&FederatedAuthRequestImpl::OnLogoutCompleted,
                       weak_ptr_factory_.GetWeakPtr()));
    GetActiveSessionPermissionContext()->RevokeActiveSession(
        logout_origin, origin_, account_id);
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

void FederatedAuthRequestImpl::CompleteRequest(
    blink::mojom::FederatedAuthRequestResult result,
    const std::string& id_token,
    bool should_call_callback) {
  DCHECK(result == FederatedAuthRequestResult::kSuccess || id_token.empty());

  if (!auth_request_callback_)
    return;

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
  }

  CleanUp();

  if (should_call_callback) {
    errors_logged_to_console_ = false;

    RequestIdTokenStatus status =
        FederatedAuthRequestResultToRequestIdTokenStatus(result);
    std::move(auth_request_callback_).Run(status, id_token);
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
  id_token_response_time_ = base::TimeTicks();
  manifest_list_checked_ = false;
  idp_metadata_.reset();
}

void FederatedAuthRequestImpl::AddInspectorIssue(
    FederatedAuthRequestResult result) {
  DCHECK_NE(result, FederatedAuthRequestResult::kSuccess);
  auto details = blink::mojom::InspectorIssueDetails::New();
  auto federated_auth_request_details =
      blink::mojom::FederatedAuthRequestIssueDetails::New(result);
  details->federated_auth_request_details =
      std::move(federated_auth_request_details);
  render_frame_host_->ReportInspectorIssue(
      blink::mojom::InspectorIssueInfo::New(
          blink::mojom::InspectorIssueCode::kFederatedAuthRequestIssue,
          std::move(details)));
}

void FederatedAuthRequestImpl::AddConsoleErrorMessage(
    FederatedAuthRequestResult result) {
  std::string message = GetConsoleErrorMessage(result);
  render_frame_host_->AddMessageToConsole(
      blink::mojom::ConsoleMessageLevel::kError, message);
}

void FederatedAuthRequestImpl::CompleteLogoutRequest(
    blink::mojom::LogoutRpsStatus status) {
  network_manager_.reset();
  base::queue<blink::mojom::LogoutRpsRequestPtr>().swap(logout_requests_);
  if (logout_callback_)
    std::move(logout_callback_).Run(status);
}

std::unique_ptr<IdpNetworkRequestManager>
FederatedAuthRequestImpl::CreateNetworkManager(const GURL& provider) {
  if (mock_network_manager_)
    return std::move(mock_network_manager_);

  return IdpNetworkRequestManager::Create(provider, render_frame_host_);
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

void FederatedAuthRequestImpl::SetIdTokenRequestDelayForTests(
    base::TimeDelta delay) {
  id_token_request_delay_ = delay;
}

void FederatedAuthRequestImpl::SetNetworkManagerForTests(
    std::unique_ptr<IdpNetworkRequestManager> manager) {
  mock_network_manager_ = std::move(manager);
}

void FederatedAuthRequestImpl::SetDialogControllerForTests(
    std::unique_ptr<IdentityRequestDialogController> controller) {
  mock_dialog_controller_ = std::move(controller);
}

void FederatedAuthRequestImpl::SetActiveSessionPermissionDelegateForTests(
    FederatedIdentityActiveSessionPermissionContextDelegate*
        active_session_permission_delegate) {
  active_session_permission_delegate_ = active_session_permission_delegate;
}

void FederatedAuthRequestImpl::SetSharingPermissionDelegateForTests(
    FederatedIdentitySharingPermissionContextDelegate*
        sharing_permission_delegate) {
  sharing_permission_delegate_ = sharing_permission_delegate;
}

void FederatedAuthRequestImpl::SetApiPermissionDelegateForTests(
    FederatedIdentityApiPermissionContextDelegate* api_permission_delegate) {
  api_permission_delegate_ = api_permission_delegate;
}

FederatedIdentityActiveSessionPermissionContextDelegate*
FederatedAuthRequestImpl::GetActiveSessionPermissionContext() {
  if (!active_session_permission_delegate_) {
    active_session_permission_delegate_ =
        render_frame_host_->GetBrowserContext()
            ->GetFederatedIdentityActiveSessionPermissionContext();
  }
  return active_session_permission_delegate_;
}

FederatedIdentityApiPermissionContextDelegate*
FederatedAuthRequestImpl::GetApiPermissionContext() {
  if (!api_permission_delegate_) {
    api_permission_delegate_ = render_frame_host_->GetBrowserContext()
                                   ->GetFederatedIdentityApiPermissionContext();
  }
  return api_permission_delegate_;
}

FederatedIdentitySharingPermissionContextDelegate*
FederatedAuthRequestImpl::GetSharingPermissionContext() {
  if (!sharing_permission_delegate_) {
    sharing_permission_delegate_ =
        render_frame_host_->GetBrowserContext()
            ->GetFederatedIdentitySharingPermissionContext();
  }
  return sharing_permission_delegate_;
}

void FederatedAuthRequestImpl::OnRejectRequest() {
  if (auth_request_callback_) {
    DCHECK(!revoke_callback_);
    DCHECK(!logout_callback_);
    CompleteRequest(FederatedAuthRequestResult::kError, "",
                    /*should_call_callback=*/true);
  }
  if (revoke_callback_) {
    DCHECK(!auth_request_callback_);
    DCHECK(!logout_callback_);
    CompleteRevokeRequest(RevokeStatus::kError, /*should_call_callback=*/true);
  }
}

}  // namespace content
