// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/public/cpp/identity_test_utils.h"

#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_gaia_cookie_manager_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "services/identity/public/cpp/identity_manager.h"

namespace identity {

namespace {

enum class IdentityManagerEvent {
  PRIMARY_ACCOUNT_SET,
  PRIMARY_ACCOUNT_CLEARED,
  REFRESH_TOKEN_UPDATED,
  REFRESH_TOKEN_REMOVED,
  ACCOUNTS_IN_COOKIE_UPDATED,
};

class OneShotIdentityManagerObserver : public IdentityManager::Observer {
 public:
  OneShotIdentityManagerObserver(IdentityManager* identity_manager,
                                 base::OnceClosure done_closure,
                                 IdentityManagerEvent event_to_wait_on);
  ~OneShotIdentityManagerObserver() override;

 private:
  // IdentityManager::Observer:
  void OnPrimaryAccountSet(const AccountInfo& primary_account_info) override;
  void OnPrimaryAccountCleared(
      const AccountInfo& previous_primary_account_info) override;
  void OnRefreshTokenUpdatedForAccount(const AccountInfo& account_info,
                                       bool is_valid) override;
  void OnRefreshTokenRemovedForAccount(const std::string& account_id) override;
  void OnAccountsInCookieUpdated(
      const std::vector<AccountInfo>& accounts) override;

  IdentityManager* identity_manager_;
  base::OnceClosure done_closure_;
  IdentityManagerEvent event_to_wait_on_;

  DISALLOW_COPY_AND_ASSIGN(OneShotIdentityManagerObserver);
};

OneShotIdentityManagerObserver::OneShotIdentityManagerObserver(
    IdentityManager* identity_manager,
    base::OnceClosure done_closure,
    IdentityManagerEvent event_to_wait_on)
    : identity_manager_(identity_manager),
      done_closure_(std::move(done_closure)),
      event_to_wait_on_(event_to_wait_on) {
  identity_manager_->AddObserver(this);
}

OneShotIdentityManagerObserver::~OneShotIdentityManagerObserver() {
  identity_manager_->RemoveObserver(this);
}

void OneShotIdentityManagerObserver::OnPrimaryAccountSet(
    const AccountInfo& primary_account_info) {
  if (event_to_wait_on_ != IdentityManagerEvent::PRIMARY_ACCOUNT_SET)
    return;

  DCHECK(done_closure_);
  std::move(done_closure_).Run();
}

void OneShotIdentityManagerObserver::OnPrimaryAccountCleared(
    const AccountInfo& previous_primary_account_info) {
  if (event_to_wait_on_ != IdentityManagerEvent::PRIMARY_ACCOUNT_CLEARED)
    return;

  DCHECK(done_closure_);
  std::move(done_closure_).Run();
}

void OneShotIdentityManagerObserver::OnRefreshTokenUpdatedForAccount(
    const AccountInfo& account_info,
    bool is_valid) {
  if (event_to_wait_on_ != IdentityManagerEvent::REFRESH_TOKEN_UPDATED)
    return;

  DCHECK(done_closure_);
  std::move(done_closure_).Run();
}

void OneShotIdentityManagerObserver::OnRefreshTokenRemovedForAccount(
    const std::string& account_id) {
  if (event_to_wait_on_ != IdentityManagerEvent::REFRESH_TOKEN_REMOVED)
    return;

  DCHECK(done_closure_);
  std::move(done_closure_).Run();
}

void OneShotIdentityManagerObserver::OnAccountsInCookieUpdated(
    const std::vector<AccountInfo>& accounts) {
  if (event_to_wait_on_ != IdentityManagerEvent::ACCOUNTS_IN_COOKIE_UPDATED)
    return;

  DCHECK(done_closure_);
  std::move(done_closure_).Run();
}

// Helper function that updates the refresh token for |account_id| to
// |new_token|. Blocks until the update is processed by |identity_manager|.
void UpdateRefreshTokenForAccount(ProfileOAuth2TokenService* token_service,
                                  IdentityManager* identity_manager,
                                  const std::string& account_id,
                                  const std::string& new_token) {
  base::RunLoop run_loop;
  OneShotIdentityManagerObserver token_updated_observer(
      identity_manager, run_loop.QuitClosure(),
      IdentityManagerEvent::REFRESH_TOKEN_UPDATED);

  token_service->UpdateCredentials(account_id, new_token);

  run_loop.Run();
}

}  // namespace

AccountInfo SetPrimaryAccount(SigninManagerBase* signin_manager,
                              IdentityManager* identity_manager,
                              const std::string& email) {
  DCHECK(!signin_manager->IsAuthenticated());
  DCHECK(!identity_manager->HasPrimaryAccount());
  std::string gaia_id = "gaia_id_for_" + email;

#if defined(OS_CHROMEOS)
  // ChromeOS has no real notion of signin, so just plumb the information
  // through (note: supply an empty string as the refresh token so that no
  // refresh token is set).
  identity_manager->SetPrimaryAccountSynchronously(gaia_id, email,
                                                   /*refresh_token=*/"");
#else

  base::RunLoop run_loop;
  OneShotIdentityManagerObserver signin_observer(
      identity_manager, run_loop.QuitClosure(),
      IdentityManagerEvent::PRIMARY_ACCOUNT_SET);

  SigninManager* real_signin_manager =
      SigninManager::FromSigninManagerBase(signin_manager);
  // Note: It's important to pass base::DoNothing() (rather than a null
  // callback) to make this work with both SigninManager and FakeSigninManager.
  // If we would pass a null callback, then SigninManager would call
  // CompletePendingSignin directly, but FakeSigninManager never does that.
  // Note: pass an empty string as the refresh token so that no refresh token is
  // set.
  real_signin_manager->StartSignInWithRefreshToken(
      /*refresh_token=*/"", gaia_id, email, /*password=*/"",
      /*oauth_fetched_callback=*/base::DoNothing());
  real_signin_manager->CompletePendingSignin();

  run_loop.Run();
#endif

  DCHECK(signin_manager->IsAuthenticated());
  DCHECK(identity_manager->HasPrimaryAccount());
  return identity_manager->GetPrimaryAccountInfo();
}

void SetRefreshTokenForPrimaryAccount(ProfileOAuth2TokenService* token_service,
                                      IdentityManager* identity_manager) {
  DCHECK(identity_manager->HasPrimaryAccount());
  std::string account_id = identity_manager->GetPrimaryAccountId();

  std::string refresh_token = "refresh_token_for_" + account_id;
  SetRefreshTokenForAccount(token_service, identity_manager, account_id);
}

void SetInvalidRefreshTokenForPrimaryAccount(
    ProfileOAuth2TokenService* token_service,
    IdentityManager* identity_manager) {
  DCHECK(identity_manager->HasPrimaryAccount());
  std::string account_id = identity_manager->GetPrimaryAccountId();

  SetInvalidRefreshTokenForAccount(token_service, identity_manager, account_id);
}

void RemoveRefreshTokenForPrimaryAccount(
    ProfileOAuth2TokenService* token_service,
    IdentityManager* identity_manager) {
  if (!identity_manager->HasPrimaryAccount())
    return;

  std::string account_id = identity_manager->GetPrimaryAccountId();

  RemoveRefreshTokenForAccount(token_service, identity_manager, account_id);
}

AccountInfo MakePrimaryAccountAvailable(
    SigninManagerBase* signin_manager,
    ProfileOAuth2TokenService* token_service,
    IdentityManager* identity_manager,
    const std::string& email) {
  AccountInfo account_info =
      SetPrimaryAccount(signin_manager, identity_manager, email);
  SetRefreshTokenForPrimaryAccount(token_service, identity_manager);
  return account_info;
}

void ClearPrimaryAccount(SigninManagerBase* signin_manager,
                         IdentityManager* identity_manager,
                         ClearPrimaryAccountPolicy policy) {
#if defined(OS_CHROMEOS)
  // TODO(blundell): If we ever need this functionality on ChromeOS (which seems
  // unlikely), plumb this through to just clear the primary account info
  // synchronously with IdentityManager.
  NOTREACHED();
#else
  if (!identity_manager->HasPrimaryAccount())
    return;

  base::RunLoop run_loop;
  OneShotIdentityManagerObserver signout_observer(
      identity_manager, run_loop.QuitClosure(),
      IdentityManagerEvent::PRIMARY_ACCOUNT_CLEARED);

  SigninManager* real_signin_manager =
      SigninManager::FromSigninManagerBase(signin_manager);
  signin_metrics::ProfileSignout signout_source_metric =
      signin_metrics::SIGNOUT_TEST;
  signin_metrics::SignoutDelete signout_delete_metric =
      signin_metrics::SignoutDelete::IGNORE_METRIC;

  switch (policy) {
    case ClearPrimaryAccountPolicy::DEFAULT:
      real_signin_manager->SignOut(signout_source_metric,
                                   signout_delete_metric);
      break;
    case ClearPrimaryAccountPolicy::KEEP_ALL_ACCOUNTS:
      real_signin_manager->SignOutAndKeepAllAccounts(signout_source_metric,
                                                     signout_delete_metric);
      break;
    case ClearPrimaryAccountPolicy::REMOVE_ALL_ACCOUNTS:
      real_signin_manager->SignOutAndRemoveAllAccounts(signout_source_metric,
                                                       signout_delete_metric);
      break;
  }

  run_loop.Run();
#endif
}

AccountInfo MakeAccountAvailable(AccountTrackerService* account_tracker_service,
                                 ProfileOAuth2TokenService* token_service,
                                 IdentityManager* identity_manager,
                                 const std::string& email) {
  DCHECK(account_tracker_service->FindAccountInfoByEmail(email).IsEmpty());

  std::string gaia_id = "gaia_id_for_" + email;
  account_tracker_service->SeedAccountInfo(gaia_id, email);

  AccountInfo account_info =
      account_tracker_service->FindAccountInfoByEmail(email);
  DCHECK(!account_info.account_id.empty());

  SetRefreshTokenForAccount(token_service, identity_manager,
                            account_info.account_id);

  return account_info;
}

void SetRefreshTokenForAccount(ProfileOAuth2TokenService* token_service,
                               IdentityManager* identity_manager,
                               const std::string& account_id) {
  std::string refresh_token = "refresh_token_for_" + account_id;
  UpdateRefreshTokenForAccount(token_service, identity_manager, account_id,
                               refresh_token);
}

void SetInvalidRefreshTokenForAccount(ProfileOAuth2TokenService* token_service,
                                      IdentityManager* identity_manager,
                                      const std::string& account_id) {
  UpdateRefreshTokenForAccount(
      token_service, identity_manager, account_id,
      OAuth2TokenServiceDelegate::kInvalidRefreshToken);
}

void RemoveRefreshTokenForAccount(ProfileOAuth2TokenService* token_service,
                                  IdentityManager* identity_manager,
                                  const std::string& account_id) {
  if (!identity_manager->HasAccountWithRefreshToken(account_id))
    return;

  base::RunLoop run_loop;
  OneShotIdentityManagerObserver token_updated_observer(
      identity_manager, run_loop.QuitClosure(),
      IdentityManagerEvent::REFRESH_TOKEN_REMOVED);

  token_service->RevokeCredentials(account_id);

  run_loop.Run();
}

void SetCookieAccounts(FakeGaiaCookieManagerService* cookie_manager,
                       IdentityManager* identity_manager,
                       const std::vector<CookieParams>& cookie_accounts) {
  // Convert |cookie_accounts| to the format FakeGaiaCookieManagerService wants.
  std::vector<FakeGaiaCookieManagerService::CookieParams> gaia_cookie_accounts;
  for (const CookieParams& params : cookie_accounts) {
    gaia_cookie_accounts.push_back({params.email, params.gaia_id,
                                    /*valid=*/true, /*signed_out=*/false,
                                    /*verified=*/true});
  }

  base::RunLoop run_loop;
  OneShotIdentityManagerObserver cookie_observer(
      identity_manager, run_loop.QuitClosure(),
      IdentityManagerEvent::ACCOUNTS_IN_COOKIE_UPDATED);

  cookie_manager->SetListAccountsResponseWithParams(gaia_cookie_accounts);

  cookie_manager->set_list_accounts_stale_for_testing(true);
  cookie_manager->ListAccounts(nullptr, nullptr, "test");

  run_loop.Run();
}

}  // namespace identity
