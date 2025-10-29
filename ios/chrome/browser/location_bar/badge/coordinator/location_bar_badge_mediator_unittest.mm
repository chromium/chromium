// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/badge/coordinator/location_bar_badge_mediator.h"

#import <memory>

#import "base/test/task_environment.h"
#import "ios/chrome/browser/intelligence/bwg/utils/bwg_constants.h"
#import "ios/chrome/browser/location_bar/badge/coordinator/location_bar_badge_mediator_delegate.h"
#import "ios/chrome/browser/location_bar/badge/model/badge_type.h"
#import "ios/chrome/browser/location_bar/badge/model/location_bar_badge_configuration.h"
#import "ios/chrome/browser/location_bar/badge/ui/location_bar_badge_consumer.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {
NSString* kTestAccessibilityLabel = @"testBadge";
}

// Test fixture for LocationBarBadgeMediator.
class LocationBarBadgeMediatorTest : public PlatformTest {
 protected:
  LocationBarBadgeMediatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    web_state_list_ = browser_->GetWebStateList();
    std::unique_ptr<web::FakeWebState> web_state =
        std::make_unique<web::FakeWebState>();
    web_state_list_->InsertWebState(std::move(web_state),
                                    WebStateList::InsertionParams::AtIndex(0));
    CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
    mock_bwg_command_handler_ = OCMProtocolMock(@protocol(BWGCommands));
    mediator_ =
        [[LocationBarBadgeMediator alloc] initWithWebStateList:web_state_list_];
    mock_consumer_ = OCMProtocolMock(@protocol(LocationBarBadgeConsumer));
    mediator_.consumer = mock_consumer_;
    mock_delegate_ =
        OCMProtocolMock(@protocol(LocationBarBadgeMediatorDelegate));
    mediator_.delegate = mock_delegate_;
    [dispatcher startDispatchingToTarget:mock_bwg_command_handler_
                             forProtocol:@protocol(BWGCommands)];
    mediator_.BWGCommandHandler = mock_bwg_command_handler_;
  }

  ~LocationBarBadgeMediatorTest() override { [mediator_ disconnect]; }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  raw_ptr<WebStateList> web_state_list_;
  LocationBarBadgeMediator* mediator_;
  id mock_bwg_command_handler_;
  id mock_consumer_;
  id mock_delegate_;
};

// Tests that the consumer is updated when the Ask Gemini chip is tapped.
TEST_F(LocationBarBadgeMediatorTest, AskGeminiChipTapped) {
  OCMExpect([mock_bwg_command_handler_
      startBWGFlowWithEntryPoint:bwg::EntryPoint::OmniboxChip]);
  [mediator_ badgeTapped:LocationBarBadgeType::kAskGeminiChip];
  EXPECT_OCMOCK_VERIFY(mock_bwg_command_handler_);
}

// Tests that the consumer is updated when the badge configuration is updated.
TEST_F(LocationBarBadgeMediatorTest, UpdateBadgeConfig) {
  OCMExpect([mock_consumer_ setBadgeConfig:[OCMArg any]]);
  OCMExpect([mock_consumer_ collapseBadgeContainer]);
  OCMExpect([mock_consumer_ showBadge]);
  LocationBarBadgeConfiguration* config = [[LocationBarBadgeConfiguration alloc]
       initWithBadgeType:LocationBarBadgeType::kReaderMode
      accessibilityLabel:kTestAccessibilityLabel
              badgeImage:[[UIImage alloc] init]];
  [mediator_ updateBadgeConfig:config];
  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that the consumer is updated when the color is updated for IPH.
TEST_F(LocationBarBadgeMediatorTest, UpdateColorForIPH) {
  OCMExpect([mock_consumer_ highlightBadge:YES]);
  [mediator_ updateColorForIPH];
  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that the consumer is updated when the IPH is dismissed.
TEST_F(LocationBarBadgeMediatorTest, DismissIPHAnimated) {
  OCMExpect([mock_consumer_ highlightBadge:NO]);
  [mediator_ dismissIPHAnimated:YES];
  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that the delegate is notified when the label is centered.
TEST_F(LocationBarBadgeMediatorTest,
       SetLocationBarLabelCenteredBetweenContent) {
  OCMExpect([mock_delegate_ setLocationBarLabelCenteredBetweenContent:mediator_
                                                             centered:YES]);
  [mediator_ setLocationBarLabelCenteredBetweenContent:YES];
  EXPECT_OCMOCK_VERIFY(mock_delegate_);
}
