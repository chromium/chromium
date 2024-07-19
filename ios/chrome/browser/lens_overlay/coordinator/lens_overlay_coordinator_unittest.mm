// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_coordinator.h"

#import "base/test/scoped_feature_list.h"
#import "components/lens/lens_overlay_permission_utils.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_tab_helper.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state_manager.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_overlay_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

@interface LensOverlayCoordinator ()
- (BOOL)isUICreated;
@end

namespace {

class LensOverlayCoordinatorTest : public PlatformTest {
 public:
  LensOverlayCoordinatorTest() {
    feature_list_.InitAndEnableFeature(kEnableLensOverlay);

    // Browser state
    browser_state_manager_ = std::make_unique<TestChromeBrowserStateManager>(
        TestChromeBrowserState::Builder().Build());
    TestingApplicationContext::GetGlobal()->SetChromeBrowserStateManager(
        browser_state_manager_.get());
    ChromeBrowserState* browser_state =
        browser_state_manager_->GetLastUsedBrowserStateForTesting();

    browser_ = std::make_unique<TestBrowser>(browser_state);
    dispatcher_ = [[CommandDispatcher alloc] init];

    GetApplicationContext()->GetLocalState()->SetInteger(
        lens::prefs::kLensOverlaySettings,
        static_cast<int>(
            lens::prefs::LensOverlaySettingsPolicyValue::kEnabled));

    base_view_controller_ = [[UIViewController alloc] init];

    // LensOverlayCoordinator
    coordinator_ = [[LensOverlayCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()];

    [dispatcher_ startDispatchingToTarget:coordinator_
                              forProtocol:@protocol(LensOverlayCommands)];

    // Tab helper
    web::WebState::CreateParams params(browser_state);
    web_state_ = web::WebState::Create(params);
    LensOverlayTabHelper::CreateForWebState(web_state_.get());
    SnapshotTabHelper::CreateForWebState(web_state_.get());
    tab_helper_ = LensOverlayTabHelper::FromWebState(web_state_.get());

    // Mark the only web state as active.
    browser_.get()->GetWebStateList()->InsertWebState(std::move(web_state_));
    browser_.get()->GetWebStateList()->ActivateWebStateAt(0);
  }

 protected:
  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::MainThreadType::IO};
  LensOverlayCoordinator* coordinator_;
  std::unique_ptr<TestChromeBrowserStateManager> browser_state_manager_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_;
  std::unique_ptr<web::WebState> web_state_;
  UIViewController* base_view_controller_;
  base::test::ScopedFeatureList feature_list_;
  id dispatcher_;
  LensOverlayTabHelper* tab_helper_;

  void DeliverMemoryWarningNotification() {
    [[NSNotificationCenter defaultCenter]
        postNotificationName:UIApplicationDidReceiveMemoryWarningNotification
                      object:nil];
  }
};

// When the UI is created but not shown, then the memory warning should destroy
// the UI.
TEST_F(LensOverlayCoordinatorTest,
       ShouldDestroyUIOnMemoryWarningWhenUIIsNotShown) {
  // Given a started `LensOverlayCoordinator`.
  [coordinator_ start];

  // When the coordinator is asked to create and show the UI.
  [HandlerForProtocol(dispatcher_, LensOverlayCommands) createAndShowLensUI:NO];

  // Then the UI should appear created.
  EXPECT_TRUE([coordinator_ isUICreated]);

  // Given a hidden lens overlay.
  tab_helper_->SetLensOverlayShown(false);

  // When UIKit delivers a low-memory warning notification.
  DeliverMemoryWarningNotification();

  // Then the UI should get destroyed.
  EXPECT_FALSE([coordinator_ isUICreated]);
}

// When the UI is created and visible to the user the memory warning should not
// destroy the UI.
TEST_F(LensOverlayCoordinatorTest,
       ShouldNotDestroyUIOnMemoryWarningWhenUIIsShown) {
  // Given a started `LensOverlayCoordinator`.
  [coordinator_ start];

  // When the coordinator is asked to create and show the UI.
  [HandlerForProtocol(dispatcher_, LensOverlayCommands) createAndShowLensUI:NO];

  // Then the UI should appear created and shown to the user.
  EXPECT_TRUE(tab_helper_->IsLensOverlayShown());
  EXPECT_TRUE([coordinator_ isUICreated]);

  // When UIKit delivers a low-memory warning notification.
  DeliverMemoryWarningNotification();

  // Then the UI should not be destroyed.
  EXPECT_TRUE([coordinator_ isUICreated]);
}

}  // namespace
