// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/request_service.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webid/disconnect_request.h"
#include "content/browser/webid/fake_identity_request_dialog_controller.h"
#include "content/browser/webid/flags.h"
#include "content/browser/webid/idp_registration_handler.h"
#include "content/browser/webid/metrics.h"
#include "content/browser/webid/request.h"
#include "content/browser/webid/request_page_data.h"
#include "content/browser/webid/user_info_request.h"
#include "content/browser/webid/webid_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/webid/federated_identity_api_permission_context_delegate.h"
#include "content/public/browser/webid/federated_identity_auto_reauthn_permission_context_delegate.h"
#include "content/public/browser/webid/federated_identity_permission_context_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/bindings/message.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"

namespace content::webid {

using DisconnectCallback =
    blink::mojom::FederatedRequestService::DisconnectCallback;
using MediationRequirement = ::password_manager::CredentialMediationRequirement;
using PreventSilentAccessCallback =
    blink::mojom::FederatedRequestService::PreventSilentAccessCallback;
using RegisterIdPCallback =
    blink::mojom::FederatedRequestService::RegisterIdPCallback;
using RequestTokenCallback = Request::RequestTokenCallback;
using RequestUserInfoCallback =
    blink::mojom::FederatedRequestService::RequestUserInfoCallback;
using ResolveTokenRequestCallback =
    blink::mojom::FederatedRequestService::ResolveTokenRequestCallback;
using SetIdpSigninStatusCallback =
    blink::mojom::FederatedRequestService::SetIdpSigninStatusCallback;
using StartTokenRequestCallback =
    blink::mojom::FederatedRequestService::StartTokenRequestCallback;
using TokenStatus = RequestIdTokenStatus;
using UnregisterIdPCallback =
    blink::mojom::FederatedRequestService::UnregisterIdPCallback;
using blink::mojom::RegisterIdpStatus;

DOCUMENT_USER_DATA_KEY_IMPL(RequestService);

RequestService::RequestService(RenderFrameHost* rfh)
    : DocumentUserData<RequestService>(rfh),
      api_permission_delegate_(
          rfh->GetBrowserContext()->GetFederatedIdentityApiPermissionContext()),
      auto_reauthn_permission_delegate_(
          rfh->GetBrowserContext()
              ->GetFederatedIdentityAutoReauthnPermissionContext()),
      permission_delegate_(
          rfh->GetBrowserContext()->GetFederatedIdentityPermissionContext()) {
  CHECK(api_permission_delegate_);
  CHECK(auto_reauthn_permission_delegate_);
  CHECK(permission_delegate_);
}

RequestService::~RequestService() {
  // Destroy the active request first, while weak pointers are still valid,
  // so that its destructor can successfully run the pending token request
  // callback via OnTokenRequestComplete.
  SetActiveRequestAndResetController(nullptr);

  // Invalidate weak pointers before clearing `user_info_requests_` to prevent
  // the destroying UserInfoRequests from calling back re-entrantly into
  // CompleteUserInfoRequest (which would cause container corruption during
  // clear).
  weak_ptr_factory_.InvalidateWeakPtrs();
  user_info_requests_.clear();
  disconnect_request_.reset();
  if (num_requests_ > 0) {
    Metrics::RecordNumRequestsPerDocument(
        render_frame_host().GetPageUkmSourceId(), num_requests_);
  }
}

void RequestService::BindFederatedRequestService(
    mojo::PendingReceiver<blink::mojom::FederatedRequestService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void RequestService::SetDelegatesForTesting(
    FederatedIdentityApiPermissionContextDelegate* api_permission_delegate,
    FederatedIdentityAutoReauthnPermissionContextDelegate*
        auto_reauthn_permission_delegate,
    FederatedIdentityPermissionContextDelegate* permission_delegate,
    IdentityRegistry* identity_registry) {
  api_permission_delegate_ = api_permission_delegate;
  auto_reauthn_permission_delegate_ = auto_reauthn_permission_delegate;
  permission_delegate_ = permission_delegate;
  mock_identity_registry_ = identity_registry;
}

Request* RequestService::GetOrCreateActiveRequest() {
  if (!active_request_) {
    RenderFrameHost& rfh = render_frame_host();
    SetActiveRequestAndResetController(std::make_unique<Request>(&rfh, *this));
  }
  return active_request_.get();
}

Request* RequestService::GetActiveRequestForTesting() const {
  return active_request_.get();
}

void RequestService::DestroyActiveRequestForTesting() {
  SetActiveRequestAndResetController(nullptr);
}

// static
void RequestService::InvokeTokenRequestCallback(
    StartTokenRequestCallback callback,
    blink::mojom::RequestTokenStatus status,
    const std::optional<GURL>& selected_idp_config_url,
    std::optional<base::Value> token,
    blink::mojom::TokenErrorPtr error,
    bool is_auto_selected) {
  if (status == blink::mojom::RequestTokenStatus::kSuccess) {
    auto success = blink::mojom::TokenRequestSuccess::New();
    success->selected_idp_config_url = selected_idp_config_url.value();
    success->token = std::move(token);
    success->is_auto_selected = is_auto_selected;
    std::move(callback).Run(std::move(success));
  } else {
    auto failure = blink::mojom::TokenRequestFailure::New();
    failure->status = status;
    failure->error = std::move(error);
    std::move(callback).Run(base::unexpected(std::move(failure)));
  }
}

void RequestService::StartTokenRequest(
    std::vector<blink::mojom::IdentityProviderGetParametersPtr> idp_get_params,
    MediationRequirement requirement,
    mojo::PendingReceiver<blink::mojom::FederatedRequest> request_receiver,
    StartTokenRequestCallback callback) {
  if (!render_frame_host().IsFeatureEnabled(
          network::mojom::PermissionsPolicyFeature::kIdentityCredentialsGet)) {
    receivers_.ReportBadMessage(
        "identity-credentials-get permissions policy not enabled");
    return;
  }

  if (idp_get_params.size() != 1u) {
    receivers_.ReportBadMessage("idp_get_params should be of size 1.");
    return;
  }

  if (idp_get_params[0]->providers.empty()) {
    receivers_.ReportBadMessage("The provider list should not be empty.");
    return;
  }

  if (idp_get_params[0]->providers.size() > 10u) {
    receivers_.ReportBadMessage(
        "The provider list should not be greater than 10.");
    return;
  }

  if (idp_get_params[0]->mode == blink::mojom::RpMode::kActive &&
      requirement == MediationRequirement::kSilent) {
    receivers_.ReportBadMessage(
        "mediation: silent is not supported in active mode.");
    return;
  }

  // The conditional mediation parameter can only be used when delegation
  // is enabled while it is under development.
  //
  // TODO(crbug.com/380367784): handle all of the many cases in which a
  // conditional mediation may interact with other features.
  if (requirement == MediationRequirement::kConditional &&
      !IsAutofillEnabled()) {
    receivers_.ReportBadMessage(
        "Conditional mediation is not supported when both autofill and "
        "delegation are disabled.");
    return;
  }

  if (render_frame_host().IsNestedWithinFencedFrame()) {
    receivers_.ReportBadMessage(
        "FedCM should not be allowed in fenced frame trees.");
    return;
  }

  RenderFrameHost& rfh = render_frame_host();
  auto new_request = std::make_unique<Request>(&rfh, *this);
  new_request->BindReceiver(std::move(request_receiver));

  auto wrapped_callback = base::BindOnce(
      &RequestService::InvokeTokenRequestCallback, std::move(callback));

  InitiateTokenRequest(std::move(new_request), std::move(idp_get_params),
                       requirement, /*navigation_handle=*/nullptr, GURL(),
                       std::move(wrapped_callback));
}

bool RequestService::StartTokenRequestFromNavigation(
    std::vector<blink::mojom::IdentityProviderGetParametersPtr> idp_get_params,
    MediationRequirement requirement,
    NavigationHandle* navigation_handle,
    const GURL& intercepted_url,
    RequestTokenCallback callback) {
  RenderFrameHost& rfh = render_frame_host();
  auto new_request = std::make_unique<Request>(&rfh, *this);

  return InitiateTokenRequest(std::move(new_request), std::move(idp_get_params),
                              requirement, navigation_handle, intercepted_url,
                              std::move(callback));
}

bool RequestService::InitiateTokenRequest(
    std::unique_ptr<Request> new_request,
    std::vector<blink::mojom::IdentityProviderGetParametersPtr> idp_get_params,
    MediationRequirement requirement,
    NavigationHandle* navigation_handle,
    const GURL& intercepted_url,
    RequestTokenCallback callback) {
  if (ShouldCancelNewRequest(new_request.get(), idp_get_params, requirement,
                             navigation_handle)) {
    std::move(callback).Run(
        blink::mojom::RequestTokenStatus::kErrorTooManyRequests, std::nullopt,
        std::nullopt, /*error=*/nullptr, /*is_auto_selected=*/false);
    return false;
  }

  // Wrap the callback to ensure the request is cleaned up from the active
  // request list and destroyed asynchronously when it completes.
  auto wrapper_callback = base::BindOnce(
      &RequestService::OnTokenRequestCompleteInternal,
      weak_ptr_factory_.GetWeakPtr(), new_request.get(), std::move(callback));

  // Temporarily hold the old active request and dialog controller on the
  // stack. This keeps it alive and valid during the RequestToken() checks,
  // preventing dangling pointers or Use-After-Free if the new request is
  // rejected or replaces the old one.
  std::unique_ptr<Request> old_request = std::move(active_request_);
  std::unique_ptr<IdentityRequestDialogController> old_dialog_controller =
      std::move(dialog_controller_);

  // Pre-assign the new request as active. This ensures that if the request
  // completes synchronously (e.g. in tests or synchronous error cases), the
  // completion callback will find it in `active_request_` and clean it up.
  SetActiveRequestAndResetController(std::move(new_request));

  // Call RequestToken on the new request.
  if (active_request_->RequestToken(std::move(idp_get_params), requirement,
                                    navigation_handle, intercepted_url,
                                    std::move(wrapper_callback))) {
    // If it started successfully, we keep it as the active request.
    // The `old_request` on the stack will go out of scope and be destroyed
    // safely.
    return true;
  } else {
    // If it failed immediately, discard the new request and restore the old
    // one.
    active_request_ = std::move(old_request);
    dialog_controller_ = std::move(old_dialog_controller);
    return false;
  }
}

void RequestService::OnTokenRequestCompleteInternal(
    Request* request,
    RequestTokenCallback callback,
    blink::mojom::RequestTokenStatus status,
    const std::optional<GURL>& selected_idp_config_url,
    std::optional<base::Value> token,
    blink::mojom::TokenErrorPtr error,
    bool is_auto_selected) {
  std::move(callback).Run(status, selected_idp_config_url, std::move(token),
                          std::move(error), is_auto_selected);
  CleanUpActiveRequest(request);
}

void RequestService::CleanUpActiveRequest(Request* request) {
  if (active_request_.get() == request) {
    std::unique_ptr<Request> completed_request = std::move(active_request_);
    // Invoke this to also reset the dialog controller.
    SetActiveRequestAndResetController(nullptr);
    // Release ownership synchronously to prevent race conditions with
    // subsequent requests, but keep it in completed_requests_ to ensure it does
    // not outlive RequestService.
    completed_requests_.push_back(std::move(completed_request));

    // Destroy the request asynchronously to allow the C++ call stack to unwind
    // safely.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&RequestService::CleanUpCompletedRequest,
                                  weak_ptr_factory_.GetWeakPtr(), request));
  }
}

void RequestService::SetActiveRequestAndResetController(
    std::unique_ptr<Request> request) {
  // Reset the dialog controller synchronously when changing the active request.
  // While this carries a potential Use-After-Free risk if the completion
  // callback was triggered synchronously from the dialog controller itself,
  // doing it synchronously is necessary to avoid asynchronous overlap where a
  // subsequent request could instantiate and display a new dialog before the
  // old one is destroyed, and ensures the dialog controller's data members are
  // kept clean for each new active request.
  dialog_controller_.reset();
  active_request_ = std::move(request);
}

void RequestService::CleanUpCompletedRequest(Request* request) {
  std::erase_if(completed_requests_,
                [&](const auto& r) { return r.get() == request; });
}

bool RequestService::ShouldCancelNewRequest(
    Request* new_request,
    const std::vector<blink::mojom::IdentityProviderGetParametersPtr>&
        idp_get_params,
    MediationRequirement requirement,
    NavigationHandle* navigation_handle) {
  Request* pending_request =
      GetPageData(render_frame_host().GetPage())->PendingWebIdentityRequest();
  if (!pending_request) {
    return false;
  }

  std::vector<GURL> new_idp_order;
  for (auto& idp_get_params_ptr : idp_get_params) {
    for (auto& idp_ptr : idp_get_params_ptr->providers) {
      new_idp_order.push_back(idp_ptr->config->config_url);
    }
  }

  bool had_transient_user_activation =
      (navigation_handle &&
       DidNavigationHandleHaveActivation(navigation_handle)) ||
      render_frame_host().HasTransientUserActivation();

  std::unique_ptr<Metrics> new_request_metrics = CreateFedCmMetrics();
  blink::mojom::RpMode pending_request_rp_mode = pending_request->GetRpMode();
  blink::mojom::RpMode new_request_rp_mode = idp_get_params[0]->mode;
  new_request_metrics->RecordMultipleRequestsRpMode(
      pending_request_rp_mode, new_request_rp_mode, new_idp_order);

  bool can_replace_pending_request =
      had_transient_user_activation &&
      new_request_rp_mode == blink::mojom::RpMode::kActive &&
      pending_request_rp_mode != blink::mojom::RpMode::kActive;
  if (!can_replace_pending_request) {
    new_request_metrics->RecordRequestTokenStatus(
        TokenStatus::kTooManyRequests, requirement, new_idp_order,
        /*num_idps_mismatch=*/0,
        /*selected_idp_config_url=*/std::nullopt,
        (idp_get_params[0]->mode == blink::mojom::RpMode::kActive)
            ? blink::mojom::RpMode::kActive
            : blink::mojom::RpMode::kPassive,
        /*use_other_account_result=*/std::nullopt,
        /*verifying_dialog_result=*/std::nullopt,
        api_permission_delegate_->AreThirdPartyCookiesEnabledInSettings()
            ? ThirdPartyCookiesStatus::kEnabledInSettings
            : ThirdPartyCookiesStatus::kDisabledInSettings,
        ComputeRequesterFrameType(
            render_frame_host(), render_frame_host().GetLastCommittedOrigin(),
            render_frame_host().GetMainFrame()->GetLastCommittedOrigin()),
        /*has_signin_account=*/std::nullopt, /*did_show_ui=*/false);

    auto details = blink::mojom::InspectorIssueDetails::New();
    details->federated_request_details =
        blink::mojom::FederatedRequestIssueDetails::New(
            blink::mojom::FederatedRequestResult::kTooManyRequests);
    render_frame_host().ReportInspectorIssue(
        blink::mojom::InspectorIssueInfo::New(
            blink::mojom::InspectorIssueCode::kFederatedAuthRequestIssue,
            std::move(details)));

    render_frame_host().AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError,
        GetConsoleErrorMessageFromResult(
            blink::mojom::FederatedRequestResult::kTooManyRequests));

    new_request_metrics->RecordMultipleRequestsFromDifferentIdPs(
        new_idp_order != pending_request->idp_order());

    return true;
  }

  new_request->fedcm_metrics_ = std::move(new_request_metrics);

  pending_request->CompleteRequestWithError(
      blink::mojom::FederatedRequestResult::kReplacedByActiveMode,
      TokenStatus::kReplacedByActiveMode,
      /*should_delay_callback=*/false);

  return false;
}

void RequestService::SetNetworkManagerForTests(
    std::unique_ptr<IdpNetworkRequestManager> manager) {
  mock_network_manager_ = std::move(manager);
}

std::unique_ptr<IdpNetworkRequestManager>
RequestService::CreateNetworkManager() {
  if (mock_network_manager_) {
    return std::move(mock_network_manager_);
  }
  return IdpNetworkRequestManager::Create(
      static_cast<RenderFrameHostImpl*>(&render_frame_host()));
}

void RequestService::RegisterIdP(const GURL& idp,
                                 RegisterIdPCallback callback) {
  if (!IsIdPRegistrationEnabled()) {
    std::move(callback).Run(RegisterIdpStatus::kErrorFeatureDisabled);
    return;
  }

  // The renderer checks this, but a compromised renderer can bypass it.
  if (!render_frame_host().GetLastCommittedOrigin().IsSameOriginWith(
          url::Origin::Create(idp))) {
    std::move(callback).Run(RegisterIdpStatus::kErrorCrossOriginConfig);
    return;
  }

  if (!registration_network_manager_) {
    registration_network_manager_ = CreateNetworkManager();
  }
  fedcm_idp_registration_handler_ = std::make_unique<IdpRegistrationHandler>(
      render_frame_host(), registration_network_manager_.get(), idp);

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
  registration_network_manager_.reset();
  if (fetch_results[0].error) {
    std::move(callback).Run(RegisterIdpStatus::kErrorInvalidConfig);
    return;
  }

  permission_delegate_->RegisterIdP(idp);
  std::move(callback).Run(RegisterIdpStatus::kSuccess);
}

void RequestService::UnregisterIdP(const GURL& idp,
                                   UnregisterIdPCallback callback) {
  if (!IsIdPRegistrationEnabled()) {
    std::move(callback).Run(false);
    return;
  }
  // The renderer checks this, but a compromised renderer can bypass it.
  if (!render_frame_host().GetLastCommittedOrigin().IsSameOriginWith(
          url::Origin::Create(idp))) {
    std::move(callback).Run(false);
    return;
  }
  permission_delegate_->UnregisterIdP(idp);
  std::move(callback).Run(true);
}

void RequestService::CloseModalDialogView() {
#if BUILDFLAG(IS_ANDROID)
  SetupIdentityRegistryFromPopup();
#endif
  // Invoke OnClose on the opener.
  if (IdentityRegistry* registry = GetIdentityRegistry()) {
    registry->NotifyClose(render_frame_host().GetLastCommittedOrigin());
  }
}
void RequestService::PreventSilentAccess(PreventSilentAccessCallback callback) {
  SetRequiresUserMediation(true, std::move(callback));
}

void RequestService::SetRequiresUserMediation(bool requires_user_mediation,
                                              base::OnceClosure callback) {
  auto_reauthn_permission_delegate_->SetRequiresUserMediation(
      render_frame_host().GetLastCommittedOrigin(), requires_user_mediation);
  if (permission_delegate_) {
    permission_delegate_->OnSetRequiresUserMediation(
        render_frame_host().GetLastCommittedOrigin(), std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

bool RequestService::SetupIdentityRegistryFromPopup() {
#if BUILDFLAG(IS_ANDROID)
  if (GetIdentityRegistry()) {
    return true;
  }
  IdentityRequestDialogController* controller = GetOrCreateDialogController();
  CHECK(controller);
  // Because ShowModalDialog does not return the web contents on Android, we
  // need to set up the IdentityRegistry now.
  WebContents* rp_web_contents = controller->GetRpWebContents();
  // This can be null if resolve was called in a regular tab (as opposed to
  // a CCT opened from ShowModalDialog).
  if (!rp_web_contents) {
    return false;
  }
  Request* rp_request = GetPageData(rp_web_contents->GetPrimaryPage())
                            ->PendingWebIdentityRequest();
  if (!rp_request) {
    return false;
  }
  WebContents* web_contents =
      WebContents::FromRenderFrameHost(&render_frame_host());
  IdentityRegistry::CreateForWebContents(
      web_contents, rp_request->weak_ptr_factory_.GetWeakPtr(),
      rp_request->config_url_);
  return true;
#else
  return false;
#endif
}

void RequestService::RequestUserInfo(
    blink::mojom::IdentityProviderConfigPtr provider,
    RequestUserInfoCallback callback) {
  // Enforce identity-credentials-get Permissions Policy browser-side.
  // The renderer checks this, but a compromised renderer can bypass it.
  if (!render_frame_host().IsFeatureEnabled(
          network::mojom::PermissionsPolicyFeature::kIdentityCredentialsGet)) {
    receivers_.ReportBadMessage(
        "identity-credentials-get permissions policy not enabled");
    return;
  }

  if (!render_frame_host().GetPage().IsPrimary()) {
    receivers_.ReportBadMessage(
        "FedCM should not be allowed in nested frame trees.");
    return;
  }
  // FedCmMetrics class is currently not used for UserInfo API. If we log UKM
  // metrics later on, we should call CreateFedCmMetrics() here.

  auto user_info_request = std::make_unique<UserInfoRequest>(
      CreateNetworkManager(), permission_delegate_, api_permission_delegate_,
      &render_frame_host(), std::move(provider));
  UserInfoRequest* user_info_request_ptr = user_info_request.get();
  user_info_requests_.insert(std::move(user_info_request));

  user_info_request_ptr->SetCallbackAndStart(base::BindOnce(
      &RequestService::CompleteUserInfoRequest, weak_ptr_factory_.GetWeakPtr(),
      user_info_request_ptr, std::move(callback)));
}

void RequestService::CompleteUserInfoRequest(
    UserInfoRequest* request,
    RequestUserInfoCallback callback,
    blink::mojom::RequestUserInfoResultPtr result) {
  auto it = user_info_requests_.find(request);
  // The request may not be found if the completion is invoked from the
  // RequestService destructor. The destructor clears `user_info_requests_`,
  // which destroys the UserInfoRequests it contains. The
  // UserInfoRequest destructor invokes this callback.
  if (it == user_info_requests_.end() && result->is_user_info()) {
    NOTREACHED() << "The successful user info request is nowhere to be found";
  }
  // Extract the request from the set first to prevent UAF if the callback
  // synchronously destroys `this` (RequestService).
  std::unique_ptr<UserInfoRequest> holder;
  if (it != user_info_requests_.end()) {
    holder = std::move(const_cast<std::unique_ptr<UserInfoRequest>&>(*it));
    user_info_requests_.erase(it);
  }

  std::move(callback).Run(std::move(result));
}

void RequestService::Disconnect(
    blink::mojom::IdentityCredentialDisconnectOptionsPtr options,
    DisconnectCallback callback) {
  // Enforce identity-credentials-get Permissions Policy browser-side.
  // The renderer checks this, but a compromised renderer can bypass it.
  if (!render_frame_host().IsFeatureEnabled(
          network::mojom::PermissionsPolicyFeature::kIdentityCredentialsGet)) {
    receivers_.ReportBadMessage(
        "identity-credentials-get permissions policy not enabled");
    return;
  }

  std::unique_ptr<Metrics> disconnect_metrics = CreateFedCmMetrics();
  if (disconnect_request_) {
    // Since we do not send any fetches in this case, consider the request to be
    // instant, e.g. duration is 0.
    render_frame_host().AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError,
        GetDisconnectConsoleErrorMessage(DisconnectStatus::kTooManyRequests));
    disconnect_metrics->RecordDisconnectMetrics(
        DisconnectStatus::kTooManyRequests, std::nullopt,
        ComputeRequesterFrameType(
            render_frame_host(), render_frame_host().GetLastCommittedOrigin(),
            render_frame_host().GetMainFrame()->GetLastCommittedOrigin()),
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

void RequestService::CompleteDisconnectRequest(
    DisconnectCallback callback,
    blink::mojom::DisconnectStatus status) {
  CHECK(disconnect_request_);
  std::move(callback).Run(status);
  disconnect_request_.reset();
}

std::unique_ptr<Metrics> RequestService::CreateFedCmMetrics() {
  // Ensure the lifecycle state as GetPageUkmSourceId doesn't support the
  // prerendering page. As FederatedRequestService runs behind the
  // BrowserInterfaceBinders, the service doesn't receive any request while
  // prerendering, and the CHECK should always meet the condition.
  CHECK(!render_frame_host().IsInLifecycleState(
      RenderFrameHost::LifecycleState::kPrerendering));
  return std::make_unique<Metrics>(render_frame_host().GetPageUkmSourceId());
}

void RequestService::ResolveTokenRequest(
    const std::optional<std::string>& account_id,
    blink::mojom::ResolveTokenParamsPtr params,
    ResolveTokenRequestCallback callback) {
  if (params->is_redirect_to()) {
    const blink::mojom::RedirectParamsPtr& redirect_to =
        params->get_redirect_to();
    const GURL& redirect_url = redirect_to->is_get()
                                   ? redirect_to->get_get()->url
                                   : redirect_to->get_post()->url;
    if (!redirect_url.is_valid()) {
      receivers_.ReportBadMessage("Invalid redirect URL");
      return;
    }
    if (redirect_to->is_post() &&
        redirect_to->get_post()->request_body.empty()) {
      receivers_.ReportBadMessage("POST redirects must have a body");
      return;
    }
  }

  if (!GetIdentityRegistry() && !SetupIdentityRegistryFromPopup()) {
    std::move(callback).Run(false);
    return;
  }

  IdentityRegistry* registry = GetIdentityRegistry();
  CHECK(registry);
  bool accepted =
      registry->NotifyResolve(render_frame_host().GetLastCommittedOrigin(),
                              account_id, std::move(params));
  std::move(callback).Run(accepted);
}

IdentityRequestDialogController* RequestService::GetOrCreateDialogController() {
  if (mock_dialog_controller_) {
    return mock_dialog_controller_.get();
  }
  if (!dialog_controller_) {
    dialog_controller_ = CreateDialogController();
  }
  return dialog_controller_.get();
}

IdentityRequestDialogController* RequestService::GetDialogController() const {
  if (mock_dialog_controller_) {
    return mock_dialog_controller_.get();
  }
  return dialog_controller_.get();
}

std::unique_ptr<IdentityRequestDialogController>
RequestService::CreateDialogController() {
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

void RequestService::SetDialogControllerForTests(
    std::unique_ptr<IdentityRequestDialogController> controller) {
  mock_dialog_controller_ = std::move(controller);
}

void RequestService::SetIdpSigninStatus(
    const url::Origin& idp_origin,
    blink::mojom::IdpSigninStatus status,
    const std::optional<blink::common::webid::LoginStatusOptions>& options,
    SetIdpSigninStatusCallback callback) {
  auto scoped_closure = base::ScopedClosureRunner(std::move(callback));

  if (render_frame_host().IsNestedWithinFencedFrame()) {
    return;
  }
  // We only allow setting the IDP signin status when the subresource is loaded
  // from the same site as the document, and the document is same site with
  // all ancestors. This is to protect from an RP embedding a tracker resource
  // that would set this signin status for the tracker, enabling the FedCM
  // request.
  if (!IsSameSiteWithAncestors(idp_origin, &render_frame_host())) {
    return;
  }

  if (!IsLightweightModeEnabled()) {
    permission_delegate_->SetIdpSigninStatus(
        idp_origin, status == blink::mojom::IdpSigninStatus::kSignedIn,
        /*options=*/std::nullopt);
  } else {
    if (options.has_value()) {
      std::vector<GURL> picture_urls;
      for (const blink::common::webid::LoginStatusAccount& account :
           options->accounts) {
        if (account.picture.has_value()) {
          // Guaranteed by Mojo deserialization traits (StructTraits::Read in
          // federated_request_mojom_traits.cc).
          DCHECK(account.picture->is_valid());
          DCHECK(network::IsUrlPotentiallyTrustworthy(account.picture.value()));
          picture_urls.emplace_back(account.picture.value());
        }
      }
      if (!signin_status_network_manager_) {
        signin_status_network_manager_ = CreateNetworkManager();
      }
      signin_status_network_manager_->CacheAccountPictures(idp_origin,
                                                           picture_urls);
    }
    permission_delegate_->SetIdpSigninStatus(
        idp_origin, status == blink::mojom::IdpSigninStatus::kSignedIn,
        options);
  }
}

IdentityRegistry* RequestService::GetIdentityRegistry() {
  if (mock_identity_registry_) {
    return mock_identity_registry_;
  }
  return IdentityRegistry::FromWebContents(
      WebContents::FromRenderFrameHost(&render_frame_host()));
}

}  // namespace content::webid
