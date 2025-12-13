// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/form_input_accessory/form_input_accessory_view_controller.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "ios/chrome/browser/autofill/ui_bundled/branding/branding_view_controller.h"
#import "ios/chrome/common/ui/elements/form_input_accessory_view.h"
#import "testing/platform_test.h"

namespace {

// Returns whether the filling product exists on iOS.
bool IsAvailableOnIos(autofill::FillingProduct filling_product) {
  switch (filling_product) {
    case autofill::FillingProduct::kAddress:
    case autofill::FillingProduct::kPlusAddresses:
    case autofill::FillingProduct::kCreditCard:
    case autofill::FillingProduct::kIban:
    case autofill::FillingProduct::kPassword:
    case autofill::FillingProduct::kAutocomplete:
    // Note: There shouldn't be any suggestion of these 3 types below on iOS,
    // but they technically exist on iOS.
    case autofill::FillingProduct::kDataList:
    case autofill::FillingProduct::kPasskey:
    case autofill::FillingProduct::kNone:
      return true;
    case autofill::FillingProduct::kCompose:
    case autofill::FillingProduct::kAutofillAi:
    case autofill::FillingProduct::kMerchantPromoCode:
    case autofill::FillingProduct::kLoyaltyCard:
    case autofill::FillingProduct::kIdentityCredential:
    case autofill::FillingProduct::kOneTimePassword:
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

}  // namespace

using FormInputAccessoryViewControllerTest = PlatformTest;

// Tests FormInputAccessoryViewController can be initiliazed.
TEST_F(FormInputAccessoryViewControllerTest, Init) {
  FormInputAccessoryViewController* view_controller =
      [[FormInputAccessoryViewController alloc]
          initWithFormInputAccessoryViewControllerDelegate:nil];
  EXPECT_TRUE(view_controller);
}

// Tests FormInputAccessoryViewController can press the manual fill button with
// any filling product that's available on iOS when that button is accessible.
TEST_F(FormInputAccessoryViewControllerTest, ManualFillButtonPress) {
  FormInputAccessoryViewController* view_controller =
      [[FormInputAccessoryViewController alloc]
          initWithFormInputAccessoryViewControllerDelegate:nil];
  EXPECT_TRUE(view_controller);

  view_controller.brandingViewController =
      [[BrandingViewController alloc] init];
  [view_controller loadView];
  FormInputAccessoryView* accessory_view =
      base::apple::ObjCCastStrict<FormInputAccessoryView>(view_controller.view);

  NSArray<FormSuggestion*>* suggestions = @[ SimpleFormSuggestion(
      u"", autofill::SuggestionType::kAutocompleteEntry) ];

  for (int i = static_cast<int>(autofill::FillingProduct::kNone);
       i <= static_cast<int>(autofill::FillingProduct::kMaxValue); ++i) {
    autofill::FillingProduct filling_product =
        static_cast<autofill::FillingProduct>(i);
    if (IsAvailableOnIos(filling_product)) {
      view_controller.mainFillingProduct = filling_product;
      [view_controller showAccessorySuggestions:suggestions];
      if (accessory_view.currentGroup ==
          FormInputAccessoryViewSubitemGroup::kExpandButton) {
        [view_controller manualFillButtonPressed:nil];
      }
    }
  }
}
