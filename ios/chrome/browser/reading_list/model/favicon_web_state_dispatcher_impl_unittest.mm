// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reading_list/model/favicon_web_state_dispatcher_impl.h"

#import "base/memory/raw_ptr.h"
#import "base/run_loop.h"
#import "base/scoped_observation.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state_observer.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace reading_list {

// WebStateObserver that invoke a closure when a WebState is destroyed.
class WaitForWebStateDestructionObserver : public web::WebStateObserver {
 public:
  WaitForWebStateDestructionObserver(web::WebState* web_state,
                                     base::OnceClosure closure)
      : closure_(std::move(closure)) {
    web_state_observation_.Observe(web_state);
  }

  bool web_state_destroyed() const { return web_state_destroyed_; }

  // web::WebStateObserver implementation.
  void WebStateDestroyed(web::WebState* web_state) override {
    web_state_observation_.Reset();
    web_state_destroyed_ = true;
    std::move(closure_).Run();
  }

 private:
  base::OnceClosure closure_;
  bool web_state_destroyed_ = false;
  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};
};

// Test class.
class FaviconWebStateDispatcherTest : public PlatformTest {
 public:
  FaviconWebStateDispatcherTest() {
    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build();
  }

  ProfileIOS* profile() { return profile_.get(); }

 private:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
};

// Tests that RequestWebState returns a WebState with a FaviconDriver attached.
TEST_F(FaviconWebStateDispatcherTest, RequestWebState) {
  FaviconWebStateDispatcherImpl dispatcher(profile());
  std::unique_ptr<web::WebState> web_state = dispatcher.RequestWebState();

  favicon::WebFaviconDriver* driver =
      favicon::WebFaviconDriver::FromWebState(web_state.get());
  EXPECT_NE(driver, nullptr);
}

// Tests that the WebState returned will be destroyed after a delay.
TEST_F(FaviconWebStateDispatcherTest, ReturnWebState) {
  FaviconWebStateDispatcherImpl dispatcher(profile(), base::Seconds(0.5));
  std::unique_ptr<web::WebState> web_state = dispatcher.RequestWebState();

  base::RunLoop run_loop;
  WaitForWebStateDestructionObserver observer(web_state.get(),
                                              run_loop.QuitClosure());
  dispatcher.ReturnWebState(std::move(web_state));
  run_loop.Run();

  EXPECT_TRUE(observer.web_state_destroyed());
}

}  // namespace reading_list
