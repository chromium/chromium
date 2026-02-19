// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOFILL_AI_UTIL_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOFILL_AI_UTIL_H_

#import <UIKit/UIKit.h>

#import "components/autofill/core/browser/permissions/autofill_ai/autofill_ai_permission_utils.h"

class ProfileIOS;

namespace autofill {

// Returns the country code from the variations service.
// If the variations service is not available, an empty string is returned.
const std::string GetCountryCodeFromVariations();

// Returns whether the wallet storage is enabled for the profile.
bool IsWalletStorageEnabled(ProfileIOS* profile);

// Returns YES if the Autofill AI action can be performed for the given profile.
bool CanPerformAutofillAiAction(ProfileIOS* profile, AutofillAiAction action);

// Returns the default icon for the Autofill AI entity type.
UIImage* DefaultIconForAutofillAiEntityType(EntityTypeName entity_type_name,
                                            CGFloat symbol_point_size);

// Returns whether Enhanced Autofill is enabled.
bool IsEnhancedAutofillEnabled(ProfileIOS* profile);

// Enables or disables Enhanced Autofill.
void SetEnhancedAutofillEnabled(ProfileIOS* profile, bool enabled);

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOFILL_AI_UTIL_H_
