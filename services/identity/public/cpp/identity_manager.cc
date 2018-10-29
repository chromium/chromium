// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/public/cpp/identity_manager.h"

#include "google_apis/gaia/gaia_auth_util.h"
#include "services/identity/public/cpp/primary_account_mutator.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace identity {

namespace {

// Local copy of the account ID used for supervised users (defined in //chrome
// as supervised_users::kSupervisedUserPseudoEmail). Simply copied to avoid
// plumbing it from //chrome all the way down through the Identity Service just
// to handle the corner cases below.
// TODO(860492): Remove this once supervised user support is removed.
const char kSupervisedUserPseudoEmail[] = "managed_user@localhost";

// A made-up Gaia ID to populate the supervised user's AccountInfo with in order
// to maintain the invariant that the AccountInfos passed out by IdentityManager
// always have an account ID, Gaia ID, and email set.
// TODO(860492): Remove this once supervised user support is removed.
const char kSupervisedUserPseudoGaiaID[] = "managed_user_gaia_id";

// Maps a vector of gaia::ListedAccount structs to a corresponding vector of
// AccountInfo structs.
std::vector<AccountInfo> ListedAccountsToAccountInfos(
    const std::vector<gaia::ListedAccount>& listed_accounts) {
  std::vector<AccountInfo> account_infos;

  for (const auto& listed_account : listed_accounts) {
    AccountInfo account_info;
    account_info.account_id = listed_account.id;
    account_info.gaia = listed_account.gaia_id;
    account_info.email = listed_account.email;
    account_infos.push_back(account_info);
  }

  return account_infos;
}

}  // namespace

IdentityManager::IdentityManager(
    SigninManagerBase* signin_manager,
    ProfileOAuth2TokenService* token_service,
    AccountTrackerService* account_tracker_service,
    GaiaCookieManagerService* gaia_cookie_manager_service,
    std::unique_ptr<PrimaryAccountMutator> primary_account_mutator)
    : signin_manager_(signin_manager),
      token_service_(token_service),
      account_tracker_service_(account_tracker_service),
      gaia_cookie_manager_service_(gaia_cookie_manager_service),
      primary_account_mutator_(std::move(primary_account_mutator)) {
  signin_manager_->AddObserver(this);
  token_service_->AddDiagnosticsObserver(this);
  token_service_->AddObserver(this);
  gaia_cookie_manager_service_->AddObserver(this);
}

IdentityManager::~IdentityManager() {
  signin_manager_->RemoveObserver(this);
  token_service_->RemoveObserver(this);
  token_service_->RemoveDiagnosticsObserver(this);
  gaia_cookie_manager_service_->RemoveObserver(this);
}

AccountInfo IdentityManager::GetPrimaryAccountInfo() const {
  return signin_manager_->GetAuthenticatedAccountInfo();
}

const std::string& IdentityManager::GetPrimaryAccountId() const {
  return signin_manager_->GetAuthenticatedAccountId();
}

bool IdentityManager::HasPrimaryAccount() const {
  return signin_manager_->IsAuthenticated();
}

#if !defined(OS_CHROMEOS)
void IdentityManager::ClearPrimaryAccount(
    ClearAccountTokensAction token_action,
    signin_metrics::ProfileSignout signout_source_metric,
    signin_metrics::SignoutDelete signout_delete_metric) {
  SigninManager* signin_manager =
      SigninManager::FromSigninManagerBase(signin_manager_);

  switch (token_action) {
    case IdentityManager::ClearAccountTokensAction::kDefault:
      signin_manager->SignOut(signout_source_metric, signout_delete_metric);
      break;
    case IdentityManager::ClearAccountTokensAction::kKeepAll:
      signin_manager->SignOutAndKeepAllAccounts(signout_source_metric,
                                                signout_delete_metric);
      break;
    case IdentityManager::ClearAccountTokensAction::kRemoveAll:
      signin_manager->SignOutAndRemoveAllAccounts(signout_source_metric,
                                                  signout_delete_metric);
      break;
  }

  // NOTE: IdentityManager::Observers are notified in GoogleSignedOut().
}
#endif  // defined(OS_CHROMEOS)

std::vector<AccountInfo> IdentityManager::GetAccountsWithRefreshTokens() const {
  std::vector<std::string> account_ids_with_tokens =
      token_service_->GetAccounts();

  std::vector<AccountInfo> accounts;
  accounts.reserve(account_ids_with_tokens.size());

  for (const std::string& account_id : account_ids_with_tokens) {
    accounts.push_back(GetAccountInfoForAccountWithRefreshToken(account_id));
  }

  return accounts;
}

std::vector<AccountInfo> IdentityManager::GetAccountsInCookieJar(
    const std::string& source) const {
  // TODO(859882): Change this implementation to interact asynchronously with
  // GaiaCookieManagerService as detailed in
  // https://docs.google.com/document/d/1hcrJ44facCSHtMGBmPusvcoP-fAR300Hi-UFez8ffYQ/edit?pli=1#heading=h.w97eil1cygs2.
  std::vector<gaia::ListedAccount> listed_accounts;
  gaia_cookie_manager_service_->ListAccounts(&listed_accounts, nullptr, source);

  return ListedAccountsToAccountInfos(listed_accounts);
}

bool IdentityManager::HasAccountWithRefreshToken(
    const std::string& account_id) const {
  return token_service_->RefreshTokenIsAvailable(account_id);
}

bool IdentityManager::HasAccountWithRefreshTokenInPersistentErrorState(
    const std::string& account_id) const {
  return GetErrorStateOfRefreshTokenForAccount(account_id).IsPersistentError();
}

GoogleServiceAuthError IdentityManager::GetErrorStateOfRefreshTokenForAccount(
    const std::string& account_id) const {
  return token_service_->GetAuthError(account_id);
}

bool IdentityManager::HasPrimaryAccountWithRefreshToken() const {
  return HasAccountWithRefreshToken(GetPrimaryAccountId());
}

std::unique_ptr<AccessTokenFetcher>
IdentityManager::CreateAccessTokenFetcherForAccount(
    const std::string& account_id,
    const std::string& oauth_consumer_name,
    const identity::ScopeSet& scopes,
    AccessTokenFetcher::TokenCallback callback,
    AccessTokenFetcher::Mode mode) {
  return std::make_unique<AccessTokenFetcher>(account_id, oauth_consumer_name,
                                              token_service_, scopes,
                                              std::move(callback), mode);
}

std::unique_ptr<AccessTokenFetcher>
IdentityManager::CreateAccessTokenFetcherForAccount(
    const std::string& account_id,
    const std::string& oauth_consumer_name,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const identity::ScopeSet& scopes,
    AccessTokenFetcher::TokenCallback callback,
    AccessTokenFetcher::Mode mode) {
  return std::make_unique<AccessTokenFetcher>(
      account_id, oauth_consumer_name, token_service_, url_loader_factory,
      scopes, std::move(callback), mode);
}

void IdentityManager::RemoveAccessTokenFromCache(
    const std::string& account_id,
    const identity::ScopeSet& scopes,
    const std::string& access_token) {
  // TODO(843510): Consider making the request to ProfileOAuth2TokenService
  // asynchronously once there are no direct clients of PO2TS. This change would
  // need to be made together with changing all callsites to
  // ProfileOAuth2TokenService::RequestAccessToken() to be made asynchronously
  // as well (to maintain ordering in the case where a client removes an access
  // token from the cache and then immediately requests an access token).
  token_service_->InvalidateAccessToken(account_id, scopes, access_token);
}

PrimaryAccountMutator* IdentityManager::GetPrimaryAccountMutator() {
  return primary_account_mutator_.get();
}

void IdentityManager::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void IdentityManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void IdentityManager::AddDiagnosticsObserver(DiagnosticsObserver* observer) {
  diagnostics_observer_list_.AddObserver(observer);
}

void IdentityManager::RemoveDiagnosticsObserver(DiagnosticsObserver* observer) {
  diagnostics_observer_list_.RemoveObserver(observer);
}

void IdentityManager::SetPrimaryAccountSynchronouslyForTests(
    const std::string& gaia_id,
    const std::string& email_address,
    const std::string& refresh_token) {
  DCHECK(!refresh_token.empty());
  SetPrimaryAccountSynchronously(gaia_id, email_address, refresh_token);
}

void IdentityManager::SetPrimaryAccountSynchronously(
    const std::string& gaia_id,
    const std::string& email_address,
    const std::string& refresh_token) {
  signin_manager_->SetAuthenticatedAccountInfo(gaia_id, email_address);

  if (!refresh_token.empty()) {
    token_service_->UpdateCredentials(GetPrimaryAccountId(), refresh_token);
  }
}

// Populates and returns an AccountInfo object corresponding to |account_id|,
// which must be an account with a refresh token.
AccountInfo IdentityManager::GetAccountInfoForAccountWithRefreshToken(
    std::string account_id) const {
  DCHECK(HasAccountWithRefreshToken(account_id));

  AccountInfo account_info =
      account_tracker_service_->GetAccountInfo(account_id);

  // In the context of supervised users, the ProfileOAuth2TokenService is used
  // without the AccountTrackerService being used. This is the only case in
  // which the AccountTrackerService will potentially not know about the
  // account. In this context, |account_id| is always set to
  // kSupervisedUserPseudoEmail. Populate the information manually in this case
  // to maintain the invariant that the account ID, gaia ID, and email are
  // always set.
  // TODO(860492): Remove this special case once supervised user support is
  // removed.
  DCHECK(!account_info.IsEmpty() || account_id == kSupervisedUserPseudoEmail);
  if (account_id == kSupervisedUserPseudoEmail && account_info.IsEmpty()) {
    account_info.account_id = account_id;
    account_info.email = kSupervisedUserPseudoEmail;
    account_info.gaia = kSupervisedUserPseudoGaiaID;
  }

  return account_info;
}

void IdentityManager::GoogleSigninSucceeded(const AccountInfo& account_info) {
  for (auto& observer : observer_list_) {
    observer.OnPrimaryAccountSet(account_info);
  }
}

void IdentityManager::GoogleSignedOut(const AccountInfo& account_info) {
  DCHECK(!HasPrimaryAccount());
  for (auto& observer : observer_list_) {
    observer.OnPrimaryAccountCleared(account_info);
  }
}

void IdentityManager::GoogleSigninFailed(const GoogleServiceAuthError& error) {
  for (auto& observer : observer_list_)
    observer.OnPrimaryAccountSigninFailed(error);
}

void IdentityManager::OnRefreshTokenAvailable(const std::string& account_id) {
  AccountInfo account_info =
      GetAccountInfoForAccountWithRefreshToken(account_id);

  // Compute the validity of the new refresh token: PO2TS sets an account's
  // refresh token to be invalid (error CREDENTIALS_REJECTED_BY_CLIENT) if the
  // user signs out of that account on the web.
  // TODO(blundell): Hide this logic inside PO2TS.
  bool is_valid = true;
  GoogleServiceAuthError token_error = token_service_->GetAuthError(account_id);
  if (token_error == GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                         GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                             CREDENTIALS_REJECTED_BY_CLIENT)) {
    is_valid = false;
  }

  for (auto& observer : observer_list_) {
    observer.OnRefreshTokenUpdatedForAccount(account_info, is_valid);
  }
}

void IdentityManager::OnRefreshTokenRevoked(const std::string& account_id) {
  for (auto& observer : observer_list_) {
    observer.OnRefreshTokenRemovedForAccount(account_id);
  }
}

void IdentityManager::OnRefreshTokensLoaded() {
  for (auto& observer : observer_list_)
    observer.OnRefreshTokensLoaded();
}

void IdentityManager::OnStartBatchChanges() {
  for (auto& observer : observer_list_)
    observer.OnStartBatchOfRefreshTokenStateChanges();
}

void IdentityManager::OnEndBatchChanges() {
  for (auto& observer : observer_list_)
    observer.OnEndBatchOfRefreshTokenStateChanges();
}

void IdentityManager::OnGaiaAccountsInCookieUpdated(
    const std::vector<gaia::ListedAccount>& accounts,
    const std::vector<gaia::ListedAccount>& signed_out_accounts,
    const GoogleServiceAuthError& error) {
  std::vector<AccountInfo> account_infos =
      ListedAccountsToAccountInfos(accounts);

  for (auto& observer : observer_list_) {
    observer.OnAccountsInCookieUpdated(account_infos);
  }
}

void IdentityManager::OnAccessTokenRequested(
    const std::string& account_id,
    const std::string& consumer_id,
    const OAuth2TokenService::ScopeSet& scopes) {
  // TODO(843510): Consider notifying observers asynchronously once there
  // are no direct clients of ProfileOAuth2TokenService.
  for (auto& observer : diagnostics_observer_list_) {
    observer.OnAccessTokenRequested(account_id, consumer_id, scopes);
  }
}

}  // namespace identity
