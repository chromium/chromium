// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/text_zoom/ui_bundled/text_zoom_mediator.h"

#import <memory>

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/text_zoom_commands.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class TextZoomMediatorTest : public PlatformTest {
 protected:
  TextZoomMediatorTest()
      : web_state_list_(&web_state_list_delegate_),
        mock_text_zoom_handler_(
            OCMStrictProtocolMock(@protocol(TextZoomCommands))) {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state_ = web_state.get();
    web_state_list_.InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate());
    mediator_ =
        [[TextZoomMediator alloc] initWithWebStateList:&web_state_list_
                                        commandHandler:mock_text_zoom_handler_
                                    distilledPagePrefs:nullptr];
  }

  void TearDown() override {
    [mediator_ disconnect];
    mediator_ = nil;
    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  FakeWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
  raw_ptr<web::FakeWebState> web_state_;
  OCMockObject<TextZoomCommands>* mock_text_zoom_handler_;
  TextZoomMediator* mediator_;
};

// Tests that an uncommitted navigation does not close the text zoom UI.
TEST_F(TextZoomMediatorTest, UncommittedNavigationDoesNotCloseTextZoom) {
  web::FakeNavigationContext context;
  context.SetHasCommitted(false);
  context.SetIsSameDocument(false);
  web_state_->OnNavigationFinished(&context);
  EXPECT_OCMOCK_VERIFY(mock_text_zoom_handler_);
}

// Tests that a committed navigation closes the text zoom UI.
TEST_F(TextZoomMediatorTest, CommittedNavigationClosesTextZoom) {
  OCMExpect([mock_text_zoom_handler_ closeTextZoom]);
  web::FakeNavigationContext context;
  context.SetHasCommitted(true);
  context.SetIsSameDocument(false);
  web_state_->OnNavigationFinished(&context);
  EXPECT_OCMOCK_VERIFY(mock_text_zoom_handler_);
}

// Tests that a committed same-document navigation closes the text zoom UI.
TEST_F(TextZoomMediatorTest, CommittedSameDocumentNavigationClosesTextZoom) {
  OCMExpect([mock_text_zoom_handler_ closeTextZoom]);
  web::FakeNavigationContext context;
  context.SetHasCommitted(true);
  context.SetIsSameDocument(true);
  web_state_->OnNavigationFinished(&context);
  EXPECT_OCMOCK_VERIFY(mock_text_zoom_handler_);
}
