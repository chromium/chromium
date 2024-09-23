// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_OAUTH2_ACCESS_TOKEN_FETCHER_H_
#define GOOGLE_APIS_GAIA_OAUTH2_ACCESS_TOKEN_FETCHER_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"

class GoogleServiceAuthError;

// Interface of a OAuth2 access token fetcher.
//
// Usage:
// * Create an instance with a consumer.
// * Call Start()
// * The consumer passed in the constructor will be called on the same
//   thread Start was called with the results.
//
// This class can handle one request at a time. To parallelize requests,
// create multiple instances.
class COMPONENT_EXPORT(GOOGLE_APIS) OAuth2AccessTokenFetcher {
 public:
  explicit OAuth2AccessTokenFetcher(OAuth2AccessTokenConsumer* consumer);

  OAuth2AccessTokenFetcher(const OAuth2AccessTokenFetcher&) = delete;
  OAuth2AccessTokenFetcher& operator=(const OAuth2AccessTokenFetcher&) = delete;

  virtual ~OAuth2AccessTokenFetcher();

  // Starts the flow with the given parameters.
  // |scopes| can be empty. If it is empty then the access token will have the
  // same scope as the refresh token. If not empty, then access token will have
  // the scopes specified. In this case, the access token will successfully be
  // generated only if refresh token has login scope of a list of scopes that is
  // a super-set of the specified scopes.
  virtual void Start(const std::string& client_id,
                     const std::string& client_secret,
                     const std::vector<std::string>& scopes) = 0;

  // Cancels the current request and informs the consumer.
  virtual void CancelRequest() = 0;

 protected:
  // Fires |OnGetTokenSuccess| on |consumer_|.
  void FireOnGetTokenSuccess(
      const OAuth2AccessTokenConsumer::TokenResponse& token_response);

  // Fires |OnGetTokenFailure| on |consumer_|.
  void FireOnGetTokenFailure(const GoogleServiceAuthError& error);

 private:
  const raw_ptr<OAuth2AccessTokenConsumer> consumer_;
};

#endif  // GOOGLE_APIS_GAIA_OAUTH2_ACCESS_TOKEN_FETCHER_H_
