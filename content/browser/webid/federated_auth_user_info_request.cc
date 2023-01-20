// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/federated_auth_user_info_request.h"

#include "base/functional/callback.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webid/flags.h"
#include "content/browser/webid/webid_utils.h"
#include "content/public/browser/federated_identity_api_permission_context_delegate.h"
#include "content/public/browser/federated_identity_permission_context_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "url/url_constants.h"

namespace content {

using FederatedApiPermissionStatus =
    FederatedIdentityApiPermissionContextDelegate::PermissionStatus;
using LoginState = IdentityRequestAccount::LoginState;

// static
std::unique_ptr<FederatedAuthUserInfoRequest>
FederatedAuthUserInfoRequest::CreateAndStart(
    std::unique_ptr<IdpNetworkRequestManager> network_manager,
    FederatedIdentityApiPermissionContextDelegate* api_permission_delegate,
    FederatedIdentityPermissionContextDelegate* permission_delegate,
    RenderFrameHost* render_frame_host,
    FedCmMetrics* metrics,
    blink::mojom::IdentityProviderConfigPtr provider,
    blink::mojom::FederatedAuthRequest::RequestUserInfoCallback callback) {
  std::unique_ptr<FederatedAuthUserInfoRequest> request =
      base::WrapUnique<FederatedAuthUserInfoRequest>(
          new FederatedAuthUserInfoRequest(
              std::move(network_manager), permission_delegate,
              render_frame_host, metrics, std::move(provider),
              std::move(callback)));
  request->Start(api_permission_delegate);
  return request;
}

FederatedAuthUserInfoRequest::~FederatedAuthUserInfoRequest() {
  CompleteWithError();
}

FederatedAuthUserInfoRequest::FederatedAuthUserInfoRequest(
    std::unique_ptr<IdpNetworkRequestManager> network_manager,
    FederatedIdentityPermissionContextDelegate* permission_delegate,
    RenderFrameHost* render_frame_host,
    FedCmMetrics* metrics,
    blink::mojom::IdentityProviderConfigPtr provider,
    blink::mojom::FederatedAuthRequest::RequestUserInfoCallback callback)
    : network_manager_(std::move(network_manager)),
      permission_delegate_(permission_delegate),
      metrics_(metrics),
      client_id_(provider->client_id),
      idp_config_url_(provider->config_url),
      origin_(render_frame_host->GetLastCommittedOrigin()),
      callback_(std::move(callback)) {
  RenderFrameHost* main_frame = render_frame_host->GetMainFrame();
  DCHECK(main_frame->IsInPrimaryMainFrame());
  embedding_origin_ = main_frame->GetLastCommittedOrigin();

  RenderFrameHost* parent_frame = render_frame_host->GetParentOrOuterDocument();
  parent_frame_origin_ =
      parent_frame ? parent_frame->GetLastCommittedOrigin() : url::Origin();
}

void FederatedAuthUserInfoRequest::Start(
    FederatedIdentityApiPermissionContextDelegate* api_permission_delegate) {
  // Renderer also checks that the origin is same origin with `idp_config_url_`.
  // The check is duplicated in case that the renderer is compromised.
  if (!origin_.IsSameOriginWith(idp_config_url_)) {
    Complete(blink::mojom::RequestUserInfoStatus::kError, absl::nullopt);
    return;
  }

  // Check that `render_frame_host` is for an iframe.
  if (!parent_frame_origin_.GetURL().is_valid()) {
    CompleteWithError();
    return;
  }

  if (!network::IsOriginPotentiallyTrustworthy(
          url::Origin::Create(idp_config_url_))) {
    CompleteWithError();
    return;
  }

  FederatedApiPermissionStatus permission_status =
      api_permission_delegate->GetApiPermissionStatus(embedding_origin_);
  if (permission_status != FederatedApiPermissionStatus::GRANTED) {
    CompleteWithError();
    return;
  }

  if (webid::ShouldFailAccountsEndpointRequestBecauseNotSignedInWithIdp(
          idp_config_url_, permission_delegate_) &&
      GetFedCmIdpSigninStatusMode() == FedCmIdpSigninStatusMode::ENABLED) {
    CompleteWithError();
    return;
  }

  // FederatedProviderFetcher is stored as a member so that
  // FederatedProviderFetcher is destroyed when FederatedAuthRequestImpl is
  // destroyed.
  provider_fetcher_ =
      std::make_unique<FederatedProviderFetcher>(network_manager_.get());
  provider_fetcher_->Start(
      {idp_config_url_}, /*icon_ideal_size=*/0, /*icon_minimum_size=*/0,
      base::BindOnce(
          &FederatedAuthUserInfoRequest::OnAllConfigAndWellKnownFetched,
          weak_ptr_factory_.GetWeakPtr()));
}

void FederatedAuthUserInfoRequest::OnAllConfigAndWellKnownFetched(
    std::vector<FederatedProviderFetcher::FetchResult> fetch_results) {
  provider_fetcher_.reset();

  if (fetch_results.size() != 1u) {
    // This could happen when the user info request was sent from a compromised
    // renderer (>1) or fetch_results is empty (<1).
    CompleteWithError();
    return;
  }

  if (fetch_results[0].error) {
    CompleteWithError();
    return;
  }

  // Make sure that we don't fetch accounts if the IDP sign-in bit is reset to
  // false during the API call. e.g. by the login/logout HEADER.
  does_idp_have_failing_signin_status_ =
      webid::ShouldFailAccountsEndpointRequestBecauseNotSignedInWithIdp(
          idp_config_url_, permission_delegate_);
  if (does_idp_have_failing_signin_status_ &&
      GetFedCmIdpSigninStatusMode() == FedCmIdpSigninStatusMode::ENABLED) {
    CompleteWithError();
    return;
  }

  network_manager_->SendAccountsRequest(
      fetch_results[0].endpoints.accounts, client_id_,
      base::BindOnce(&FederatedAuthUserInfoRequest::OnAccountsResponseReceived,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FederatedAuthUserInfoRequest::OnAccountsResponseReceived(
    IdpNetworkRequestManager::FetchStatus fetch_status,
    IdpNetworkRequestManager::AccountList accounts) {
  webid::UpdateIdpSigninStatusForAccountsEndpointResponse(
      idp_config_url_, fetch_status, does_idp_have_failing_signin_status_,
      permission_delegate_, metrics_);

  if (fetch_status.parse_status !=
      IdpNetworkRequestManager::ParseStatus::kSuccess) {
    CompleteWithError();
    return;
  }
  MaybeReturnAccounts(std::move(accounts));
}

void FederatedAuthUserInfoRequest::MaybeReturnAccounts(
    const IdpNetworkRequestManager::AccountList& accounts) {
  DCHECK(!accounts.empty());

  bool has_returning_accounts = false;
  for (const auto& account : accounts) {
    // The |login_state| will only be |kSignUp| if IDP doesn't provide an
    // |approved_clients| or the client id is NOT on the |approved_clients|
    // list, in which case we trust the IDP that we should treat the user as a
    // new user and shouldn't return the user info. This should override browser
    // local stored permission since a user can revoke their account out of
    // band.
    // Note that we start with the restrictive model and can later evaluate what
    // the expected behavior is when |approved_clients| list is not provided.
    if (account.login_state == LoginState::kSignUp) {
      continue;
    }

    if (!permission_delegate_->HasSharingPermission(
            parent_frame_origin_, embedding_origin_,
            url::Origin::Create(idp_config_url_), account.id)) {
      continue;
    }

    has_returning_accounts = true;
  }

  if (!has_returning_accounts) {
    CompleteWithError();
    return;
  }

  // The user previously accepted the FedCM prompt for one of the returned IdP
  // accounts. Return data for all the IdP accounts.
  std::vector<blink::mojom::IdentityUserInfoPtr> user_info;
  for (const auto& account : accounts) {
    user_info.push_back(blink::mojom::IdentityUserInfo::New(
        account.email, account.given_name, account.name,
        account.picture.spec()));
  }
  Complete(blink::mojom::RequestUserInfoStatus::kSuccess, std::move(user_info));
}

void FederatedAuthUserInfoRequest::Complete(
    blink::mojom::RequestUserInfoStatus status,
    absl::optional<std::vector<blink::mojom::IdentityUserInfoPtr>> user_info) {
  if (!callback_) {
    return;
  }

  std::move(callback_).Run(status, std::move(user_info));
}

void FederatedAuthUserInfoRequest::CompleteWithError() {
  Complete(blink::mojom::RequestUserInfoStatus::kError, absl::nullopt);
}

}  // namespace content
