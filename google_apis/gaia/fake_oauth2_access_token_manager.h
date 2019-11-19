// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_FAKE_OAUTH2_ACCESS_TOKEN_MANAGER_H_
#define GOOGLE_APIS_GAIA_FAKE_OAUTH2_ACCESS_TOKEN_MANAGER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"

namespace network {
class SharedURLLoaderFactory;
}

// Helper class to simplify writing unittests that depend on an instance of
// OAuth2AccessTokenManager.
class FakeOAuth2AccessTokenManager : public OAuth2AccessTokenManager {
 public:
  struct PendingRequest {
    PendingRequest();
    PendingRequest(const PendingRequest& other);
    ~PendingRequest();

    CoreAccountId account_id;
    std::string client_id;
    std::string client_secret;
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory;
    OAuth2AccessTokenManager::ScopeSet scopes;
    base::WeakPtr<OAuth2AccessTokenManager::RequestImpl> request;
  };

  explicit FakeOAuth2AccessTokenManager(
      OAuth2AccessTokenManager::Delegate* delegate);
  ~FakeOAuth2AccessTokenManager() override;

  // Gets a list of active requests (can be used by tests to validate that the
  // correct request has been issued).
  std::vector<PendingRequest> GetPendingRequests();

  // Helper routines to issue tokens for pending requests.
  void IssueAllTokensForAccount(const CoreAccountId& account_id,
                                const std::string& access_token,
                                const base::Time& expiration);

  // Helper routines to issue token for pending requests based on TokenResponse.
  void IssueAllTokensForAccount(
      const CoreAccountId& account_id,
      const OAuth2AccessTokenConsumer::TokenResponse& token_response);

  void IssueErrorForAllPendingRequestsForAccount(
      const CoreAccountId& account_id,
      const GoogleServiceAuthError& error);

  void IssueTokenForScope(const OAuth2AccessTokenManager::ScopeSet& scopes,
                          const std::string& access_token,
                          const base::Time& expiration);

  void IssueTokenForScope(
      const OAuth2AccessTokenManager::ScopeSet& scopes,
      const OAuth2AccessTokenConsumer::TokenResponse& token_response);

  void IssueErrorForScope(const OAuth2AccessTokenManager::ScopeSet& scopes,
                          const GoogleServiceAuthError& error);

  void IssueTokenForAllPendingRequests(const std::string& access_token,
                                       const base::Time& expiration);

  void IssueTokenForAllPendingRequests(
      const OAuth2AccessTokenConsumer::TokenResponse& token_response);

  void IssueErrorForAllPendingRequests(const GoogleServiceAuthError& error);

  void set_auto_post_fetch_response_on_message_loop(bool auto_post_response) {
    auto_post_fetch_response_on_message_loop_ = auto_post_response;
  }

  // OAuth2AccessTokenManager overrides.
  void CancelAllRequests() override;

  void CancelRequestsForAccount(const CoreAccountId& account_id) override;

  void FetchOAuth2Token(
      OAuth2AccessTokenManager::RequestImpl* request,
      const CoreAccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& client_id,
      const std::string& client_secret,
      const OAuth2AccessTokenManager::ScopeSet& scopes) override;

  void InvalidateAccessTokenImpl(
      const CoreAccountId& account_id,
      const std::string& client_id,
      const OAuth2AccessTokenManager::ScopeSet& scopes,
      const std::string& access_token) override;

 private:
  // Helper function to complete pending requests - if |all_scopes| is true,
  // then all pending requests are completed, otherwise, only those requests
  // matching |scopes| are completed.  If |account_id| is empty, then pending
  // requests for all accounts are completed, otherwise only requests for the
  // given account.
  void CompleteRequests(
      const CoreAccountId& account_id,
      bool all_scopes,
      const OAuth2AccessTokenManager::ScopeSet& scopes,
      const GoogleServiceAuthError& error,
      const OAuth2AccessTokenConsumer::TokenResponse& token_response);

  std::vector<PendingRequest> pending_requests_;

  // If true, then this fake manager will post responses to
  // |FetchOAuth2Token| on the current run loop. There is no need to call
  // |IssueTokenForScope| in this case.
  bool auto_post_fetch_response_on_message_loop_;

  base::WeakPtrFactory<FakeOAuth2AccessTokenManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FakeOAuth2AccessTokenManager);
};

#endif  // GOOGLE_APIS_GAIA_FAKE_OAUTH2_ACCESS_TOKEN_MANAGER_H_
