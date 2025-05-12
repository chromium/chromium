// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_web_state_observer.h"

#import "base/memory/raw_ptr.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_model.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/test/fullscreen_model_test_util.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/test/test_fullscreen_controller.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/test/test_fullscreen_mediator.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/security/ssl_status.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"

class FullscreenWebStateObserverTest : public PlatformTest {
 public:
  FullscreenWebStateObserverTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    TestFullscreenController::CreateForBrowser(browser_.get());
    TestFullscreenController* controller =
        TestFullscreenController::FromBrowser(browser_.get());
    mediator_ = std::make_unique<TestFullscreenMediator>(
        controller, controller->getModel());
    observer_ = std::make_unique<FullscreenWebStateObserver>(
        controller, controller->getModel(), mediator_.get());
    // Set up a FakeNavigationManager.
    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    navigation_manager_ = navigation_manager.get();
    web_state_.SetNavigationManager(std::move(navigation_manager));
    // Begin observing the WebState.
    observer_->SetWebState(&web_state_);
    // Set up model.
    SetUpFullscreenModelForTesting(controller->getModel(), 100.0);
  }

  ~FullscreenWebStateObserverTest() override {
    mediator_->Disconnect();
    observer_->SetWebState(nullptr);
  }

  FullscreenModel* model() {
    return TestFullscreenController::FromBrowser(browser_.get())->getModel();
  }
  web::FakeWebState& web_state() { return web_state_; }
  web::FakeNavigationManager& navigation_manager() {
    return *navigation_manager_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  std::unique_ptr<TestFullscreenMediator> mediator_;
  std::unique_ptr<FullscreenWebStateObserver> observer_;
  web::FakeWebState web_state_;
  raw_ptr<web::FakeNavigationManager> navigation_manager_;
};

// Tests that the model is reset when a navigation is committed.
TEST_F(FullscreenWebStateObserverTest, ResetForNavigation) {
  // Simulate a scroll to 0.5 progress.
  SimulateFullscreenUserScrollForProgress(model(), 0.5);
  EXPECT_EQ(0.5, model()->progress());
  // Simulate a navigation.
  web::FakeNavigationContext context;
  web_state().OnNavigationFinished(&context);
  EXPECT_FALSE(model()->has_base_offset());
  EXPECT_EQ(1.0, model()->progress());
}

// Tests that the FullscreenModel is not reset for same-document navigations
// with the same URL.
TEST_F(FullscreenWebStateObserverTest, NoResetForSameDocumentSameURL) {
  // Navigate to a URL.
  web::FakeNavigationContext context;
  context.SetUrl(GURL("https://www.test.com"));
  web_state().OnNavigationFinished(&context);
  model()->SetYContentOffset(0.0);
  // Simulate a scroll to 0.5 progress.
  SimulateFullscreenUserScrollForProgress(model(), 0.5);
  EXPECT_EQ(0.5, model()->progress());
  // Simulate a same-document navigation to the same URL and verify that the 0.5
  // progress hasn't been reset to 1.0.
  context.SetIsSameDocument(true);
  web_state().OnNavigationFinished(&context);
  EXPECT_EQ(0.5, model()->progress());
}

// Tests that the FullscreenModel is not reset for a same-document navigation.
TEST_F(FullscreenWebStateObserverTest, NoResetForSameDocumentFragmentChange) {
  // Navigate to a URL.
  web::FakeNavigationContext context;
  context.SetUrl(GURL("https://www.test.com"));
  web_state().OnNavigationFinished(&context);
  model()->SetYContentOffset(0.0);
  // Simulate a scroll to 0.5 progress.
  SimulateFullscreenUserScrollForProgress(model(), 0.5);
  EXPECT_EQ(0.5, model()->progress());
  // Simulate a same-document navigation to a URL with a different fragment and
  // verify that the 0.5 progress hasn't been reset to 1.0.
  context.SetUrl(GURL("https://www.test.com#fragment"));
  context.SetIsSameDocument(true);
  web_state().OnNavigationFinished(&context);
  EXPECT_EQ(0.5, model()->progress());
}

// Tests that the FullscreenModel is not reset for a same-document navigation.
TEST_F(FullscreenWebStateObserverTest, ResetForSameDocumentURLChange) {
  // Navigate to a URL.
  web::FakeNavigationContext context;
  context.SetUrl(GURL("https://www.test.com"));
  web_state().OnNavigationFinished(&context);
  model()->SetYContentOffset(0.0);
  // Simulate a scroll to 0.5 progress.
  SimulateFullscreenUserScrollForProgress(model(), 0.5);
  EXPECT_EQ(0.5, model()->progress());
  // Simulate a same-document navigation to a new URL and verify that the 0.5
  // progress is reset to 1.0.
  context.SetUrl(GURL("https://www.test2.com"));
  context.SetIsSameDocument(true);
  web_state().OnNavigationFinished(&context);
  EXPECT_EQ(1.0, model()->progress());
}
