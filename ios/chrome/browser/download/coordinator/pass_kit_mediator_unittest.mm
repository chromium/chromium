// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/coordinator/pass_kit_mediator.h"

#import <memory>

#import "base/test/task_environment.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/commands/web_content_commands.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class PassKitMediatorTest : public PlatformTest {
 public:
  PassKitMediatorTest() {
    web_state_list_ = std::make_unique<WebStateList>(&web_state_list_delegate_);
    web_content_handler_mock_ =
        OCMStrictProtocolMock(@protocol(WebContentCommands));
    mediator_ = [[PassKitMediator alloc]
        initWithWebStateList:web_state_list_.get()
           webContentHandler:web_content_handler_mock_];
  }

  ~PassKitMediatorTest() override { [mediator_ disconnect]; }

 protected:
  base::test::TaskEnvironment task_environment_;
  FakeWebStateListDelegate web_state_list_delegate_;
  std::unique_ptr<WebStateList> web_state_list_;
  id web_content_handler_mock_;
  PassKitMediator* mediator_;
};

// Tests that PassKit dialog is dismissed when active WebState starts
// cross-document navigation.
TEST_F(PassKitMediatorTest, DismissOnNavigationStart) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebState* web_state_ptr = web_state.get();
  web_state_list_->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::Automatic().Activate());

  OCMExpect([web_content_handler_mock_ dismissPassKitDialog]);

  web::FakeNavigationContext context;
  context.SetIsSameDocument(false);
  web_state_ptr->OnNavigationStarted(&context);

  EXPECT_OCMOCK_VERIFY(web_content_handler_mock_);
}

// Tests that PassKit dialog is NOT dismissed when active WebState starts
// same-document navigation.
TEST_F(PassKitMediatorTest, DoNotDismissOnSameDocumentNavigationStart) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebState* web_state_ptr = web_state.get();
  web_state_list_->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::Automatic().Activate());

  // No expectation set on web_content_handler_mock_. Invocation would fail
  // strict mock.
  web::FakeNavigationContext context;
  context.SetIsSameDocument(true);
  web_state_ptr->OnNavigationStarted(&context);

  EXPECT_OCMOCK_VERIFY(web_content_handler_mock_);
}

// Tests that PassKit dialog is dismissed when active WebState finishes
// committed cross-document navigation.
TEST_F(PassKitMediatorTest, DismissOnNavigationFinish) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebState* web_state_ptr = web_state.get();
  web_state_list_->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::Automatic().Activate());

  OCMExpect([web_content_handler_mock_ dismissPassKitDialog]);

  web::FakeNavigationContext context;
  context.SetHasCommitted(true);
  context.SetIsSameDocument(false);
  web_state_ptr->OnNavigationFinished(&context);

  EXPECT_OCMOCK_VERIFY(web_content_handler_mock_);
}

// Tests that PassKit dialog is NOT dismissed when active WebState finishes
// same-document navigation.
TEST_F(PassKitMediatorTest, DoNotDismissOnSameDocumentNavigationFinish) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebState* web_state_ptr = web_state.get();
  web_state_list_->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::Automatic().Activate());

  // No expectation set on web_content_handler_mock_. Invocation would fail
  // strict mock.
  web::FakeNavigationContext context;
  context.SetHasCommitted(true);
  context.SetIsSameDocument(true);
  web_state_ptr->OnNavigationFinished(&context);

  EXPECT_OCMOCK_VERIFY(web_content_handler_mock_);
}

// Tests that PassKit dialog is NOT dismissed when active WebState finishes
// uncommitted navigation.
TEST_F(PassKitMediatorTest, DoNotDismissOnUncommittedNavigationFinish) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebState* web_state_ptr = web_state.get();
  web_state_list_->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::Automatic().Activate());

  // No expectation set on web_content_handler_mock_. Invocation would fail
  // strict mock.
  web::FakeNavigationContext context;
  context.SetHasCommitted(false);
  context.SetIsSameDocument(false);
  web_state_ptr->OnNavigationFinished(&context);

  EXPECT_OCMOCK_VERIFY(web_content_handler_mock_);
}

// Tests that PassKit dialog is dismissed when active WebState changes.
TEST_F(PassKitMediatorTest, DismissOnActiveWebStateChange) {
  auto web_state1 = std::make_unique<web::FakeWebState>();
  web_state_list_->InsertWebState(
      std::move(web_state1),
      WebStateList::InsertionParams::Automatic().Activate());

  OCMExpect([web_content_handler_mock_ dismissPassKitDialog]);

  auto web_state2 = std::make_unique<web::FakeWebState>();
  web_state_list_->InsertWebState(
      std::move(web_state2),
      WebStateList::InsertionParams::Automatic().Activate());

  EXPECT_OCMOCK_VERIFY(web_content_handler_mock_);
}

// Tests that PassKit dialog is dismissed when active WebState is destroyed,
// and does not crash even when dismissal triggers mediator disconnection.
TEST_F(PassKitMediatorTest, DismissOnWebStateDestroyed) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state_list_->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::Automatic().Activate());

  OCMExpect([web_content_handler_mock_ dismissPassKitDialog])
      .andDo(^(NSInvocation* invocation) {
        [mediator_ disconnect];
      });

  web_state_list_->CloseWebStateAt(0, WebStateList::ClosingReason::kDefault);

  EXPECT_OCMOCK_VERIFY(web_content_handler_mock_);
}

// Tests that navigation in a background WebState does NOT dismiss the PassKit
// dialog on the active WebState.
TEST_F(PassKitMediatorTest, BackgroundWebStateNavigationDoesNotDismiss) {
  auto active_web_state = std::make_unique<web::FakeWebState>();
  web_state_list_->InsertWebState(
      std::move(active_web_state),
      WebStateList::InsertionParams::Automatic().Activate());

  auto background_web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebState* background_web_state_ptr = background_web_state.get();
  web_state_list_->InsertWebState(std::move(background_web_state),
                                  WebStateList::InsertionParams::AtIndex(1));

  // No expectations on web_content_handler_mock_. Any invocation would fail
  // strict mock.
  web::FakeNavigationContext context;
  context.SetIsSameDocument(false);
  context.SetHasCommitted(true);
  background_web_state_ptr->OnNavigationStarted(&context);
  background_web_state_ptr->OnNavigationFinished(&context);

  EXPECT_OCMOCK_VERIFY(web_content_handler_mock_);
}
