// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/tips/ui/use_autofill_instructional_view_controller.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// The name of the animation used for the use autofill view.
NSString* const kAnimationName = @"use_autofill";

// The name of the animation used for the use autofill view in dark mode.
NSString* const kAnimationNameDarkMode = @"use_autofill_darkmode";

// The accessibility identifier for the use autofill instructional view.
NSString* const kUseAutofillInstructionalAXID =
    @"kUseAutofillInstructionalAXID";

// The keypath for the "Use Keyboard" animation text in the
// `animationTextProvider` dictionary.
NSString* const kUseKeyboardKeypath = @"use_keyboard";

// The keypath for the "Use Password" animation text in the
// `animationTextProvider` dictionary.
NSString* const kUsePasswordKeypath = @"use_password";

}  // namespace

@implementation UseAutofillInstructionalViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.animationName = kAnimationName;
  self.animationNameDarkMode = kAnimationNameDarkMode;
  self.animationBackgroundColor =
      [UIColor colorNamed:kSecondaryBackgroundColor];

  // Set the text localization.
  NSString* useKeyboardTitle =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_BOTTOM_SHEET_USE_KEYBOARD);
  NSString* usePasswordTitle =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_BOTTOM_SHEET_USE_PASSWORD);
  self.animationTextProvider = @{
    kUseKeyboardKeypath : useKeyboardTitle,
    kUsePasswordKeypath : usePasswordTitle,
  };

  self.titleString =
      l10n_util::GetNSString(IDS_IOS_MAGIC_STACK_TIP_AUTOFILL_PASSWORDS_TITLE);
  self.subtitleString = l10n_util::GetNSString(
      IDS_IOS_MAGIC_STACK_TIP_AUTOFILL_PASSWORDS_DESCRIPTION);
  self.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_MAGIC_STACK_TIP_GOT_IT);
  self.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_MAGIC_STACK_TIP_SHOW_ME_HOW);

  [super viewDidLoad];

  self.view.accessibilityIdentifier = kUseAutofillInstructionalAXID;
}

@end
