// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_OAUTH2_ACCESS_TOKEN_FETCHER_IMMEDIATE_ERROR_H_
#define GOOGLE_APIS_GAIA_OAUTH2_ACCESS_TOKEN_FETCHER_IMMEDIATE_ERROR_H_

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"

// This is an implementation of the OAuth2 fetcher that immediately returns
// an error.  This is useful as a replacement to a real fetcher when a
// immediate error has previously been seen.
//
// This class should be used on a single thread, but it can be whichever thread
// that you like.
// Also, do not reuse the same instance. Once Start() is called, the instance
// should not be reused.
//
// Usage:
// * Create an instance with a consumer.
// * Call Start()
// * The consumer passed in the constructor will be called on the same
//   thread Start was called with the results.
//
// This class can handle one request at a time. To parallelize requests,
// create multiple instances.
class COMPONENT_EXPORT(GOOGLE_APIS) OAuth2AccessTokenFetcherImmediateError
    : public OAuth2AccessTokenFetcher {
 public:
  OAuth2AccessTokenFetcherImmediateError(OAuth2AccessTokenConsumer* consumer,
                                         const GoogleServiceAuthError& error);

  OAuth2AccessTokenFetcherImmediateError(
      const OAuth2AccessTokenFetcherImmediateError&) = delete;
  OAuth2AccessTokenFetcherImmediateError& operator=(
      const OAuth2AccessTokenFetcherImmediateError&) = delete;

  ~OAuth2AccessTokenFetcherImmediateError() override;

  void Start(const std::string& client_id,
             const std::string& client_secret,
             const std::vector<std::string>& scopes) override;

  void CancelRequest() override;

 private:

  void Fail();

  GoogleServiceAuthError immediate_error_;
  base::WeakPtrFactory<OAuth2AccessTokenFetcherImmediateError>
      weak_ptr_factory_{this};
};

#endif  // GOOGLE_APIS_GAIA_OAUTH2_ACCESS_TOKEN_FETCHER_IMMEDIATE_ERROR_H_
