// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/utils/atmemory_ui_util.h"

#import <vector>

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/integrators/at_memory/memory_data_type.h"
#import "components/autofill/core/browser/integrators/at_memory/memory_data_type_util.h"
#import "components/autofill/core/browser/suggestions/suggestion.h"
#import "components/autofill/core/browser/suggestions/suggestion_type.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_granular_fill_item.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Test string constants.
constexpr char16_t kPassportNumberTypeName[] = u"Passport Number";
constexpr char16_t kPassportNumberValue[] = u"1234";
constexpr char16_t kPassportExpirationDateTypeName[] = u"Expiration Date";
constexpr char16_t kPassportExpirationDateValue[] = u"2030-01-01";
constexpr char16_t kPassportNameTypeName[] = u"Name";
constexpr char16_t kPassportNameValue[] = u"John Doe";

constexpr char16_t kNameFullTypeName[] = u"Name";
constexpr char16_t kNameFullValue[] = u"John Doe";
constexpr char16_t kAddressFullTypeName[] = u"Address";
constexpr char16_t kAddressFullValue[] = u"123 Main St";

constexpr char16_t kUnknownTypeName[] = u"Concert Ticket";
constexpr char16_t kUnknownValue[] = u"Seat 4A";
constexpr char16_t kEmptyTypeName[] = u"";
constexpr char16_t kEmptyUnknownValue[] = u"Value";

NSString* const kUnknownTitle = @"Concert Ticket";
NSString* const kEmptyTitle = @"";

// Helper to create an AtMemory Suggestion for testing.
autofill::Suggestion CreateTestSuggestion(
    autofill::MemoryDataType type,
    std::u16string type_name = u"",
    std::u16string value = u"",
    std::vector<autofill::Suggestion> children = {}) {
  autofill::Suggestion suggestion(
      autofill::SuggestionType::kAtMemorySearchResult);
  autofill::Suggestion::AtMemoryPayload payload;
  payload.memory_data_type = type;
  payload.type_name = std::move(type_name);
  payload.value = std::move(value);
  suggestion.payload = std::move(payload);
  suggestion.children = std::move(children);
  return suggestion;
}

}  // namespace

using autofill::MemoryDataType;

using AtMemoryUIUtilTest = PlatformTest;

// Tests that `GetAtMemoryGranularFillTitle` returns the entry's `type_name`
// for entity types.
TEST_F(AtMemoryUIUtilTest, TestGetAtMemoryGranularFillTitleForPassport) {
  NSString* const expected_passport_title =
      l10n_util::GetNSString(IDS_AUTOFILL_AI_PASSPORT_ENTITY_NAME);
  autofill::Suggestion passport_number_entry = CreateTestSuggestion(
      MemoryDataType::kPassportNumber,
      base::SysNSStringToUTF16(expected_passport_title), kPassportNumberValue);
  EXPECT_NSEQ(GetAtMemoryGranularFillTitle(passport_number_entry),
              expected_passport_title);
}

// Tests that `GetAtMemoryGranularFillTitle` returns the entry's `type_name`
// for standard field types (non-entity types).
TEST_F(AtMemoryUIUtilTest, TestGetAtMemoryGranularFillTitleForFieldTypes) {
  autofill::Suggestion name_entry = CreateTestSuggestion(
      MemoryDataType::kNameFull, kNameFullTypeName, kNameFullValue);
  EXPECT_NSEQ(GetAtMemoryGranularFillTitle(name_entry),
              base::SysUTF16ToNSString(kNameFullTypeName));

  autofill::Suggestion address_entry = CreateTestSuggestion(
      MemoryDataType::kAddressFull, kAddressFullTypeName, kAddressFullValue);
  EXPECT_NSEQ(GetAtMemoryGranularFillTitle(address_entry),
              base::SysUTF16ToNSString(kAddressFullTypeName));
}

// Tests that `GetAtMemoryGranularFillTitle` returns the entry's `type_name`
// for unknown memory data types.
TEST_F(AtMemoryUIUtilTest, TestGetAtMemoryGranularFillTitleForUnknownType) {
  autofill::Suggestion unknown_entry = CreateTestSuggestion(
      MemoryDataType::kUnknown, kUnknownTypeName, kUnknownValue);
  EXPECT_NSEQ(GetAtMemoryGranularFillTitle(unknown_entry), kUnknownTitle);

  autofill::Suggestion empty_unknown_entry = CreateTestSuggestion(
      MemoryDataType::kUnknown, kEmptyTypeName, kEmptyUnknownValue);
  EXPECT_NSEQ(GetAtMemoryGranularFillTitle(empty_unknown_entry), kEmptyTitle);
}

// Tests that `AtMemoryGranularFillItemsForSuggestion` converts suggestion
// children into `AtMemoryGranularFillItem` instances.
TEST_F(AtMemoryUIUtilTest, TestAtMemoryGranularFillItemsForSuggestion) {
  autofill::Suggestion child1 = CreateTestSuggestion(
      MemoryDataType::kPassportExpirationDate, kPassportExpirationDateTypeName,
      kPassportExpirationDateValue);
  autofill::Suggestion child2 = CreateTestSuggestion(
      MemoryDataType::kPassportName, kPassportNameTypeName, kPassportNameValue);
  autofill::Suggestion entry = CreateTestSuggestion(
      MemoryDataType::kPassportNumber, kPassportNumberTypeName,
      kPassportNumberValue, {child1, child2});

  NSArray<AtMemoryGranularFillItem*>* items =
      AtMemoryGranularFillItemsForSuggestion(entry);
  ASSERT_EQ(items.count, 2u);

  EXPECT_NSEQ(items[0].attributeName,
              base::SysUTF16ToNSString(kPassportExpirationDateTypeName));
  EXPECT_NSEQ(items[0].attributeValue,
              base::SysUTF16ToNSString(kPassportExpirationDateValue));

  EXPECT_NSEQ(items[1].attributeName,
              base::SysUTF16ToNSString(kPassportNameTypeName));
  EXPECT_NSEQ(items[1].attributeValue,
              base::SysUTF16ToNSString(kPassportNameValue));
}
