// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/oauth2_access_token_manager_test_util.h"

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"

namespace {
const char kValidTokenResponseFormat[] = R"(
    {
      "access_token": "%s",
      "expires_in": %d,
      "token_type": "Bearer"
    }
)";

const char kValidBoundTokenResponseFormat[] = R"(
  {
      "token": "%s",
      "issueAdvice": "auto",
      "expiresIn": "%d",
      "grantedScopes": "%s"
  }
)";
}  // namespace

std::string GetValidTokenResponse(const std::string& token, int expiration) {
  return base::StringPrintf(kValidTokenResponseFormat, token.c_str(),
                            expiration);
}

std::string GetValidBoundTokenResponse(const std::string& token,
                                       base::TimeDelta time_to_live,
                                       const std::vector<std::string>& scopes) {
  std::string scopes_string = base::JoinString(scopes, " ");
  return base::StringPrintf(kValidBoundTokenResponseFormat, token.c_str(),
                            static_cast<int>(time_to_live.InSeconds()),
                            scopes_string.c_str());
}

TestingOAuth2AccessTokenManagerConsumer::
    TestingOAuth2AccessTokenManagerConsumer()
    : OAuth2AccessTokenManager::Consumer("test"),
      number_of_successful_tokens_(0),
      last_error_(GoogleServiceAuthError::AuthErrorNone()),
      number_of_errors_(0) {}

TestingOAuth2AccessTokenManagerConsumer::
    ~TestingOAuth2AccessTokenManagerConsumer() {}

void TestingOAuth2AccessTokenManagerConsumer::OnGetTokenSuccess(
    const OAuth2AccessTokenManager::Request* request,
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  last_token_ = token_response.access_token;
  ++number_of_successful_tokens_;
}

void TestingOAuth2AccessTokenManagerConsumer::OnGetTokenFailure(
    const OAuth2AccessTokenManager::Request* request,
    const GoogleServiceAuthError& error) {
  last_error_ = error;
  ++number_of_errors_;
}
