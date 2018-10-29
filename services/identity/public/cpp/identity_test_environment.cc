// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/public/cpp/identity_test_environment.h"

#include "build/build_config.h"

#include "base/run_loop.h"
#include "components/signin/core/browser/profile_management_switches.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "services/identity/public/cpp/identity_test_utils.h"
#include "services/identity/public/cpp/primary_account_mutator.h"

#if !defined(OS_CHROMEOS)
#include "services/identity/public/cpp/primary_account_mutator_impl.h"
#endif

namespace identity {

class IdentityManagerDependenciesOwner {
 public:
  IdentityManagerDependenciesOwner(
      bool use_fake_url_loader_for_gaia_cookie_manager);
  ~IdentityManagerDependenciesOwner();

  AccountTrackerService* account_tracker_service();

  SigninManagerForTest* signin_manager();

  FakeProfileOAuth2TokenService* token_service();

  FakeGaiaCookieManagerService* gaia_cookie_manager_service();

 private:
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  AccountTrackerService account_tracker_;
  TestSigninClient signin_client_;
  FakeProfileOAuth2TokenService token_service_;
  SigninManagerForTest signin_manager_;
  FakeGaiaCookieManagerService gaia_cookie_manager_service_;

  DISALLOW_COPY_AND_ASSIGN(IdentityManagerDependenciesOwner);
};

IdentityManagerDependenciesOwner::IdentityManagerDependenciesOwner(
    bool use_fake_url_loader_for_gaia_cookie_manager)
    : signin_client_(&pref_service_),
      token_service_(&pref_service_),
#if defined(OS_CHROMEOS)
      signin_manager_(&signin_client_, &account_tracker_),
#else
      signin_manager_(&signin_client_,
                      &token_service_,
                      &account_tracker_,
                      nullptr),
#endif
      // NOTE: Some unittests set up their own TestURLFetcherFactory. In these
      // contexts FakeGaiaCookieManagerService can't set up its own
      // FakeURLFetcherFactory, as {Test, Fake}URLFetcherFactory allow only one
      // instance to be alive at a time. If some users require that
      // GaiaCookieManagerService have a FakeURLFetcherFactory while *also*
      // having their own FakeURLFetcherFactory, we'll need to pass the actual
      // object in and have GaiaCookieManagerService have a reference to the
      // object (or figure out the sharing some other way). Contact
      // blundell@chromium.org if you come up against this issue.
      gaia_cookie_manager_service_(
          &token_service_,
          "identity_test_environment",
          &signin_client_,
          use_fake_url_loader_for_gaia_cookie_manager) {
  AccountTrackerService::RegisterPrefs(pref_service_.registry());
  ProfileOAuth2TokenService::RegisterProfilePrefs(pref_service_.registry());
  SigninManagerBase::RegisterProfilePrefs(pref_service_.registry());
  SigninManagerBase::RegisterPrefs(pref_service_.registry());

  account_tracker_.Initialize(&pref_service_, base::FilePath());

}

IdentityManagerDependenciesOwner::~IdentityManagerDependenciesOwner() {}

AccountTrackerService*
IdentityManagerDependenciesOwner::account_tracker_service() {
  return &account_tracker_;
}

SigninManagerForTest* IdentityManagerDependenciesOwner::signin_manager() {
  return &signin_manager_;
}

FakeProfileOAuth2TokenService*
IdentityManagerDependenciesOwner::token_service() {
  return &token_service_;
}

FakeGaiaCookieManagerService*
IdentityManagerDependenciesOwner::gaia_cookie_manager_service() {
  return &gaia_cookie_manager_service_;
}

IdentityTestEnvironment::IdentityTestEnvironment(
    bool use_fake_url_loader_for_gaia_cookie_manager)
    : IdentityTestEnvironment(
          /*account_tracker_service=*/nullptr,
          /*token_service=*/nullptr,
          /*signin_manager=*/nullptr,
          /*gaia_cookie_manager_service=*/nullptr,
          std::make_unique<IdentityManagerDependenciesOwner>(
              use_fake_url_loader_for_gaia_cookie_manager),
          /*identity_manager=*/nullptr) {}

IdentityTestEnvironment::IdentityTestEnvironment(
    AccountTrackerService* account_tracker_service,
    FakeProfileOAuth2TokenService* token_service,
    SigninManagerForTest* signin_manager,
    FakeGaiaCookieManagerService* gaia_cookie_manager_service)
    : IdentityTestEnvironment(account_tracker_service,
                              token_service,
                              signin_manager,
                              gaia_cookie_manager_service,
                              /*dependency_owner=*/nullptr,
                              /*identity_manager=*/nullptr) {}

IdentityTestEnvironment::IdentityTestEnvironment(
    AccountTrackerService* account_tracker_service,
    FakeProfileOAuth2TokenService* token_service,
    SigninManagerForTest* signin_manager,
    FakeGaiaCookieManagerService* gaia_cookie_manager_service,
    IdentityManager* identity_manager)
    : IdentityTestEnvironment(account_tracker_service,
                              token_service,
                              signin_manager,
                              gaia_cookie_manager_service,
                              /*dependency_owner=*/nullptr,
                              identity_manager) {}

IdentityTestEnvironment::IdentityTestEnvironment(
    AccountTrackerService* account_tracker_service,
    FakeProfileOAuth2TokenService* token_service,
    SigninManagerForTest* signin_manager,
    FakeGaiaCookieManagerService* gaia_cookie_manager_service,
    std::unique_ptr<IdentityManagerDependenciesOwner> dependencies_owner,
    IdentityManager* identity_manager) {
  if (dependencies_owner) {
    DCHECK(!(account_tracker_service || token_service || signin_manager ||
             gaia_cookie_manager_service || identity_manager));

    dependencies_owner_ = std::move(dependencies_owner);

    account_tracker_service_ = dependencies_owner_->account_tracker_service();
    token_service_ = dependencies_owner_->token_service();
    signin_manager_ = dependencies_owner_->signin_manager();
    gaia_cookie_manager_service_ =
        dependencies_owner_->gaia_cookie_manager_service();

  } else {
    DCHECK(account_tracker_service && token_service && signin_manager &&
           gaia_cookie_manager_service);

    account_tracker_service_ = account_tracker_service;
    token_service_ = token_service;
    signin_manager_ = signin_manager;
    gaia_cookie_manager_service_ = gaia_cookie_manager_service;
  }

  if (identity_manager) {
    raw_identity_manager_ = identity_manager;
  } else {
#if !defined(OS_CHROMEOS)
    std::unique_ptr<PrimaryAccountMutator> account_mutator =
        std::make_unique<PrimaryAccountMutatorImpl>(
            account_tracker_service_,
            static_cast<SigninManager*>(signin_manager_));
#else
    std::unique_ptr<PrimaryAccountMutator> account_mutator;
#endif

    owned_identity_manager_ = std::make_unique<IdentityManager>(
        signin_manager_, token_service_, account_tracker_service_,
        gaia_cookie_manager_service_, std::move(account_mutator));
  }

  this->identity_manager()->AddDiagnosticsObserver(this);
}

IdentityTestEnvironment::~IdentityTestEnvironment() {
  identity_manager()->RemoveDiagnosticsObserver(this);
}

IdentityManager* IdentityTestEnvironment::identity_manager() {
  DCHECK(raw_identity_manager_ || owned_identity_manager_);
  DCHECK(!(raw_identity_manager_ && owned_identity_manager_));

  return raw_identity_manager_ ? raw_identity_manager_
                               : owned_identity_manager_.get();
}

AccountInfo IdentityTestEnvironment::SetPrimaryAccount(
    const std::string& email) {
  return identity::SetPrimaryAccount(signin_manager_, identity_manager(),
                                     email);
}

void IdentityTestEnvironment::SetRefreshTokenForPrimaryAccount() {
  identity::SetRefreshTokenForPrimaryAccount(token_service_,
                                             identity_manager());
}

void IdentityTestEnvironment::SetInvalidRefreshTokenForPrimaryAccount() {
  identity::SetInvalidRefreshTokenForPrimaryAccount(token_service_,
                                                    identity_manager());
}

void IdentityTestEnvironment::RemoveRefreshTokenForPrimaryAccount() {
  identity::RemoveRefreshTokenForPrimaryAccount(token_service_,
                                                identity_manager());
}

AccountInfo IdentityTestEnvironment::MakePrimaryAccountAvailable(
    const std::string& email) {
  return identity::MakePrimaryAccountAvailable(signin_manager_, token_service_,
                                               identity_manager(), email);
}

void IdentityTestEnvironment::ClearPrimaryAccount(
    ClearPrimaryAccountPolicy policy) {
  identity::ClearPrimaryAccount(signin_manager_, identity_manager(), policy);
}

AccountInfo IdentityTestEnvironment::MakeAccountAvailable(
    const std::string& email) {
  return identity::MakeAccountAvailable(
      account_tracker_service_, token_service_, identity_manager(), email);
}

void IdentityTestEnvironment::SetRefreshTokenForAccount(
    const std::string& account_id) {
  return identity::SetRefreshTokenForAccount(token_service_, identity_manager(),
                                             account_id);
}

void IdentityTestEnvironment::SetInvalidRefreshTokenForAccount(
    const std::string& account_id) {
  return identity::SetInvalidRefreshTokenForAccount(
      token_service_, identity_manager(), account_id);
}

void IdentityTestEnvironment::RemoveRefreshTokenForAccount(
    const std::string& account_id) {
  return identity::RemoveRefreshTokenForAccount(token_service_,
                                                identity_manager(), account_id);
}

void IdentityTestEnvironment::SetCookieAccounts(
    const std::vector<CookieParams>& cookie_accounts) {
  identity::SetCookieAccounts(gaia_cookie_manager_service_, identity_manager(),
                              cookie_accounts);
}

void IdentityTestEnvironment::SetAutomaticIssueOfAccessTokens(bool grant) {
  token_service_->set_auto_post_fetch_response_on_message_loop(grant);
}

void IdentityTestEnvironment::
    WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
        const std::string& token,
        const base::Time& expiration) {
  WaitForAccessTokenRequestIfNecessary(base::nullopt);
  token_service_->IssueTokenForAllPendingRequests(token, expiration);
}

void IdentityTestEnvironment::
    WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
        const std::string& token,
        const base::Time& expiration,
        const std::string& id_token) {
  WaitForAccessTokenRequestIfNecessary(base::nullopt);
  token_service_->IssueTokenForAllPendingRequests(
      OAuth2AccessTokenConsumer::TokenResponse(token, expiration, id_token));
}

void IdentityTestEnvironment::
    WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
        const std::string& account_id,
        const std::string& token,
        const base::Time& expiration) {
  WaitForAccessTokenRequestIfNecessary(account_id);
  token_service_->IssueAllTokensForAccount(account_id, token, expiration);
}

void IdentityTestEnvironment::
    WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
        const GoogleServiceAuthError& error) {
  WaitForAccessTokenRequestIfNecessary(base::nullopt);
  token_service_->IssueErrorForAllPendingRequests(error);
}

void IdentityTestEnvironment::
    WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
        const std::string& account_id,
        const GoogleServiceAuthError& error) {
  WaitForAccessTokenRequestIfNecessary(account_id);
  token_service_->IssueErrorForAllPendingRequestsForAccount(account_id, error);
}

void IdentityTestEnvironment::SetCallbackForNextAccessTokenRequest(
    base::OnceClosure callback) {
  on_access_token_requested_callback_ = std::move(callback);
}

IdentityTestEnvironment::AccessTokenRequestState::AccessTokenRequestState() =
    default;
IdentityTestEnvironment::AccessTokenRequestState::~AccessTokenRequestState() =
    default;
IdentityTestEnvironment::AccessTokenRequestState::AccessTokenRequestState(
    AccessTokenRequestState&& other) = default;
IdentityTestEnvironment::AccessTokenRequestState&
IdentityTestEnvironment::AccessTokenRequestState::operator=(
    AccessTokenRequestState&& other) = default;

void IdentityTestEnvironment::OnAccessTokenRequested(
    const std::string& account_id,
    const std::string& consumer_id,
    const identity::ScopeSet& scopes) {
  // Post a task to handle this access token request in order to support the
  // case where the access token request is handled synchronously in the
  // production code, in which case this callback could be coming in ahead
  // of an invocation of WaitForAccessTokenRequestIfNecessary() that will be
  // made in this same iteration of the run loop.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&IdentityTestEnvironment::HandleOnAccessTokenRequested,
                     base::Unretained(this), account_id));
}

void IdentityTestEnvironment::HandleOnAccessTokenRequested(
    std::string account_id) {
  if (on_access_token_requested_callback_) {
    std::move(on_access_token_requested_callback_).Run();
    return;
  }

  for (auto it = requesters_.begin(); it != requesters_.end(); ++it) {
    if (!it->account_id || (it->account_id.value() == account_id)) {
      if (it->state == AccessTokenRequestState::kAvailable)
        return;
      if (it->on_available)
        std::move(it->on_available).Run();
      requesters_.erase(it);
      return;
    }
  }

  // A requests came in for a request for which we are not waiting. Record that
  // it's available.
  requesters_.emplace_back();
  requesters_.back().state = AccessTokenRequestState::kAvailable;
  requesters_.back().account_id = account_id;
}

void IdentityTestEnvironment::WaitForAccessTokenRequestIfNecessary(
    base::Optional<std::string> account_id) {
  // Handle HandleOnAccessTokenRequested getting called before
  // WaitForAccessTokenRequestIfNecessary.
  if (account_id) {
    for (auto it = requesters_.begin(); it != requesters_.end(); ++it) {
      if (it->account_id && it->account_id.value() == account_id.value()) {
        // Can't wait twice for same thing.
        DCHECK_EQ(AccessTokenRequestState::kAvailable, it->state);
        requesters_.erase(it);
        return;
      }
    }
  } else {
    for (auto it = requesters_.begin(); it != requesters_.end(); ++it) {
      if (it->state == AccessTokenRequestState::kAvailable) {
        requesters_.erase(it);
        return;
      }
    }
  }

  base::RunLoop run_loop;
  requesters_.emplace_back();
  requesters_.back().state = AccessTokenRequestState::kPending;
  requesters_.back().account_id = std::move(account_id);
  requesters_.back().on_available = run_loop.QuitClosure();
  run_loop.Run();
}

}  // namespace identity
