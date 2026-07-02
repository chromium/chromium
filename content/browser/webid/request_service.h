// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_REQUEST_SERVICE_H_
#define CONTENT_BROWSER_WEBID_REQUEST_SERVICE_H_

#include <memory>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/webid/config_fetcher.h"
#include "content/common/content_export.h"
#include "content/public/browser/document_user_data.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"

namespace content {

class RenderFrameHost;
class FederatedIdentityApiPermissionContextDelegate;
class FederatedIdentityAutoReauthnPermissionContextDelegate;
class FederatedIdentityPermissionContextDelegate;
class IdentityRequestDialogController;

namespace webid {

class Request;
class IdentityRegistry;
class IdpRegistrationHandler;
class IdpNetworkRequestManager;
class UserInfoRequest;
class DisconnectRequest;

using ResolveTokenRequestCallback =
    blink::mojom::FederatedRequestService::ResolveTokenRequestCallback;
using SetIdpSigninStatusCallback =
    blink::mojom::FederatedRequestService::SetIdpSigninStatusCallback;
using RegisterIdPCallback =
    blink::mojom::FederatedRequestService::RegisterIdPCallback;
using UnregisterIdPCallback =
    blink::mojom::FederatedRequestService::UnregisterIdPCallback;
using PreventSilentAccessCallback =
    blink::mojom::FederatedRequestService::PreventSilentAccessCallback;
using DisconnectCallback =
    blink::mojom::FederatedRequestService::DisconnectCallback;
using RequestUserInfoCallback =
    blink::mojom::FederatedRequestService::RequestUserInfoCallback;
using StartTokenRequestCallback =
    blink::mojom::FederatedRequestService::StartTokenRequestCallback;
using MediationRequirement = ::password_manager::CredentialMediationRequirement;

// RequestService is a document-scoped manager class that coordinates
// Federated Credential Management (FedCM) requests for a given RenderFrameHost.
// It owns the active Request session.
class CONTENT_EXPORT RequestService
    : public DocumentUserData<RequestService>,
      public blink::mojom::FederatedRequestService {
 public:
  DOCUMENT_USER_DATA_KEY_DECL();

  explicit RequestService(RenderFrameHost* rfh);
  ~RequestService() override;

  RequestService(const RequestService&) = delete;
  RequestService& operator=(const RequestService&) = delete;

  void SetForceAllowRedirectToForTesting(bool allow) {
    force_allow_redirect_to_for_testing_ = allow;
  }
  bool force_allow_redirect_to_for_testing() const {
    return force_allow_redirect_to_for_testing_;
  }

  // Binds a new receiver to a request session.
  void BindFederatedAuthRequest(
      mojo::PendingReceiver<blink::mojom::FederatedAuthRequest> receiver);

  void BindFederatedRequestService(
      mojo::PendingReceiver<blink::mojom::FederatedRequestService> receiver);

  // blink::mojom::FederatedRequestService:
  void StartTokenRequest(
      std::vector<blink::mojom::IdentityProviderGetParametersPtr>
          idp_get_params,
      MediationRequirement requirement,
      mojo::PendingReceiver<blink::mojom::FederatedRequest> request_receiver,
      StartTokenRequestCallback callback) override;
  void RequestUserInfo(blink::mojom::IdentityProviderConfigPtr provider,
                       RequestUserInfoCallback callback) override;
  void RegisterIdP(const GURL& idp, RegisterIdPCallback callback) override;
  void UnregisterIdP(const GURL& idp, UnregisterIdPCallback callback) override;
  void PreventSilentAccess(PreventSilentAccessCallback callback) override;
  void Disconnect(blink::mojom::IdentityCredentialDisconnectOptionsPtr options,
                  DisconnectCallback callback) override;
  void ResolveTokenRequest(const std::optional<std::string>& account_id,
                           blink::mojom::ResolveTokenParamsPtr params,
                           ResolveTokenRequestCallback callback) override;
  void SetIdpSigninStatus(
      const url::Origin& idp_origin,
      blink::mojom::IdpSigninStatus status,
      const std::optional<::blink::common::webid::LoginStatusOptions>& options,
      SetIdpSigninStatusCallback callback) override;

  Request* GetActiveRequestForTesting() { return active_request_.get(); }

  base::WeakPtr<RequestService> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void SetNetworkManagerForTests(
      std::unique_ptr<IdpNetworkRequestManager> manager);
  std::unique_ptr<IdpNetworkRequestManager> CreateNetworkManager();
  void CloseModalDialogView() override;
  IdentityRequestDialogController* GetOrCreateDialogController();
  IdentityRequestDialogController* GetDialogController() const;
  void SetDialogControllerForTests(
      std::unique_ptr<IdentityRequestDialogController> controller);

  // Returns the active Request if one exists, or instantiates a new one if not.
  Request* GetOrCreateActiveRequest();
  Request* GetActiveRequestForTesting() const;

  void SetDelegatesForTesting(
      FederatedIdentityApiPermissionContextDelegate* api_permission_delegate,
      FederatedIdentityAutoReauthnPermissionContextDelegate*
          auto_reauthn_permission_delegate,
      FederatedIdentityPermissionContextDelegate* permission_delegate,
      IdentityRegistry* identity_registry);

  // Destroys the active request. Strictly for use in tests.
  void DestroyActiveRequestForTesting();

  void IncrementNumRequests() { ++num_requests_; }

 private:
  friend class DocumentUserData<RequestService>;
  friend class Request;
  friend class RequestTest;
  friend class RequestRegistryTest;

  bool SetupIdentityRegistryFromPopup();
  void SetRequiresUserMediation(bool requires_user_mediation,
                                base::OnceClosure callback);
  void OnIdpRegistrationConfigFetched(
      RegisterIdPCallback callback,
      const GURL& idp,
      std::vector<ConfigFetcher::FetchResult> fetch_results);
  void CompleteUserInfoRequest(UserInfoRequest* request,
                               RequestUserInfoCallback callback,
                               blink::mojom::RequestUserInfoResultPtr result);
  void CompleteDisconnectRequest(DisconnectCallback callback,
                                 blink::mojom::DisconnectStatus status);
  void OnTokenRequestComplete(
      Request* request,
      StartTokenRequestCallback callback,
      blink::mojom::RequestTokenStatus status,
      const std::optional<GURL>& selected_idp_config_url,
      std::optional<base::Value> token,
      blink::mojom::TokenErrorPtr error,
      bool is_auto_selected);
  void CleanUpCompletedRequest(Request* request);
  std::unique_ptr<Metrics> CreateFedCmMetrics();
  std::unique_ptr<IdentityRequestDialogController> CreateDialogController();
  void MaybeDestroyDialogController();

  std::unique_ptr<Request> active_request_;
  // Temporary storage for completed requests pending destruction.
  std::vector<std::unique_ptr<Request>> completed_requests_;

  // Number of navigator.credentials.get() requests made for metrics purposes.
  // Requests made when there is a pending FedCM request or for the purpose of
  // Wallets or multi-IDP are not counted.
  int num_requests_{0};

  bool force_allow_redirect_to_for_testing_ = false;

  raw_ptr<IdentityRegistry> identity_registry_ = nullptr;

  mojo::ReceiverSet<blink::mojom::FederatedRequestService> receivers_;

  std::unique_ptr<IdpNetworkRequestManager> registration_network_manager_;
  std::unique_ptr<IdpRegistrationHandler> fedcm_idp_registration_handler_;
  std::unique_ptr<IdpNetworkRequestManager> mock_network_manager_;
  std::unique_ptr<IdpNetworkRequestManager> signin_status_network_manager_;
  base::flat_set<std::unique_ptr<UserInfoRequest>, base::UniquePtrComparator>
      user_info_requests_;

  raw_ptr<FederatedIdentityApiPermissionContextDelegate>
      api_permission_delegate_ = nullptr;
  raw_ptr<FederatedIdentityAutoReauthnPermissionContextDelegate>
      auto_reauthn_permission_delegate_ = nullptr;
  raw_ptr<FederatedIdentityPermissionContextDelegate> permission_delegate_ =
      nullptr;
  std::unique_ptr<DisconnectRequest> disconnect_request_;

  std::unique_ptr<IdentityRequestDialogController> dialog_controller_;
  std::unique_ptr<IdentityRequestDialogController> mock_dialog_controller_;

  base::WeakPtrFactory<RequestService> weak_ptr_factory_{this};
};

}  // namespace webid
}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_REQUEST_SERVICE_H_
