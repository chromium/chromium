// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/ui/actor_input_plate_view.h"

#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_constants.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

// Fake delegate recording the calls it receives.
@interface FakeActorInputPlateViewDelegate
    : NSObject <ActorInputPlateViewDelegate>
@property(nonatomic, assign) NSInteger sendCount;
@property(nonatomic, copy) NSString* lastText;
// The plate's `text` at the time of the last notification.
@property(nonatomic, copy) NSString* textWhenNotified;
@end

@implementation FakeActorInputPlateViewDelegate
- (void)actorInputPlateView:(ActorInputPlateView*)inputPlateView
                didSendText:(NSString*)text {
  _sendCount++;
  _lastText = text;
  _textWhenNotified = inputPlateView.text;
}
@end

namespace {

using chrome_test_util::FindViewById;

// Sendable and whitespace-only inputs. `kSendableText` is padded to check that
// the payload is trimmed to `kTrimmedText`.
NSString* const kSendableText = @"  Yes  ";
NSString* const kTrimmedText = @"Yes";
NSString* const kWhitespaceText = @" \n\t ";

class ActorInputPlateViewTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    view_ = [[ActorInputPlateView alloc] init];
    delegate_ = [[FakeActorInputPlateViewDelegate alloc] init];
    view_.delegate = delegate_;
  }

  // Returns the send button inside `view_`.
  UIButton* SendButton() {
    return FindViewById<UIButton>(
        view_, kActorInputPlateSendButtonAccessibilityIdentifier);
  }

  // Returns the text field inside `view_`.
  UITextField* TextField() {
    return FindViewById<UITextField>(
        view_, kActorInputPlateTextFieldAccessibilityIdentifier);
  }

  // Simulates the user typing `text`, replacing any existing text.
  void Type(NSString* text) {
    UITextField* text_field = TextField();
    text_field.text = text;
    [text_field sendActionsForControlEvents:UIControlEventEditingChanged];
  }

  ActorInputPlateView* view_ = nil;
  FakeActorInputPlateViewDelegate* delegate_ = nil;
};

// Tests that the send button is enabled only for non-whitespace text.
TEST_F(ActorInputPlateViewTest, SendEnabledOnlyForNonWhitespaceText) {
  ASSERT_NE(SendButton(), nil);
  EXPECT_FALSE(SendButton().enabled);
  Type(kWhitespaceText);
  EXPECT_FALSE(SendButton().enabled);
  Type(kSendableText);
  EXPECT_TRUE(SendButton().enabled);
}

// Tests that tapping send clears the text, then notifies the delegate with the
// trimmed text.
TEST_F(ActorInputPlateViewTest, SendTapClearsTextAndNotifiesDelegate) {
  Type(kSendableText);
  [SendButton() sendActionsForControlEvents:UIControlEventTouchUpInside];

  EXPECT_EQ(delegate_.sendCount, 1);
  EXPECT_NSEQ(delegate_.lastText, kTrimmedText);
  EXPECT_NSEQ(delegate_.textWhenNotified, @"");
  EXPECT_FALSE(SendButton().enabled);
}

// Tests that the return key sends only non-whitespace text.
TEST_F(ActorInputPlateViewTest, ReturnKeySendsOnlyNonWhitespaceText) {
  UITextField* text_field = TextField();
  Type(kWhitespaceText);
  [text_field sendActionsForControlEvents:UIControlEventPrimaryActionTriggered];
  EXPECT_EQ(delegate_.sendCount, 0);

  Type(kSendableText);
  [text_field sendActionsForControlEvents:UIControlEventPrimaryActionTriggered];
  EXPECT_EQ(delegate_.sendCount, 1);
  EXPECT_NSEQ(delegate_.lastText, kTrimmedText);
}

}  // namespace
