// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/gaia_auth_fetcher_ios.h"

#import <WebKit/WebKit.h>
#include <memory>

#include "base/ios/ios_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/signin/gaia_auth_fetcher_ios_wk_webview_bridge.h"
#include "ios/web/public/test/web_task_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class FakeGaiaAuthFetcherIOSWKWebViewBridge
    : public GaiaAuthFetcherIOSWKWebViewBridge {
 public:
  FakeGaiaAuthFetcherIOSWKWebViewBridge(
      GaiaAuthFetcherIOSBridge::GaiaAuthFetcherIOSBridgeDelegate* delegate,
      web::BrowserState* browser_state)
      : GaiaAuthFetcherIOSWKWebViewBridge(delegate, browser_state),
        mock_web_view_(nil) {}

 private:
  WKWebView* BuildWKWebView() override {
    if (!mock_web_view_) {
      mock_web_view_ = [OCMockObject niceMockForClass:[WKWebView class]];
    }
    return mock_web_view_;
  }
  id mock_web_view_;
};

class MockGaiaConsumer : public GaiaAuthConsumer {
 public:
  MockGaiaConsumer() {}
  ~MockGaiaConsumer() {}

  MOCK_METHOD1(OnMergeSessionSuccess, void(const std::string& data));
  MOCK_METHOD1(OnClientLoginFailure, void(const GoogleServiceAuthError& error));
  MOCK_METHOD1(OnLogOutFailure, void(const GoogleServiceAuthError& error));
  MOCK_METHOD1(OnGetCheckConnectionInfoSuccess, void(const std::string& data));
};
}

// Tests fixture for GaiaAuthFetcherIOS
class GaiaAuthFetcherIOSTest : public PlatformTest {
 protected:
  GaiaAuthFetcherIOSTest() {
    browser_state_ = TestChromeBrowserState::Builder().Build();

    ActiveStateManager::FromBrowserState(browser_state())->SetActive(true);
    gaia_auth_fetcher_.reset(new GaiaAuthFetcherIOS(
        &consumer_, gaia::GaiaSource::kChrome,
        test_url_loader_factory_.GetSafeWeakWrapper(), browser_state()));
    gaia_auth_fetcher_->bridge_.reset(new FakeGaiaAuthFetcherIOSWKWebViewBridge(
        gaia_auth_fetcher_.get(), browser_state()));
  }

  ~GaiaAuthFetcherIOSTest() override {
    gaia_auth_fetcher_.reset();
    ActiveStateManager::FromBrowserState(browser_state())->SetActive(false);
  }

  GaiaAuthFetcherIOSBridge* GetBridge() {
    return gaia_auth_fetcher_->bridge_.get();
  }

  ios::ChromeBrowserState* browser_state() { return browser_state_.get(); }

  id GetMockWKWebView() {
    GaiaAuthFetcherIOSWKWebViewBridge* wkWebviewBridge =
        reinterpret_cast<GaiaAuthFetcherIOSWKWebViewBridge*>(
            gaia_auth_fetcher_->bridge_.get());
    return wkWebviewBridge->GetWKWebView();
  }

  web::WebTaskEnvironment task_environment_;
  // BrowserState, required for WKWebView creation.
  std::unique_ptr<ios::ChromeBrowserState> browser_state_;
  MockGaiaConsumer consumer_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<GaiaAuthFetcherIOS> gaia_auth_fetcher_;
};

// Tests that the cancel mechanism works properly by cancelling an OAuthLogin
// request and controlling that the consumer is properly called.
TEST_F(GaiaAuthFetcherIOSTest, StartOAuthLoginCancelled) {
  GoogleServiceAuthError expected_error =
      GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED);
  EXPECT_CALL(consumer_, OnClientLoginFailure(expected_error)).Times(1);

  [static_cast<WKWebView*>([GetMockWKWebView() expect])
      loadRequest:[OCMArg any]];
  [[GetMockWKWebView() expect] stopLoading];

  gaia_auth_fetcher_->StartOAuthLogin("fake_token", "gaia");
  gaia_auth_fetcher_->CancelRequest();
  EXPECT_OCMOCK_VERIFY(GetMockWKWebView());
}

// Tests that the successful case works properly by starting a MergeSession
// request, making it succeed and controlling that the consumer is properly
// called.
TEST_F(GaiaAuthFetcherIOSTest, StartMergeSession) {
  EXPECT_CALL(consumer_, OnMergeSessionSuccess("data")).Times(1);

  [static_cast<WKWebView*>([[GetMockWKWebView() expect] andDo:^(NSInvocation*) {
    GetBridge()->OnURLFetchSuccess("data", 200);
  }]) loadRequest:[OCMArg any]];

  gaia_auth_fetcher_->StartMergeSession("uber_token", "");
  EXPECT_OCMOCK_VERIFY(GetMockWKWebView());
}

// Tests that the failure case works properly by starting a LogOut request,
// making it fail, and controlling that the consumer is properly called.
TEST_F(GaiaAuthFetcherIOSTest, StartLogOutError) {
  GoogleServiceAuthError expected_error =
      GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED);
  EXPECT_CALL(consumer_, OnLogOutFailure(expected_error)).Times(1);

  [static_cast<WKWebView*>([[GetMockWKWebView() expect] andDo:^(NSInvocation*) {
    GetBridge()->OnURLFetchFailure(net::ERR_FAILED, 500);
  }]) loadRequest:[OCMArg any]];

  gaia_auth_fetcher_->StartLogOut();
  EXPECT_OCMOCK_VERIFY(GetMockWKWebView());
}

// Tests that requests that do not require cookies are using the original
// GaiaAuthFetcher and not the GaiaAuthFetcherIOS specialization.
TEST_F(GaiaAuthFetcherIOSTest, StartGetCheckConnectionInfo) {
  std::string data(
      "[{\"carryBackToken\": \"token1\", \"url\": \"http://www.google.com\"}]");
  EXPECT_CALL(consumer_, OnGetCheckConnectionInfoSuccess(data)).Times(1);

  // Set up the fake response.
  test_url_loader_factory_.AddResponse(
      GaiaUrls::GetInstance()
          ->GetCheckConnectionInfoURLWithSource(GaiaConstants::kChromeSource)
          .spec(),
      data);

  gaia_auth_fetcher_->StartGetCheckConnectionInfo();
  base::RunLoop().RunUntilIdle();
}

// Tests whether the WKWebView is actually stopped when the browser state is
// inactive.
TEST_F(GaiaAuthFetcherIOSTest, OnInactive) {
  [[GetMockWKWebView() expect] stopLoading];
  ActiveStateManager::FromBrowserState(browser_state())->SetActive(false);
  EXPECT_OCMOCK_VERIFY(GetMockWKWebView());
}

// Tests that the pending request is processed when the browser state becomes
// active.
TEST_F(GaiaAuthFetcherIOSTest, FetchOnActive) {
  EXPECT_CALL(consumer_, OnMergeSessionSuccess("data")).Times(1);

  // No action is made until the browser state is active, then a WKWebView and
  // its navigation delegate are created, and the request is processed.
  [[GetMockWKWebView() expect] setNavigationDelegate:[OCMArg isNotNil]];
  [static_cast<WKWebView*>([[GetMockWKWebView() expect] andDo:^(NSInvocation*) {
    GetBridge()->OnURLFetchSuccess("data", 200);
  }]) loadRequest:[OCMArg any]];

  ActiveStateManager::FromBrowserState(browser_state())->SetActive(false);
  gaia_auth_fetcher_->StartMergeSession("uber_token", "");
  ActiveStateManager::FromBrowserState(browser_state())->SetActive(true);
  EXPECT_OCMOCK_VERIFY(GetMockWKWebView());
}

// Tests that the pending request is stopped when the browser state becomes
// inactive and restarted when it becomes active again.
TEST_F(GaiaAuthFetcherIOSTest, StopOnInactiveReFetchOnActive) {
  EXPECT_CALL(consumer_, OnMergeSessionSuccess("data")).Times(1);

  [static_cast<WKWebView*>([GetMockWKWebView() expect])
      loadRequest:[OCMArg any]];
  [[GetMockWKWebView() expect] setNavigationDelegate:[OCMArg isNil]];
  [[GetMockWKWebView() expect] setNavigationDelegate:[OCMArg isNotNil]];
  [static_cast<WKWebView*>([[GetMockWKWebView() expect] andDo:^(NSInvocation*) {
    GetBridge()->OnURLFetchSuccess("data", 200);
  }]) loadRequest:[OCMArg any]];

  gaia_auth_fetcher_->StartMergeSession("uber_token", "");
  ActiveStateManager::FromBrowserState(browser_state())->SetActive(false);
  ActiveStateManager::FromBrowserState(browser_state())->SetActive(true);
  EXPECT_OCMOCK_VERIFY(GetMockWKWebView());
}
