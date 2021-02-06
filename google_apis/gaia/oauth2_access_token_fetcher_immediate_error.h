// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_OAUTH2_ACCESS_TOKEN_FETCHER_IMMEDIATE_ERROR_H_
#define GOOGLE_APIS_GAIA_OAUTH2_ACCESS_TOKEN_FETCHER_IMMEDIATE_ERROR_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
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
class OAuth2AccessTokenFetcherImmediateError : public OAuth2AccessTokenFetcher {
 public:
  OAuth2AccessTokenFetcherImmediateError(OAuth2AccessTokenConsumer* consumer,
                                         const GoogleServiceAuthError& error);
  ~OAuth2AccessTokenFetcherImmediateError() override;

  void Start(const std::string& client_id,
             const std::string& client_secret,
             const std::vector<std::string>& scopes) override;

  void CancelRequest() override;

 private:
  class FailCaller : public base::RefCounted<FailCaller> {
   public:
    FailCaller(OAuth2AccessTokenFetcherImmediateError* fetcher);

    void run();
    void detach();

   private:
    friend class base::RefCounted<FailCaller>;
    ~FailCaller();

    OAuth2AccessTokenFetcherImmediateError* fetcher_;
  };

  void Fail();

  scoped_refptr<FailCaller> failer_;
  GoogleServiceAuthError immediate_error_;
  DISALLOW_COPY_AND_ASSIGN(OAuth2AccessTokenFetcherImmediateError);
};

#endif  // GOOGLE_APIS_GAIA_OAUTH2_ACCESS_TOKEN_FETCHER_IMMEDIATE_ERROR_H_
