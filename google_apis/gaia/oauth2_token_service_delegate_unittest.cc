// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <string>

#include "base/macros.h"
#include "google_apis/gaia/fake_oauth2_token_service_delegate.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_token_service_delegate.h"
#include "google_apis/gaia/oauth2_token_service_test_util.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// The default implementation of
// OAuth2TokenServiceDelegate::InvalidateTokensForMultilogin is used on mobile
// where refresh tokens are not accessible. This test checks that refresh tokens
// are not affected on INVALID_TOKENS Multilogin error.
TEST(OAuth2TokenServiceDelegateTest, InvalidateTokensForMultilogin) {
  class TokenServiceObserver : public OAuth2TokenService::Observer {
   public:
    MOCK_METHOD2(OnAuthErrorChanged,
                 void(const std::string&, const GoogleServiceAuthError&));
  };

  FakeOAuth2TokenServiceDelegate delegate;

  TokenServiceObserver observer;
  delegate.AddObserver(&observer);
  // Check that OnAuthErrorChanged is not fired from
  // InvalidateTokensForMultilogin and refresh tokens are not set in error.
  EXPECT_CALL(
      observer,
      OnAuthErrorChanged(::testing::_,
                         GoogleServiceAuthError(
                             GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS)))
      .Times(0);

  const std::string account_id1 = "account_id1";
  const std::string account_id2 = "account_id2";

  delegate.UpdateCredentials(account_id1, "refresh_token1");
  delegate.UpdateCredentials(account_id2, "refresh_token2");

  delegate.InvalidateTokenForMultilogin(account_id1);

  EXPECT_EQ(delegate.GetAuthError(account_id1).state(),
            GoogleServiceAuthError::NONE);
  EXPECT_EQ(delegate.GetAuthError(account_id2).state(),
            GoogleServiceAuthError::NONE);

  delegate.RemoveObserver(&observer);
}
