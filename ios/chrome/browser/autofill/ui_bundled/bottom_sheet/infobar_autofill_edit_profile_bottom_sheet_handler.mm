// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/infobar_autofill_edit_profile_bottom_sheet_handler.h"

@implementation InfobarAutofillEditProfileBottomSheetHandler

#pragma mark - AutofillEditProfileBottomSheetHandler

- (void)didCancelBottomSheetView {
  // TODO(crbug.com/394653508): Implement method.
}

- (void)didSaveProfile {
  // TODO(crbug.com/394653508): Implement method.
}

- (BOOL)isMigrationToAccount {
  // TODO(crbug.com/394653508): Implement method.
  return NO;
}

- (std::unique_ptr<autofill::AutofillProfile>)autofillProfile {
  // TODO(crbug.com/394653508): Implement method.
  return nullptr;
}

- (AutofillSaveProfilePromptMode)saveProfilePromptMode {
  // TODO(crbug.com/394653508): Implement method.
  return AutofillSaveProfilePromptMode::kNewProfile;
}

- (NSString*)userEmail {
  // TODO(crbug.com/394653508): Implement method.
  return nil;
}

@end
