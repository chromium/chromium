// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens/model/lens_tab_helper.h"

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_lens_input_selection_command.h"
#import "ios/chrome/browser/ui/lens/lens_entrypoint.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

class LensTabHelperTest : public PlatformTest {
 public:
  LensTabHelperTest() {
    profile_ = TestProfileIOS::Builder().Build();

    web::WebState::CreateParams params(profile_.get());
    web_state_ = web::WebState::Create(params);

    LensTabHelper::CreateForWebState(web_state_.get());
  }

  void SetUp() override {
    PlatformTest::SetUp();

    id dispatcher = [[CommandDispatcher alloc] init];
    dispatcher_ = dispatcher;

    helper_ = LensTabHelper::FromWebState(web_state_.get());
    ASSERT_TRUE(helper_);

    helper_->SetLensCommandsHandler(dispatcher);
  }

 protected:
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::WebState> web_state_;
  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::MainThreadType::IO};
  raw_ptr<LensTabHelper> helper_ = nullptr;
  id handler_;
  id dispatcher_;
};

// Test `ShouldAllowRequest` for the web search bar.
TEST_F(LensTabHelperTest, ShouldAllowRequest_WebSearchBar) {
  NSURLRequest* request = [NSURLRequest
      requestWithURL:
          [NSURL URLWithString:@"googlechromeaction://lens/web-search-box"]];
  const web::WebStatePolicyDecider::RequestInfo request_info(
      ui::PageTransition::PAGE_TRANSITION_LINK,
      /*target_frame_is_main=*/true,
      /*target_frame_is_cross_origin=*/false,
      /*target_window_is_cross_origin=*/false,
      /*is_user_initiated=*/false, /*user_tapped_recently=*/false);
  __block bool callback_called = false;
  __block web::WebStatePolicyDecider::PolicyDecision request_policy =
      web::WebStatePolicyDecider::PolicyDecision::Allow();
  auto callback =
      base::BindOnce(^(web::WebStatePolicyDecider::PolicyDecision decision) {
        request_policy = decision;
        callback_called = true;
      });

  id mock_lens_commands_handler = OCMProtocolMock(@protocol(LensCommands));
  [dispatcher_ startDispatchingToTarget:mock_lens_commands_handler
                            forProtocol:@protocol(LensCommands)];

  OCMExpect([mock_lens_commands_handler
      openLensInputSelection:[OCMArg
                                 checkWithBlock:^(
                                     OpenLensInputSelectionCommand* command) {
                                   return command.entryPoint ==
                                          LensEntrypoint::WebSearchBar;
                                 }]]);

  helper_->ShouldAllowRequest(request, request_info, std::move(callback));

  EXPECT_OCMOCK_VERIFY(mock_lens_commands_handler);
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(request_policy.ShouldCancelNavigation());
}

// Test `ShouldAllowRequest` for the translate onebox.
TEST_F(LensTabHelperTest, ShouldAllowRequest_TranslateOnebox) {
  NSURLRequest* request = [NSURLRequest
      requestWithURL:
          [NSURL URLWithString:@"googlechromeaction://lens/translate-onebox"]];
  const web::WebStatePolicyDecider::RequestInfo request_info(
      ui::PageTransition::PAGE_TRANSITION_LINK,
      /*target_frame_is_main=*/true,
      /*target_frame_is_cross_origin=*/false,
      /*target_window_is_cross_origin=*/false,
      /*is_user_initiated=*/false, /*user_tapped_recently=*/false);
  __block bool callback_called = false;
  __block web::WebStatePolicyDecider::PolicyDecision request_policy =
      web::WebStatePolicyDecider::PolicyDecision::Allow();
  auto callback =
      base::BindOnce(^(web::WebStatePolicyDecider::PolicyDecision decision) {
        request_policy = decision;
        callback_called = true;
      });

  id mock_lens_commands_handler = OCMProtocolMock(@protocol(LensCommands));
  [dispatcher_ startDispatchingToTarget:mock_lens_commands_handler
                            forProtocol:@protocol(LensCommands)];

  OCMExpect([mock_lens_commands_handler
      openLensInputSelection:[OCMArg
                                 checkWithBlock:^(
                                     OpenLensInputSelectionCommand* command) {
                                   return command.entryPoint ==
                                          LensEntrypoint::TranslateOnebox;
                                 }]]);

  helper_->ShouldAllowRequest(request, request_info, std::move(callback));

  EXPECT_OCMOCK_VERIFY(mock_lens_commands_handler);
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(request_policy.ShouldCancelNavigation());
}

// Test `ShouldAllowRequest` for the web images search bar.
TEST_F(LensTabHelperTest, ShouldAllowRequest_WebImagesSearchBar) {
  NSURLRequest* request = [NSURLRequest
      requestWithURL:[NSURL URLWithString:@"googlechromeaction://lens/"
                                          @"web-images-search-box"]];
  const web::WebStatePolicyDecider::RequestInfo request_info(
      ui::PageTransition::PAGE_TRANSITION_LINK,
      /*target_frame_is_main=*/true,
      /*target_frame_is_cross_origin=*/false,
      /*target_window_is_cross_origin=*/false,
      /*is_user_initiated=*/false, /*user_tapped_recently=*/false);
  __block bool callback_called = false;
  __block web::WebStatePolicyDecider::PolicyDecision request_policy =
      web::WebStatePolicyDecider::PolicyDecision::Allow();
  auto callback =
      base::BindOnce(^(web::WebStatePolicyDecider::PolicyDecision decision) {
        request_policy = decision;
        callback_called = true;
      });

  id mock_lens_commands_handler = OCMProtocolMock(@protocol(LensCommands));
  [dispatcher_ startDispatchingToTarget:mock_lens_commands_handler
                            forProtocol:@protocol(LensCommands)];

  OCMExpect([mock_lens_commands_handler
      openLensInputSelection:[OCMArg
                                 checkWithBlock:^(
                                     OpenLensInputSelectionCommand* command) {
                                   return command.entryPoint ==
                                          LensEntrypoint::WebImagesSearchBar;
                                 }]]);

  helper_->ShouldAllowRequest(request, request_info, std::move(callback));

  EXPECT_OCMOCK_VERIFY(mock_lens_commands_handler);
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(request_policy.ShouldCancelNavigation());
}

// Test `EntryPointForGoogleChromeActionURLPath` for the web search bar.
TEST_F(LensTabHelperTest, EntryPointForGoogleChromeActionURLPath_WebSearchBar) {
  std::optional<LensEntrypoint> entry_point =
      LensTabHelper::EntryPointForGoogleChromeActionURLPath(@"/web-search-box");
  ASSERT_TRUE(entry_point);
  EXPECT_EQ(entry_point.value(), LensEntrypoint::WebSearchBar);
}

// Test `EntryPointForGoogleChromeActionURLPath` for the translate onebox.
TEST_F(LensTabHelperTest,
       EntryPointForGoogleChromeActionURLPath_TranslateOnebox) {
  std::optional<LensEntrypoint> entry_point =
      LensTabHelper::EntryPointForGoogleChromeActionURLPath(
          @"/translate-onebox");
  ASSERT_TRUE(entry_point);
  EXPECT_EQ(entry_point.value(), LensEntrypoint::TranslateOnebox);
}

// Test `EntryPointForGoogleChromeActionURLPath` for the web images search bar.
TEST_F(LensTabHelperTest,
       EntryPointForGoogleChromeActionURLPath_WebImagesSearchBar) {
  std::optional<LensEntrypoint> entry_point =
      LensTabHelper::EntryPointForGoogleChromeActionURLPath(
          @"/web-images-search-box");
  ASSERT_TRUE(entry_point);
  EXPECT_EQ(entry_point.value(), LensEntrypoint::WebImagesSearchBar);
}

// Test `EntryPointForGoogleChromeActionURLPath` for an invalid entry point.
TEST_F(LensTabHelperTest, EntryPointForGoogleChromeActionURLPath_Invalid) {
  std::optional<LensEntrypoint> entry_point =
      LensTabHelper::EntryPointForGoogleChromeActionURLPath(@"/invalid");
  EXPECT_FALSE(entry_point);
}

}  // namespace
