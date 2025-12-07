// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/web_view_proxy/web_view_proxy_tab_helper.h"

#import "base/scoped_observation.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

// A mock observer for WebViewProxyTabHelper.
class MockWebViewProxyTabHelperObserver
    : public WebViewProxyTabHelper::Observer {
 public:
  MockWebViewProxyTabHelperObserver() = default;

  MOCK_METHOD(void,
              WebViewProxyChanged,
              (WebViewProxyTabHelper * tab_helper),
              (override));
  MOCK_METHOD(void,
              WebViewProxyTabHelperDestroyed,
              (WebViewProxyTabHelper * tab_helper),
              (override));
};

// Test fixture for WebViewProxyTabHelper.
class WebViewProxyTabHelperTest : public PlatformTest {
 protected:
  WebViewProxyTabHelperTest()
      : web_state_(std::make_unique<web::FakeWebState>()) {
    WebViewProxyTabHelper::CreateForWebState(web_state_.get());
  }

  WebViewProxyTabHelper* tab_helper() {
    return WebViewProxyTabHelper::FromWebState(web_state_.get());
  }

  std::unique_ptr<web::FakeWebState> web_state_;
};

// Tests that the observer is notified when the web view proxy changes.
TEST_F(WebViewProxyTabHelperTest, WebViewProxyChanged) {
  MockWebViewProxyTabHelperObserver observer;
  base::ScopedObservation<WebViewProxyTabHelper,
                          WebViewProxyTabHelper::Observer>
      observation(&observer);
  observation.Observe(tab_helper());

  EXPECT_CALL(observer, WebViewProxyChanged(tab_helper()));
  tab_helper()->SetOverridingWebViewProxy(
      OCMProtocolMock(@protocol(CRWWebViewProxy)));
}

// Tests that the observer is notified when the tab helper is destroyed.
TEST_F(WebViewProxyTabHelperTest, WebViewProxyTabHelperDestroyed) {
  MockWebViewProxyTabHelperObserver observer;
  tab_helper()->AddObserver(&observer);

  EXPECT_CALL(observer, WebViewProxyTabHelperDestroyed(tab_helper()))
      .WillOnce([&observer](WebViewProxyTabHelper* tab_helper) {
        tab_helper->RemoveObserver(&observer);
      });
  web_state_.reset();
}

// Tests that the observer is not notified after it has been removed.
TEST_F(WebViewProxyTabHelperTest, ObserverRemoved) {
  MockWebViewProxyTabHelperObserver observer;
  base::ScopedObservation<WebViewProxyTabHelper,
                          WebViewProxyTabHelper::Observer>
      observation(&observer);
  observation.Observe(tab_helper());
  observation.Reset();

  EXPECT_CALL(observer, WebViewProxyChanged(tab_helper())).Times(0);
  tab_helper()->SetOverridingWebViewProxy(
      OCMProtocolMock(@protocol(CRWWebViewProxy)));
}
