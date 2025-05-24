// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reading_list/model/favicon_web_state_dispatcher_impl.h"

#import "base/memory/ptr_util.h"
#import "base/test/ios/wait_util.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state_observer.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace reading_list {

// Test class.
class FaviconWebStateDispatcherTest : public PlatformTest,
                                      public web::WebStateObserver {
 public:
  FaviconWebStateDispatcherTest() : web_state_destroyed_(false) {
    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build();
  }

  web::BrowserState* GetBrowserState() { return profile_.get(); }

  bool IsWebStateDestroyed() { return web_state_destroyed_; }

  // web::WebStateObserver implementation.
  void WebStateDestroyed(web::WebState* web_state) override {
    web_state->RemoveObserver(this);
    web_state_destroyed_ = true;
  }

 private:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  bool web_state_destroyed_;
};

// Tests that RequestWebState returns a WebState with a FaviconDriver attached.
TEST_F(FaviconWebStateDispatcherTest, RequestWebState) {
  FaviconWebStateDispatcherImpl dispatcher(GetBrowserState(), -1);
  std::unique_ptr<web::WebState> web_state = dispatcher.RequestWebState();

  favicon::WebFaviconDriver* driver =
      favicon::WebFaviconDriver::FromWebState(web_state.get());
  EXPECT_NE(driver, nullptr);
}

// Tests that the WebState returned will be destroyed after a delay.
TEST_F(FaviconWebStateDispatcherTest, ReturnWebState) {
  FaviconWebStateDispatcherImpl dispatcher(GetBrowserState(), 0);
  std::unique_ptr<web::WebState> web_state = dispatcher.RequestWebState();
  web_state->AddObserver(this);

  ConditionBlock condition = ^{
    return IsWebStateDestroyed();
  };

  dispatcher.ReturnWebState(std::move(web_state));

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::Milliseconds(500), condition));
}
}  // namespace reading_list
