// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_ADDRESS_AUTOFILLPROFILE_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_ADDRESS_AUTOFILLPROFILE_H_

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_address.h"

#include <vector>

namespace autofill {
class AutofillProfile;
}

// Extends `ManualFillAddress` with a convenience initializer from c++
// `autofill::AutofillProfile`.
@interface ManualFillAddress (AutofillProfile)

// Convenience initializer from an autofill::AutofillProfile.
- (instancetype)initWithProfile:(const autofill::AutofillProfile&)profile;

// Converts a list of `autofill::AutofillProfile` into a list of
// `ManualFillAddress`.
+ (NSArray<ManualFillAddress*>*)manualFillAddressesFromProfiles:
    (std::vector<autofill::AutofillProfile*>)profiles;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_ADDRESS_AUTOFILLPROFILE_H_
