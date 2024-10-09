// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/federated_auth_user_info_request.h"

#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webid/federated_auth_request_page_data.h"
#include "content/browser/webid/flags.h"
#include "content/browser/webid/webid_utils.h"
#include "content/public/browser/federated_identity_api_permission_context_delegate.h"
#include "content/public/browser/federated_identity_permission_context_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "url/url_constants.h"

namespace content {
namespace {

std::string GetConsoleErrorMessage(FederatedAuthUserInfoRequestResult error) {
  switch (error) {
    case FederatedAuthUserInfoRequestResult::kNotSameOrigin: {
      return "getUserInfo() caller is not same origin as the config URL.";
    }
    case FederatedAuthUserInfoRequestResult::kNotIframe: {
      return "getUserInfo() caller is not an iframe.";
    }
    case FederatedAuthUserInfoRequestResult::kNotPotentiallyTrustworthy: {
      return "getUserInfo() failed because the config URL is not potentially "
             "trustworthy.";
    }
    case FederatedAuthUserInfoRequestResult::kNoApiPermission: {
      return "getUserInfo() is disabled because FedCM is disabled.";
    }
    case FederatedAuthUserInfoRequestResult::kNotSignedInWithIdp: {
      return "getUserInfo() is disabled because the IDP Sign-In Status is "
             "signed-out.";
    }
    case FederatedAuthUserInfoRequestResult::kNoAccountSharingPermission: {
      return "getUserInfo() failed because the user has not yet used FedCM on "
             "this site with the provided IDP.";
    }
    case FederatedAuthUserInfoRequestResult::kInvalidConfigOrWellKnown: {
      return "getUserInfo() failed because the config and well-known files "
             "were invalid.";
    }
    case FederatedAuthUserInfoRequestResult::kInvalidAccountsResponse: {
      return "getUserInfo() failed because of an invalid accounts response.";
    }
    case FederatedAuthUserInfoRequestResult::
        kNoReturningUserFromFetchedAccounts: {
      return "getUserInfo() failed because no account received was a returning "
             "account.";
    }
    case FederatedAuthUserInfoRequestResult::kUnhandledRequest:
    case FederatedAuthUserInfoRequestResult::kSuccess: {
      NOTREACHED_IN_MIGRATION();
      return "";
    }
  }
}

}  // namespace

using FederatedApiPermissionStatus =
    FederatedIdentityApiPermissionContextDelegate::PermissionStatus;
using LoginState = IdentityRequestAccount::LoginState;

// static
std::unique_ptr<FederatedAuthUserInfoRequest>
FederatedAuthUserInfoRequest::Create(
    std::unique_ptr<IdpNetworkRequestManager> network_manager,
    FederatedIdentityPermissionContextDelegate* permission_delegate,
    FederatedIdentityApiPermissionContextDelegate* api_permission_delegate,
    RenderFrameHost* render_frame_host,
    blink::mojom::IdentityProviderConfigPtr provider) {
  std::unique_ptr<FederatedAuthUserInfoRequest> request =
      base::WrapUnique<FederatedAuthUserInfoRequest>(
          new FederatedAuthUserInfoRequest(
              std::move(network_manager), permission_delegate,
              api_permission_delegate, render_frame_host, std::move(provider)));
  return request;
}

FederatedAuthUserInfoRequest::~FederatedAuthUserInfoRequest() {
  CompleteWithError(FederatedAuthUserInfoRequestResult::kUnhandledRequest);
}

FederatedAuthUserInfoRequest::FederatedAuthUserInfoRequest(
    std::unique_ptr<IdpNetworkRequestManager> network_manager,
    FederatedIdentityPermissionContextDelegate* permission_delegate,
    FederatedIdentityApiPermissionContextDelegate* api_permission_delegate,
    RenderFrameHost* render_frame_host,
    blink::mojom::IdentityProviderConfigPtr provider)
    : network_manager_(std::move(network_manager)),
      permission_delegate_(permission_delegate),
      api_permission_delegate_(api_permission_delegate),
      render_frame_host_(render_frame_host),
      client_id_(provider->client_id),
      idp_config_url_(provider->config_url),
      origin_(render_frame_host->GetLastCommittedOrigin()) {
  RenderFrameHost* main_frame = render_frame_host->GetMainFrame();
  DCHECK(main_frame->IsInPrimaryMainFrame());
  embedding_origin_ = main_frame->GetLastCommittedOrigin();

  RenderFrameHost* parent_frame = render_frame_host->GetParentOrOuterDocument();
  parent_frame_origin_ =
      parent_frame ? parent_frame->GetLastCommittedOrigin() : url::Origin();
}

void FederatedAuthUserInfoRequest::SetCallbackAndStart(
    blink::mojom::FederatedAuthRequest::RequestUserInfoCallback callback) {
  callback_ = std::move(callback);

  request_start_time_ = base::TimeTicks::Now();

  // Renderer also checks that the origin is same origin with `idp_config_url_`.
  // The check is duplicated in case that the renderer is compromised.
  if (!origin_.IsSameOriginWith(idp_config_url_)) {
    CompleteWithError(FederatedAuthUserInfoRequestResult::kNotSameOrigin);
    return;
  }

  // Check that `render_frame_host` is for an iframe.
  if (!parent_frame_origin_.GetURL().is_valid()) {
    CompleteWithError(FederatedAuthUserInfoRequestResult::kNotIframe);
    return;
  }

  url::Origin idp_origin = url::Origin::Create(idp_config_url_);
  if (!network::IsOriginPotentiallyTrustworthy(idp_origin)) {
    CompleteWithError(
        FederatedAuthUserInfoRequestResult::kNotPotentiallyTrustworthy);
    return;
  }

  FederatedApiPermissionStatus permission_status =
      api_permission_delegate_->GetApiPermissionStatus(embedding_origin_);
  if (permission_status != FederatedApiPermissionStatus::GRANTED) {
    CompleteWithError(FederatedAuthUserInfoRequestResult::kNoApiPermission);
    return;
  }

  if (webid::ShouldFailAccountsEndpointRequestBecauseNotSignedInWithIdp(
          *render_frame_host_, idp_config_url_, permission_delegate_) &&
      webid::GetIdpSigninStatusMode(*render_frame_host_, idp_origin) ==
          FedCmIdpSigninStatusMode::ENABLED) {
    CompleteWithError(FederatedAuthUserInfoRequestResult::kNotSignedInWithIdp);
    return;
  }

  if (!webid::HasSharingPermissionOrIdpHasThirdPartyCookiesAccess(
          *render_frame_host_, idp_config_url_, embedding_origin_,
          parent_frame_origin_, /*account_id=*/std::nullopt,
          permission_delegate_, api_permission_delegate_)) {
    // If there is no sharing permission or the IdP does not have third party
    // cookies access, we can abort before performing any fetch.
    CompleteWithError(
        FederatedAuthUserInfoRequestResult::kNoAccountSharingPermission);
    return;
  }

  // FederatedProviderFetcher is stored as a member so that
  // FederatedProviderFetcher is destroyed when FederatedAuthRequestImpl is
  // destroyed.
  provider_fetcher_ = std::make_unique<FederatedProviderFetcher>(
      *render_frame_host_, network_manager_.get());
  provider_fetcher_->Start(
      {idp_config_url_}, blink::mojom::RpMode::kPassive, /*icon_ideal_size=*/0,
      /*icon_minimum_size=*/0,
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
    CompleteWithError(
        FederatedAuthUserInfoRequestResult::kInvalidConfigOrWellKnown);
    return;
  }

  if (fetch_results[0].error) {
    CompleteWithError(
        FederatedAuthUserInfoRequestResult::kInvalidConfigOrWellKnown);
    return;
  }

  // Make sure that we don't fetch accounts if the IDP sign-in bit is reset to
  // false during the API call. e.g. by the login/logout HEADER.
  does_idp_have_failing_signin_status_ =
      webid::ShouldFailAccountsEndpointRequestBecauseNotSignedInWithIdp(
          *render_frame_host_, idp_config_url_, permission_delegate_);
  if (does_idp_have_failing_signin_status_ &&
      webid::GetIdpSigninStatusMode(*render_frame_host_,
                                    url::Origin::Create(idp_config_url_)) ==
          FedCmIdpSigninStatusMode::ENABLED) {
    CompleteWithError(FederatedAuthUserInfoRequestResult::kNotSignedInWithIdp);
    return;
  }

  network_manager_->SendAccountsRequest(
      fetch_results[0].endpoints.accounts, client_id_,
      base::BindOnce(&FederatedAuthUserInfoRequest::OnAccountsResponseReceived,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FederatedAuthUserInfoRequest::OnAccountsResponseReceived(
    IdpNetworkRequestManager::FetchStatus fetch_status,
    std::vector<IdentityRequestAccountPtr> accounts) {
  webid::UpdateIdpSigninStatusForAccountsEndpointResponse(
      *render_frame_host_, idp_config_url_, fetch_status,
      does_idp_have_failing_signin_status_, permission_delegate_);

  if (fetch_status.parse_status !=
      IdpNetworkRequestManager::ParseStatus::kSuccess) {
    CompleteWithError(
        FederatedAuthUserInfoRequestResult::kInvalidAccountsResponse);
    return;
  }

  webid::GetPageData(render_frame_host_->GetPage())
      ->SetUserInfoAccountsResponseTime(idp_config_url_,
                                        base::TimeTicks::Now());

  // Populate the accounts login state.
  for (auto& account : accounts) {
    // We set the login state based on the IDP response if it sends
    // back an approved_clients list. If it does not, we need to set
    // it here based on browser state.
    if (account->login_state) {
      continue;
    }

    LoginState login_state = LoginState::kSignUp;
    // Consider this a sign-in if we have seen a successful sign-up for
    // this account before.
    if (permission_delegate_->GetLastUsedTimestamp(
            parent_frame_origin_, embedding_origin_,
            url::Origin::Create(idp_config_url_), account->id)) {
      login_state = LoginState::kSignIn;
    }
    account->login_state = login_state;
  }

  MaybeReturnAccounts(std::move(accounts));
}

void FederatedAuthUserInfoRequest::MaybeReturnAccounts(
    const std::vector<IdentityRequestAccountPtr>& accounts) {
  DCHECK(!accounts.empty());

  bool has_returning_accounts = false;
  for (const auto& account : accounts) {
    if (IsReturningAccount(*account)) {
      has_returning_accounts = true;
      break;
    }
  }

  FedCmMetrics::NumAccounts num_accounts = FedCmMetrics::NumAccounts::kZero;
  if (has_returning_accounts) {
    num_accounts = accounts.size() == 1u ? FedCmMetrics::NumAccounts::kOne
                                         : FedCmMetrics::NumAccounts::kMultiple;
  }
  base::UmaHistogramEnumeration("Blink.FedCm.UserInfo.NumAccounts",
                                num_accounts);
  base::UmaHistogramMediumTimes("Blink.FedCm.UserInfo.TimeToRequestCompleted",
                                base::TimeTicks::Now() - request_start_time_);
  if (!has_returning_accounts) {
    CompleteWithError(FederatedAuthUserInfoRequestResult::
                          kNoReturningUserFromFetchedAccounts);
    return;
  }

  // The user previously accepted the FedCM prompt for one of the returned IdP
  // accounts. Return data for all the IdP accounts.
  std::vector<blink::mojom::IdentityUserInfoPtr> user_info;
  std::vector<blink::mojom::IdentityUserInfoPtr> not_returning_accounts;
  for (const auto& account : accounts) {
    if (IsReturningAccount(*account)) {
      user_info.push_back(blink::mojom::IdentityUserInfo::New(
          account->email, account->given_name, account->name,
          account->picture.spec()));
    } else {
      not_returning_accounts.push_back(blink::mojom::IdentityUserInfo::New(
          account->email, account->given_name, account->name,
          account->picture.spec()));
    }
  }
  user_info.insert(user_info.end(),
                   std::make_move_iterator(not_returning_accounts.begin()),
                   std::make_move_iterator(not_returning_accounts.end()));
  Complete(blink::mojom::RequestUserInfoStatus::kSuccess, std::move(user_info),
           FederatedAuthUserInfoRequestResult::kSuccess);
}

bool FederatedAuthUserInfoRequest::IsReturningAccount(
    const IdentityRequestAccount& account) {
  // The |login_state| will only be |kSignUp| if IDP provides an
  // |approved_clients| AND the client id is NOT on the |approved_clients|
  // list, in which case we trust the IDP that we should treat the user as a
  // new user and shouldn't return the user info. This should override browser
  // local stored permission since a user can revoke their account out of
  // band.
  if (account.login_state == LoginState::kSignUp) {
    return false;
  }

  return webid::HasSharingPermissionOrIdpHasThirdPartyCookiesAccess(
      *render_frame_host_, idp_config_url_, embedding_origin_,
      parent_frame_origin_, account.id, permission_delegate_,
      api_permission_delegate_);
}

void FederatedAuthUserInfoRequest::Complete(
    blink::mojom::RequestUserInfoStatus status,
    std::optional<std::vector<blink::mojom::IdentityUserInfoPtr>> user_info,
    FederatedAuthUserInfoRequestResult request_status) {
  if (!callback_) {
    return;
  }

  base::UmaHistogramEnumeration("Blink.FedCm.UserInfo.Status", request_status);

  std::move(callback_).Run(status, std::move(user_info));
}

void FederatedAuthUserInfoRequest::CompleteWithError(
    FederatedAuthUserInfoRequestResult error) {
  // Do not add a console error for an unhandled request: the RenderFrameHost
  // may have been destroyed.
  if (error != FederatedAuthUserInfoRequestResult::kUnhandledRequest) {
    render_frame_host_->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError,
        GetConsoleErrorMessage(error));
    AddDevToolsIssue(error);
  }
  Complete(blink::mojom::RequestUserInfoStatus::kError, std::nullopt, error);
}

void FederatedAuthUserInfoRequest::AddDevToolsIssue(
    FederatedAuthUserInfoRequestResult error) {
  DCHECK_NE(error, FederatedAuthUserInfoRequestResult::kSuccess);

  auto details = blink::mojom::InspectorIssueDetails::New();
  auto federated_auth_user_info_request_details =
      blink::mojom::FederatedAuthUserInfoRequestIssueDetails::New(error);
  details->federated_auth_user_info_request_details =
      std::move(federated_auth_user_info_request_details);
  render_frame_host_->ReportInspectorIssue(
      blink::mojom::InspectorIssueInfo::New(
          blink::mojom::InspectorIssueCode::kFederatedAuthUserInfoRequestIssue,
          std::move(details)));
}

}  // namespace content
