// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_ai/utils/autofill_ai_date_util.h"

#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#import "components/autofill/core/browser/test_utils/entity_data_test_utils.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using autofill::AttributeInstance;
using autofill::EntityInstance;
using autofill::test::GetPassportEntityInstance;

namespace {

constexpr std::u16string kIssueDate = u"2026-03-06";
constexpr std::u16string kNewIssueDate = u"2026-02-01";

// Helper function to create a new EntityInstance with a passport issue date.
EntityInstance CreateEntityInstanceWithIssueDate() {
  EntityInstance instance =
      GetPassportEntityInstance({.issue_date = kIssueDate.data()});

  // GetPassportEntityInstance constructs the date attribute via SetInfo.
  // For date utilities, we need the RawInfo to be populated as it would be from
  // the page, so we explicitly update the RawInfo.
  AttributeInstance attribute =
      instance
          .attribute(autofill::AttributeType(
              autofill::AttributeTypeName::kPassportIssueDate))
          .value();
  attribute.SetRawInfo(autofill::PASSPORT_ISSUE_DATE, kIssueDate,
                       autofill::VerificationStatus::kParsed);

  return instance.CopyWithUpdatedAttribute(attribute);
}

// Helper function to get the passport issue date from an EntityInstance.
NSDate* GetPassportIssueDate(const EntityInstance& instance) {
  base::optional_ref<const AttributeInstance> issue_date = instance.attribute(
      autofill::AttributeType(autofill::AttributeTypeName::kPassportIssueDate));
  if (!issue_date.has_value()) {
    return nil;
  }

  return NSDateFromAttributeInstance(issue_date.value());
}

// Helper function to verify an NSDate has expected year, month, and day.
void VerifyDate(NSDate* date,
                int expected_year,
                int expected_month,
                int expected_day) {
  ASSERT_NE(date, nil);

  NSCalendar* calendar = [[NSCalendar alloc]
      initWithCalendarIdentifier:NSCalendarIdentifierGregorian];
  NSDateComponents* components = [calendar
      components:NSCalendarUnitYear | NSCalendarUnitMonth | NSCalendarUnitDay
        fromDate:date];

  EXPECT_EQ(components.year, expected_year);
  EXPECT_EQ(components.month, expected_month);
  EXPECT_EQ(components.day, expected_day);
}

}  // namespace

class AutofillAIDateUtilTest : public PlatformTest {};

// Tests that NSDate can be extracted from an AttributeInstance and then
// modified and successfully saved back.
TEST_F(AutofillAIDateUtilTest, DateExtractionModificationAndSaving) {
  EntityInstance instance = CreateEntityInstanceWithIssueDate();

  NSDate* extracted_date = GetPassportIssueDate(instance);
  VerifyDate(extracted_date, 2026, 3, 6);

  NSDateComponents* updated_components = [[NSDateComponents alloc] init];
  updated_components.year = 2026;
  updated_components.month = 2;
  updated_components.day = 1;

  NSCalendar* calendar = [[NSCalendar alloc]
      initWithCalendarIdentifier:NSCalendarIdentifierGregorian];
  NSDate* modified_date = [calendar dateFromComponents:updated_components];

  std::u16string new_value = AttributeValueFromNSDate(modified_date);
  EXPECT_EQ(new_value, kNewIssueDate);

  AttributeInstance new_date_attribute =
      instance
          .attribute(autofill::AttributeType(
              autofill::AttributeTypeName::kPassportIssueDate))
          .value();
  new_date_attribute.SetRawInfo(autofill::PASSPORT_ISSUE_DATE, kNewIssueDate,
                                autofill::VerificationStatus::kParsed);

  EntityInstance updated_instance =
      instance.CopyWithUpdatedAttribute(new_date_attribute);
  NSDate* updated_extracted_date = GetPassportIssueDate(updated_instance);
  VerifyDate(updated_extracted_date, 2026, 2, 1);
}

// Tests that extraction fails for an AttributeInstance of non-date type.
TEST_F(AutofillAIDateUtilTest, ExtractionFailsForNonDateType) {
  EntityInstance entity_instance =
      GetPassportEntityInstance({.name = u"John Doe"});
  base::optional_ref<const AttributeInstance> passport_name =
      entity_instance.attribute(
          autofill::AttributeType(autofill::AttributeTypeName::kPassportName));
  ASSERT_TRUE(passport_name.has_value());

  EXPECT_TRUE(NSDateFromAttributeInstance(passport_name.value()) == nil);
}

// Tests that AttributeValueFromNSDate returns an empty string for nil input.
TEST_F(AutofillAIDateUtilTest, ValueFromNilDate) {
  EXPECT_EQ(AttributeValueFromNSDate(nil), u"");
}
