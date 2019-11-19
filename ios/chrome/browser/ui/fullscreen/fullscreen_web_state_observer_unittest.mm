// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_web_state_observer.h"

#import "ios/chrome/browser/ui/fullscreen/fullscreen_model.h"
#import "ios/chrome/browser/ui/fullscreen/test/fullscreen_model_test_util.h"
#import "ios/chrome/browser/ui/fullscreen/test/test_fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/test/test_fullscreen_mediator.h"
#import "ios/web/public/navigation/navigation_item.h"
#include "ios/web/public/security/ssl_status.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class FullscreenWebStateObserverTest : public PlatformTest {
 public:
  FullscreenWebStateObserverTest()
      : PlatformTest(),
        controller_(&model_),
        mediator_(&controller_, &model_),
        observer_(&controller_, &model_, &mediator_) {
    // Set up a TestNavigationManager.
    std::unique_ptr<web::TestNavigationManager> navigation_manager =
        std::make_unique<web::TestNavigationManager>();
    navigation_manager_ = navigation_manager.get();
    web_state_.SetNavigationManager(std::move(navigation_manager));
    // Begin observing the WebState.
    observer_.SetWebState(&web_state_);
    // Set up model.
    SetUpFullscreenModelForTesting(&model_, 100.0);
  }

  ~FullscreenWebStateObserverTest() override {
    mediator_.Disconnect();
    observer_.SetWebState(nullptr);
  }

  FullscreenModel& model() { return model_; }
  web::TestWebState& web_state() { return web_state_; }
  web::TestNavigationManager& navigation_manager() {
    return *navigation_manager_;
  }

 private:
  FullscreenModel model_;
  TestFullscreenController controller_;
  TestFullscreenMediator mediator_;
  web::TestWebState web_state_;
  web::TestNavigationManager* navigation_manager_;
  FullscreenWebStateObserver observer_;
};

// Tests that the model is reset when a navigation is committed.
TEST_F(FullscreenWebStateObserverTest, ResetForNavigation) {
  // Simulate a scroll to 0.5 progress.
  SimulateFullscreenUserScrollForProgress(&model(), 0.5);
  EXPECT_EQ(0.5, model().progress());
  // Simulate a navigation.
  web::FakeNavigationContext context;
  web_state().OnNavigationFinished(&context);
  EXPECT_FALSE(model().has_base_offset());
  EXPECT_EQ(1.0, model().progress());
}

// Tests that the FullscreenModel is not reset for same-document navigations
// with the same URL.
TEST_F(FullscreenWebStateObserverTest, NoResetForSameDocumentSameURL) {
  // Navigate to a URL.
  web::FakeNavigationContext context;
  context.SetUrl(GURL("https://www.test.com"));
  web_state().OnNavigationFinished(&context);
  model().SetYContentOffset(0.0);
  // Simulate a scroll to 0.5 progress.
  SimulateFullscreenUserScrollForProgress(&model(), 0.5);
  EXPECT_EQ(0.5, model().progress());
  // Simulate a same-document navigation to the same URL and verify that the 0.5
  // progress hasn't been reset to 1.0.
  context.SetIsSameDocument(true);
  web_state().OnNavigationFinished(&context);
  EXPECT_EQ(0.5, model().progress());
}

// Tests that the FullscreenModel is not reset for a same-document navigation.
TEST_F(FullscreenWebStateObserverTest, NoResetForSameDocumentFragmentChange) {
  // Navigate to a URL.
  web::FakeNavigationContext context;
  context.SetUrl(GURL("https://www.test.com"));
  web_state().OnNavigationFinished(&context);
  model().SetYContentOffset(0.0);
  // Simulate a scroll to 0.5 progress.
  SimulateFullscreenUserScrollForProgress(&model(), 0.5);
  EXPECT_EQ(0.5, model().progress());
  // Simulate a same-document navigation to a URL with a different fragment and
  // verify that the 0.5 progress hasn't been reset to 1.0.
  context.SetUrl(GURL("https://www.test.com#fragment"));
  context.SetIsSameDocument(true);
  web_state().OnNavigationFinished(&context);
  EXPECT_EQ(0.5, model().progress());
}

// Tests that the FullscreenModel is not reset for a same-document navigation.
TEST_F(FullscreenWebStateObserverTest, ResetForSameDocumentURLChange) {
  // Navigate to a URL.
  web::FakeNavigationContext context;
  context.SetUrl(GURL("https://www.test.com"));
  web_state().OnNavigationFinished(&context);
  model().SetYContentOffset(0.0);
  // Simulate a scroll to 0.5 progress.
  SimulateFullscreenUserScrollForProgress(&model(), 0.5);
  EXPECT_EQ(0.5, model().progress());
  // Simulate a same-document navigation to a new URL and verify that the 0.5
  // progress is reset to 1.0.
  context.SetUrl(GURL("https://www.test2.com"));
  context.SetIsSameDocument(true);
  web_state().OnNavigationFinished(&context);
  EXPECT_EQ(1.0, model().progress());
}
