// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/infobar_autofill_edit_profile_bottom_sheet_handler.h"

#import <Foundation/Foundation.h>

#import "base/check_deref.h"
#import "base/debug/dump_without_crashing.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/form_import/addresses/autofill_save_update_address_profile_delegate_ios.h"
#import "components/infobars/core/infobar.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/model/infobar_type.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"

@implementation InfobarAutofillEditProfileBottomSheetHandler {
  // The WebState this instance is observing.
  base::WeakPtr<web::WebState> _webState;
}

- (instancetype)initWithWebState:(web::WebState*)webState {
  if (self) {
    CHECK(webState);
    _webState = webState->GetWeakPtr();
  }
  return self;
}

#pragma mark - AutofillEditProfileBottomSheetHandler

- (void)didCancelBottomSheetView {
  autofill::AutofillSaveUpdateAddressProfileDelegateIOS* delegate =
      [self fetchDelegate];
  CHECK(delegate);

  if (delegate->IsMigrationToAccount()) {
    delegate->Never();
    infobars::InfoBar* infobar = [self addressInfobar];
    if (infobar) {
      [self infobarManager].RemoveInfoBar(infobar);
    }
  } else {
    delegate->EditDeclined();
  }
}

- (void)didSaveProfile:(autofill::AutofillProfile*)profile {
  autofill::AutofillSaveUpdateAddressProfileDelegateIOS* delegate =
      [self acceptInfobarAndFetchDelegate];

  if (delegate) {
    delegate->SetProfile(profile);
    delegate->EditAccepted();
  }
}

- (BOOL)isMigrationToAccount {
  autofill::AutofillSaveUpdateAddressProfileDelegateIOS* delegate =
      [self fetchDelegate];
  CHECK(delegate);

  return delegate->IsMigrationToAccount();
}

- (std::unique_ptr<autofill::AutofillProfile>)autofillProfile {
  autofill::AutofillSaveUpdateAddressProfileDelegateIOS* delegate =
      [self fetchDelegate];
  CHECK(delegate);

  return std::make_unique<autofill::AutofillProfile>(*delegate->GetProfile());
}

- (AutofillSaveProfilePromptMode)saveProfilePromptMode {
  autofill::AutofillSaveUpdateAddressProfileDelegateIOS* delegate =
      [self fetchDelegate];
  CHECK(delegate);

  AutofillSaveProfilePromptMode saveProfilePromptMode =
      AutofillSaveProfilePromptMode::kNewProfile;
  if (delegate->IsMigrationToAccount()) {
    saveProfilePromptMode = AutofillSaveProfilePromptMode::kMigrateProfile;
  } else if (delegate->GetOriginalProfile() != nullptr) {
    saveProfilePromptMode = AutofillSaveProfilePromptMode::kUpdateProfile;
  }

  return saveProfilePromptMode;
}

- (NSString*)userEmail {
  autofill::AutofillSaveUpdateAddressProfileDelegateIOS* delegate =
      [self fetchDelegate];
  CHECK(delegate);

  return delegate->UserAccountEmail()
             ? base::SysUTF16ToNSString(delegate->UserAccountEmail().value())
             : nil;
}

- (BOOL)addingManualAddress {
  return NO;
}

#pragma mark - Private

// Retrieves the AutofillSaveUpdateAddressProfileDelegateIOS from the address
// infobar.
- (autofill::AutofillSaveUpdateAddressProfileDelegateIOS*)fetchDelegate {
  // TODO(crbug.com/397716718): Store the delegate so that it doesn't have to be
  // fetched every time it's needed.
  InfoBarIOS* infobar = static_cast<InfoBarIOS*>([self addressInfobar]);
  if (!infobar) {
    return nullptr;
  }

  return [self fetchDelegateFromInfobar:infobar];
}

// Extracts the AutofillSaveUpdateAddressProfileDelegateIOS from a given
// `infobar`.
- (autofill::AutofillSaveUpdateAddressProfileDelegateIOS*)
    fetchDelegateFromInfobar:(InfoBarIOS*)infobar {
  autofill::AutofillSaveUpdateAddressProfileDelegateIOS* delegate =
      autofill::AutofillSaveUpdateAddressProfileDelegateIOS::
          FromInfobarDelegate(infobar->delegate());

  return delegate;
}

// Retrieves and accepts the address infobar, then fetches and returns its
// delegate.
- (autofill::AutofillSaveUpdateAddressProfileDelegateIOS*)
    acceptInfobarAndFetchDelegate {
  InfoBarIOS* infobar = static_cast<InfoBarIOS*>([self addressInfobar]);
  if (!infobar) {
    return nullptr;
  }

  infobar->set_accepted(YES);
  return [self fetchDelegateFromInfobar:infobar];
}

// Finds and returns the address autofill infobar form the web state, if
// present.
- (infobars::InfoBar*)addressInfobar {
  InfoBarManagerImpl& manager = [self infobarManager];
  const auto it = std::ranges::find(
      manager.infobars(), InfobarType::kInfobarTypeSaveAutofillAddressProfile,
      [](const infobars::InfoBar* infobar) {
        return static_cast<const InfoBarIOS*>(infobar)->infobar_type();
      });

  return it != manager.infobars().cend() ? *it : nullptr;
}

// Retrieves the InfoBarManagerImpl from the WebState.
- (InfoBarManagerImpl&)infobarManager {
  CHECK(_webState);
  return CHECK_DEREF(InfoBarManagerImpl::FromWebState(_webState.get()));
}

@end
