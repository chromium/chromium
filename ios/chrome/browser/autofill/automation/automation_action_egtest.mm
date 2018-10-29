// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#include "base/values.h"
#import "ios/chrome/browser/autofill/automation/automation_action.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/web/public/test/http_server/http_server.h"
#import "ios/web/public/test/js_test_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const char kTestPageUrl[] =
    "https://components/test/data/autofill/"
    "credit_card_upload_form_address_and_cc.html";

// Tests each automation that can be performed, by performing them individually
// against a self-hosted webpage and verifying the action was performed through
// JS queries.
@interface AutofillAutomationActionTestCase : ChromeTestCase
@end

@implementation AutofillAutomationActionTestCase

// Tests the click action, by clicking a button that populates the web page,
// then using JS to assert that the web page has been populated as a result
// of the click.
- (void)testAutomationActionClick {
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kTestPageUrl)];

  base::DictionaryValue dict = base::DictionaryValue();
  dict.SetKey("type", base::Value("click"));
  dict.SetKey("selector", base::Value("//*[@id=\"fill_form\"]"));
  AutomationAction* action = [AutomationAction actionWithValueDictionary:dict];
  [action execute];

  NSError* error;
  id result = chrome_test_util::ExecuteJavaScript(
      @"document.getElementsByName(\"name_address\")[0].value == \"John "
      @"Smith\"",
      &error);
  GREYAssert([result boolValue] && !error,
             @"Click automation action did not populate the name field.");
}

// Tests the waitFor action, by using the click action to click a button that
// populates the name field after a few seconds, and using waitFor to verify
// this eventually happens.
- (void)testAutomationActionClickAndWaitFor {
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kTestPageUrl)];

  base::DictionaryValue clickDict = base::DictionaryValue();
  clickDict.SetKey("type", base::Value("click"));
  clickDict.SetKey("selector", base::Value("//*[@id=\"fill_form_delay\"]"));
  AutomationAction* clickAction =
      [AutomationAction actionWithValueDictionary:clickDict];
  [clickAction execute];

  base::DictionaryValue waitForDict = base::DictionaryValue();
  waitForDict.SetKey("type", base::Value("waitFor"));
  base::Value assertions = base::Value(base::Value::Type::LIST);
  assertions.GetList().emplace_back(base::Value(
      "return document.getElementsByName(\"name_address\")[0].value == \"Jane "
      "Smith\";"));
  waitForDict.SetKey("assertions", std::move(assertions));
  AutomationAction* waitForAction =
      [AutomationAction actionWithValueDictionary:waitForDict];
  [waitForAction execute];
}

- (void)testAutomationActionSelectDropdown {
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kTestPageUrl)];

  base::DictionaryValue selectDict = base::DictionaryValue();
  selectDict.SetKey("type", base::Value("select"));
  selectDict.SetKey("selector", base::Value("//*[@name=\"cc_month_exp\"]"));
  selectDict.SetKey("index", base::Value(5));
  AutomationAction* selectAction =
      [AutomationAction actionWithValueDictionary:selectDict];
  [selectAction execute];

  NSError* error;
  id result = chrome_test_util::ExecuteJavaScript(
      @"document.getElementsByName(\"cc_month_exp\")[0].value == \"6\"",
      &error);
  GREYAssert([result boolValue] && !error,
             @"Select automation action did not change the dropdown.");
}

@end
