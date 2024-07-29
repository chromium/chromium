// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/model/lens_overlay_tab_helper.h"

#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "components/lens/lens_overlay_permission_utils.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state_manager.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_overlay_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

class LensOverlayTabHelperTest : public PlatformTest {
 public:
  LensOverlayTabHelperTest() {
    browser_state_ = browser_state_manager_.AddBrowserStateWithBuilder(
        TestChromeBrowserState::Builder());
  }

  void SetUp() override {
    PlatformTest::SetUp();

    feature_list_.InitAndEnableFeature(kEnableLensOverlay);

    GetApplicationContext()->GetLocalState()->SetInteger(
        lens::prefs::kLensOverlaySettings,
        static_cast<int>(
            lens::prefs::LensOverlaySettingsPolicyValue::kEnabled));

    web::WebState::CreateParams params(browser_state_.get());
    web_state_ = web::WebState::Create(params);

    id dispatcher = [[CommandDispatcher alloc] init];
    dispatcher_ = dispatcher;

    LensOverlayTabHelper::CreateForWebState(web_state_.get());
    helper_ = LensOverlayTabHelper::FromWebState(web_state_.get());
    ASSERT_TRUE(helper_);

    helper_->SetLensOverlayCommandsHandler(dispatcher);

    mock_commands_handler_ = OCMProtocolMock(@protocol(LensOverlayCommands));
    [dispatcher_ startDispatchingToTarget:mock_commands_handler_
                              forProtocol:@protocol(LensOverlayCommands)];
  }

 protected:
  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::MainThreadType::IO};
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestChromeBrowserStateManager browser_state_manager_;
  raw_ptr<ChromeBrowserState> browser_state_;
  std::unique_ptr<web::WebState> web_state_;
  raw_ptr<LensOverlayTabHelper> helper_ = nullptr;
  id handler_;
  id dispatcher_;
  id mock_commands_handler_;
  base::test::ScopedFeatureList feature_list_;
};

// Test that web state destruction causes lens UI to destroy with no animation.
TEST_F(LensOverlayTabHelperTest, ShouldDestroyUIOnWebStateDestruction) {
  helper_->SetLensOverlayShown(true);
  OCMExpect([mock_commands_handler_ destroyLensUI:NO]);
  web_state_.reset();
  EXPECT_OCMOCK_VERIFY(mock_commands_handler_);
}

// Test that destroyLensUI should not be called if the lens overlay is not
// shown.
TEST_F(LensOverlayTabHelperTest, ShouldNotDestroyUIOnWebStateDestruction) {
  [[mock_commands_handler_ reject] destroyLensUI:[OCMArg any]];
  helper_->SetLensOverlayShown(false);
  web_state_.reset();
  EXPECT_OCMOCK_VERIFY(mock_commands_handler_);
}

// Tests that a change in the web state is propagated to the commands handler.
TEST_F(LensOverlayTabHelperTest, ShouldShowTheUIWhenWebStateChanges) {
  // Given a shown lens overlay state.
  helper_->SetLensOverlayShown(true);
  // Then the Lens UI should be shown.
  OCMExpect([mock_commands_handler_ showLensUI:YES]);
  // When the tab helper is notify of a change in the web state.
  helper_->WasShown(web_state_.get());

  EXPECT_OCMOCK_VERIFY(mock_commands_handler_);
}

// Tests that a change in the web state is propagated to the commands handler.
TEST_F(LensOverlayTabHelperTest, ShouldHideTheUIWhenWebStateChanges) {
  // Given a shown lens overlay state.
  helper_->SetLensOverlayShown(true);
  // Then the Lens UI should be hidden.
  OCMExpect([mock_commands_handler_ hideLensUI:YES]);
  // When the tab helper is notify of a change in the web state.
  helper_->WasHidden(web_state_.get());

  EXPECT_OCMOCK_VERIFY(mock_commands_handler_);
}

// Tests that a change in the web state is not propagated to the commands
// handler when the lens overlay is not shown.
TEST_F(LensOverlayTabHelperTest, ShouldNotChangeUIStateWhenOverlayIsNotShown) {
  // Given a lens overlay not show.
  helper_->SetLensOverlayShown(false);
  // Then the Lens UI methods should not be called.
  OCMReject([mock_commands_handler_ showLensUI:YES]);
  OCMReject([mock_commands_handler_ hideLensUI:YES]);
  // When the tab helper is notify of a change in the web state.
  helper_->WasShown(web_state_.get());
  helper_->WasHidden(web_state_.get());

  EXPECT_OCMOCK_VERIFY(mock_commands_handler_);
}

}  // namespace
