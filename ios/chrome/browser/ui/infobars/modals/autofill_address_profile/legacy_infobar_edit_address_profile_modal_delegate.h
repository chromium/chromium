// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_AUTOFILL_ADDRESS_PROFILE_LEGACY_INFOBAR_EDIT_ADDRESS_PROFILE_MODAL_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_AUTOFILL_ADDRESS_PROFILE_LEGACY_INFOBAR_EDIT_ADDRESS_PROFILE_MODAL_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_delegate.h"

// Delegate to handle Edit Address Profile Infobar Modal actions.
@protocol LegacyInfobarEditAddressProfileModalDelegate <InfobarModalDelegate>

// Saves the edited profile data.
- (void)saveEditedProfileWithData:(NSDictionary*)profileData;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_AUTOFILL_ADDRESS_PROFILE_LEGACY_INFOBAR_EDIT_ADDRESS_PROFILE_MODAL_DELEGATE_H_
