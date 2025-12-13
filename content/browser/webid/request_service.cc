// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/browser/webid/request_service.h"

#include <algorithm>
#include <random>
#include <vector>

#include "base/barrier_closure.h"
#include "base/base64url.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "content/browser/bad_message.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/webid/disconnect_request.h"
#include "content/browser/webid/fake_identity_request_dialog_controller.h"
#include "content/browser/webid/flags.h"
#include "content/browser/webid/identity_registry.h"
#include "content/browser/webid/idp_network_request_manager.h"
#include "content/browser/webid/mappers.h"
#include "content/browser/webid/request_page_data.h"
#include "content/browser/webid/url_computations.h"
#include "content/browser/webid/user_info_request.h"
#include "content/browser/webid/webid_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/webid/federated_identity_api_permission_context_delegate.h"
#include "content/public/browser/webid/federated_identity_auto_reauthn_permission_context_delegate.h"
#include "content/public/browser/webid/federated_identity_permission_context_delegate.h"
#include "content/public/browser/webid/identity_request_account.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/page_visibility_state.h"
#include "mojo/public/cpp/base/values_mojom_traits.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/webid/login_status_account.h"
#include "third_party/blink/public/common/webid/login_status_options.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "url/gurl.h"

using base::Value;
using blink::mojom::FederatedAuthRequestResult;
using blink::mojom::IdentityProviderConfig;
using blink::mojom::IdentityProviderRequestOptionsPtr;
using blink::mojom::RegisterIdpStatus;
using blink::mojom::RequestTokenStatus;
using blink::mojom::RequestUserInfoStatus;
using FederatedApiPermissionStatus =
    content::FederatedIdentityApiPermissionContextDelegate::PermissionStatus;
using DisconnectStatusForMetrics = content::webid::DisconnectStatus;
using TokenStatus = content::webid::RequestIdTokenStatus;
using SignInStateMatchStatus = content::webid::SignInStateMatchStatus;
using LoginState = content::IdentityRequestAccount::LoginState;
using SignInMode = content::IdentityRequestAccount::SignInMode;
using ErrorDialogResult = content::webid::ErrorDialogResult;
using CompleteRequestWithErrorCallback =
    base::OnceCallback<void(blink::mojom::FederatedAuthRequestResult,
                            std::optional<content::webid::RequestIdTokenStatus>,
                            bool)>;

namespace content::webid {

using TokenResponseType = IdpNetworkRequestManager::FedCmTokenResponseType;
using ErrorDialogType = IdpNetworkRequestManager::FedCmErrorDialogType;
using ErrorUrlType = IdpNetworkRequestManager::FedCmErrorUrlType;

namespace {
static constexpr base::TimeDelta kTokenRequestDelay = base::Seconds(3);
static constexpr base::TimeDelta kMaxRejectionTime = base::Seconds(60);

// Users spend less time on Android to dismiss the UI. Given the difference, we
// use two set of values. The values are calculated based on UMA data to follow
// lognormal distribution.
#if BUILDFLAG(IS_ANDROID)
static constexpr double kRejectionLogNormalMu = 7.4;
static constexpr double kRejectionLogNormalSigma = 1.24;
#else
static constexpr double kRejectionLogNormalMu = 8.6;
static constexpr double kRejectionLogNormalSigma = 1.4;
#endif  // BUILDFLAG(IS_ANDROID)

// The time from when the accounts dialog is shown to when a user explicitly
// closes it follows normal distribution. To make the random failures
// indistinguishable from user declines, we use lognormal distribution to
// generate the random number.
base::TimeDelta GetRandomRejectionTime() {
  base::RandomBitGenerator generator;
  std::lognormal_distribution<double> distribution(kRejectionLogNormalMu,
                                                   kRejectionLogNormalSigma);

  base::TimeDelta rejection_time =
      base::Seconds(distribution(generator) / 1000);

  return std::min(kMaxRejectionTime, rejection_time);
}

std::string FormatOriginForDisplay(const url::Origin& origin) {
  return FormatUrlForDisplay(origin.GetURL());
}

std::string GetTopFrameOriginForDisplay(const url::Origin& top_frame_origin) {
  return FormatOriginForDisplay(top_frame_origin);
}

bool IsFrameActive(RenderFrameHost* frame) {
  return frame && frame->IsActive();
}

bool IsFrameVisible(RenderFrameHost* frame) {
  return frame && frame->IsActive() &&
         frame->GetVisibilityState() == content::PageVisibilityState::kVisible;
}

bool CanBypassPermissionStatusCheck(
    const blink::mojom::RpMode& rp_mode,
    const MediationRequirement& mediation_requirement) {
  // Embargo or browser settings should not affect active mode. Since
  // conditional flow isn't intrusive which was the main reason we added such
  // controls, we can bypass the check for it as well.
  return rp_mode == RpMode::kActive ||
         (IsAutofillEnabled() &&
          mediation_requirement == MediationRequirement::kConditional);
}

}  // namespace

RequestService::FetchData::FetchData() = default;
RequestService::FetchData::~FetchData() = default;

DOCUMENT_USER_DATA_KEY_IMPL(RequestService);

RequestService::RequestService(RenderFrameHost* rfh)
    : RequestService(
          rfh,
          rfh->GetBrowserContext()->GetFederatedIdentityApiPermissionContext(),
          rfh->GetBrowserContext()
              ->GetFederatedIdentityAutoReauthnPermissionContext(),
          rfh->GetBrowserContext()->GetFederatedIdentityPermissionContext(),
          IdentityRegistry::FromWebContents(
              WebContents::FromRenderFrameHost(rfh))) {}

RequestService::RequestService(
    RenderFrameHost* rfh,
    FederatedIdentityApiPermissionContextDelegate* api_permission_delegate,
    FederatedIdentityAutoReauthnPermissionContextDelegate*
        auto_reauthn_permission_delegate,
    FederatedIdentityPermissionContextDelegate* permission_delegate,
    IdentityRegistry* identity_registry)
    : DocumentUserData(rfh),
      api_permission_delegate_(api_permission_delegate),
      auto_reauthn_permission_delegate_(auto_reauthn_permission_delegate),
      permission_delegate_(permission_delegate),
      identity_registry_(identity_registry),
      perfetto_track_(CreatePerfettoTrackForFedCM(this)) {
  CHECK(api_permission_delegate_);
  CHECK(auto_reauthn_permission_delegate_);
  CHECK(permission_delegate_);
}

RequestService::~RequestService() {
  // Ensures key data members are destructed in proper order and resolves any
  // pending promise.
  if (auth_request_token_callback_) {
    CompleteRequestWithError(FederatedAuthRequestResult::kError,
                             TokenStatus::kUnhandledRequest,
                             /*should_delay_callback=*/false);
  }
  // Calls |UserInfoRequest|'s destructor to complete the user
  // info request. This is needed because otherwise some resources like
  // `fedcm_metrics_` may no longer be usable when the destructor get invoked
  // naturally.
  user_info_requests_.clear();

  // Calls |DisconnectRequest|'s destructor to complete the
  // revocation request. This is needed because otherwise some resources like
  // `fedcm_metrics_` may no longer be usable when the destructor get invoked
  // naturally.
  disconnect_request_.reset();

  // Since RequestService is a subclass of DocumentUserData, it only lives as
  // long as the current document.
  if (num_requests_ > 0) {
    Metrics::RecordNumRequestsPerDocument(
        render_frame_host().GetPageUkmSourceId(), num_requests_);
  }
}

RequestService& RequestService::CreateForTesting(
    RenderFrameHost& host,
    FederatedIdentityApiPermissionContextDelegate* api_permission_context,
    FederatedIdentityAutoReauthnPermissionContextDelegate*
        auto_reauthn_permission_context,
    FederatedIdentityPermissionContextDelegate* permission_context,
    IdentityRegistry* identity_registry,
    mojo::PendingReceiver<blink::mojom::FederatedAuthRequest> receiver) {
  RequestService::CreateForCurrentDocument(
      &host, api_permission_context, auto_reauthn_permission_context,
      permission_context, identity_registry);
  RequestService* service = RequestService::GetForCurrentDocument(&host);
  service->BindReceiver(std::move(receiver));
  return *service;
}

void RequestService::BindReceiver(
    mojo::PendingReceiver<blink::mojom::FederatedAuthRequest>
        pending_receiver) {
  if (receiver_.is_bound()) {
    // This should only happen with a compromised renderer.
    // TODO(crbug.com/40810039): Call ReportBadMessage.
    return;
  }
  receiver_.Bind(std::move(pending_receiver));
}

void RequestService::ReportBadMessage(const char* message) {
  receiver_.ReportBadMessage(message);
}

void RequestService::ResetAndDeleteThisForTesting() {
  // Resetting the receiver_ before we destruct the objects means that
  // callbacks won't be called. This matches DocumentService::ResetAndDeleteThis
  // and is what our tests expect.
  receiver_.reset();
  DeleteForCurrentDocument(&render_frame_host());
}

std::vector<IdentityProviderRequestOptionsPtr>
RequestService::MaybeAddRegisteredProviders(
    std::vector<IdentityProviderRequestOptionsPtr>& providers) {
  std::vector<IdentityProviderRequestOptionsPtr> result;

  std::vector<GURL> registered_config_urls =
      permission_delegate_->GetRegisteredIdPs();

  // TODO(crbug.com/40252825): we insert the registered IdPs to
  // the list of IdPs in a reverse chronological order:
  // first IdPs to be registered goes first. It is not clear
  // yet what's the right order, but this seems like a reasonable
  // starting point.
  std::ranges::reverse(registered_config_urls);

  for (auto& provider : providers) {
    if (!provider->config->from_idp_registration_api) {
      result.emplace_back(provider->Clone());
      continue;
    }

    for (auto& configURL : registered_config_urls) {
      IdentityProviderRequestOptionsPtr idp = provider->Clone();
      // Keep `from_idp_registration_api` so it is clear this is a registered
      // provider.
      idp->config->config_url = configURL;
      result.emplace_back(std::move(idp));
    }
  }

  // TODO(crbug.com/40252825): Consider removing duplicate
  // IdPs in case they were present in the registry as well
  // as added individually.

  return result;
}

void RequestService::RequestToken(
    std::vector<IdentityProviderGetParametersPtr> idp_get_params_ptrs,
    MediationRequirement requirement,
    RequestTokenCallback callback) {
  // This call is coming from Mojo, so we have no navigation handle.
  RequestToken(std::move(idp_get_params_ptrs), requirement,
               /*navigation_handle=*/nullptr, std::move(callback));
}

void RequestService::RequestToken(
    std::vector<IdentityProviderGetParametersPtr> idp_get_params_ptrs,
    MediationRequirement requirement,
    NavigationHandle* navigation_handle,
    RequestTokenCallback callback) {
  if (ShouldTerminateRequest(idp_get_params_ptrs, requirement)) {
    return;
  }
  bool intercept = false;
  bool should_complete_request_immediately = false;
  devtools_instrumentation::WillSendFedCmRequest(
      render_frame_host(), &intercept, &should_complete_request_immediately);
  should_complete_request_immediately =
      (intercept && should_complete_request_immediately) ||
      api_permission_delegate_->ShouldCompleteRequestImmediately();

  // Expand the providers list with registered providers.
  if (IsIdPRegistrationEnabled()) {
    for (auto& idp_get_params_ptr : idp_get_params_ptrs) {
      std::vector<IdentityProviderRequestOptionsPtr> providers =
          MaybeAddRegisteredProviders(idp_get_params_ptr->providers);
      if (providers.empty()) {
        render_frame_host().AddMessageToConsole(
            blink::mojom::ConsoleMessageLevel::kError,
            "No identity providers are registered.");
        base::TimeDelta delay;
        if (!should_complete_request_immediately) {
          delay = GetRandomRejectionTime();
        }
        base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(std::move(callback), RequestTokenStatus::kError,
                           std::nullopt, std::nullopt,
                           /*error=*/nullptr,
                           /*is_auto_selected=*/false),
            delay);
        return;
      }
      idp_get_params_ptr->providers = std::move(providers);
    }
  }

  if (!render_frame_host().GetPage().IsPrimary()) {
    // This should not be possible but seems to be happening, so we log
    // the lifecycle state for further investigation.
    RenderFrameHostImpl* host_impl =
        static_cast<RenderFrameHostImpl*>(&render_frame_host());

    RecordLifecycleStateFailureReason(
        LifecycleStateImplLifecycleStateImplToFedCmLifecycleStateFailureReason(
            host_impl->lifecycle_state()));
    std::move(callback).Run(RequestTokenStatus::kError, std::nullopt,
                            std::nullopt,
                            /*error=*/nullptr,
                            /*is_auto_selected=*/false);
    return;
  }

  had_transient_user_activation_ =
      (navigation_handle &&
       DidNavigationHandleHaveActivation(navigation_handle)) ||
      render_frame_host().HasTransientUserActivation();

  // Store the previous `idp_order_` value from this class. Note that this is {}
  // unless there is a pending request from the same RFH. In particular, this is
  // still {} if there is a pending request but from a different RFH.
  std::vector<GURL> old_idp_order = std::move(idp_order_);
  idp_order_ = {};
  for (auto& idp_get_params_ptr : idp_get_params_ptrs) {
    for (auto& idp_ptr : idp_get_params_ptr->providers) {
      idp_order_.push_back(idp_ptr->config->config_url);
    }
  }

  if (HasPendingRequest() &&
      HandlePendingRequestAndCancelNewRequest(
          old_idp_order, idp_get_params_ptrs, requirement)) {
    std::move(callback).Run(RequestTokenStatus::kErrorTooManyRequests,
                            std::nullopt, std::nullopt, /*error=*/nullptr,
                            /*is_auto_selected=*/false);
    return;
  }

  // From here on out, all failures go through CompleteRequest, so this is
  // where we start the trace event.
  TRACE_EVENT_BEGIN("content.fedcm", "FedCM get", perfetto_track_);

  should_complete_request_immediately_ = should_complete_request_immediately;
  mediation_requirement_ = requirement;
  auth_request_token_callback_ = std::move(callback);
  GetPageData(render_frame_host().GetPage())
      ->SetPendingWebIdentityRequest(this);
  network_manager_ = CreateNetworkManager();
  request_dialog_controller_ = CreateDialogController();
  start_time_ = base::TimeTicks::Now();
  if (!fedcm_metrics_) {
    fedcm_metrics_ = CreateFedCmMetrics();
  }
  std::set<GURL> idps_with_nonce;
  std::set<GURL> idps_with_nonce_outside_params_only;
  for (const auto& idp_get_params_ptr : idp_get_params_ptrs) {
    for (const auto& idp_ptr : idp_get_params_ptr->providers) {
      if (!idp_ptr->nonce.empty()) {
        idps_with_nonce.insert(idp_ptr->config->config_url);

        bool has_nonce_in_params = false;
        if (idp_ptr->params_json) {
          std::optional<base::Value> params = base::JSONReader::Read(
              *idp_ptr->params_json, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
          if (params && params->is_dict()) {
            if (params->GetDict().contains("nonce")) {
              has_nonce_in_params = true;
            }
          }
        }
        if (!has_nonce_in_params) {
          idps_with_nonce_outside_params_only.insert(
              idp_ptr->config->config_url);
        }
      }
    }
  }
  fedcm_metrics_->RecordHasNonce(idps_with_nonce);
  fedcm_metrics_->RecordHasNonceOutsideParamsOnly(
      idps_with_nonce_outside_params_only);

  // TODO(crbug.com/40218857): handle active mode with multiple IdP.
  if (idp_get_params_ptrs[0]->mode == blink::mojom::RpMode::kActive) {
    rp_mode_ = RpMode::kActive;
    std::optional<base::TimeTicks> user_info_accounts_response_time =
        GetPageData(render_frame_host().GetPage())
            ->ConsumeUserInfoAccountsResponseTime(
                idp_get_params_ptrs[0]->providers[0]->config->config_url);
    if (user_info_accounts_response_time) {
      fedcm_metrics_->RecordTimeBetweenUserInfoAndActiveModeAPI(
          start_time_ - user_info_accounts_response_time.value());
    }
    if (!had_transient_user_activation_) {
      CompleteRequestWithError(
          FederatedAuthRequestResult::kMissingTransientUserActivation,
          TokenStatus::kMissingTransientUserActivation,
          /*should_delay_callback=*/false);
      return;
    }
  } else {
    rp_mode_ = RpMode::kPassive;
  }

  if (origin().opaque()) {
    CompleteRequestWithError(
        FederatedAuthRequestResult::kRelyingPartyOriginIsOpaque,
        TokenStatus::kRpOriginIsOpaque,
        /*should_delay_callback=*/false);
    return;
  }

  FederatedApiPermissionStatus permission_status = GetApiPermissionStatus();

  if (!CanBypassPermissionStatusCheck(rp_mode_, mediation_requirement_)) {
    if (permission_status != FederatedApiPermissionStatus::GRANTED) {
      std::pair<FederatedAuthRequestResult, TokenStatus> resultAndTokenStatus =
          PermissionStatusToRequestResultAndTokenStatus(permission_status);
      CompleteRequestWithError(resultAndTokenStatus.first,
                               resultAndTokenStatus.second,
                               /*should_delay_callback=*/true);
      return;
    }
  }

  ++num_requests_;

  std::set<GURL> unique_idps;
  for (auto& idp_get_params_ptr : idp_get_params_ptrs) {
    for (auto& idp_ptr : idp_get_params_ptr->providers) {
      // Throw an error if duplicate IDPs are specified.
      const bool is_unique_idp =
          unique_idps.insert(idp_ptr->config->config_url).second;
      if (!is_unique_idp) {
        CompleteRequestWithError(FederatedAuthRequestResult::kError,
                                 /*token_status=*/std::nullopt,
                                 /*should_delay_callback=*/false);
        return;
      }

      url::Origin idp_origin = url::Origin::Create(idp_ptr->config->config_url);
      if (!network::IsOriginPotentiallyTrustworthy(idp_origin)) {
        CompleteRequestWithError(
            FederatedAuthRequestResult::kIdpNotPotentiallyTrustworthy,
            TokenStatus::kIdpNotPotentiallyTrustworthy,
            /*should_delay_callback=*/false);
        return;
      }
    }
  }

  bool any_idp_has_custom_scopes = false;
  bool any_idp_has_parameters = false;
  for (auto& idp_get_params_ptr : idp_get_params_ptrs) {
    for (auto& idp_ptr : idp_get_params_ptr->providers) {
      bool has_failing_idp_signin_status =
          ShouldFailAccountsEndpointRequestBecauseNotSignedInWithIdp(
              render_frame_host(), idp_ptr->config->config_url,
              permission_delegate_);

      if (has_failing_idp_signin_status) {
        if (idp_get_params_ptr->mode == blink::mojom::RpMode::kPassive) {
          // In the multi IDP case, we do not want to complete the request
          // right away as there are other IDPs which may be logged in. But we
          // also do not want to fetch this IDP.
          unique_idps.erase(idp_ptr->config->config_url);
          continue;
        } else if (idp_get_params_ptr->mode == blink::mojom::RpMode::kActive) {
          // We fail sooner before, but just to double check, we assert that
          // we are inside a user gesture here again.
          CHECK(had_transient_user_activation_);
        }
      }
      if (ShouldFailBeforeFetchingAccounts(idp_ptr->config->config_url)) {
        // In the multi IDP case, we do not want to complete the request right
        // away as there are other IDPs which may be logged in. But we also do
        // not want to fetch this IDP.
        unique_idps.erase(idp_ptr->config->config_url);
        continue;
      }

      any_idp_has_custom_scopes = any_idp_has_custom_scopes ||
                                  GetDisclosureFields(idp_ptr->fields).empty();
      any_idp_has_parameters = any_idp_has_parameters || idp_ptr->params_json;

      blink::mojom::RpContext rp_context = idp_get_params_ptr->context;
      blink::mojom::RpMode rp_mode = idp_get_params_ptr->mode;
      const GURL& idp_config_url = idp_ptr->config->config_url;
      std::optional<blink::mojom::Format> format =
          IsDelegationEnabled() ? idp_ptr->format : std::nullopt;
      token_request_get_infos_.emplace(
          idp_config_url, IdentityProviderGetInfo(std::move(idp_ptr),
                                                  rp_context, rp_mode, format));
    }
  }

  if (any_idp_has_parameters || any_idp_has_custom_scopes) {
    RpParameters parameters;
    if (any_idp_has_custom_scopes && any_idp_has_parameters) {
      parameters = RpParameters::kHasParametersAndNonDefaultScope;
    } else if (any_idp_has_parameters) {
      parameters = RpParameters::kHasParameters;
    } else {
      DCHECK(any_idp_has_custom_scopes);
      parameters = RpParameters::kHasNonDefaultScope;
    }
    fedcm_metrics_->RecordRpParameters(parameters);
  }

  if (unique_idps.empty()) {
    // At this point either all IDPs are signed out or mediation:silent was used
    // and there are no returning accounts.
    auto result = mediation_requirement_ == MediationRequirement::kSilent
                      ? FederatedAuthRequestResult::kSilentMediationFailure
                      : FederatedAuthRequestResult::kNotSignedInWithIdp;
    auto token_status = mediation_requirement_ == MediationRequirement::kSilent
                            ? TokenStatus::kSilentMediationFailure
                            : TokenStatus::kNotSignedInWithIdp;
    CompleteRequestWithError(result, token_status,
                             /*should_delay_callback=*/true);
    return;
  }

  // Show loading dialog while fetching endpoints if it is a active flow. This
  // is needed even if the LoginStatus is "logged-out" because we need to fetch
  // the config file to get the login_url which may take some time.
  if (rp_mode_ == RpMode::kActive) {
    CHECK_GT(idp_order_.size(), 0u);
    // TODO(crbug.com/40218857): Handle active mode with multiple IdP.
    const GURL& idp_config_url = idp_order_[0];
    auto get_info_it = token_request_get_infos_.find(idp_config_url);
    CHECK(get_info_it != token_request_get_infos_.end());
    if (!request_dialog_controller_->ShowLoadingDialog(
            CreateRpData(/*client_metadata_received=*/false),
            FormatOriginForDisplay(url::Origin::Create(idp_config_url)),
            get_info_it->second.rp_context, rp_mode_,
            base::BindOnce(&RequestService::OnDialogDismissed,
                           weak_ptr_factory_.GetWeakPtr()))) {
      return;
    }
  }

  fedcm_metrics_->RecordIdentityProvidersCount(idp_order_.size());

  CHECK(!unique_idps.empty());
  if (rp_mode_ == RpMode::kPassive && idp_order_.size() == 1u) {
    request_dialog_controller_->ShouldShowAccountsPassiveDialog(
        base::BindOnce(&RequestService::OnShouldShowAccountsPassiveDialogResult,
                       weak_ptr_factory_.GetWeakPtr(), std::move(unique_idps)));
    return;
  }
  FetchEndpointsForIdps(std::move(unique_idps));
}

void RequestService::RequestUserInfo(
    blink::mojom::IdentityProviderConfigPtr provider,
    RequestUserInfoCallback callback) {
  if (!render_frame_host().GetPage().IsPrimary()) {
    ReportBadMessage("FedCM should not be allowed in nested frame trees.");
    return;
  }
  // FedCmMetrics class is currently not used for UserInfo API. If we log UKM
  // metrics later on, we should call CreateFedCmMetrics() here.

  auto network_manager = IdpNetworkRequestManager::Create(
      static_cast<RenderFrameHostImpl*>(&render_frame_host()));
  auto user_info_request = UserInfoRequest::Create(
      std::move(network_manager), permission_delegate_,
      api_permission_delegate_, &render_frame_host(), std::move(provider));
  UserInfoRequest* user_info_request_ptr = user_info_request.get();
  user_info_requests_.insert(std::move(user_info_request));

  user_info_request_ptr->SetCallbackAndStart(base::BindOnce(
      &RequestService::CompleteUserInfoRequest, weak_ptr_factory_.GetWeakPtr(),
      user_info_request_ptr, std::move(callback)));
}

void RequestService::CancelTokenRequest() {
  if (!auth_request_token_callback_) {
    // This can happen if the renderer requested an abort() after the browser
    // invoked the callback but before the renderer received the callback.
    return;
  }

  // Dialog will be hidden by the destructor for request_dialog_controller_,
  // triggered by CompleteRequest.

  CompleteRequestWithError(FederatedAuthRequestResult::kCanceled,
                           TokenStatus::kAborted,
                           /*should_delay_callback=*/false);
}

void RequestService::ResolveTokenRequest(
    const std::optional<std::string>& account_id,
    base::Value token,
    ResolveTokenRequestCallback callback) {
  if (!identity_registry_ && !SetupIdentityRegistryFromPopup()) {
    std::move(callback).Run(false);
    return;
  }

  bool accepted =
      identity_registry_->NotifyResolve(origin(), account_id, token);
  std::move(callback).Run(accepted);
}

void RequestService::SetIdpSigninStatus(
    const url::Origin& idp_origin,
    blink::mojom::IdpSigninStatus status,
    const std::optional<blink::common::webid::LoginStatusOptions>& options,
    SetIdpSigninStatusCallback callback) {
  auto scoped_closure = base::ScopedClosureRunner(std::move(callback));

  if (render_frame_host().IsNestedWithinFencedFrame()) {
    RecordSetLoginStatusIgnoredReason(
        SetLoginStatusIgnoredReason::kInFencedFrame);
    return;
  }
  // We only allow setting the IDP signin status when the subresource is loaded
  // from the same site as the document, and the document is same site with
  // all ancestors. This is to protect from an RP embedding a tracker resource
  // that would set this signin status for the tracker, enabling the FedCM
  // request.
  if (!IsSameSiteWithAncestors(idp_origin, &render_frame_host())) {
    RecordSetLoginStatusIgnoredReason(
        SetLoginStatusIgnoredReason::kCrossOrigin);
    return;
  }

  if (!IsLightweightModeEnabled()) {
    permission_delegate_->SetIdpSigninStatus(
        idp_origin, status == blink::mojom::IdpSigninStatus::kSignedIn,
        std::nullopt);
  } else {
    if (options.has_value()) {
      std::vector<GURL> picture_urls;
      for (const blink::common::webid::LoginStatusAccount& account :
           options->accounts) {
        if (account.picture.has_value() && account.picture->is_valid()) {
          picture_urls.emplace_back(account.picture.value());
        }
      }
      if (!network_manager_) {
        network_manager_ = CreateNetworkManager();
      }
      network_manager_->CacheAccountPictures(idp_origin, picture_urls);
    }
    permission_delegate_->SetIdpSigninStatus(
        idp_origin, status == blink::mojom::IdpSigninStatus::kSignedIn,
        options);
  }
}

void RequestService::RegisterIdP(const GURL& idp,
                                 RegisterIdPCallback callback) {
  if (!IsIdPRegistrationEnabled()) {
    std::move(callback).Run(RegisterIdpStatus::kErrorFeatureDisabled);
    return;
  }

  if (!origin().IsSameOriginWith(url::Origin::Create(idp))) {
    std::move(callback).Run(RegisterIdpStatus::kErrorCrossOriginConfig);
    return;
  }

  if (!render_frame_host().HasTransientUserActivation()) {
    std::move(callback).Run(RegisterIdpStatus::kErrorNoTransientActivation);
    return;
  }

  if (!network_manager_) {
    network_manager_ = CreateNetworkManager();
  }

  fedcm_idp_registration_handler_ = std::make_unique<IdpRegistrationHandler>(
      render_frame_host(), network_manager_.get(), idp);
  fedcm_idp_registration_handler_->FetchConfig(
      base::BindOnce(&RequestService::OnIdpRegistrationConfigFetched,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback), idp));
}

void RequestService::OnIdpRegistrationConfigFetched(
    RegisterIdPCallback callback,
    const GURL& idp,
    std::vector<ConfigFetcher::FetchResult> fetch_results) {
  CHECK_EQ(fetch_results.size(), 1u);
  fedcm_idp_registration_handler_.reset();
  if (fetch_results[0].error) {
    std::move(callback).Run(RegisterIdpStatus::kErrorInvalidConfig);
    return;
  }

  if (!request_dialog_controller_) {
    request_dialog_controller_ = CreateDialogController();
  }

  request_dialog_controller_->RequestIdPRegistrationPermision(
      origin(),
      base::BindOnce(&RequestService::OnRegisterIdPPermissionResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback), idp));
}

void RequestService::OnRegisterIdPPermissionResponse(
    RegisterIdPCallback callback,
    const GURL& idp,
    bool accepted) {
  if (accepted) {
    permission_delegate_->RegisterIdP(idp);
  }
  std::move(callback).Run(accepted ? RegisterIdpStatus::kSuccess
                                   : RegisterIdpStatus::kErrorDeclined);
}

void RequestService::UnregisterIdP(const GURL& idp,
                                   UnregisterIdPCallback callback) {
  if (!IsIdPRegistrationEnabled()) {
    std::move(callback).Run(false);
    return;
  }
  if (!origin().IsSameOriginWith(url::Origin::Create(idp))) {
    std::move(callback).Run(false);
    return;
  }
  permission_delegate_->UnregisterIdP(idp);
  std::move(callback).Run(true);
}

void RequestService::OnIdpSigninStatusReceived(
    const url::Origin& idp_config_origin,
    bool idp_signin_status) {
  if (!idp_signin_status) {
    return;
  }

  for (const auto& [get_idp_config_url, get_info] : token_request_get_infos_) {
    if (url::Origin::Create(get_idp_config_url) == idp_config_origin) {
      permission_delegate_->RemoveIdpSigninStatusObserver(this);
      idps_user_tried_to_signin_to_.insert(get_idp_config_url);
      FetchEndpointsForIdps({get_idp_config_url});
      break;
    }
  }
}

bool RequestService::HasPendingRequest() const {
  RequestPageData* page_data = GetPageData(render_frame_host().GetPage());
  bool has_pending_request = page_data->PendingWebIdentityRequest() != nullptr;
  DCHECK(has_pending_request || !auth_request_token_callback_);
  return has_pending_request;
}

void RequestService::FetchEndpointsForIdps(
    const std::set<GURL>& idp_config_urls) {
  int icon_ideal_size =
      request_dialog_controller_->GetBrandIconIdealSize(rp_mode_);
  int icon_minimum_size =
      request_dialog_controller_->GetBrandIconMinimumSize(rp_mode_);
  std::set<GURL> pending_idps = std::move(fetch_data_.pending_idps);
  pending_idps.insert(idp_config_urls.begin(), idp_config_urls.end());
  fetch_data_ = FetchData();
  fetch_data_.pending_idps = std::move(pending_idps);

  fedcm_accounts_fetcher_ = std::make_unique<AccountsFetcher>(
      render_frame_host(), network_manager_.get(), api_permission_delegate_,
      permission_delegate_,
      AccountsFetcher::FedCmFetchingParams(
          rp_mode_, icon_ideal_size, icon_minimum_size, mediation_requirement_),
      this);
  fedcm_accounts_fetcher_->FetchEndpointsForIdps(idp_config_urls);
}

void RequestService::CompleteDisconnectRequest(
    DisconnectCallback callback,
    blink::mojom::DisconnectStatus status) {
  // `disconnect_request_` may be null here if the completion is invoked from
  // the RequestService destructor, which destroys
  // `disconnect_request_`. The DisconnectRequest destructor would
  // trigger the callback.
  if (!disconnect_request_ &&
      status == blink::mojom::DisconnectStatus::kSuccess) {
    NOTREACHED() << "The successful disconnect request is nowhere to be found";
  }
  std::move(callback).Run(status);
  disconnect_request_.reset();
}

bool RequestService::CanShowContinueOnPopup() const {
  if (mediation_requirement_ == MediationRequirement::kConditional) {
    // Because conditional mediation always requires a user gesture to sign in,
    // we can always allow the continuation popup.
    return true;
  }

  if (mediation_requirement_ == MediationRequirement::kSilent) {
    return false;
  }

  if (mediation_requirement_ == MediationRequirement::kRequired) {
    // In this case, we always have a user gesture (the user had to choose
    // an account), so we can show a popup.
    return true;
  }

  if (identity_selection_type_ == kExplicit) {
    return true;
  }
  DCHECK_EQ(identity_selection_type_, kAutoPassive);
  return had_transient_user_activation_;
}

UseOtherAccountResult RequestService::ComputeUseOtherAccountResult(
    blink::mojom::FederatedAuthRequestResult result,
    const std::optional<GURL>& selected_idp_config_url) {
  if (result != FederatedAuthRequestResult::kSuccess) {
    return UseOtherAccountResult::kUserDoesNotSignIn;
  }

  CHECK(selected_idp_config_url);
  if (IsEndpointSameOrigin(*selected_idp_config_url, login_url_) &&
      !account_ids_before_login_.contains(account_id_)) {
    return UseOtherAccountResult::kUserSignsInWithNewAccount;
  }
  return UseOtherAccountResult::kUserSignsInWithExistingAccount;
}

void RequestService::OnFetchDataForIdpSucceeded(
    std::vector<IdentityRequestAccountPtr> accounts,
    std::unique_ptr<IdentityProviderInfo> idp_info) {
  fetch_data_.did_succeed_for_at_least_one_idp = true;

  const GURL& idp_config_url = idp_info->provider->config->config_url;
  // If the IDP data existed before, we need to remove the old accounts data.
  // This can happen with the 'use other account' feature.
  if (idp_infos_.find(idp_config_url) != idp_infos_.end()) {
    std::erase_if(accounts_, [&idp_config_url](const auto& account) {
      return account->identity_provider->idp_metadata.config_url ==
             idp_config_url;
    });
  }
  idp_infos_[idp_config_url] = std::move(idp_info);
  idp_accounts_[idp_config_url] = std::move(accounts);

  fetch_data_.pending_idps.erase(idp_config_url);
  MaybeShowAccountsDialog();
}

void RequestService::SetIdpLoginInfo(const GURL& idp_login_url,
                                     const std::string& login_hint,
                                     const std::string& domain_hint) {
  idp_login_infos_[idp_login_url] = {login_hint, domain_hint};
}

void RequestService::SetWellKnownAndConfigFetchedTime(base::TimeTicks time) {
  well_known_and_config_fetched_time_ = time;
  fedcm_metrics_->RecordWellKnownAndConfigFetchTime(
      well_known_and_config_fetched_time_ - start_time_);
}

void RequestService::OnFetchDataForIdpFailed(
    const std::unique_ptr<IdentityProviderInfo> idp_info,
    blink::mojom::FederatedAuthRequestResult result,
    std::optional<RequestIdTokenStatus> token_status,
    bool should_delay_callback) {
  const GURL& idp_config_url = idp_info->provider->config->config_url;
  fetch_data_.pending_idps.erase(idp_config_url);

  if (fetch_data_.pending_idps.empty() &&
      !fetch_data_.did_succeed_for_at_least_one_idp) {
    CompleteRequestWithError(result, token_status, should_delay_callback);
    return;
  }

  AddDevToolsIssue(result);
  AddConsoleErrorMessage(result);

  // We do not call both OnFetchDataForIdpFailed() after OnFetchDataSucceeded()
  // for the same IDP.
  DCHECK(idp_infos_.find(idp_config_url) == idp_infos_.end());
  MaybeShowAccountsDialog();
}

const std::optional<std::vector<IdentityRequestAccountPtr>>
RequestService::GetAutofillSuggestions() const {
  // Requires conditional FedCM to be enabled.
  if (!IsAutofillEnabled()) {
    return std::nullopt;
  }

  // There isn't a request hanging.
  if (!HasPendingRequest()) {
    return std::nullopt;
  }

  // We only augment autofill when it is a conditional mediation request.
  if (mediation_requirement_ != MediationRequirement::kConditional) {
    return std::nullopt;
  }

  return GetAccounts();
}

void RequestService::MaybeShowAccountsDialog() {
  if (!fetch_data_.pending_idps.empty()) {
    return;
  }

  // The accounts fetch could be delayed for legitimate reasons. A user may be
  // able to disable FedCM API (e.g. via settings or dismissing another FedCM UI
  // on the same RP origin) before the browser receives the accounts response.
  // We should exit early without showing any UI.
  if (!CanBypassPermissionStatusCheck(rp_mode_, mediation_requirement_) &&
      GetApiPermissionStatus() != FederatedApiPermissionStatus::GRANTED) {
    CompleteRequestWithError(FederatedAuthRequestResult::kDisabledInSettings,
                             TokenStatus::kDisabledInSettings,
                             /*should_delay_callback=*/true);
    return;
  }

  // This map may have contents already if we came here through the "Add
  // Account" flow or the IDP login mismatch in multiple IDP case.
  idp_data_for_display_.clear();

  for (const auto& idp : idp_order_) {
    auto idp_info_it = idp_infos_.find(idp);
    if (idp_info_it != idp_infos_.end() && idp_info_it->second->data) {
      idp_info_it->second->data->idp_metadata.has_filtered_out_account = false;
      idp_data_for_display_.push_back(idp_info_it->second->data);
    }
    auto accounts_it = idp_accounts_.find(idp);
    if (accounts_it != idp_accounts_.end()) {
      accounts_.insert(accounts_.end(),
                       std::make_move_iterator(accounts_it->second.begin()),
                       std::make_move_iterator(accounts_it->second.end()));
    }
  }
  idp_accounts_.clear();

  std::stable_sort(
      accounts_.begin(), accounts_.end(),
      [&](const auto& account1, const auto& account2) {
        // Show filtered accounts after valid ones.
        if (account1->is_filtered_out || account2->is_filtered_out) {
          return !account1->is_filtered_out;
        }
        // Show newly logged in accounts, if any.
        bool is_account1_new = IsNewlyLoggedIn(*account1);
        bool is_account2_new = IsNewlyLoggedIn(*account2);
        if (is_account1_new || is_account2_new) {
          return !is_account2_new;
        }
        // Show returning accounts before non-returning.
        if (account1->idp_claimed_login_state.value_or(
                account1->browser_trusted_login_state) == LoginState::kSignUp ||
            account2->idp_claimed_login_state.value_or(
                account2->browser_trusted_login_state) == LoginState::kSignUp) {
          return account1->idp_claimed_login_state.value_or(
                     account1->browser_trusted_login_state) ==
                 LoginState::kSignIn;
        }
        // Within returning accounts, prefer those with last used
        // timestamp.
        if (!account1->last_used_timestamp || !account2->last_used_timestamp) {
          return !!account1->last_used_timestamp;
        }
        // If both have last used timestamp, prefer the latest.
        return *account1->last_used_timestamp > *account2->last_used_timestamp;
      });
  // Set the display priority for newly logged in accounts.
  for (const auto& account : accounts_) {
    if (IsNewlyLoggedIn(*account)) {
      account->display_priority = IdentityRequestAccount::DisplayPriority::kNew;
    } else {
      account->display_priority =
          IdentityRequestAccount::DisplayPriority::kRegular;
    }
    if (account->is_filtered_out) {
      account->identity_provider->idp_metadata.has_filtered_out_account = true;
    }
  }

  // Conditional mediation doesn't display the account chooser when called,
  // it instead waits for another UI surface (say, autofill) to trigger the
  // account chooser.
  if (mediation_requirement_ == MediationRequirement::kConditional) {
    request_dialog_controller_->NotifyAutofillSourceReadyForTesting();
    return;
  }

  // TODO(crbug.com/40246099): Handle auto_reauthn_ for multi IDP.
  // TODO(crbug.com/380367784): Handle auto_reauthn_ for delegated IdP.
  bool auto_reauthn_enabled =
      mediation_requirement_ != MediationRequirement::kRequired;

  dialog_type_ = auto_reauthn_enabled ? DialogType::kAutoReauth
                                      : DialogType::kSelectAccount;
  bool is_auto_reauthn_setting_enabled = false;
  bool is_auto_reauthn_embargoed = false;
  bool is_auto_reauthn_blocked_by_embedder =
      IsFedCmEmbedderCheckEnabled() &&
      auto_reauthn_permission_delegate_->IsAutoReauthnDisabledByEmbedder(
          WebContents::FromRenderFrameHost(&render_frame_host()));

  std::optional<base::TimeDelta> time_from_embargo;
  bool requires_user_mediation = false;
  IdentityProviderDataPtr auto_reauthn_idp = nullptr;
  IdentityRequestAccountPtr auto_reauthn_account = nullptr;
  bool has_single_returning_account = false;
  if (auto_reauthn_enabled) {
    is_auto_reauthn_setting_enabled =
        auto_reauthn_permission_delegate_->IsAutoReauthnSettingEnabled();
    is_auto_reauthn_embargoed =
        auto_reauthn_permission_delegate_->IsAutoReauthnEmbargoed(
            GetEmbeddingOrigin());
    if (is_auto_reauthn_embargoed) {
      time_from_embargo =
          base::Time::Now() -
          auto_reauthn_permission_delegate_->GetAutoReauthnEmbargoStartTime(
              GetEmbeddingOrigin());

      // See `kFederatedIdentityAutoReauthnEmbargoDuration`.
      render_frame_host().AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kInfo,
          "Auto re-authn was previously triggered less than 10 minutes ago. "
          "Only one auto re-authn request can be made every 10 minutes.");
    }
    requires_user_mediation = RequiresUserMediation();
    // Auto signs in returning users if they have a single returning account and
    // are signing in.
    has_single_returning_account =
        GetAccountForAutoReauthn(&auto_reauthn_idp, &auto_reauthn_account);
    if (dialog_type_ == DialogType::kAutoReauth &&
        (requires_user_mediation || !is_auto_reauthn_setting_enabled ||
         is_auto_reauthn_embargoed || !has_single_returning_account ||
         is_auto_reauthn_blocked_by_embedder)) {
      dialog_type_ = DialogType::kSelectAccount;
    }
    if (!has_single_returning_account &&
        mediation_requirement_ == MediationRequirement::kSilent) {
      fedcm_metrics_->RecordAutoReauthnMetrics(
          has_single_returning_account, auto_reauthn_account.get(),
          dialog_type_ == DialogType::kAutoReauth,
          !is_auto_reauthn_setting_enabled, is_auto_reauthn_embargoed,
          is_auto_reauthn_blocked_by_embedder, time_from_embargo,
          requires_user_mediation);

      // By this moment we know that the user has granted permission in the past
      // for the RP/IdP. Because otherwise we have returned already in
      // `ShouldFailBeforeFetchingAccounts`. It means that we don't need to show
      // any UI to respect `mediation: silent`.
      render_frame_host().AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kError,
          "Silent mediation issue: the user has used FedCM with multiple "
          "accounts on this site.");
      CompleteRequestWithError(
          FederatedAuthRequestResult::kSilentMediationFailure,
          TokenStatus::kSilentMediationFailure,
          /*should_delay_callback=*/true);
      return;
    }

    if (dialog_type_ == DialogType::kAutoReauth) {
      accounts_ = {auto_reauthn_account};
      idp_data_for_display_ = {auto_reauthn_idp};
      accounts_[0]->identity_provider = idp_data_for_display_[0];
    }
  }

  if (dialog_type_ != DialogType::kAutoReauth) {
    identity_selection_type_ = kExplicit;
  } else if (rp_mode_ == blink::mojom::RpMode::kPassive) {
    identity_selection_type_ = kAutoPassive;
  } else {
    identity_selection_type_ = kAutoActive;
  }

  if (auto_reauthn_enabled) {
    fedcm_metrics_->RecordAutoReauthnMetrics(
        has_single_returning_account, auto_reauthn_account.get(),
        dialog_type_ == DialogType::kAutoReauth,
        !is_auto_reauthn_setting_enabled, is_auto_reauthn_embargoed,
        is_auto_reauthn_blocked_by_embedder, time_from_embargo,
        requires_user_mediation);
  }

  // The RenderFrameHost may be alive but not visible in the following
  // situations:
  // Situation #1: User switched tabs
  // Situation #2: User navigated the page with bfcache
  //
  // - If this fetch is as a result of an IdP sign-in status change, the FedCM
  // dialog is either visible or temporarily hidden. Update the contents of
  // the dialog.
  // - If the FedCM dialog has not already been shown, do not show the dialog
  // if the RenderFrameHost is hidden because the user does not seem interested
  // in the contents of the current page.
  if (idps_user_tried_to_signin_to_.empty()) {
    bool is_active = IsFrameActive(render_frame_host().GetMainFrame());
    fedcm_metrics_->RecordWebContentsStatusUponReadyToShowDialog(
        IsFrameVisible(render_frame_host().GetMainFrame()), is_active);

    if (!is_active) {
      CompleteRequestWithError(FederatedAuthRequestResult::kRpPageNotVisible,
                               TokenStatus::kRpPageNotVisible,
                               /*should_delay_callback=*/true);
      return;
    }

    ready_to_display_accounts_dialog_time_ = base::TimeTicks::Now();
    fedcm_metrics_->RecordShowAccountsDialogTime(
        idp_data_for_display_,
        ready_to_display_accounts_dialog_time_ - start_time_);

    fedcm_metrics_->RecordShowAccountsDialogTimeBreakdown(
        well_known_and_config_fetched_time_ - start_time_,
        accounts_fetched_time_ - well_known_and_config_fetched_time_,
        client_metadata_fetched_time_ != base::TimeTicks()
            ? client_metadata_fetched_time_ - accounts_fetched_time_
            : base::TimeDelta());
  }
  bool did_succeed_for_at_least_one_idp =
      fetch_data_.did_succeed_for_at_least_one_idp;

  fetch_data_ = FetchData();

  // RenderFrameHost should be in the primary page (ex not in the BFCache).
  DCHECK(render_frame_host().GetPage().IsPrimary());

  bool intercept = false;
  // In tests (content_shell or when --use-fake-ui-for-fedcm is used), the
  // dialog controller will immediately select an account. But if browser
  // automation is enabled, we don't want that to happen because automation
  // should be able to choose which account to select or to cancel.
  // So we use this call to see whether interception is enabled.
  // It is not needed in regular Chrome even when automation is used because
  // there, the dialog will wait for user input anyway.
  devtools_instrumentation::WillShowFedCmDialog(render_frame_host(),
                                                &intercept);
  // Since we don't reuse the controller for each request, and intercept
  // defaults to false, we only need to call this if intercept is true.
  if (intercept) {
    request_dialog_controller_->SetIsInterceptionEnabled(intercept);
  }

  if (identity_selection_type_ != kExplicit) {
    OnAccountSelected(accounts_[0]->identity_provider->idp_metadata.config_url,
                      accounts_[0]->id, /*is_sign_in=*/true);
    if (!request_dialog_controller_->ShowVerifyingDialog(
            CreateRpData(/*client_metadata_received=*/true), auto_reauthn_idp,
            accounts_[0], SignInMode::kAuto, rp_mode_,
            base::BindOnce(&RequestService::OnAccountsDisplayed,
                           weak_ptr_factory_.GetWeakPtr()))) {
      return;
    }
  } else {
    if (!request_dialog_controller_->ShowAccountsDialog(
            CreateRpData(/*client_metadata_received=*/true),
            idp_data_for_display_, accounts_, rp_mode_,
            base::BindOnce(&RequestService::OnAccountSelected,
                           weak_ptr_factory_.GetWeakPtr()),
            base::BindRepeating(&RequestService::LoginToIdP,
                                weak_ptr_factory_.GetWeakPtr(),
                                /*can_append_hints=*/false),
            base::BindOnce(&RequestService::OnDialogDismissed,
                           weak_ptr_factory_.GetWeakPtr()),
            base::BindOnce(&RequestService::OnAccountsDisplayed,
                           weak_ptr_factory_.GetWeakPtr()))) {
      return;
    }
  }
  AfterAccountsDialogShown(did_succeed_for_at_least_one_idp);
}

void RequestService::OnShouldShowAccountsPassiveDialogResult(
    const std::set<GURL>& unique_idps,
    bool should_show) {
  if (!should_show) {
    CompleteRequestWithError(
        FederatedAuthRequestResult::kSuppressedBySegmentationPlatform,
        TokenStatus::kSuppressedBySegmentationPlatform,
        /*should_delay_callback=*/true);
    return;
  }
  FetchEndpointsForIdps(std::move(unique_idps));
}

void RequestService::AfterAccountsDialogShown(
    bool did_succeed_for_at_least_one_idp) {
  devtools_instrumentation::DidShowFedCmDialog(render_frame_host());

  if (identity_selection_type_ == kExplicit &&
      did_succeed_for_at_least_one_idp) {
    // We omit recording the accounts dialog shown metric for auto re-authn
    // because the metric is used to detect IDPs flashing UI. Auto re-authn
    // verifying UI cannot be flashed since it is destroyed automatically after
    // 3 seconds and cannot be destroyed earlier for a11y reasons.
    accounts_dialog_shown_time_ = base::TimeTicks::Now();
  }

  // Note that accounts dialog shown after mismatch dialog is also recorded.
  // Although not useful for catching malicious IDPs, it should only be a very
  // small percentage of the samples recorded.
  fedcm_metrics_->RecordAccountsDialogShown(idp_data_for_display_);
  fedcm_metrics_->RecordRpUrlHasPath(
      render_frame_host().GetMainFrame()->GetLastCommittedURL().GetPath() !=
      "/");
}

void RequestService::NotifyAutofillSuggestionAccepted(
    const GURL& idp,
    const std::string& account_id,
    bool show_modal,
    OnFederatedTokenReceivedCallback callback) {
  token_received_callback_for_autofill_ = std::move(callback);

  // Currently the verified email flow opens a modal UI upon notification and
  // the autofill dropdown UI gets dismissed immediately. i.e. it doesn't need a
  // valid callback. However, if a user is presented a full federated account,
  // upon the account selection we'd proceed with fetching tokens directly and
  // update he autofill dropdown UI to a loading UI.
  if (!show_modal) {
    OnAccountSelected(idp, account_id, true);
    return;
  }
  // TODO(crbug.com/380367784): The third argument of OnAccountSelected checks
  // if this is a sign-in or a sign-up moment. In delegation, however, by
  // design, the IdP doesn't get to learn about the presentations, so wouldn't
  // know whether this is a sign-in or sign-up moment (e.g. wouldn't have a
  // approved_clients array). We should figure out how to reconcile these two
  // modes.
  auto get_info_it = token_request_get_infos_.find(idp);

  // TODO(crbug.com/412640661): Currently, in order to skip the account chooser
  // and go straight to the disclosure UI, we have to call ShowLoadingDialog()
  // before we can call ShowAccountsDialog() to create the internal state
  // necessary in the dialog controller. We should probably be able to create
  // the internal state on demand in case it isn't available.
  if (!request_dialog_controller_->ShowLoadingDialog(
          CreateRpData(/*client_metadata_received=*/true),
          FormatOriginForDisplay(url::Origin::Create(idp)),
          get_info_it->second.rp_context, blink::mojom::RpMode::kActive,
          base::BindOnce(&RequestService::OnDialogDismissed,
                         weak_ptr_factory_.GetWeakPtr()))) {
    return;
  }

  std::vector<IdentityRequestAccountPtr> selected;

  for (auto account : accounts_) {
    if (account->identity_provider->idp_metadata.config_url == idp &&
        account->id == account_id) {
      selected.push_back(account);
    }
  }

  // TODO(crbug.com/412640661): in order to skip the account chooser, we
  // set the display priority to `kNew`. We should probably refactor the API to
  // support this use case, rather than overload an unintended use.
  for (const auto& account : selected) {
    account->display_priority = IdentityRequestAccount::DisplayPriority::kNew;
  }
  if (!request_dialog_controller_->ShowAccountsDialog(
          CreateRpData(/*client_metadata_received=*/true),
          idp_data_for_display_, selected, blink::mojom::RpMode::kActive,
          base::BindOnce(&RequestService::OnAccountSelected,
                         weak_ptr_factory_.GetWeakPtr()),
          base::BindRepeating(&RequestService::LoginToIdP,
                              weak_ptr_factory_.GetWeakPtr(),
                              /*can_append_hints=*/false),
          base::BindOnce(&RequestService::OnDialogDismissed,
                         weak_ptr_factory_.GetWeakPtr()),
          base::BindOnce(&RequestService::OnAccountsDisplayed,
                         weak_ptr_factory_.GetWeakPtr()))) {
    return;
  }
  // TODO(crbug.com/435216589): Should we call AfterAccountsDialogShown here?
}

void RequestService::OnAccountsDisplayed() {
  accounts_dialog_display_time_ = base::TimeTicks::Now();
}

void RequestService::OnIdpMismatch(
    std::unique_ptr<IdentityProviderInfo> idp_info) {
  const GURL& idp_config_url = idp_info->provider->config->config_url;

  idp_infos_[idp_config_url] = std::move(idp_info);

  fetch_data_.pending_idps.erase(idp_config_url);
  if (!fetch_data_.pending_idps.empty()) {
    return;
  }

  // Invoke the accounts dialog flow if there is at least one account or more
  // than one IDP for which we should show the mismatch dialog.
  // TODO(crbug.com/331426009): make this code clearer by creating a separate
  // method for showing multiple mismatch UI.
  if (fetch_data_.did_succeed_for_at_least_one_idp || idp_infos_.size() > 1u) {
    MaybeShowAccountsDialog();
    // If there are no successful IDPs, this is the multi IDP case where all are
    // mismatch.
    if (!fetch_data_.did_succeed_for_at_least_one_idp) {
      mismatch_dialog_shown_time_ = base::TimeTicks::Now();
      has_shown_mismatch_ = true;
      devtools_instrumentation::DidShowFedCmDialog(render_frame_host());
    }
    return;
  }

  if (rp_mode_ == RpMode::kActive) {
    MaybeShowActiveModeModalDialog(
        idp_config_url, idp_infos_[idp_config_url]->metadata.idp_login_url);
    return;
  }

  ShowSingleIdpFailureDialog();
}

void RequestService::ShowSingleIdpFailureDialog() {
  CHECK_EQ(idp_infos_.size(), 1u);
  IdentityProviderInfo* idp_info = idp_infos_.begin()->second.get();
  url::Origin idp_origin =
      url::Origin::Create(idp_info->provider->config->config_url);
  // RenderFrameHost should be in the primary page (ex not in the BFCache).
  DCHECK(render_frame_host().GetPage().IsPrimary());

  fetch_data_ = FetchData();

  // Set `idp_data_for_display_` so it is always the case that we can rely on it
  // to know which IDPs have been seen in the UI.
  CHECK(idp_info->data);
  idp_data_for_display_ = {idp_info->data};

  // If IdP login status mismatch dialog is already visible, calling
  // ShowFailureDialog() a 2nd time should notify the user that login
  // failed.
  dialog_type_ = DialogType::kConfirmIdpLogin;
  config_url_ = idp_info->provider->config->config_url;
  login_url_ = idp_info->metadata.idp_login_url;

  // Store variables used in RecordMismatchDialogShown since they may be cleaned
  // up in ShowFailureDialog().
  bool has_shown_mismatch = has_shown_mismatch_;
  bool has_hints = !idp_info->provider->login_hint.empty() ||
                   !idp_info->provider->domain_hint.empty() ||
                   !idp_info->metadata.requested_label.empty();

  if (!request_dialog_controller_->ShowFailureDialog(
          CreateRpData(/*client_metadata_received=*/true),
          FormatOriginForDisplay(idp_origin), idp_info->rp_context, rp_mode_,
          idp_info->metadata,
          base::BindOnce(&RequestService::OnDismissFailureDialog,
                         weak_ptr_factory_.GetWeakPtr()),
          base::BindRepeating(&RequestService::LoginToIdP,
                              weak_ptr_factory_.GetWeakPtr(),
                              /*can_append_hints=*/true))) {
    return;
  }

  CHECK_EQ(idp_data_for_display_.size(), 1u);
  fedcm_metrics_->RecordSingleIdpMismatchDialogShown(
      *idp_data_for_display_[0], has_shown_mismatch, has_hints);
  mismatch_dialog_shown_time_ = base::TimeTicks::Now();
  has_shown_mismatch_ = true;
  devtools_instrumentation::DidShowFedCmDialog(render_frame_host());
}

void RequestService::CloseModalDialogView() {
#if BUILDFLAG(IS_ANDROID)
  SetupIdentityRegistryFromPopup();
#endif
  // Invoke OnClose on the opener.
  if (identity_registry_) {
    identity_registry_->NotifyClose(origin());
  }
}

void RequestService::OnAccountSelected(const GURL& idp_config_url,
                                       const std::string& account_id,
                                       bool is_sign_in) {
  DCHECK(!account_id.empty());
  const IdentityProviderInfo& idp_info = *idp_infos_[idp_config_url];

  // Check if the user has disabled the FedCM API after the FedCM UI is
  // displayed. This ensures that requests are not wrongfully sent to IDPs when
  // settings are changed while an existing FedCM UI is displayed. Ideally, we
  // should enforce this check before all requests but users typically won't
  // have time to disable the FedCM API in other types of requests.
  // Note that for the active flow is not affected by the permission status.
  if (!CanBypassPermissionStatusCheck(rp_mode_, mediation_requirement_) &&
      GetApiPermissionStatus() != FederatedApiPermissionStatus::GRANTED) {
    CompleteRequestWithError(FederatedAuthRequestResult::kDisabledInSettings,
                             TokenStatus::kDisabledInSettings,
                             /*should_delay_callback=*/true);
    return;
  }

  if (identity_selection_type_ != kExplicit) {
    // Embargo auto re-authn to mitigate a deadloop where an auto
    // re-authenticated user gets auto re-authenticated again soon after logging
    // out of the active session.
    auto_reauthn_permission_delegate_->RecordEmbargoForAutoReauthn(
        GetEmbeddingOrigin());
  } else {
    // Once a user has explicitly selected an account, there is no need to block
    // auto re-authn with embargo.
    auto_reauthn_permission_delegate_->RemoveEmbargoForAutoReauthn(
        GetEmbeddingOrigin());

    // Record page scroll Y-axis position upon account selection to analyse
    // for intrusion. Do not record for auto re-authn because we want to detect
    // whether users scroll the webpage before choosing to sign-in.
    RenderFrameHostImpl* host_impl = static_cast<RenderFrameHostImpl*>(
        render_frame_host().GetOutermostMainFrame());
    host_impl->GetAssociatedLocalFrame()->GetScrollPosition(
        base::BindOnce(&RecordAccountSelectionScrollPosition,
                       render_frame_host().GetPageUkmSourceId(),
                       fedcm_metrics_->GetSessionID()));
  }

  fedcm_metrics_->RecordIsSignInUser(is_sign_in);

  api_permission_delegate_->RemoveEmbargoAndResetCounts(GetEmbeddingOrigin());

  account_id_ = account_id;
  select_account_time_ = base::TimeTicks::Now();
  fedcm_metrics_->RecordContinueOnPopupTime(
      idp_config_url, select_account_time_ - accounts_dialog_display_time_);

  IdpNetworkRequestManager::ContinueOnCallback continue_on = base::BindOnce(
      &RequestService::OnContinueOnResponseReceived,
      weak_ptr_factory_.GetWeakPtr(), idp_info.provider->Clone());

  std::vector<std::string> disclosure_shown_for;
  if (!is_sign_in) {
    disclosure_shown_for =
        DisclosureFieldsToStringList(idp_info.data->disclosure_fields);
  }

  CHECK(idp_info.data);

  has_sent_token_request_ = true;

  bool idp_blindness =
      idp_info.provider->format &&
      *idp_info.provider->format == blink::mojom::Format::kSdJwt;

  GURL endpoint;
  std::string query;
  if (idp_blindness) {
    // Checked previously.
    DCHECK(IsDelegationEnabled());

    endpoint = idp_info.endpoints.issuance;
    federated_sdjwt_handler_ = std::make_unique<FederatedSdJwtHandler>(
        idp_info.provider, render_frame_host(), this);
    query = federated_sdjwt_handler_->ComputeUrlEncodedTokenPostDataForIssuers(
        account_id);
  } else {
    endpoint = idp_info.endpoints.token;
    query = ComputeUrlEncodedTokenPostData(
        render_frame_host(), idp_info.provider->config->client_id,
        idp_info.provider->nonce, account_id,
        identity_selection_type_ != kExplicit, rp_mode_,
        idp_info.provider->fields, disclosure_shown_for,
        idp_info.provider->params_json.value_or(""),
        idp_info.provider->config->type);
  }

  network_manager_->SendTokenRequest(
      endpoint, account_id_, query, idp_blindness,
      base::BindOnce(&RequestService::OnTokenResponseReceived,
                     weak_ptr_factory_.GetWeakPtr(),
                     idp_info.provider->Clone()),
      std::move(continue_on),
      base::BindOnce(&RequestService::RecordErrorMetrics,
                     weak_ptr_factory_.GetWeakPtr(),
                     idp_info.provider->Clone()));
}

void RequestService::OnDismissFailureDialog(
    IdentityRequestDialogController::DismissReason dismiss_reason) {
  // Clicking the close active and swiping away the account chooser are more
  // intentional than other ways of dismissing the account chooser such as
  // the virtual keyboard showing on Android. Dismissal through closing the
  // pop-up window is not embargoed since the user has taken some action to
  // continue to open the pop-up window.
  bool should_embargo =
      dismiss_reason ==
          IdentityRequestDialogController::DismissReason::kCloseButton ||
      dismiss_reason == IdentityRequestDialogController::DismissReason::kSwipe;
  fedcm_metrics_->RecordCancelReason(dismiss_reason);

  should_embargo &= rp_mode_ == RpMode::kPassive;
  if (should_embargo) {
    api_permission_delegate_->RecordDismissAndEmbargo(GetEmbeddingOrigin());
  }

  CompleteRequestWithError(
      should_embargo ? FederatedAuthRequestResult::kShouldEmbargo
                     : FederatedAuthRequestResult::kUiDismissedNoEmbargo,
      should_embargo ? TokenStatus::kShouldEmbargo
                     : TokenStatus::kNotSignedInWithIdp,

      /*should_delay_callback=*/false);
}

void RequestService::OnDismissErrorDialog(
    const GURL& idp_config_url,
    FetchStatus status,
    IdentityRequestDialogController::DismissReason dismiss_reason) {
  bool has_url = token_error_ && !token_error_->url.is_empty();
  ErrorDialogResult result =
      DismissReasonToErrorDialogResult(dismiss_reason, has_url);
  fedcm_metrics_->RecordErrorDialogResult(result, idp_config_url);

  CompleteTokenRequest(idp_config_url, status, /*token=*/std::nullopt,
                       token_error_, /*should_delay_callback=*/false);
}

void RequestService::OnDialogDismissed(
    IdentityRequestDialogController::DismissReason dismiss_reason) {
  if (has_sent_token_request_) {
    verifying_dialog_result_ = identity_selection_type_ == kExplicit
                                   ? VerifyingDialogResult::kCancelExplicit
                                   : VerifyingDialogResult::kCancelAutoReauthn;
  }

  if (dialog_type_ == DialogType::kContinueOnPopup) {
    fedcm_metrics_->RecordContinueOnPopupResult(
        ContinueOnPopupResult::kWindowClosed);
    // Popups always get dismissed with reason kOther, so we never embargo.
    CompleteRequestWithError(FederatedAuthRequestResult::kError,
                             TokenStatus::kContinuationPopupClosedByUser,
                             /*should_delay_callback=*/false);
    return;
  }

  // Clicking the close active and swiping away the account chooser are more
  // intentional than other ways of dismissing the account chooser such as
  // the virtual keyboard showing on Android.
  bool should_embargo =
      dismiss_reason ==
          IdentityRequestDialogController::DismissReason::kCloseButton ||
      dismiss_reason == IdentityRequestDialogController::DismissReason::kSwipe;
  if (should_embargo) {
    base::TimeTicks dismiss_dialog_time = base::TimeTicks::Now();
    fedcm_metrics_->RecordCancelOnDialogTime(
        idp_data_for_display_,
        dismiss_dialog_time - accounts_dialog_display_time_);
  }
  fedcm_metrics_->RecordCancelReason(dismiss_reason);

  should_embargo &= rp_mode_ == RpMode::kPassive;
  if (should_embargo) {
    api_permission_delegate_->RecordDismissAndEmbargo(GetEmbeddingOrigin());
  }

  TokenStatus token_status;
  FederatedAuthRequestResult result;
  if (should_embargo) {
    token_status = TokenStatus::kShouldEmbargo;
    result = FederatedAuthRequestResult::kShouldEmbargo;
  } else {
    token_status = TokenStatus::kNotSelectAccount;
    result = FederatedAuthRequestResult::kUiDismissedNoEmbargo;
  }

  // Reject the promise immediately if the UI is dismissed without selecting
  // an account. Meanwhile, we fuzz the rejection time for other failures to
  // make it indistinguishable.
  CompleteRequestWithError(result, token_status,
                           /*should_delay_callback=*/false);
}

void RequestService::ShowModalDialog(DialogType dialog_type,
                                     const GURL& idp_config_url,
                                     const GURL& url_to_show) {
  // Reset dialog type, since we are typically not showing a FedCM dialog while
  // the popup window is open. When using the active flow the dialog may
  // still be up in some cases, but we do not expect that browser automation
  // needs to interact with the account chooser in this case.
  if (dialog_type_ != DialogType::kNone) {
    // This call ensures that we send a dialogClosed event if an account
    // chooser or mismatch dialog is open.
    devtools_instrumentation::DidCloseFedCmDialog(render_frame_host());
  }
  // TODO(crbug.com/336815315): Should we notify browser automation of this
  // dialog?
  dialog_type_ = dialog_type;
  config_url_ = idp_config_url;
  UMA_HISTOGRAM_ENUMERATION("Blink.FedCm.Popup.DialogType", dialog_type_);

  WebContents* web_contents = request_dialog_controller_->ShowModalDialog(
      url_to_show, rp_mode_,
      base::BindOnce(&RequestService::OnDialogDismissed,
                     weak_ptr_factory_.GetWeakPtr()));
  // This may be null on Android, as the method cannot return the WebContents of
  // the CCT that will be created.
  if (web_contents) {
    IdentityRegistry::CreateForWebContents(
        web_contents, weak_ptr_factory_.GetWeakPtr(), idp_config_url);
  }

  // Samples are at most 10 minutes. This metric is used to determine a
  // reasonable minimum duration for the mismatch dialog to be shown to prevent
  // abuse through flashing UI. When users trigger the IDP sign-in flow, the
  // mismatch dialog is hidden so we record this metric upon user triggering the
  // flow.
  if (mismatch_dialog_shown_time_.has_value()) {
    fedcm_metrics_->RecordMismatchDialogShownDuration(
        idp_data_for_display_,
        base::TimeTicks::Now() - mismatch_dialog_shown_time_.value());
    mismatch_dialog_shown_time_ = std::nullopt;
  }
}

void RequestService::OnContinueOnResponseReceived(
    IdentityProviderRequestOptionsPtr idp,
    FetchStatus status,
    const GURL& continue_on) {
  id_assertion_response_time_ = base::TimeTicks::Now();

  GetContentClient()->browser()->LogWebFeatureForCurrentPage(
      &render_frame_host(), blink::mojom::WebFeature::kFedCmContinueOnResponse);

  url::Origin idp_origin = url::Origin::Create(idp->config->config_url);
  // We only allow loading continue_on urls that are same-origin
  // with the IdP.
  // This isn't necessarily final, but seemed like a safer
  // and sufficient default for now.
  // This behavior may change in https://crbug.com/1429083
  bool is_same_origin =
      url::Origin::Create(continue_on).IsSameOriginWith(idp_origin);

  bool can_show_popup = CanShowContinueOnPopup();
  if (!is_same_origin || !can_show_popup) {
    if (!is_same_origin && !can_show_popup) {
      fedcm_metrics_->RecordContinueOnPopupStatus(
          ContinueOnPopupStatus::kUrlNotSameOriginAndPopupNotAllowed);
    } else if (!is_same_origin) {
      fedcm_metrics_->RecordContinueOnPopupStatus(
          ContinueOnPopupStatus::kUrlNotSameOrigin);
    } else if (!can_show_popup) {
      fedcm_metrics_->RecordContinueOnPopupStatus(
          ContinueOnPopupStatus::kPopupNotAllowed);
    }

    CompleteRequestWithError(
        FederatedAuthRequestResult::kIdTokenInvalidResponse,
        TokenStatus::kIdTokenInvalidResponse,

        /*should_delay_callback=*/false);
    return;
  }

  fedcm_metrics_->RecordContinueOnPopupStatus(
      webid::ContinueOnPopupStatus::kPopupOpened);
  ShowModalDialog(DialogType::kContinueOnPopup, idp->config->config_url,
                  continue_on);
}

void RequestService::ShowErrorDialog(const GURL& idp_config_url,
                                     FetchStatus status,
                                     std::optional<TokenError> token_error) {
  CHECK(idp_infos_.find(idp_config_url) != idp_infos_.end());

  dialog_type_ = DialogType::kError;
  config_url_ = idp_config_url;
  token_request_status_ = status;
  token_error_ = token_error;

  // TODO(crbug.com/40282657): Refactor IdentityCredentialTokenError
  if (!request_dialog_controller_->ShowErrorDialog(
          CreateRpData(/*client_metadata_received=*/true),
          FormatOriginForDisplay(url::Origin::Create(idp_config_url)),
          idp_infos_[idp_config_url]->rp_context, rp_mode_,
          idp_infos_[idp_config_url]->metadata, token_error,
          base::BindOnce(&RequestService::OnDismissErrorDialog,
                         weak_ptr_factory_.GetWeakPtr(), idp_config_url,
                         status),
          token_error && !token_error->url.is_empty()
              ? base::BindOnce(&RequestService::ShowModalDialog,
                               weak_ptr_factory_.GetWeakPtr(),
                               DialogType::kErrorUrlPopup, config_url_,
                               token_error->url)
              : base::NullCallback())) {
    return;
  }
  devtools_instrumentation::DidShowFedCmDialog(render_frame_host());
}

void RequestService::OnTokenResponseReceived(
    IdentityProviderRequestOptionsPtr idp,
    FetchStatus status,
    IdpNetworkRequestManager::TokenResult&& result) {
  CHECK(result.token.has_value() || result.error.has_value());

  verifying_dialog_result_ = identity_selection_type_ == kExplicit
                                 ? VerifyingDialogResult::kSuccessExplicit
                                 : VerifyingDialogResult::kSuccessAutoReauthn;

  bool should_show_error_ui =
      result.error || status.parse_status != ParseStatus::kSuccess;
  auto complete_request_callback =
      should_show_error_ui
          ? base::BindOnce(&RequestService::ShowErrorDialog,
                           weak_ptr_factory_.GetWeakPtr(),
                           idp->config->config_url, status, result.error)
          : base::BindOnce(&RequestService::CompleteTokenRequest,
                           weak_ptr_factory_.GetWeakPtr(),
                           idp->config->config_url, status,
                           std::move(result.token), result.error,
                           /*should_delay_callback=*/false);

  // When fetching id tokens we show a "Verify" sheet to users in case fetching
  // takes a long time due to latency etc. In case that the fetching process is
  // fast, we still want to show the "Verify" sheet for at least
  // `kTokenRequestDelay` seconds for better UX.
  // Note that for active flow or conditional flow we can complete without delay
  // because there is no contextual UI displayed to users.
  id_assertion_response_time_ = base::TimeTicks::Now();
  base::TimeDelta fetch_time =
      id_assertion_response_time_ - select_account_time_;
  if (should_complete_request_immediately_ || rp_mode_ == RpMode::kActive ||
      mediation_requirement_ == MediationRequirement::kConditional ||
      fetch_time >= kTokenRequestDelay) {
    std::move(complete_request_callback).Run();
    return;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, std::move(complete_request_callback),
      kTokenRequestDelay - fetch_time);
}

void RequestService::MarkUserAsSignedIn(const GURL& idp_config_url,
                                        const std::string& account_id) {
  // Auto re-authentication can only be triggered when there's already a
  // sharing permission OR the IdP is exempted with 3PC access. Either way
  // we shouldn't explicitly grant permission here.
  CHECK(!account_id_.empty());
  if (identity_selection_type_ == kExplicit) {
    permission_delegate_->GrantSharingPermission(
        origin(), GetEmbeddingOrigin(), url::Origin::Create(idp_config_url),
        account_id);
  } else {
    permission_delegate_->RefreshExistingSharingPermission(
        origin(), GetEmbeddingOrigin(), url::Origin::Create(idp_config_url),
        account_id);
  }

  SetRequiresUserMediation(false, base::DoNothing());
}

void RequestService::CompleteTokenRequest(const GURL& idp_config_url,
                                          FetchStatus status,
                                          std::optional<base::Value> token,
                                          std::optional<TokenError> token_error,
                                          bool should_delay_callback) {
  DCHECK(!start_time_.is_null());
  constexpr char kIdAssertionUrl[] = "id assertion endpoint";
  if (status.parse_status != ParseStatus::kSuccess) {
    MaybeAddResponseCodeToConsole(render_frame_host(), kIdAssertionUrl,
                                  status.response_code);
    std::pair<FederatedAuthRequestResult, TokenStatus> resultAndTokenStatus =
        IdAssertionFetchStatusToRequestResultAndTokenStatus(status);
    CompleteRequestWithError(resultAndTokenStatus.first,
                             resultAndTokenStatus.second,
                             should_delay_callback);
    return;
  }
  if (token_error_) {
    MaybeAddResponseCodeToConsole(render_frame_host(), kIdAssertionUrl,
                                  status.response_code);
    if (error_url_type_ && *error_url_type_ == ErrorUrlType::kCrossSite) {
      CompleteRequestWithError(
          FederatedAuthRequestResult::kIdTokenCrossSiteIdpErrorResponse,
          TokenStatus::kIdTokenCrossSiteIdpErrorResponse,
          should_delay_callback);
      return;
    }
    CompleteRequestWithError(
        FederatedAuthRequestResult::kIdTokenIdpErrorResponse,
        TokenStatus::kIdTokenIdpErrorResponse, should_delay_callback);
    return;
  }

  MarkUserAsSignedIn(idp_config_url, account_id_);

  fedcm_metrics_->RecordTokenResponseAndTurnaroundTime(
      idp_config_url, id_assertion_response_time_ - select_account_time_,
      id_assertion_response_time_ - start_time_ -
          (accounts_dialog_display_time_ -
           ready_to_display_accounts_dialog_time_));

  const IdentityProviderRequestOptionsPtr& provider =
      idp_infos_[idp_config_url]->provider;
  DCHECK(provider);

  if (provider->format && *provider->format == blink::mojom::Format::kSdJwt) {
    if (token->is_string()) {
      federated_sdjwt_handler_->ProcessSdJwt(token->GetString());
      return;
    } else {
      CompleteRequestWithError(FederatedAuthRequestResult::kError,
                               TokenStatus::kIdTokenInvalidResponse,
                               /*should_delay_callback=*/false);
      return;
    }
  }

  CompleteRequest(FederatedAuthRequestResult::kSuccess,
                  TokenStatus::kSuccessUsingTokenInHttpResponse,
                  /*token_error=*/std::nullopt, idp_config_url,
                  std::move(*token),
                  /*should_delay_callback=*/false);
}

void RequestService::CompleteRequestWithError(
    blink::mojom::FederatedAuthRequestResult result,
    std::optional<RequestIdTokenStatus> token_status,
    bool should_delay_callback) {
  CompleteRequest(result, token_status, token_error_,
                  /*selected_idp_config_url=*/std::nullopt,
                  /*token_data=*/std::nullopt, should_delay_callback);
}

void RequestService::CompleteRequest(
    blink::mojom::FederatedAuthRequestResult result,
    std::optional<RequestIdTokenStatus> token_status,
    std::optional<TokenError> token_error,
    const std::optional<GURL>& selected_idp_config_url,
    std::optional<base::Value> token_data,
    bool should_delay_callback) {
  DCHECK(result == FederatedAuthRequestResult::kSuccess ||
         !token_data.has_value());

  if (accounts_dialog_shown_time_.has_value()) {
    fedcm_metrics_->RecordAccountsDialogShownDuration(
        idp_data_for_display_,
        base::TimeTicks::Now() - accounts_dialog_shown_time_.value());
    accounts_dialog_shown_time_ = std::nullopt;
  }

  if (mismatch_dialog_shown_time_.has_value()) {
    fedcm_metrics_->RecordMismatchDialogShownDuration(
        idp_data_for_display_,
        base::TimeTicks::Now() - mismatch_dialog_shown_time_.value());
    mismatch_dialog_shown_time_ = std::nullopt;
  }

  if (!auth_request_token_callback_) {
    return;
  }

  if (token_status) {
    int num_idps_mismatch = std::count_if(
        idp_data_for_display_.begin(), idp_data_for_display_.end(),
        [](auto& provider) { return provider->has_login_status_mismatch; });
    std::optional<UseOtherAccountResult> use_other_account_result;
    // We know that use other account was used if and only if
    // account_ids_before_login_ is not empty.
    if (!account_ids_before_login_.empty()) {
      use_other_account_result =
          ComputeUseOtherAccountResult(result, selected_idp_config_url);
    }

    if (!verifying_dialog_result_ && has_sent_token_request_) {
      verifying_dialog_result_ =
          identity_selection_type_ == kExplicit
              ? VerifyingDialogResult::kDestroyExplicit
              : VerifyingDialogResult::kDestroyAutoReauthn;
    }

    std::optional<bool> has_signin_account;
    // Note: accounts_ does not include the ones that got filtered out. In case
    // that all accounts are filtered out, we'd show the mismatch UI and skip
    // recording the account status on the mismatch UI.
    for (const auto& account : accounts_) {
      has_signin_account = false;
      if (account->idp_claimed_login_state.value_or(
              account->browser_trusted_login_state) == LoginState::kSignIn) {
        has_signin_account = true;
        break;
      }
    }

    fedcm_metrics_->RecordRequestTokenStatus(
        *token_status, mediation_requirement_, idp_order_, num_idps_mismatch,
        selected_idp_config_url, rp_mode_, use_other_account_result,
        verifying_dialog_result_,
        api_permission_delegate_->AreThirdPartyCookiesEnabledInSettings()
            ? ThirdPartyCookiesStatus::kEnabledInSettings
            : ThirdPartyCookiesStatus::kDisabledInSettings,
        ComputeRequesterFrameType(render_frame_host(), origin(),
                                  GetEmbeddingOrigin()),
        has_signin_account, request_dialog_controller_->DidShowUi());
  }

  if (result == FederatedAuthRequestResult::kSuccess) {
    CHECK(selected_idp_config_url);
    CHECK(fedcm_accounts_fetcher_);
    if (IsMetricsEndpointEnabled()) {
      fedcm_accounts_fetcher_->SendSuccessfulTokenRequestMetrics(
          *selected_idp_config_url,
          ready_to_display_accounts_dialog_time_ - start_time_,
          select_account_time_ - accounts_dialog_display_time_,
          id_assertion_response_time_ - select_account_time_,
          id_assertion_response_time_ - start_time_ -
              (accounts_dialog_display_time_ -
               ready_to_display_accounts_dialog_time_),
          request_dialog_controller_->DidShowUi());
    }
  } else if (!errors_logged_to_console_) {
    errors_logged_to_console_ = true;

    AddDevToolsIssue(result);
    AddConsoleErrorMessage(result);

    // fedcm_accounts_fetcher_ could be null if configs were not fetched, e.g.
    // because of cooldown.
    if (IsMetricsEndpointEnabled() && fedcm_accounts_fetcher_) {
      fedcm_accounts_fetcher_->SendAllFailedTokenRequestMetrics(
          result, request_dialog_controller_->DidShowUi());
    }
  }

  bool is_auto_selected = identity_selection_type_ != kExplicit;

  if (ShouldNotifyDevtoolsForDialogType(dialog_type_)) {
    devtools_instrumentation::DidCloseFedCmDialog(render_frame_host());
  }

  if (token_received_callback_for_autofill_) {
    std::move(token_received_callback_for_autofill_)
        .Run(result == FederatedAuthRequestResult::kSuccess);
  }

  if (!should_delay_callback || should_complete_request_immediately_) {
    CleanUp();
    GetPageData(render_frame_host().GetPage())
        ->SetPendingWebIdentityRequest(nullptr);
    errors_logged_to_console_ = false;

    blink::mojom::TokenErrorPtr error;
    if (token_error) {
      error = blink::mojom::TokenError::New();
      error->code = token_error->code;
      error->url = token_error->url.spec();
    }
    RequestTokenStatus status =
        FederatedAuthRequestResultToRequestTokenStatus(result);
    std::move(auth_request_token_callback_)
        .Run(status, selected_idp_config_url, std::move(token_data),
             std::move(error), is_auto_selected);
    auth_request_token_callback_.Reset();

    TRACE_EVENT_END("content.fedcm", perfetto_track_);
  } else {
    base::TimeDelta delay = GetRandomRejectionTime();
    TRACE_EVENT_INSTANT("content.fedcm", "Delaying FedCM rejection",
                        perfetto_track_, "delay", delay);
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&RequestService::OnRejectRequest,
                       weak_ptr_factory_.GetWeakPtr()),
        delay);
  }
}

void RequestService::CleanUp() {
  weak_ptr_factory_.InvalidateWeakPtrs();

  permission_delegate_->RemoveIdpSigninStatusObserver(this);

  // Given that |request_dialog_controller_| has reference to this web content
  // instance we destroy that first.
  request_dialog_controller_.reset();
  fedcm_accounts_fetcher_.reset();
  federated_sdjwt_handler_.reset();
  network_manager_.reset();
  fedcm_metrics_.reset();
  account_id_ = std::string();
  start_time_ = base::TimeTicks();
  well_known_and_config_fetched_time_ = base::TimeTicks();
  accounts_fetched_time_ = base::TimeTicks();
  client_metadata_fetched_time_ = base::TimeTicks();
  ready_to_display_accounts_dialog_time_ = base::TimeTicks();
  accounts_dialog_display_time_ = base::TimeTicks();
  select_account_time_ = base::TimeTicks();
  id_assertion_response_time_ = base::TimeTicks();
  accounts_dialog_shown_time_ = std::nullopt;
  mismatch_dialog_shown_time_ = std::nullopt;
  has_shown_mismatch_ = false;
  idp_accounts_.clear();
  accounts_.clear();
  idp_login_infos_.clear();
  idp_infos_.clear();
  idp_data_for_display_.clear();
  account_ids_before_login_.clear();
  fetch_data_ = FetchData();
  idps_user_tried_to_signin_to_.clear();
  idp_order_.clear();
  token_request_get_infos_.clear();
  login_url_ = GURL();
  config_url_ = GURL();
  token_error_ = std::nullopt;
  dialog_type_ = DialogType::kNone;
  identity_selection_type_ = kExplicit;
  had_transient_user_activation_ = false;
  rp_mode_ = RpMode::kPassive;
}

void RequestService::AddDevToolsIssue(FederatedAuthRequestResult result) {
  DCHECK_NE(result, FederatedAuthRequestResult::kSuccess);

  // It would be possible to add this inspector issue on the renderer, which
  // will receive the callback. However, it is preferable to do so on the
  // browser because this is closer to the source, which means adding
  // additional metadata is easier. In addition, in the future we may only
  // need to pass a small amount of information to the renderer in the case of
  // an error, so it would be cleaner to do this by reporting the inspector
  // issue from the browser.
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

void RequestService::AddConsoleErrorMessage(FederatedAuthRequestResult result) {
  render_frame_host().AddMessageToConsole(
      blink::mojom::ConsoleMessageLevel::kError,
      GetConsoleErrorMessageFromResult(result));
}

url::Origin RequestService::GetEmbeddingOrigin() const {
  return render_frame_host().GetMainFrame()->GetLastCommittedOrigin();
}

void RequestService::CompleteUserInfoRequest(
    UserInfoRequest* request,
    RequestUserInfoCallback callback,
    blink::mojom::RequestUserInfoStatus status,
    std::optional<std::vector<blink::mojom::IdentityUserInfoPtr>> user_info) {
  auto it =
      std::find_if(user_info_requests_.begin(), user_info_requests_.end(),
                   [request](const std::unique_ptr<UserInfoRequest>& ptr) {
                     return ptr.get() == request;
                   });
  // The request may not be found if the completion is invoked from
  // RequestService destructor. The destructor clears
  // `user_info_requests_`, which destroys the FederatedAuthUserInfoRequests it
  // contains. The FederatedAuthUserInfoRequest destructor invokes this
  // callback.
  if (it == user_info_requests_.end() &&
      status == blink::mojom::RequestUserInfoStatus::kSuccess) {
    NOTREACHED() << "The successful user info request is nowhere to be found";
  }
  std::move(callback).Run(status, std::move(user_info));
  if (it != user_info_requests_.end()) {
    user_info_requests_.erase(it);
  }
}

std::unique_ptr<IdpNetworkRequestManager>
RequestService::CreateNetworkManager() {
  if (mock_network_manager_) {
    return std::move(mock_network_manager_);
  }

  return IdpNetworkRequestManager::Create(
      static_cast<RenderFrameHostImpl*>(&render_frame_host()));
}

std::unique_ptr<IdentityRequestDialogController>
RequestService::CreateDialogController() {
  if (mock_dialog_controller_) {
    return std::move(mock_dialog_controller_);
  }

  WebContents* web_contents =
      WebContents::FromRenderFrameHost(&render_frame_host());
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeUIForFedCM)) {
    std::string selected_account =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kUseFakeUIForFedCM);
    return std::make_unique<FakeIdentityRequestDialogController>(
        selected_account.empty() ? std::nullopt
                                 : std::optional<std::string>(selected_account),
        web_contents);
  }

  return GetContentClient()->browser()->CreateIdentityRequestDialogController(
      web_contents);
}

void RequestService::SetNetworkManagerForTests(
    std::unique_ptr<IdpNetworkRequestManager> manager) {
  mock_network_manager_ = std::move(manager);
}

void RequestService::SetDialogControllerForTests(
    std::unique_ptr<IdentityRequestDialogController> controller) {
  mock_dialog_controller_ = std::move(controller);
}

void RequestService::OnClose() {
  CHECK(request_dialog_controller_);
  request_dialog_controller_->CloseModalDialog();

  // If we have not gotten a signin status change, abort the flow.
  // The same goes if we did get a status change but the accounts fetch
  // failed.
  if ((idps_user_tried_to_signin_to_.empty() ||
       (fetch_data_.pending_idps.empty() &&
        !fetch_data_.did_succeed_for_at_least_one_idp)) &&
      dialog_type_ == DialogType::kLoginToIdpPopup) {
    CompleteRequestWithError(FederatedAuthRequestResult::kError,
                             TokenStatus::kLoginPopupClosedWithoutSignin,
                             /*should_delay_callback=*/false);
    return;
  }

  // When IdentityProvider.close is called in the continuation popup, we
  // should abort the flow.
  if (dialog_type_ == DialogType::kContinueOnPopup) {
    fedcm_metrics_->RecordContinueOnPopupResult(
        ContinueOnPopupResult::kClosedByIdentityProviderClose);
    // Popups always get dismissed with reason kOther, so we never embargo.
    CompleteRequestWithError(
        FederatedAuthRequestResult::kError,
        TokenStatus::kContinuationPopupClosedByIdentityProviderClose,
        /*should_delay_callback=*/false);
    return;
  }
}

bool RequestService::OnResolve(GURL idp_config_url,
                               const std::optional<std::string>& account_id,
                               const base::Value& token) {
  // Close the pop-up window post user permission.
  if (!request_dialog_controller_) {
    return false;
  }

  // IdentityProvider.resolve() is only allowed for continuation API.
  if (dialog_type_ != DialogType::kContinueOnPopup) {
    return false;
  }

  request_dialog_controller_->CloseModalDialog();

  MarkUserAsSignedIn(idp_config_url, account_id.value_or(account_id_));

  fedcm_metrics_->RecordContinueOnResponseAndTurnaroundTime(
      id_assertion_response_time_ - select_account_time_,
      base::TimeTicks::Now() - start_time_ -
          (accounts_dialog_display_time_ -
           ready_to_display_accounts_dialog_time_));
  fedcm_metrics_->RecordContinueOnPopupResult(
      ContinueOnPopupResult::kTokenReceived);

  const IdentityProviderRequestOptionsPtr& provider =
      idp_infos_[idp_config_url]->provider;
  DCHECK(provider);

  if (provider->format && *provider->format == blink::mojom::Format::kSdJwt) {
    if (token.is_string()) {
      federated_sdjwt_handler_->ProcessSdJwt(token.GetString());
      return true;
    } else {
      CompleteRequestWithError(FederatedAuthRequestResult::kError,
                               TokenStatus::kIdTokenInvalidResponse,
                               /*should_delay_callback=*/false);
      return false;
    }
  }

  CompleteRequest(FederatedAuthRequestResult::kSuccess,
                  TokenStatus::kSuccessUsingIdentityProviderResolve,
                  /*token_error=*/std::nullopt, idp_config_url, token.Clone(),
                  /*should_delay_callback=*/false);
  // TODO(crbug.com/40262526): handle the corner cases where CompleteRequest
  // can't actually fulfill the request.
  return true;
}

void RequestService::OnOriginMismatch(Method method,
                                      const url::Origin& expected,
                                      const url::Origin& actual) {
  const char* method_string = method == Method::kClose ? "close" : "resolve";
  std::string error_messsage = base::StringPrintf(
      "IdentityProvider.%s called from incorrect origin '%s'; expected '%s'",
      method_string, actual.Serialize().c_str(), expected.Serialize().c_str());
  render_frame_host().AddMessageToConsole(
      blink::mojom::ConsoleMessageLevel::kError, error_messsage);
}

bool RequestService::SetupIdentityRegistryFromPopup() {
#if BUILDFLAG(IS_ANDROID)
  if (identity_registry_) {
    return true;
  }

  if (!request_dialog_controller_) {
    request_dialog_controller_ = CreateDialogController();
    CHECK(request_dialog_controller_);
  }

  // Because ShowModalDialog does not return the web contents on Android, we
  // need to set up the IdentityRegistry now.
  WebContents* rp_web_contents = request_dialog_controller_->GetRpWebContents();
  // This can be null if resolve was called in a regular tab (as opposed to
  // a CCT opened from ShowModalDialog).
  if (!rp_web_contents) {
    return false;
  }
  RequestService* rp_auth_request =
      GetPageData(rp_web_contents->GetPrimaryPage())
          ->PendingWebIdentityRequest();
  if (!rp_auth_request) {
    return false;
  }

  WebContents* web_contents =
      WebContents::FromRenderFrameHost(&render_frame_host());
  IdentityRegistry::CreateForWebContents(
      web_contents, rp_auth_request->weak_ptr_factory_.GetWeakPtr(),
      rp_auth_request->config_url_);
  identity_registry_ = IdentityRegistry::FromWebContents(web_contents);

  return true;
#else
  return false;
#endif
}

void RequestService::OnRejectRequest() {
  if (!auth_request_token_callback_) {
    return;
  }
  DCHECK(errors_logged_to_console_);
  CompleteRequestWithError(FederatedAuthRequestResult::kError,
                           /*token_status=*/std::nullopt,
                           /*should_delay_callback=*/false);
}

FederatedApiPermissionStatus RequestService::GetApiPermissionStatus() {
  DCHECK(api_permission_delegate_);
  return api_permission_delegate_->GetApiPermissionStatus(GetEmbeddingOrigin());
}

bool RequestService::ShouldNotifyDevtoolsForDialogType(DialogType type) {
  return type != DialogType::kNone && type != DialogType::kLoginToIdpPopup &&
         type != DialogType::kContinueOnPopup &&
         type != DialogType::kErrorUrlPopup;
}

void RequestService::AcceptAccountsDialogForDevtools(
    const GURL& config_url,
    const IdentityRequestAccount& account) {
  bool is_sign_in = account.idp_claimed_login_state.value_or(
                        account.browser_trusted_login_state) ==
                    IdentityRequestAccount::LoginState::kSignIn;
  OnAccountSelected(config_url, account.id, is_sign_in);
}

void RequestService::DismissAccountsDialogForDevtools(bool should_embargo) {
  // We somewhat arbitrarily pick a reason that does/does not trigger
  // cooldown.
  IdentityRequestDialogController::DismissReason reason =
      should_embargo
          ? IdentityRequestDialogController::DismissReason::kCloseButton
          : IdentityRequestDialogController::DismissReason::kOther;
  OnDialogDismissed(reason);
}

void RequestService::AcceptConfirmIdpLoginDialogForDevtools() {
  DCHECK(login_url_.is_valid());
  LoginToIdP(/*can_append_hints=*/true, config_url_, login_url_);
}

void RequestService::DismissConfirmIdpLoginDialogForDevtools() {
  // These values match what HandleAccountsFetchFailure passes.
  OnDismissFailureDialog(
      IdentityRequestDialogController::DismissReason::kOther);
}

bool RequestService::UseAnotherAccountForDevtools(
    const IdentityProviderData& provider) {
  if (!provider.idp_metadata.supports_add_account) {
    return false;
  }
  LoginToIdP(/*can_append_hints=*/true, provider.idp_metadata.config_url,
             provider.idp_metadata.idp_login_url);
  return true;
}

bool RequestService::HasMoreDetailsButtonForDevtools() {
  return token_error_ && token_error_->url.is_valid();
}

void RequestService::ClickErrorDialogGotItForDevtools() {
  DCHECK(token_error_);
  OnDismissErrorDialog(
      config_url_, token_request_status_,
      IdentityRequestDialogController::DismissReason::kGotItButton);
}

void RequestService::ClickErrorDialogMoreDetailsForDevtools() {
  DCHECK(token_error_ && token_error_->url.is_valid());
  ShowModalDialog(DialogType::kErrorUrlPopup, config_url_, token_error_->url);
  OnDismissErrorDialog(
      config_url_, token_request_status_,
      IdentityRequestDialogController::DismissReason::kMoreDetailsButton);
}

void RequestService::DismissErrorDialogForDevtools() {
  OnDismissErrorDialog(config_url_, token_request_status_,
                       IdentityRequestDialogController::DismissReason::kOther);
}

bool RequestService::GetAccountForAutoReauthn(
    IdentityProviderDataPtr* out_idp_data,
    IdentityRequestAccountPtr* out_account) {
  for (const auto& idp_info : idp_infos_) {
    if (idp_info.second->data->has_login_status_mismatch) {
      // If we need to show IDP login status mismatch UI, we cannot
      // auto-reauthenticate a user even if there really is a single returning
      // account.
      return false;
    }
  }
  for (const auto& account : accounts_) {
    if (account->idp_claimed_login_state.value_or(
            account->browser_trusted_login_state) == LoginState::kSignUp ||
        account->is_filtered_out) {
      continue;
    }
    // account.idp_claimed_login_state will be set to kSignIn if the client is
    // on the `approved_clients` list provided by IDP. However, in this case we
    // have to trust the browser observed sign-in unless the IDP can be
    // exempted. For example, they have third party cookies access on the RP
    // site.
    if (!HasSharingPermissionOrIdpHasThirdPartyCookiesAccess(
            render_frame_host(),
            /*provider_url=*/
            account->identity_provider->idp_metadata.config_url,
            GetEmbeddingOrigin(), origin(), account->id, permission_delegate_,
            api_permission_delegate_)) {
      continue;
    }

    if (*out_account) {
      return false;
    }
    *out_idp_data = account->identity_provider;
    *out_account = account;
  }

  if (*out_account) {
    return true;
  }

  return false;
}

bool RequestService::ShouldFailBeforeFetchingAccounts(const GURL& config_url) {
  if (mediation_requirement_ != MediationRequirement::kSilent) {
    return false;
  }

  bool is_auto_reauthn_blocked_by_embedder =
      IsFedCmEmbedderCheckEnabled() &&
      auto_reauthn_permission_delegate_->IsAutoReauthnDisabledByEmbedder(
          WebContents::FromRenderFrameHost(&render_frame_host()));
  if (is_auto_reauthn_blocked_by_embedder) {
    render_frame_host().AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError,
        "Silent mediation issue: ongoing actor task in the tab.");
  }

  bool is_auto_reauthn_setting_enabled =
      auto_reauthn_permission_delegate_->IsAutoReauthnSettingEnabled();
  if (!is_auto_reauthn_setting_enabled) {
    render_frame_host().AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError,
        "Silent mediation issue: the user has disabled auto re-authn.");
  }

  bool is_auto_reauthn_embargoed =
      auto_reauthn_permission_delegate_->IsAutoReauthnEmbargoed(
          GetEmbeddingOrigin());
  std::optional<base::TimeDelta> time_from_embargo;
  if (is_auto_reauthn_embargoed) {
    time_from_embargo =
        base::Time::Now() -
        auto_reauthn_permission_delegate_->GetAutoReauthnEmbargoStartTime(
            GetEmbeddingOrigin());
    render_frame_host().AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError,
        "Silent mediation issue: auto re-authn is in quiet period because it "
        "was recently used on this site.");
  }

  bool has_sharing_permission_for_any_account =
      HasSharingPermissionOrIdpHasThirdPartyCookiesAccess(
          render_frame_host(), config_url, GetEmbeddingOrigin(), origin(),
          /*account_id=*/std::nullopt, permission_delegate_,
          api_permission_delegate_);

  if (!has_sharing_permission_for_any_account) {
    render_frame_host().AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError,
        "Silent mediation issue: the user has not used FedCM on this site with "
        "this identity provider.");
  }

  bool requires_user_mediation = RequiresUserMediation();
  if (requires_user_mediation) {
    render_frame_host().AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError,
        "Silent mediation issue: preventSilentAccess() has been invoked on the "
        "site.");
  }

  if (requires_user_mediation || !is_auto_reauthn_setting_enabled ||
      is_auto_reauthn_embargoed || !has_sharing_permission_for_any_account ||
      is_auto_reauthn_blocked_by_embedder) {
    // Record the relevant auto reauthn metrics before aborting the FedCM flow.
    fedcm_metrics_->RecordAutoReauthnMetrics(
        /*has_single_returning_account=*/std::nullopt,
        /*auto_signin_account=*/nullptr,
        /*auto_reauthn_success=*/false, !is_auto_reauthn_setting_enabled,
        is_auto_reauthn_embargoed, is_auto_reauthn_blocked_by_embedder,
        time_from_embargo, requires_user_mediation);
    return true;
  }
  return false;
}

bool RequestService::RequiresUserMediation() {
  return auto_reauthn_permission_delegate_->RequiresUserMediation(origin());
}

void RequestService::SetRequiresUserMediation(bool requires_user_mediation,
                                              base::OnceClosure callback) {
  auto_reauthn_permission_delegate_->SetRequiresUserMediation(
      origin(), requires_user_mediation);
  if (permission_delegate_) {
    permission_delegate_->OnSetRequiresUserMediation(origin(),
                                                     std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

void RequestService::LoginToIdP(bool can_append_hints,
                                const GURL& idp_config_url,
                                GURL login_url) {
  const auto& it = idp_login_infos_.find(login_url);
  CHECK(it != idp_login_infos_.end());
  login_url_ = login_url;
  if (can_append_hints) {
    // Before invoking UI, append the query parameters to the `idp_login_url` if
    // needed.
    MaybeAppendQueryParameters(it->second, &login_url);
  }
  permission_delegate_->AddIdpSigninStatusObserver(this);

  account_ids_before_login_.clear();
  for (const auto& account : accounts_) {
    if (account->identity_provider->idp_metadata.idp_login_url == login_url) {
      account_ids_before_login_.insert(account->id);
    }
  }

  ShowModalDialog(DialogType::kLoginToIdpPopup, idp_config_url, login_url);
}

void RequestService::MaybeShowActiveModeModalDialog(const GURL& idp_config_url,
                                                    const GURL& idp_login_url) {
  if (idp_infos_.size() > 1) {
    // TODO(crbug.com/40283218): handle the active flow and the
    // Multi IdP API (what should happen if you are logged in to some
    // IdPs but not to others).
    // TODO(crbug.com/326987150): This is temporary so we should degrade
    // gracefully.
    return;
  }

  // We fail sooner before, but just to double check, we assert that
  // we are inside a user gesture here again.
  CHECK(had_transient_user_activation_);
  // TODO(crbug.com/40283219): we should probably make idp_login_url
  // optional instead of empty.
  LoginToIdP(/*can_append_hints=*/false, idp_config_url, idp_login_url);
  return;
}

void RequestService::PreventSilentAccess(PreventSilentAccessCallback callback) {
  SetRequiresUserMediation(true, std::move(callback));
  if (permission_delegate_->HasSharingPermission(GetEmbeddingOrigin())) {
    // Ensure the lifecycle state as GetPageUkmSourceId doesn't support the
    // prerendering page. As FederatedAuthRequest runs behind the
    // BrowserInterfaceBinders, the service doesn't receive any request while
    // prerendering, and the CHECK should always meet the condition.
    CHECK(!render_frame_host().IsInLifecycleState(
        RenderFrameHost::LifecycleState::kPrerendering));
    RecordPreventSilentAccess(
        ComputeRequesterFrameType(render_frame_host(), origin(),
                                  GetEmbeddingOrigin()),
        render_frame_host().GetPageUkmSourceId());
  }
}

void RequestService::Disconnect(
    blink::mojom::IdentityCredentialDisconnectOptionsPtr options,
    DisconnectCallback callback) {
  std::unique_ptr<Metrics> disconnect_metrics = CreateFedCmMetrics();
  if (disconnect_request_) {
    // Since we do not send any fetches in this case, consider the request to be
    // instant, e.g. duration is 0.
    render_frame_host().AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError,
        GetDisconnectConsoleErrorMessage(DisconnectStatus::kTooManyRequests));
    disconnect_metrics->RecordDisconnectMetrics(
        DisconnectStatus::kTooManyRequests, std::nullopt,
        ComputeRequesterFrameType(render_frame_host(), origin(),
                                  GetEmbeddingOrigin()),
        options->config->config_url);
    std::move(callback).Run(
        blink::mojom::DisconnectStatus::kErrorTooManyRequests);
    return;
  }

  bool intercept = false;
  bool should_complete_request_immediately = false;
  devtools_instrumentation::WillSendFedCmRequest(
      render_frame_host(), &intercept, &should_complete_request_immediately);

  auto network_manager = CreateNetworkManager();

  disconnect_request_ = DisconnectRequest::Create(
      std::move(network_manager), permission_delegate_, &render_frame_host(),
      std::move(disconnect_metrics), std::move(options));
  DisconnectRequest* disconnect_request_ptr = disconnect_request_.get();

  disconnect_request_ptr->SetCallbackAndStart(
      base::BindOnce(&RequestService::CompleteDisconnectRequest,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      api_permission_delegate_);
}

void RequestService::RecordErrorMetrics(
    IdentityProviderRequestOptionsPtr idp,
    TokenResponseType token_response_type,
    std::optional<ErrorDialogType> error_dialog_type,
    std::optional<ErrorUrlType> error_url_type) {
  fedcm_metrics_->RecordErrorMetricsBeforeShowingErrorDialog(
      token_response_type, error_dialog_type, error_url_type,
      idp->config->config_url);

  if (error_url_type) {
    // This is used to determine if we need to use the cross-site specific
    // devtools issue when failing the request.
    error_url_type_ = error_url_type;
  }
}

std::unique_ptr<Metrics> RequestService::CreateFedCmMetrics() {
  // Ensure the lifecycle state as GetPageUkmSourceId doesn't support the
  // prerendering page. As FederatedAithRequest runs behind the
  // BrowserInterfaceBinders, the service doesn't receive any request while
  // prerendering, and the CHECK should always meet the condition.
  CHECK(!render_frame_host().IsInLifecycleState(
      RenderFrameHost::LifecycleState::kPrerendering));

  return std::make_unique<Metrics>(render_frame_host().GetPageUkmSourceId());
}

bool RequestService::IsNewlyLoggedIn(const IdentityRequestAccount& account) {
  if (login_url_.is_empty() ||
      login_url_ != account.identity_provider->idp_metadata.idp_login_url) {
    return false;
  }
  // Exclude filtered out accounts so they are not shown at the top.
  return !account.is_filtered_out &&
         !account_ids_before_login_.contains(account.id);
}

bool RequestService::ShouldTerminateRequest(
    const std::vector<IdentityProviderGetParametersPtr>& idp_get_params_ptrs,
    const MediationRequirement& requirement) {
  // idp_get_params_ptrs sent from the renderer should be of size 1.
  if (idp_get_params_ptrs.size() != 1u) {
    ReportBadMessage("idp_get_params_ptrs should be of size 1.");
    return true;
  }
  // This could only happen with a compromised renderer process. We ensure that
  // the provider list size is > 0 on the renderer side at the beginning of
  // parsing |IdentityCredentialRequestOptions|.
  for (const auto& idp_get_params_ptr : idp_get_params_ptrs) {
    if (idp_get_params_ptr->providers.size() == 0) {
      ReportBadMessage("The provider list should not be empty.");
      return true;
    }
    if (idp_get_params_ptr->providers.size() > 10u) {
      ReportBadMessage("The provider list should not be greater than 10.");
      return true;
    }
    if (idp_get_params_ptr->mode == RpMode::kActive &&
        requirement == MediationRequirement::kSilent) {
      ReportBadMessage("mediation: silent is not supported in active mode.");
      return true;
    }
  }

  if (requirement == MediationRequirement::kConditional &&
      !IsAutofillEnabled()) {
    // The conditional mediation parameter can only be used when delegation
    // is enabled while it is under development.
    //
    // TODO(crbug.com/380367784): handle all of the many cases in which a
    // conditional mediation may interact with other features.
    ReportBadMessage(
        "Conditional mediation is not supported when both autofill and "
        "delegation are disabled.");
    return true;
  }

  if (render_frame_host().IsNestedWithinFencedFrame()) {
    ReportBadMessage("FedCM should not be allowed in fenced frame trees.");
    return true;
  }

  return false;
}

bool RequestService::HandlePendingRequestAndCancelNewRequest(
    const std::vector<GURL>& old_idp_order,
    const std::vector<IdentityProviderGetParametersPtr>& idp_get_params_ptrs,
    const MediationRequirement& requirement) {
  RequestService* pending_request =
      GetPageData(render_frame_host().GetPage())->PendingWebIdentityRequest();

  std::unique_ptr<Metrics> new_request_metrics = CreateFedCmMetrics();
  RpMode pending_request_rp_mode = pending_request->GetRpMode();
  RpMode new_request_rp_mode = idp_get_params_ptrs[0]->mode;
  new_request_metrics->RecordMultipleRequestsRpMode(
      pending_request_rp_mode, new_request_rp_mode, idp_order_);

  bool can_replace_pending_request = had_transient_user_activation_ &&
                                     new_request_rp_mode == RpMode::kActive &&
                                     pending_request_rp_mode != RpMode::kActive;
  if (!can_replace_pending_request) {
    // Cancel this new request.
    new_request_metrics->RecordRequestTokenStatus(
        TokenStatus::kTooManyRequests, requirement, idp_order_,
        /*num_idps_mismatch=*/0,
        /*selected_idp_config_url=*/std::nullopt,
        (idp_get_params_ptrs[0]->mode == blink::mojom::RpMode::kActive)
            ? RpMode::kActive
            : RpMode::kPassive,
        /*use_other_account_result=*/std::nullopt,
        /*verifying_dialog_result=*/std::nullopt,
        api_permission_delegate_->AreThirdPartyCookiesEnabledInSettings()
            ? ThirdPartyCookiesStatus::kEnabledInSettings
            : ThirdPartyCookiesStatus::kDisabledInSettings,
        ComputeRequesterFrameType(render_frame_host(), origin(),
                                  GetEmbeddingOrigin()),
        /*has_signin_account=*/std::nullopt, /*did_show_ui=*/false);

    AddDevToolsIssue(
        blink::mojom::FederatedAuthRequestResult::kTooManyRequests);
    AddConsoleErrorMessage(
        blink::mojom::FederatedAuthRequestResult::kTooManyRequests);

    // Since multiple `get` calls is not yet supported, if one IdP invokes the
    // API while another request from different IdPs is in-flight, the new API
    // call will be rejected. The two requests may be from different RFHs so
    // we should calculate properly.
    if (old_idp_order.empty()) {
      new_request_metrics->RecordMultipleRequestsFromDifferentIdPs(
          idp_order_ != pending_request->idp_order_);
    } else {
      new_request_metrics->RecordMultipleRequestsFromDifferentIdPs(
          idp_order_ != old_idp_order);
    }
    idp_order_ = std::move(old_idp_order);
    return true;
  }

  // Cancel the pending request before starting the new active flow request.
  // Set the old values before completing in case the pending request
  // corresponds to one in this object.
  std::vector<GURL> new_idp_order = std::move(idp_order_);
  idp_order_ = std::move(old_idp_order);
  pending_request->CompleteRequestWithError(
      FederatedAuthRequestResult::kReplacedByActiveMode,
      TokenStatus::kReplacedByActiveMode,
      /*should_delay_callback=*/false);
  CHECK(!auth_request_token_callback_);

  // Some members were reset to false during CleanUp when replacing a passive
  // flow from the same frame so we need to set them again.
  had_transient_user_activation_ = true;
  fedcm_metrics_ = std::move(new_request_metrics);
  idp_order_ = std::move(new_idp_order);

  return false;
}

RelyingPartyData RequestService::CreateRpData(
    bool client_metadata_received) const {
  // We want to show the iframe origin if any IDP requests it.
  bool show_iframe_origin = false;
  for (const auto& entry : idp_infos_) {
    if (entry.second->client_is_third_party_to_top_frame_origin) {
      show_iframe_origin = true;
      break;
    }
  }
  std::u16string iframe_origin;
  if (show_iframe_origin) {
    iframe_origin = base::UTF8ToUTF16(FormatOriginForDisplay(origin()));
  }
  bool display_strings_may_change = false;
  if (IsIframeOriginEnabled()) {
    display_strings_may_change =
        !client_metadata_received &&
        !net::SchemefulSite::IsSameSite(origin(), GetEmbeddingOrigin());
  }
  return RelyingPartyData(
      base::UTF8ToUTF16(GetTopFrameOriginForDisplay(GetEmbeddingOrigin())),
      iframe_origin, display_strings_may_change);
}

}  // namespace content::webid
