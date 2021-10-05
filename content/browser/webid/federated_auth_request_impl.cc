// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/federated_auth_request_impl.h"

#include "base/callback.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_piece.h"
#include "content/browser/bad_message.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webid/id_token_request_callback_data.h"
#include "content/browser/webid/webid_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/federated_identity_active_session_permission_context_delegate.h"
#include "content/public/browser/federated_identity_request_permission_context_delegate.h"
#include "content/public/browser/federated_identity_sharing_permission_context_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "ui/accessibility/ax_mode.h"
#include "url/url_constants.h"

using blink::mojom::LogoutStatus;
using blink::mojom::RequestIdTokenStatus;
using blink::mojom::RequestMode;
using UserApproval = content::IdentityRequestDialogController::UserApproval;
using LoginState = content::IdentityRequestAccount::LoginState;
using SignInMode = content::IdentityRequestAccount::SignInMode;

namespace content {

namespace {
std::string FormatRequestParams(const std::string& client_id,
                                const std::string& nonce) {
  std::string query;
  // 'openid' scope is required for IdPs using OpenID Connect. We hope that IdPs
  // using OAuth will ignore it, although some might return errors.
  query += "scope=openid profile email";
  if (client_id.length() > 0) {
    query += "&client_id=" + client_id;
  }
  if (nonce.length() > 0) {
    query += "&nonce=" + nonce;
  }
  return query;
}
}  // namespace

FederatedAuthRequestImpl::FederatedAuthRequestImpl(RenderFrameHost* host,
                                                   const url::Origin& origin)
    : render_frame_host_(host), origin_(origin) {}

FederatedAuthRequestImpl::~FederatedAuthRequestImpl() {
  // Ensures key data members are destructed in proper order and resolves any
  // pending promise.
  CompleteRequest(RequestIdTokenStatus::kError, "");
}

void FederatedAuthRequestImpl::RequestIdToken(
    const GURL& provider,
    const std::string& client_id,
    const std::string& nonce,
    RequestMode mode,
    bool prefer_auto_sign_in,
    blink::mojom::FederatedAuthRequest::RequestIdTokenCallback callback) {
  if (logout_callback_ || auth_request_callback_) {
    std::move(callback).Run(RequestIdTokenStatus::kErrorTooManyRequests, "");
    return;
  }

  auth_request_callback_ = std::move(callback);
  provider_ = provider;
  client_id_ = client_id;
  nonce_ = nonce;
  mode_ = mode;
  prefer_auto_sign_in_ = prefer_auto_sign_in;

  network_manager_ = CreateNetworkManager(provider);
  if (!network_manager_) {
    CompleteRequest(RequestIdTokenStatus::kError, "");
    return;
  }

  request_dialog_controller_ = CreateDialogController();

  // Skip request permission if it is already given for this IDP or if we are
  // using the mediated mode in which case the permission is combined with
  // account selector UI.
  if (mode_ == RequestMode::kMediated ||
      (GetRequestPermissionContext() &&
       GetRequestPermissionContext()->HasRequestPermission(
           origin_, url::Origin::Create(provider_)))) {
    network_manager_->FetchIdpWellKnown(
        base::BindOnce(&FederatedAuthRequestImpl::OnWellKnownFetched,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  DCHECK_EQ(mode_, RequestMode::kPermission);
  // Use the web contents of the page that initiated the WebID request (i.e.
  // the Relying Party) for showing the initial permission dialog.
  WebContents* web_contents =
      WebContents::FromRenderFrameHost(render_frame_host_);

  request_dialog_controller_->ShowInitialPermissionDialog(
      web_contents, provider_,
      IdentityRequestDialogController::PermissionDialogMode::kStateful,
      base::BindOnce(&FederatedAuthRequestImpl::OnSigninApproved,
                     weak_ptr_factory_.GetWeakPtr()));
}

// TODO(kenrb): Depending on how this code evolves, it might make sense to
// spin session management code into its own service. The prohibition on
// making authentication requests and logout requests at the same time, while
// not problematic for any plausible use case, need not be strictly necessary
// if there is a good way to not have to resource contention between requests.
// https://crbug.com/1200581
void FederatedAuthRequestImpl::Logout(
    std::vector<blink::mojom::LogoutRequestPtr> logout_requests,
    blink::mojom::FederatedAuthRequest::LogoutCallback callback) {
  if (logout_callback_ || auth_request_callback_) {
    std::move(callback).Run(LogoutStatus::kErrorTooManyRequests);
    return;
  }

  DCHECK(logout_requests_.empty());

  logout_callback_ = std::move(callback);

  if (logout_requests.empty()) {
    CompleteLogoutRequest(LogoutStatus::kError);
    return;
  }

  if (base::ranges::any_of(logout_requests, [](auto& request) {
        return !request->endpoint.is_valid();
      })) {
    bad_message::ReceivedBadMessage(render_frame_host_->GetProcess(),
                                    bad_message::FARI_LOGOUT_BAD_ENDPOINT);
    CompleteLogoutRequest(LogoutStatus::kError);
    return;
  }

  for (auto& request : logout_requests) {
    logout_requests_.push(std::move(request));
  }

  network_manager_ = CreateNetworkManager(origin_.GetURL());
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
  endpoints_.client_id_metadata = ResolveUrl(endpoints.client_id_metadata);

  switch (mode_) {
    case RequestMode::kMediated: {
      // For Mediated mode we require accounts, token and client ID endpoints.
      if (endpoints_.token.is_empty() || endpoints_.accounts.is_empty() ||
          endpoints_.client_id_metadata.is_empty()) {
        CompleteRequest(RequestIdTokenStatus::kErrorInvalidWellKnown, "");
        return;
      }
      // TODO(kenrb): This has to be same-origin with the provider.
      // https://crbug.com/1141125
      if (!IdpUrlIsValid(endpoints_.token) ||
          !IdpUrlIsValid(endpoints_.accounts) ||
          !IdpUrlIsValid(endpoints_.client_id_metadata)) {
        CompleteRequest(RequestIdTokenStatus::kError, "");
        return;
      }
      network_manager_->FetchClientIdMetadata(
          endpoints_.client_id_metadata, client_id_,
          base::BindOnce(
              &FederatedAuthRequestImpl::OnClientIdMetadataResponseReceived,
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
          endpoints_.idp, FormatRequestParams(client_id_, nonce_),
          base::BindOnce(&FederatedAuthRequestImpl::OnSigninResponseReceived,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
    }
  }
}

void FederatedAuthRequestImpl::OnClientIdMetadataResponseReceived(
    IdpNetworkRequestManager::FetchStatus status,
    IdpNetworkRequestManager::ClientIdMetadata data) {
  // TODO(cbiesinger): check status argument to make sure fetching/parsing
  // succeeded?
  client_id_metadata_ = data;
  network_manager_->SendAccountsRequest(
      endpoints_.accounts,
      base::BindOnce(&FederatedAuthRequestImpl::OnAccountsResponseReceived,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FederatedAuthRequestImpl::OnSigninApproved(
    IdentityRequestDialogController::UserApproval approval) {
  if (approval != IdentityRequestDialogController::UserApproval::kApproved) {
    CompleteRequest(RequestIdTokenStatus::kApprovalDeclined, "");
    return;
  }

  if (GetRequestPermissionContext()) {
    GetRequestPermissionContext()->GrantRequestPermission(
        origin_, url::Origin::Create(provider_));
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
          WebContents::FromRenderFrameHost(render_frame_host_);

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
      WebContents::FromRenderFrameHost(render_frame_host_);

  if (GetSharingPermissionContext() &&
      GetSharingPermissionContext()->HasSharingPermission(
          url::Origin::Create(provider_), origin_)) {
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
    // Grant sharing permission for RP/IDP pair without a specific account.
    GetSharingPermissionContext()->GrantSharingPermission(
        url::Origin::Create(provider_), origin_);
  }

  CompleteRequest(RequestIdTokenStatus::kSuccess, id_token_);
}

void FederatedAuthRequestImpl::OnAccountsResponseReceived(
    IdpNetworkRequestManager::AccountsResponse status,
    IdpNetworkRequestManager::AccountList accounts) {
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
          WebContents::FromRenderFrameHost(render_frame_host_);
      DCHECK(!idp_web_contents_);

      // Populate the accounts login state.
      for (auto& account : accounts) {
        LoginState login_state = LoginState::kSignUp;
        // Consider this a sign-in if we have seen a successful sign-up for
        // this account before.
        if (GetSharingPermissionContext() &&
            GetSharingPermissionContext()->HasSharingPermissionForAccount(
                url::Origin::Create(provider_), origin_, account.sub)) {
          login_state = LoginState::kSignIn;
        }
        account.login_state = login_state;
      }

      idp_web_contents_ = CreateIdpWebContents();
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
      ClientIdData data{endpoints_.client_id_metadata.Resolve(
                            client_id_metadata_.terms_of_service_url),
                        endpoints_.client_id_metadata.Resolve(
                            client_id_metadata_.privacy_policy_url)};
      request_dialog_controller_->ShowAccountsDialog(
          rp_web_contents, idp_web_contents_.get(), provider_, accounts, data,
          is_auto_sign_in ? SignInMode::kAuto : SignInMode::kExplicit,
          base::BindOnce(&FederatedAuthRequestImpl::OnAccountSelected,
                         weak_ptr_factory_.GetWeakPtr()));
      return;
    }
  }
}

void FederatedAuthRequestImpl::OnAccountSelected(
    const std::string& account_id) {
  // This could happen if user didn't select any accounts.
  if (account_id.empty()) {
    CompleteRequest(RequestIdTokenStatus::kError, "");
    return;
  }

  // Account selection is considered sufficient for granting request permission
  // (which also implies the logout permission).
  if (GetRequestPermissionContext()) {
    GetRequestPermissionContext()->GrantRequestPermission(
        origin_, url::Origin::Create(provider_));
  }

  account_id_ = account_id;
  network_manager_->SendTokenRequest(
      endpoints_.token, account_id_, FormatRequestParams(client_id_, nonce_),
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
      if (GetSharingPermissionContext()) {
        DCHECK_EQ(mode_, RequestMode::kMediated);
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
        GetSharingPermissionContext()->GrantSharingPermissionForAccount(
            url::Origin::Create(provider_), origin_, account_id_);
      }

      if (GetActiveSessionPermissionContext()) {
        GetActiveSessionPermissionContext()->GrantActiveSession(
            origin_, url::Origin::Create(provider_), account_id_);
      }

      id_token_ = id_token;
      CompleteRequest(RequestIdTokenStatus::kSuccess, id_token_);
      return;
    }
  }
}

void FederatedAuthRequestImpl::DispatchOneLogout() {
  auto logout_request = std::move(logout_requests_.front());
  DCHECK(logout_request->endpoint.is_valid());
  std::string account_id = logout_request->account_id;
  auto endpoint_origin = url::Origin::Create(logout_request->endpoint);
  logout_requests_.pop();

  if (!GetActiveSessionPermissionContext()) {
    CompleteLogoutRequest(LogoutStatus::kError);
    return;
  }

  if (GetActiveSessionPermissionContext()->HasActiveSession(
          endpoint_origin, origin_, account_id)) {
    network_manager_->SendLogout(
        logout_request->endpoint,
        base::BindOnce(&FederatedAuthRequestImpl::OnLogoutCompleted,
                       weak_ptr_factory_.GetWeakPtr()));
    GetActiveSessionPermissionContext()->RevokeActiveSession(
        endpoint_origin, origin_, account_id);
  } else {
    if (logout_requests_.empty()) {
      CompleteLogoutRequest(LogoutStatus::kSuccess);
      return;
    }

    DispatchOneLogout();
  }
}

void FederatedAuthRequestImpl::OnLogoutCompleted() {
  if (logout_requests_.empty()) {
    CompleteLogoutRequest(LogoutStatus::kSuccess);
    return;
  }

  DispatchOneLogout();
}

std::unique_ptr<WebContents> FederatedAuthRequestImpl::CreateIdpWebContents() {
  auto idp_web_contents = content::WebContents::Create(
      WebContents::CreateParams(render_frame_host_->GetBrowserContext()));

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
  account_id_ = std::string();
  if (auth_request_callback_)
    std::move(auth_request_callback_).Run(status, id_token);
}

void FederatedAuthRequestImpl::CompleteLogoutRequest(
    blink::mojom::LogoutStatus status) {
  network_manager_.reset();
  base::queue<blink::mojom::LogoutRequestPtr>().swap(logout_requests_);
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

void FederatedAuthRequestImpl::SetActiveSessionPermissionDelegateForTests(
    FederatedIdentityActiveSessionPermissionContextDelegate*
        active_session_permission_delegate) {
  active_session_permission_delegate_ = active_session_permission_delegate;
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

FederatedIdentityActiveSessionPermissionContextDelegate*
FederatedAuthRequestImpl::GetActiveSessionPermissionContext() {
  if (!active_session_permission_delegate_) {
    active_session_permission_delegate_ =
        render_frame_host_->GetBrowserContext()
            ->GetFederatedIdentityActiveSessionPermissionContext();
  }
  return active_session_permission_delegate_;
}

FederatedIdentityRequestPermissionContextDelegate*
FederatedAuthRequestImpl::GetRequestPermissionContext() {
  if (!request_permission_delegate_) {
    request_permission_delegate_ =
        render_frame_host_->GetBrowserContext()
            ->GetFederatedIdentityRequestPermissionContext();
  }
  return request_permission_delegate_;
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

}  // namespace content
