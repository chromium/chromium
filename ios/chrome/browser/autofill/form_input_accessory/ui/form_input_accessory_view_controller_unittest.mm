// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/form_input_accessory/ui/form_input_accessory_view_controller.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "ios/chrome/browser/autofill/form_input_accessory/public/autofill_suggestion_context_menu_handler.h"
#import "ios/chrome/browser/autofill/form_input_accessory/ui/form_input_accessory_view_controller+testing.h"
#import "ios/chrome/browser/autofill/form_input_accessory/ui/form_input_accessory_view_controller_delegate.h"
#import "ios/chrome/browser/autofill/form_input_accessory/ui/form_suggestion_label.h"
#import "ios/chrome/browser/autofill/form_input_accessory/ui/form_suggestion_view.h"
#import "ios/chrome/browser/autofill/model/features.h"
#import "ios/chrome/browser/autofill/ui_bundled/branding/branding_view_controller.h"
#import "ios/chrome/common/ui/elements/form_input_accessory_view.h"
#import "ios/chrome/common/ui/elements/form_input_accessory_view_text_data.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

// Test constants.
NSString* const kPasskeyLabel = @"Passkey";
NSString* const kDefaultRPId = @"google.com";

NSString* const kUsernameBob1 = @"bob1";
NSString* const kUsernameBob2 = @"bob2";
NSString* const kUsernameBob = @"bob";
NSString* const kUsernameAlice = @"alice";

NSString* const kDisplayNameBob = @"Bob";
NSString* const kDisplayNameBobSmith = @"Bob Smith";
NSString* const kDisplayNameAliceJones = @"Alice Jones";

NSString* const kDescriptionBob1 = @"Passkey • bob1";
NSString* const kDescriptionBob2 = @"Passkey • bob2";

NSString* const kDescriptionBob1WithRPId = @"Passkey • bob1 • google.com";
NSString* const kDescriptionBob2WithRPId = @"Passkey • bob2 • google.com";

NSString* const kDelegateKey = @"formInputAccessoryViewControllerDelegate";

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
    [suggestions addObject:SimpleFormSuggestion(
                               base::NumberToString16(i),
                               autofill::SuggestionType::kAutocompleteEntry)];
  }
  return suggestions;
}

// Returns a passkey form suggestion.
FormSuggestion* PasskeyFormSuggestion(NSString* username,
                                      NSString* displayName,
                                      NSString* rpId) {
  NSString* value = displayName.length ? displayName : username;
  NSString* displayDescription = kPasskeyLabel;
  if (displayName.length && ![displayName isEqualToString:username]) {
    displayDescription =
        [NSString stringWithFormat:@"%@ • %@", kPasskeyLabel, username];
  }
  return [FormSuggestion
              suggestionWithValue:value
                       minorValue:rpId
               displayDescription:displayDescription
                             icon:nil
                             type:autofill::SuggestionType::kWebauthnCredential
                          payload:autofill::Suggestion::Payload()
      fieldByFieldFillingTypeUsed:autofill::FieldType::EMPTY_TYPE
                   requiresReauth:YES
       acceptanceA11yAnnouncement:nil];
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
      u"value", autofill::SuggestionType::kAutocompleteEntry) ];

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
      u"value", autofill::SuggestionType::kAutocompleteEntry) ];

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

// Tests that empty suggestions with no value, no display description, and no
// icon are filtered out.
TEST_F(FormInputAccessoryViewControllerTest,
       ShowAccessorySuggestions_FiltersEmptySuggestions) {
  id mock_view_controller = OCMPartialMock(view_controller_);

  FormSuggestion* emptySuggestion = [FormSuggestion
      suggestionWithValue:@""
       displayDescription:@""
                     icon:nil
                     type:autofill::SuggestionType::kSaveAndFillCreditCardEntry
                  payload:autofill::Suggestion::Payload()
           requiresReauth:NO];

  FormSuggestion* validSuggestion = SimpleFormSuggestion(
      u"Valid", autofill::SuggestionType::kAutocompleteEntry);

  OCMExpect([mock_view_controller
      updateFormSuggestionView:[OCMArg checkWithBlock:^BOOL(
                                           NSArray* suggestions) {
        return suggestions.count == 1 &&
               [suggestions containsObject:validSuggestion];
      }]]);

  [mock_view_controller
      showAccessorySuggestions:@[ emptySuggestion, validSuggestion ]];

  EXPECT_OCMOCK_VERIFY(mock_view_controller);
}

// Tests that suggestions with an empty value but a non-empty display
// description are not filtered out.
TEST_F(FormInputAccessoryViewControllerTest,
       ShowAccessorySuggestions_KeepsSuggestionsWithDisplayDescription) {
  id mock_view_controller = OCMPartialMock(view_controller_);

  FormSuggestion* suggestionWithDescription = [FormSuggestion
      suggestionWithValue:@""
       displayDescription:@"Description"
                     icon:nil
                     type:autofill::SuggestionType::kAddressEntry
                  payload:autofill::Suggestion::Payload()
           requiresReauth:NO];

  OCMExpect([mock_view_controller
      updateFormSuggestionView:[OCMArg checkWithBlock:^BOOL(
                                           NSArray* suggestions) {
        return suggestions.count == 1 &&
               [suggestions containsObject:suggestionWithDescription];
      }]]);

  [mock_view_controller
      showAccessorySuggestions:@[ suggestionWithDescription ]];

  EXPECT_OCMOCK_VERIFY(mock_view_controller);
}

// Tests that when the only suggestion is kAutocompleteAtMemoryButton, manual
// fill buttons are shown instead of the expand button.
TEST_F(FormInputAccessoryViewControllerTest,
       ShowAccessorySuggestions_OnlyAtMemorySuggestionShowsManualFillButtons) {
  FormInputAccessoryView* accessory_view =
      base::apple::ObjCCastStrict<FormInputAccessoryView>(
          view_controller_.view);

  FormSuggestion* atMemorySuggestion = SimpleFormSuggestion(
      u"AtMemory", autofill::SuggestionType::kAutocompleteAtMemoryButton);

  [view_controller_ showAccessorySuggestions:@[ atMemorySuggestion ]];

  EXPECT_EQ(accessory_view.currentGroup,
            FormInputAccessoryViewSubitemGroup::kManualFillButtons);
}

// Tests that special suggestions (AtMemory and Ambient) are preserved when
// suggestions exceed the limit.
TEST_F(FormInputAccessoryViewControllerTest,
       ShowAccessorySuggestions_SpecialSuggestionsPreservedWhenTruncated) {
  id mock_view_controller = OCMPartialMock(view_controller_);

  NSMutableArray<FormSuggestion*>* suggestions =
      [SimpleFormSuggestions(kKeyboardAccessorySuggestionsLimit + 5)
          mutableCopy];

  FormSuggestion* ambientSuggestion = SimpleFormSuggestion(
      u"Ambient", autofill::SuggestionType::kFetchingAmbientData);
  FormSuggestion* atMemorySuggestion = SimpleFormSuggestion(
      u"AtMemory", autofill::SuggestionType::kAutocompleteAtMemoryButton);

  [suggestions addObject:ambientSuggestion];
  [suggestions addObject:atMemorySuggestion];

  OCMExpect([mock_view_controller
      updateFormSuggestionView:[OCMArg checkWithBlock:^BOOL(
                                           NSArray* resultSuggestions) {
        return resultSuggestions.count ==
                   kKeyboardAccessorySuggestionsLimit + 2 &&
               [resultSuggestions containsObject:ambientSuggestion] &&
               [resultSuggestions containsObject:atMemorySuggestion];
      }]]);

  [mock_view_controller showAccessorySuggestions:suggestions];

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

// Tests that duplicate passkey suggestions have their username appended to
// their display descriptions.
TEST_F(FormInputAccessoryViewControllerTest,
       PasskeySuggestionDisplayDescriptionDuplicateHandling) {
  FormSuggestion* suggestion1 =
      PasskeyFormSuggestion(kUsernameBob1, kDisplayNameBob, kDefaultRPId);
  FormSuggestion* suggestion2 =
      PasskeyFormSuggestion(kUsernameBob2, kDisplayNameBob, kDefaultRPId);

  id delegate_mock =
      OCMProtocolMock(@protocol(FormInputAccessoryViewControllerDelegate));
  [view_controller_ setValue:delegate_mock forKey:kDelegateKey];

  OCMStub([delegate_mock formInputAccessoryViewController:view_controller_
                                    usernameForSuggestion:suggestion1])
      .andReturn(kUsernameBob1);
  OCMStub([delegate_mock formInputAccessoryViewController:view_controller_
                                    usernameForSuggestion:suggestion2])
      .andReturn(kUsernameBob2);

  NSArray<FormSuggestion*>* suggestions = @[ suggestion1, suggestion2 ];
  [view_controller_ showAccessorySuggestions:suggestions];

  FormSuggestionView* suggestion_view = view_controller_.formSuggestionView;
  EXPECT_NE(suggestion_view, nil);

  NSString* desc1 = [(id<FormSuggestionLabelDelegate>)suggestion_view
      displayDescriptionForSuggestion:suggestion1];
  NSString* desc2 = [(id<FormSuggestionLabelDelegate>)suggestion_view
      displayDescriptionForSuggestion:suggestion2];

  EXPECT_NSEQ(desc1, kDescriptionBob1);
  EXPECT_NSEQ(desc2, kDescriptionBob2);
}

// Tests that duplicate passkey suggestions with RP ID shown append both their
// username and RP ID to their display descriptions.
TEST_F(FormInputAccessoryViewControllerTest,
       PasskeySuggestionDisplayDescriptionDuplicateHandlingAndRPId) {
  FormSuggestion* suggestion1 =
      PasskeyFormSuggestion(kUsernameBob1, kDisplayNameBob, kDefaultRPId);
  FormSuggestion* suggestion2 =
      PasskeyFormSuggestion(kUsernameBob2, kDisplayNameBob, kDefaultRPId);

  id delegate_mock =
      OCMProtocolMock(@protocol(FormInputAccessoryViewControllerDelegate));
  [view_controller_ setValue:delegate_mock forKey:kDelegateKey];

  OCMStub([delegate_mock formInputAccessoryViewController:view_controller_
                                    usernameForSuggestion:suggestion1])
      .andReturn(kUsernameBob1);
  OCMStub([delegate_mock formInputAccessoryViewController:view_controller_
                                    usernameForSuggestion:suggestion2])
      .andReturn(kUsernameBob2);
  OCMStub([delegate_mock formInputAccessoryViewController:view_controller_
                                           shouldShowRPId:kDefaultRPId])
      .andReturn(YES);

  NSArray<FormSuggestion*>* suggestions = @[ suggestion1, suggestion2 ];
  [view_controller_ showAccessorySuggestions:suggestions];

  FormSuggestionView* suggestion_view = view_controller_.formSuggestionView;
  EXPECT_NE(suggestion_view, nil);

  NSString* desc1 = [(id<FormSuggestionLabelDelegate>)suggestion_view
      displayDescriptionForSuggestion:suggestion1];
  NSString* desc2 = [(id<FormSuggestionLabelDelegate>)suggestion_view
      displayDescriptionForSuggestion:suggestion2];

  EXPECT_NSEQ(desc1, kDescriptionBob1WithRPId);
  EXPECT_NSEQ(desc2, kDescriptionBob2WithRPId);
}

// Tests that duplicate passkey suggestions with no display name append their
// usernames to distinguish themselves.
TEST_F(FormInputAccessoryViewControllerTest,
       PasskeySuggestionDisplayDescriptionDuplicateHandling_NoDisplayName) {
  FormSuggestion* suggestion1 =
      PasskeyFormSuggestion(kUsernameBob1, @"", kDefaultRPId);
  FormSuggestion* suggestion2 =
      PasskeyFormSuggestion(kUsernameBob2, @"", kDefaultRPId);

  id delegate_mock =
      OCMProtocolMock(@protocol(FormInputAccessoryViewControllerDelegate));
  [view_controller_ setValue:delegate_mock forKey:kDelegateKey];

  OCMStub([delegate_mock formInputAccessoryViewController:view_controller_
                                    usernameForSuggestion:suggestion1])
      .andReturn(kUsernameBob1);
  OCMStub([delegate_mock formInputAccessoryViewController:view_controller_
                                    usernameForSuggestion:suggestion2])
      .andReturn(kUsernameBob2);

  NSArray<FormSuggestion*>* suggestions = @[ suggestion1, suggestion2 ];
  [view_controller_ showAccessorySuggestions:suggestions];

  FormSuggestionView* suggestion_view = view_controller_.formSuggestionView;
  EXPECT_NE(suggestion_view, nil);

  NSString* desc1 = [(id<FormSuggestionLabelDelegate>)suggestion_view
      displayDescriptionForSuggestion:suggestion1];
  NSString* desc2 = [(id<FormSuggestionLabelDelegate>)suggestion_view
      displayDescriptionForSuggestion:suggestion2];

  EXPECT_NSEQ(desc1, kPasskeyLabel);
  EXPECT_NSEQ(desc2, kPasskeyLabel);
}

// Tests that non-duplicate passkey suggestions (suggestions with different
// values but the same default description "Passkey") do not have their
// usernames appended.
TEST_F(FormInputAccessoryViewControllerTest,
       PasskeySuggestionDisplayDescriptionNoDuplicates) {
  FormSuggestion* suggestion1 =
      PasskeyFormSuggestion(kUsernameBob, kDisplayNameBobSmith, kDefaultRPId);
  FormSuggestion* suggestion2 = PasskeyFormSuggestion(
      kUsernameAlice, kDisplayNameAliceJones, kDefaultRPId);

  id delegate_mock =
      OCMProtocolMock(@protocol(FormInputAccessoryViewControllerDelegate));
  [view_controller_ setValue:delegate_mock forKey:kDelegateKey];

  OCMStub([delegate_mock formInputAccessoryViewController:view_controller_
                                    usernameForSuggestion:suggestion1])
      .andReturn(kUsernameBob);
  OCMStub([delegate_mock formInputAccessoryViewController:view_controller_
                                    usernameForSuggestion:suggestion2])
      .andReturn(kUsernameAlice);

  NSArray<FormSuggestion*>* suggestions = @[ suggestion1, suggestion2 ];
  [view_controller_ showAccessorySuggestions:suggestions];

  FormSuggestionView* suggestion_view = view_controller_.formSuggestionView;
  EXPECT_NE(suggestion_view, nil);

  NSString* desc1 = [(id<FormSuggestionLabelDelegate>)suggestion_view
      displayDescriptionForSuggestion:suggestion1];
  NSString* desc2 = [(id<FormSuggestionLabelDelegate>)suggestion_view
      displayDescriptionForSuggestion:suggestion2];

  // Since they have different values, they are not duplicates, so they keep
  // their default description "Passkey".
  EXPECT_NSEQ(desc1, kPasskeyLabel);
  EXPECT_NSEQ(desc2, kPasskeyLabel);
}

// Tests that FormInputAccessoryViewController forwards
// hasSourcesForSuggestion: to the context menu handler.
TEST_F(FormInputAccessoryViewControllerTest, HasSourcesForSuggestion) {
  id context_menu_handler =
      OCMProtocolMock(@protocol(AutofillSuggestionContextMenuHandler));
  view_controller_.contextMenuHandler = context_menu_handler;

  FormSuggestion* suggestion = SimpleFormSuggestion(
      u"Test Suggestion", autofill::SuggestionType::kFillAutofillAi);

  OCMStub([context_menu_handler hasSourcesForSuggestion:suggestion])
      .andReturn(YES);

  EXPECT_TRUE([(id<FormSuggestionViewDelegate>)view_controller_
      hasSourcesForSuggestion:suggestion]);
}

// Tests that FormInputAccessoryViewController forwards
// openSourcesForSuggestion: to the context menu handler.
TEST_F(FormInputAccessoryViewControllerTest, OpenSourcesForSuggestion) {
  id context_menu_handler =
      OCMProtocolMock(@protocol(AutofillSuggestionContextMenuHandler));
  view_controller_.contextMenuHandler = context_menu_handler;

  FormSuggestion* suggestion = SimpleFormSuggestion(
      u"Test Suggestion", autofill::SuggestionType::kFillAutofillAi);

  OCMExpect([context_menu_handler openSourcesForSuggestion:suggestion]);

  [(id<FormSuggestionViewDelegate>)view_controller_
      openSourcesForSuggestion:suggestion];

  EXPECT_OCMOCK_VERIFY(context_menu_handler);
}
