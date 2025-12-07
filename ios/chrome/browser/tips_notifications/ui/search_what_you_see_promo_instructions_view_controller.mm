// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tips_notifications/ui/search_what_you_see_promo_instructions_view_controller.h"

#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// AccessibilityIdentifier for the Search What You See Promo Instructions
// view.
NSString* const kSearchWhatYouSeePromoInstructionsAXID =
    @"kSearchWhatYouSeePromoInstructionsAXID";

}  // namespace

@implementation SearchWhatYouSeePromoInstructionsViewController

- (instancetype)init {
  self = [super
      initWithTitle:l10n_util::GetNSString(
                        IDS_IOS_SEARCH_WHAT_YOU_SEE_TIPS_INSTRUCTION_TITLE)
       instructions:@[
         l10n_util::GetNSString(
             IDS_IOS_SEARCH_WHAT_YOU_SEE_TIPS_INSTRUCTION_STEP_1),
         l10n_util::GetNSString(
             IDS_IOS_SEARCH_WHAT_YOU_SEE_TIPS_INSTRUCTION_STEP_2),
         l10n_util::GetNSString(
             IDS_IOS_SEARCH_WHAT_YOU_SEE_TIPS_INSTRUCTION_STEP_3),
       ]];
  if (self) {
    self.configuration.secondaryActionString = l10n_util::GetNSString(
        IDS_IOS_SEARCH_WHAT_YOU_SEE_TIPS_INSTRUCTION_LEARN_MORE_ACTION);
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.accessibilityIdentifier = kSearchWhatYouSeePromoInstructionsAXID;
}

@end
