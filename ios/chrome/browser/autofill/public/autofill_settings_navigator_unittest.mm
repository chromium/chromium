// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/public/autofill_settings_navigator.h"

#import "base/test/scoped_feature_list.h"
#import "components/autofill/core/browser/integrators/at_memory/memory_data_type.h"
#import "components/autofill/core/browser/suggestions/suggestion.h"
#import "components/autofill/core/browser/suggestions/suggestion_type.h"
#import "ios/chrome/browser/shared/public/features/features.h"
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

// Expected settings page for a given memory data type, in both
// `kYourSavedInfoSettingsPageIos` states. `std::nullopt` means the type has no
// matching settings page.
struct SettingsPageTestCase {
  // Suffix appended to the parameterized test name.
  const char* test_name;
  MemoryDataType memory_data_type;
  std::optional<AutofillSettingsPage> page_with_your_saved_info;
  std::optional<AutofillSettingsPage> page_without_your_saved_info;
};

const SettingsPageTestCase kSettingsPageTestCases[] = {
    // Identity documents.
    {"Passport", MemoryDataType::kPassportNumber,
     AutofillSettingsPage::kIdentityDocs, AutofillSettingsPage::kAddresses},
    {"DriversLicense", MemoryDataType::kDriversLicenseNumber,
     AutofillSettingsPage::kIdentityDocs, AutofillSettingsPage::kAddresses},
    {"NationalIdCard", MemoryDataType::kNationalIdCardNumber,
     AutofillSettingsPage::kIdentityDocs, AutofillSettingsPage::kAddresses},
    // Travel.
    {"FlightReservation", MemoryDataType::kFlightReservationFlightNumber,
     AutofillSettingsPage::kTravel, AutofillSettingsPage::kAddresses},
    {"Vehicle", MemoryDataType::kVehicleMake, AutofillSettingsPage::kTravel,
     AutofillSettingsPage::kAddresses},
    {"KnownTravelerNumber", MemoryDataType::kKnownTravelerNumberNumber,
     AutofillSettingsPage::kTravel, AutofillSettingsPage::kAddresses},
    // Shopping.
    {"Order", MemoryDataType::kOrderDate, AutofillSettingsPage::kShopping,
     AutofillSettingsPage::kAddresses},
    {"Shipment", MemoryDataType::kShipmentTrackingNumber,
     AutofillSettingsPage::kShopping, AutofillSettingsPage::kAddresses},
    // Types that are not gated behind `kYourSavedInfoSettingsPageIos`.
    {"Address", MemoryDataType::kAddressFull, AutofillSettingsPage::kAddresses,
     AutofillSettingsPage::kAddresses},
    {"CreditCard", MemoryDataType::kCreditCardNumber,
     AutofillSettingsPage::kCreditCards, AutofillSettingsPage::kCreditCards},
    // Types with no settings page.
    {"Iban", MemoryDataType::kIban, std::nullopt, std::nullopt},
    {"Unknown", MemoryDataType::kUnknown, std::nullopt, std::nullopt},
};

}  // namespace

using AutofillSettingsNavigatorTest = PlatformTest;

// Test that a suggestion sourced from personal context routes to the enhanced
// autofill settings page, regardless of its memory data type.
TEST_F(AutofillSettingsNavigatorTest, TestSettingsPageForPersonalContext) {
  base::test::ScopedFeatureList feature_list(kYourSavedInfoSettingsPageIos);

  EXPECT_EQ(AutofillSettingsPageForAtMemorySuggestion(
                CreateTestSuggestion(MemoryDataType::kPassportNumber,
                                     /*is_personal_context_sourced=*/true)),
            AutofillSettingsPage::kEnhancedAutofill);
}

// Test that a malformed suggestion without an AtMemory payload maps to no
// settings page.
TEST_F(AutofillSettingsNavigatorTest, TestSettingsPageForMissingPayload) {
  Suggestion suggestion(autofill::SuggestionType::kAtMemorySearchResult);

  EXPECT_EQ(AutofillSettingsPageForAtMemorySuggestion(suggestion),
            std::nullopt);
}

class AutofillSettingsNavigatorMappingTest
    : public PlatformTest,
      public testing::WithParamInterface<SettingsPageTestCase> {};

// Test that each memory data type routes to its category settings page when
// `kYourSavedInfoSettingsPageIos` is enabled, and falls back to the addresses
// settings page when it is disabled.
TEST_P(AutofillSettingsNavigatorMappingTest,
       TestSettingsPageForMemoryDataType) {
  const SettingsPageTestCase& test_case = GetParam();
  const Suggestion suggestion =
      CreateTestSuggestion(test_case.memory_data_type);

  {
    base::test::ScopedFeatureList feature_list(kYourSavedInfoSettingsPageIos);
    EXPECT_EQ(AutofillSettingsPageForAtMemorySuggestion(suggestion),
              test_case.page_with_your_saved_info);
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(kYourSavedInfoSettingsPageIos);
    EXPECT_EQ(AutofillSettingsPageForAtMemorySuggestion(suggestion),
              test_case.page_without_your_saved_info);
  }
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    AutofillSettingsNavigatorMappingTest,
    testing::ValuesIn(kSettingsPageTestCases),
    [](const testing::TestParamInfo<SettingsPageTestCase>& info) {
      return info.param.test_name;
    });
