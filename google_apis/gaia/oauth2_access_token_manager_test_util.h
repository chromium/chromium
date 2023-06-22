// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_OAUTH2_ACCESS_TOKEN_MANAGER_TEST_UTIL_H_
#define GOOGLE_APIS_GAIA_OAUTH2_ACCESS_TOKEN_MANAGER_TEST_UTIL_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"

std::string GetValidTokenResponse(const std::string& token, int expiration);

std::string GetValidBoundTokenResponse(const std::string& token,
                                       base::TimeDelta time_to_live,
                                       const std::vector<std::string>& scopes);

// A simple testing consumer.
class TestingOAuth2AccessTokenManagerConsumer
    : public OAuth2AccessTokenManager::Consumer {
 public:
  TestingOAuth2AccessTokenManagerConsumer();
  ~TestingOAuth2AccessTokenManagerConsumer() override;

  // OAuth2AccessTokenManager::Consumer overrides.
  void OnGetTokenSuccess(
      const OAuth2AccessTokenManager::Request* request,
      const OAuth2AccessTokenConsumer::TokenResponse& token_response) override;
  void OnGetTokenFailure(const OAuth2AccessTokenManager::Request* request,
                         const GoogleServiceAuthError& error) override;

  std::string last_token_;
  int number_of_successful_tokens_;
  GoogleServiceAuthError last_error_;
  int number_of_errors_;
};

#endif  // GOOGLE_APIS_GAIA_OAUTH2_ACCESS_TOKEN_MANAGER_TEST_UTIL_H_
