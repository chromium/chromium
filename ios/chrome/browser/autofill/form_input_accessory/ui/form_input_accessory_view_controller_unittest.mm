// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/form_input_accessory/ui/form_input_accessory_view_controller.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "ios/chrome/browser/autofill/form_input_accessory/ui/form_input_accessory_view_controller+testing.h"
#import "ios/chrome/browser/autofill/model/features.h"
#import "ios/chrome/browser/autofill/ui_bundled/branding/branding_view_controller.h"
#import "ios/chrome/common/ui/elements/form_input_accessory_view.h"
#import "ios/chrome/common/ui/elements/form_input_accessory_view_text_data.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

// Returns whether the filling product exists on iOS.
bool IsAvailableOnIos(autofill::FillingProduct filling_product) {
  switch (filling_product) {
    case autofill::FillingProduct::kAddress:
    case autofill::FillingProduct::kCreditCard:
    case autofill::FillingProduct::kIban:
    case autofill::FillingProduct::kPassword:
    case autofill::FillingProduct::kAutocomplete:
    case autofill::FillingProduct::kAutofillAi:
    // Note: There shouldn't be any suggestion of these 3 types below on iOS,
    // but they technically exist on iOS.
    case autofill::FillingProduct::kDataList:
    case autofill::FillingProduct::kPasskey:
    case autofill::FillingProduct::kNone:
      return true;
    case autofill::FillingProduct::kCompose:
    case autofill::FillingProduct::kMerchantPromoCode:
    case autofill::FillingProduct::kLoyaltyCard:
    case autofill::FillingProduct::kIdentityCredential:
    case autofill::FillingProduct::kOneTimePassword:
    case autofill::FillingProduct::kAtMemory:
      return false;
  }
}

// Returns a simple form suggestion that only consists of a `value` and a
// `type`.
FormSuggestion* SimpleFormSuggestion(std::u16string value,
                                     autofill::SuggestionType type) {
  return [FormSuggestion suggestionWithValue:base::SysUTF16ToNSString(value)
                          displayDescription:@""
                                        icon:nil
                                        type:type
                                     payload:autofill::Suggestion::Payload()
                              requiresReauth:NO];
}

// Returns an array of `count` simple form suggestions.
NSArray<FormSuggestion*>* SimpleFormSuggestions(int count) {
  NSMutableArray<FormSuggestion*>* suggestions = [NSMutableArray array];
  for (int i = 0; i < count; i++) {
    [suggestions
        addObject:SimpleFormSuggestion(
                      u"", autofill::SuggestionType::kAutocompleteEntry)];
  }
  return suggestions;
}

}  // namespace

class FormInputAccessoryViewControllerTest : public PlatformTest {
 public:
  FormInputAccessoryViewControllerTest() {
    view_controller_ = [[FormInputAccessoryViewController alloc]
        initWithFormInputAccessoryViewControllerDelegate:nil];
    view_controller_.brandingViewController =
        [[BrandingViewController alloc] init];
    [view_controller_ loadView];
  }

 protected:
  FormInputAccessoryViewController* view_controller_;
};

// Tests FormInputAccessoryViewController can press the manual fill button with
// any filling product that's available on iOS when that button is accessible.
TEST_F(FormInputAccessoryViewControllerTest, ManualFillButtonPress) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{autofill::features::kAutofillAiWithDataSchema},
      /*disabled_features=*/{});

  FormInputAccessoryView* accessory_view =
      base::apple::ObjCCastStrict<FormInputAccessoryView>(
          view_controller_.view);

  NSArray<FormSuggestion*>* suggestions = @[ SimpleFormSuggestion(
      u"", autofill::SuggestionType::kAutocompleteEntry) ];

  for (autofill::FillingProduct filling_product :
       autofill::FillingProductSet::all()) {
    if (IsAvailableOnIos(filling_product)) {
      view_controller_.mainFillingProduct = filling_product;
      [view_controller_ showAccessorySuggestions:suggestions];
      if (accessory_view.currentGroup ==
          FormInputAccessoryViewSubitemGroup::kExpandButton) {
        [view_controller_ manualFillButtonPressed:nil];
      }
    }
  }
}

// Tests that the manual fill button is hidden when the main filling product is
// set to a product that maps to ManualFillDataType::kOther.
TEST_F(FormInputAccessoryViewControllerTest, ManualFillButtonHiddenForOther) {
  id delegate_mock = OCMProtocolMock(@protocol(FormInputAccessoryViewDelegate));
  id text_data_mock = OCMClassMock([FormInputAccessoryViewTextData class]);
  OCMStub([delegate_mock textDataforFormInputAccessoryView:[OCMArg any]])
      .andReturn(text_data_mock);

  FormInputAccessoryViewController* controller =
      [[FormInputAccessoryViewController alloc]
          initWithFormInputAccessoryViewControllerDelegate:nil];
  controller.brandingViewController = [[BrandingViewController alloc] init];
  controller.navigationDelegate = delegate_mock;
  [controller loadView];

  FormInputAccessoryView* accessory_view =
      base::apple::ObjCCastStrict<FormInputAccessoryView>(controller.view);

  NSArray<FormSuggestion*>* suggestions = @[ SimpleFormSuggestion(
      u"", autofill::SuggestionType::kAutocompleteEntry) ];

  controller.mainFillingProduct = autofill::FillingProduct::kAutocomplete;
  [controller showAccessorySuggestions:suggestions];

  EXPECT_NE(accessory_view.manualFillButton, nil);
  EXPECT_TRUE(accessory_view.manualFillButton.hidden);
}

// Tests that the number of suggestions to show is capped at
// kKeyboardAccessorySuggestionsLimit.
TEST_F(FormInputAccessoryViewControllerTest,
       ShowAccessorySuggestions_CappedAtLimit) {
  id mock_view_controller = OCMPartialMock(view_controller_);

  NSArray<FormSuggestion*>* manySuggestions =
      SimpleFormSuggestions(kKeyboardAccessorySuggestionsLimit + 1);

  OCMExpect([mock_view_controller
      updateFormSuggestionView:[OCMArg checkWithBlock:^BOOL(
                                           NSArray* suggestions) {
        return suggestions.count == kKeyboardAccessorySuggestionsLimit;
      }]]);

  [mock_view_controller showAccessorySuggestions:manySuggestions];

  EXPECT_OCMOCK_VERIFY(mock_view_controller);
}

// Tests that updateFormSuggestionView takes less than a threshold with the
// amount of suggestions we intend to support. Updating suggestions should be
// done within this threshold to maintain smooth UI animations.
TEST_F(FormInputAccessoryViewControllerTest,
       UpdateFormSuggestionViewPerformance) {
  // 20ms is 1/60 of a second rounding up to the nearest tenth of a second.
  // 5ms is added to account for slower testing computers.
  base::TimeDelta threshold = base::Milliseconds(25);

  NSArray<FormSuggestion*>* suggestions =
      SimpleFormSuggestions(kKeyboardAccessorySuggestionsLimit);

  base::TimeTicks start = base::TimeTicks::Now();
  [view_controller_ updateFormSuggestionView:suggestions];
  base::TimeDelta duration = base::TimeTicks::Now() - start;

  EXPECT_LT(duration, threshold);
}
