// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/signin/web_view_gaia_auth_fetcher.h"

#include "google_apis/gaia/gaia_auth_consumer.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

namespace ios_web_view {

class TestGaiaAuthConsumer : public GaiaAuthConsumer {
 public:
  void OnListAccountsSuccess(const std::string& data) override {
    list_accounts_success_called_ = true;
  }
  void OnListAccountsFailure(const GoogleServiceAuthError& error) override {
    list_accounts_failure_called_ = true;
  }
  bool list_accounts_success_called_ = false;
  bool list_accounts_failure_called_ = false;
};

using WebViewGaiaAuthFetcherTest = PlatformTest;

// Tests ListAccounts fails as expected.
TEST_F(WebViewGaiaAuthFetcherTest, ListAccounts) {
  TestGaiaAuthConsumer consumer;
  WebViewGaiaAuthFetcher fetcher(&consumer, gaia::GaiaSource::kChrome,
                                 /*url_loader_factory=*/nullptr);
  fetcher.StartListAccounts();
  EXPECT_FALSE(consumer.list_accounts_success_called_);
  EXPECT_TRUE(consumer.list_accounts_failure_called_);
}

}  // namespace ios_web_view
