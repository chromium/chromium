// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/public/autofill_settings_navigator.h"

#import <optional>
#import <utility>

#import "components/autofill/core/browser/integrators/at_memory/memory_data_type.h"
#import "components/autofill/core/browser/suggestions/suggestion.h"
#import "components/autofill/core/browser/suggestions/suggestion_type.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using autofill::MemoryDataType;
using autofill::Suggestion;

namespace {

// Returns an AtMemory suggestion carrying `type` as its memory data type.
Suggestion CreateTestSuggestion(MemoryDataType type,
                                bool is_personal_context_sourced = false) {
  Suggestion suggestion(autofill::SuggestionType::kAtMemorySearchResult);
  Suggestion::AtMemoryPayload payload;
  payload.memory_data_type = type;
  payload.is_personal_context_sourced = is_personal_context_sourced;
  suggestion.payload = std::move(payload);
  return suggestion;
}

// Expected settings page for a given memory data type. `std::nullopt` means the
// type has no matching settings page.
struct SettingsPageTestCase {
  // Suffix appended to the parameterized test name.
  const char* test_name;
  MemoryDataType memory_data_type;
  std::optional<AutofillSettingsPage> expected_page;
};

const SettingsPageTestCase kSettingsPageTestCases[] = {
    // Identity documents.
    {"Passport", MemoryDataType::kPassportNumber,
     AutofillSettingsPage::kIdentityDocs},
    {"DriversLicense", MemoryDataType::kDriversLicenseNumber,
     AutofillSettingsPage::kIdentityDocs},
    {"NationalIdCard", MemoryDataType::kNationalIdCardNumber,
     AutofillSettingsPage::kIdentityDocs},
    // Travel.
    {"FlightReservation", MemoryDataType::kFlightReservationFlightNumber,
     AutofillSettingsPage::kTravel},
    {"Vehicle", MemoryDataType::kVehicleMake, AutofillSettingsPage::kTravel},
    {"KnownTravelerNumber", MemoryDataType::kKnownTravelerNumberNumber,
     AutofillSettingsPage::kTravel},
    // Shopping.
    {"Order", MemoryDataType::kOrderDate, AutofillSettingsPage::kShopping},
    {"Shipment", MemoryDataType::kShipmentTrackingNumber,
     AutofillSettingsPage::kShopping},
    // Addresses and payments.
    {"Address", MemoryDataType::kAddressFull, AutofillSettingsPage::kAddresses},
    {"CreditCard", MemoryDataType::kCreditCardNumber,
     AutofillSettingsPage::kCreditCards},
    {"Iban", MemoryDataType::kIban, AutofillSettingsPage::kCreditCards},
    // Types with no settings page.
    {"Unknown", MemoryDataType::kUnknown, std::nullopt},
};

}  // namespace

using AutofillSettingsNavigatorTest = PlatformTest;

// Tests that a suggestion sourced from personal context routes to the
// Suggestions from Gemini settings page, regardless of its memory data type.
TEST_F(AutofillSettingsNavigatorTest, TestSettingsPageForPersonalContext) {
  EXPECT_EQ(AutofillSettingsPageForAtMemorySuggestion(
                CreateTestSuggestion(MemoryDataType::kPassportNumber,
                                     /*is_personal_context_sourced=*/true)),
            AutofillSettingsPage::kSuggestionsFromGemini);
}

// Tests that a malformed suggestion without an AtMemory payload maps to no
// settings page.
TEST_F(AutofillSettingsNavigatorTest, TestSettingsPageForMissingPayload) {
  Suggestion suggestion(autofill::SuggestionType::kAtMemorySearchResult);

  EXPECT_EQ(AutofillSettingsPageForAtMemorySuggestion(suggestion),
            std::nullopt);
}

class AutofillSettingsNavigatorMappingTest
    : public PlatformTest,
      public testing::WithParamInterface<SettingsPageTestCase> {};

// Tests that each memory data type routes to its category settings page.
TEST_P(AutofillSettingsNavigatorMappingTest,
       TestSettingsPageForMemoryDataType) {
  const SettingsPageTestCase& test_case = GetParam();
  const Suggestion suggestion =
      CreateTestSuggestion(test_case.memory_data_type);

  EXPECT_EQ(AutofillSettingsPageForAtMemorySuggestion(suggestion),
            test_case.expected_page);
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    AutofillSettingsNavigatorMappingTest,
    testing::ValuesIn(kSettingsPageTestCases),
    [](const testing::TestParamInfo<SettingsPageTestCase>& info) {
      return info.param.test_name;
    });
