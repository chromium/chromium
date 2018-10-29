// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_IDENTITY_PUBLIC_CPP_PRIMARY_ACCOUNT_ACCESS_TOKEN_FETCHER_H_
#define SERVICES_IDENTITY_PUBLIC_CPP_PRIMARY_ACCOUNT_ACCESS_TOKEN_FETCHER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "services/identity/public/cpp/access_token_fetcher.h"
#include "services/identity/public/cpp/identity_manager.h"

namespace identity {

// Helper class to ease the task of obtaining an OAuth2 access token for the
// authenticated account. This handles various special cases, e.g. when the
// refresh token isn't loaded yet (during startup), or when there is some
// transient error.
// May only be used on the UI thread.
class PrimaryAccountAccessTokenFetcher : public IdentityManager::Observer {
 public:
  // Specifies how this instance should behave:
  // |kImmediate|: Makes one-shot immediate request.
  // |kWaitUntilAvailable|: Waits for the primary account to be available
  // before making the request. In particular, "available" is defined as the
  // moment when (a) there is a primary account and (b) that account has a
  // refresh token. This semantics is richer than using an AccessTokenFetcher in
  // kWaitUntilRefreshTokenAvailable mode, as the latter will make a request
  // once the specified account has a refresh token, regardless of whether it's
  // the primary account at that point.
  // Note that using |kWaitUntilAvailable| can result in waiting forever
  // if the user is not signed in and doesn't sign in.
  enum class Mode { kImmediate, kWaitUntilAvailable };

  // Instantiates a fetcher and immediately starts the process of obtaining an
  // OAuth2 access token for the given |scopes|. The |callback| is called once
  // the request completes (successful or not). If the
  // PrimaryAccountAccessTokenFetcher is destroyed before the process completes,
  // the callback is not called.
  PrimaryAccountAccessTokenFetcher(const std::string& oauth_consumer_name,
                                   IdentityManager* identity_manager,
                                   const identity::ScopeSet& scopes,
                                   AccessTokenFetcher::TokenCallback callback,
                                   Mode mode);

  ~PrimaryAccountAccessTokenFetcher() override;

  // Exposed for tests.
  bool access_token_request_retried() { return access_token_retried_; }

 private:
  // Returns true iff there is a primary account with a refresh token. Should
  // only be called in mode |kWaitUntilAvailable|.
  bool AreCredentialsAvailable() const;

  void StartAccessTokenRequest();

  // IdentityManager::Observer implementation.
  void OnPrimaryAccountSet(const AccountInfo& primary_account_info) override;
  void OnRefreshTokenUpdatedForAccount(const AccountInfo& account_info,
                                       bool is_valid) override;

  // Checks whether credentials are now available and starts an access token
  // request if so. Should only be called in mode |kWaitUntilAvailable|.
  void ProcessSigninStateChange();

  // Invoked by |fetcher_| when an access token request completes.
  void OnAccessTokenFetchComplete(GoogleServiceAuthError error,
                                  AccessTokenInfo access_token_info);

  std::string oauth_consumer_name_;
  IdentityManager* identity_manager_;
  identity::ScopeSet scopes_;

  // Per the contract of this class, it is allowed for clients to delete this
  // object as part of the invocation of |callback_|. Hence, this object must
  // assume that it is dead after invoking |callback_| and must not run any more
  // code.
  AccessTokenFetcher::TokenCallback callback_;

  ScopedObserver<IdentityManager, PrimaryAccountAccessTokenFetcher>
      identity_manager_observer_;

  // Internal fetcher that does the actual access token request.
  std::unique_ptr<AccessTokenFetcher> access_token_fetcher_;

  // When a token request gets canceled, we want to retry once.
  bool access_token_retried_;

  Mode mode_;

  DISALLOW_COPY_AND_ASSIGN(PrimaryAccountAccessTokenFetcher);
};

}  // namespace identity

#endif  // SERVICES_IDENTITY_PUBLIC_CPP_PRIMARY_ACCOUNT_ACCESS_TOKEN_FETCHER_H_
