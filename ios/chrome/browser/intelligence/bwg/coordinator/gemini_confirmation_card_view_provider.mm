// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_confirmation_card_view_provider.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_task_card_view.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_block_data_types.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_browser_actuation_confirmation_card.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/bwg/gemini_api.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Returns the confirmation card `block` carries, or nil if it carries none.
id<GeminiBrowserActuationConfirmationCard> CardForBlock(
    id<GeminiBlockNode> block) {
  if (![block conformsToProtocol:
                  @protocol(GeminiBrowserActuationConfirmationCardBlockNode)]) {
    return nil;
  }
  return static_cast<id<GeminiBrowserActuationConfirmationCardBlockNode>>(block)
      .browserActuationConfirmationCard;
}

}  // namespace

@interface GeminiConfirmationCardViewProvider () <ActuationTaskCardViewDelegate>
@end

@implementation GeminiConfirmationCardViewProvider

#pragma mark - GeminiCustomUIViewProvider

- (BOOL)providesViewForBlock:(id<GeminiBlockNode>)block {
  return CardForBlock(block) != nil;
}

- (BOOL)hidesBlock:(id<GeminiBlockNode>)block {
  return NO;
}

- (UIView*)viewForBlock:(id<GeminiBlockNode>)block
                context:(id<GeminiBlockContext>)context
            reusingView:(UIView*)view {
  ActuationTaskCardView* cardView =
      base::apple::ObjCCast<ActuationTaskCardView>(view);
  if (!cardView) {
    cardView = [[ActuationTaskCardView alloc]
        initWithTitle:l10n_util::GetNSString(
                          IDS_IOS_GEMINI_CONFIRMATION_CARD_TITLE)
          buttonTitle:l10n_util::GetNSString(
                          IDS_IOS_GEMINI_CONFIRMATION_CARD_BUTTON)
          collapsible:NO];
    cardView.accessibilityIdentifier =
        kGeminiConfirmationCardAccessibilityIdentifier;
  }
  cardView.delegate = self;
  cardView.subtitle = CardForBlock(block).task ?: @"";
  cardView.enabled = context.responseState == GeminiResponseState::kDone &&
                     context.latestResponse;
  return cardView;
}

#pragma mark - ActuationTaskCardViewDelegate

- (void)taskCardViewDidTapActionButton:(ActuationTaskCardView*)view {
  if (!view.enabled) {
    return;
  }
  view.enabled = NO;
  ios::provider::UpdatePromptAction(
      gemini::InputType::kText,
      l10n_util::GetNSString(IDS_IOS_GEMINI_CONFIRMATION_CARD_BUTTON),
      /*should_auto_submit=*/true);
}

- (void)taskCardView:(ActuationTaskCardView*)view
    didChangeCollapsedState:(BOOL)isCollapsed {
  // No-op.
}

@end
