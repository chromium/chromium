// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_state_list/web_state_list_favicon_driver_observer.h"

#include "base/logging.h"
#include "components/favicon/ios/web_favicon_driver.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FakeWebStateFaviconDriverObserver
    : NSObject <WebStateFaviconDriverObserver>

@property(nonatomic, readonly) BOOL didUpdateFaviconCalled;

@end

@implementation FakeWebStateFaviconDriverObserver

@synthesize didUpdateFaviconCalled = _didUpdateFaviconCalled;

- (void)faviconDriver:(favicon::FaviconDriver*)driver
    didUpdateFaviconForWebState:(web::WebState*)webState {
  _didUpdateFaviconCalled = YES;
}

@end

class WebStateListFaviconDriverObserverTest : public PlatformTest {
 public:
  WebStateListFaviconDriverObserverTest();
  ~WebStateListFaviconDriverObserverTest() override = default;

  favicon::FaviconDriver* CreateAndInsertWebState();

  WebStateListFaviconDriverObserver* web_state_list_favicon_driver_observer() {
    return &web_state_list_favicon_driver_observer_;
  }

  FakeWebStateFaviconDriverObserver* favicon_observer() {
    return favicon_observer_;
  }

 private:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<ios::ChromeBrowserState> browser_state_;
  FakeWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
  FakeWebStateFaviconDriverObserver* favicon_observer_;
  WebStateListFaviconDriverObserver web_state_list_favicon_driver_observer_;

  DISALLOW_COPY_AND_ASSIGN(WebStateListFaviconDriverObserverTest);
};

WebStateListFaviconDriverObserverTest::WebStateListFaviconDriverObserverTest()
    : browser_state_(TestChromeBrowserState::Builder().Build()),
      web_state_list_(&web_state_list_delegate_),
      favicon_observer_([[FakeWebStateFaviconDriverObserver alloc] init]),

      web_state_list_favicon_driver_observer_(&web_state_list_,
                                              favicon_observer_) {}

favicon::FaviconDriver*
WebStateListFaviconDriverObserverTest::CreateAndInsertWebState() {
  std::unique_ptr<web::WebState> web_state =
      web::WebState::Create(web::WebState::CreateParams(browser_state_.get()));
  favicon::WebFaviconDriver::CreateForWebState(web_state.get(),
                                               /*favicon_service=*/nullptr);

  favicon::FaviconDriver* favicon_driver =
      favicon::WebFaviconDriver::FromWebState(web_state.get());

  web_state_list_.InsertWebState(0, std::move(web_state),
                                 WebStateList::INSERT_FORCE_INDEX,
                                 WebStateOpener());

  return favicon_driver;
}

// Tests that calls to OnFaviconUpdated are forwarded correctly to
// WebStateFaviconDriverObserver.
TEST_F(WebStateListFaviconDriverObserverTest, OnFaviconUpdated) {
  ASSERT_FALSE([favicon_observer() didUpdateFaviconCalled]);

  favicon::FaviconDriver* favicon_driver = CreateAndInsertWebState();
  ASSERT_FALSE([favicon_observer() didUpdateFaviconCalled]);

  web_state_list_favicon_driver_observer()->OnFaviconUpdated(
      favicon_driver, favicon::FaviconDriverObserver::TOUCH_LARGEST, GURL(),
      true, gfx::Image());
  ASSERT_TRUE([favicon_observer() didUpdateFaviconCalled]);
}
