// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/url_loading/model/url_loading_observer_bridge.h"

#import "base/memory/raw_ptr.h"
#import "base/rand_util.h"
#import "base/scoped_observation.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_observer.h"
#import "ios/chrome/browser/url_loading/model/url_loading_observer_bridge.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {
// Test URL.
const char kTestUrl[] = "https://chromium.test/";
}  // namespace

class UrlLoadingObserverBridgeTest : public PlatformTest {
 protected:
  UrlLoadingObserverBridgeTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());

    url_loading_notifier_ =
        UrlLoadingNotifierBrowserAgent::FromBrowser(browser_.get());
    observer_ = OCMProtocolMock(@protocol(URLLoadingObserving));
    observer_bridge_ = std::make_unique<UrlLoadingObserverBridge>(observer_);
    scoped_observation_ =
        std::make_unique<base::ScopedObservation<UrlLoadingNotifierBrowserAgent,
                                                 UrlLoadingObserver>>(
            observer_bridge_.get());
    scoped_observation_->Observe(url_loading_notifier_.get());
  }

  // Returns URL loading notifier.
  UrlLoadingNotifierBrowserAgent* url_loading_notifier() {
    return url_loading_notifier_;
  }

  // Returns the url loading observer.
  id observer() { return observer_; }

 private:
  // Environment dependencies.
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  // Test dependencies.
  raw_ptr<UrlLoadingNotifierBrowserAgent> url_loading_notifier_;
  id observer_;
  std::unique_ptr<UrlLoadingObserverBridge> observer_bridge_;
  std::unique_ptr<base::ScopedObservation<UrlLoadingNotifierBrowserAgent,
                                          UrlLoadingObserver>>
      scoped_observation_;
};

// Tests `TabWillLoadUrl` forwarding.
TEST_F(UrlLoadingObserverBridgeTest, TabWillLoadUrl) {
  const GURL url(kTestUrl);
  OCMExpect([observer()
      tabWillLoadURL:url
      transitionType:ui::PageTransition::PAGE_TRANSITION_TYPED]);
  url_loading_notifier()->TabWillLoadUrl(
      url, ui::PageTransition::PAGE_TRANSITION_TYPED);
  EXPECT_OCMOCK_VERIFY(observer());
}

// Tests `TabFailedToLoadUrl` forwarding.
TEST_F(UrlLoadingObserverBridgeTest, TabFailedToLoadUrl) {
  const GURL url(kTestUrl);
  OCMExpect([observer()
      tabFailedToLoadURL:url
          transitionType:ui::PageTransition::PAGE_TRANSITION_AUTO_BOOKMARK]);
  url_loading_notifier()->TabFailedToLoadUrl(
      url, ui::PageTransition::PAGE_TRANSITION_AUTO_BOOKMARK);
  EXPECT_OCMOCK_VERIFY(observer());
}

// Tests `TabDidPrerenderUrl` forwarding.
TEST_F(UrlLoadingObserverBridgeTest, TabDidPrerenderUrl) {
  const GURL url(kTestUrl);
  OCMExpect([observer()
      tabDidPrerenderURL:url
          transitionType:ui::PageTransition::PAGE_TRANSITION_FIRST]);
  url_loading_notifier()->TabDidPrerenderUrl(
      url, ui::PageTransition::PAGE_TRANSITION_FIRST);
  EXPECT_OCMOCK_VERIFY(observer());
}

// Tests `TabDidReloadUrl` forwarding.
TEST_F(UrlLoadingObserverBridgeTest, TabDidReloadUrl) {
  const GURL url(kTestUrl);
  OCMExpect([observer()
      tabDidReloadURL:url
       transitionType:ui::PageTransition::PAGE_TRANSITION_RELOAD]);
  url_loading_notifier()->TabDidReloadUrl(
      url, ui::PageTransition::PAGE_TRANSITION_RELOAD);
  EXPECT_OCMOCK_VERIFY(observer());
}

// Tests `TabDidLoadUrl` forwarding.
TEST_F(UrlLoadingObserverBridgeTest, TabDidLoadUrl) {
  const GURL url(kTestUrl);
  OCMExpect([observer()
       tabDidLoadURL:url
      transitionType:ui::PageTransition::PAGE_TRANSITION_FORM_SUBMIT]);
  url_loading_notifier()->TabDidLoadUrl(
      url, ui::PageTransition::PAGE_TRANSITION_FORM_SUBMIT);
  EXPECT_OCMOCK_VERIFY(observer());
}

// Tests `NewTabWillLoadUrl` forwarding.
TEST_F(UrlLoadingObserverBridgeTest, NewTabWillLoadUrl) {
  const GURL url_1(kTestUrl);
  const GURL url_2(kTestUrl);
  OCMExpect([observer() newTabWillLoadURL:url_1 isUserInitiated:YES]);
  OCMExpect([observer() newTabWillLoadURL:url_2 isUserInitiated:NO]);
  url_loading_notifier()->NewTabWillLoadUrl(url_1, true);
  url_loading_notifier()->NewTabWillLoadUrl(url_2, false);
  EXPECT_OCMOCK_VERIFY(observer());
}

// Tests `NewTabDidLoadUrl` forwarding.
TEST_F(UrlLoadingObserverBridgeTest, NewTabDidLoadUrl) {
  const GURL url_1(kTestUrl);
  const GURL url_2(kTestUrl);
  OCMExpect([observer() newTabDidLoadURL:url_1 isUserInitiated:YES]);
  OCMExpect([observer() newTabDidLoadURL:url_2 isUserInitiated:NO]);
  url_loading_notifier()->NewTabDidLoadUrl(url_1, true);
  url_loading_notifier()->NewTabDidLoadUrl(url_2, false);
  EXPECT_OCMOCK_VERIFY(observer());
}

// Tests `WillSwitchToTabWithUrl` forwarding.
TEST_F(UrlLoadingObserverBridgeTest, WillSwitchToTabWithUrl) {
  const GURL url(kTestUrl);
  int idx = base::RandInt(0, 1000);
  OCMExpect([observer() willSwitchToTabWithURL:url newWebStateIndex:idx]);
  url_loading_notifier()->WillSwitchToTabWithUrl(url, idx);
  EXPECT_OCMOCK_VERIFY(observer());
}

// Tests `DidSwitchToTabWithUrl` forwarding.
TEST_F(UrlLoadingObserverBridgeTest, DidSwitchToTabWithUrl) {
  const GURL url(kTestUrl);
  int idx = base::RandInt(0, 1000);
  OCMExpect([observer() didSwitchToTabWithURL:url newWebStateIndex:idx]);
  url_loading_notifier()->DidSwitchToTabWithUrl(url, idx);
  EXPECT_OCMOCK_VERIFY(observer());
}
