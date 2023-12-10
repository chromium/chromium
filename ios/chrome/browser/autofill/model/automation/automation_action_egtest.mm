// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/base_paths.h"
#import "base/path_service.h"
#import "base/values.h"
#import "ios/chrome/browser/autofill/model/automation/automation_action.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"

const char kTestPageUrl[] = "/components/test/data/autofill/"
                            "credit_card_upload_form_address_and_cc.html";

// Tests each automation that can be performed, by performing them individually
// against a self-hosted webpage and verifying the action was performed through
// JS queries.
@interface AutofillAutomationActionTestCase : ChromeTestCase
@end

@implementation AutofillAutomationActionTestCase

- (void)setUp {
  [super setUp];

  self.testServer->ServeFilesFromDirectory(
      base::PathService::CheckedGet(base::DIR_ASSETS));
  XCTAssertTrue(self.testServer->Start());

  [ChromeEarlGrey loadURL:self.testServer->GetURL(kTestPageUrl)];
}

// Tests the click action, by clicking a button that populates the web page,
// then using JS to assert that the web page has been populated as a result
// of the click.
- (void)testAutomationActionClick {
  base::Value::Dict dict;
  dict.Set("type", "click");
  dict.Set("selector", "//*[@id=\"fill_form\"]");
  AutomationAction* action =
      [AutomationAction actionWithValueDict:std::move(dict)];
  [action execute];

  base::Value result = [ChromeEarlGrey
      evaluateJavaScript:
          @"document.getElementsByName(\"name_address\")[0].value == \"John "
          @"Smith\""];
  GREYAssertTrue(result.is_bool(), @"The output is not a boolean.");
  GREYAssert(result.GetBool(),
             @"Click automation action did not populate the name field.");
}

// Tests the waitFor action, by using the click action to click a button that
// populates the name field after a few seconds, and using waitFor to verify
// this eventually happens.
- (void)testAutomationActionClickAndWaitFor {
  base::Value::Dict clickDict;
  clickDict.Set("type", "click");
  clickDict.Set("selector", "//*[@id=\"fill_form_delay\"]");
  AutomationAction* clickAction =
      [AutomationAction actionWithValueDict:std::move(clickDict)];
  [clickAction execute];

  base::Value::Dict waitForDict;
  waitForDict.Set("type", "waitFor");
  base::Value::List assertions = base::Value::List();
  assertions.Append(
      "return document.getElementsByName(\"name_address\")[0].value == \"Jane "
      "Smith\";");
  waitForDict.Set("assertions", std::move(assertions));
  AutomationAction* waitForAction =
      [AutomationAction actionWithValueDict:std::move(waitForDict)];
  [waitForAction execute];
}

- (void)testAutomationActionSelectDropdown {
  base::Value::Dict selectDict;
  selectDict.Set("type", "select");
  selectDict.Set("selector", "//*[@name=\"cc_month_exp\"]");
  selectDict.Set("index", 5);
  AutomationAction* selectAction =
      [AutomationAction actionWithValueDict:std::move(selectDict)];
  [selectAction execute];

  base::Value result = [ChromeEarlGrey
      evaluateJavaScript:
          @"document.getElementsByName(\"cc_month_exp\")[0].value == \"6\""];
  GREYAssertTrue(result.is_bool(), @"The result is not a boolean");
  GREYAssert(result.GetBool(),
             @"Select automation action did not change the dropdown.");
}

@end
