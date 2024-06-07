// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/fake_oauth2_access_token_manager.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

using TokenResponseBuilder = OAuth2AccessTokenConsumer::TokenResponse::Builder;

FakeOAuth2AccessTokenManager::PendingRequest::PendingRequest() = default;

FakeOAuth2AccessTokenManager::PendingRequest::PendingRequest(
    const PendingRequest& other) = default;

FakeOAuth2AccessTokenManager::PendingRequest::~PendingRequest() = default;

FakeOAuth2AccessTokenManager::FakeOAuth2AccessTokenManager(
    OAuth2AccessTokenManager::Delegate* delegate)
    : OAuth2AccessTokenManager(delegate),
      auto_post_fetch_response_on_message_loop_(false) {}

FakeOAuth2AccessTokenManager::~FakeOAuth2AccessTokenManager() {
  CompleteRequests(
      CoreAccountId(), true, FakeOAuth2AccessTokenManager::ScopeSet(),
      GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED),
      OAuth2AccessTokenConsumer::TokenResponse());
}

void FakeOAuth2AccessTokenManager::IssueAllTokensForAccount(
    const CoreAccountId& account_id,
    const std::string& access_token,
    const base::Time& expiration) {
  DCHECK(!auto_post_fetch_response_on_message_loop_);
  CompleteRequests(account_id, true, FakeOAuth2AccessTokenManager::ScopeSet(),
                   GoogleServiceAuthError::AuthErrorNone(),
                   TokenResponseBuilder()
                       .WithAccessToken(access_token)
                       .WithExpirationTime(expiration)
                       .build());
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
                   TokenResponseBuilder()
                       .WithAccessToken(access_token)
                       .WithExpirationTime(expiration)
                       .build());
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
                   TokenResponseBuilder()
                       .WithAccessToken(access_token)
                       .WithExpirationTime(expiration)
                       .build());
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
  for (const PendingRequest& request : requests) {
    // Consumers can drop requests in response to callbacks on other requests
    // (e.g., OAuthMultiloginFetcher clears all of its requests when it gets an
    // error on any of them).
    if (!request.request) {
      continue;
    }

    bool scope_matches = all_scopes || request.scopes == scope;
    bool account_matches =
        account_id.empty() || account_id == request.account_id;
    if (account_matches && scope_matches) {
      for (auto& diagnostic_observer : GetDiagnosticsObserversForTesting()) {
        diagnostic_observer.OnFetchAccessTokenComplete(
            account_id, request.request->GetConsumerId(), scope, error,
            base::Time());
      }

      request.request->InformConsumer(error, token_response);
    }
  }
}

std::vector<FakeOAuth2AccessTokenManager::PendingRequest>
FakeOAuth2AccessTokenManager::GetPendingRequests() {
  std::vector<PendingRequest> valid_requests;
  for (const PendingRequest& pending_request : pending_requests_) {
    if (pending_request.request) {
      valid_requests.push_back(pending_request);
    }
  }
  return valid_requests;
}

void FakeOAuth2AccessTokenManager::CancelRequestsForAccount(
    const CoreAccountId& account_id,
    const GoogleServiceAuthError& error) {
  CompleteRequests(account_id, true, FakeOAuth2AccessTokenManager::ScopeSet(),
                   error, OAuth2AccessTokenConsumer::TokenResponse());
}

void FakeOAuth2AccessTokenManager::FetchOAuth2Token(
    FakeOAuth2AccessTokenManager::RequestImpl* request,
    const CoreAccountId& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& client_id,
    const std::string& client_secret,
    const std::string& consumer_name,
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
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&FakeOAuth2AccessTokenManager::CompleteRequests,
                       weak_ptr_factory_.GetWeakPtr(), account_id,
                       /*all_scoped=*/true, scopes,
                       GoogleServiceAuthError::AuthErrorNone(),
                       TokenResponseBuilder()
                           .WithAccessToken("access_token")
                           .WithExpirationTime(base::Time::Max())
                           .build()));
  }
}

void FakeOAuth2AccessTokenManager::InvalidateAccessTokenImpl(
    const CoreAccountId& account_id,
    const std::string& client_id,
    const FakeOAuth2AccessTokenManager::ScopeSet& scopes,
    const std::string& access_token) {
  for (DiagnosticsObserver& observer : GetDiagnosticsObserversForTesting()) {
    observer.OnAccessTokenRemoved(account_id, scopes);
  }
  // Do nothing else, as we don't have a cache from which to remove the token.
}
