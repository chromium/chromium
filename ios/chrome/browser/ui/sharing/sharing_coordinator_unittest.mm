// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sharing/sharing_coordinator.h"

#import <MaterialComponents/MaterialSnackbar.h>
#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/values.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "components/bookmarks/test/bookmark_test_helpers.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_ios_unit_test_support.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/commands/bookmarks_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/generate_qr_code_command.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/qr_generation_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/ui/sharing/activity_services/activity_service_presentation.h"
#import "ios/chrome/browser/ui/sharing/activity_services/canonical_url_retriever.h"
#import "ios/chrome/browser/ui/sharing/sharing_params.h"
#import "ios/chrome/browser/ui/sharing/sharing_positioner.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"

using base::test::ios::kWaitForActionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;
using bookmarks::BookmarkNode;

// Test fixture for testing SharingCoordinator.
class SharingCoordinatorTest : public BookmarkIOSUnitTestSupport {
 protected:
  SharingCoordinatorTest()
      : base_view_controller_([[UIViewController alloc] init]),
        fake_origin_view_([[UIView alloc] init]),
        test_scenario_(SharingScenario::TabShareButton) {
    [scoped_key_window_.Get() setRootViewController:base_view_controller_];
  }

  void SetUp() override {
    BookmarkIOSUnitTestSupport::SetUp();
    snackbar_handler_ = OCMStrictProtocolMock(@protocol(SnackbarCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:snackbar_handler_
                     forProtocol:@protocol(SnackbarCommands)];

    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:OCMStrictProtocolMock(
                                     @protocol(BookmarksCommands))
                     forProtocol:@protocol(BookmarksCommands)];
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:OCMStrictProtocolMock(@protocol(HelpCommands))
                     forProtocol:@protocol(HelpCommands)];
  }

  void AppendNewWebState(std::unique_ptr<web::FakeWebState> web_state) {
    browser_->GetWebStateList()->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate());
  }

  ScopedKeyWindow scoped_key_window_;
  UIViewController* base_view_controller_;
  UIView* fake_origin_view_;
  id snackbar_handler_;
  SharingScenario test_scenario_;
};

// Tests that the start method shares the current page and ends up presenting
// a UIActivityViewController.
TEST_F(SharingCoordinatorTest, Start_ShareCurrentPage) {
  // Create a test web state.
  GURL test_url = GURL("https://example.com");
  base::Value url_value = base::Value(test_url.spec());
  auto test_web_state = std::make_unique<web::FakeWebState>();
  test_web_state->SetNavigationManager(
      std::make_unique<web::FakeNavigationManager>());
  test_web_state->SetCurrentURL(test_url);
  test_web_state->SetBrowserState(browser_->GetProfile());

  auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
  web::FakeWebFramesManager* frames_manager_ptr = frames_manager.get();
  test_web_state->SetWebFramesManager(std::move(frames_manager));

  auto main_frame = web::FakeWebFrame::CreateMainWebFrame(test_url);
  web::FakeWebFrame* main_frame_ptr = main_frame.get();
  frames_manager_ptr->AddWebFrame(std::move(main_frame));

  main_frame_ptr->AddResultForExecutedJs(
      &url_value, activity_services::kCanonicalURLScript);

  AppendNewWebState(std::move(test_web_state));

  SharingParams* params =
      [[SharingParams alloc] initWithScenario:test_scenario_];

  SharingCoordinator* coordinator = [[SharingCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()
                          params:params
                      originView:fake_origin_view_];

  __block bool completion_handler_called = false;
  id vc_partial_mock = OCMPartialMock(base_view_controller_);
  [[vc_partial_mock expect]
      presentViewController:[OCMArg checkWithBlock:^BOOL(
                                        UIViewController* viewController) {
        if ([viewController isKindOfClass:[UIActivityViewController class]]) {
          completion_handler_called = true;
          return YES;
        }
        return NO;
      }]
                   animated:YES
                 completion:nil];

  [coordinator start];

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return completion_handler_called;
  }));

  // Verify that the positioning is correct.
  auto activityHandler =
      static_cast<id<SharingPositioner, ActivityServicePresentation>>(
          coordinator);
  EXPECT_EQ(fake_origin_view_, activityHandler.sourceView);
  EXPECT_TRUE(
      CGRectEqualToRect(fake_origin_view_.bounds, activityHandler.sourceRect));

  [activityHandler activityServiceDidEndPresenting];

  [vc_partial_mock verify];
  [coordinator stop];
}

// Tests that the coordinator handles the QRGenerationCommands protocol.
TEST_F(SharingCoordinatorTest, GenerateQRCode) {
  SharingParams* params =
      [[SharingParams alloc] initWithScenario:test_scenario_];
  SharingCoordinator* coordinator = [[SharingCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()
                          params:params
                      originView:fake_origin_view_];

  id vc_partial_mock = OCMPartialMock(base_view_controller_);
  [[vc_partial_mock expect] presentViewController:[OCMArg any]
                                         animated:YES
                                       completion:nil];

  auto handler = static_cast<id<QRGenerationCommands>>(coordinator);
  [handler generateQRCode:[[GenerateQRCodeCommand alloc]
                              initWithURL:GURL("https://example.com")
                                    title:@"Some Title"]];

  [vc_partial_mock verify];

  [[vc_partial_mock expect] dismissViewControllerAnimated:YES completion:nil];

  [handler hideQRCode];

  [vc_partial_mock verify];
  [coordinator stop];
}

// Tests that the start method shares the given URL and ends up presenting
// a UIActivityViewController.
TEST_F(SharingCoordinatorTest, Start_ShareURL) {
  GURL testURL = GURL("https://example.com");
  NSString* testTitle = @"Some title";
  SharingParams* params = [[SharingParams alloc] initWithURL:testURL
                                                       title:testTitle
                                                    scenario:test_scenario_];
  SharingCoordinator* coordinator = [[SharingCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()
                          params:params
                      originView:fake_origin_view_];

  id vc_partial_mock = OCMPartialMock(base_view_controller_);
  [[vc_partial_mock expect]
      presentViewController:[OCMArg checkWithBlock:^BOOL(
                                        UIViewController* viewController) {
        if ([viewController isKindOfClass:[UIActivityViewController class]]) {
          return YES;
        }
        return NO;
      }]
                   animated:YES
                 completion:nil];

  [coordinator start];

  [vc_partial_mock verify];

  // Make sure share sheet finishes it's init (which means calling
  // canPerformWithActivityItems and reading prefs) before the
  // WebTaskEnvironment is shut down.
  base::RunLoop().RunUntilIdle();
  [coordinator stop];
}
