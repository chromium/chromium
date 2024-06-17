// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/model/lens_overlay_tab_helper.h"

#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_overlay_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

class LensOverlayTabHelperTest : public PlatformTest {
 public:
  LensOverlayTabHelperTest() {
    browser_state_ = TestChromeBrowserState::Builder().Build();
  }

  void SetUp() override {
    PlatformTest::SetUp();

    feature_list_.InitAndEnableFeature(kEnableLensOverlay);

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
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<web::WebState> web_state_;
  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::MainThreadType::IO};
  raw_ptr<LensOverlayTabHelper> helper_ = nullptr;
  id handler_;
  id dispatcher_;
  id mock_commands_handler_;
  base::test::ScopedFeatureList feature_list_;
};

// Test that web state destruction causes lens UI to destroy with no animation.
TEST_F(LensOverlayTabHelperTest, ShouldDestroyUIOnWebStateDestrictuion) {
  OCMExpect([mock_commands_handler_ destroyLensUI:NO]);
  web_state_.reset();
  EXPECT_OCMOCK_VERIFY(mock_commands_handler_);
}

}  // namespace
