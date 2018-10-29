// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#import "ios/chrome/browser/autofill/automation/automation_action.h"

#include "base/guid.h"
#include "base/mac/foundation_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/ios/browser/autofill_driver_ios.h"
#import "ios/chrome/browser/autofill/form_suggestion_label.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/web/public/test/earl_grey/web_view_actions.h"
#import "ios/web/public/test/earl_grey/web_view_matchers.h"
#include "ios/web/public/test/element_selector.h"
#import "ios/web/public/test/js_test_util.h"
#include "ios/web/public/web_state/web_frame_util.h"
#import "ios/web/public/web_state/web_frames_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using web::test::ElementSelector;

@interface AutomationAction () {
  std::unique_ptr<const base::DictionaryValue> actionDictionary_;
}

@property(nonatomic, readonly)
    const std::unique_ptr<const base::DictionaryValue>& actionDictionary;

// Selects the proper subclass in the class cluster for the given type. Called
// from the class method creating the actions.
+ (Class)classForType:(NSString*)type;

- (instancetype)initWithValueDictionary:
    (const base::DictionaryValue&)actionDictionary NS_DESIGNATED_INITIALIZER;
@end

// An action that always fails.
@interface AutomationActionUnrecognized : AutomationAction
@end

// An action that simply tap on an element on the page.
// Right now this always assumes a click event of the following format:
//  {
//    "selectorType": "xpath",
//    "selector": "//*[@id=\"add-to-cart-button\"]",
//    "context": [],
//    "type": "click"
//  }
@interface AutomationActionClick : AutomationAction
@end

// An action that waits for a series of JS assertions to become true before
// continuing. We assume this action has a format resembling:
// {
//   "context": [],
//   "type": "waitFor",
//   "assertions": ["return document.querySelector().style.display ===
//   'none';"]
// }
@interface AutomationActionWaitFor : AutomationAction
@end

// An action that performs autofill on a form by selecting an element
// that is part of an autofillable form, then tapping the relevant
// autofill suggestion. We assume this action has a format resembling:
// {
//   "selectorType": "xpath",
//   "selector": "//*[@data-tl-id=\"COAC2ShpAddrFirstName\"]",
//   "context": [],
//   "type": "autofill"
// }
@interface AutomationActionAutofill : AutomationAction
@end

// An action that validates a previously autofilled element.
// Confirms that the target element has is of the expected autofill field type
// and contains the specified value.
// We assume this action has a format resembling:
// {
//   "selectorType": "xpath",
//   "selector": "//*[@data-tl-id=\"COAC2ShpAddrFirstName\"]",
//   "context": [],
//   "expectedAutofillType": "NAME_FIRST",
//   "expectedValue": "Yuki",
//   "type": "validateField"
// }
@interface AutomationActionValidateField : AutomationAction
@end

// An action that selects a given option from a dropdown selector.
// Checks are not made to confirm that given item is a dropdown.
// We assume this action has a format resembling:
// {
//   "selectorType": "xpath",
//   "selector": "//*[@id=\"shipping-user-lookup-options\"]",
//   "context": [],
//   "index": 1,
//   "type": "select"
// }
@interface AutomationActionSelectDropdown : AutomationAction
@end

@implementation AutomationAction

+ (instancetype)actionWithValueDictionary:
    (const base::DictionaryValue&)actionDictionary {
  const base::Value* typeValue =
      actionDictionary.FindKeyOfType("type", base::Value::Type::STRING);
  GREYAssert(typeValue, @"Type is missing in action.");

  const std::string type(typeValue->GetString());
  GREYAssert(!type.empty(), @"Type is an empty value.");

  return [[[self classForType:base::SysUTF8ToNSString(type)] alloc]
      initWithValueDictionary:actionDictionary];
}

+ (Class)classForType:(NSString*)type {
  static NSDictionary* classForType = @{
    @"click" : [AutomationActionClick class],
    @"waitFor" : [AutomationActionWaitFor class],
    @"autofill" : [AutomationActionAutofill class],
    @"validateField" : [AutomationActionValidateField class],
    @"select" : [AutomationActionSelectDropdown class],
    // More to come.
  };

  return classForType[type] ?: [AutomationActionUnrecognized class];
}

- (instancetype)initWithValueDictionary:
    (const base::DictionaryValue&)actionDictionary {
  self = [super init];
  if (self) {
    actionDictionary_ = actionDictionary.DeepCopyWithoutEmptyChildren();
  }
  return self;
}

- (void)execute {
  GREYAssert(NO, @"Should not be called!");
}

- (const std::unique_ptr<const base::DictionaryValue>&)actionDictionary {
  return actionDictionary_;
}

// A shared flow across many actions, this waits for the target element to be
// visible, scrolls it into view, then taps on it.
- (void)tapOnTarget:(web::test::ElementSelector)selector {
  web::WebState* web_state = chrome_test_util::GetCurrentWebState();

  // Wait for the element to be visible on the page.
  [ChromeEarlGrey waitForWebViewContainingElement:selector];
  // Potentially scroll into view if below the fold.
  [[EarlGrey selectElementWithMatcher:web::WebViewInWebState(web_state)]
      performAction:WebViewScrollElementToVisible(web_state, selector)];
  // Tap on the element.
  [[EarlGrey selectElementWithMatcher:web::WebViewInWebState(web_state)]
      performAction:web::WebViewTapElement(web_state, selector)];
}

// Creates a selector targeting the element specified in the action.
- (web::test::ElementSelector)selectorForTarget {
  const std::string xpath = [self getStringFromDictionaryWithKey:"selector"];

  // Creates a selector from the action dictionary.
  web::test::ElementSelector selector(
      ElementSelector::ElementSelectorXPath(xpath));
  return selector;
}

// Returns a std::string corrensponding to the given key in the action
// dictionary. Will raise a test failure if the key is missing or the value is
// empty.
- (std::string)getStringFromDictionaryWithKey:(std::string)key {
  const base::Value* expectedTypeValue(
      self.actionDictionary->FindKeyOfType(key, base::Value::Type::STRING));
  GREYAssert(expectedTypeValue, @"%s is missing in action.", key.c_str());

  const std::string expectedType(expectedTypeValue->GetString());
  GREYAssert(!expectedType.empty(), @"%s is an empty value", key.c_str());

  return expectedType;
}

// Returns an int corrensponding to the given key in the action
// dictionary. Will raise a test failure if the key is missing or the value is
// empty.
- (int)getIntFromDictionaryWithKey:(std::string)key {
  const base::Value* expectedTypeValue(
      self.actionDictionary->FindKeyOfType(key, base::Value::Type::INTEGER));
  GREYAssert(expectedTypeValue, @"%s is missing in action.", key.c_str());

  return expectedTypeValue->GetInt();
}

// Runs the JS code passed in against the target element specified by the
// selector passed in. The target element is passed in to the JS function
// by the name "target", so example JS code is like:
// return target.value
- (id)executeJavascript:(std::string)function
               onTarget:(web::test::ElementSelector)selector {
  NSError* error;

  id result = chrome_test_util::ExecuteJavaScript(
      [NSString
          stringWithFormat:@"    (function() {"
                            "      try {"
                            "        return function(target){%@}(%@);"
                            "      } catch (ex) {return 'Exception encountered "
                            "' + ex.message;}"
                            "     "
                            "    })();",
                           base::SysUTF8ToNSString(function),
                           base::SysUTF8ToNSString(
                               selector.GetSelectorScript())],
      &error);

  if (error) {
    GREYAssert(NO, @"Javascript execution error: %@", result);
    return nil;
  }
  return result;
}

@end

@implementation AutomationActionClick

- (void)execute {
  web::test::ElementSelector selector = [self selectorForTarget];
  [self tapOnTarget:selector];
}

@end

@implementation AutomationActionWaitFor

- (void)execute {
  const base::Value* assertionsValue(self.actionDictionary->FindKeyOfType(
      "assertions", base::Value::Type::LIST));
  GREYAssert(assertionsValue, @"Assertions key is missing in action.");

  const base::Value::ListStorage& assertionsValues(assertionsValue->GetList());
  GREYAssert(assertionsValues.size(), @"Assertions list is empty.");

  std::vector<std::string> state_assertions;

  for (auto const& assertionValue : assertionsValues) {
    const std::string assertionString(assertionValue.GetString());
    GREYAssert(!assertionString.empty(), @"assertionString is an empty value.");
    state_assertions.push_back(assertionString);
  }

  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForActionTimeout,
                 ^{
                   return [self CheckForJsAssertionFailures:state_assertions] ==
                          nil;
                 }),
             @"waitFor State change hasn't completed within timeout.");
}

// Executes a vector of Javascript assertions on the webpage, returning the
// first assertion that fails to be true, or nil if all assertions are true.
- (NSString*)CheckForJsAssertionFailures:
    (const std::vector<std::string>&)assertions {
  for (std::string const& assertion : assertions) {
    NSError* error;
    NSString* assertionString = base::SysUTF8ToNSString(assertion);

    NSNumber* result =
        base::mac::ObjCCastStrict<NSNumber>(chrome_test_util::ExecuteJavaScript(
            [NSString stringWithFormat:@""
                                        "    (function() {"
                                        "      try {"
                                        "        %@"
                                        "      } catch (ex) {}"
                                        "      return false;"
                                        "    })();",
                                       assertionString],
            &error));

    if (![result boolValue] || error) {
      return assertionString;
    }
  }
  return nil;
}

@end

@implementation AutomationActionAutofill

static const char PROFILE_NAME_FULL[] = "Milton C. Waddams";
static const char PROFILE_NAME_FIRST[] = "Milton";
static const char PROFILE_NAME_MIDDLE[] = "C.";
static const char PROFILE_NAME_LAST[] = "Waddams";
static const char PROFILE_HOME_LINE1[] = "4120 Freidrich Lane";
static const char PROFILE_HOME_LINE2[] = "Apt 8";
static const char PROFILE_HOME_CITY[] = "Austin";
static const char PROFILE_HOME_STATE[] = "Texas";
static const char PROFILE_COMPANY_NAME[] = "Initech";
static const char PROFILE_EMAIL_ADDRESS[] = "red.swingline@initech.com";
static const char PROFILE_HOME_ZIP[] = "78744";
static const char PROFILE_PHONE_HOME_CITY_CODE[] = "512";
static const char PROFILE_PHONE_HOME_WHOLE[] = "5125551234";
static const char PROFILE_CREDIT_CARD_NUMBER[] = "9621327911759602";
static const char PROFILE_CREDIT_CARD_NAME_FULL[] = "Milton Waddams";
static const char PROFILE_CREDIT_CARD_EXP_MONTH[] = "5";
static const char PROFILE_CREDIT_CARD_EXP_4_DIGIT_YEAR[] = "2027";

// Loads the predefined autofill profile into the personal data manager, so that
// autofill actions will be suggested when tapping on an autofillable form.
- (void)prepareAutofillProfileWithWebState:(web::WebState*)web_state {
  web::WebFrame* main_frame = web::GetMainWebFrame(web_state);
  autofill::AutofillManager* autofill_manager =
      autofill::AutofillDriverIOS::FromWebStateAndWebFrame(web_state,
                                                           main_frame)
          ->autofill_manager();
  autofill::PersonalDataManager* personal_data_manager =
      autofill_manager->client()->GetPersonalDataManager();
  autofill::AutofillProfile profile(base::GenerateGUID(),
                                    "https://www.example.com/");
  profile.SetRawInfo(autofill::NAME_FULL, base::UTF8ToUTF16(PROFILE_NAME_FULL));
  profile.SetRawInfo(autofill::NAME_FIRST,
                     base::UTF8ToUTF16(PROFILE_NAME_FIRST));
  profile.SetRawInfo(autofill::NAME_MIDDLE,
                     base::UTF8ToUTF16(PROFILE_NAME_MIDDLE));
  profile.SetRawInfo(autofill::NAME_LAST, base::UTF8ToUTF16(PROFILE_NAME_LAST));
  profile.SetRawInfo(autofill::ADDRESS_HOME_LINE1,
                     base::UTF8ToUTF16(PROFILE_HOME_LINE1));
  profile.SetRawInfo(autofill::ADDRESS_HOME_LINE2,
                     base::UTF8ToUTF16(PROFILE_HOME_LINE2));
  profile.SetRawInfo(autofill::ADDRESS_HOME_CITY,
                     base::UTF8ToUTF16(PROFILE_HOME_CITY));
  profile.SetRawInfo(autofill::ADDRESS_HOME_STATE,
                     base::UTF8ToUTF16(PROFILE_HOME_STATE));
  profile.SetRawInfo(autofill::COMPANY_NAME,
                     base::UTF8ToUTF16(PROFILE_COMPANY_NAME));
  profile.SetRawInfo(autofill::EMAIL_ADDRESS,
                     base::UTF8ToUTF16(PROFILE_EMAIL_ADDRESS));
  profile.SetRawInfo(autofill::ADDRESS_HOME_ZIP,
                     base::UTF8ToUTF16(PROFILE_HOME_ZIP));
  profile.SetRawInfo(autofill::PHONE_HOME_CITY_CODE,
                     base::UTF8ToUTF16(PROFILE_PHONE_HOME_CITY_CODE));
  profile.SetRawInfo(autofill::PHONE_HOME_WHOLE_NUMBER,
                     base::UTF8ToUTF16(PROFILE_PHONE_HOME_WHOLE));
  personal_data_manager->SaveImportedProfile(profile);

  autofill::CreditCard credit_card(base::GenerateGUID(),
                                   "https://www.example.com/");
  credit_card.SetRawInfo(autofill::CREDIT_CARD_NUMBER,
                         base::UTF8ToUTF16(PROFILE_CREDIT_CARD_NUMBER));
  credit_card.SetRawInfo(autofill::CREDIT_CARD_NAME_FULL,
                         base::UTF8ToUTF16(PROFILE_CREDIT_CARD_NAME_FULL));
  credit_card.SetRawInfo(autofill::CREDIT_CARD_EXP_MONTH,
                         base::UTF8ToUTF16(PROFILE_CREDIT_CARD_EXP_MONTH));
  credit_card.SetRawInfo(
      autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR,
      base::UTF8ToUTF16(PROFILE_CREDIT_CARD_EXP_4_DIGIT_YEAR));
  personal_data_manager->AddCreditCard(credit_card);
}

- (void)execute {
  web::WebState* web_state = chrome_test_util::GetCurrentWebState();
  [self prepareAutofillProfileWithWebState:web_state];

  web::test::ElementSelector selector = [self selectorForTarget];
  [self tapOnTarget:selector];

  // Tap on the autofill suggestion to perform the actual autofill.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kFormSuggestionLabelAccessibilityIdentifier)]
      performAction:grey_tap()];
}

@end

@implementation AutomationActionValidateField

- (void)execute {
  web::test::ElementSelector selector = [self selectorForTarget];

  // Wait for the element to be visible on the page.
  [ChromeEarlGrey waitForWebViewContainingElement:selector];

  NSString* expectedType = base::SysUTF8ToNSString(
      [self getStringFromDictionaryWithKey:"expectedAutofillType"]);
  NSString* expectedValue = base::SysUTF8ToNSString(
      [self getStringFromDictionaryWithKey:"expectedValue"]);

  NSString* predictionType = base::mac::ObjCCastStrict<NSString>([self
      executeJavascript:"return target.getAttribute('placeholder');"
               onTarget:[self selectorForTarget]]);

  NSString* autofilledValue = base::mac::ObjCCastStrict<NSString>(
      [self executeJavascript:"return target.value;" onTarget:selector]);

  GREYAssertEqualObjects(predictionType, expectedType,
                         @"Expected prediction type %@ but got %@",
                         expectedType, predictionType);
  GREYAssertEqualObjects(autofilledValue, expectedValue,
                         @"Expected autofilled value %@ but got %@",
                         expectedValue, autofilledValue);
}

@end

@implementation AutomationActionSelectDropdown

- (void)execute {
  web::test::ElementSelector selector = [self selectorForTarget];

  // Wait for the element to be visible on the page.
  [ChromeEarlGrey waitForWebViewContainingElement:selector];

  int selectedIndex = [self getIntFromDictionaryWithKey:"index"];
  [self executeJavascript:
            base::SysNSStringToUTF8([NSString
                stringWithFormat:@"target.options.selectedIndex = %d; "
                                 @"triggerOnChangeEventOnElement(target);",
                                 selectedIndex])
                 onTarget:selector];
}

@end

@implementation AutomationActionUnrecognized

- (void)execute {
  const base::Value* typeValue =
      self.actionDictionary->FindKeyOfType("type", base::Value::Type::STRING);
  const std::string type(typeValue->GetString());

  GREYAssert(NO, @"Unknown action of type %s", type.c_str());
}

@end
