// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <string>

#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_app_interface.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/intelligence/actor/tools/test/actor_app_interface.h"
#import "ios/chrome/browser/intelligence/actor/tools/test/actor_tools_base_test_case.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "url/gurl.h"

namespace {

constexpr std::string_view kProfileFormHTML = R"(
Profile form.
<form name='profile_form' id='profile_form'>
  Name: <input type='text' name='name' id='name'><br/>
  Address: <input type='text' name='address' id='address'><br/>
  City: <input type='text' name='city' id='city'><br/>
  State: <input type='text' name='state' id='state'><br/>
  Zip: <input type='text' name='zip' id='zip'><br/>
</form>
)";

constexpr std::string_view kCreditCardFormHTML = R"(
Credit Card form.
<form name='cc_form' id='cc_form'>
  Name on card: <input type='text' name='CCName' id='CCName'><br/>
  Credit card number: <input type='text' name='CCNo' id='CCNo'><br/>
  Expiry Date: <input type='text' name='CCExpiresMonth' id='CCExpiresMonth'> /
    <input type='text' name='CCExpiresYear' id='CCExpiresYear'><br/>
  CVC: <input type='text' name='cvc' id='cvc'><br/>
</form>
)";

// Returns HTML containing a profile form in the main frame and an iframe
// pointing to `iframe_src`.
std::string GetProfileFormWithIframeHTML(const GURL& iframe_src) {
  return base::StringPrintf("%s<br/>"
                            "<iframe id='subframe' src='%s'></iframe>",
                            kProfileFormHTML.data(), iframe_src.spec().c_str());
}

// Returns the FindNodeResult for the "name" form field inside the subframe of
// `page_context`.
FindNodeResult FindFormFieldInSubframe(
    const optimization_guide::proto::PageContext& page_context) {
  FindNodeResult iframe_result = FindNodeWithPredicate(
      page_context.annotated_page_content().root_node(),
      [](const optimization_guide::proto::ContentNode& node) {
        return node.content_attributes().has_iframe_data();
      },
      "");
  if (!iframe_result.node) {
    return {};
  }
  std::string subframe_token = iframe_result.node->content_attributes()
                                   .iframe_data()
                                   .frame_data()
                                   .document_identifier()
                                   .serialized_token();
  return FindNodeWithPredicate(
      *iframe_result.node,
      [](const optimization_guide::proto::ContentNode& node) {
        return node.content_attributes().has_form_control_data() &&
               node.content_attributes().form_control_data().field_name() ==
                   "name";
      },
      subframe_token);
}

// Returns the FindNodeResult for a form field matching `field_name`
// inside the main frame of `page_context`.
FindNodeResult FindFormFieldInMainFrame(
    const optimization_guide::proto::PageContext& page_context,
    const std::string& field_name) {
  std::string main_frame_token = page_context.annotated_page_content()
                                     .main_frame_data()
                                     .document_identifier()
                                     .serialized_token();
  return FindNodeWithPredicate(
      page_context.annotated_page_content().root_node(),
      [&field_name](const optimization_guide::proto::ContentNode& node) {
        return node.content_attributes().has_form_control_data() &&
               node.content_attributes().form_control_data().field_name() ==
                   field_name;
      },
      main_frame_token);
}

// Sets the content node ID and document identifier on `target` from `result`.
void SetTargetFromNodeResult(optimization_guide::proto::ActionTarget* target,
                             const FindNodeResult& result) {
  if (result.node) {
    target->set_content_node_id(
        result.node->content_attributes().common_ancestor_dom_node_id());
  }
  target->mutable_document_identifier()->set_serialized_token(
      result.frame_token);
}

}  // namespace

@interface ActorAttemptFormFillingToolTestCase : ActorToolsBaseTestCase
@end

@implementation ActorAttemptFormFillingToolTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_enabled.push_back(autofill::features::kGlicActorAutofill);
  return config;
}

- (void)setUp {
  [super setUp];
  [AutofillAppInterface clearProfilesStore];
  [AutofillAppInterface clearCreditCardStore];
}

- (void)tearDownHelper {
  [AutofillAppInterface clearProfilesStore];
  [AutofillAppInterface clearCreditCardStore];
  [super tearDownHelper];
}

#pragma mark - Helpers

// Verifies that the credit card form is filled with the sample credit card
// data.
- (void)verifyCreditCardFormFilled {
  NSString* condition = [NSString
      stringWithFormat:@"document.getElementById('CCName')?.value === '%@' && "
                        "document.getElementById('CCNo')?.value === '%@'",
                       [AutofillAppInterface exampleCreditCardName],
                       [AutofillAppInterface exampleCreditCardNumber]];
  [ChromeEarlGrey waitForJavaScriptCondition:condition];
}

// Verifies that the profile form in the main frame is filled with the sample
// profile data.
- (void)verifyProfileFormFilledInMainFrame {
  NSString* condition = [NSString
      stringWithFormat:@"document.getElementById('name')?.value === '%@' && "
                        "document.getElementById('address')?.value === '%@' && "
                        "document.getElementById('city')?.value === '%@' && "
                        "document.getElementById('state')?.value === '%@' && "
                        "document.getElementById('zip')?.value === '%@'",
                       [AutofillAppInterface exampleProfileName],
                       [AutofillAppInterface exampleProfileAddress],
                       [AutofillAppInterface exampleProfileCity],
                       [AutofillAppInterface exampleProfileState],
                       [AutofillAppInterface exampleProfileZip]];
  [ChromeEarlGrey waitForJavaScriptCondition:condition];
}

// Verifies that the profile form in the subframe is filled with the sample
// profile data.
- (void)verifyProfileFormFilledInSubframe {
  NSString* condition = [NSString
      stringWithFormat:@"document.getElementById('subframe')?.contentDocument"
                        ".getElementById('name')?.value === '%@' && "
                        "document.getElementById('subframe')?.contentDocument"
                        ".getElementById('address')?.value === '%@' && "
                        "document.getElementById('subframe')?.contentDocument"
                        ".getElementById('city')?.value === '%@' && "
                        "document.getElementById('subframe')?.contentDocument"
                        ".getElementById('state')?.value === '%@' && "
                        "document.getElementById('subframe')?.contentDocument"
                        ".getElementById('zip')?.value === '%@'",
                       [AutofillAppInterface exampleProfileName],
                       [AutofillAppInterface exampleProfileAddress],
                       [AutofillAppInterface exampleProfileCity],
                       [AutofillAppInterface exampleProfileState],
                       [AutofillAppInterface exampleProfileZip]];
  [ChromeEarlGrey waitForJavaScriptCondition:condition];
}

#pragma mark - Tests

// Tests that the `AttemptFormFillingTool` fills a credit card form with
// existing credit card data from Autofill.
- (void)testAttemptFormFillingTool_fillsCreditCardForm {
  [AutofillAppInterface saveLocalCreditCard];
  [AutofillAppInterface considerCreditCardFormSecureForTesting];

  GURL url = self.testServer->GetURL("/credit_card.html");
  [ChromeEarlGrey loadURL:url];
  [ChromeEarlGrey waitForWebStateContainingText:"Credit Card Info"];

  optimization_guide::proto::Action action;
  optimization_guide::proto::AttemptFormFillingAction* formFilling =
      action.mutable_attempt_form_filling();
  formFilling->set_tab_id([ChromeEarlGrey currentTabID].intValue);
  auto* request = formFilling->add_form_filling_requests();
  request->set_requested_data(
      optimization_guide::proto::FormFillingRequest_RequestedData_CREDIT_CARD);
  [self setCoordinatesOnTarget:request->add_trigger_fields()
                  withSelector:"#CCName"];

  GREYAssertNil([self executeAction:action], @"Action execution failed.");

  [self verifyCreditCardFormFilled];
}

// Tests that the `AttemptFormFillingTool` fills multiple forms located across
// the page using DOM node IDs as trigger fields, where the first form is at the
// top of the page and the second form is at a scrolling distance.
- (void)testAttemptFormFillingTool_scrollsAndFillsFormsByNodeId {
  [AutofillAppInterface saveExampleProfile];
  [AutofillAppInterface saveLocalCreditCard];
  [AutofillAppInterface considerCreditCardFormSecureForTesting];

  std::string html = base::StringPrintf(
      "%s"
      "<div style=\"width: 100px; height: 2000px; background-color: "
      "gray;\">Long Spacer</div>"
      "%s",
      kProfileFormHTML.data(), kCreditCardFormHTML.data());
  GURL url = [self URLForHTML:html];

  [ChromeEarlGrey loadURL:url];
  [ChromeEarlGrey waitForWebStateContainingText:"Profile form."];
  [ChromeEarlGrey waitForWebStateContainingText:"Credit Card form."];

  NSData* apcData = [ActorAppInterface fetchLatestAPC];
  optimization_guide::proto::PageContext pageContext;
  GREYAssertTrue(pageContext.ParseFromArray([apcData bytes], [apcData length]),
                 @"Failed to parse PageContext");

  FindNodeResult addressNodeResult =
      FindFormFieldInMainFrame(pageContext, "name");
  GREYAssertTrue(addressNodeResult.node != nullptr,
                 @"Failed to find address name field node");

  FindNodeResult ccNodeResult = FindFormFieldInMainFrame(pageContext, "CCName");
  GREYAssertTrue(ccNodeResult.node != nullptr,
                 @"Failed to find credit card name field node");

  optimization_guide::proto::Action action;
  optimization_guide::proto::AttemptFormFillingAction* formFilling =
      action.mutable_attempt_form_filling();
  formFilling->set_tab_id([ChromeEarlGrey currentTabID].intValue);

  // Request 0: Top form (Address) on top of the page.
  auto* addressRequest = formFilling->add_form_filling_requests();
  addressRequest->set_requested_data(
      optimization_guide::proto::FormFillingRequest_RequestedData_ADDRESS);
  SetTargetFromNodeResult(addressRequest->add_trigger_fields(),
                          addressNodeResult);

  // Request 1: Bottom form (Credit Card) at a scrolling distance.
  auto* ccRequest = formFilling->add_form_filling_requests();
  ccRequest->set_requested_data(
      optimization_guide::proto::FormFillingRequest_RequestedData_CREDIT_CARD);
  SetTargetFromNodeResult(ccRequest->add_trigger_fields(), ccNodeResult);

  // Verify that the page starts at the top before executing the action.
  [ChromeEarlGrey waitForJavaScriptCondition:@"window.scrollY === 0;"];

  GREYAssertNil([self executeAction:action], @"Action execution failed.");

  [self verifyProfileFormFilledInMainFrame];
  [self verifyCreditCardFormFilled];

  // TODO(crbug.com/472287741): After implementing
  // `AutofillDriverIOS::ScrollFieldIntoView`, uncomment to verify that the page
  // actually scrolled down to the credit card form.
  //
  // [ChromeEarlGrey waitForJavaScriptCondition:@"window.scrollY > 0;"];
}

// TODO(crbug.com/472287741): Add
// `testAttemptFormFillingTool_scrollsAndFillsFormsByCoordinates` if needed.

// Tests that the `AttemptFormFillingTool` fails when the targeted trigger field
// is not a form input element.
- (void)testAttemptFormFillingTool_targetNotAutofillElement_fails {
  [AutofillAppInterface saveExampleProfile];

  GURL url = self.testServer->GetURL("/profile_form.html");
  [ChromeEarlGrey loadURL:url];
  [ChromeEarlGrey waitForWebStateContainingText:"Profile form"];

  optimization_guide::proto::Action action;
  optimization_guide::proto::AttemptFormFillingAction* formFilling =
      action.mutable_attempt_form_filling();
  formFilling->set_tab_id([ChromeEarlGrey currentTabID].intValue);
  auto* request = formFilling->add_form_filling_requests();
  request->set_requested_data(
      optimization_guide::proto::FormFillingRequest_RequestedData_ADDRESS);
  [self setCoordinatesOnTarget:request->add_trigger_fields() withSelector:"h3"];

  NSError* error = [self executeAction:action];
  GREYAssertNotNil(error, @"Expected action to fail, but it succeeded.");
  GREYAssertEqual(
      error.code,
      static_cast<NSInteger>(
          actor::mojom::ActionResultCode::kFormFillingFieldNotFound),
      @"Expected kFormFillingFieldNotFound, got %@", error);
}

// Tests that the `AttemptFormFillingTool` fails when no suggestions are
// available for the requested form data.
- (void)testAttemptFormFillingTool_noSuggestions_fails {
  GURL url = self.testServer->GetURL("/profile_form.html");
  [ChromeEarlGrey loadURL:url];
  [ChromeEarlGrey waitForWebStateContainingText:"Profile form"];

  optimization_guide::proto::Action action;
  optimization_guide::proto::AttemptFormFillingAction* formFilling =
      action.mutable_attempt_form_filling();
  formFilling->set_tab_id([ChromeEarlGrey currentTabID].intValue);
  auto* request = formFilling->add_form_filling_requests();
  request->set_requested_data(
      optimization_guide::proto::FormFillingRequest_RequestedData_ADDRESS);
  [self setCoordinatesOnTarget:request->add_trigger_fields()
                  withSelector:"#name"];

  NSError* error = [self executeAction:action];
  GREYAssertNotNil(error, @"Expected action to fail, but it succeeded.");
  GREYAssertEqual(
      error.code,
      static_cast<NSInteger>(
          actor::mojom::ActionResultCode::kFormFillingNoSuggestionsAvailable),
      @"Expected kFormFillingNoSuggestionsAvailable, got %@", error);
}

// Tests that a single form filling request with trigger fields across both the
// main frame targeted by coordinates and a subframe targeted by identifiers
// in an iframe fills the form.
- (void)testAttemptFormFillingTool_mixedCoordinatesAndIdentifiers {
  [AutofillAppInterface saveExampleProfile];

  GURL addressURL = self.testServer->GetURL("/profile_form.html");
  std::string mainHTML = GetProfileFormWithIframeHTML(addressURL);

  [ChromeEarlGrey loadURL:[self URLForHTML:mainHTML]];
  [ChromeEarlGrey waitForWebStateContainingText:"Profile form."];
  [ChromeEarlGrey waitForWebStateFrameContainingText:"Profile form"];

  NSData* apcData = [ActorAppInterface fetchLatestAPC];
  optimization_guide::proto::PageContext pageContext;
  GREYAssertTrue(pageContext.ParseFromArray([apcData bytes], [apcData length]),
                 @"Failed to parse PageContext");

  FindNodeResult subframeNodeResult = FindFormFieldInSubframe(pageContext);
  GREYAssertTrue(subframeNodeResult.node != nullptr,
                 @"Failed to find address field node in iframe");

  optimization_guide::proto::Action action;
  optimization_guide::proto::AttemptFormFillingAction* formFilling =
      action.mutable_attempt_form_filling();
  formFilling->set_tab_id([ChromeEarlGrey currentTabID].intValue);

  auto* request = formFilling->add_form_filling_requests();
  request->set_requested_data(
      optimization_guide::proto::FormFillingRequest_RequestedData_ADDRESS);

  // Trigger field 0: Coordinates in the main frame.
  [self setCoordinatesOnTarget:request->add_trigger_fields()
                  withSelector:"#name"];

  // Trigger field 1: DOM node ID and frame token in the iframe.
  SetTargetFromNodeResult(request->add_trigger_fields(), subframeNodeResult);

  GREYAssertNil([self executeAction:action], @"Action execution failed.");

  [self verifyProfileFormFilledInMainFrame];
  [self verifyProfileFormFilledInSubframe];
}

// Tests that a single form filling request with trigger fields across both the
// main frame and a subframe targeted by DOM node IDs fills both forms.
- (void)testAttemptFormFillingTool_mixedMainFrameAndSubframe {
  [AutofillAppInterface saveExampleProfile];

  GURL addressURL = self.testServer->GetURL("/profile_form.html");
  std::string mainHTML = GetProfileFormWithIframeHTML(addressURL);

  [ChromeEarlGrey loadURL:[self URLForHTML:mainHTML]];
  [ChromeEarlGrey waitForWebStateContainingText:"Profile form."];
  [ChromeEarlGrey waitForWebStateFrameContainingText:"Profile form"];

  NSData* apcData = [ActorAppInterface fetchLatestAPC];
  optimization_guide::proto::PageContext pageContext;
  GREYAssertTrue(pageContext.ParseFromArray([apcData bytes], [apcData length]),
                 @"Failed to parse PageContext");

  FindNodeResult mainNodeResult = FindFormFieldInMainFrame(pageContext, "name");
  GREYAssertTrue(mainNodeResult.node != nullptr,
                 @"Failed to find name field node in main frame");

  FindNodeResult subframeNodeResult = FindFormFieldInSubframe(pageContext);
  GREYAssertTrue(subframeNodeResult.node != nullptr,
                 @"Failed to find name field node in subframe");

  optimization_guide::proto::Action action;
  optimization_guide::proto::AttemptFormFillingAction* formFilling =
      action.mutable_attempt_form_filling();
  formFilling->set_tab_id([ChromeEarlGrey currentTabID].intValue);

  auto* request = formFilling->add_form_filling_requests();
  request->set_requested_data(
      optimization_guide::proto::FormFillingRequest_RequestedData_ADDRESS);

  // Trigger field 0: DOM node ID in the main frame.
  SetTargetFromNodeResult(request->add_trigger_fields(), mainNodeResult);

  // Trigger field 1: DOM node ID in the subframe.
  SetTargetFromNodeResult(request->add_trigger_fields(), subframeNodeResult);

  GREYAssertNil([self executeAction:action], @"Action execution failed.");

  [self verifyProfileFormFilledInMainFrame];
  [self verifyProfileFormFilledInSubframe];
}

@end
