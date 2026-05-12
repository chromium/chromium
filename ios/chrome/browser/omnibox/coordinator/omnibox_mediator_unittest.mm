// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/coordinator/omnibox_mediator.h"

#import "base/test/task_environment.h"
#import "components/open_from_clipboard/fake_clipboard_recent_content.h"
#import "ios/chrome/browser/omnibox/model/omnibox_autocomplete_controller.h"
#import "ios/chrome/browser/omnibox/model/omnibox_text_controller.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_text_input.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

class OmniboxMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    ClipboardRecentContent::SetInstance(
        std::make_unique<FakeClipboardRecentContent>());

    mediator_ = [[OmniboxMediator alloc]
          initWithIncognito:NO
                    tracker:nullptr
        presentationContext:OmniboxPresentationContext::kLocationBar];

    mock_text_controller_ = OCMClassMock([OmniboxTextController class]);
    mock_text_input_ = OCMProtocolMock(@protocol(OmniboxTextInput));
    mock_autocomplete_controller_ =
        OCMClassMock([OmniboxAutocompleteController class]);

    OCMStub([mock_text_controller_ textInput]).andReturn(mock_text_input_);
    OCMStub([mock_text_controller_ omniboxAutocompleteController])
        .andReturn(mock_autocomplete_controller_);

    mediator_.omniboxTextController = mock_text_controller_;
  }

  void TearDown() override {
    [mediator_ disconnect];
    mediator_ = nil;
    mock_text_controller_ = nil;
    mock_text_input_ = nil;
    mock_autocomplete_controller_ = nil;
    ClipboardRecentContent::SetInstance(nullptr);
    PlatformTest::TearDown();
  }

  base::test::TaskEnvironment task_environment_;
  OmniboxMediator* mediator_;
  id mock_text_controller_;
  id mock_text_input_;
  id mock_autocomplete_controller_;
};

// Tests that suggestions are not reloaded on foreground if the omnibox is not
// being edited.
TEST_F(OmniboxMediatorTest, DoesNotReloadSuggestionsWhenNotEditing) {
  OCMStub([mock_text_input_ isEditing]).andReturn(NO);
  OCMStub([mock_text_input_ text]).andReturn(@"");

  __block BOOL clear_called = NO;
  OCMStub([mock_autocomplete_controller_ clearAndRestartAutocomplete])
      .andDo(^(NSInvocation* invocation) {
        clear_called = YES;
      });

  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIApplicationDidBecomeActiveNotification
                    object:nil];

  EXPECT_FALSE(clear_called);
}

// Tests that suggestions are not reloaded on foreground if the omnibox contains
// text.
TEST_F(OmniboxMediatorTest, DoesNotReloadSuggestionsWhenHasText) {
  OCMStub([mock_text_input_ isEditing]).andReturn(YES);
  OCMStub([mock_text_input_ text]).andReturn(@"http://chromium.org");

  __block BOOL clear_called = NO;
  OCMStub([mock_autocomplete_controller_ clearAndRestartAutocomplete])
      .andDo(^(NSInvocation* invocation) {
        clear_called = YES;
      });

  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIApplicationDidBecomeActiveNotification
                    object:nil];

  EXPECT_FALSE(clear_called);
}

// Tests that suggestions are reloaded on foreground if the omnibox is being
// edited, is empty, and clipboard content changed.
TEST_F(OmniboxMediatorTest, ReloadsSuggestionsWhenEditingAndEmpty) {
  OCMStub([mock_text_input_ isEditing]).andReturn(YES);
  OCMStub([mock_text_input_ text]).andReturn(@"");

  __block BOOL clear_called = NO;
  OCMStub([mock_autocomplete_controller_ clearAndRestartAutocomplete])
      .andDo(^(NSInvocation* invocation) {
        clear_called = YES;
      });

  // Set clipboard content so checkClipboardContent detects a change.
  FakeClipboardRecentContent* fake_clipboard =
      static_cast<FakeClipboardRecentContent*>(
          ClipboardRecentContent::GetInstance());
  fake_clipboard->SetClipboardURL(GURL("http://example.com"),
                                  base::Minutes(1));

  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIApplicationDidBecomeActiveNotification
                    object:nil];

  EXPECT_TRUE(clear_called);
}

}  // namespace
