// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/federated_auth_request_impl.h"

#include "base/callback.h"
#include "base/strings/string_piece.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webid/id_token_request_callback_data.h"
#include "content/browser/webid/webid_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/federated_identity_request_permission_context_delegate.h"
#include "content/public/browser/federated_identity_sharing_permission_context_delegate.h"
#include "content/public/common/content_client.h"
#include "url/url_constants.h"

using blink::mojom::LogoutStatus;
using blink::mojom::RequestIdTokenStatus;
using blink::mojom::RequestMode;
using UserApproval = content::IdentityRequestDialogController::UserApproval;

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
                                              RequestMode mode,
                                              RequestIdTokenCallback callback) {
  if (logout_callback_ || auth_request_callback_) {
    std::move(callback).Run(RequestIdTokenStatus::kErrorTooManyRequests, "");
    return;
  }

  auth_request_callback_ = std::move(callback);
  provider_ = provider;
  id_request_ = id_request;
  mode_ = mode;

  network_manager_ = CreateNetworkManager(provider);
  if (!network_manager_) {
    CompleteRequest(RequestIdTokenStatus::kError, "");
    return;
  }

  request_dialog_controller_ = CreateDialogController();

  if (GetRequestPermissionContext() &&
      GetRequestPermissionContext()->HasRequestPermission(
          origin(), url::Origin::Create(provider_))) {
    network_manager_->FetchIdpWellKnown(
        base::BindOnce(&FederatedAuthRequestImpl::OnWellKnownFetched,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // Use the web contents of the page that initiated the WebID request (i.e.
  // the Relying Party) for showing the initial permission dialog.
  WebContents* web_contents =
      WebContents::FromRenderFrameHost(render_frame_host());

  switch (mode_) {
    case RequestMode::kMediated:
      // Skip permissions for Mediated mode since they are combined with
      // account selection UX.
      OnSigninApproved(UserApproval::kApproved);
      break;
    case RequestMode::kPermission:
      request_dialog_controller_->ShowInitialPermissionDialog(
          web_contents, provider_,
          base::BindOnce(&FederatedAuthRequestImpl::OnSigninApproved,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
  }
}

// TODO(kenrb): Depending on how this code evolves, it might make sense to
// spin session management code into its own service. The prohibition on
// making authentication requests and logout requests at the same time, while
// not problematic for any plausible use case, need not be strictly necessary
// if there is a good way to not have to resource contention between requests.
// https://crbug.com/1200581
void FederatedAuthRequestImpl::Logout(
    const std::vector<std::string>& logout_endpoints,
    LogoutCallback callback) {
  if (logout_callback_ || auth_request_callback_) {
    std::move(callback).Run(LogoutStatus::kErrorTooManyRequests);
    return;
  }

  if (logout_endpoints.empty()) {
    std::move(callback).Run(LogoutStatus::kError);
    return;
  }

  logout_callback_ = std::move(callback);
  logout_endpoints_ = std::move(logout_endpoints);

  network_manager_ = CreateNetworkManager(origin().GetURL());
  if (!network_manager_) {
    CompleteLogoutRequest(LogoutStatus::kError);
    return;
  }

  // TODO(kenrb): These should be parallelized rather than being dispatched
  // serially. https://crbug.com/1200581.
  DispatchOneLogout();
}

void FederatedAuthRequestImpl::OnWellKnownFetched(
    IdpNetworkRequestManager::FetchStatus status,
    IdpNetworkRequestManager::Endpoints endpoints) {
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

  auto ResolveUrl = [&](const std::string& endpoint) {
    if (endpoint.empty())
      return GURL();
    const url::Origin& idp_origin = url::Origin::Create(provider_);
    GURL well_known_url = idp_origin.GetURL().Resolve(
        IdpNetworkRequestManager::kWellKnownFilePath);
    return well_known_url.Resolve(endpoint);
  };

  endpoints_.idp = ResolveUrl(endpoints.idp);
  endpoints_.token = ResolveUrl(endpoints.token);
  endpoints_.accounts = ResolveUrl(endpoints.accounts);

  switch (mode_) {
    case RequestMode::kMediated: {
      // For Mediated mode we require both accounts and token endpoints.
      if (endpoints_.token.is_empty() || endpoints_.accounts.is_empty()) {
        CompleteRequest(RequestIdTokenStatus::kErrorInvalidWellKnown, "");
        return;
      }
      // TODO(kenrb): This has to be same-origin with the provider.
      // https://crbug.com/1141125
      if (!IdpUrlIsValid(endpoints_.token) ||
          !IdpUrlIsValid(endpoints_.accounts)) {
        CompleteRequest(RequestIdTokenStatus::kError, "");
        return;
      }
      network_manager_->SendAccountsRequest(
          endpoints_.accounts,
          base::BindOnce(&FederatedAuthRequestImpl::OnAccountsResponseReceived,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
    }
    case RequestMode::kPermission: {
      // For Permission mode we require both accounts and token endpoints.
      if (endpoints_.idp.is_empty()) {
        CompleteRequest(RequestIdTokenStatus::kErrorInvalidWellKnown, "");
        return;
      }
      // TODO(kenrb): This has to be same-origin with the provider.
      // https://crbug.com/1141125
      if (!IdpUrlIsValid(endpoints_.idp)) {
        CompleteRequest(RequestIdTokenStatus::kError, "");
        return;
      }

      network_manager_->SendSigninRequest(
          endpoints_.idp, id_request_,
          base::BindOnce(&FederatedAuthRequestImpl::OnSigninResponseReceived,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
    }
  }
}

void FederatedAuthRequestImpl::OnSigninApproved(
    IdentityRequestDialogController::UserApproval approval) {
  if (approval != IdentityRequestDialogController::UserApproval::kApproved) {
    CompleteRequest(RequestIdTokenStatus::kApprovalDeclined, "");
    return;
  }

  if (GetRequestPermissionContext()) {
    GetRequestPermissionContext()->GrantRequestPermission(
        origin(), url::Origin::Create(provider_));
  }

  network_manager_->FetchIdpWellKnown(
      base::BindOnce(&FederatedAuthRequestImpl::OnWellKnownFetched,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FederatedAuthRequestImpl::OnSigninResponseReceived(
    IdpNetworkRequestManager::SigninResponse status,
    const std::string& url_or_token) {
  // |url_or_token| is either the URL for the sign-in page or the ID token,
  // depending on |status|.
  switch (status) {
    case IdpNetworkRequestManager::SigninResponse::kLoadIdp: {
      GURL idp_signin_page_url = endpoints_.idp.Resolve(url_or_token);
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
      CompleteRequest(RequestIdTokenStatus::kSuccess, url_or_token);
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

  if (GetSharingPermissionContext() &&
      GetSharingPermissionContext()->HasSharingPermission(
          url::Origin::Create(provider_), origin())) {
    CompleteRequest(RequestIdTokenStatus::kSuccess, id_token_);
    return;
  }

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

  if (GetSharingPermissionContext()) {
    GetSharingPermissionContext()->GrantSharingPermission(
        url::Origin::Create(provider_), origin());
  }

  CompleteRequest(RequestIdTokenStatus::kSuccess, id_token_);
}

void FederatedAuthRequestImpl::OnAccountsResponseReceived(
    IdpNetworkRequestManager::AccountsResponse status,
    const IdpNetworkRequestManager::AccountList& accounts) {
  switch (status) {
    case IdpNetworkRequestManager::AccountsResponse::kNetError: {
      CompleteRequest(RequestIdTokenStatus::kError, "");
      return;
    }
    case IdpNetworkRequestManager::AccountsResponse::kInvalidResponseError: {
      CompleteRequest(RequestIdTokenStatus::kErrorInvalidAccountsResponse, "");
      return;
    }
    case IdpNetworkRequestManager::AccountsResponse::kSuccess: {
      WebContents* rp_web_contents =
          WebContents::FromRenderFrameHost(render_frame_host());
      DCHECK(!idp_web_contents_);
      idp_web_contents_ = CreateIdpWebContents();
      request_dialog_controller_->ShowAccountsDialog(
          rp_web_contents, idp_web_contents_.get(), provider_, accounts,
          base::BindOnce(&FederatedAuthRequestImpl::OnAccountSelected,
                         weak_ptr_factory_.GetWeakPtr()));
      return;
    }
  }
}

void FederatedAuthRequestImpl::OnAccountSelected(
    const std::string& account_id) {
  // This could happen if provider didn't provide any token or user closed the
  // IdP window before it could.
  if (account_id.empty()) {
    CompleteRequest(RequestIdTokenStatus::kError, "");
    return;
  }

  network_manager_->SendTokenRequest(
      endpoints_.token, account_id, id_request_,
      base::BindOnce(&FederatedAuthRequestImpl::OnTokenResponseReceived,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FederatedAuthRequestImpl::OnTokenResponseReceived(
    IdpNetworkRequestManager::TokenResponse status,
    const std::string& id_token) {
  switch (status) {
    case IdpNetworkRequestManager::TokenResponse::kNetError: {
      CompleteRequest(RequestIdTokenStatus::kError, "");
      return;
    }
    case IdpNetworkRequestManager::TokenResponse::kInvalidRequestError: {
      CompleteRequest(RequestIdTokenStatus::kErrorInvalidTokenResponse, "");
      return;
    }
    case IdpNetworkRequestManager::TokenResponse::kInvalidResponseError: {
      CompleteRequest(RequestIdTokenStatus::kErrorInvalidTokenResponse, "");
      return;
    }
    case IdpNetworkRequestManager::TokenResponse::kSuccess: {
      id_token_ = id_token;
      CompleteRequest(RequestIdTokenStatus::kSuccess, id_token_);
      return;
    }
  }
}

void FederatedAuthRequestImpl::DispatchOneLogout() {
  GURL endpoint = GURL(logout_endpoints_.back());
  logout_endpoints_.pop_back();

  if (endpoint.is_valid() && GetRequestPermissionContext() &&
      GetRequestPermissionContext()->HasRequestPermission(
          url::Origin::Create(endpoint), origin())) {
    network_manager_->SendLogout(
        endpoint, base::BindOnce(&FederatedAuthRequestImpl::OnLogoutCompleted,
                                 weak_ptr_factory_.GetWeakPtr()));
  } else {
    logout_status_ = blink::mojom::LogoutStatus::kError;
    if (logout_endpoints_.empty()) {
      CompleteLogoutRequest(logout_status_);
      return;
    }

    DispatchOneLogout();
  }
}

void FederatedAuthRequestImpl::OnLogoutCompleted(
    IdpNetworkRequestManager::LogoutResponse status) {
  // |status| is deliberately ignored because we don't want to tell the
  // calling page whether this cross-origin load succeeded or not.
  if (logout_endpoints_.empty()) {
    CompleteLogoutRequest(logout_status_);
    return;
  }

  DispatchOneLogout();
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
  DCHECK(status == RequestIdTokenStatus::kSuccess || id_token.empty());
  request_dialog_controller_.reset();
  network_manager_.reset();
  // Given that |request_dialog_controller_| has reference to this web content
  // instance we destroy that first.
  idp_web_contents_.reset();
  if (auth_request_callback_)
    std::move(auth_request_callback_).Run(status, id_token);
}

void FederatedAuthRequestImpl::CompleteLogoutRequest(
    blink::mojom::LogoutStatus status) {
  network_manager_.reset();
  if (logout_callback_)
    std::move(logout_callback_).Run(status);
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

void FederatedAuthRequestImpl::SetRequestPermissionDelegateForTests(
    FederatedIdentityRequestPermissionContextDelegate*
        request_permission_delegate) {
  request_permission_delegate_ = request_permission_delegate;
}

void FederatedAuthRequestImpl::SetSharingPermissionDelegateForTests(
    FederatedIdentitySharingPermissionContextDelegate*
        sharing_permission_delegate) {
  sharing_permission_delegate_ = sharing_permission_delegate;
}

FederatedIdentityRequestPermissionContextDelegate*
FederatedAuthRequestImpl::GetRequestPermissionContext() {
  if (!request_permission_delegate_) {
    render_frame_host()
        ->GetBrowserContext()
        ->GetFederatedIdentityRequestPermissionContext();
  }
  return request_permission_delegate_;
}

FederatedIdentitySharingPermissionContextDelegate*
FederatedAuthRequestImpl::GetSharingPermissionContext() {
  if (!sharing_permission_delegate_) {
    render_frame_host()
        ->GetBrowserContext()
        ->GetFederatedIdentitySharingPermissionContext();
  }
  return sharing_permission_delegate_;
}

}  // namespace content
