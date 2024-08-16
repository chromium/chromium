// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_toolbar_consumer.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_overlay_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_coordinator.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

@interface FakeResultConsumer : NSObject <LensOverlayResultConsumer>
@property(nonatomic, assign) GURL lastPushedURL;
@end

@implementation FakeResultConsumer
- (void)loadResultsURL:(GURL)url {
  self.lastPushedURL = url;
}
@end

namespace {

class LensOverlayMediatorTest : public PlatformTest {
 public:
  LensOverlayMediatorTest() {
    mediator_ = [[LensOverlayMediator alloc] init];
    mock_result_consumer_ = [[FakeResultConsumer alloc] init];
    mock_omnibox_coordinator_ =
        [OCMockObject mockForClass:OmniboxCoordinator.class];
    mock_toolbar_consumer_ =
        [OCMockObject mockForProtocol:@protocol(LensToolbarConsumer)];
    fake_web_state_ = std::make_unique<web::FakeWebState>();

    mediator_.resultConsumer = mock_result_consumer_;
    mediator_.omniboxCoordinator = mock_omnibox_coordinator_;
    mediator_.toolbarConsumer = mock_toolbar_consumer_;
    mediator_.webState = fake_web_state_.get();
  }

  ~LensOverlayMediatorTest() override { [mediator_ disconnect]; }

 protected:
  LensOverlayMediator* mediator_;

  FakeResultConsumer* mock_result_consumer_;
  id mock_omnibox_coordinator_;
  std::unique_ptr<web::FakeWebState> fake_web_state_;
  OCMockObject<LensToolbarConsumer>* mock_toolbar_consumer_;
};

/// Tests that the omnibox and toolbar are updated on omnibox focus.
TEST_F(LensOverlayMediatorTest, FocusOmnibox) {
  // Focus from LensOmniboxMutator.
  OCMExpect([mock_omnibox_coordinator_ focusOmnibox]);
  OCMExpect([mock_toolbar_consumer_ setOmniboxFocused:YES]);
  [mediator_ focusOmnibox];
  EXPECT_OCMOCK_VERIFY(mock_omnibox_coordinator_);
  EXPECT_OCMOCK_VERIFY(mock_toolbar_consumer_);

  // Focus from OmniboxFocusDelegate.
  OCMExpect([mock_omnibox_coordinator_ focusOmnibox]);
  OCMExpect([mock_toolbar_consumer_ setOmniboxFocused:YES]);
  [mediator_ omniboxDidBecomeFirstResponder];
  EXPECT_OCMOCK_VERIFY(mock_omnibox_coordinator_);
  EXPECT_OCMOCK_VERIFY(mock_toolbar_consumer_);
}

/// Tests that the omnibox and toolbar are updated on omnibox defocus.
TEST_F(LensOverlayMediatorTest, DefocusOmnibox) {
  // Defocus from LensOmniboxMutator.
  OCMExpect([mock_omnibox_coordinator_ endEditing]);
  OCMExpect([mock_toolbar_consumer_ setOmniboxFocused:NO]);
  [mediator_ defocusOmnibox];
  EXPECT_OCMOCK_VERIFY(mock_omnibox_coordinator_);
  EXPECT_OCMOCK_VERIFY(mock_toolbar_consumer_);

  // Defocus from OmniboxFocusDelegate.
  OCMExpect([mock_omnibox_coordinator_ endEditing]);
  OCMExpect([mock_toolbar_consumer_ setOmniboxFocused:NO]);
  [mediator_ omniboxDidResignFirstResponder];
  EXPECT_OCMOCK_VERIFY(mock_omnibox_coordinator_);
  EXPECT_OCMOCK_VERIFY(mock_toolbar_consumer_);
}

// Tests that the omnibox is updated on navigation.
TEST_F(LensOverlayMediatorTest, UpdateOmniboxOnNavigation) {
  OCMExpect([mock_omnibox_coordinator_ updateOmniboxState]);
  fake_web_state_->OnNavigationFinished(nullptr);
  EXPECT_OCMOCK_VERIFY(mock_omnibox_coordinator_);
}

}  // namespace
