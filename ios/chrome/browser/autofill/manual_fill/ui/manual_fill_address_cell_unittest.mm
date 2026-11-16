// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/manual_fill/ui/manual_fill_address_cell.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/autofill/manual_fill/model/manual_fill_address.h"
#import "ios/chrome/browser/autofill/manual_fill/public/manual_fill_constants.h"
#import "ios/chrome/browser/autofill/manual_fill/ui/manual_fill_address_cell_mutator.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

constexpr NSInteger kCellIndex = 2;

// Creates a test address with 12 distinct field values so each chip button can
// be uniquely identified and verified.
ManualFillAddress* CreateTestAddress() {
  return [[ManualFillAddress alloc]
             initWithGUID:@"00000000-0000-0000-0000-000000000001"
                firstName:@"Jane"
      middleNameOrInitial:@"M."
                 lastName:@"Doe"
                  company:@"Chromium"
                    line1:@"123 Main St"
                    line2:@"Apt 4B"
                      zip:@"94043"
                     city:@"Mountain View"
                    state:@"CA"
                  country:@"US"
              phoneNumber:@"650-555-0100"
             emailAddress:@"jane@example.com"];
}

// Returns the visible chip button inside `view` displaying `title`.
UIButton* FindChipWithTitle(UIView* view, NSString* title) {
  return chrome_test_util::FindViewWithPredicate<UIButton>(
      view, ^BOOL(UIView* candidate) {
        UIButton* button = base::apple::ObjCCast<UIButton>(candidate);
        return button && !button.hidden &&
               [button.configuration.attributedTitle.string
                   isEqualToString:title];
      });
}

}  // namespace

// Test fixture for testing `ManualFillAddressCell`.
class ManualFillAddressCellTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    mutator_ = OCMProtocolMock(@protocol(ManualFillAddressCellMutator));
    address_ = CreateTestAddress();
    cell_ = [[ManualFillAddressCell alloc] init];
    [cell_ setUpWithAddress:address_
                            mutator:mutator_
                        menuActions:@[]
                          cellIndex:kCellIndex
        cellIndexAccessibilityLabel:@""
             showAutofillFormButton:YES];
  }

  void TearDown() override {
    [cell_ prepareForReuse];
    cell_ = nil;
    address_ = nil;
    mutator_ = nil;
    PlatformTest::TearDown();
  }

  OCMockObject<ManualFillAddressCellMutator>* mutator_;
  ManualFillAddress* address_;
  ManualFillAddressCell* cell_;
};

// Tests that tapping each address field chip notifies the mutator with the
// address and corresponding `ManualFillAddressFieldType`.
TEST_F(ManualFillAddressCellTest, TappingChipsNotifiesMutator) {
  struct ChipTestCase {
    NSString* title;
    ManualFillAddressFieldType field_type;
  };
  const ChipTestCase kChips[] = {
      {.title = @"Jane", .field_type = ManualFillAddressFieldType::kFirstName},
      {.title = @"M.", .field_type = ManualFillAddressFieldType::kMiddleName},
      {.title = @"Doe", .field_type = ManualFillAddressFieldType::kLastName},
      {.title = @"Chromium",
       .field_type = ManualFillAddressFieldType::kCompany},
      {.title = @"123 Main St",
       .field_type = ManualFillAddressFieldType::kLine1},
      {.title = @"Apt 4B", .field_type = ManualFillAddressFieldType::kLine2},
      {.title = @"94043", .field_type = ManualFillAddressFieldType::kZip},
      {.title = @"Mountain View",
       .field_type = ManualFillAddressFieldType::kCity},
      {.title = @"CA", .field_type = ManualFillAddressFieldType::kState},
      {.title = @"US", .field_type = ManualFillAddressFieldType::kCountry},
      {.title = @"650-555-0100",
       .field_type = ManualFillAddressFieldType::kPhoneNumber},
      {.title = @"jane@example.com",
       .field_type = ManualFillAddressFieldType::kEmailAddress},
  };

  for (const ChipTestCase& chip_case : kChips) {
    SCOPED_TRACE(base::SysNSStringToUTF8(chip_case.title));
    UIButton* chip = FindChipWithTitle(cell_, chip_case.title);
    ASSERT_NE(chip, nil);
    OCMExpect([mutator_ didTapChipForAddress:address_
                                   fieldType:chip_case.field_type]);
    [chip sendActionsForControlEvents:UIControlEventTouchUpInside];
    EXPECT_OCMOCK_VERIFY(mutator_);
  }
}

// Tests that tapping the "Autofill Form" button notifies the mutator with the
// address and cell index.
TEST_F(ManualFillAddressCellTest, TappingAutofillFormButtonNotifiesMutator) {
  UIButton* autofill_button = chrome_test_util::FindViewById<UIButton>(
      cell_, manual_fill::kExpandedManualFillAutofillFormButtonID);
  ASSERT_NE(autofill_button, nil);
  EXPECT_FALSE(autofill_button.hidden);

  OCMExpect([mutator_ didTapAutofillFormButtonForAddress:address_
                                               cellIndex:kCellIndex]);
  [autofill_button sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_OCMOCK_VERIFY(mutator_);
}
