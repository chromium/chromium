// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/public/cpp/identity_manager.h"
#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_gaia_cookie_manager_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/signin/core/browser/profile_management_switches.h"
#include "components/signin/core/browser/signin_switches.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_token_service_delegate.h"
#include "services/identity/public/cpp/identity_test_utils.h"
#include "services/identity/public/cpp/primary_account_mutator.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace identity {
namespace {

#if defined(OS_CHROMEOS)
using SigninManagerForTest = FakeSigninManagerBase;
#else
using SigninManagerForTest = FakeSigninManager;
#endif  // OS_CHROMEOS

const char kTestGaiaId[] = "dummyId";
const char kTestGaiaId2[] = "dummyId2";
const char kTestGaiaId3[] = "dummyId3";
const char kTestEmail[] = "me@gmail.com";
const char kTestEmail2[] = "me2@gmail.com";
const char kTestEmail3[] = "me3@gmail.com";

#if defined(OS_CHROMEOS)
const char kTestEmailWithPeriod[] = "m.e@gmail.com";
#endif

// Subclass of FakeProfileOAuth2TokenService with bespoke behavior.
class CustomFakeProfileOAuth2TokenService
    : public FakeProfileOAuth2TokenService {
 public:
  CustomFakeProfileOAuth2TokenService(PrefService* user_prefs)
      : FakeProfileOAuth2TokenService(user_prefs) {}

  void set_on_access_token_invalidated_info(
      std::string expected_account_id_to_invalidate,
      std::set<std::string> expected_scopes_to_invalidate,
      std::string expected_access_token_to_invalidate,
      base::OnceClosure callback) {
    expected_account_id_to_invalidate_ = expected_account_id_to_invalidate;
    expected_scopes_to_invalidate_ = expected_scopes_to_invalidate;
    expected_access_token_to_invalidate_ = expected_access_token_to_invalidate;
    on_access_token_invalidated_callback_ = std::move(callback);
  }

 private:
  // OAuth2TokenService:
  void InvalidateAccessTokenImpl(const std::string& account_id,
                                 const std::string& client_id,
                                 const ScopeSet& scopes,
                                 const std::string& access_token) override {
    if (on_access_token_invalidated_callback_) {
      EXPECT_EQ(expected_account_id_to_invalidate_, account_id);
      EXPECT_EQ(expected_scopes_to_invalidate_, scopes);
      EXPECT_EQ(expected_access_token_to_invalidate_, access_token);
      std::move(on_access_token_invalidated_callback_).Run();
    }
  }

  std::string expected_account_id_to_invalidate_;
  std::set<std::string> expected_scopes_to_invalidate_;
  std::string expected_access_token_to_invalidate_;
  base::OnceClosure on_access_token_invalidated_callback_;
};

class AccountTrackerServiceForTest : public AccountTrackerService {
 public:
  void SetAccountStateFromUserInfo(const std::string& account_id,
                                   const base::DictionaryValue* user_info) {
    AccountTrackerService::SetAccountStateFromUserInfo(account_id, user_info);
  }
};

class TestSigninManagerObserver : public SigninManagerBase::Observer {
 public:
  explicit TestSigninManagerObserver(SigninManagerBase* signin_manager)
      : signin_manager_(signin_manager) {
    signin_manager_->AddObserver(this);
  }
  ~TestSigninManagerObserver() override {
    signin_manager_->RemoveObserver(this);
  }

  void set_identity_manager(IdentityManager* identity_manager) {
    identity_manager_ = identity_manager;
  }

  void set_on_google_signin_succeeded_callback(base::OnceClosure callback) {
    on_google_signin_succeeded_callback_ = std::move(callback);
  }
  void set_on_google_signed_out_callback(base::OnceClosure callback) {
    on_google_signed_out_callback_ = std::move(callback);
  }

  const AccountInfo& primary_account_from_signin_callback() const {
    return primary_account_from_signin_callback_;
  }
  const AccountInfo& primary_account_from_signout_callback() const {
    return primary_account_from_signout_callback_;
  }

 private:
  // SigninManager::Observer:
  void GoogleSigninSucceeded(const AccountInfo& account_info) override {
    ASSERT_TRUE(identity_manager_);
    primary_account_from_signin_callback_ =
        identity_manager_->GetPrimaryAccountInfo();
    if (on_google_signin_succeeded_callback_)
      std::move(on_google_signin_succeeded_callback_).Run();
  }
  void GoogleSignedOut(const AccountInfo& account_info) override {
    ASSERT_TRUE(identity_manager_);
    primary_account_from_signout_callback_ =
        identity_manager_->GetPrimaryAccountInfo();
    if (on_google_signed_out_callback_)
      std::move(on_google_signed_out_callback_).Run();
  }

  SigninManagerBase* signin_manager_;
  IdentityManager* identity_manager_;
  base::OnceClosure on_google_signin_succeeded_callback_;
  base::OnceClosure on_google_signin_failed_callback_;
  base::OnceClosure on_google_signed_out_callback_;
  AccountInfo primary_account_from_signin_callback_;
  AccountInfo primary_account_from_signout_callback_;
};

// Class that observes updates from ProfileOAuth2TokenService and and verifies
// thereby that IdentityManager receives updates before direct observers of
// ProfileOAuth2TokenService.
class TestTokenServiceObserver : public OAuth2TokenService::Observer,
                                 public identity::IdentityManager::Observer {
 public:
  explicit TestTokenServiceObserver(OAuth2TokenService* token_service)
      : token_service_(token_service) {
    token_service_->AddObserver(this);
  }
  ~TestTokenServiceObserver() override {
    token_service_->RemoveObserver(this);
  }

  void set_identity_manager(IdentityManager* identity_manager) {
    identity_manager_ = identity_manager;
  }

  void set_on_refresh_token_available_callback(base::OnceClosure callback) {
    on_refresh_token_available_callback_ = std::move(callback);
  }
  void set_on_refresh_token_revoked_callback(base::OnceClosure callback) {
    on_refresh_token_revoked_callback_ = std::move(callback);
  }

 private:
  // OAuth2TokenService::Observer:
  void OnRefreshTokenAvailable(const std::string& account_id) override {
    // IdentityManager should have already updated its state.
    EXPECT_TRUE(identity_manager_->HasAccountWithRefreshToken(account_id));
    if (on_refresh_token_available_callback_)
      std::move(on_refresh_token_available_callback_).Run();
  }
  void OnRefreshTokenRevoked(const std::string& account_id) override {
    // IdentityManager should have already updated its state.
    EXPECT_FALSE(identity_manager_->HasAccountWithRefreshToken(account_id));
    if (on_refresh_token_revoked_callback_)
      std::move(on_refresh_token_revoked_callback_).Run();
  }

  OAuth2TokenService* token_service_;
  IdentityManager* identity_manager_;
  base::OnceClosure on_refresh_token_available_callback_;
  base::OnceClosure on_refresh_token_revoked_callback_;
};

class TestIdentityManagerObserver : IdentityManager::Observer {
 public:
  explicit TestIdentityManagerObserver(IdentityManager* identity_manager)
      : identity_manager_(identity_manager) {
    identity_manager_->AddObserver(this);
  }
  ~TestIdentityManagerObserver() override {
    identity_manager_->RemoveObserver(this);
  }

  void set_on_primary_account_set_callback(base::OnceClosure callback) {
    on_primary_account_set_callback_ = std::move(callback);
  }
  void set_on_primary_account_cleared_callback(base::OnceClosure callback) {
    on_primary_account_cleared_callback_ = std::move(callback);
  }
  void set_on_primary_account_signin_failed_callback(
      base::OnceClosure callback) {
    on_primary_account_signin_failed_callback_ = std::move(callback);
  }

  const AccountInfo& primary_account_from_set_callback() {
    return primary_account_from_set_callback_;
  }
  const AccountInfo& primary_account_from_cleared_callback() {
    return primary_account_from_cleared_callback_;
  }

  void set_on_refresh_token_updated_callback(base::OnceClosure callback) {
    on_refresh_token_updated_callback_ = std::move(callback);
  }
  // This method uses a RepeatingCallback to simplify verification of multiple
  // removed tokens.
  void set_on_refresh_token_removed_callback(
      base::RepeatingCallback<void(const std::string&)> callback) {
    on_refresh_token_removed_callback_ = std::move(callback);
  }

  void set_on_refresh_tokens_loaded_callback(base::OnceClosure callback) {
    on_refresh_tokens_loaded_callback_ = std::move(callback);
  }

  const AccountInfo& account_from_refresh_token_updated_callback() {
    return account_from_refresh_token_updated_callback_;
  }
  bool validity_from_refresh_token_updated_callback() {
    return validity_from_refresh_token_updated_callback_;
  }
  const std::string& account_from_refresh_token_removed_callback() {
    return account_from_refresh_token_removed_callback_;
  }

  void set_on_accounts_in_cookie_updated_callback(base::OnceClosure callback) {
    on_accounts_in_cookie_updated_callback_ = std::move(callback);
  }

  const std::vector<AccountInfo>& accounts_from_cookie_change_callback() {
    return accounts_from_cookie_change_callback_;
  }

  const GoogleServiceAuthError& error_from_signin_failed_callback() const {
    return google_signin_failed_error_;
  }

  // Each element represents all the changes from an individual batch that has
  // occurred, with the elements ordered from oldest to newest batch occurrence.
  const std::vector<std::vector<std::string>>& batch_change_records() const {
    return batch_change_records_;
  }

 private:
  // IdentityManager::Observer:
  void OnPrimaryAccountSet(const AccountInfo& primary_account_info) override {
    primary_account_from_set_callback_ = primary_account_info;
    if (on_primary_account_set_callback_)
      std::move(on_primary_account_set_callback_).Run();
  }
  void OnPrimaryAccountCleared(
      const AccountInfo& previous_primary_account_info) override {
    primary_account_from_cleared_callback_ = previous_primary_account_info;
    if (on_primary_account_cleared_callback_)
      std::move(on_primary_account_cleared_callback_).Run();
  }
  void OnPrimaryAccountSigninFailed(
      const GoogleServiceAuthError& error) override {
    google_signin_failed_error_ = error;
    if (on_primary_account_signin_failed_callback_)
      std::move(on_primary_account_signin_failed_callback_).Run();
  }
  void OnRefreshTokenUpdatedForAccount(const AccountInfo& account_info,
                                       bool is_valid) override {
    EXPECT_TRUE(is_inside_batch_);
    batch_change_records_.rbegin()->emplace_back(account_info.account_id);
    account_from_refresh_token_updated_callback_ = account_info;
    validity_from_refresh_token_updated_callback_ = is_valid;
    if (on_refresh_token_updated_callback_)
      std::move(on_refresh_token_updated_callback_).Run();
  }
  void OnRefreshTokenRemovedForAccount(const std::string& account_id) override {
    EXPECT_TRUE(is_inside_batch_);
    batch_change_records_.rbegin()->emplace_back(account_id);
    account_from_refresh_token_removed_callback_ = account_id;
    if (on_refresh_token_removed_callback_)
      on_refresh_token_removed_callback_.Run(account_id);
  }
  void OnRefreshTokensLoaded() override {
    if (on_refresh_tokens_loaded_callback_)
      std::move(on_refresh_tokens_loaded_callback_).Run();
  }
  void OnAccountsInCookieUpdated(
      const std::vector<AccountInfo>& accounts) override {
    accounts_from_cookie_change_callback_ = accounts;
    if (on_accounts_in_cookie_updated_callback_)
      std::move(on_accounts_in_cookie_updated_callback_).Run();
  }
  void OnStartBatchOfRefreshTokenStateChanges() override {
    EXPECT_FALSE(is_inside_batch_);
    is_inside_batch_ = true;

    // Start a new batch.
    batch_change_records_.emplace_back(std::vector<std::string>());
  }
  void OnEndBatchOfRefreshTokenStateChanges() override {
    EXPECT_TRUE(is_inside_batch_);
    is_inside_batch_ = false;
  }

  IdentityManager* identity_manager_;
  base::OnceClosure on_primary_account_set_callback_;
  base::OnceClosure on_primary_account_cleared_callback_;
  base::OnceClosure on_primary_account_signin_failed_callback_;
  base::OnceClosure on_refresh_token_updated_callback_;
  base::RepeatingCallback<void(const std::string&)>
      on_refresh_token_removed_callback_;
  base::OnceClosure on_refresh_tokens_loaded_callback_;
  base::OnceClosure on_accounts_in_cookie_updated_callback_;
  AccountInfo primary_account_from_set_callback_;
  AccountInfo primary_account_from_cleared_callback_;
  AccountInfo account_from_refresh_token_updated_callback_;
  bool validity_from_refresh_token_updated_callback_;
  std::string account_from_refresh_token_removed_callback_;
  std::vector<AccountInfo> accounts_from_cookie_change_callback_;
  GoogleServiceAuthError google_signin_failed_error_;
  bool is_inside_batch_ = false;
  std::vector<std::vector<std::string>> batch_change_records_;
};

class TestIdentityManagerDiagnosticsObserver
    : IdentityManager::DiagnosticsObserver {
 public:
  explicit TestIdentityManagerDiagnosticsObserver(
      IdentityManager* identity_manager)
      : identity_manager_(identity_manager) {
    identity_manager_->AddDiagnosticsObserver(this);
  }
  ~TestIdentityManagerDiagnosticsObserver() override {
    identity_manager_->RemoveDiagnosticsObserver(this);
  }

  void set_on_access_token_requested_callback(base::OnceClosure callback) {
    on_access_token_requested_callback_ = std::move(callback);
  }

  const std::string& token_requestor_account_id() {
    return token_requestor_account_id_;
  }
  const std::string& token_requestor_consumer_id() {
    return token_requestor_consumer_id_;
  }
  const identity::ScopeSet& token_requestor_scopes() {
    return token_requestor_scopes_;
  }

 private:
  // IdentityManager::DiagnosticsObserver:
  void OnAccessTokenRequested(const std::string& account_id,
                              const std::string& consumer_id,
                              const identity::ScopeSet& scopes) override {
    token_requestor_account_id_ = account_id;
    token_requestor_consumer_id_ = consumer_id;
    token_requestor_scopes_ = scopes;

    if (on_access_token_requested_callback_)
      std::move(on_access_token_requested_callback_).Run();
  }

  IdentityManager* identity_manager_;
  base::OnceClosure on_access_token_requested_callback_;
  std::string token_requestor_account_id_;
  std::string token_requestor_consumer_id_;
  identity::ScopeSet token_requestor_scopes_;
};

}  // namespace

class IdentityManagerTest : public testing::Test {
 public:
  IdentityManagerTest()
      : signin_client_(&pref_service_),
        token_service_(&pref_service_),
        gaia_cookie_manager_service_(&token_service_,
                                     "identity_manager_unittest",
                                     &signin_client_) {
    AccountTrackerService::RegisterPrefs(pref_service_.registry());
    ProfileOAuth2TokenService::RegisterProfilePrefs(pref_service_.registry());
    SigninManagerBase::RegisterProfilePrefs(pref_service_.registry());
    SigninManagerBase::RegisterPrefs(pref_service_.registry());

    account_tracker_.Initialize(&pref_service_, base::FilePath());

    RecreateSigninAndIdentityManager(
        signin::AccountConsistencyMethod::kDisabled,
        SigninManagerSetup::kWithAuthenticatedAccout);
  }

  IdentityManager* identity_manager() { return identity_manager_.get(); }
  TestIdentityManagerObserver* identity_manager_observer() {
    return identity_manager_observer_.get();
  }
  TestIdentityManagerDiagnosticsObserver*
  identity_manager_diagnostics_observer() {
    return identity_manager_diagnostics_observer_.get();
  }
  AccountTrackerServiceForTest* account_tracker() { return &account_tracker_; }
  SigninManagerForTest* signin_manager() { return signin_manager_.get(); }
  CustomFakeProfileOAuth2TokenService* token_service() {
    return &token_service_;
  }
  FakeGaiaCookieManagerService* gaia_cookie_manager_service() {
    return &gaia_cookie_manager_service_;
  }

  // See RecreateSigninAndIdentityManager.
  enum class SigninManagerSetup {
    kWithAuthenticatedAccout,
    kNoAuthenticatedAccount
  };

  // Recreates SigninManager and IdentityManager with given
  // |account_consistency| and optionally seeds with an authenticated account
  // depending on |singin_manager_setup|. This process destroys any existing
  // IdentityManager and its dependencies, then remakes them. Dependencies that
  // outlive SigninManager (e.g. SigninClient) will be reused.
  void RecreateSigninAndIdentityManager(
      signin::AccountConsistencyMethod account_consistency,
      SigninManagerSetup signin_manager_setup) {
    // Reset dependents to null first to ensure that they're destroyed, as
    // otherwise destructors of SigninManager and dependents will DCHECK because
    // they still having living observers.
    identity_manager_observer_.reset();
    identity_manager_diagnostics_observer_.reset();
    identity_manager_.reset();

    if (signin_manager_) {
      signin_manager_->Shutdown();
      signin_manager_.reset();
    }

#if defined(OS_CHROMEOS)
    DCHECK_EQ(account_consistency, signin::AccountConsistencyMethod::kDisabled)
        << "AccountConsistency is not used by SigninManagerBase";
    signin_manager_ = std::make_unique<FakeSigninManagerBase>(
        &signin_client_, &account_tracker_);
#else
    signin_manager_ = std::make_unique<FakeSigninManager>(
        &signin_client_, &token_service_, &account_tracker_,
        &gaia_cookie_manager_service_, nullptr, account_consistency);
#endif

    // Passing this switch ensures that the new SigninManager starts with a
    // clean slate. Otherwise SigninManagerBase::Initialize will use the account
    // id stored in prefs::kGoogleServicesAccountId.
    base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
    cmd_line->AppendSwitch(switches::kClearTokenService);

    signin_manager_->Initialize(&pref_service_);

    if (signin_manager_setup == SigninManagerSetup::kWithAuthenticatedAccout) {
      signin_manager()->SetAuthenticatedAccountInfo(kTestGaiaId, kTestEmail);
    }

    RecreateIdentityManager();
  }

  // Used by some tests that need to re-instantiate IdentityManager after
  // performing some other setup.
  void RecreateIdentityManager() {
    DCHECK(signin_manager_) << "Create signin_manager_ first";

    // Reset them all to null first to ensure that they're destroyed, as
    // otherwise SigninManager ends up getting a new DiagnosticsObserver added
    // before the old one is removed.
    identity_manager_observer_.reset();
    identity_manager_diagnostics_observer_.reset();
    identity_manager_.reset();

    identity_manager_.reset(new IdentityManager(
        signin_manager_.get(), &token_service_, &account_tracker_,
        &gaia_cookie_manager_service_, nullptr));
    identity_manager_observer_.reset(
        new TestIdentityManagerObserver(identity_manager_.get()));
    identity_manager_diagnostics_observer_.reset(
        new TestIdentityManagerDiagnosticsObserver(identity_manager_.get()));
  }

#if !defined(OS_CHROMEOS)
  enum class RemoveTokenExpectation{kKeepAll, kRemovePrimary, kRemoveAll};

  // Helper for testing of ClearPrimaryAccount(). This method requires lots
  // of tests due to having different behaviors based on its arguments. But the
  // setup and execution of these test is all the boiler plate you see here:
  // 1) Ensure you have 2 accounts, both with refresh tokens
  // 2) Clear the primary account
  // 3) Assert clearing succeeds and refresh tokens are optionally removed based
  //    on arguments.
  void RunClearPrimaryAccountTest(
      IdentityManager::ClearAccountTokensAction token_action,
      RemoveTokenExpectation token_expectation) {
    EXPECT_TRUE(identity_manager()->HasPrimaryAccount());
    EXPECT_EQ(identity_manager()->GetPrimaryAccountInfo().email, kTestEmail);

    // Ensure primary and secondary emails are set up with refresh tokens. Some
    // tests may do this externally for the primary account.
    if (!identity_manager()->HasPrimaryAccountWithRefreshToken())
      SetRefreshTokenForPrimaryAccount(token_service(), identity_manager());
    EXPECT_TRUE(identity_manager()->HasPrimaryAccountWithRefreshToken());
    AccountInfo secondary_account_info = MakeAccountAvailable(
        account_tracker(), token_service(), identity_manager(), kTestEmail2);
    EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(
        secondary_account_info.account_id));

    // Track Observer token removal notifications.
    base::flat_set<std::string> observed_removals;
    identity_manager_observer()->set_on_refresh_token_removed_callback(
        base::BindRepeating(
            [](base::flat_set<std::string>* observed_removals,
               const std::string& removed_account) {
              observed_removals->insert(removed_account);
            },
            &observed_removals));

    // Grab this before clearing for token checks below.
    auto former_primary_account = identity_manager()->GetPrimaryAccountInfo();

    base::RunLoop run_loop;
    identity_manager_observer()->set_on_primary_account_cleared_callback(
        run_loop.QuitClosure());
    identity_manager()->ClearPrimaryAccount(
        token_action, signin_metrics::SIGNOUT_TEST,
        signin_metrics::SignoutDelete::IGNORE_METRIC);
    run_loop.Run();

    EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
    // NOTE: IdentityManager _may_ still possess this token (see switch below),
    // but it is no longer considered part of the primary account.
    EXPECT_FALSE(identity_manager()->HasPrimaryAccountWithRefreshToken());

    switch (token_expectation) {
      case RemoveTokenExpectation::kKeepAll:
        EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(
            former_primary_account.account_id));
        EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(
            secondary_account_info.account_id));
        EXPECT_TRUE(observed_removals.empty());
        break;
      case RemoveTokenExpectation::kRemovePrimary:
        EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(
            former_primary_account.account_id));
        EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(
            secondary_account_info.account_id));
        EXPECT_TRUE(base::ContainsKey(observed_removals,
                                      former_primary_account.account_id));
        EXPECT_FALSE(base::ContainsKey(observed_removals,
                                       secondary_account_info.account_id));
        break;
      case RemoveTokenExpectation::kRemoveAll:
        EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(
            former_primary_account.account_id));
        EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(
            secondary_account_info.account_id));
        EXPECT_TRUE(base::ContainsKey(observed_removals,
                                      former_primary_account.account_id));
        EXPECT_TRUE(base::ContainsKey(observed_removals,
                                      secondary_account_info.account_id));
        break;
    }
  }
#endif  // !defined(OS_CHROMEOS)

 private:
  base::MessageLoop message_loop_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  AccountTrackerServiceForTest account_tracker_;
  TestSigninClient signin_client_;
  CustomFakeProfileOAuth2TokenService token_service_;
  FakeGaiaCookieManagerService gaia_cookie_manager_service_;
  std::unique_ptr<SigninManagerForTest> signin_manager_;
  std::unique_ptr<IdentityManager> identity_manager_;
  std::unique_ptr<TestIdentityManagerObserver> identity_manager_observer_;
  std::unique_ptr<TestIdentityManagerDiagnosticsObserver>
      identity_manager_diagnostics_observer_;

  DISALLOW_COPY_AND_ASSIGN(IdentityManagerTest);
};

// Test that IdentityManager starts off with the information in SigninManager.
TEST_F(IdentityManagerTest, PrimaryAccountInfoAtStartup) {
  AccountInfo primary_account_info =
      identity_manager()->GetPrimaryAccountInfo();
  EXPECT_EQ(kTestGaiaId, primary_account_info.gaia);
  EXPECT_EQ(kTestEmail, primary_account_info.email);
}

// Signin/signout tests aren't relevant and cannot build on ChromeOS, which
// doesn't support signin/signout.
#if !defined(OS_CHROMEOS)
// Test that the user signing in results in firing of the IdentityManager
// observer callback and the IdentityManager's state being updated.
TEST_F(IdentityManagerTest, PrimaryAccountInfoAfterSignin) {
  base::RunLoop run_loop;
  identity_manager_observer()->set_on_primary_account_set_callback(
      run_loop.QuitClosure());

  signin_manager()->SignIn(kTestGaiaId, kTestEmail, "password");
  run_loop.Run();

  AccountInfo primary_account_from_set_callback =
      identity_manager_observer()->primary_account_from_set_callback();
  EXPECT_EQ(kTestGaiaId, primary_account_from_set_callback.gaia);
  EXPECT_EQ(kTestEmail, primary_account_from_set_callback.email);

  AccountInfo primary_account_info =
      identity_manager()->GetPrimaryAccountInfo();
  EXPECT_EQ(kTestGaiaId, primary_account_info.gaia);
  EXPECT_EQ(kTestEmail, primary_account_info.email);

  std::string primary_account_id = identity_manager()->GetPrimaryAccountId();
  EXPECT_EQ(primary_account_id, kTestGaiaId);
  EXPECT_EQ(primary_account_id, primary_account_info.account_id);
}

TEST_F(IdentityManagerTest, ClearPrimaryAccount_RemoveAll) {
  RunClearPrimaryAccountTest(
      IdentityManager::ClearAccountTokensAction::kRemoveAll,
      RemoveTokenExpectation::kRemoveAll);
}

TEST_F(IdentityManagerTest, ClearPrimaryAccount_KeepAll) {
  RunClearPrimaryAccountTest(
      IdentityManager::ClearAccountTokensAction::kKeepAll,
      RemoveTokenExpectation::kKeepAll);
}

// Test that ClearPrimaryAccount(...) with ClearAccountTokensAction::kDefault
// and AccountConsistencyMethod::kDisabled (notably != kDice) removes all
// tokens.
TEST_F(IdentityManagerTest, ClearPrimaryAccount_Default_DisabledConsistency) {
  // Tests default to kDisabled, so this is just being explicit.
  RecreateSigninAndIdentityManager(
      signin::AccountConsistencyMethod::kDisabled,
      SigninManagerSetup::kWithAuthenticatedAccout);

  RunClearPrimaryAccountTest(
      IdentityManager::ClearAccountTokensAction::kDefault,
      RemoveTokenExpectation::kRemoveAll);
}

// Test that ClearPrimaryAccount(...) with ClearAccountTokensAction::kDefault
// and AccountConsistencyMethod::kMirror (notably != kDice) removes all
// tokens.
TEST_F(IdentityManagerTest, ClearPrimaryAccount_Default_MirrorConsistency) {
  RecreateSigninAndIdentityManager(
      signin::AccountConsistencyMethod::kMirror,
      SigninManagerSetup::kWithAuthenticatedAccout);

  RunClearPrimaryAccountTest(
      IdentityManager::ClearAccountTokensAction::kDefault,
      RemoveTokenExpectation::kRemoveAll);
}

// Test that ClearPrimaryAccount(...) with ClearAccountTokensAction::kDefault
// and AccountConsistencyMethod::kDice keeps all accounts when the the primary
// account does not have an authentication error (see *_AuthError test).
TEST_F(IdentityManagerTest, ClearPrimaryAccount_Default_DiceConsistency) {
  RecreateSigninAndIdentityManager(
      signin::AccountConsistencyMethod::kDice,
      SigninManagerSetup::kWithAuthenticatedAccout);

  RunClearPrimaryAccountTest(
      IdentityManager::ClearAccountTokensAction::kDefault,
      RemoveTokenExpectation::kKeepAll);
}

// Test that ClearPrimaryAccount(...) with ClearAccountTokensAction::kDefault
// and AccountConsistencyMethod::kDice removes *only* the primary account
// due to it authentication error.
TEST_F(IdentityManagerTest,
       ClearPrimaryAccount_Default_DiceConsistency_AuthError) {
  RecreateSigninAndIdentityManager(
      signin::AccountConsistencyMethod::kDice,
      SigninManagerSetup::kWithAuthenticatedAccout);

  // Set primary account to have authentication error.
  SetRefreshTokenForPrimaryAccount(token_service(), identity_manager());
  token_service()->UpdateAuthErrorForTesting(
      identity_manager()->GetPrimaryAccountId(),
      GoogleServiceAuthError(
          GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS));

  RunClearPrimaryAccountTest(
      IdentityManager::ClearAccountTokensAction::kDefault,
      RemoveTokenExpectation::kRemovePrimary);
}

// Test that ClearPrimaryAccount(...) with authentication in progress notifies
// Observers that sign-in is canceled and does not remove any tokens.
TEST_F(IdentityManagerTest, ClearPrimaryAccount_AuthInProgress) {
  // Recreate components without setting an authenticated (primary) account.
  RecreateSigninAndIdentityManager(signin::AccountConsistencyMethod::kDisabled,
                                   SigninManagerSetup::kNoAuthenticatedAccount);
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());

  // Simulate authentication in progress (id value not important, treated as
  // potentially invalid until authentication completes).
  signin_manager()->set_auth_in_progress("bogus_id");
  EXPECT_TRUE(signin_manager()->AuthInProgress());

  // Add a secondary account to verify that its refresh token survives the
  // call to ClearPrimaryAccount(...) below.
  AccountInfo secondary_account_info = MakeAccountAvailable(
      account_tracker(), token_service(), identity_manager(), kTestEmail2);
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(
      secondary_account_info.account_id));

  // Observe that in-progress authentication is *canceled* and quit the RunLoop.
  base::RunLoop run_loop;
  identity_manager_observer()->set_on_primary_account_signin_failed_callback(
      run_loop.QuitClosure());

  // Observer should not be notified of any token removals.
  identity_manager_observer()->set_on_refresh_token_removed_callback(
      base::BindRepeating([](const std::string&) { EXPECT_TRUE(false); }));

  // No primary account to "clear", so no callback.
  identity_manager_observer()->set_on_primary_account_cleared_callback(
      base::BindOnce([] { EXPECT_TRUE(false); }));

  identity_manager()->ClearPrimaryAccount(
      IdentityManager::ClearAccountTokensAction::kRemoveAll,
      signin_metrics::SIGNOUT_TEST,
      signin_metrics::SignoutDelete::IGNORE_METRIC);
  run_loop.Run();

  // Verify in-progress authentication was canceled.
  EXPECT_EQ(
      identity_manager_observer()->error_from_signin_failed_callback().state(),
      GoogleServiceAuthError::State::REQUEST_CANCELED);
  EXPECT_FALSE(signin_manager()->AuthInProgress());

  // We didn't have a primary account to start with, we shouldn't have one now
  // either.
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
  EXPECT_FALSE(identity_manager()->HasPrimaryAccountWithRefreshToken());

  // Secondary account token still exists.
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(
      secondary_account_info.account_id));
}

// Test that the user signing out results in firing of the IdentityManager
// observer callback and the IdentityManager's state being updated.
TEST_F(IdentityManagerTest, PrimaryAccountInfoAfterSigninAndSignout) {
  // First ensure that the user is signed in from the POV of the
  // IdentityManager.
  base::RunLoop run_loop;
  identity_manager_observer()->set_on_primary_account_set_callback(
      run_loop.QuitClosure());
  signin_manager()->SignIn(kTestGaiaId, kTestEmail, "password");
  run_loop.Run();

  // Sign the user out and check that the IdentityManager responds
  // appropriately.
  base::RunLoop run_loop2;
  identity_manager_observer()->set_on_primary_account_cleared_callback(
      run_loop2.QuitClosure());

  signin_manager()->ForceSignOut();
  run_loop2.Run();

  AccountInfo primary_account_from_cleared_callback =
      identity_manager_observer()->primary_account_from_cleared_callback();
  EXPECT_EQ(kTestGaiaId, primary_account_from_cleared_callback.gaia);
  EXPECT_EQ(kTestEmail, primary_account_from_cleared_callback.email);

  AccountInfo primary_account_info =
      identity_manager()->GetPrimaryAccountInfo();
  EXPECT_EQ("", primary_account_info.gaia);
  EXPECT_EQ("", primary_account_info.email);

  std::string primary_account_id = identity_manager()->GetPrimaryAccountId();
  EXPECT_EQ("", primary_account_id);
  EXPECT_EQ(primary_account_id, primary_account_info.account_id);
}

// Test that the primary account's ID remains tracked by the IdentityManager
// after signing in even after having removed the account without signing out.
TEST_F(IdentityManagerTest, PrimaryAccountInfoAfterSigninAndAccountRemoval) {
  // First ensure that the user is signed in from the POV of the
  // IdentityManager.
  base::RunLoop run_loop;
  identity_manager_observer()->set_on_primary_account_set_callback(
      run_loop.QuitClosure());
  signin_manager()->SignIn(kTestGaiaId, kTestEmail, "password");
  run_loop.Run();

  // Remove the account from the AccountTrackerService and check that
  // the returned AccountInfo won't have a valid ID anymore, even if
  // the IdentityManager is still storing the primary account's ID.
  account_tracker()->RemoveAccount(kTestGaiaId);

  AccountInfo primary_account_info =
      identity_manager()->GetPrimaryAccountInfo();
  EXPECT_EQ("", primary_account_info.gaia);
  EXPECT_EQ("", primary_account_info.email);
  EXPECT_EQ("", primary_account_info.account_id);

  std::string primary_account_id = identity_manager()->GetPrimaryAccountId();
  EXPECT_EQ(primary_account_id, kTestGaiaId);
}
#endif  // !defined(OS_CHROMEOS)

TEST_F(IdentityManagerTest, HasPrimaryAccount) {
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount());

  // Removing the account from the AccountTrackerService should not cause
  // IdentityManager to think that there is no longer a primary account.
  account_tracker()->RemoveAccount(identity_manager()->GetPrimaryAccountId());
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount());

#if !defined(OS_CHROMEOS)
  // Signing out should cause IdentityManager to recognize that there is no
  // longer a primary account.
  base::RunLoop run_loop;
  identity_manager_observer()->set_on_primary_account_cleared_callback(
      run_loop.QuitClosure());

  signin_manager()->ForceSignOut();
  run_loop.Run();
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
#endif
}

TEST_F(IdentityManagerTest, GetAccountsInteractionWithPrimaryAccount) {
  // Should not have any refresh tokens at initialization.
  EXPECT_TRUE(identity_manager()->GetAccountsWithRefreshTokens().empty());

  std::string account_id = signin_manager()->GetAuthenticatedAccountId();

  // Add a refresh token for the primary account and check that it shows up in
  // GetAccountsWithRefreshTokens().
  SetRefreshTokenForPrimaryAccount(token_service(), identity_manager());

  std::vector<AccountInfo> accounts_after_update =
      identity_manager()->GetAccountsWithRefreshTokens();

  EXPECT_EQ(1u, accounts_after_update.size());
  EXPECT_EQ(accounts_after_update[0].account_id, account_id);
  EXPECT_EQ(accounts_after_update[0].gaia, kTestGaiaId);
  EXPECT_EQ(accounts_after_update[0].email, kTestEmail);

  // Update the token and check that it doesn't change the state (or blow up).
  SetRefreshTokenForPrimaryAccount(token_service(), identity_manager());

  std::vector<AccountInfo> accounts_after_second_update =
      identity_manager()->GetAccountsWithRefreshTokens();

  EXPECT_EQ(1u, accounts_after_second_update.size());
  EXPECT_EQ(accounts_after_second_update[0].account_id, account_id);
  EXPECT_EQ(accounts_after_second_update[0].gaia, kTestGaiaId);
  EXPECT_EQ(accounts_after_second_update[0].email, kTestEmail);

  // Remove the token for the primary account and check that this is likewise
  // reflected.
  RemoveRefreshTokenForPrimaryAccount(token_service(), identity_manager());

  EXPECT_TRUE(identity_manager()->GetAccountsWithRefreshTokens().empty());
}

TEST_F(IdentityManagerTest,
       QueryingOfRefreshTokensInteractionWithPrimaryAccount) {
  AccountInfo account_info = identity_manager()->GetPrimaryAccountInfo();
  std::string account_id = account_info.account_id;

  // Should not have a refresh token for the primary account at initialization.
  EXPECT_FALSE(
      identity_manager()->HasAccountWithRefreshToken(account_info.account_id));
  EXPECT_FALSE(identity_manager()->HasPrimaryAccountWithRefreshToken());

  // Add a refresh token for the primary account and check that it affects this
  // state.
  SetRefreshTokenForPrimaryAccount(token_service(), identity_manager());

  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshToken(account_info.account_id));
  EXPECT_TRUE(identity_manager()->HasPrimaryAccountWithRefreshToken());

  // Update the token and check that it doesn't change the state (or blow up).
  SetRefreshTokenForPrimaryAccount(token_service(), identity_manager());

  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshToken(account_info.account_id));
  EXPECT_TRUE(identity_manager()->HasPrimaryAccountWithRefreshToken());

  // Remove the token for the primary account and check that this is likewise
  // reflected.
  RemoveRefreshTokenForPrimaryAccount(token_service(), identity_manager());

  EXPECT_FALSE(
      identity_manager()->HasAccountWithRefreshToken(account_info.account_id));
  EXPECT_FALSE(identity_manager()->HasPrimaryAccountWithRefreshToken());
}

TEST_F(IdentityManagerTest, GetAccountsReflectsNonemptyInitialState) {
  EXPECT_TRUE(identity_manager()->GetAccountsWithRefreshTokens().empty());

  std::string account_id = signin_manager()->GetAuthenticatedAccountId();

  // Add a refresh token for the primary account and sanity-check that it shows
  // up in GetAccountsWithRefreshTokens().
  SetRefreshTokenForPrimaryAccount(token_service(), identity_manager());

  std::vector<AccountInfo> accounts_after_update =
      identity_manager()->GetAccountsWithRefreshTokens();

  EXPECT_EQ(1u, accounts_after_update.size());
  EXPECT_EQ(accounts_after_update[0].account_id, account_id);
  EXPECT_EQ(accounts_after_update[0].gaia, kTestGaiaId);
  EXPECT_EQ(accounts_after_update[0].email, kTestEmail);

  // Recreate the IdentityManager and check that the newly-created instance
  // reflects the current state.
  RecreateIdentityManager();

  std::vector<AccountInfo> accounts_after_recreation =
      identity_manager()->GetAccountsWithRefreshTokens();
  EXPECT_EQ(1u, accounts_after_recreation.size());
  EXPECT_EQ(accounts_after_recreation[0].account_id, account_id);
  EXPECT_EQ(accounts_after_recreation[0].gaia, kTestGaiaId);
  EXPECT_EQ(accounts_after_recreation[0].email, kTestEmail);
}

TEST_F(IdentityManagerTest,
       QueryingOfRefreshTokensReflectsNonemptyInitialState) {
  AccountInfo account_info = identity_manager()->GetPrimaryAccountInfo();
  std::string account_id = account_info.account_id;

  EXPECT_FALSE(
      identity_manager()->HasAccountWithRefreshToken(account_info.account_id));
  EXPECT_FALSE(identity_manager()->HasPrimaryAccountWithRefreshToken());

  SetRefreshTokenForPrimaryAccount(token_service(), identity_manager());

  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshToken(account_info.account_id));
  EXPECT_TRUE(identity_manager()->HasPrimaryAccountWithRefreshToken());

  // Recreate the IdentityManager and check that the newly-created instance
  // reflects the current state.
  RecreateIdentityManager();

  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshToken(account_info.account_id));
  EXPECT_TRUE(identity_manager()->HasPrimaryAccountWithRefreshToken());
}

TEST_F(IdentityManagerTest, GetAccountsInteractionWithSecondaryAccounts) {
  // Should not have any refresh tokens at initialization.
  EXPECT_TRUE(identity_manager()->GetAccountsWithRefreshTokens().empty());

  // Add a refresh token for a secondary account and check that it shows up in
  // GetAccountsWithRefreshTokens().
  account_tracker()->SeedAccountInfo(kTestGaiaId2, kTestEmail2);
  std::string account_id2 =
      account_tracker()->FindAccountInfoByGaiaId(kTestGaiaId2).account_id;
  SetRefreshTokenForAccount(token_service(), identity_manager(), account_id2);

  std::vector<AccountInfo> accounts_after_update =
      identity_manager()->GetAccountsWithRefreshTokens();

  EXPECT_EQ(1u, accounts_after_update.size());
  EXPECT_EQ(accounts_after_update[0].account_id, account_id2);
  EXPECT_EQ(accounts_after_update[0].gaia, kTestGaiaId2);
  EXPECT_EQ(accounts_after_update[0].email, kTestEmail2);

  // Add a refresh token for a different secondary account and check that it
  // also shows up in GetAccountsWithRefreshTokens().
  account_tracker()->SeedAccountInfo(kTestGaiaId3, kTestEmail3);
  std::string account_id3 =
      account_tracker()->FindAccountInfoByGaiaId(kTestGaiaId3).account_id;
  SetRefreshTokenForAccount(token_service(), identity_manager(), account_id3);

  std::vector<AccountInfo> accounts_after_second_update =
      identity_manager()->GetAccountsWithRefreshTokens();
  EXPECT_EQ(2u, accounts_after_second_update.size());

  for (AccountInfo account_info : accounts_after_second_update) {
    if (account_info.account_id == account_id2) {
      EXPECT_EQ(account_info.gaia, kTestGaiaId2);
      EXPECT_EQ(account_info.email, kTestEmail2);
    } else {
      EXPECT_EQ(account_info.gaia, kTestGaiaId3);
      EXPECT_EQ(account_info.email, kTestEmail3);
    }
  }

  // Remove the token for account2 and check that account3 is still present.
  RemoveRefreshTokenForAccount(token_service(), identity_manager(),
                               account_id2);

  std::vector<AccountInfo> accounts_after_third_update =
      identity_manager()->GetAccountsWithRefreshTokens();

  EXPECT_EQ(1u, accounts_after_third_update.size());
  EXPECT_EQ(accounts_after_third_update[0].account_id, account_id3);
  EXPECT_EQ(accounts_after_third_update[0].gaia, kTestGaiaId3);
  EXPECT_EQ(accounts_after_third_update[0].email, kTestEmail3);
}

TEST_F(IdentityManagerTest,
       HasPrimaryAccountWithRefreshTokenInteractionWithSecondaryAccounts) {
  EXPECT_FALSE(identity_manager()->HasPrimaryAccountWithRefreshToken());

  // Adding a refresh token for a secondary account shouldn't change anything
  // about the primary account
  account_tracker()->SeedAccountInfo(kTestGaiaId2, kTestEmail2);
  std::string account_id2 =
      account_tracker()->FindAccountInfoByGaiaId(kTestGaiaId2).account_id;
  SetRefreshTokenForAccount(token_service(), identity_manager(), account_id2);

  EXPECT_FALSE(identity_manager()->HasPrimaryAccountWithRefreshToken());

  // Adding a refresh token for a different secondary account should not do so
  // either.
  account_tracker()->SeedAccountInfo(kTestGaiaId3, kTestEmail3);
  std::string account_id3 =
      account_tracker()->FindAccountInfoByGaiaId(kTestGaiaId3).account_id;
  SetRefreshTokenForAccount(token_service(), identity_manager(), account_id3);

  EXPECT_FALSE(identity_manager()->HasPrimaryAccountWithRefreshToken());

  // Removing the token for account2 should have no effect.
  RemoveRefreshTokenForAccount(token_service(), identity_manager(),
                               account_id2);

  EXPECT_FALSE(identity_manager()->HasPrimaryAccountWithRefreshToken());
}

TEST_F(IdentityManagerTest,
       HasAccountWithRefreshTokenInteractionWithSecondaryAccounts) {
  account_tracker()->SeedAccountInfo(kTestGaiaId2, kTestEmail2);
  AccountInfo account_info2 =
      account_tracker()->FindAccountInfoByGaiaId(kTestGaiaId2);
  std::string account_id2 = account_info2.account_id;

  EXPECT_FALSE(
      identity_manager()->HasAccountWithRefreshToken(account_info2.account_id));

  // Add a refresh token for account_info2 and check that this is reflected by
  // HasAccountWithRefreshToken(.account_id).
  SetRefreshTokenForAccount(token_service(), identity_manager(), account_id2);

  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshToken(account_info2.account_id));

  // Go through the same process for a different secondary account.
  account_tracker()->SeedAccountInfo(kTestGaiaId3, kTestEmail3);
  AccountInfo account_info3 =
      account_tracker()->FindAccountInfoByGaiaId(kTestGaiaId3);
  std::string account_id3 = account_info3.account_id;

  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshToken(account_info2.account_id));
  EXPECT_FALSE(
      identity_manager()->HasAccountWithRefreshToken(account_info3.account_id));

  SetRefreshTokenForAccount(token_service(), identity_manager(), account_id3);

  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshToken(account_info2.account_id));
  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshToken(account_info3.account_id));

  // Remove the token for account2.
  RemoveRefreshTokenForAccount(token_service(), identity_manager(),
                               account_id2);

  EXPECT_FALSE(
      identity_manager()->HasAccountWithRefreshToken(account_info2.account_id));
  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshToken(account_info3.account_id));
}

TEST_F(IdentityManagerTest,
       GetAccountsInteractionBetweenPrimaryAndSecondaryAccounts) {
  // Should not have any refresh tokens at initialization.
  EXPECT_TRUE(identity_manager()->GetAccountsWithRefreshTokens().empty());

  // Add a refresh token for a secondary account and check that it shows up in
  // GetAccountsWithRefreshTokens().
  account_tracker()->SeedAccountInfo(kTestGaiaId2, kTestEmail2);
  std::string account_id2 =
      account_tracker()->FindAccountInfoByGaiaId(kTestGaiaId2).account_id;
  SetRefreshTokenForAccount(token_service(), identity_manager(), account_id2);

  std::vector<AccountInfo> accounts_after_update =
      identity_manager()->GetAccountsWithRefreshTokens();

  EXPECT_EQ(1u, accounts_after_update.size());
  EXPECT_EQ(accounts_after_update[0].account_id, account_id2);
  EXPECT_EQ(accounts_after_update[0].gaia, kTestGaiaId2);
  EXPECT_EQ(accounts_after_update[0].email, kTestEmail2);

  EXPECT_FALSE(identity_manager()->HasPrimaryAccountWithRefreshToken());

  // Add a refresh token for the primary account and check that it
  // also shows up in GetAccountsWithRefreshTokens().
  std::string primary_account_id =
      signin_manager()->GetAuthenticatedAccountId();
  SetRefreshTokenForPrimaryAccount(token_service(), identity_manager());

  std::vector<AccountInfo> accounts_after_second_update =
      identity_manager()->GetAccountsWithRefreshTokens();
  EXPECT_EQ(2u, accounts_after_second_update.size());

  for (AccountInfo account_info : accounts_after_second_update) {
    if (account_info.account_id == account_id2) {
      EXPECT_EQ(account_info.gaia, kTestGaiaId2);
      EXPECT_EQ(account_info.email, kTestEmail2);
    } else {
      EXPECT_EQ(account_info.gaia, kTestGaiaId);
      EXPECT_EQ(account_info.email, kTestEmail);
    }
  }

  EXPECT_TRUE(identity_manager()->HasPrimaryAccountWithRefreshToken());

  // Remove the token for the primary account and check that account2 is still
  // present.
  RemoveRefreshTokenForPrimaryAccount(token_service(), identity_manager());

  std::vector<AccountInfo> accounts_after_third_update =
      identity_manager()->GetAccountsWithRefreshTokens();

  EXPECT_EQ(1u, accounts_after_third_update.size());
  EXPECT_EQ(accounts_after_update[0].account_id, account_id2);
  EXPECT_EQ(accounts_after_update[0].gaia, kTestGaiaId2);
  EXPECT_EQ(accounts_after_update[0].email, kTestEmail2);

  EXPECT_FALSE(identity_manager()->HasPrimaryAccountWithRefreshToken());
}

TEST_F(
    IdentityManagerTest,
    HasPrimaryAccountWithRefreshTokenInteractionBetweenPrimaryAndSecondaryAccounts) {
  EXPECT_FALSE(identity_manager()->HasPrimaryAccountWithRefreshToken());

  // Add a refresh token for a secondary account and check that it doesn't
  // impact the above state.
  account_tracker()->SeedAccountInfo(kTestGaiaId2, kTestEmail2);
  std::string account_id2 =
      account_tracker()->FindAccountInfoByGaiaId(kTestGaiaId2).account_id;
  SetRefreshTokenForAccount(token_service(), identity_manager(), account_id2);

  EXPECT_FALSE(identity_manager()->HasPrimaryAccountWithRefreshToken());

  // Add a refresh token for the primary account and check that it
  // *does* impact the stsate of HasPrimaryAccountWithRefreshToken().
  std::string primary_account_id =
      signin_manager()->GetAuthenticatedAccountId();
  SetRefreshTokenForPrimaryAccount(token_service(), identity_manager());

  EXPECT_TRUE(identity_manager()->HasPrimaryAccountWithRefreshToken());

  // Remove the token for the secondary account and check that this doesn't flip
  // the state.
  RemoveRefreshTokenForAccount(token_service(), identity_manager(),
                               account_id2);

  EXPECT_TRUE(identity_manager()->HasPrimaryAccountWithRefreshToken());

  // Remove the token for the primary account and check that this flips the
  // state.
  RemoveRefreshTokenForPrimaryAccount(token_service(), identity_manager());

  EXPECT_FALSE(identity_manager()->HasPrimaryAccountWithRefreshToken());
}

TEST_F(
    IdentityManagerTest,
    HasAccountWithRefreshTokenInteractionBetweenPrimaryAndSecondaryAccounts) {
  AccountInfo primary_account_info =
      identity_manager()->GetPrimaryAccountInfo();
  std::string primary_account_id = primary_account_info.account_id;

  account_tracker()->SeedAccountInfo(kTestGaiaId2, kTestEmail2);
  AccountInfo account_info2 =
      account_tracker()->FindAccountInfoByGaiaId(kTestGaiaId2);
  std::string account_id2 = account_info2.account_id;

  EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(
      primary_account_info.account_id));
  EXPECT_FALSE(
      identity_manager()->HasAccountWithRefreshToken(account_info2.account_id));

  // Add a refresh token for account_info2 and check that this is reflected by
  // HasAccountWithRefreshToken(.account_id).
  SetRefreshTokenForAccount(token_service(), identity_manager(), account_id2);

  EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(
      primary_account_info.account_id));
  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshToken(account_info2.account_id));

  // Go through the same process for the primary account.
  SetRefreshTokenForPrimaryAccount(token_service(), identity_manager());

  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(
      primary_account_info.account_id));
  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshToken(account_info2.account_id));

  // Remove the token for account2.
  RemoveRefreshTokenForAccount(token_service(), identity_manager(),
                               account_id2);

  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(
      primary_account_info.account_id));
  EXPECT_FALSE(
      identity_manager()->HasAccountWithRefreshToken(account_info2.account_id));
}

TEST_F(IdentityManagerTest, GetErrorStateOfRefreshTokenForAccount) {
  AccountInfo primary_account_info =
      identity_manager()->GetPrimaryAccountInfo();
  std::string primary_account_id = primary_account_info.account_id;

  // A primary account without a refresh token should not be in an error
  // state, and setting a refresh token should not affect that.
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            identity_manager()->GetErrorStateOfRefreshTokenForAccount(
                primary_account_id));
  EXPECT_FALSE(
      identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
          primary_account_id));

  SetRefreshTokenForPrimaryAccount(token_service(), identity_manager());
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            identity_manager()->GetErrorStateOfRefreshTokenForAccount(
                primary_account_id));
  EXPECT_FALSE(
      identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
          primary_account_id));

  // A secondary account without a refresh token should not be in an error
  // state, and setting a refresh token should not affect that.
  account_tracker()->SeedAccountInfo(kTestGaiaId2, kTestEmail2);
  AccountInfo account_info2 =
      account_tracker()->FindAccountInfoByGaiaId(kTestGaiaId2);
  std::string account_id2 = account_info2.account_id;
  EXPECT_EQ(
      GoogleServiceAuthError::AuthErrorNone(),
      identity_manager()->GetErrorStateOfRefreshTokenForAccount(account_id2));
  EXPECT_FALSE(
      identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
          account_id2));

  SetRefreshTokenForAccount(token_service(), identity_manager(), account_id2);
  EXPECT_EQ(
      GoogleServiceAuthError::AuthErrorNone(),
      identity_manager()->GetErrorStateOfRefreshTokenForAccount(account_id2));
  EXPECT_FALSE(
      identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
          account_id2));

  GoogleServiceAuthError account_deleted_error =
      GoogleServiceAuthError(GoogleServiceAuthError::State::ACCOUNT_DELETED);
  GoogleServiceAuthError account_disabled_error =
      GoogleServiceAuthError(GoogleServiceAuthError::State::ACCOUNT_DISABLED);
  GoogleServiceAuthError transient_error = GoogleServiceAuthError(
      GoogleServiceAuthError::State::SERVICE_UNAVAILABLE);

  // Set a persistent error for |account_id2| and check that it's reflected.
  token_service()->UpdateAuthErrorForTesting(account_id2,
                                             account_deleted_error);
  EXPECT_EQ(
      account_deleted_error,
      identity_manager()->GetErrorStateOfRefreshTokenForAccount(account_id2));
  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
          account_id2));
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            identity_manager()->GetErrorStateOfRefreshTokenForAccount(
                primary_account_id));
  EXPECT_FALSE(
      identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
          primary_account_id));

  // A transient error should cause no change in the error state.
  token_service()->UpdateAuthErrorForTesting(primary_account_id,
                                             transient_error);
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            identity_manager()->GetErrorStateOfRefreshTokenForAccount(
                primary_account_id));
  EXPECT_FALSE(
      identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
          primary_account_id));

  // Set a different persistent error for the primary account and check that
  // it's reflected.
  token_service()->UpdateAuthErrorForTesting(primary_account_id,
                                             account_disabled_error);
  EXPECT_EQ(
      account_deleted_error,
      identity_manager()->GetErrorStateOfRefreshTokenForAccount(account_id2));
  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
          account_id2));
  EXPECT_EQ(account_disabled_error,
            identity_manager()->GetErrorStateOfRefreshTokenForAccount(
                primary_account_id));
  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
          primary_account_id));

  // Remove the token for account2 and check that it goes back to having no
  // error.
  RemoveRefreshTokenForAccount(token_service(), identity_manager(),
                               account_id2);
  EXPECT_EQ(
      GoogleServiceAuthError::AuthErrorNone(),
      identity_manager()->GetErrorStateOfRefreshTokenForAccount(account_id2));
  EXPECT_FALSE(
      identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
          account_id2));
  EXPECT_EQ(account_disabled_error,
            identity_manager()->GetErrorStateOfRefreshTokenForAccount(
                primary_account_id));
  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
          primary_account_id));
}

TEST_F(IdentityManagerTest, RemoveAccessTokenFromCache) {
  std::set<std::string> scopes{"scope"};
  std::string access_token = "access_token";

  signin_manager()->SetAuthenticatedAccountInfo(kTestGaiaId, kTestEmail);
  std::string account_id = signin_manager()->GetAuthenticatedAccountId();
  token_service()->UpdateCredentials(account_id, "refresh_token");

  base::RunLoop run_loop;
  token_service()->set_on_access_token_invalidated_info(
      account_id, scopes, access_token, run_loop.QuitClosure());

  identity_manager()->RemoveAccessTokenFromCache(account_id, scopes,
                                                 access_token);

  run_loop.Run();
}

TEST_F(IdentityManagerTest, CreateAccessTokenFetcher) {
  std::set<std::string> scopes{"scope"};
  AccessTokenFetcher::TokenCallback callback = base::BindOnce(
      [](GoogleServiceAuthError error, AccessTokenInfo access_token_info) {});
  std::unique_ptr<AccessTokenFetcher> token_fetcher =
      identity_manager()->CreateAccessTokenFetcherForAccount(
          identity_manager()->GetPrimaryAccountId(), "dummy_consumer", scopes,
          std::move(callback), AccessTokenFetcher::Mode::kImmediate);
  EXPECT_TRUE(token_fetcher);
}

TEST_F(IdentityManagerTest,
       CreateAccessTokenFetcherWithCustomURLLoaderFactory) {
  base::RunLoop run_loop;
  identity_manager_diagnostics_observer()
      ->set_on_access_token_requested_callback(run_loop.QuitClosure());

  signin_manager()->SetAuthenticatedAccountInfo(kTestGaiaId, kTestEmail);
  std::string account_id = signin_manager()->GetAuthenticatedAccountId();
  token_service()->UpdateCredentials(account_id, "refresh_token");

  std::set<std::string> scopes{"scope"};
  AccessTokenFetcher::TokenCallback callback = base::BindOnce(
      [](GoogleServiceAuthError error, AccessTokenInfo access_token_info) {});

  // We first create and AccessTokenFetcher with a custom URLLoaderFactory,
  // to check that such factory is actually used in the requests generated.
  network::TestURLLoaderFactory test_url_loader_factory;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_url_loader_factory(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory));
  std::unique_ptr<AccessTokenFetcher> token_fetcher =
      identity_manager()->CreateAccessTokenFetcherForAccount(
          account_id, "dummy_consumer", test_shared_url_loader_factory, scopes,
          std::move(callback), AccessTokenFetcher::Mode::kImmediate);

  run_loop.Run();

  // The URLLoaderFactory present in the pending request should match
  // the one we specified when creating the AccessTokenFetcher.
  std::vector<FakeProfileOAuth2TokenService::PendingRequest> pending_requests =
      token_service()->GetPendingRequests();
  EXPECT_EQ(pending_requests.size(), 1U);
  EXPECT_EQ(pending_requests[0].url_loader_factory,
            test_shared_url_loader_factory);

  // The account ID and consumer's name should match the data passed as well.
  EXPECT_EQ(
      account_id,
      identity_manager_diagnostics_observer()->token_requestor_account_id());
  EXPECT_EQ(
      "dummy_consumer",
      identity_manager_diagnostics_observer()->token_requestor_consumer_id());

  // Cancel the pending request in preparation to check that creating an
  // AccessTokenFetcher without a custom factory works as expected as well.
  token_service()->IssueErrorForAllPendingRequestsForAccount(
      account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED));

  // Now add a second account and request an access token for it to test
  // that the default URLLoaderFactory is used if none is specified.
  base::RunLoop run_loop2;
  identity_manager_diagnostics_observer()
      ->set_on_access_token_requested_callback(run_loop2.QuitClosure());

  account_tracker()->SeedAccountInfo(kTestGaiaId2, kTestEmail2);
  std::string account_id2 =
      account_tracker()->FindAccountInfoByGaiaId(kTestGaiaId2).account_id;
  token_service()->UpdateCredentials(account_id2, "refresh_token");

  // No changes to the declared scopes and callback, we can reuse them.
  std::unique_ptr<AccessTokenFetcher> token_fetcher2 =
      identity_manager()->CreateAccessTokenFetcherForAccount(
          account_id2, "dummy_consumer 2", scopes, std::move(callback),
          AccessTokenFetcher::Mode::kImmediate);

  run_loop2.Run();

  // There should be one pending request now as well, just like before.
  std::vector<FakeProfileOAuth2TokenService::PendingRequest> pending_requests2 =
      token_service()->GetPendingRequests();
  EXPECT_EQ(pending_requests2.size(), 1U);

  // The URLLoaderFactory present in the pending request should match
  // the one created by default for the token service's delegate.
  OAuth2TokenServiceDelegate* service_delegate = token_service()->GetDelegate();
  EXPECT_EQ(pending_requests2[0].url_loader_factory,
            service_delegate->GetURLLoaderFactory());

  // The account ID and consumer's name should match the data passed again.
  EXPECT_EQ(
      account_id2,
      identity_manager_diagnostics_observer()->token_requestor_account_id());
  EXPECT_EQ(
      "dummy_consumer 2",
      identity_manager_diagnostics_observer()->token_requestor_consumer_id());
}

TEST_F(IdentityManagerTest, ObserveAccessTokenFetch) {
  base::RunLoop run_loop;
  identity_manager_diagnostics_observer()
      ->set_on_access_token_requested_callback(run_loop.QuitClosure());

  signin_manager()->SetAuthenticatedAccountInfo(kTestGaiaId, kTestEmail);
  std::string account_id = signin_manager()->GetAuthenticatedAccountId();
  token_service()->UpdateCredentials(account_id, "refresh_token");

  std::set<std::string> scopes{"scope"};
  AccessTokenFetcher::TokenCallback callback = base::BindOnce(
      [](GoogleServiceAuthError error, AccessTokenInfo access_token_info) {});
  std::unique_ptr<AccessTokenFetcher> token_fetcher =
      identity_manager()->CreateAccessTokenFetcherForAccount(
          identity_manager()->GetPrimaryAccountId(), "dummy_consumer", scopes,
          std::move(callback), AccessTokenFetcher::Mode::kImmediate);

  run_loop.Run();

  EXPECT_EQ(
      account_id,
      identity_manager_diagnostics_observer()->token_requestor_account_id());
  EXPECT_EQ(
      "dummy_consumer",
      identity_manager_diagnostics_observer()->token_requestor_consumer_id());
  EXPECT_EQ(scopes,
            identity_manager_diagnostics_observer()->token_requestor_scopes());
}

#if !defined(OS_CHROMEOS)
TEST_F(
    IdentityManagerTest,
    IdentityManagerGivesConsistentValuesFromSigninManagerObserverNotificationOfSignIn) {
  signin_manager()->ForceSignOut();

  base::RunLoop run_loop;
  TestSigninManagerObserver signin_manager_observer(signin_manager());
  signin_manager_observer.set_on_google_signin_succeeded_callback(
      run_loop.QuitClosure());

  // NOTE: For this test to be meaningful, TestSigninManagerObserver
  // needs to be created before the IdentityManager instance that it's
  // interacting with. Otherwise, even an implementation where they're
  // both SigninManager::Observers would work as IdentityManager would
  // get notified first during the observer callbacks.
  RecreateIdentityManager();
  signin_manager_observer.set_identity_manager(identity_manager());

  signin_manager()->SignIn(kTestGaiaId, kTestEmail, "password");
  run_loop.Run();

  AccountInfo primary_account_from_signin_callback =
      signin_manager_observer.primary_account_from_signin_callback();
  EXPECT_EQ(kTestGaiaId, primary_account_from_signin_callback.gaia);
  EXPECT_EQ(kTestEmail, primary_account_from_signin_callback.email);
}

TEST_F(
    IdentityManagerTest,
    IdentityManagerGivesConsistentValuesFromSigninManagerObserverNotificationOfSignOut) {
  base::RunLoop run_loop;
  TestSigninManagerObserver signin_manager_observer(signin_manager());
  signin_manager_observer.set_on_google_signed_out_callback(
      run_loop.QuitClosure());

  // NOTE: For this test to be meaningful, TestSigninManagerObserver
  // needs to be created before the IdentityManager instance that it's
  // interacting with. Otherwise, even an implementation where they're
  // both SigninManager::Observers would work as IdentityManager would
  // get notified first during the observer callbacks.
  RecreateIdentityManager();
  signin_manager_observer.set_identity_manager(identity_manager());

  signin_manager()->ForceSignOut();
  run_loop.Run();

  AccountInfo primary_account_from_signout_callback =
      signin_manager_observer.primary_account_from_signout_callback();
  EXPECT_EQ(std::string(), primary_account_from_signout_callback.gaia);
  EXPECT_EQ(std::string(), primary_account_from_signout_callback.email);
}
#endif

#if defined(OS_CHROMEOS)
// On ChromeOS, AccountTrackerService first receives the normalized email
// address from GAIA and then later has it updated with the user's
// originally-specified version of their email address (at the time of that
// address' creation). This latter will differ if the user's originally-
// specified address was not in normalized form (e.g., if it contained
// periods). This test simulates such a flow in order to verify that
// IdentityManager correctly reflects the updated version. See crbug.com/842041
// and crbug.com/842670 for further details.
TEST_F(IdentityManagerTest, IdentityManagerReflectsUpdatedEmailAddress) {
  AccountInfo primary_account_info =
      identity_manager()->GetPrimaryAccountInfo();
  EXPECT_EQ(kTestGaiaId, primary_account_info.gaia);
  EXPECT_EQ(kTestEmail, primary_account_info.email);

  // Simulate the flow wherein the user's email address was updated
  // to the originally-created non-normalized version.
  base::DictionaryValue user_info;
  user_info.SetString("id", kTestGaiaId);
  user_info.SetString("email", kTestEmailWithPeriod);
  account_tracker()->SetAccountStateFromUserInfo(
      primary_account_info.account_id, &user_info);

  // Verify that IdentityManager reflects the update.
  primary_account_info = identity_manager()->GetPrimaryAccountInfo();
  EXPECT_EQ(kTestGaiaId, primary_account_info.gaia);
  EXPECT_EQ(kTestEmailWithPeriod, primary_account_info.email);
}
#endif

TEST_F(IdentityManagerTest,
       CallbackSentOnPrimaryAccountRefreshTokenUpdateWithValidToken) {
  std::string account_id = signin_manager()->GetAuthenticatedAccountId();

  SetRefreshTokenForPrimaryAccount(token_service(), identity_manager());

  AccountInfo account_info =
      identity_manager_observer()
          ->account_from_refresh_token_updated_callback();
  EXPECT_EQ(kTestGaiaId, account_info.gaia);
  EXPECT_EQ(kTestEmail, account_info.email);

  EXPECT_TRUE(identity_manager_observer()
                  ->validity_from_refresh_token_updated_callback());
}

TEST_F(IdentityManagerTest,
       CallbackSentOnPrimaryAccountRefreshTokenUpdateWithInvalidToken) {
  std::string account_id = signin_manager()->GetAuthenticatedAccountId();

  SetInvalidRefreshTokenForPrimaryAccount(token_service(), identity_manager());

  AccountInfo account_info =
      identity_manager_observer()
          ->account_from_refresh_token_updated_callback();
  EXPECT_EQ(kTestGaiaId, account_info.gaia);
  EXPECT_EQ(kTestEmail, account_info.email);

  EXPECT_FALSE(identity_manager_observer()
                   ->validity_from_refresh_token_updated_callback());
}

TEST_F(IdentityManagerTest, CallbackSentOnPrimaryAccountRefreshTokenRemoval) {
  std::string account_id = signin_manager()->GetAuthenticatedAccountId();

  SetRefreshTokenForPrimaryAccount(token_service(), identity_manager());

  RemoveRefreshTokenForPrimaryAccount(token_service(), identity_manager());

  EXPECT_EQ(account_id, identity_manager_observer()
                            ->account_from_refresh_token_removed_callback());
}

TEST_F(IdentityManagerTest,
       CallbackSentOnSecondaryAccountRefreshTokenUpdateWithValidToken) {
  AccountInfo expected_account_info = MakeAccountAvailable(
      account_tracker(), token_service(), identity_manager(), kTestEmail2);
  EXPECT_EQ(kTestEmail2, expected_account_info.email);

  AccountInfo account_info =
      identity_manager_observer()
          ->account_from_refresh_token_updated_callback();
  EXPECT_EQ(expected_account_info.account_id, account_info.account_id);
  EXPECT_EQ(expected_account_info.gaia, account_info.gaia);
  EXPECT_EQ(expected_account_info.email, account_info.email);

  EXPECT_TRUE(identity_manager_observer()
                  ->validity_from_refresh_token_updated_callback());
}

TEST_F(IdentityManagerTest,
       CallbackSentOnSecondaryAccountRefreshTokenUpdateWithInvalidToken) {
  AccountInfo expected_account_info = MakeAccountAvailable(
      account_tracker(), token_service(), identity_manager(), kTestEmail2);
  EXPECT_EQ(kTestEmail2, expected_account_info.email);

  SetInvalidRefreshTokenForAccount(token_service(), identity_manager(),
                                   expected_account_info.account_id);

  AccountInfo account_info =
      identity_manager_observer()
          ->account_from_refresh_token_updated_callback();
  EXPECT_EQ(expected_account_info.account_id, account_info.account_id);
  EXPECT_EQ(expected_account_info.gaia, account_info.gaia);
  EXPECT_EQ(expected_account_info.email, account_info.email);

  EXPECT_FALSE(identity_manager_observer()
                   ->validity_from_refresh_token_updated_callback());
}

TEST_F(IdentityManagerTest, CallbackSentOnSecondaryAccountRefreshTokenRemoval) {
  AccountInfo expected_account_info = MakeAccountAvailable(
      account_tracker(), token_service(), identity_manager(), kTestEmail2);
  EXPECT_EQ(kTestEmail2, expected_account_info.email);

  RemoveRefreshTokenForAccount(token_service(), identity_manager(),
                               expected_account_info.account_id);

  EXPECT_EQ(expected_account_info.account_id,
            identity_manager_observer()
                ->account_from_refresh_token_removed_callback());
}

#if !defined(OS_CHROMEOS)
TEST_F(
    IdentityManagerTest,
    CallbackSentOnSecondaryAccountRefreshTokenUpdateWithValidTokenWhenNoPrimaryAccount) {
  base::RunLoop run_loop;
  identity_manager_observer()->set_on_primary_account_cleared_callback(
      run_loop.QuitClosure());
  signin_manager()->ForceSignOut();
  run_loop.Run();

  AccountInfo expected_account_info = MakeAccountAvailable(
      account_tracker(), token_service(), identity_manager(), kTestEmail2);
  EXPECT_EQ(kTestEmail2, expected_account_info.email);

  AccountInfo account_info =
      identity_manager_observer()
          ->account_from_refresh_token_updated_callback();
  EXPECT_EQ(expected_account_info.account_id, account_info.account_id);
  EXPECT_EQ(expected_account_info.gaia, account_info.gaia);
  EXPECT_EQ(expected_account_info.email, account_info.email);

  EXPECT_TRUE(identity_manager_observer()
                  ->validity_from_refresh_token_updated_callback());
}

TEST_F(
    IdentityManagerTest,
    CallbackSentOnSecondaryAccountRefreshTokenUpdateWithInvalidTokenWhenNoPrimaryAccount) {
  base::RunLoop run_loop;
  identity_manager_observer()->set_on_primary_account_cleared_callback(
      run_loop.QuitClosure());
  signin_manager()->ForceSignOut();
  run_loop.Run();

  AccountInfo expected_account_info = MakeAccountAvailable(
      account_tracker(), token_service(), identity_manager(), kTestEmail2);
  EXPECT_EQ(kTestEmail2, expected_account_info.email);

  SetInvalidRefreshTokenForAccount(token_service(), identity_manager(),
                                   expected_account_info.account_id);

  AccountInfo account_info =
      identity_manager_observer()
          ->account_from_refresh_token_updated_callback();
  EXPECT_EQ(expected_account_info.account_id, account_info.account_id);
  EXPECT_EQ(expected_account_info.gaia, account_info.gaia);
  EXPECT_EQ(expected_account_info.email, account_info.email);

  EXPECT_FALSE(identity_manager_observer()
                   ->validity_from_refresh_token_updated_callback());
}

TEST_F(IdentityManagerTest,
       CallbackSentOnSecondaryAccountRefreshTokenRemovalWhenNoPrimaryAccount) {
  base::RunLoop run_loop;
  identity_manager_observer()->set_on_primary_account_cleared_callback(
      run_loop.QuitClosure());
  signin_manager()->ForceSignOut();
  run_loop.Run();

  AccountInfo expected_account_info = MakeAccountAvailable(
      account_tracker(), token_service(), identity_manager(), kTestEmail2);
  EXPECT_EQ(kTestEmail2, expected_account_info.email);

  RemoveRefreshTokenForAccount(token_service(), identity_manager(),
                               expected_account_info.account_id);

  EXPECT_EQ(expected_account_info.account_id,
            identity_manager_observer()
                ->account_from_refresh_token_removed_callback());
}
#endif

TEST_F(IdentityManagerTest, CallbackSentOnRefreshTokenRemovalOfUnknownAccount) {
  // When the token service is still loading credentials, it may send token
  // revoked callbacks for accounts that it has never sent a token available
  // callback. Our common test setup actually completes this loading, so use the
  // *for_testing() method below to simulate the race condition and ensure that
  // IdentityManager passes on the callback in this case.
  token_service()->set_all_credentials_loaded_for_testing(false);

  std::string dummy_account_id = "dummy_account";

  base::RunLoop run_loop;
  token_service()->RevokeCredentials(dummy_account_id);
  run_loop.RunUntilIdle();

  EXPECT_EQ(dummy_account_id,
            identity_manager_observer()
                ->account_from_refresh_token_removed_callback());
}

TEST_F(
    IdentityManagerTest,
    IdentityManagerGivesConsistentValuesFromTokenServiceObserverNotificationOfTokenUpdate) {
  std::string account_id = signin_manager()->GetAuthenticatedAccountId();

  base::RunLoop run_loop;
  TestTokenServiceObserver token_service_observer(token_service());
  token_service_observer.set_on_refresh_token_available_callback(
      run_loop.QuitClosure());

  // NOTE: For this test to be meaningful, TestTokenServiceObserver
  // needs to be created before the IdentityManager instance that it's
  // interacting with. Otherwise, even an implementation where they're
  // both TokenService::Observers would work as IdentityManager would
  // get notified first during the observer callbacks.
  RecreateIdentityManager();
  EXPECT_TRUE(identity_manager()->GetAccountsWithRefreshTokens().empty());
  token_service_observer.set_identity_manager(identity_manager());

  // When the observer receives the callback directly from the token service,
  // IdentityManager should have already received the event and forwarded it on
  // to its own observers. This is checked internally by
  // TestTokenServiceObserver.
  token_service()->UpdateCredentials(account_id, "refresh_token");
  run_loop.Run();
}

TEST_F(
    IdentityManagerTest,
    IdentityManagerGivesConsistentValuesFromTokenServiceObserverNotificationOfTokenRemoval) {
  std::string account_id = signin_manager()->GetAuthenticatedAccountId();

  base::RunLoop run_loop;
  TestTokenServiceObserver token_service_observer(token_service());
  token_service_observer.set_on_refresh_token_available_callback(
      run_loop.QuitClosure());

  // NOTE: For this test to be meaningful, TestTokenServiceObserver
  // needs to be created before the IdentityManager instance that it's
  // interacting with. Otherwise, even an implementation where they're
  // both TokenService::Observers would work as IdentityManager would
  // get notified first during the observer callbacks.
  RecreateIdentityManager();
  token_service_observer.set_identity_manager(identity_manager());

  token_service()->UpdateCredentials(account_id, "refresh_token");
  run_loop.Run();

  // When the observer receives the callback directly from the token service,
  // IdentityManager should have already received the event and forwarded it on
  // to its own observers. This is checked internally by
  // TestTokenServiceObserver.
  base::RunLoop run_loop2;
  token_service_observer.set_on_refresh_token_revoked_callback(
      run_loop2.QuitClosure());
  token_service()->RevokeCredentials(account_id);
  run_loop2.Run();
}

TEST_F(IdentityManagerTest, IdentityManagerGetsTokensLoadedEvent) {
  std::string account_id = signin_manager()->GetAuthenticatedAccountId();

  base::RunLoop run_loop;
  identity_manager_observer()->set_on_refresh_tokens_loaded_callback(
      run_loop.QuitClosure());

  // Credentials are already loaded in SigninManager::Initialize()
  // which runs even before the IdentityManager is created. That's why
  // we fake the credentials loaded state and force another load in
  // order to be able to capture the TokensLoaded event.
  token_service()->set_all_credentials_loaded_for_testing(false);
  token_service()->LoadCredentials("");
  run_loop.Run();
}

TEST_F(IdentityManagerTest,
       CallbackSentOnUpdateToAccountsInCookieWithNoAccounts) {
  base::RunLoop run_loop;
  identity_manager_observer()->set_on_accounts_in_cookie_updated_callback(
      run_loop.QuitClosure());

  gaia_cookie_manager_service()->SetListAccountsResponseNoAccounts();
  gaia_cookie_manager_service()->TriggerListAccounts(
      "identity_manager_unittest");

  run_loop.Run();

  EXPECT_TRUE(identity_manager_observer()
                  ->accounts_from_cookie_change_callback()
                  .empty());
}

TEST_F(IdentityManagerTest,
       CallbackSentOnUpdateToAccountsInCookieWithOneAccount) {
  base::RunLoop run_loop;
  identity_manager_observer()->set_on_accounts_in_cookie_updated_callback(
      run_loop.QuitClosure());

  gaia_cookie_manager_service()->SetListAccountsResponseOneAccount(kTestEmail,
                                                                   kTestGaiaId);
  gaia_cookie_manager_service()->TriggerListAccounts(
      "identity_manager_unittest");
  run_loop.Run();

  EXPECT_EQ(1u, identity_manager_observer()
                    ->accounts_from_cookie_change_callback()
                    .size());

  AccountInfo account_info =
      identity_manager_observer()->accounts_from_cookie_change_callback()[0];
  EXPECT_EQ(account_tracker()->PickAccountIdForAccount(kTestGaiaId, kTestEmail),
            account_info.account_id);
  EXPECT_EQ(kTestGaiaId, account_info.gaia);
  EXPECT_EQ(kTestEmail, account_info.email);
}

TEST_F(IdentityManagerTest,
       CallbackSentOnUpdateToAccountsInCookieWithTwoAccounts) {
  base::RunLoop run_loop;
  identity_manager_observer()->set_on_accounts_in_cookie_updated_callback(
      run_loop.QuitClosure());

  gaia_cookie_manager_service()->SetListAccountsResponseTwoAccounts(
      kTestEmail, kTestGaiaId, kTestEmail2, kTestGaiaId2);
  gaia_cookie_manager_service()->TriggerListAccounts(
      "identity_manager_unittest");

  run_loop.Run();

  EXPECT_EQ(2u, identity_manager_observer()
                    ->accounts_from_cookie_change_callback()
                    .size());

  // Verify not only that both accounts are present but that they are listed in
  // the expected order as well.
  AccountInfo account_info1 =
      identity_manager_observer()->accounts_from_cookie_change_callback()[0];
  EXPECT_EQ(account_tracker()->PickAccountIdForAccount(kTestGaiaId, kTestEmail),
            account_info1.account_id);
  EXPECT_EQ(kTestGaiaId, account_info1.gaia);
  EXPECT_EQ(kTestEmail, account_info1.email);

  AccountInfo account_info2 =
      identity_manager_observer()->accounts_from_cookie_change_callback()[1];
  EXPECT_EQ(
      account_tracker()->PickAccountIdForAccount(kTestGaiaId2, kTestEmail2),
      account_info2.account_id);
  EXPECT_EQ(kTestGaiaId2, account_info2.gaia);
  EXPECT_EQ(kTestEmail2, account_info2.email);
}

TEST_F(IdentityManagerTest, GetAccountsInCookieJarWithNoAccounts) {
  base::RunLoop run_loop;
  identity_manager_observer()->set_on_accounts_in_cookie_updated_callback(
      run_loop.QuitClosure());

  gaia_cookie_manager_service()->SetListAccountsResponseNoAccounts();

  // Do an initial call to GetAccountsInCookieJar(). This call should return no
  // accounts but should also trigger an internal update and eventual
  // notification that the accounts in the cookie jar have been updated.
  std::vector<AccountInfo> accounts_in_cookie_jar =
      identity_manager()->GetAccountsInCookieJar("identity_manager_unittest");
  EXPECT_TRUE(accounts_in_cookie_jar.empty());

  run_loop.Run();

  // The state of the accounts in IdentityManager should now reflect the
  // internal update.
  accounts_in_cookie_jar =
      identity_manager()->GetAccountsInCookieJar("identity_manager_unittest");

  EXPECT_TRUE(accounts_in_cookie_jar.empty());
}

TEST_F(IdentityManagerTest, GetAccountsInCookieJarWithOneAccount) {
  base::RunLoop run_loop;
  identity_manager_observer()->set_on_accounts_in_cookie_updated_callback(
      run_loop.QuitClosure());

  gaia_cookie_manager_service()->SetListAccountsResponseOneAccount(kTestEmail,
                                                                   kTestGaiaId);

  // Do an initial call to GetAccountsInCookieJar(). This call should return no
  // accounts but should also trigger an internal update and eventual
  // notification that the accounts in the cookie jar have been updated.
  std::vector<AccountInfo> accounts_in_cookie_jar =
      identity_manager()->GetAccountsInCookieJar("identity_manager_unittest");
  EXPECT_TRUE(accounts_in_cookie_jar.empty());

  run_loop.Run();

  // The state of the accounts in IdentityManager should now reflect the
  // internal update.
  accounts_in_cookie_jar =
      identity_manager()->GetAccountsInCookieJar("identity_manager_unittest");

  EXPECT_EQ(1u, accounts_in_cookie_jar.size());

  AccountInfo account_info = accounts_in_cookie_jar[0];
  EXPECT_EQ(account_tracker()->PickAccountIdForAccount(kTestGaiaId, kTestEmail),
            account_info.account_id);
  EXPECT_EQ(kTestGaiaId, account_info.gaia);
  EXPECT_EQ(kTestEmail, account_info.email);
}

TEST_F(IdentityManagerTest, GetAccountsInCookieJarWithTwoAccounts) {
  base::RunLoop run_loop;
  identity_manager_observer()->set_on_accounts_in_cookie_updated_callback(
      run_loop.QuitClosure());

  gaia_cookie_manager_service()->SetListAccountsResponseTwoAccounts(
      kTestEmail, kTestGaiaId, kTestEmail2, kTestGaiaId2);

  // Do an initial call to GetAccountsInCookieJar(). This call should return no
  // accounts but should also trigger an internal update and eventual
  // notification that the accounts in the cookie jar have been updated.
  std::vector<AccountInfo> accounts_in_cookie_jar =
      identity_manager()->GetAccountsInCookieJar("identity_manager_unittest");
  EXPECT_TRUE(accounts_in_cookie_jar.empty());

  run_loop.Run();

  // The state of the accounts in IdentityManager should now reflect the
  // internal update.
  accounts_in_cookie_jar =
      identity_manager()->GetAccountsInCookieJar("identity_manager_unittest");

  EXPECT_EQ(2u, accounts_in_cookie_jar.size());

  // Verify not only that both accounts are present but that they are listed in
  // the expected order as well.
  AccountInfo account_info1 = accounts_in_cookie_jar[0];
  EXPECT_EQ(account_tracker()->PickAccountIdForAccount(kTestGaiaId, kTestEmail),
            account_info1.account_id);
  EXPECT_EQ(kTestGaiaId, account_info1.gaia);
  EXPECT_EQ(kTestEmail, account_info1.email);

  AccountInfo account_info2 = accounts_in_cookie_jar[1];
  EXPECT_EQ(
      account_tracker()->PickAccountIdForAccount(kTestGaiaId2, kTestEmail2),
      account_info2.account_id);
  EXPECT_EQ(kTestGaiaId2, account_info2.gaia);
  EXPECT_EQ(kTestEmail2, account_info2.email);
}

TEST_F(IdentityManagerTest,
       BatchChangeObserversAreNotifiedOnCredentialsUpdate) {
  signin_manager()->SetAuthenticatedAccountInfo(kTestGaiaId, kTestEmail);
  std::string account_id = signin_manager()->GetAuthenticatedAccountId();
  token_service()->UpdateCredentials(account_id, "refresh_token");

  EXPECT_EQ(1ul, identity_manager_observer()->batch_change_records().size());
  EXPECT_EQ(1ul,
            identity_manager_observer()->batch_change_records().at(0).size());
  EXPECT_EQ(account_id,
            identity_manager_observer()->batch_change_records().at(0).at(0));
}

}  // namespace identity
