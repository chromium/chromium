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
#include "content/browser/webid/idp_network_request_manager.h"
#include "content/common/content_export.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "url/gurl.h"

namespace content {

class FederatedIdentityActiveSessionPermissionContextDelegate;
class FederatedIdentityRequestPermissionContextDelegate;
class FederatedIdentitySharingPermissionContextDelegate;
class RenderFrameHost;

// FederatedAuthRequestImpl contains the state machines for executing federated
// authentication requests. This can be owned either by a
// FederatedAuthRequestService, when the invocation is done from the renderer
// via a mojo call, or by a FederatedAuthNavigationThrottle, when the
// invocation is from an intercepted HTTP request.
class CONTENT_EXPORT FederatedAuthRequestImpl {
 public:
  FederatedAuthRequestImpl(RenderFrameHost* host, const url::Origin& origin);

  FederatedAuthRequestImpl(const FederatedAuthRequestImpl&) = delete;
  FederatedAuthRequestImpl& operator=(const FederatedAuthRequestImpl&) = delete;

  ~FederatedAuthRequestImpl();

  void RequestIdToken(
      const GURL& provider,
      const std::string& client_id,
      const std::string& nonce,
      blink::mojom::RequestMode mode,
      bool prefer_auto_sign_in,
      blink::mojom::FederatedAuthRequest::RequestIdTokenCallback);
  void Revoke(const GURL& provider,
              const std::string& client_id,
              const std::string& account_id,
              blink::mojom::FederatedAuthRequest::RevokeCallback);
  void Logout(std::vector<blink::mojom::LogoutRequestPtr> logout_requests,
              blink::mojom::FederatedAuthRequest::LogoutCallback);

  void SetNetworkManagerForTests(
      std::unique_ptr<IdpNetworkRequestManager> manager);
  void SetDialogControllerForTests(
      std::unique_ptr<IdentityRequestDialogController> controller);
  void SetActiveSessionPermissionDelegateForTests(
      FederatedIdentityActiveSessionPermissionContextDelegate*);
  void SetRequestPermissionDelegateForTests(
      FederatedIdentityRequestPermissionContextDelegate*);
  void SetSharingPermissionDelegateForTests(
      FederatedIdentitySharingPermissionContextDelegate*);

 private:
  bool HasPendingRequest() const;
  GURL ResolveWellKnownUrl(const std::string& url);
  void OnWellKnownFetched(IdpNetworkRequestManager::FetchStatus status,
                          IdpNetworkRequestManager::Endpoints);
  void OnClientIdMetadataResponseReceived(
      IdpNetworkRequestManager::FetchStatus status,
      IdpNetworkRequestManager::ClientIdMetadata data);

  void OnSigninApproved(IdentityRequestDialogController::UserApproval approval);
  void OnSigninResponseReceived(IdpNetworkRequestManager::SigninResponse status,
                                const std::string& url_or_token);
  void OnTokenProvided(const std::string& id_token);
  void OnIdpPageClosed();
  void OnTokenProvisionApproved(
      IdentityRequestDialogController::UserApproval approval);
  void OnAccountsResponseReceived(
      IdpNetworkRequestManager::AccountsResponse status,
      IdpNetworkRequestManager::AccountList accounts,
      content::IdentityProviderMetadata idp_metadata);
  void OnAccountSelected(const std::string& account_id);
  void OnTokenResponseReceived(IdpNetworkRequestManager::TokenResponse status,
                               const std::string& id_token);
  void DispatchOneLogout();
  void OnLogoutCompleted();
  std::unique_ptr<WebContents> CreateIdpWebContents();
  void CompleteRequest(blink::mojom::RequestIdTokenStatus,
                       const std::string& id_token);
  void CompleteLogoutRequest(blink::mojom::LogoutStatus);
  void OnWellKnownFetchedForRevoke(IdpNetworkRequestManager::FetchStatus status,
                                   IdpNetworkRequestManager::Endpoints);
  void OnRevokeResponse(IdpNetworkRequestManager::RevokeResponse response);
  void CompleteRevokeRequest(blink::mojom::RevokeStatus status);

  std::unique_ptr<IdpNetworkRequestManager> CreateNetworkManager(
      const GURL& provider);
  std::unique_ptr<IdentityRequestDialogController> CreateDialogController();

  FederatedIdentityActiveSessionPermissionContextDelegate*
  GetActiveSessionPermissionContext();
  FederatedIdentityRequestPermissionContextDelegate*
  GetRequestPermissionContext();
  FederatedIdentitySharingPermissionContextDelegate*
  GetSharingPermissionContext();

  const raw_ptr<RenderFrameHost> render_frame_host_ = nullptr;
  const url::Origin origin_;

  std::unique_ptr<IdpNetworkRequestManager> network_manager_;
  std::unique_ptr<IdentityRequestDialogController> request_dialog_controller_;

  // Replacements for testing.
  std::unique_ptr<IdpNetworkRequestManager> mock_network_manager_;
  std::unique_ptr<IdentityRequestDialogController> mock_dialog_controller_;

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

  blink::mojom::RequestMode mode_;

  bool prefer_auto_sign_in_;

  // Fetched from the IDP well-known configuration.
  struct {
    GURL idp;
    GURL token;
    GURL accounts;
    GURL client_id_metadata;
  } endpoints_;

  // The WebContents that is used to load the IDP sign-up page. This is
  // created here to allow us to setup proper callbacks on it using
  // |IdTokenRequestCallbackData|. It is then passed along to
  // chrome/browser/ui machinery to be used to load IDP sign-in content.
  std::unique_ptr<WebContents> idp_web_contents_;

  raw_ptr<FederatedIdentityActiveSessionPermissionContextDelegate>
      active_session_permission_delegate_ = nullptr;
  raw_ptr<FederatedIdentityRequestPermissionContextDelegate>
      request_permission_delegate_ = nullptr;
  raw_ptr<FederatedIdentitySharingPermissionContextDelegate>
      sharing_permission_delegate_ = nullptr;

  IdpNetworkRequestManager::ClientIdMetadata client_id_metadata_;
  // The account that was selected by the user. This is only applicable to the
  // mediation flow.
  std::string account_id_;
  std::string id_token_;
  blink::mojom::FederatedAuthRequest::RequestIdTokenCallback
      auth_request_callback_;

  base::queue<blink::mojom::LogoutRequestPtr> logout_requests_;
  blink::mojom::FederatedAuthRequest::LogoutCallback logout_callback_;

  blink::mojom::FederatedAuthRequest::RevokeCallback revoke_callback_;

  base::WeakPtrFactory<FederatedAuthRequestImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_FEDERATED_AUTH_REQUEST_IMPL_H_
