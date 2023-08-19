// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/google_service_auth_error.h"

#include <memory>
#include <string>

#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(GoogleServiceAuthErrorTest, State) {
  for (GoogleServiceAuthError::State i = GoogleServiceAuthError::NONE;
       i < GoogleServiceAuthError::NUM_STATES;
       i = GoogleServiceAuthError::State(i + 1)) {
    if (!GoogleServiceAuthError::IsValid(i))
      continue;

    GoogleServiceAuthError error(i);
    EXPECT_EQ(i, error.state());
    EXPECT_TRUE(error.error_message().empty());

    if (i == GoogleServiceAuthError::CONNECTION_FAILED)
      EXPECT_EQ(net::ERR_FAILED, error.network_error());
    else
      EXPECT_EQ(net::OK, error.network_error());

    if (i == GoogleServiceAuthError::NONE) {
      EXPECT_FALSE(error.IsTransientError());
      EXPECT_FALSE(error.IsPersistentError());
    } else if ((i == GoogleServiceAuthError::CONNECTION_FAILED) ||
               (i == GoogleServiceAuthError::SERVICE_UNAVAILABLE) ||
               (i == GoogleServiceAuthError::REQUEST_CANCELED) ||
               (i == GoogleServiceAuthError::CHALLENGE_RESPONSE_REQUIRED)) {
      EXPECT_TRUE(error.IsTransientError());
      EXPECT_FALSE(error.IsPersistentError());
    } else {
      EXPECT_FALSE(error.IsTransientError());
      EXPECT_TRUE(error.IsPersistentError());
    }

    if (i == GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS) {
      EXPECT_EQ(GoogleServiceAuthError::InvalidGaiaCredentialsReason::UNKNOWN,
                error.GetInvalidGaiaCredentialsReason());
    }

    if (i == GoogleServiceAuthError::CHALLENGE_RESPONSE_REQUIRED) {
      EXPECT_TRUE(error.GetTokenBindingChallenge().empty());
    }
  }
}

TEST(GoogleServiceAuthErrorTest, FromConnectionError) {
  GoogleServiceAuthError error =
      GoogleServiceAuthError::FromConnectionError(net::ERR_TIMED_OUT);
  EXPECT_EQ(GoogleServiceAuthError::CONNECTION_FAILED, error.state());
  EXPECT_EQ(net::ERR_TIMED_OUT, error.network_error());
}

TEST(GoogleServiceAuthErrorTest, FromServiceError) {
  GoogleServiceAuthError error =
      GoogleServiceAuthError::FromServiceError("Foo");
  EXPECT_EQ(GoogleServiceAuthError::SERVICE_ERROR, error.state());
  EXPECT_EQ("Foo", error.error_message());
}

TEST(GoogleServiceAuthErrorTest, FromInvalidGaiaCredentialsReason) {
  GoogleServiceAuthError error =
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER);
  EXPECT_EQ(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS, error.state());
  EXPECT_EQ(GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                CREDENTIALS_REJECTED_BY_SERVER,
            error.GetInvalidGaiaCredentialsReason());
  EXPECT_EQ("Invalid credentials (credentials rejected by server).",
            error.ToString());
}

TEST(GoogleServiceAuthErrorTest, AuthErrorNone) {
  EXPECT_EQ(GoogleServiceAuthError(GoogleServiceAuthError::NONE),
            GoogleServiceAuthError::AuthErrorNone());
}

}  // namespace
