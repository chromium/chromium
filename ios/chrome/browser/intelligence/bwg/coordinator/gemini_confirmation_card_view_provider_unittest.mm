// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_confirmation_card_view_provider.h"

#import <UIKit/UIKit.h>

#import <optional>

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_task_card_view.h"
#import "ios/chrome/browser/intelligence/bwg/model/fake_gemini_block_data_types.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_block_data_types.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "ios/public/provider/chrome/browser/bwg/gemini_api.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace ios::provider {
std::optional<gemini::InputType> GetLastUpdatePromptActionInputType();
NSString* GetLastUpdatePromptActionPrompt();
BOOL GetLastUpdatePromptActionShouldAutoSubmit();
}  // namespace ios::provider

// Block node of a type Chrome does not render.
@interface FakeGeminiOtherBlockNode : NSObject <GeminiBlockNode>
@end

@implementation FakeGeminiOtherBlockNode
@end

namespace {

NSString* const kTask = @"Book a table for two";

class GeminiConfirmationCardViewProviderTest : public PlatformTest {
 protected:
  GeminiConfirmationCardViewProviderTest() {
    ios::provider::ResetGemini();
    provider_ = [[GeminiConfirmationCardViewProvider alloc] init];
    block_ = MakeCardBlock();
    context_ = [[FakeGeminiBlockContext alloc] init];
  }

  ~GeminiConfirmationCardViewProviderTest() override {
    ios::provider::ResetGemini();
  }

  // Returns a block node carrying a confirmation card for `kTask`.
  FakeGeminiBlockNode* MakeCardBlock() {
    FakeGeminiBlockNode* block = [[FakeGeminiBlockNode alloc] init];
    block.browserActuationConfirmationCard =
        [[FakeGeminiBrowserActuationConfirmationCard alloc] init];
    block.browserActuationConfirmationCard.task = kTask;
    return block;
  }

  // Requests a card view for `block_` in `context_`, reusing `view`.
  ActuationTaskCardView* RequestCardView(UIView* view) {
    return base::apple::ObjCCastStrict<ActuationTaskCardView>(
        [provider_ viewForBlock:block_ context:context_ reusingView:view]);
  }

  // Taps the action button of `card_view`.
  void TapActionButton(ActuationTaskCardView* card_view) {
    [chrome_test_util::FindViewById<UIButton>(
        card_view, kActuationTaskCardActionButtonAccessibilityIdentifier)
        sendActionsForControlEvents:UIControlEventTouchUpInside];
  }

  GeminiConfirmationCardViewProvider* provider_;
  FakeGeminiBlockNode* block_;
  FakeGeminiBlockContext* context_;
};

// Tests that only blocks carrying a confirmation card are rendered, and none
// are hidden.
TEST_F(GeminiConfirmationCardViewProviderTest, ProvidesViewsForCardBlocksOnly) {
  EXPECT_TRUE([provider_ providesViewForBlock:block_]);
  EXPECT_FALSE([provider_ hidesBlock:block_]);

  FakeGeminiBlockNode* cardless_block = [[FakeGeminiBlockNode alloc] init];
  EXPECT_FALSE([provider_ providesViewForBlock:cardless_block]);
  EXPECT_FALSE([provider_ hidesBlock:cardless_block]);

  FakeGeminiOtherBlockNode* other_block =
      [[FakeGeminiOtherBlockNode alloc] init];
  EXPECT_FALSE([provider_ providesViewForBlock:other_block]);
  EXPECT_FALSE([provider_ hidesBlock:other_block]);
}

// Tests that a completed, latest response yields an enabled card populated with
// the task, and that non-card views passed for reuse are replaced.
TEST_F(GeminiConfirmationCardViewProviderTest,
       CreatesEnabledCardForDoneResponse) {
  UIView* foreign_view = [[UIView alloc] init];
  ActuationTaskCardView* card_view = RequestCardView(foreign_view);
  ASSERT_TRUE(card_view);
  EXPECT_NE(foreign_view, card_view);
  EXPECT_NSEQ(kGeminiConfirmationCardAccessibilityIdentifier,
              card_view.accessibilityIdentifier);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_GEMINI_CONFIRMATION_CARD_TITLE),
              card_view.title);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_GEMINI_CONFIRMATION_CARD_BUTTON),
              card_view.buttonTitle);
  EXPECT_NSEQ(kTask, card_view.subtitle);
  EXPECT_FALSE(card_view.collapsible);
  EXPECT_FALSE(card_view.collapsed);
  EXPECT_TRUE(card_view.enabled);
  EXPECT_EQ(nil, ios::provider::GetLastUpdatePromptActionPrompt());
}

// Tests that the reusable view is updated in place across streaming,
// completion, a newer response, and non-completed terminal states.
TEST_F(GeminiConfirmationCardViewProviderTest,
       UpdatesStateAndReusesViewInPlace) {
  block_.browserActuationConfirmationCard.task = nil;
  context_.responseState = GeminiResponseState::kStreaming;
  ActuationTaskCardView* card_view = RequestCardView(nil);
  EXPECT_NSEQ(@"", card_view.subtitle);
  EXPECT_FALSE(card_view.enabled);

  block_.browserActuationConfirmationCard.task = kTask;
  EXPECT_EQ(card_view, RequestCardView(card_view));
  EXPECT_NSEQ(kTask, card_view.subtitle);
  EXPECT_FALSE(card_view.enabled);

  context_.responseState = GeminiResponseState::kDone;
  EXPECT_EQ(card_view, RequestCardView(card_view));
  EXPECT_TRUE(card_view.enabled);

  context_.latestResponse = NO;
  EXPECT_EQ(card_view, RequestCardView(card_view));
  EXPECT_FALSE(card_view.enabled);

  context_.latestResponse = YES;
  context_.responseState = GeminiResponseState::kStopped;
  EXPECT_FALSE(RequestCardView(card_view).enabled);
  context_.responseState = GeminiResponseState::kFailed;
  EXPECT_FALSE(RequestCardView(card_view).enabled);
}

// Tests that tapping a disabled card sends nothing, tapping once enabled sends
// the confirmation prompt once even when tapped again right away, and the card
// stays disabled once its response is superseded by the reply.
TEST_F(GeminiConfirmationCardViewProviderTest, TapStartsTaskOnceWhenEnabled) {
  context_.responseState = GeminiResponseState::kStreaming;
  ActuationTaskCardView* card_view = RequestCardView(nil);
  TapActionButton(card_view);
  EXPECT_EQ(nil, ios::provider::GetLastUpdatePromptActionPrompt());

  context_.responseState = GeminiResponseState::kDone;
  ASSERT_TRUE(RequestCardView(card_view).enabled);
  TapActionButton(card_view);
  EXPECT_FALSE(card_view.enabled);
  EXPECT_THAT(ios::provider::GetLastUpdatePromptActionInputType(),
              testing::Optional(gemini::InputType::kText));
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_GEMINI_CONFIRMATION_CARD_BUTTON),
              ios::provider::GetLastUpdatePromptActionPrompt());
  EXPECT_TRUE(ios::provider::GetLastUpdatePromptActionShouldAutoSubmit());

  ios::provider::ResetGemini();
  TapActionButton(card_view);
  EXPECT_EQ(nil, ios::provider::GetLastUpdatePromptActionPrompt());

  context_.latestResponse = NO;
  EXPECT_FALSE(RequestCardView(card_view).enabled);
  TapActionButton(card_view);
  EXPECT_EQ(nil, ios::provider::GetLastUpdatePromptActionPrompt());
}

// Tests that starting a task on one card does not disable another block's card,
// and that a new provider takes over an earlier provider's card on update.
TEST_F(GeminiConfirmationCardViewProviderTest,
       SentStateIsPerCardAndTakenOverByNewProvider) {
  ActuationTaskCardView* first_card = RequestCardView(nil);
  ActuationTaskCardView* second_card =
      base::apple::ObjCCastStrict<ActuationTaskCardView>([provider_
          viewForBlock:MakeCardBlock()
               context:context_
           reusingView:nil]);
  EXPECT_NE(first_card, second_card);

  TapActionButton(first_card);
  EXPECT_TRUE(second_card.enabled);

  ios::provider::ResetGemini();
  provider_ = [[GeminiConfirmationCardViewProvider alloc] init];
  EXPECT_EQ(second_card, RequestCardView(second_card));
  TapActionButton(second_card);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_GEMINI_CONFIRMATION_CARD_BUTTON),
              ios::provider::GetLastUpdatePromptActionPrompt());
}

}  // namespace
