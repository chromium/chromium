// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_state_list/model/web_state_list_favicon_driver_observer.h"

#import "components/favicon/ios/web_favicon_driver.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "ui/gfx/image/image.h"
#import "url/gurl.h"

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

  WebStateListFaviconDriverObserverTest(
      const WebStateListFaviconDriverObserverTest&) = delete;
  WebStateListFaviconDriverObserverTest& operator=(
      const WebStateListFaviconDriverObserverTest&) = delete;

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
  std::unique_ptr<ProfileIOS> profile_;
  FakeWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
  FakeWebStateFaviconDriverObserver* favicon_observer_;
  WebStateListFaviconDriverObserver web_state_list_favicon_driver_observer_;
};

WebStateListFaviconDriverObserverTest::WebStateListFaviconDriverObserverTest()
    : profile_(TestProfileIOS::Builder().Build()),
      web_state_list_(&web_state_list_delegate_),
      favicon_observer_([[FakeWebStateFaviconDriverObserver alloc] init]),

      web_state_list_favicon_driver_observer_(&web_state_list_,
                                              favicon_observer_) {}

favicon::FaviconDriver*
WebStateListFaviconDriverObserverTest::CreateAndInsertWebState() {
  std::unique_ptr<web::WebState> web_state =
      web::WebState::Create(web::WebState::CreateParams(profile_.get()));
  favicon::WebFaviconDriver::CreateForWebState(web_state.get(),
                                               /*favicon_service=*/nullptr);

  favicon::FaviconDriver* favicon_driver =
      favicon::WebFaviconDriver::FromWebState(web_state.get());

  web_state_list_.InsertWebState(std::move(web_state),
                                 WebStateList::InsertionParams::AtIndex(0));

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
