// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/public/autofill_settings_navigator.h"

#import <variant>

#import "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#import "components/autofill/core/browser/integrators/at_memory/memory_data_type.h"
#import "components/autofill/core/browser/integrators/at_memory/memory_data_type_util.h"
#import "components/autofill/core/browser/suggestions/suggestion.h"
#import "ios/chrome/browser/shared/public/features/features.h"

using autofill::EntityTypeName;
using autofill::MemoryDataType;
using autofill::MemoryDataTypeCategory;
using autofill::Suggestion;

AutofillSettingsPage AutofillSettingsPageForEntityTypeName(
    EntityTypeName entity_type_name) {
  switch (entity_type_name) {
    case EntityTypeName::kPassport:
    case EntityTypeName::kDriversLicense:
    case EntityTypeName::kNationalIdCard:
      return AutofillSettingsPage::kIdentityDocs;
    case EntityTypeName::kOrder:
    case EntityTypeName::kShipment:
      return AutofillSettingsPage::kShopping;
    case EntityTypeName::kFlightReservation:
    case EntityTypeName::kVehicle:
    case EntityTypeName::kKnownTravelerNumber:
    case EntityTypeName::kRedressNumber:
      return AutofillSettingsPage::kTravel;
  }
}

std::optional<AutofillSettingsPage> AutofillSettingsPageForMemoryDataType(
    MemoryDataType memory_data_type) {
  switch (autofill::GetMemoryDataTypeCategory(memory_data_type)) {
    case MemoryDataTypeCategory::kPassport:
    case MemoryDataTypeCategory::kDriversLicense:
    case MemoryDataTypeCategory::kNationalIdCard:
      return IsYourSavedInfoSettingsPageIosEnabled()
                 ? AutofillSettingsPage::kIdentityDocs
                 : AutofillSettingsPage::kAddresses;
    case MemoryDataTypeCategory::kFlightReservation:
    case MemoryDataTypeCategory::kKnownTravelerNumber:
    case MemoryDataTypeCategory::kRedressNumber:
    case MemoryDataTypeCategory::kVehicle:
      return IsYourSavedInfoSettingsPageIosEnabled()
                 ? AutofillSettingsPage::kTravel
                 : AutofillSettingsPage::kAddresses;
    case MemoryDataTypeCategory::kOrder:
    case MemoryDataTypeCategory::kShipment:
      return IsYourSavedInfoSettingsPageIosEnabled()
                 ? AutofillSettingsPage::kShopping
                 : AutofillSettingsPage::kAddresses;
    // IBANs are bank account numbers, which live in the payment methods
    // (credit cards) settings page.
    case MemoryDataTypeCategory::kCreditCard:
    case MemoryDataTypeCategory::kIban:
      return AutofillSettingsPage::kCreditCards;
    case MemoryDataTypeCategory::kContactInfo:
      return AutofillSettingsPage::kAddresses;
    case MemoryDataTypeCategory::kUnknown:
      return std::nullopt;
  }
}

std::optional<AutofillSettingsPage> AutofillSettingsPageForAtMemorySuggestion(
    const Suggestion& suggestion) {
  const Suggestion::AtMemoryPayload* payload =
      std::get_if<Suggestion::AtMemoryPayload>(&suggestion.payload);
  if (!payload) {
    return std::nullopt;
  }

  if (payload->is_personal_context_sourced) {
    return AutofillSettingsPage::kSuggestionsFromGemini;
  }

  return AutofillSettingsPageForMemoryDataType(payload->memory_data_type);
}
