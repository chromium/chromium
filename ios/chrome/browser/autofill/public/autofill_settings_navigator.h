// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_PUBLIC_AUTOFILL_SETTINGS_NAVIGATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_PUBLIC_AUTOFILL_SETTINGS_NAVIGATOR_H_

#import <Foundation/Foundation.h>

#import <optional>

#import "base/ios/block_types.h"

namespace autofill {
enum class EntityTypeName;
enum class MemoryDataType;
struct Suggestion;
}  // namespace autofill

// Pages in the Settings UI that can be navigated to from Autofill surfaces.
enum class AutofillSettingsPage {
  kPasswordManager,
  kPasswordSettings,
  kCreditCards,
  kAddresses,
  kIdentityDocs,
  kShopping,
  kTravel,
  kEnhancedAutofill,
  kSuggestionsFromGeminiHelpImprove,
};

// Returns the `AutofillSettingsPage` corresponding to `entity_type_name`.
AutofillSettingsPage AutofillSettingsPageForEntityTypeName(
    autofill::EntityTypeName entity_type_name);

// Returns the `AutofillSettingsPage` corresponding to `memory_data_type`, or
// `std::nullopt` if no settings page hosts that type. If the `YourSavedInfo`
// feature flag is disabled, entity types map to
// `AutofillSettingsPage::kAddresses`.
std::optional<AutofillSettingsPage> AutofillSettingsPageForMemoryDataType(
    autofill::MemoryDataType memory_data_type);

// Returns the `AutofillSettingsPage` corresponding to the AtMemory
// `suggestion`, or `std::nullopt` if no settings page hosts it. Suggestions
// sourced from personal context map to
// `AutofillSettingsPage::kEnhancedAutofill`; all others are routed by their
// memory data type.
std::optional<AutofillSettingsPage> AutofillSettingsPageForAtMemorySuggestion(
    const autofill::Suggestion& suggestion);

// Delegate protocol for handling navigation to settings pages.
@protocol AutofillSettingsNavigator <NSObject>

// Requests to open the settings page for the specified `page`.
- (void)openSettingsForPage:(AutofillSettingsPage)page;

// Same as above, but invokes `completion` once the settings UI is dismissed.
// Only `AutofillSettingsPage::kEnhancedAutofill` reports its dismissal, so a
// non-nil `completion` is only valid for that page.
- (void)openSettingsForPage:(AutofillSettingsPage)page
                 completion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_PUBLIC_AUTOFILL_SETTINGS_NAVIGATOR_H_
