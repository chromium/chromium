// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/google_service_auth_error.h"

#include <memory>
#include <string>

#include "google_apis/gaia/fake_device_management_error_details.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(GoogleServiceAuthErrorTest, FromConnectionError) {
  GoogleServiceAuthError error =
      GoogleServiceAuthError::FromConnectionError(net::ERR_TIMED_OUT);
  EXPECT_EQ(GoogleServiceAuthError::CONNECTION_FAILED, error.state());
  EXPECT_EQ(net::ERR_TIMED_OUT, error.GetNetworkError());
  EXPECT_TRUE(error.IsTransientError());
  EXPECT_FALSE(error.IsPersistentError());
}

TEST(GoogleServiceAuthErrorTest, FromServiceUnavailable) {
  GoogleServiceAuthError error =
      GoogleServiceAuthError::FromServiceUnavailable("Foo");
  EXPECT_EQ(GoogleServiceAuthError::SERVICE_UNAVAILABLE, error.state());
  EXPECT_EQ("Foo", error.error_message());
  EXPECT_TRUE(error.IsTransientError());
  EXPECT_FALSE(error.IsPersistentError());
}

TEST(GoogleServiceAuthErrorTest, CreateRequestCanceled) {
  GoogleServiceAuthError error =
      GoogleServiceAuthError::CreateRequestCanceled();
  EXPECT_EQ(GoogleServiceAuthError::REQUEST_CANCELED, error.state());
  EXPECT_TRUE(error.IsTransientError());
  EXPECT_FALSE(error.IsPersistentError());
}

TEST(GoogleServiceAuthErrorTest, FromTokenBindingChallenge) {
  GoogleServiceAuthError error =
      GoogleServiceAuthError::FromTokenBindingChallenge("Foo");
  EXPECT_EQ(GoogleServiceAuthError::CHALLENGE_RESPONSE_REQUIRED, error.state());
  EXPECT_EQ("Foo", error.GetTokenBindingChallenge());
  EXPECT_TRUE(error.IsTransientError());
  EXPECT_FALSE(error.IsPersistentError());
}

TEST(GoogleServiceAuthErrorTest, FromServiceError) {
  GoogleServiceAuthError error =
      GoogleServiceAuthError::FromServiceError("Foo");
  EXPECT_EQ(GoogleServiceAuthError::SERVICE_ERROR, error.state());
  EXPECT_EQ("Foo", error.error_message());
  EXPECT_FALSE(error.IsTransientError());
  EXPECT_TRUE(error.IsPersistentError());
}

TEST(GoogleServiceAuthErrorTest, FromUnexpectedServiceResponse) {
  GoogleServiceAuthError error =
      GoogleServiceAuthError::FromUnexpectedServiceResponse("Foo");
  EXPECT_EQ(GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE, error.state());
  EXPECT_EQ("Foo", error.error_message());
  EXPECT_FALSE(error.IsTransientError());
  EXPECT_TRUE(error.IsPersistentError());
}

TEST(GoogleServiceAuthErrorTest, FromDeviceManagementError) {
  GoogleServiceAuthError error =
      GoogleServiceAuthError::FromDeviceManagementError(
          std::make_unique<gaia::FakeDeviceManagementErrorDetails>());
  EXPECT_EQ(GoogleServiceAuthError::DEVICE_MANAGEMENT_ERROR, error.state());
  EXPECT_FALSE(error.IsTransientError());
  EXPECT_TRUE(error.IsPersistentError());
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
  EXPECT_FALSE(error.IsTransientError());
  EXPECT_TRUE(error.IsPersistentError());
  EXPECT_EQ("Invalid credentials (credentials rejected by server).",
            error.ToString());
}

TEST(GoogleServiceAuthErrorTest, FromScopeLimitedUnrecoverableErrorReason) {
  GoogleServiceAuthError error =
      GoogleServiceAuthError::FromScopeLimitedUnrecoverableErrorReason(
          GoogleServiceAuthError::ScopeLimitedUnrecoverableErrorReason::
              kAdminPolicyEnforced);
  EXPECT_EQ(GoogleServiceAuthError::SCOPE_LIMITED_UNRECOVERABLE_ERROR,
            error.state());
  EXPECT_EQ(GoogleServiceAuthError::ScopeLimitedUnrecoverableErrorReason::
                kAdminPolicyEnforced,
            error.GetScopeLimitedUnrecoverableErrorReason());
  EXPECT_FALSE(error.IsTransientError());
  EXPECT_TRUE(error.IsPersistentError());
  EXPECT_EQ("OAuth scope error (admin policy enforced).", error.ToString());
}

TEST(GoogleServiceAuthErrorTest, CreateAccountNotFound) {
  GoogleServiceAuthError error =
      GoogleServiceAuthError::CreateAccountNotFound();
  EXPECT_EQ(GoogleServiceAuthError::ACCOUNT_NOT_FOUND, error.state());
  EXPECT_FALSE(error.IsTransientError());
  EXPECT_TRUE(error.IsPersistentError());
}

TEST(GoogleServiceAuthErrorTest, AuthErrorNone) {
  GoogleServiceAuthError error = GoogleServiceAuthError::AuthErrorNone();
  EXPECT_EQ(GoogleServiceAuthError::NONE, error.state());
  EXPECT_FALSE(error.IsTransientError());
  EXPECT_FALSE(error.IsPersistentError());
}

}  // namespace
