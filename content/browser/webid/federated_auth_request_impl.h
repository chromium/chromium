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
#include "base/timer/timer.h"
#include "content/browser/webid/idp_network_request_manager.h"
#include "content/common/content_export.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "url/gurl.h"

namespace content {

class FederatedIdentityActiveSessionPermissionContextDelegate;
class FederatedIdentityApiPermissionContextDelegate;
class FederatedIdentityRequestPermissionContextDelegate;
class FederatedIdentitySharingPermissionContextDelegate;
class RenderFrameHostImpl;

// FederatedAuthRequestImpl contains the state machines for executing federated
// authentication requests. This can be owned either by a
// FederatedAuthRequestService, when the invocation is done from the renderer
// via a mojo call, or by a FederatedAuthNavigationThrottle, when the
// invocation is from an intercepted HTTP request.
class CONTENT_EXPORT FederatedAuthRequestImpl {
 public:
  FederatedAuthRequestImpl(RenderFrameHostImpl* host,
                           const url::Origin& origin);

  FederatedAuthRequestImpl(const FederatedAuthRequestImpl&) = delete;
  FederatedAuthRequestImpl& operator=(const FederatedAuthRequestImpl&) = delete;

  ~FederatedAuthRequestImpl();

  void RequestIdToken(
      const GURL& provider,
      const std::string& client_id,
      const std::string& nonce,
      bool prefer_auto_sign_in,
      blink::mojom::FederatedAuthRequest::RequestIdTokenCallback);
  void CancelTokenRequest();
  void Revoke(const GURL& provider,
              const std::string& client_id,
              const std::string& account_id,
              blink::mojom::FederatedAuthRequest::RevokeCallback);
  void Logout(const GURL& provider,
              const std::string& account_id,
              blink::mojom::FederatedAuthRequest::LogoutCallback callback);
  void LogoutRps(std::vector<blink::mojom::LogoutRpsRequestPtr> logout_requests,
                 blink::mojom::FederatedAuthRequest::LogoutRpsCallback);

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
  void SetApiPermissionDelegateForTests(
      FederatedIdentityApiPermissionContextDelegate*);

  // Rejects the pending request if it has not been resolved naturally yet.
  void OnRejectRequest();

 private:
  bool HasPendingRequest() const;
  GURL ResolveManifestUrl(const std::string& url);

  // Checks validity of the passed-in endpoint URL origin.
  bool IsEndpointUrlValid(const GURL& endpoint_url);

  void FetchManifest(IdpNetworkRequestManager::FetchManifestCallback callback);
  void OnManifestFetched(IdpNetworkRequestManager::FetchStatus status,
                         IdpNetworkRequestManager::Endpoints,
                         IdentityProviderMetadata idp_metadata);
  void OnBrandIconDownloaded(int icon_minimum_size,
                             IdentityProviderMetadata idp_metadata,
                             int id,
                             int http_status_code,
                             const GURL& image_url,
                             const std::vector<SkBitmap>& bitmaps,
                             const std::vector<gfx::Size>& sizes);
  void OnClientMetadataResponseReceived(
      IdentityProviderMetadata idp_metadata,
      IdpNetworkRequestManager::FetchStatus status,
      IdpNetworkRequestManager::ClientMetadata data);

  void DownloadBitmap(const GURL& icon_url,
                      int ideal_icon_size,
                      WebContents::ImageDownloadCallback callback);
  void OnAccountsResponseReceived(
      IdentityProviderMetadata idp_metadata,
      IdpNetworkRequestManager::FetchStatus status,
      IdpNetworkRequestManager::AccountList accounts);
  void OnAccountSelected(const std::string& account_id, bool is_sign_in);
  void CompleteIdTokenRequest(IdpNetworkRequestManager::FetchStatus status,
                              const std::string& id_token);
  void OnTokenResponseReceived(IdpNetworkRequestManager::FetchStatus status,
                               const std::string& id_token);
  void DispatchOneLogout();
  void OnLogoutCompleted();
  void CompleteRequest(blink::mojom::FederatedAuthRequestResult,
                       const std::string& id_token,
                       bool should_call_callback);
  void CompleteLogoutRequest(blink::mojom::LogoutRpsStatus);
  void OnManifestFetchedForRevoke(IdpNetworkRequestManager::FetchStatus status,
                                  IdpNetworkRequestManager::Endpoints,
                                  IdentityProviderMetadata idp_metadata);
  void OnRevokeResponse(IdpNetworkRequestManager::RevokeResponse response);
  // |should_call_callback| represents whether we should call the callback to
  // either resolve or reject the promise immediately when the renderer receives
  // the IPC from the browser. For some failures we choose to reject with
  // |delay_timer_| for privacy reasons.
  void CompleteRevokeRequest(blink::mojom::RevokeStatus status,
                             bool should_call_callback);

  void CleanUp();

  std::unique_ptr<IdpNetworkRequestManager> CreateNetworkManager(
      const GURL& provider);
  std::unique_ptr<IdentityRequestDialogController> CreateDialogController();

  FederatedIdentityActiveSessionPermissionContextDelegate*
  GetActiveSessionPermissionContext();
  FederatedIdentityApiPermissionContextDelegate* GetApiPermissionContext();
  FederatedIdentityRequestPermissionContextDelegate*
  GetRequestPermissionContext();
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

  const raw_ptr<RenderFrameHostImpl> render_frame_host_ = nullptr;
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

  bool prefer_auto_sign_in_;

  // Fetched from the IDP FedCM manifest configuration.
  struct {
    GURL idp;
    GURL token;
    GURL accounts;
    GURL client_metadata;
  } endpoints_;

  raw_ptr<FederatedIdentityActiveSessionPermissionContextDelegate>
      active_session_permission_delegate_ = nullptr;
  raw_ptr<FederatedIdentityApiPermissionContextDelegate>
      api_permission_delegate_ = nullptr;
  raw_ptr<FederatedIdentityRequestPermissionContextDelegate>
      request_permission_delegate_ = nullptr;
  raw_ptr<FederatedIdentitySharingPermissionContextDelegate>
      sharing_permission_delegate_ = nullptr;

  IdpNetworkRequestManager::ClientMetadata client_metadata_;
  // The account that was selected by the user. This is only applicable to the
  // mediation flow.
  std::string account_id_;
  // Used by revocation.
  std::string hint_;
  base::TimeTicks start_time_;
  base::TimeTicks show_accounts_dialog_time_;
  base::TimeTicks select_account_time_;
  base::TimeTicks id_token_response_time_;
  base::DelayTimer delay_timer_;
  blink::mojom::FederatedAuthRequest::RequestIdTokenCallback
      auth_request_callback_;

  base::queue<blink::mojom::LogoutRpsRequestPtr> logout_requests_;
  blink::mojom::FederatedAuthRequest::LogoutRpsCallback logout_callback_;

  blink::mojom::FederatedAuthRequest::RevokeCallback revoke_callback_;

  base::WeakPtrFactory<FederatedAuthRequestImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_FEDERATED_AUTH_REQUEST_IMPL_H_
