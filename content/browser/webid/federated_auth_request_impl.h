// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_FEDERATED_AUTH_REQUEST_IMPL_H_
#define CONTENT_BROWSER_WEBID_FEDERATED_AUTH_REQUEST_IMPL_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "content/browser/webid/idp_network_request_manager.h"
#include "content/common/content_export.h"
#include "content/public/browser/frame_service_base.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "url/gurl.h"

namespace content {

class RenderFrameHost;

// FederatedAuthRequestImpl handles mojo connections from the renderer to
// fulfill WebID-related requests.
//
// In practice, it is owned and managed by a RenderFrameHost. It accomplishes
// that via subclassing FrameServiceBase, which observes the lifecycle of a
// RenderFrameHost and manages its own memory.
// Create() creates a self-managed instance of FederatedAuthRequestImpl and
// binds it to the receiver.
class CONTENT_EXPORT FederatedAuthRequestImpl
    : public FrameServiceBase<blink::mojom::FederatedAuthRequest> {
 public:
  static void Create(RenderFrameHost*,
                     mojo::PendingReceiver<blink::mojom::FederatedAuthRequest>);

  FederatedAuthRequestImpl(
      RenderFrameHost*,
      mojo::PendingReceiver<blink::mojom::FederatedAuthRequest>);

  FederatedAuthRequestImpl(const FederatedAuthRequestImpl&) = delete;
  FederatedAuthRequestImpl& operator=(const FederatedAuthRequestImpl&) = delete;

  ~FederatedAuthRequestImpl() override;

  // blink::mojom::FederatedAuthRequest:
  void RequestIdToken(const GURL& provider,
                      const std::string& id_request,
                      blink::mojom::RequestMode mode,
                      RequestIdTokenCallback) override;

  void SetNetworkManagerForTests(
      std::unique_ptr<IdpNetworkRequestManager> manager);
  void SetDialogControllerForTests(
      std::unique_ptr<IdentityRequestDialogController> controller);

 private:
  void OnWellKnownFetched(IdpNetworkRequestManager::FetchStatus status,
                          IdpNetworkRequestManager::Endpoints);

  void OnSigninApproved(IdentityRequestDialogController::UserApproval approval);
  void OnSigninResponseReceived(IdpNetworkRequestManager::SigninResponse status,
                                const std::string& url_or_token);
  void OnTokenProvided(const std::string& id_token);
  void OnIdpPageClosed();
  void OnTokenProvisionApproved(
      IdentityRequestDialogController::UserApproval approval);
  void OnAccountsResponseReceived(
      IdpNetworkRequestManager::AccountsResponse status,
      const IdpNetworkRequestManager::AccountList& accounts);
  void OnAccountSelected(const std::string& account_id);
  void OnTokenResponseReceived(IdpNetworkRequestManager::TokenResponse status,
                               const std::string& id_token);
  std::unique_ptr<WebContents> CreateIdpWebContents();
  void CompleteRequest(blink::mojom::RequestIdTokenStatus,
                       const std::string& id_token);

  std::unique_ptr<IdpNetworkRequestManager> CreateNetworkManager(
      const GURL& provider);
  std::unique_ptr<IdentityRequestDialogController> CreateDialogController();

  std::unique_ptr<IdpNetworkRequestManager> network_manager_;
  std::unique_ptr<IdentityRequestDialogController> request_dialog_controller_;

  // Replacements for testing.
  std::unique_ptr<IdpNetworkRequestManager> mock_network_manager_;
  std::unique_ptr<IdentityRequestDialogController> mock_dialog_controller_;

  // Parameters of auth request.
  GURL provider_;
  std::string id_request_;
  blink::mojom::RequestMode mode_;

  // Fetched from the IDP well-known configuration.
  struct {
    GURL idp;
    GURL token;
    GURL accounts;
  } endpoints_;

  // The WebContents that is used to load the IDP sign-up page. This is
  // created here to allow us to setup proper callbacks on it using
  // |IdTokenRequestCallbackData|. It is then passed along to
  // chrome/browser/ui machinery to be used to load IDP sign-in content.
  std::unique_ptr<WebContents> idp_web_contents_;

  std::string id_token_;
  RequestIdTokenCallback callback_;

  base::WeakPtrFactory<FederatedAuthRequestImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_FEDERATED_AUTH_REQUEST_IMPL_H_
