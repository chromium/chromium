// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/utils/atmemory_ui_util.h"

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/integrators/at_memory/memory_data_type.h"
#import "components/autofill/core/browser/integrators/at_memory/memory_data_type_util.h"
#import "components/autofill/core/browser/integrators/at_memory/memory_search_result.h"
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

}  // namespace

using autofill::MemoryDataType;
using autofill::MemorySearchResult;

using AtMemoryUIUtilTest = PlatformTest;

// Tests that `GetAtMemoryGranularFillTitle` returns the localized passport
// title for passport memory data types.
TEST_F(AtMemoryUIUtilTest, TestGetAtMemoryGranularFillTitleForPassport) {
  NSString* const expected_passport_title =
      l10n_util::GetNSString(IDS_AUTOFILL_AI_PASSPORT_ENTITY_NAME);
  MemorySearchResult passport_number_entry(MemoryDataType::kPassportNumber,
                                           kPassportNumberTypeName,
                                           kPassportNumberValue);
  EXPECT_NSEQ(GetAtMemoryGranularFillTitle(passport_number_entry),
              expected_passport_title);

  MemorySearchResult passport_expiration_entry(
      MemoryDataType::kPassportExpirationDate, kPassportExpirationDateTypeName,
      kPassportExpirationDateValue);
  EXPECT_NSEQ(GetAtMemoryGranularFillTitle(passport_expiration_entry),
              expected_passport_title);

  MemorySearchResult passport_name_entry(
      MemoryDataType::kPassportName, kPassportNameTypeName, kPassportNameValue);
  EXPECT_NSEQ(GetAtMemoryGranularFillTitle(passport_name_entry),
              expected_passport_title);
}

// Tests that `GetAtMemoryGranularFillTitle` returns the localized data type
// name for standard field types (non-entity types).
TEST_F(AtMemoryUIUtilTest, TestGetAtMemoryGranularFillTitleForFieldTypes) {
  NSString* const expected_name_title = base::SysUTF16ToNSString(
      autofill::GetMemoryDataTypeNameForI18n(MemoryDataType::kNameFull));
  MemorySearchResult name_entry(MemoryDataType::kNameFull, kNameFullTypeName,
                                kNameFullValue);
  EXPECT_NSEQ(GetAtMemoryGranularFillTitle(name_entry), expected_name_title);

  NSString* const expected_address_title = base::SysUTF16ToNSString(
      autofill::GetMemoryDataTypeNameForI18n(MemoryDataType::kAddressFull));
  MemorySearchResult address_entry(MemoryDataType::kAddressFull,
                                   kAddressFullTypeName, kAddressFullValue);
  EXPECT_NSEQ(GetAtMemoryGranularFillTitle(address_entry),
              expected_address_title);
}

// Tests that `GetAtMemoryGranularFillTitle` returns the entry's `type_name`
// for unknown memory data types.
TEST_F(AtMemoryUIUtilTest, TestGetAtMemoryGranularFillTitleForUnknownType) {
  MemorySearchResult unknown_entry(MemoryDataType::kUnknown, kUnknownTypeName,
                                   kUnknownValue);
  EXPECT_NSEQ(GetAtMemoryGranularFillTitle(unknown_entry), kUnknownTitle);

  MemorySearchResult empty_unknown_entry(MemoryDataType::kUnknown,
                                         kEmptyTypeName, kEmptyUnknownValue);
  EXPECT_NSEQ(GetAtMemoryGranularFillTitle(empty_unknown_entry), kEmptyTitle);
}

// Tests that `AtMemoryGranularFillItemsForSearchResult` converts entry value
// and metadata into `AtMemoryGranularFillItem` instances.
TEST_F(AtMemoryUIUtilTest, TestAtMemoryGranularFillItemsForSearchResult) {
  MemorySearchResult entry(MemoryDataType::kPassportNumber,
                           kPassportNumberTypeName, kPassportNumberValue);
  entry.metadata_list.emplace_back(MemoryDataType::kPassportExpirationDate,
                                   kPassportExpirationDateTypeName,
                                   kPassportExpirationDateValue);
  entry.metadata_list.emplace_back(MemoryDataType::kPassportName,
                                   kPassportNameTypeName, kPassportNameValue);

  NSArray<AtMemoryGranularFillItem*>* items =
      AtMemoryGranularFillItemsForSearchResult(entry);
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
