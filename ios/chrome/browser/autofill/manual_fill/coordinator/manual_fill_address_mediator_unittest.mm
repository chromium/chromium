// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/manual_fill/coordinator/manual_fill_address_mediator.h"

#import <string>
#import <string_view>

#import "base/strings/sys_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/metrics/user_action_tester.h"
#import "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#import "components/autofill/core/browser/field_types.h"
#import "components/autofill/core/browser/suggestions/suggestion.h"
#import "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/manual_fill/model/manual_fill_address.h"
#import "ios/chrome/browser/autofill/manual_fill/public/manual_fill_content_injector.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

constexpr std::string_view kAddressGuid =
    "00000000-0000-0000-0000-000000000001";
constexpr std::string_view kSuggestionAcceptedAction =
    "ManualFallback_Profiles_SuggestionAccepted";
constexpr std::string_view kAcceptedSuggestionHistogram =
    "Autofill.UserAcceptedSuggestionAtIndex.Address.ManualFallback";

// Creates a test address with 12 distinct field values so each field-type
// mapping can be verified independently.
ManualFillAddress* CreateTestAddress() {
  return [[ManualFillAddress alloc]
             initWithGUID:base::SysUTF8ToNSString(kAddressGuid)
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

}  // namespace

// Test fixture for testing `ManualFillAddressMediator`.
class ManualFillAddressMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    // `authenticationService` is only used by the overflow-menu edit flow,
    // which is not exercised in these tests.
    mediator_ = [[ManualFillAddressMediator alloc]
        initWithPersonalDataManager:&test_personal_data_manager_
             showAutofillFormButton:YES
              authenticationService:nullptr];
    content_injector_ = OCMProtocolMock(@protocol(ManualFillContentInjector));
    mediator_.contentInjector = content_injector_;
  }

  void TearDown() override { [mediator_ disconnect]; }

  ManualFillAddressMediator* mediator() { return mediator_; }

  OCMockObject<ManualFillContentInjector>* content_injector() {
    return content_injector_;
  }

  // Expects `content_injector()` to receive `expected_value` via
  // `userDidPickContent:`, invokes `didTapChipForAddress:fieldType:` on the
  // mediator, and verifies that `expected_action` was recorded.
  void ExpectAddressChipFill(ManualFillAddress* address,
                             ManualFillAddressFieldType field_type,
                             NSString* expected_value,
                             std::string_view expected_action) {
    SCOPED_TRACE(expected_action);
    base::UserActionTester user_action_tester;
    OCMExpect([content_injector()
        userDidPickContent:expected_value
             passwordField:NO
             requiresHTTPS:NO
           jumpToNextField:YES
                actionType:autofill::mojom::FieldActionType::kReplaceAll]);

    [mediator() didTapChipForAddress:address fieldType:field_type];

    EXPECT_OCMOCK_VERIFY(content_injector());
    EXPECT_EQ(user_action_tester.GetActionCount(expected_action), 1);
  }

 private:
  autofill::TestPersonalDataManager test_personal_data_manager_;
  ManualFillAddressMediator* mediator_;
  OCMockObject<ManualFillContentInjector>* content_injector_;
};

// Tests that tapping each address field chip extracts the expected field value,
// records the corresponding user action, and forwards the value to the content
// injector.
TEST_F(ManualFillAddressMediatorTest, DidTapAddressChip) {
  ManualFillAddress* address = CreateTestAddress();
  ExpectAddressChipFill(address, ManualFillAddressFieldType::kFirstName,
                        @"Jane", "ManualFallback_Profiles_SelectFirstName");
  ExpectAddressChipFill(address, ManualFillAddressFieldType::kMiddleName, @"M.",
                        "ManualFallback_Profiles_SelectMiddleName");
  ExpectAddressChipFill(address, ManualFillAddressFieldType::kLastName, @"Doe",
                        "ManualFallback_Profiles_SelectLastName");
  ExpectAddressChipFill(address, ManualFillAddressFieldType::kCompany,
                        @"Chromium", "ManualFallback_Profiles_Company");
  ExpectAddressChipFill(address, ManualFillAddressFieldType::kLine1,
                        @"123 Main St", "ManualFallback_Profiles_Address1");
  ExpectAddressChipFill(address, ManualFillAddressFieldType::kLine2, @"Apt 4B",
                        "ManualFallback_Profiles_Address2");
  ExpectAddressChipFill(address, ManualFillAddressFieldType::kZip, @"94043",
                        "ManualFallback_Profiles_Zip");
  ExpectAddressChipFill(address, ManualFillAddressFieldType::kCity,
                        @"Mountain View", "ManualFallback_Profiles_City");
  ExpectAddressChipFill(address, ManualFillAddressFieldType::kState, @"CA",
                        "ManualFallback_Profiles_State");
  ExpectAddressChipFill(address, ManualFillAddressFieldType::kCountry, @"US",
                        "ManualFallback_Profiles_Country");
  ExpectAddressChipFill(address, ManualFillAddressFieldType::kPhoneNumber,
                        @"650-555-0100", "ManualFallback_Profiles_PhoneNumber");
  ExpectAddressChipFill(address, ManualFillAddressFieldType::kEmailAddress,
                        @"jane@example.com",
                        "ManualFallback_Profiles_EmailAddress");
}

// Tests that tapping the "Autofill Form" button records metrics, constructs the
// `FormSuggestion`, and forwards it to the content injector.
TEST_F(ManualFillAddressMediatorTest, DidTapAutofillFormButton) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  constexpr NSInteger kCellIndex = 2;
  OCMExpect([content_injector()
      autofillFormWithSuggestion:[OCMArg checkWithBlock:^BOOL(
                                             FormSuggestion* suggestion) {
        EXPECT_EQ(suggestion.type, autofill::SuggestionType::kAddressEntry);
        EXPECT_EQ(
            suggestion.payload,
            autofill::Suggestion::Payload(
                autofill::Suggestion::AutofillProfilePayload(
                    autofill::Suggestion::Guid(std::string(kAddressGuid)))));
        EXPECT_EQ(suggestion.fieldByFieldFillingTypeUsed, autofill::EMPTY_TYPE);
        EXPECT_FALSE(suggestion.requiresReauth);
        EXPECT_NSEQ(
            suggestion.acceptanceA11yAnnouncement,
            l10n_util::GetNSString(IDS_AUTOFILL_A11Y_ANNOUNCE_FILLED_FORM));
        return YES;
      }]
                         atIndex:kCellIndex]);

  [mediator() didTapAutofillFormButtonForAddress:CreateTestAddress()
                                       cellIndex:kCellIndex];

  EXPECT_OCMOCK_VERIFY(content_injector());
  histogram_tester.ExpectUniqueSample(kAcceptedSuggestionHistogram, kCellIndex,
                                      1);
  EXPECT_EQ(user_action_tester.GetActionCount(kSuggestionAcceptedAction), 1);
}
