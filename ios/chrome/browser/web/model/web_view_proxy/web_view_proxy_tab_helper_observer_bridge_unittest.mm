// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/web_view_proxy/web_view_proxy_tab_helper_observer_bridge.h"

#import <memory>

#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

@protocol TestWebViewProxyTabHelperObserving <WebViewProxyTabHelperObserving>
@end

// Test fixture for WebViewProxyTabHelperObserverBridge.
class WebViewProxyTabHelperObserverBridgeTest : public PlatformTest {
 public:
  WebViewProxyTabHelperObserverBridgeTest() {
    web_state_ = std::make_unique<web::FakeWebState>();
    WebViewProxyTabHelper::CreateForWebState(web_state_.get());
    mock_observer_ = [OCMockObject
        mockForProtocol:@protocol(TestWebViewProxyTabHelperObserving)];
    bridge_ =
        std::make_unique<WebViewProxyTabHelperObserverBridge>(mock_observer_);
    tab_helper()->AddObserver(bridge_.get());
  }

  ~WebViewProxyTabHelperObserverBridgeTest() override {
    if (web_state_) {
      tab_helper()->RemoveObserver(bridge_.get());
    }
  }

  WebViewProxyTabHelper* tab_helper() {
    return WebViewProxyTabHelper::FromWebState(web_state_.get());
  }

 protected:
  std::unique_ptr<web::FakeWebState> web_state_;
  id mock_observer_;
  std::unique_ptr<WebViewProxyTabHelperObserverBridge> bridge_;
};

// Tests that the bridge forwards the WebViewProxyChanged event.
TEST_F(WebViewProxyTabHelperObserverBridgeTest, WebViewProxyChanged) {
  [[mock_observer_ expect] webViewProxyDidChange:tab_helper()];
  tab_helper()->SetOverridingWebViewProxy(nil);
  [mock_observer_ verify];
}

// Tests that the bridge forwards the WebViewProxyTabHelperDestroyed event.
TEST_F(WebViewProxyTabHelperObserverBridgeTest,
       WebViewProxyTabHelperDestroyed) {
  WebViewProxyTabHelper* helper = tab_helper();
  [[[mock_observer_ expect] andDo:^(NSInvocation* invocation) {
    // The user of the bridge is responsible for removing the observer when the
    // tab helper is destroyed.
    helper->RemoveObserver(bridge_.get());
  }] webViewProxyTabHelperWasDestroyed:helper];
  web_state_.reset();
  [mock_observer_ verify];
}
