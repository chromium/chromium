// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "build/build_config.h"
#include "components/signin/core/browser/account_info.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/identity/identity_service.h"
#include "services/identity/public/cpp/account_state.h"
#include "services/identity/public/cpp/scope_set.h"
#include "services/identity/public/mojom/constants.mojom.h"
#include "services/identity/public/mojom/identity_manager.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/cpp/service_test.h"
#include "services/service_manager/public/mojom/service_factory.mojom.h"

namespace identity {
namespace {

#if defined(OS_CHROMEOS)
using SigninManagerForTest = FakeSigninManagerBase;
#else
using SigninManagerForTest = FakeSigninManager;
#endif  // OS_CHROMEOS

const char kTestGaiaId[] = "dummyId";
const char kTestEmail[] = "me@dummy.com";
const char kSecondaryTestGaiaId[] = "secondaryDummyId";
const char kSecondaryTestEmail[] = "metoo@dummy.com";
const char kTestRefreshToken[] = "dummy-refresh-token";
const char kTestAccessToken[] = "access_token";

class ServiceTestClient : public service_manager::test::ServiceTestClient,
                          public service_manager::mojom::ServiceFactory {
 public:
  ServiceTestClient(service_manager::test::ServiceTest* test,
                    AccountTrackerService* account_tracker,
                    SigninManagerBase* signin_manager,
                    ProfileOAuth2TokenService* token_service)
      : service_manager::test::ServiceTestClient(test),
        account_tracker_(account_tracker),
        signin_manager_(signin_manager),
        token_service_(token_service) {
    registry_.AddInterface<service_manager::mojom::ServiceFactory>(
        base::BindRepeating(&ServiceTestClient::Create,
                            base::Unretained(this)));
  }

 protected:
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    registry_.BindInterface(interface_name, std::move(interface_pipe));
  }

  void CreateService(
      service_manager::mojom::ServiceRequest request,
      const std::string& name,
      service_manager::mojom::PIDReceiverPtr pid_receiver) override {
    if (name == mojom::kServiceName) {
      identity_service_context_.reset(new service_manager::ServiceContext(
          std::make_unique<IdentityService>(account_tracker_, signin_manager_,
                                            token_service_),
          std::move(request)));
    }
  }

  void Create(service_manager::mojom::ServiceFactoryRequest request) {
    service_factory_bindings_.AddBinding(this, std::move(request));
  }

 private:
  AccountTrackerService* account_tracker_;
  SigninManagerBase* signin_manager_;
  ProfileOAuth2TokenService* token_service_;
  service_manager::BinderRegistry registry_;
  mojo::BindingSet<service_manager::mojom::ServiceFactory>
      service_factory_bindings_;
  std::unique_ptr<service_manager::ServiceContext> identity_service_context_;
};

class IdentityManagerImplTest : public service_manager::test::ServiceTest {
 public:
  IdentityManagerImplTest()
      : ServiceTest("identity_unittests"),
        signin_client_(&pref_service_),
        token_service_(&pref_service_),
#if defined(OS_CHROMEOS)
        signin_manager_(&signin_client_, &account_tracker_) {
#else
        signin_manager_(&signin_client_,
                        &token_service_,
                        &account_tracker_,
                        nullptr) {
#endif
    AccountTrackerService::RegisterPrefs(pref_service_.registry());
    ProfileOAuth2TokenService::RegisterProfilePrefs(pref_service_.registry());
    SigninManagerBase::RegisterProfilePrefs(pref_service_.registry());
    SigninManagerBase::RegisterPrefs(pref_service_.registry());

    account_tracker_.Initialize(&pref_service_, base::FilePath());
  }

  void TearDown() override {
    // Shut down the SigninManager so that the IdentityManagerImpl doesn't end
    // up outliving it.
    signin_manager_.Shutdown();
    ServiceTest::TearDown();
  }

  void OnReceivedPrimaryAccountInfo(
      base::RepeatingClosure quit_closure,
      const base::Optional<AccountInfo>& account_info,
      const AccountState& account_state) {
    primary_account_info_ = account_info;
    primary_account_state_ = account_state;
    quit_closure.Run();
  }

  void OnPrimaryAccountAvailable(base::RepeatingClosure quit_closure,
                                 AccountInfo* caller_account_info,
                                 AccountState* caller_account_state,
                                 const AccountInfo& account_info,
                                 const AccountState& account_state) {
    *caller_account_info = account_info;
    *caller_account_state = account_state;
    quit_closure.Run();
  }

  void OnReceivedAccountInfoFromGaiaId(
      base::RepeatingClosure quit_closure,
      const base::Optional<AccountInfo>& account_info,
      const AccountState& account_state) {
    account_info_from_gaia_id_ = account_info;
    account_state_from_gaia_id_ = account_state;
    quit_closure.Run();
  }

  void OnGotAccounts(base::RepeatingClosure quit_closure,
                     std::vector<mojom::AccountPtr>* output,
                     std::vector<mojom::AccountPtr> accounts) {
    *output = std::move(accounts);
    quit_closure.Run();
  }

  void OnReceivedAccessToken(base::RepeatingClosure quit_closure,
                             const base::Optional<std::string>& access_token,
                             base::Time expiration_time,
                             const GoogleServiceAuthError& error) {
    access_token_ = access_token;
    access_token_error_ = error;
    quit_closure.Run();
  }

 protected:
  void SetUp() override { ServiceTest::SetUp(); }

  mojom::IdentityManager* GetIdentityManagerImpl() {
    if (!identity_manager_)
      connector()->BindInterface(mojom::kServiceName, &identity_manager_);
    return identity_manager_.get();
  }

  void ResetIdentityManagerImpl() { identity_manager_.reset(); }

  void FlushIdentityManagerImplForTesting() {
    GetIdentityManagerImpl();
    identity_manager_.FlushForTesting();
  }

  void SetIdentityManagerImplConnectionErrorHandler(
      base::RepeatingClosure handler) {
    GetIdentityManagerImpl();
    identity_manager_.set_connection_error_handler(handler);
  }

  // service_manager::test::ServiceTest:
  std::unique_ptr<service_manager::Service> CreateService() override {
    return std::make_unique<ServiceTestClient>(
        this, &account_tracker_, &signin_manager_, &token_service_);
  }

  mojom::IdentityManagerPtr identity_manager_;
  base::Optional<AccountInfo> primary_account_info_;
  AccountState primary_account_state_;
  base::Optional<AccountInfo> account_info_from_gaia_id_;
  AccountState account_state_from_gaia_id_;
  base::Optional<std::string> access_token_;
  GoogleServiceAuthError access_token_error_;

  AccountTrackerService* account_tracker() { return &account_tracker_; }
  SigninManagerBase* signin_manager() { return &signin_manager_; }
  FakeProfileOAuth2TokenService* token_service() { return &token_service_; }

 private:
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  AccountTrackerService account_tracker_;
  TestSigninClient signin_client_;
  FakeProfileOAuth2TokenService token_service_;
  SigninManagerForTest signin_manager_;

  DISALLOW_COPY_AND_ASSIGN(IdentityManagerImplTest);
};

// Tests that it is not possible to connect to the Identity Manager if
// initiated after SigninManager shutdown.
TEST_F(IdentityManagerImplTest, SigninManagerShutdownBeforeConnection) {
  AccountInfo sentinel;
  sentinel.account_id = "sentinel";
  primary_account_info_ = sentinel;

  // Ensure that the Identity Service has actually been created before
  // invoking SigninManagerBase::Shutdown(), since otherwise this test will
  // spin forever. Then reset the Identity Manager so that the next request
  // makes a fresh connection.
  FlushIdentityManagerImplForTesting();
  ResetIdentityManagerImpl();

  // Make a call to connect to the IdentityManagerImpl *after* SigninManager
  // shutdown; it should get notified of an error when the Identity Service
  // drops the connection.
  signin_manager()->Shutdown();
  base::RunLoop run_loop;
  SetIdentityManagerImplConnectionErrorHandler(run_loop.QuitClosure());

  GetIdentityManagerImpl()->GetPrimaryAccountInfo(base::BindRepeating(
      &IdentityManagerImplTest::OnReceivedPrimaryAccountInfo,
      base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  // Verify that the callback to GetPrimaryAccountInfo() was not invoked.
  EXPECT_TRUE(primary_account_info_);
  EXPECT_EQ("sentinel", primary_account_info_->account_id);
}

// Tests that the Identity Manager destroys itself on SigninManager shutdown.
TEST_F(IdentityManagerImplTest, SigninManagerShutdownAfterConnection) {
  base::RunLoop run_loop;
  SetIdentityManagerImplConnectionErrorHandler(run_loop.QuitClosure());

  // Ensure that the IdentityManagerImpl instance has actually been created
  // before invoking SigninManagerBase::Shutdown(), since otherwise this test
  // will spin forever.
  FlushIdentityManagerImplForTesting();
  signin_manager()->Shutdown();
  run_loop.Run();
}

// Tests that the Identity Manager properly handles its own destruction in the
// case where there is an active consumer request (i.e., a pending callback from
// a Mojo call). In particular, this flow should not cause a DCHECK to fire in
// debug mode.
TEST_F(IdentityManagerImplTest, IdentityManagerImplShutdownWithActiveRequest) {
  base::RunLoop run_loop;
  SetIdentityManagerImplConnectionErrorHandler(run_loop.QuitClosure());

  // Call a method on the IdentityManagerImpl that will cause it to store a
  // pending callback. This callback will never be invoked, so just pass dummy
  // arguments to it.
  GetIdentityManagerImpl()->GetPrimaryAccountWhenAvailable(base::BindRepeating(
      &IdentityManagerImplTest::OnPrimaryAccountAvailable,
      base::Unretained(this), base::RepeatingClosure(), nullptr, nullptr));

  // Ensure that the IdentityManagerImpl has received the above call before
  // invoking SigninManagerBase::Shutdown(), as otherwise this test is
  // pointless.
  FlushIdentityManagerImplForTesting();

  // This flow is what would cause a DCHECK to fire if IdentityManagerImpl is
  // not properly closing its binding on shutdown.
  signin_manager()->Shutdown();
  run_loop.Run();
}

// Check that the primary account info is null if not signed in.
TEST_F(IdentityManagerImplTest, GetPrimaryAccountInfoNotSignedIn) {
  base::RunLoop run_loop;
  GetIdentityManagerImpl()->GetPrimaryAccountInfo(base::BindRepeating(
      &IdentityManagerImplTest::OnReceivedPrimaryAccountInfo,
      base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_FALSE(primary_account_info_);
}

// Check that the primary account info has expected values if signed in without
// a refresh token available.
TEST_F(IdentityManagerImplTest, GetPrimaryAccountInfoSignedInNoRefreshToken) {
  signin_manager()->SetAuthenticatedAccountInfo(kTestGaiaId, kTestEmail);
  base::RunLoop run_loop;
  GetIdentityManagerImpl()->GetPrimaryAccountInfo(base::BindRepeating(
      &IdentityManagerImplTest::OnReceivedPrimaryAccountInfo,
      base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(primary_account_info_);
  EXPECT_EQ(signin_manager()->GetAuthenticatedAccountId(),
            primary_account_info_->account_id);
  EXPECT_EQ(kTestGaiaId, primary_account_info_->gaia);
  EXPECT_EQ(kTestEmail, primary_account_info_->email);
  EXPECT_FALSE(primary_account_state_.has_refresh_token);
  EXPECT_TRUE(primary_account_state_.is_primary_account);
}

// Check that the primary account info has expected values if signed in with a
// refresh token available.
TEST_F(IdentityManagerImplTest, GetPrimaryAccountInfoSignedInRefreshToken) {
  signin_manager()->SetAuthenticatedAccountInfo(kTestGaiaId, kTestEmail);
  token_service()->UpdateCredentials(
      signin_manager()->GetAuthenticatedAccountId(), kTestRefreshToken);
  base::RunLoop run_loop;
  GetIdentityManagerImpl()->GetPrimaryAccountInfo(base::BindRepeating(
      &IdentityManagerImplTest::OnReceivedPrimaryAccountInfo,
      base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(primary_account_info_);
  EXPECT_EQ(signin_manager()->GetAuthenticatedAccountId(),
            primary_account_info_->account_id);
  EXPECT_EQ(kTestGaiaId, primary_account_info_->gaia);
  EXPECT_EQ(kTestEmail, primary_account_info_->email);
  EXPECT_TRUE(primary_account_state_.has_refresh_token);
  EXPECT_TRUE(primary_account_state_.is_primary_account);
}

// Check that GetPrimaryAccountWhenAvailable() returns immediately in the
// case where the primary account is available when the call is received.
TEST_F(IdentityManagerImplTest, GetPrimaryAccountWhenAvailableSignedIn) {
  signin_manager()->SetAuthenticatedAccountInfo(kTestGaiaId, kTestEmail);
  token_service()->UpdateCredentials(
      signin_manager()->GetAuthenticatedAccountId(), kTestRefreshToken);

  AccountInfo account_info;
  AccountState account_state;
  base::RunLoop run_loop;
  GetIdentityManagerImpl()->GetPrimaryAccountWhenAvailable(base::BindRepeating(
      &IdentityManagerImplTest::OnPrimaryAccountAvailable,
      base::Unretained(this), run_loop.QuitClosure(),
      base::Unretained(&account_info), base::Unretained(&account_state)));
  run_loop.Run();

  EXPECT_EQ(signin_manager()->GetAuthenticatedAccountId(),
            account_info.account_id);
  EXPECT_EQ(kTestGaiaId, account_info.gaia);
  EXPECT_EQ(kTestEmail, account_info.email);
  EXPECT_TRUE(account_state.has_refresh_token);
  EXPECT_TRUE(account_state.is_primary_account);
}

// Check that GetPrimaryAccountWhenAvailable() returns the expected account
// info in the case where the primary account is made available *after* the
// call is received.
TEST_F(IdentityManagerImplTest, GetPrimaryAccountWhenAvailableSignInLater) {
  AccountInfo account_info;
  AccountState account_state;

  base::RunLoop run_loop;
  GetIdentityManagerImpl()->GetPrimaryAccountWhenAvailable(base::BindRepeating(
      &IdentityManagerImplTest::OnPrimaryAccountAvailable,
      base::Unretained(this), run_loop.QuitClosure(),
      base::Unretained(&account_info), base::Unretained(&account_state)));

  // Verify that the primary account info is not currently available (this also
  // serves to ensure that the preceding call has been received by the Identity
  // Manager before proceeding).
  base::RunLoop run_loop2;
  GetIdentityManagerImpl()->GetPrimaryAccountInfo(base::BindRepeating(
      &IdentityManagerImplTest::OnReceivedPrimaryAccountInfo,
      base::Unretained(this), run_loop2.QuitClosure()));
  run_loop2.Run();
  EXPECT_FALSE(primary_account_info_);

  // Make the primary account available and check that the callback is invoked
  // as expected.
  signin_manager()->SetAuthenticatedAccountInfo(kTestGaiaId, kTestEmail);
  token_service()->UpdateCredentials(
      signin_manager()->GetAuthenticatedAccountId(), kTestRefreshToken);
  run_loop.Run();

  EXPECT_EQ(signin_manager()->GetAuthenticatedAccountId(),
            account_info.account_id);
  EXPECT_EQ(kTestGaiaId, account_info.gaia);
  EXPECT_EQ(kTestEmail, account_info.email);
  EXPECT_TRUE(account_state.has_refresh_token);
  EXPECT_TRUE(account_state.is_primary_account);
}

// Check that GetPrimaryAccountWhenAvailable() returns the expected account
// info in the case where signin is done before the call is received but the
// refresh token is made available only *after* the call is received.
TEST_F(IdentityManagerImplTest,
       GetPrimaryAccountWhenAvailableTokenAvailableLater) {
  AccountInfo account_info;
  AccountState account_state;

  // Sign in, but don't set the refresh token yet.
  signin_manager()->SetAuthenticatedAccountInfo(kTestGaiaId, kTestEmail);
  base::RunLoop run_loop;
  GetIdentityManagerImpl()->GetPrimaryAccountWhenAvailable(base::BindRepeating(
      &IdentityManagerImplTest::OnPrimaryAccountAvailable,
      base::Unretained(this), run_loop.QuitClosure(),
      base::Unretained(&account_info), base::Unretained(&account_state)));

  // Verify that the primary account info is present, but that the primary
  // account is not yet considered available (this also
  // serves to ensure that the preceding call has been received by the Identity
  // Manager before proceeding).
  base::RunLoop run_loop2;
  GetIdentityManagerImpl()->GetPrimaryAccountInfo(base::BindRepeating(
      &IdentityManagerImplTest::OnReceivedPrimaryAccountInfo,
      base::Unretained(this), run_loop2.QuitClosure()));
  run_loop2.Run();

  EXPECT_TRUE(primary_account_info_);
  EXPECT_EQ(signin_manager()->GetAuthenticatedAccountId(),
            primary_account_info_->account_id);
  EXPECT_TRUE(account_info.account_id.empty());

  // Set the refresh token and check that the callback is invoked as expected
  // (i.e., the primary account is now considered available).
  token_service()->UpdateCredentials(
      signin_manager()->GetAuthenticatedAccountId(), kTestRefreshToken);
  run_loop.Run();

  EXPECT_EQ(signin_manager()->GetAuthenticatedAccountId(),
            account_info.account_id);
  EXPECT_EQ(kTestGaiaId, account_info.gaia);
  EXPECT_EQ(kTestEmail, account_info.email);
  EXPECT_TRUE(account_state.has_refresh_token);
  EXPECT_TRUE(account_state.is_primary_account);
}

// Check that GetPrimaryAccountWhenAvailable() returns the expected account info
// in the case where the token is available before the call is received but the
// account is made authenticated only *after* the call is received. This test is
// relevant only on non-ChromeOS platforms, as the flow being tested here is not
// possible on ChromeOS.
#if !defined(OS_CHROMEOS)
TEST_F(IdentityManagerImplTest,
       GetPrimaryAccountWhenAvailableAuthenticationAvailableLater) {
  AccountInfo account_info;
  AccountState account_state;

  // Set the refresh token, but don't sign in yet.
  std::string account_id_to_use =
      account_tracker()->SeedAccountInfo(kTestGaiaId, kTestEmail);
  token_service()->UpdateCredentials(account_id_to_use, kTestRefreshToken);
  base::RunLoop run_loop;
  GetIdentityManagerImpl()->GetPrimaryAccountWhenAvailable(base::BindRepeating(
      &IdentityManagerImplTest::OnPrimaryAccountAvailable,
      base::Unretained(this), run_loop.QuitClosure(),
      base::Unretained(&account_info), base::Unretained(&account_state)));

  // Verify that the account is present and has a refresh token, but that the
  // primary account is not yet considered available (this also serves to ensure
  // that the preceding call has been received by the Identity Manager before
  // proceeding).
  base::RunLoop run_loop2;
  GetIdentityManagerImpl()->GetAccountInfoFromGaiaId(
      kTestGaiaId,
      base::BindRepeating(
          &IdentityManagerImplTest::OnReceivedAccountInfoFromGaiaId,
          base::Unretained(this), run_loop2.QuitClosure()));
  run_loop2.Run();

  EXPECT_TRUE(account_info_from_gaia_id_);
  EXPECT_EQ(account_id_to_use, account_info_from_gaia_id_->account_id);
  EXPECT_EQ(kTestGaiaId, account_info_from_gaia_id_->gaia);
  EXPECT_EQ(kTestEmail, account_info_from_gaia_id_->email);
  EXPECT_TRUE(account_state_from_gaia_id_.has_refresh_token);
  EXPECT_FALSE(account_state_from_gaia_id_.is_primary_account);

  EXPECT_TRUE(account_info.account_id.empty());

  // Sign the user in and check that the callback is invoked as expected (i.e.,
  // the primary account is now considered available). Note that it is necessary
  // to call SignIn() here to ensure that GoogleSigninSucceeded() is fired by
  // the fake signin manager.
  static_cast<FakeSigninManager*>(signin_manager())
      ->SignIn(kTestGaiaId, kTestEmail, "password");

  run_loop.Run();

  EXPECT_EQ(signin_manager()->GetAuthenticatedAccountId(),
            account_info.account_id);
  EXPECT_EQ(kTestGaiaId, account_info.gaia);
  EXPECT_EQ(kTestEmail, account_info.email);
  EXPECT_TRUE(account_state.has_refresh_token);
  EXPECT_TRUE(account_state.is_primary_account);
}
#endif

// Check that GetPrimaryAccountWhenAvailable() returns the expected account
// info to all callers in the case where the primary account is made available
// after multiple overlapping calls have been received.
TEST_F(IdentityManagerImplTest,
       GetPrimaryAccountWhenAvailableOverlappingCalls) {
  AccountInfo account_info1;
  AccountState account_state1;
  base::RunLoop run_loop;
  GetIdentityManagerImpl()->GetPrimaryAccountWhenAvailable(base::BindRepeating(
      &IdentityManagerImplTest::OnPrimaryAccountAvailable,
      base::Unretained(this), run_loop.QuitClosure(),
      base::Unretained(&account_info1), base::Unretained(&account_state1)));

  AccountInfo account_info2;
  AccountState account_state2;
  base::RunLoop run_loop2;
  GetIdentityManagerImpl()->GetPrimaryAccountWhenAvailable(base::BindRepeating(
      &IdentityManagerImplTest::OnPrimaryAccountAvailable,
      base::Unretained(this), run_loop2.QuitClosure(),
      base::Unretained(&account_info2), base::Unretained(&account_state2)));

  // Verify that the primary account info is not currently available (this also
  // serves to ensure that the preceding call has been received by the Identity
  // Manager before proceeding).
  base::RunLoop run_loop3;
  GetIdentityManagerImpl()->GetPrimaryAccountInfo(base::BindRepeating(
      &IdentityManagerImplTest::OnReceivedPrimaryAccountInfo,
      base::Unretained(this), run_loop3.QuitClosure()));
  run_loop3.Run();
  EXPECT_FALSE(primary_account_info_);

  // Make the primary account available and check that the callbacks are invoked
  // as expected.
  signin_manager()->SetAuthenticatedAccountInfo(kTestGaiaId, kTestEmail);
  token_service()->UpdateCredentials(
      signin_manager()->GetAuthenticatedAccountId(), kTestRefreshToken);
  run_loop.Run();
  run_loop2.Run();

  EXPECT_EQ(signin_manager()->GetAuthenticatedAccountId(),
            account_info1.account_id);
  EXPECT_EQ(kTestGaiaId, account_info1.gaia);
  EXPECT_EQ(kTestEmail, account_info1.email);
  EXPECT_TRUE(account_state1.has_refresh_token);
  EXPECT_TRUE(account_state1.is_primary_account);

  EXPECT_EQ(signin_manager()->GetAuthenticatedAccountId(),
            account_info2.account_id);
  EXPECT_EQ(kTestGaiaId, account_info2.gaia);
  EXPECT_EQ(kTestEmail, account_info2.email);
  EXPECT_TRUE(account_state2.has_refresh_token);
  EXPECT_TRUE(account_state2.is_primary_account);
}

// Check that GetPrimaryAccountWhenAvailable() doesn't return the account as
// available if the refresh token has an auth error.
TEST_F(IdentityManagerImplTest,
       GetPrimaryAccountWhenAvailableRefreshTokenHasAuthError) {
  signin_manager()->SetAuthenticatedAccountInfo(kTestGaiaId, kTestEmail);
  token_service()->UpdateCredentials(
      signin_manager()->GetAuthenticatedAccountId(), kTestRefreshToken);
  token_service()->UpdateAuthErrorForTesting(
      signin_manager()->GetAuthenticatedAccountId(),
      GoogleServiceAuthError(
          GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS));

  AccountInfo account_info;
  AccountState account_state;
  base::RunLoop run_loop;
  GetIdentityManagerImpl()->GetPrimaryAccountWhenAvailable(base::BindRepeating(
      &IdentityManagerImplTest::OnPrimaryAccountAvailable,
      base::Unretained(this), run_loop.QuitClosure(),
      base::Unretained(&account_info), base::Unretained(&account_state)));

  // Flush the Identity Manager and check that the callback didn't fire.
  FlushIdentityManagerImplForTesting();
  EXPECT_TRUE(account_info.account_id.empty());

  // Clear the auth error, update credentials, and check that the callback
  // fires.
  token_service()->UpdateAuthErrorForTesting(
      signin_manager()->GetAuthenticatedAccountId(), GoogleServiceAuthError());
  token_service()->UpdateCredentials(
      signin_manager()->GetAuthenticatedAccountId(), kTestRefreshToken);
  run_loop.Run();

  EXPECT_EQ(signin_manager()->GetAuthenticatedAccountId(),
            account_info.account_id);
  EXPECT_EQ(kTestGaiaId, account_info.gaia);
  EXPECT_EQ(kTestEmail, account_info.email);
  EXPECT_TRUE(account_state.has_refresh_token);
  EXPECT_TRUE(account_state.is_primary_account);
}

// Check that the account info for a given GAIA ID is null if that GAIA ID is
// unknown.
TEST_F(IdentityManagerImplTest, GetAccountInfoForUnknownGaiaID) {
  base::RunLoop run_loop;
  GetIdentityManagerImpl()->GetAccountInfoFromGaiaId(
      kTestGaiaId,
      base::BindRepeating(
          &IdentityManagerImplTest::OnReceivedAccountInfoFromGaiaId,
          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_FALSE(account_info_from_gaia_id_);
}

// Check that the account info for a given GAIA ID has expected values if that
// GAIA ID is known and there is no refresh token available for it.
TEST_F(IdentityManagerImplTest, GetAccountInfoForKnownGaiaIdNoRefreshToken) {
  std::string account_id =
      account_tracker()->SeedAccountInfo(kTestGaiaId, kTestEmail);
  base::RunLoop run_loop;
  GetIdentityManagerImpl()->GetAccountInfoFromGaiaId(
      kTestGaiaId,
      base::BindRepeating(
          &IdentityManagerImplTest::OnReceivedAccountInfoFromGaiaId,
          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(account_info_from_gaia_id_);
  EXPECT_EQ(account_id, account_info_from_gaia_id_->account_id);
  EXPECT_EQ(kTestGaiaId, account_info_from_gaia_id_->gaia);
  EXPECT_EQ(kTestEmail, account_info_from_gaia_id_->email);
  EXPECT_FALSE(account_state_from_gaia_id_.has_refresh_token);
  EXPECT_FALSE(account_state_from_gaia_id_.is_primary_account);
}

// Check that the account info for a given GAIA ID has expected values if that
// GAIA ID is known and has a refresh token available.
TEST_F(IdentityManagerImplTest, GetAccountInfoForKnownGaiaIdRefreshToken) {
  std::string account_id =
      account_tracker()->SeedAccountInfo(kTestGaiaId, kTestEmail);
  token_service()->UpdateCredentials(account_id, kTestRefreshToken);
  base::RunLoop run_loop;
  GetIdentityManagerImpl()->GetAccountInfoFromGaiaId(
      kTestGaiaId,
      base::BindRepeating(
          &IdentityManagerImplTest::OnReceivedAccountInfoFromGaiaId,
          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(account_info_from_gaia_id_);
  EXPECT_EQ(account_id, account_info_from_gaia_id_->account_id);
  EXPECT_EQ(kTestGaiaId, account_info_from_gaia_id_->gaia);
  EXPECT_EQ(kTestEmail, account_info_from_gaia_id_->email);
  EXPECT_TRUE(account_state_from_gaia_id_.has_refresh_token);
  EXPECT_FALSE(account_state_from_gaia_id_.is_primary_account);
}

// Check the implementation of GetAccounts() when there are no accounts.
TEST_F(IdentityManagerImplTest, GetAccountsNoAccount) {
  token_service()->LoadCredentials("dummy");

  std::vector<mojom::AccountPtr> accounts;

  // Check that an empty list is returned when there are no accounts.
  base::RunLoop run_loop;
  GetIdentityManagerImpl()->GetAccounts(base::BindRepeating(
      &IdentityManagerImplTest::OnGotAccounts, base::Unretained(this),
      run_loop.QuitClosure(), &accounts));
  run_loop.Run();
  EXPECT_EQ(0u, accounts.size());
}

// Check the implementation of GetAccounts() when there is a single account,
// which is the primary account.
TEST_F(IdentityManagerImplTest, GetAccountsPrimaryAccount) {
  token_service()->LoadCredentials("dummy");
  std::vector<mojom::AccountPtr> accounts;

  // Add a primary account.
  signin_manager()->SetAuthenticatedAccountInfo(kTestGaiaId, kTestEmail);
  token_service()->UpdateCredentials(
      signin_manager()->GetAuthenticatedAccountId(), kTestRefreshToken);

  base::RunLoop run_loop;
  GetIdentityManagerImpl()->GetAccounts(base::BindRepeating(
      &IdentityManagerImplTest::OnGotAccounts, base::Unretained(this),
      run_loop.QuitClosure(), &accounts));
  run_loop.Run();

  // Verify that |accounts| contains the primary account.
  EXPECT_EQ(1u, accounts.size());
  const mojom::AccountPtr& primary_account = accounts[0];
  EXPECT_EQ(signin_manager()->GetAuthenticatedAccountId(),
            primary_account->info.account_id);
  EXPECT_EQ(kTestGaiaId, primary_account->info.gaia);
  EXPECT_EQ(kTestEmail, primary_account->info.email);
  EXPECT_TRUE(primary_account->state.has_refresh_token);
  EXPECT_TRUE(primary_account->state.is_primary_account);
}

// Check the implementation of GetAccounts() when there are multiple accounts,
// in particular that ProfileOAuth2TokenService is the source of truth for
// whether an account is present.
TEST_F(IdentityManagerImplTest, GetAccountsMultipleAccounts) {
  token_service()->LoadCredentials("dummy");
  std::vector<mojom::AccountPtr> accounts;

  // Add a primary account.
  signin_manager()->SetAuthenticatedAccountInfo(kTestGaiaId, kTestEmail);
  token_service()->UpdateCredentials(
      signin_manager()->GetAuthenticatedAccountId(), kTestRefreshToken);

  // Add a secondary account with AccountTrackerService, but don't yet make
  // ProfileOAuth2TokenService aware of it.
  std::string secondary_account_id = account_tracker()->SeedAccountInfo(
      kSecondaryTestGaiaId, kSecondaryTestEmail);
  base::RunLoop run_loop;
  GetIdentityManagerImpl()->GetAccounts(base::BindRepeating(
      &IdentityManagerImplTest::OnGotAccounts, base::Unretained(this),
      run_loop.QuitClosure(), &accounts));
  run_loop.Run();

  // Verify that |accounts| contains only the primary account at this time.
  EXPECT_EQ(1u, accounts.size());
  const mojom::AccountPtr& primary_account = accounts[0];
  EXPECT_EQ(signin_manager()->GetAuthenticatedAccountId(),
            primary_account->info.account_id);
  EXPECT_EQ(kTestGaiaId, primary_account->info.gaia);
  EXPECT_EQ(kTestEmail, primary_account->info.email);
  EXPECT_TRUE(primary_account->state.has_refresh_token);
  EXPECT_TRUE(primary_account->state.is_primary_account);

  // Make PO2TS aware of the secondary account.
  token_service()->UpdateCredentials(secondary_account_id, kTestRefreshToken);
  base::RunLoop run_loop2;
  GetIdentityManagerImpl()->GetAccounts(base::BindRepeating(
      &IdentityManagerImplTest::OnGotAccounts, base::Unretained(this),
      run_loop2.QuitClosure(), &accounts));
  run_loop2.Run();

  // Verify that |accounts| contains both accounts, with the primary account
  // being first and having the same information as previously.
  EXPECT_EQ(2u, accounts.size());
  const mojom::AccountPtr& primary_account_redux = accounts[0];
  EXPECT_EQ(signin_manager()->GetAuthenticatedAccountId(),
            primary_account_redux->info.account_id);
  EXPECT_EQ(kTestGaiaId, primary_account_redux->info.gaia);
  EXPECT_EQ(kTestEmail, primary_account_redux->info.email);
  EXPECT_TRUE(primary_account_redux->state.has_refresh_token);
  EXPECT_TRUE(primary_account_redux->state.is_primary_account);

  const mojom::AccountPtr& secondary_account = accounts[1];
  EXPECT_EQ(secondary_account_id, secondary_account->info.account_id);
  EXPECT_EQ(kSecondaryTestGaiaId, secondary_account->info.gaia);
  EXPECT_EQ(kSecondaryTestEmail, secondary_account->info.email);
  EXPECT_TRUE(secondary_account->state.has_refresh_token);
  EXPECT_FALSE(secondary_account->state.is_primary_account);
}

// Check that the expected error is received if requesting an access token when
// not signed in.
TEST_F(IdentityManagerImplTest, GetAccessTokenNotSignedIn) {
  base::RunLoop run_loop;
  GetIdentityManagerImpl()->GetAccessToken(
      kTestGaiaId, ScopeSet(), "dummy_consumer",
      base::BindRepeating(&IdentityManagerImplTest::OnReceivedAccessToken,
                          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_FALSE(access_token_);
  EXPECT_EQ(GoogleServiceAuthError::State::USER_NOT_SIGNED_UP,
            access_token_error_.state());
}

// Check that the expected access token is received if requesting an access
// token when signed in.
TEST_F(IdentityManagerImplTest, GetAccessTokenSignedIn) {
  signin_manager()->SetAuthenticatedAccountInfo(kTestGaiaId, kTestEmail);
  std::string account_id = signin_manager()->GetAuthenticatedAccountId();
  token_service()->UpdateCredentials(account_id, kTestRefreshToken);
  token_service()->set_auto_post_fetch_response_on_message_loop(true);
  base::RunLoop run_loop;

  GetIdentityManagerImpl()->GetAccessToken(
      account_id, ScopeSet(), "dummy_consumer",
      base::BindRepeating(&IdentityManagerImplTest::OnReceivedAccessToken,
                          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(access_token_);
  EXPECT_EQ(kTestAccessToken, access_token_.value());
  EXPECT_EQ(GoogleServiceAuthError::State::NONE, access_token_error_.state());
}

}  // namespace
}  // namespace identity
