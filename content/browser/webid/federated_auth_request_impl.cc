// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/federated_auth_request_impl.h"

#include "base/callback.h"
#include "base/strings/string_piece.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webid/id_token_request_callback_data.h"
#include "content/browser/webid/webid_utils.h"
#include "content/public/common/content_client.h"
#include "url/url_constants.h"

using blink::mojom::RequestIdTokenStatus;

namespace content {

FederatedAuthRequestImpl::FederatedAuthRequestImpl(
    RenderFrameHost* host,
    mojo::PendingReceiver<blink::mojom::FederatedAuthRequest> receiver)
    : FrameServiceBase(host, std::move(receiver)) {}

FederatedAuthRequestImpl::~FederatedAuthRequestImpl() {
  // Ensures key data members are destructed in proper order and resolves any
  // pending promise.
  CompleteRequest(RequestIdTokenStatus::kError, "");
}

// static
void FederatedAuthRequestImpl::Create(
    RenderFrameHost* host,
    mojo::PendingReceiver<blink::mojom::FederatedAuthRequest> receiver) {
  DCHECK(host);

  // TODO(kenrb): This should also be verified in the renderer process before
  // the mojo method is invoked, causing the promise to be rejected.
  // https://crbug.com/1141125
  // It is safe to access host->GetLastCommittedOrigin during construction
  // but FrameServiceBase::origin() should be used thereafter.
  if (!IsSameOriginWithAncestors(host, host->GetLastCommittedOrigin())) {
    mojo::ReportBadMessage(
        "navigator.id.get cannot be invoked from within cross-origin iframes.");
    return;
  }

  // FederatedAuthRequestImpl owns itself. It will self-destruct when a mojo
  // interface error occurs, the render frame host is deleted, or the render
  // frame host navigates to a new document.
  new FederatedAuthRequestImpl(host, std::move(receiver));
}

void FederatedAuthRequestImpl::RequestIdToken(const GURL& provider,
                                              const std::string& id_request,
                                              RequestIdTokenCallback callback) {
  if (callback_) {
    std::move(callback).Run(RequestIdTokenStatus::kErrorTooManyRequests, "");
    return;
  }

  callback_ = std::move(callback);
  provider_ = provider;
  id_request_ = id_request;

  network_manager_ = CreateNetworkManager(provider);
  if (!network_manager_) {
    CompleteRequest(RequestIdTokenStatus::kError, "");
    return;
  }

  request_dialog_controller_ = CreateDialogController();

  // Use the web contents of the page that initiated the WebID request (i.e.
  // the Relying Party) for showing the initial permission dialog.
  WebContents* web_contents =
      WebContents::FromRenderFrameHost(render_frame_host());

  request_dialog_controller_->ShowInitialPermissionDialog(
      web_contents, provider_,
      base::BindOnce(&FederatedAuthRequestImpl::OnSigninApproved,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FederatedAuthRequestImpl::OnWellKnownFetched(
    IdpNetworkRequestManager::FetchStatus status,
    const std::string& idp_endpoint) {
  switch (status) {
    case IdpNetworkRequestManager::FetchStatus::kWebIdNotSupported: {
      CompleteRequest(RequestIdTokenStatus::kErrorWebIdNotSupportedByProvider,
                      "");
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kFetchError: {
      CompleteRequest(RequestIdTokenStatus::kErrorFetchingWellKnown, "");
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kInvalidResponseError: {
      CompleteRequest(RequestIdTokenStatus::kErrorInvalidWellKnown, "");
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kSuccess: {
      // Intentional fall-through.
    }
  }

  const url::Origin& idp_origin = url::Origin::Create(provider_);
  GURL well_known_url =
      idp_origin.GetURL().Resolve(IdpNetworkRequestManager::kWellKnownFilePath);
  idp_endpoint_url_ = well_known_url.Resolve(idp_endpoint);

  // TODO(kenrb): This has to be same-origin with the provider.
  // https://crbug.com/1141125
  if (!IdpUrlIsValid(idp_endpoint_url_)) {
    CompleteRequest(RequestIdTokenStatus::kError, "");
    return;
  }

  network_manager_->SendSigninRequest(
      idp_endpoint_url_, id_request_,
      base::BindOnce(&FederatedAuthRequestImpl::OnSigninResponseReceived,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FederatedAuthRequestImpl::OnSigninApproved(
    IdentityRequestDialogController::UserApproval approval) {
  if (approval != IdentityRequestDialogController::UserApproval::kApproved) {
    CompleteRequest(RequestIdTokenStatus::kApprovalDeclined, "");
    return;
  }

  network_manager_->FetchIdpWellKnown(
      base::BindOnce(&FederatedAuthRequestImpl::OnWellKnownFetched,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FederatedAuthRequestImpl::OnSigninResponseReceived(
    IdpNetworkRequestManager::SigninResponse status,
    const std::string& response) {
  // |response| is either the URL for the sign-in page or the ID token,
  // depending on |status|.
  switch (status) {
    case IdpNetworkRequestManager::SigninResponse::kLoadIdp: {
      GURL idp_signin_page_url = idp_endpoint_url_.Resolve(response);
      if (!IdpUrlIsValid(idp_signin_page_url)) {
        CompleteRequest(RequestIdTokenStatus::kError, "");
        return;
      }
      WebContents* rp_web_contents =
          WebContents::FromRenderFrameHost(render_frame_host());

      DCHECK(!idp_web_contents_);
      idp_web_contents_ = CreateIdpWebContents();

      request_dialog_controller_->ShowIdProviderWindow(
          rp_web_contents, idp_web_contents_.get(), idp_signin_page_url,
          base::BindOnce(&FederatedAuthRequestImpl::OnIdpPageClosed,
                         weak_ptr_factory_.GetWeakPtr()));
      return;
    }
    case IdpNetworkRequestManager::SigninResponse::kTokenGranted: {
      // TODO(kenrb): Returning success here has to be dependent on whether
      // a WebID flow has succeeded in the past, otherwise jump to
      // the token permission dialog.
      CompleteRequest(RequestIdTokenStatus::kSuccess, response);
      return;
    }
    case IdpNetworkRequestManager::SigninResponse::kSigninError: {
      CompleteRequest(RequestIdTokenStatus::kErrorFetchingSignin, "");
      return;
    }
    case IdpNetworkRequestManager::SigninResponse::kInvalidResponseError: {
      CompleteRequest(RequestIdTokenStatus::kErrorInvalidSigninResponse, "");
      return;
    }
  }
}

void FederatedAuthRequestImpl::OnTokenProvided(const std::string& id_token) {
  id_token_ = id_token;

  // Close the IDP window which leads to OnIdpPageClosed which is our common.
  //
  // TODO(majidvp): Consider if we should not wait on the IDP window closing and
  // instead should directly call `OnIdpPageClosed` here.
  request_dialog_controller_->CloseIdProviderWindow();

  // Note that we always process the token on `OnIdpPageClosed()`.
  // It is possible to get there either via:
  //  (a) IDP providing a token as shown below, or
  //  (b) User closing the sign-in window.
  //
  // +-----------------------+     +-------------------+     +-----------------+
  // | FederatedAuthRequest  |     | DialogController  |     | IDPWebContents  |
  // +-----------------------+     +-------------------+     +-----------------+
  //             |                           |                        |
  //             | ShowIdProviderWindow()    |                        |
  //             |-------------------------->|                        |
  //             |                           |                        |
  //             |                           | navigate to idp.com    |
  //             |                           |----------------------->|
  //             |                           |                        |
  //             |                           |  OnTokenProvided(token)|
  //             |<---------------------------------------------------|
  //             |                           |                        |
  //             | CloseIdProviderWindow()   |                        |
  //             |-------------------------->|                        |
  //             |                           |                        |
  //             |                    closed |                        |
  //             |<--------------------------|                        |
  //             |                           |                        |
  //     OnIdpPageClosed()                   |                        |
  //             |                           |                        |
  //
}

void FederatedAuthRequestImpl::OnIdpPageClosed() {
  // This could happen if provider didn't provide any token or user closed the
  // IdP window before it could.
  if (id_token_.empty()) {
    CompleteRequest(RequestIdTokenStatus::kError, "");
    return;
  }

  WebContents* rp_web_contents =
      WebContents::FromRenderFrameHost(render_frame_host());

  request_dialog_controller_->ShowTokenExchangePermissionDialog(
      rp_web_contents, provider_,
      base::BindOnce(&FederatedAuthRequestImpl::OnTokenProvisionApproved,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FederatedAuthRequestImpl::OnTokenProvisionApproved(
    IdentityRequestDialogController::UserApproval approval) {
  if (approval != IdentityRequestDialogController::UserApproval::kApproved) {
    CompleteRequest(RequestIdTokenStatus::kApprovalDeclined, "");
    return;
  }

  CompleteRequest(RequestIdTokenStatus::kSuccess, id_token_);
}

std::unique_ptr<WebContents> FederatedAuthRequestImpl::CreateIdpWebContents() {
  auto idp_web_contents = content::WebContents::Create(
      WebContents::CreateParams(render_frame_host()->GetBrowserContext()));

  // Store the callback on the provider web contents so that it can be
  // used later.
  IdTokenRequestCallbackData::Set(
      idp_web_contents.get(),
      base::BindOnce(&FederatedAuthRequestImpl::OnTokenProvided,
                     weak_ptr_factory_.GetWeakPtr()));
  return idp_web_contents;
}

void FederatedAuthRequestImpl::CompleteRequest(
    blink::mojom::RequestIdTokenStatus status,
    const std::string& id_token) {
  request_dialog_controller_.reset();
  network_manager_.reset();
  // Given that |request_dialog_controller_| has reference to this web content
  // instance we destroy that first.
  idp_web_contents_.reset();
  if (callback_)
    std::move(callback_).Run(status, id_token);
}

std::unique_ptr<IdpNetworkRequestManager>
FederatedAuthRequestImpl::CreateNetworkManager(const GURL& provider) {
  if (mock_network_manager_)
    return std::move(mock_network_manager_);

  return IdpNetworkRequestManager::Create(provider, render_frame_host());
}

std::unique_ptr<IdentityRequestDialogController>
FederatedAuthRequestImpl::CreateDialogController() {
  if (mock_dialog_controller_)
    return std::move(mock_dialog_controller_);

  return GetContentClient()->browser()->CreateIdentityRequestDialogController();
}

void FederatedAuthRequestImpl::SetNetworkManagerForTests(
    std::unique_ptr<IdpNetworkRequestManager> manager) {
  mock_network_manager_ = std::move(manager);
}

void FederatedAuthRequestImpl::SetDialogControllerForTests(
    std::unique_ptr<IdentityRequestDialogController> controller) {
  mock_dialog_controller_ = std::move(controller);
}

}  // namespace content
