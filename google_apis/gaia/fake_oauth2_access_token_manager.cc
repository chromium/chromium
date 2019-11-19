// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/fake_oauth2_access_token_manager.h"

#include <memory>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

FakeOAuth2AccessTokenManager::PendingRequest::PendingRequest() {}

FakeOAuth2AccessTokenManager::PendingRequest::PendingRequest(
    const PendingRequest& other) = default;

FakeOAuth2AccessTokenManager::PendingRequest::~PendingRequest() {}

FakeOAuth2AccessTokenManager::FakeOAuth2AccessTokenManager(
    OAuth2AccessTokenManager::Delegate* delegate)
    : OAuth2AccessTokenManager(delegate),
      auto_post_fetch_response_on_message_loop_(false) {}

FakeOAuth2AccessTokenManager::~FakeOAuth2AccessTokenManager() {}

void FakeOAuth2AccessTokenManager::IssueAllTokensForAccount(
    const CoreAccountId& account_id,
    const std::string& access_token,
    const base::Time& expiration) {
  DCHECK(!auto_post_fetch_response_on_message_loop_);
  CompleteRequests(account_id, true, FakeOAuth2AccessTokenManager::ScopeSet(),
                   GoogleServiceAuthError::AuthErrorNone(),
                   OAuth2AccessTokenConsumer::TokenResponse(
                       access_token, expiration, std::string() /* id_token */));
}

void FakeOAuth2AccessTokenManager::IssueAllTokensForAccount(
    const CoreAccountId& account_id,
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  DCHECK(!auto_post_fetch_response_on_message_loop_);
  CompleteRequests(account_id, true, FakeOAuth2AccessTokenManager::ScopeSet(),
                   GoogleServiceAuthError::AuthErrorNone(), token_response);
}

void FakeOAuth2AccessTokenManager::IssueErrorForAllPendingRequestsForAccount(
    const CoreAccountId& account_id,
    const GoogleServiceAuthError& error) {
  DCHECK(!auto_post_fetch_response_on_message_loop_);
  CompleteRequests(account_id, true, FakeOAuth2AccessTokenManager::ScopeSet(),
                   error, OAuth2AccessTokenConsumer::TokenResponse());
}

void FakeOAuth2AccessTokenManager::IssueTokenForScope(
    const FakeOAuth2AccessTokenManager::ScopeSet& scope,
    const std::string& access_token,
    const base::Time& expiration) {
  DCHECK(!auto_post_fetch_response_on_message_loop_);
  CompleteRequests(CoreAccountId(), false, scope,
                   GoogleServiceAuthError::AuthErrorNone(),
                   OAuth2AccessTokenConsumer::TokenResponse(
                       access_token, expiration, std::string() /* id_token */));
}

void FakeOAuth2AccessTokenManager::IssueTokenForScope(
    const FakeOAuth2AccessTokenManager::ScopeSet& scope,
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  DCHECK(!auto_post_fetch_response_on_message_loop_);
  CompleteRequests(CoreAccountId(), false, scope,
                   GoogleServiceAuthError::AuthErrorNone(), token_response);
}

void FakeOAuth2AccessTokenManager::IssueErrorForScope(
    const FakeOAuth2AccessTokenManager::ScopeSet& scope,
    const GoogleServiceAuthError& error) {
  DCHECK(!auto_post_fetch_response_on_message_loop_);
  CompleteRequests(CoreAccountId(), false, scope, error,
                   OAuth2AccessTokenConsumer::TokenResponse());
}

void FakeOAuth2AccessTokenManager::IssueErrorForAllPendingRequests(
    const GoogleServiceAuthError& error) {
  DCHECK(!auto_post_fetch_response_on_message_loop_);
  CompleteRequests(CoreAccountId(), true,
                   FakeOAuth2AccessTokenManager::ScopeSet(), error,
                   OAuth2AccessTokenConsumer::TokenResponse());
}

void FakeOAuth2AccessTokenManager::IssueTokenForAllPendingRequests(
    const std::string& access_token,
    const base::Time& expiration) {
  DCHECK(!auto_post_fetch_response_on_message_loop_);
  CompleteRequests(CoreAccountId(), true,
                   FakeOAuth2AccessTokenManager::ScopeSet(),
                   GoogleServiceAuthError::AuthErrorNone(),
                   OAuth2AccessTokenConsumer::TokenResponse(
                       access_token, expiration, std::string() /* id_token */));
}

void FakeOAuth2AccessTokenManager::IssueTokenForAllPendingRequests(
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  DCHECK(!auto_post_fetch_response_on_message_loop_);
  CompleteRequests(CoreAccountId(), true,
                   FakeOAuth2AccessTokenManager::ScopeSet(),
                   GoogleServiceAuthError::AuthErrorNone(), token_response);
}

void FakeOAuth2AccessTokenManager::CompleteRequests(
    const CoreAccountId& account_id,
    bool all_scopes,
    const FakeOAuth2AccessTokenManager::ScopeSet& scope,
    const GoogleServiceAuthError& error,
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  std::vector<FakeOAuth2AccessTokenManager::PendingRequest> requests =
      GetPendingRequests();

  // Walk the requests and notify the callbacks.
  for (auto it = requests.begin(); it != requests.end(); ++it) {
    // Consumers can drop requests in response to callbacks on other requests
    // (e.g., OAuthMultiloginFetcher clears all of its requests when it gets an
    // error on any of them).
    if (!it->request)
      continue;

    bool scope_matches = all_scopes || it->scopes == scope;
    bool account_matches = account_id.empty() || account_id == it->account_id;
    if (account_matches && scope_matches) {
      for (auto& diagnostic_observer : GetDiagnosticsObserversForTesting()) {
        diagnostic_observer.OnFetchAccessTokenComplete(
            account_id, it->request->GetConsumerId(), scope, error,
            base::Time());
      }

      it->request->InformConsumer(
          error, OAuth2AccessTokenConsumer::TokenResponse(
                     token_response.access_token,
                     token_response.expiration_time, token_response.id_token));
    }
  }
}

std::vector<FakeOAuth2AccessTokenManager::PendingRequest>
FakeOAuth2AccessTokenManager::GetPendingRequests() {
  std::vector<PendingRequest> valid_requests;
  for (auto it = pending_requests_.begin(); it != pending_requests_.end();
       ++it) {
    if (it->request)
      valid_requests.push_back(*it);
  }
  return valid_requests;
}

void FakeOAuth2AccessTokenManager::CancelAllRequests() {
  CompleteRequests(
      CoreAccountId(), true, FakeOAuth2AccessTokenManager::ScopeSet(),
      GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED),
      OAuth2AccessTokenConsumer::TokenResponse());
}

void FakeOAuth2AccessTokenManager::CancelRequestsForAccount(
    const CoreAccountId& account_id) {
  CompleteRequests(
      account_id, true, FakeOAuth2AccessTokenManager::ScopeSet(),
      GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED),
      OAuth2AccessTokenConsumer::TokenResponse());
}

void FakeOAuth2AccessTokenManager::FetchOAuth2Token(
    FakeOAuth2AccessTokenManager::RequestImpl* request,
    const CoreAccountId& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& client_id,
    const std::string& client_secret,
    const FakeOAuth2AccessTokenManager::ScopeSet& scopes) {
  PendingRequest pending_request;
  pending_request.account_id = account_id;
  pending_request.client_id = client_id;
  pending_request.client_secret = client_secret;
  pending_request.url_loader_factory = url_loader_factory;
  pending_request.scopes = scopes;
  pending_request.request = request->AsWeakPtr();
  pending_requests_.push_back(pending_request);

  if (auto_post_fetch_response_on_message_loop_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&FakeOAuth2AccessTokenManager::CompleteRequests,
                       weak_ptr_factory_.GetWeakPtr(), account_id,
                       /*all_scoped=*/true, scopes,
                       GoogleServiceAuthError::AuthErrorNone(),
                       OAuth2AccessTokenConsumer::TokenResponse(
                           "access_token", base::Time::Max(), std::string())));
  }
}

void FakeOAuth2AccessTokenManager::InvalidateAccessTokenImpl(
    const CoreAccountId& account_id,
    const std::string& client_id,
    const FakeOAuth2AccessTokenManager::ScopeSet& scopes,
    const std::string& access_token) {
  // Do nothing, as we don't have a cache from which to remove the token.
}
