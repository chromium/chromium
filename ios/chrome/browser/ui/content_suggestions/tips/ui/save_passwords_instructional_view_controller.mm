// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/tips/ui/save_passwords_instructional_view_controller.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// The name of the animation used for the save passwords view.
NSString* const kAnimationName = @"save_passwords";

// The name of the animation used for the save passwords view in dark mode.
NSString* const kAnimationNameDarkMode = @"save_passwords_darkmode";

// The accessibility identifier for the save passwords instructional view.
NSString* const kSavePasswordsInstructionalAXID =
    @"kSavePasswordsInstructionalAXID";

// The keypath for the "Save Password?" animation text in the
// `animationTextProvider` dictionary.
NSString* const kSavePasswordKeypath = @"save_password";

// The keypath for the "Save" animation text in the `animationTextProvider`
// dictionary.
NSString* const kSaveKeypath = @"save";

// The keypath for the "Done" animation text in the `animationTextProvider`
// dictionary.
NSString* const kDoneKeypath = @"done";

}  // namespace

@implementation SavePasswordsInstructionalViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.animationName = kAnimationName;
  self.animationNameDarkMode = kAnimationNameDarkMode;
  self.animationBackgroundColor =
      [UIColor colorNamed:kSecondaryBackgroundColor];

  // Set the text localization.
  NSString* savePasswordTitle =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_MANAGER_SAVE_PASSWORD_PROMPT);
  NSString* saveTitle =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_MANAGER_SAVE_BUTTON);
  NSString* doneTitle = l10n_util::GetNSString(IDS_IOS_SUGGESTIONS_DONE);
  self.animationTextProvider = @{
    kSavePasswordKeypath : savePasswordTitle,
    kSaveKeypath : saveTitle,
    kDoneKeypath : doneTitle,
  };

  self.titleString =
      l10n_util::GetNSString(IDS_IOS_MAGIC_STACK_TIP_SAVE_PASSWORD_TITLE);
  self.subtitleString =
      l10n_util::GetNSString(IDS_IOS_MAGIC_STACK_TIP_SAVE_PASSWORD_DESCRIPTION);
  self.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_MAGIC_STACK_TIP_GOT_IT);
  self.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_MAGIC_STACK_TIP_SHOW_ME_HOW);

  [super viewDidLoad];

  self.view.accessibilityIdentifier = kSavePasswordsInstructionalAXID;
}

@end
