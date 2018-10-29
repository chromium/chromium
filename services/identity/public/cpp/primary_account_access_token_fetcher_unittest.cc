// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/public/cpp/primary_account_access_token_fetcher.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/identity/public/cpp/identity_test_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gmock_mutant.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::MockCallback;
using testing::CallbackToFunctor;
using testing::InvokeWithoutArgs;
using testing::StrictMock;

namespace identity {

namespace {

void OnAccessTokenFetchComplete(
    base::OnceClosure done_closure,
    const GoogleServiceAuthError& expected_error,
    const AccessTokenInfo& expected_access_token_info,
    GoogleServiceAuthError error,
    AccessTokenInfo access_token_info) {
  EXPECT_EQ(expected_error, error);
  if (expected_error == GoogleServiceAuthError::AuthErrorNone())
    EXPECT_EQ(expected_access_token_info, access_token_info);

  std::move(done_closure).Run();
}

}  // namespace

class PrimaryAccountAccessTokenFetcherTest : public testing::Test,
                                             public IdentityManager::Observer {
 public:
  using TestTokenCallback =
      StrictMock<MockCallback<AccessTokenFetcher::TokenCallback>>;

  PrimaryAccountAccessTokenFetcherTest()
      : access_token_info_("access token",
                           base::Time::Now() + base::TimeDelta::FromHours(1),
                           "id_token") {}

  ~PrimaryAccountAccessTokenFetcherTest() override {
  }

  std::unique_ptr<PrimaryAccountAccessTokenFetcher> CreateFetcher(
      AccessTokenFetcher::TokenCallback callback,
      PrimaryAccountAccessTokenFetcher::Mode mode) {
    std::set<std::string> scopes{"scope"};
    return std::make_unique<PrimaryAccountAccessTokenFetcher>(
        "test_consumer", identity_test_env_.identity_manager(), scopes,
        std::move(callback), mode);
  }

  IdentityTestEnvironment* identity_test_env() { return &identity_test_env_; }

  // Signs the user in to the primary account, returning the account ID.
  std::string SignIn() {
    return identity_test_env_.MakePrimaryAccountAvailable("me@gmail.com")
        .account_id;
  }

  // Returns an AccessTokenInfo with valid information that can be used for
  // completing access token requests.
  const AccessTokenInfo& access_token_info() const {
    return access_token_info_;
  }

 private:
  base::MessageLoop message_loop_;
  IdentityTestEnvironment identity_test_env_;
  AccessTokenInfo access_token_info_;
};

TEST_F(PrimaryAccountAccessTokenFetcherTest, OneShotShouldReturnAccessToken) {
  TestTokenCallback callback;

  std::string account_id = SignIn();

  // Signed in and refresh token already exists, so this should result in a
  // request for an access token.
  auto fetcher = CreateFetcher(
      callback.Get(), PrimaryAccountAccessTokenFetcher::Mode::kImmediate);

  // Once the access token request is fulfilled, we should get called back with
  // the access token.
  EXPECT_CALL(callback, Run(GoogleServiceAuthError::AuthErrorNone(),
                            access_token_info()));
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      access_token_info().token, access_token_info().expiration_time,
      access_token_info().id_token);
}

TEST_F(PrimaryAccountAccessTokenFetcherTest,
       WaitAndRetryShouldReturnAccessToken) {
  TestTokenCallback callback;

  std::string account_id = SignIn();

  // Signed in and refresh token already exists, so this should result in a
  // request for an access token.
  auto fetcher = CreateFetcher(
      callback.Get(),
      PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable);

  // Once the access token request is fulfilled, we should get called back with
  // the access token.
  EXPECT_CALL(callback, Run(GoogleServiceAuthError::AuthErrorNone(),
                            access_token_info()));
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      access_token_info().token, access_token_info().expiration_time,
      access_token_info().id_token);
}

TEST_F(PrimaryAccountAccessTokenFetcherTest, ShouldNotReplyIfDestroyed) {
  TestTokenCallback callback;

  std::string account_id = SignIn();

  // Signed in and refresh token already exists, so this should result in a
  // request for an access token.
  auto fetcher = CreateFetcher(
      callback.Get(), PrimaryAccountAccessTokenFetcher::Mode::kImmediate);

  // Destroy the fetcher before the access token request is fulfilled.
  fetcher.reset();

  // Fulfilling the request now should have no effect.
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      access_token_info().token, access_token_info().expiration_time,
      access_token_info().id_token);
}

TEST_F(PrimaryAccountAccessTokenFetcherTest, OneShotCallsBackWhenSignedOut) {
  base::RunLoop run_loop;

  // Signed out -> we should get called back.
  auto fetcher = CreateFetcher(
      base::BindOnce(&OnAccessTokenFetchComplete, run_loop.QuitClosure(),
                     GoogleServiceAuthError(
                         GoogleServiceAuthError::State::USER_NOT_SIGNED_UP),
                     AccessTokenInfo()),
      PrimaryAccountAccessTokenFetcher::Mode::kImmediate);

  run_loop.Run();
}

TEST_F(PrimaryAccountAccessTokenFetcherTest,
       OneShotCallsBackWhenNoRefreshToken) {
  base::RunLoop run_loop;

  identity_test_env()->SetPrimaryAccount("me@gmail.com");

  // Signed in, but there is no refresh token -> we should get called back.
  auto fetcher = CreateFetcher(
      base::BindOnce(&OnAccessTokenFetchComplete, run_loop.QuitClosure(),
                     GoogleServiceAuthError(
                         GoogleServiceAuthError::State::USER_NOT_SIGNED_UP),
                     AccessTokenInfo()),
      PrimaryAccountAccessTokenFetcher::Mode::kImmediate);

  run_loop.Run();
}

TEST_F(PrimaryAccountAccessTokenFetcherTest,
       WaitAndRetryNoCallbackWhenSignedOut) {
  TestTokenCallback callback;

  // Signed out -> the fetcher should wait for a sign-in which never happens
  // in this test, so we shouldn't get called back.
  auto fetcher = CreateFetcher(
      callback.Get(),
      PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable);
}

// Tests related to waiting for sign-in don't apply on ChromeOS (it doesn't have
// that concept).
#if !defined(OS_CHROMEOS)

TEST_F(PrimaryAccountAccessTokenFetcherTest, ShouldWaitForSignIn) {
  TestTokenCallback callback;

  // Not signed in, so this should wait for a sign-in to complete.
  auto fetcher = CreateFetcher(
      callback.Get(),
      PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable);

  std::string account_id = SignIn();

  // Once the access token request is fulfilled, we should get called back with
  // the access token.
  EXPECT_CALL(callback, Run(GoogleServiceAuthError::AuthErrorNone(),
                            access_token_info()));
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      access_token_info().token, access_token_info().expiration_time,
      access_token_info().id_token);

  // The request should not have to have been retried.
  EXPECT_FALSE(fetcher->access_token_request_retried());
}

#endif  // !OS_CHROMEOS

TEST_F(PrimaryAccountAccessTokenFetcherTest, ShouldWaitForRefreshToken) {
  TestTokenCallback callback;

  std::string account_id =
      identity_test_env()->SetPrimaryAccount("me@gmail.com").account_id;

  // Signed in, but there is no refresh token -> we should not get called back
  // (yet).
  auto fetcher = CreateFetcher(
      callback.Get(),
      PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable);

  // Getting a refresh token should result in a request for an access token.
  identity_test_env()->SetRefreshTokenForPrimaryAccount();

  // Once the access token request is fulfilled, we should get called back with
  // the access token.
  EXPECT_CALL(callback, Run(GoogleServiceAuthError::AuthErrorNone(),
                            access_token_info()));
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      access_token_info().token, access_token_info().expiration_time,
      access_token_info().id_token);

  // The request should not have to have been retried.
  EXPECT_FALSE(fetcher->access_token_request_retried());
}

TEST_F(PrimaryAccountAccessTokenFetcherTest,
       ShouldIgnoreRefreshTokensForOtherAccounts) {
  TestTokenCallback callback;

  // Signed-in to account_id, but there's only a refresh token for a different
  // account.
  std::string account_id =
      identity_test_env()->SetPrimaryAccount("me@gmail.com").account_id;
  identity_test_env()->MakeAccountAvailable(account_id + "2");

  // The fetcher should wait for the correct refresh token.
  auto fetcher = CreateFetcher(
      callback.Get(),
      PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable);

  // A refresh token for yet another account shouldn't matter either.
  identity_test_env()->MakeAccountAvailable(account_id + "3");
}

TEST_F(PrimaryAccountAccessTokenFetcherTest,
       OneShotCanceledAccessTokenRequest) {
  std::string account_id = SignIn();

  base::RunLoop run_loop;

  // Signed in and refresh token already exists, so this should result in a
  // request for an access token.
  auto fetcher = CreateFetcher(
      base::BindOnce(
          &OnAccessTokenFetchComplete, run_loop.QuitClosure(),
          GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED),
          AccessTokenInfo()),
      PrimaryAccountAccessTokenFetcher::Mode::kImmediate);

  // A canceled access token request should result in a callback.
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED));
}

TEST_F(PrimaryAccountAccessTokenFetcherTest,
       WaitAndRetryCanceledAccessTokenRequest) {
  TestTokenCallback callback;

  std::string account_id = SignIn();

  // Signed in and refresh token already exists, so this should result in a
  // request for an access token.
  auto fetcher = CreateFetcher(
      callback.Get(),
      PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable);

  // A canceled access token request should get retried once.
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED));

  // Once the access token request is fulfilled, we should get called back with
  // the access token.
  EXPECT_CALL(callback, Run(GoogleServiceAuthError::AuthErrorNone(),
                            access_token_info()));
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      access_token_info().token, access_token_info().expiration_time,
      access_token_info().id_token);
}

TEST_F(PrimaryAccountAccessTokenFetcherTest,
       ShouldRetryCanceledAccessTokenRequestOnlyOnce) {
  TestTokenCallback callback;

  std::string account_id = SignIn();

  // Signed in and refresh token already exists, so this should result in a
  // request for an access token.
  auto fetcher = CreateFetcher(
      callback.Get(),
      PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable);

  // A canceled access token request should get retried once.
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED));

  // On the second failure, we should get called back with an empty access
  // token.
  EXPECT_CALL(
      callback,
      Run(GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED),
          AccessTokenInfo()));
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED));
}

#if !defined(OS_CHROMEOS)

TEST_F(PrimaryAccountAccessTokenFetcherTest,
       ShouldNotRetryCanceledAccessTokenRequestIfSignedOut) {
  TestTokenCallback callback;

  std::string account_id = SignIn();

  // Signed in and refresh token already exists, so this should result in a
  // request for an access token.
  auto fetcher = CreateFetcher(
      callback.Get(),
      PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable);

  // Simulate the user signing out while the access token request is pending.
  // In this case, the pending request gets canceled, and the fetcher should
  // *not* retry.
  EXPECT_CALL(
      callback,
      Run(GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED),
          AccessTokenInfo()));

  identity_test_env()->ClearPrimaryAccount();
}

#endif  // !OS_CHROMEOS

TEST_F(PrimaryAccountAccessTokenFetcherTest,
       ShouldNotRetryCanceledAccessTokenRequestIfRefreshTokenRevoked) {
  TestTokenCallback callback;

  std::string account_id = SignIn();

  // Signed in and refresh token already exists, so this should result in a
  // request for an access token.
  auto fetcher = CreateFetcher(
      callback.Get(),
      PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable);

  // Simulate the refresh token getting removed. In this case, pending
  // access token requests get canceled, and the fetcher should *not* retry.
  EXPECT_CALL(
      callback,
      Run(GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED),
          AccessTokenInfo()));
  identity_test_env()->RemoveRefreshTokenForPrimaryAccount();
}

TEST_F(PrimaryAccountAccessTokenFetcherTest,
       ShouldNotRetryFailedAccessTokenRequest) {
  TestTokenCallback callback;

  std::string account_id = SignIn();

  // Signed in and refresh token already exists, so this should result in a
  // request for an access token.
  auto fetcher = CreateFetcher(
      callback.Get(),
      PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable);

  // An access token failure other than "canceled" should not be retried; we
  // should immediately get called back with an empty access token.
  EXPECT_CALL(
      callback,
      Run(GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE),
          AccessTokenInfo()));
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE));
}

}  // namespace identity
