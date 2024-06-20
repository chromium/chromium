// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/oauth2_access_token_manager.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/memory/ref_counted.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "google_apis/gaia/gaia_access_token_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_impl.h"
#include "google_apis/gaia/oauth2_access_token_manager_test_util.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kTestAccountId[] = "test_user_account_id";

class FakeOAuth2AccessTokenManagerDelegate
    : public OAuth2AccessTokenManager::Delegate {
 public:
  explicit FakeOAuth2AccessTokenManagerDelegate(
      network::TestURLLoaderFactory* test_url_loader_factory)
      : shared_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                test_url_loader_factory)) {}
  ~FakeOAuth2AccessTokenManagerDelegate() override = default;

  // OAuth2AccessTokenManager::Delegate:
  std::unique_ptr<OAuth2AccessTokenFetcher> CreateAccessTokenFetcher(
      const CoreAccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      OAuth2AccessTokenConsumer* consumer,
      const std::string& token_binding_challenge) override {
    EXPECT_TRUE(base::Contains(account_ids_to_refresh_tokens_, account_id));
    return GaiaAccessTokenFetcher::
        CreateExchangeRefreshTokenForAccessTokenInstance(
            consumer, url_loader_factory,
            account_ids_to_refresh_tokens_[account_id]);
  }

  bool HasRefreshToken(const CoreAccountId& account_id) const override {
    return base::Contains(account_ids_to_refresh_tokens_, account_id);
  }

  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      const override {
    return shared_factory_;
  }

  bool HandleAccessTokenFetch(
      OAuth2AccessTokenManager::RequestImpl* request,
      const CoreAccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& client_id,
      const std::string& client_secret,
      const OAuth2AccessTokenManager::ScopeSet& scopes) override {
    if (access_token_fetch_closure_) {
      std::move(access_token_fetch_closure_).Run();
      return true;
    }
    return false;
  }

  void OnAccessTokenInvalidated(
      const CoreAccountId& account_id,
      const std::string& client_id,
      const OAuth2AccessTokenManager::ScopeSet& scopes,
      const std::string& access_token) override {
    if (!on_access_token_invalidated_callback_)
      return;

    EXPECT_EQ(access_token_invalidated_account_id_, account_id);
    EXPECT_EQ(access_token_invalidated_client_id_, client_id);
    EXPECT_EQ(access_token_invalidated_scopes_, scopes);
    EXPECT_EQ(access_token_invalidated_access_token_, access_token);
    std::move(on_access_token_invalidated_callback_).Run();
  }

  void OnAccessTokenFetched(const CoreAccountId& account_id,
                            const GoogleServiceAuthError& error) override {
    if (!access_token_fetched_callback_)
      return;

    EXPECT_EQ(access_token_fetched_account_id_, account_id);
    EXPECT_EQ(access_token_fetched_error_, error);
    std::move(access_token_fetched_callback_).Run();
  }

  void AddAccount(CoreAccountId id, std::string refresh_token) {
    account_ids_to_refresh_tokens_[id] = refresh_token;
  }

  void SetAccessTokenHandleClosure(base::OnceClosure closure) {
    access_token_fetch_closure_ = std::move(closure);
  }

  void SetOnAccessTokenInvalidated(
      const CoreAccountId& account_id,
      const std::string& client_id,
      const OAuth2AccessTokenManager::ScopeSet& scopes,
      const std::string& access_token,
      base::OnceClosure callback) {
    access_token_invalidated_account_id_ = account_id;
    access_token_invalidated_client_id_ = client_id;
    access_token_invalidated_scopes_ = scopes;
    access_token_invalidated_access_token_ = access_token;
    on_access_token_invalidated_callback_ = std::move(callback);
  }

  void SetOnAccessTokenFetched(const CoreAccountId& account_id,
                               const GoogleServiceAuthError& error,
                               base::OnceClosure callback) {
    access_token_fetched_account_id_ = account_id;
    access_token_fetched_error_ = error;
    access_token_fetched_callback_ = std::move(callback);
  }

 private:
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_;
  std::map<CoreAccountId, std::string> account_ids_to_refresh_tokens_;
  base::OnceClosure access_token_fetch_closure_;
  CoreAccountId access_token_invalidated_account_id_;
  std::string access_token_invalidated_client_id_;
  OAuth2AccessTokenManager::ScopeSet access_token_invalidated_scopes_;
  std::string access_token_invalidated_access_token_;
  base::OnceClosure on_access_token_invalidated_callback_;
  CoreAccountId access_token_fetched_account_id_;
  GoogleServiceAuthError access_token_fetched_error_;
  base::OnceClosure access_token_fetched_callback_;
};

class FakeOAuth2AccessTokenManagerConsumer
    : public TestingOAuth2AccessTokenManagerConsumer {
 public:
  FakeOAuth2AccessTokenManagerConsumer() = default;
  ~FakeOAuth2AccessTokenManagerConsumer() override = default;

  // TestingOAuth2AccessTokenManagerConsumer overrides.
  void OnGetTokenSuccess(
      const OAuth2AccessTokenManager::Request* request,
      const OAuth2AccessTokenConsumer::TokenResponse& token_response) override {
    TestingOAuth2AccessTokenManagerConsumer::OnGetTokenSuccess(request,
                                                               token_response);
    if (closure_)
      std::move(closure_).Run();
  }

  void OnGetTokenFailure(const OAuth2AccessTokenManager::Request* request,
                         const GoogleServiceAuthError& error) override {
    TestingOAuth2AccessTokenManagerConsumer::OnGetTokenFailure(request, error);
    if (closure_)
      std::move(closure_).Run();
  }

  void SetResponseCompletedClosure(base::OnceClosure closure) {
    closure_ = std::move(closure);
  }

 private:
  base::OnceClosure closure_;
};

class DiagnosticsObserverForTesting
    : public OAuth2AccessTokenManager::DiagnosticsObserver {
 public:
  // OAuth2AccessTokenManager::DiagnosticsObserver:
  void OnAccessTokenRequested(
      const CoreAccountId& account_id,
      const std::string& consumer_id,
      const OAuth2AccessTokenManager::ScopeSet& scopes) override {
    if (!access_token_requested_callback_)
      return;
    EXPECT_EQ(access_token_requested_account_id_, account_id);
    EXPECT_EQ(access_token_requested_consumer_id_, consumer_id);
    EXPECT_EQ(access_token_requested_scopes_, scopes);
    std::move(access_token_requested_callback_).Run();
  }
  void OnFetchAccessTokenComplete(
      const CoreAccountId& account_id,
      const std::string& consumer_id,
      const OAuth2AccessTokenManager::ScopeSet& scopes,
      const GoogleServiceAuthError& error,
      base::Time expiration_time) override {
    if (!fetch_access_token_completed_callback_)
      return;
    EXPECT_EQ(fetch_access_token_completed_account_id_, account_id);
    EXPECT_EQ(fetch_access_token_completed_consumer_id_, consumer_id);
    EXPECT_EQ(fetch_access_token_completed_scopes_, scopes);
    EXPECT_EQ(fetch_access_token_completed_error_, error);
    std::move(fetch_access_token_completed_callback_).Run();
  }
  void OnAccessTokenRemoved(
      const CoreAccountId& account_id,
      const OAuth2AccessTokenManager::ScopeSet& scopes) override {
    if (!access_token_removed_callback_)
      return;
    auto iterator = access_token_removed_account_to_scopes_.find(account_id);
    EXPECT_NE(iterator, access_token_removed_account_to_scopes_.end());
    EXPECT_EQ(iterator->second, scopes);
    access_token_removed_account_to_scopes_.erase(iterator);

    if (access_token_removed_account_to_scopes_.empty())
      std::move(access_token_removed_callback_).Run();
  }

  void SetOnAccessTokenRequested(
      const CoreAccountId& account_id,
      const std::string& consumer_id,
      const OAuth2AccessTokenManager::ScopeSet& scopes,
      base::OnceClosure callback) {
    access_token_requested_account_id_ = account_id;
    access_token_requested_consumer_id_ = consumer_id;
    access_token_requested_scopes_ = scopes;
    access_token_requested_callback_ = std::move(callback);
  }
  void SetOnFetchAccessTokenComplete(
      const CoreAccountId& account_id,
      const std::string& consumer_id,
      const OAuth2AccessTokenManager::ScopeSet& scopes,
      GoogleServiceAuthError error,
      base::OnceClosure callback) {
    fetch_access_token_completed_account_id_ = account_id;
    fetch_access_token_completed_consumer_id_ = consumer_id;
    fetch_access_token_completed_scopes_ = scopes;
    fetch_access_token_completed_error_ = error;
    fetch_access_token_completed_callback_ = std::move(callback);
  }

  typedef std::map<CoreAccountId, OAuth2AccessTokenManager::ScopeSet>
      AccountToScopeSet;
  // OnAccessTokenRemoved() can be invoked multiple times as part of a given
  // test expectation (e.g., when clearing the cache of multiple tokens). To
  // support this, this method takes in a map of account IDs to scopesets, and
  // OnAccessTokenRemoved() invokes |callback| only once invocations of it have
  // occurred for all of the (account_id, scopeset) pairs in
  // |account_to_scopeset|.
  void SetOnAccessTokenRemoved(const AccountToScopeSet& account_to_scopeset,
                               base::OnceClosure callback) {
    access_token_removed_account_to_scopes_ = account_to_scopeset;
    access_token_removed_callback_ = std::move(callback);
  }

 private:
  CoreAccountId access_token_requested_account_id_;
  std::string access_token_requested_consumer_id_;
  OAuth2AccessTokenManager::ScopeSet access_token_requested_scopes_;
  base::OnceClosure access_token_requested_callback_;
  CoreAccountId fetch_access_token_completed_account_id_;
  std::string fetch_access_token_completed_consumer_id_;
  OAuth2AccessTokenManager::ScopeSet fetch_access_token_completed_scopes_;
  GoogleServiceAuthError fetch_access_token_completed_error_;
  base::OnceClosure fetch_access_token_completed_callback_;
  AccountToScopeSet access_token_removed_account_to_scopes_;
  base::OnceClosure access_token_removed_callback_;
};

}  // namespace

// Any public API surfaces that are wrapped by ProfileOAuth2TokenService are
// unittested as part of the unittests of that class.

class OAuth2AccessTokenManagerTest : public testing::Test {
 public:
  OAuth2AccessTokenManagerTest()
      : delegate_(&test_url_loader_factory_),
        token_manager_(std::make_unique<OAuth2AccessTokenManager>(&delegate_)) {
  }

  void SetUp() override {
    account_id_ = CoreAccountId::FromGaiaId(kTestAccountId);
    delegate_.AddAccount(account_id_, "fake_refresh_token");
  }

  void TearDown() override {
    // Makes sure that all the clean up tasks are run. It's required because of
    // cleaning up OAuth2AccessTokenManager::Fetcher on
    // InformWaitingRequestsAndDelete().
    base::RunLoop().RunUntilIdle();
  }

  void SimulateOAuthTokenResponse(const std::string& token,
                                  net::HttpStatusCode status = net::HTTP_OK) {
    test_url_loader_factory_.AddResponse(
        GaiaUrls::GetInstance()->oauth2_token_url().spec(), token, status);
  }

  void CreateRequestAndBlockUntilComplete(
      const CoreAccountId& account,
      const OAuth2AccessTokenManager::ScopeSet& scopeset) {
    base::RunLoop run_loop;
    consumer_.SetResponseCompletedClosure(run_loop.QuitClosure());
    std::unique_ptr<OAuth2AccessTokenManager::Request> request(
        token_manager_->StartRequest(account, scopeset, &consumer_));
    run_loop.Run();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  CoreAccountId account_id_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  FakeOAuth2AccessTokenManagerDelegate delegate_;
  std::unique_ptr<OAuth2AccessTokenManager> token_manager_;
  FakeOAuth2AccessTokenManagerConsumer consumer_;
};

// Test that StartRequest gets a response properly.
TEST_F(OAuth2AccessTokenManagerTest, StartRequest) {
  base::RunLoop run_loop;
  consumer_.SetResponseCompletedClosure(run_loop.QuitClosure());
  std::unique_ptr<OAuth2AccessTokenManager::Request> request(
      token_manager_->StartRequest(
          account_id_, OAuth2AccessTokenManager::ScopeSet(), &consumer_));
  SimulateOAuthTokenResponse(GetValidTokenResponse("token", 3600));
  run_loop.Run();

  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
}

// Test that destroying `OAuth2AccessTokenManager` triggers
// `OnGetTokenFailure()`.
TEST_F(OAuth2AccessTokenManagerTest, CancelAllRequests) {
  std::unique_ptr<OAuth2AccessTokenManager::Request> request(
      token_manager_->StartRequest(
          account_id_, OAuth2AccessTokenManager::ScopeSet(), &consumer_));
  const CoreAccountId account_id_2 = CoreAccountId::FromGaiaId("account_id_2");
  delegate_.AddAccount(account_id_2, "refreshToken2");
  std::unique_ptr<OAuth2AccessTokenManager::Request> request2(
      token_manager_->StartRequest(
          account_id_2, OAuth2AccessTokenManager::ScopeSet(), &consumer_));

  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);

  token_manager_.reset();

  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(2, consumer_.number_of_errors_);
}

// Test that `CancelRequestsForAccount()` cancels requests for the specific
// account.
TEST_F(OAuth2AccessTokenManagerTest, CancelRequestsForAccount) {
  OAuth2AccessTokenManager::ScopeSet scope_set_1;
  scope_set_1.insert("scope1");
  scope_set_1.insert("scope2");
  OAuth2AccessTokenManager::ScopeSet scope_set_2(scope_set_1.begin(),
                                                 scope_set_1.end());
  scope_set_2.insert("scope3");

  std::unique_ptr<OAuth2AccessTokenManager::Request> request1(
      token_manager_->StartRequest(account_id_, scope_set_1, &consumer_));
  std::unique_ptr<OAuth2AccessTokenManager::Request> request2(
      token_manager_->StartRequest(account_id_, scope_set_2, &consumer_));

  const CoreAccountId account_id_2 = CoreAccountId::FromGaiaId("account_id_2");
  delegate_.AddAccount(account_id_2, "refreshToken2");
  std::unique_ptr<OAuth2AccessTokenManager::Request> request3(
      token_manager_->StartRequest(account_id_2, scope_set_1, &consumer_));

  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);

  token_manager_->CancelRequestsForAccount(
      account_id_,
      GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED));

  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(2, consumer_.number_of_errors_);

  token_manager_->CancelRequestsForAccount(
      account_id_2,
      GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED));

  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(3, consumer_.number_of_errors_);
}

// Test that `StartRequest()` fetches a network request after
// `ClearCacheForAccount()`.
TEST_F(OAuth2AccessTokenManagerTest, ClearCache) {
  base::RunLoop run_loop1;
  consumer_.SetResponseCompletedClosure(run_loop1.QuitClosure());

  std::set<std::string> scope_list;
  scope_list.insert("scope");
  std::unique_ptr<OAuth2AccessTokenManager::Request> request(
      token_manager_->StartRequest(account_id_, scope_list, &consumer_));
  SimulateOAuthTokenResponse(GetValidTokenResponse("token", 3600));
  run_loop1.Run();

  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token", consumer_.last_token_);
  EXPECT_EQ(1U, token_manager_->token_cache().size());

  token_manager_->ClearCacheForAccount(account_id_);

  EXPECT_EQ(0U, token_manager_->token_cache().size());
  base::RunLoop run_loop2;
  consumer_.SetResponseCompletedClosure(run_loop2.QuitClosure());

  SimulateOAuthTokenResponse(GetValidTokenResponse("another token", 3600));
  request = token_manager_->StartRequest(account_id_, scope_list, &consumer_);
  run_loop2.Run();
  EXPECT_EQ(2, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("another token", consumer_.last_token_);
  EXPECT_EQ(1U, token_manager_->token_cache().size());
}

// Test that `ClearCacheForAccount()` clears caches for the specific account.
TEST_F(OAuth2AccessTokenManagerTest, ClearCacheForAccount) {
  base::RunLoop run_loop1;
  consumer_.SetResponseCompletedClosure(run_loop1.QuitClosure());

  std::unique_ptr<OAuth2AccessTokenManager::Request> request1(
      token_manager_->StartRequest(
          account_id_, OAuth2AccessTokenManager::ScopeSet(), &consumer_));
  SimulateOAuthTokenResponse(GetValidTokenResponse("token", 3600));
  run_loop1.Run();

  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token", consumer_.last_token_);
  EXPECT_EQ(1U, token_manager_->token_cache().size());

  base::RunLoop run_loop2;
  consumer_.SetResponseCompletedClosure(run_loop2.QuitClosure());
  const CoreAccountId account_id_2 = CoreAccountId::FromGaiaId("account_id_2");
  delegate_.AddAccount(account_id_2, "refreshToken2");
  // Makes a request for |account_id_2|.
  std::unique_ptr<OAuth2AccessTokenManager::Request> request2(
      token_manager_->StartRequest(
          account_id_2, OAuth2AccessTokenManager::ScopeSet(), &consumer_));
  run_loop2.Run();

  EXPECT_EQ(2, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token", consumer_.last_token_);
  EXPECT_EQ(2U, token_manager_->token_cache().size());

  // Clears caches for |account_id_|.
  token_manager_->ClearCacheForAccount(account_id_);
  EXPECT_EQ(1U, token_manager_->token_cache().size());

  base::RunLoop run_loop3;
  consumer_.SetResponseCompletedClosure(run_loop3.QuitClosure());
  SimulateOAuthTokenResponse(GetValidTokenResponse("another token", 3600));
  // Makes a request for |account_id_| again.
  std::unique_ptr<OAuth2AccessTokenManager::Request> request3(
      token_manager_->StartRequest(
          account_id_, OAuth2AccessTokenManager::ScopeSet(), &consumer_));
  run_loop3.Run();

  EXPECT_EQ(3, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("another token", consumer_.last_token_);
  EXPECT_EQ(2U, token_manager_->token_cache().size());

  // Clears caches for |account_id_|.
  token_manager_->ClearCacheForAccount(account_id_);
  EXPECT_EQ(1U, token_manager_->token_cache().size());

  // Clears caches for |account_id_2|.
  token_manager_->ClearCacheForAccount(account_id_2);
  EXPECT_EQ(0U, token_manager_->token_cache().size());
}

// Test that StartRequest checks HandleAccessTokenFetch() from |delegate_|
// before FetchOAuth2Token.
TEST_F(OAuth2AccessTokenManagerTest, HandleAccessTokenFetch) {
  base::RunLoop run_loop;
  delegate_.SetAccessTokenHandleClosure(run_loop.QuitClosure());
  std::unique_ptr<OAuth2AccessTokenManager::Request> request(
      token_manager_->StartRequest(
          account_id_, OAuth2AccessTokenManager::ScopeSet(), &consumer_));
  SimulateOAuthTokenResponse(GetValidTokenResponse("token", 3600));
  run_loop.Run();

  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ(0U, token_manager_->GetNumPendingRequestsForTesting(
                    GaiaUrls::GetInstance()->oauth2_chrome_client_id(),
                    account_id_, OAuth2AccessTokenManager::ScopeSet()));
}

// Test that InvalidateAccessToken triggers OnAccessTokenInvalidated.
TEST_F(OAuth2AccessTokenManagerTest, OnAccessTokenInvalidated) {
  base::RunLoop run_loop;
  OAuth2AccessTokenManager::ScopeSet scope_set;
  scope_set.insert("scope");
  std::string access_token("access_token");
  delegate_.SetOnAccessTokenInvalidated(
      account_id_, GaiaUrls::GetInstance()->oauth2_chrome_client_id(),
      scope_set, access_token, run_loop.QuitClosure());
  token_manager_->InvalidateAccessToken(account_id_, scope_set, access_token);
  run_loop.Run();
}

// Test that `OnAccessTokenFetched()` is invoked when a request is canceled.
TEST_F(OAuth2AccessTokenManagerTest, OnAccessTokenFetchedOnRequestCanceled) {
  GoogleServiceAuthError::State error_states[] = {
      GoogleServiceAuthError::REQUEST_CANCELED,
      GoogleServiceAuthError::USER_NOT_SIGNED_UP};
  for (const auto& state : error_states) {
    GoogleServiceAuthError error(state);
    SCOPED_TRACE(error.ToString());
    base::RunLoop run_loop;
    delegate_.SetOnAccessTokenFetched(account_id_, error,
                                      run_loop.QuitClosure());
    std::unique_ptr<OAuth2AccessTokenManager::Request> request(
        token_manager_->StartRequest(
            account_id_, OAuth2AccessTokenManager::ScopeSet(), &consumer_));
    token_manager_->CancelRequestsForAccount(account_id_, error);
    run_loop.Run();
  }
}

// Test that OnAccessTokenFetched is invoked when a request is completed.
TEST_F(OAuth2AccessTokenManagerTest, OnAccessTokenFetchedOnRequestCompleted) {
  base::RunLoop run_loop;
  GoogleServiceAuthError error(GoogleServiceAuthError::NONE);
  delegate_.SetOnAccessTokenFetched(account_id_, error, run_loop.QuitClosure());
  std::unique_ptr<OAuth2AccessTokenManager::Request> request(
      token_manager_->StartRequest(
          account_id_, OAuth2AccessTokenManager::ScopeSet(), &consumer_));
  SimulateOAuthTokenResponse(GetValidTokenResponse("token", 3600));
  run_loop.Run();
}

// Test that canceling requests from OnAccessTokenFetched doesn't crash.
// Regression test for https://crbug.com/1186630.
TEST_F(OAuth2AccessTokenManagerTest, OnAccessTokenFetchedCancelsRequests) {
  base::RunLoop run_loop;
  GoogleServiceAuthError error(GoogleServiceAuthError::SERVICE_ERROR);
  delegate_.SetOnAccessTokenFetched(
      account_id_, error, base::BindLambdaForTesting([&]() {
        token_manager_->CancelRequestsForAccount(
            account_id_,
            GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED));
        run_loop.Quit();
      }));
  std::unique_ptr<OAuth2AccessTokenManager::Request> request(
      token_manager_->StartRequest(
          account_id_, OAuth2AccessTokenManager::ScopeSet(), &consumer_));
  SimulateOAuthTokenResponse("", net::HTTP_BAD_REQUEST);
  run_loop.Run();
}

// Test that StartRequest triggers DiagnosticsObserver::OnAccessTokenRequested.
TEST_F(OAuth2AccessTokenManagerTest, OnAccessTokenRequested) {
  DiagnosticsObserverForTesting observer;
  OAuth2AccessTokenManager::ScopeSet scopeset;
  scopeset.insert("scope");
  base::RunLoop run_loop;
  observer.SetOnAccessTokenRequested(account_id_, consumer_.id(), scopeset,
                                     run_loop.QuitClosure());
  token_manager_->AddDiagnosticsObserver(&observer);

  std::unique_ptr<OAuth2AccessTokenManager::Request> request(
      token_manager_->StartRequest(account_id_, scopeset, &consumer_));
  run_loop.Run();
  token_manager_->RemoveDiagnosticsObserver(&observer);
}

// Test that DiagnosticsObserver::OnFetchAccessTokenComplete is invoked when a
// request is completed.
TEST_F(OAuth2AccessTokenManagerTest,
       OnFetchAccessTokenCompleteOnRequestCompleted) {
  DiagnosticsObserverForTesting observer;
  OAuth2AccessTokenManager::ScopeSet scopeset;
  scopeset.insert("scope");
  base::RunLoop run_loop;
  GoogleServiceAuthError error(GoogleServiceAuthError::NONE);
  observer.SetOnFetchAccessTokenComplete(account_id_, consumer_.id(), scopeset,
                                         error, run_loop.QuitClosure());
  token_manager_->AddDiagnosticsObserver(&observer);
  SimulateOAuthTokenResponse(GetValidTokenResponse("token", 3600));

  std::unique_ptr<OAuth2AccessTokenManager::Request> request(
      token_manager_->StartRequest(account_id_, scopeset, &consumer_));
  run_loop.Run();
  token_manager_->RemoveDiagnosticsObserver(&observer);
}

// Test that DiagnosticsObserver::OnFetchAccessTokenComplete is invoked when
// StartRequest is called for an account without a refresh token.
TEST_F(OAuth2AccessTokenManagerTest,
       OnFetchAccessTokenCompleteOnRequestWithoutRefreshToken) {
  DiagnosticsObserverForTesting observer;
  OAuth2AccessTokenManager::ScopeSet scopeset;
  scopeset.insert("scope");
  base::RunLoop run_loop;
  // |account_id| doesn't have a refresh token, OnFetchAccessTokenComplete
  // should report GoogleServiceAuthError::USER_NOT_SIGNED_UP.
  GoogleServiceAuthError error(GoogleServiceAuthError::USER_NOT_SIGNED_UP);
  const CoreAccountId account_id = CoreAccountId::FromGaiaId("new_account_id");
  observer.SetOnFetchAccessTokenComplete(account_id, consumer_.id(), scopeset,
                                         error, run_loop.QuitClosure());
  token_manager_->AddDiagnosticsObserver(&observer);

  std::unique_ptr<OAuth2AccessTokenManager::Request> request(
      token_manager_->StartRequest(account_id, scopeset, &consumer_));
  run_loop.Run();
  token_manager_->RemoveDiagnosticsObserver(&observer);
}

// Test that DiagnosticsObserver::OnAccessTokenRemoved is called when a token is
// removed from the token cache.
TEST_F(OAuth2AccessTokenManagerTest, OnAccessTokenRemoved) {
  const std::string access_token("token");
  SimulateOAuthTokenResponse(GetValidTokenResponse(access_token, 3600));

  // First populate the cache with access tokens for four accounts.
  OAuth2AccessTokenManager::ScopeSet scopeset1;
  scopeset1.insert("scope1");
  CreateRequestAndBlockUntilComplete(account_id_, scopeset1);

  OAuth2AccessTokenManager::ScopeSet scopeset2;
  scopeset2.insert("scope2");
  CoreAccountId account_id_2 = CoreAccountId::FromGaiaId("account_id_2");
  delegate_.AddAccount(account_id_2, "refreshToken2");
  CreateRequestAndBlockUntilComplete(account_id_2, scopeset2);

  OAuth2AccessTokenManager::ScopeSet scopeset3;
  scopeset3.insert("scope3");
  CoreAccountId account_id_3 = CoreAccountId::FromGaiaId("account_id_3");
  delegate_.AddAccount(account_id_3, "refreshToken3");
  CreateRequestAndBlockUntilComplete(account_id_3, scopeset3);

  OAuth2AccessTokenManager::ScopeSet scopeset4;
  scopeset4.insert("scope4");
  CoreAccountId account_id_4 = CoreAccountId::FromGaiaId("account_id_4");
  delegate_.AddAccount(account_id_4, "refreshToken4");
  CreateRequestAndBlockUntilComplete(account_id_4, scopeset4);

  EXPECT_EQ(4, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token", consumer_.last_token_);
  EXPECT_EQ(4U, token_manager_->token_cache().size());

  DiagnosticsObserverForTesting observer;
  token_manager_->AddDiagnosticsObserver(&observer);

  DiagnosticsObserverForTesting::AccountToScopeSet account_to_scopeset;

  // `ClearCacheForAccount()` should call `OnAccessTokenRemoved()`.
  base::RunLoop run_loop1;
  account_to_scopeset[account_id_] = scopeset1;
  observer.SetOnAccessTokenRemoved(account_to_scopeset,
                                   run_loop1.QuitClosure());
  token_manager_->ClearCacheForAccount(account_id_);
  run_loop1.Run();
  EXPECT_EQ(3U, token_manager_->token_cache().size());

  // InvalidateAccessToken should call OnAccessTokenRemoved for the cached
  // token.
  base::RunLoop run_loop2;
  account_to_scopeset.clear();
  account_to_scopeset[account_id_2] = scopeset2;
  observer.SetOnAccessTokenRemoved(account_to_scopeset,
                                   run_loop2.QuitClosure());
  token_manager_->InvalidateAccessToken(account_id_2, scopeset2, access_token);
  run_loop2.Run();
  EXPECT_EQ(2U, token_manager_->token_cache().size());

  token_manager_->RemoveDiagnosticsObserver(&observer);
}
