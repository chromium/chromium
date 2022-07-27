// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_FEDERATED_AUTH_REQUEST_IMPL_H_
#define CONTENT_BROWSER_WEBID_FEDERATED_AUTH_REQUEST_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/queue.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "content/browser/webid/fedcm_metrics.h"
#include "content/browser/webid/idp_network_request_manager.h"
#include "content/common/content_export.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "url/gurl.h"

namespace content {

class FederatedIdentityActiveSessionPermissionContextDelegate;
class FederatedIdentityApiPermissionContextDelegate;
class FederatedIdentitySharingPermissionContextDelegate;
class RenderFrameHost;

// FederatedAuthRequestImpl handles mojo connections from the renderer to
// fulfill WebID-related requests.
//
// In practice, it is owned and managed by a RenderFrameHost. It accomplishes
// that via subclassing DocumentService, which observes the lifecycle of a
// RenderFrameHost and manages its own memory.
// Create() creates a self-managed instance of FederatedAuthRequestImpl and
// binds it to the receiver.
class CONTENT_EXPORT FederatedAuthRequestImpl
    : public DocumentService<blink::mojom::FederatedAuthRequest> {
 public:
  static void Create(RenderFrameHost*,
                     mojo::PendingReceiver<blink::mojom::FederatedAuthRequest>);
  static FederatedAuthRequestImpl& CreateForTesting(
      RenderFrameHost&,
      mojo::PendingReceiver<blink::mojom::FederatedAuthRequest>);

  FederatedAuthRequestImpl(const FederatedAuthRequestImpl&) = delete;
  FederatedAuthRequestImpl& operator=(const FederatedAuthRequestImpl&) = delete;

  ~FederatedAuthRequestImpl() override;

  // blink::mojom::FederatedAuthRequest:
  void RequestToken(const GURL& provider,
                    const std::string& client_id,
                    const std::string& nonce,
                    bool prefer_auto_sign_in,
                    RequestTokenCallback) override;
  void CancelTokenRequest() override;
  void LogoutRps(std::vector<blink::mojom::LogoutRpsRequestPtr> logout_requests,
                 LogoutRpsCallback) override;

  void SetTokenRequestDelayForTests(base::TimeDelta delay);
  void SetNetworkManagerForTests(
      std::unique_ptr<IdpNetworkRequestManager> manager);
  void SetDialogControllerForTests(
      std::unique_ptr<IdentityRequestDialogController> controller);
  void SetActiveSessionPermissionDelegateForTests(
      FederatedIdentityActiveSessionPermissionContextDelegate*);
  void SetSharingPermissionDelegateForTests(
      FederatedIdentitySharingPermissionContextDelegate*);
  void SetApiPermissionDelegateForTests(
      FederatedIdentityApiPermissionContextDelegate*);

  // Rejects the pending request if it has not been resolved naturally yet.
  void OnRejectRequest();

 private:
  friend class FederatedAuthRequestImplTest;

  FederatedAuthRequestImpl(
      RenderFrameHost&,
      mojo::PendingReceiver<blink::mojom::FederatedAuthRequest>);

  bool HasPendingRequest() const;
  GURL ResolveManifestUrl(const std::string& url);

  // Checks validity of the passed-in endpoint URL origin.
  bool IsEndpointUrlValid(const GURL& endpoint_url);

  void FetchManifest();
  void OnManifestListFetched(IdpNetworkRequestManager::FetchStatus status,
                             const std::set<GURL>& urls);
  void OnManifestFetched(IdpNetworkRequestManager::FetchStatus status,
                         IdpNetworkRequestManager::Endpoints,
                         IdentityProviderMetadata idp_metadata);
  void OnManifestReady(IdentityProviderMetadata idp_metadata);
  void OnClientMetadataResponseReceived(
      IdentityProviderMetadata idp_metadata,
      IdpNetworkRequestManager::FetchStatus status,
      IdpNetworkRequestManager::ClientMetadata data);

  void OnAccountsResponseReceived(
      IdentityProviderMetadata idp_metadata,
      IdpNetworkRequestManager::FetchStatus status,
      IdpNetworkRequestManager::AccountList accounts);
  void OnAccountSelected(const std::string& account_id, bool is_sign_in);
  void OnDialogDismissed(
      IdentityRequestDialogController::DismissReason dismiss_reason);
  void CompleteTokenRequest(IdpNetworkRequestManager::FetchStatus status,
                            const std::string& token);
  void OnTokenResponseReceived(IdpNetworkRequestManager::FetchStatus status,
                               const std::string& token);
  void DispatchOneLogout();
  void OnLogoutCompleted();
  void CompleteRequest(blink::mojom::FederatedAuthRequestResult,
                       const std::string& token,
                       bool should_call_callback);
  void CompleteLogoutRequest(blink::mojom::LogoutRpsStatus);

  void CleanUp();

  std::unique_ptr<IdpNetworkRequestManager> CreateNetworkManager(
      const GURL& provider);
  std::unique_ptr<IdentityRequestDialogController> CreateDialogController();

  FederatedIdentityActiveSessionPermissionContextDelegate*
  GetActiveSessionPermissionContext();
  FederatedIdentityApiPermissionContextDelegate* GetApiPermissionContext();
  FederatedIdentitySharingPermissionContextDelegate*
  GetSharingPermissionContext();

  // Creates an inspector issue related to a federated authentication request to
  // the Issues panel in DevTools.
  void AddInspectorIssue(blink::mojom::FederatedAuthRequestResult result);

  // Adds a console error message related to a federated authentication request
  // issue. The Issues panel is preferred, but for now we also surface console
  // error messages since it is much simpler to add.
  // TODO(crbug.com/1294415): When the FedCM API is more stable, we should
  // ensure that the Issues panel contains all of the needed debugging
  // information and then we can remove the console error messages.
  void AddConsoleErrorMessage(blink::mojom::FederatedAuthRequestResult result);

  bool ShouldCompleteRequestImmediately();

  std::unique_ptr<IdpNetworkRequestManager> network_manager_;
  std::unique_ptr<IdentityRequestDialogController> request_dialog_controller_;

  // Replacements for testing.
  std::unique_ptr<IdpNetworkRequestManager> mock_network_manager_;
  std::unique_ptr<IdentityRequestDialogController> mock_dialog_controller_;

  // Helper that records FedCM UMA and UKM metrics. Initialized in the
  // RequestToken() method, so all metrics must be recorded after that.
  std::unique_ptr<FedCmMetrics> fedcm_metrics_;

  // Parameters of auth request.
  GURL provider_;

  // The federated auth request parameters provided by RP. Note that these
  // parameters will uniquely identify the users so they should only be passed
  // to IDP after user permission has been granted.
  //
  // TODO(majidvp): Implement a mechanism (e.g., a getter) that checks the
  // request permission is granted before providing access to this parameter
  // this way we avoid accidentally sharing these values.
  std::string client_id_;
  std::string nonce_;

  bool prefer_auto_sign_in_;

  // Fetched from the IDP FedCM manifest configuration.
  struct {
    GURL idp;
    GURL token;
    GURL accounts;
    GURL client_metadata;
  } endpoints_;

  // Represents whether the manifest has been validated via checking the
  // manifest list.
  bool manifest_list_checked_ = false;
  absl::optional<IdentityProviderMetadata> idp_metadata_;

  raw_ptr<FederatedIdentityActiveSessionPermissionContextDelegate>
      active_session_permission_delegate_ = nullptr;
  raw_ptr<FederatedIdentityApiPermissionContextDelegate>
      api_permission_delegate_ = nullptr;
  raw_ptr<FederatedIdentitySharingPermissionContextDelegate>
      sharing_permission_delegate_ = nullptr;

  IdpNetworkRequestManager::ClientMetadata client_metadata_;
  // The account that was selected by the user. This is only applicable to the
  // mediation flow.
  std::string account_id_;
  base::TimeTicks start_time_;
  base::TimeTicks show_accounts_dialog_time_;
  base::TimeTicks select_account_time_;
  base::TimeTicks token_response_time_;
  base::TimeDelta token_request_delay_;
  bool errors_logged_to_console_{false};
  RequestTokenCallback auth_request_callback_;

  base::queue<blink::mojom::LogoutRpsRequestPtr> logout_requests_;
  LogoutRpsCallback logout_callback_;

  base::WeakPtrFactory<FederatedAuthRequestImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_FEDERATED_AUTH_REQUEST_IMPL_H_
