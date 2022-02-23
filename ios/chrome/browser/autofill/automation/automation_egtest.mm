// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/autofill_features.h"
#import "ios/chrome/browser/autofill/automation/automation_action.h"
#import "ios/chrome/browser/autofill/automation/automation_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

static const char kAutofillAutomationSwitch[] = "autofillautomation";
static const int kRecipeRetryLimit = 5;

// Private helper method for accessing app interface method.
NSError* SetAutofillAutomationProfile(const std::string& profile_json_string) {
  NSString* profile_json_nsstring =
      base::SysUTF8ToNSString(profile_json_string);
  return [AutomationAppInterface
      setAutofillAutomationProfile:profile_json_nsstring];
}

// Loads the recipe file and reads it into std::string.
std::string ReadRecipeJsonFromPath(const base::FilePath& path) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  std::string json_text;
  bool read_success = base::ReadFileToString(path, &json_text);
  GREYAssert(read_success, @"Unable to read JSON file.");
  return json_text;
}

// Parses recipe std::string into base::Value.
base::Value RecipeJsonToValue(const std::string& recipe_json) {
  absl::optional<base::Value> value = base::JSONReader::Read(recipe_json);
  GREYAssert(value.has_value(), @"Unable to parse JSON string.");
  GREYAssert(value.value().is_dict(),
             @"Expecting a dictionary in the recipe JSON string.");
  return std::move(value).value();
}

}  // namespace

// The autofill automation test case is intended to run a script against a
// captured web site. It gets the script from the command line.
@interface AutofillAutomationTestCase : ChromeTestCase {
  bool _shouldRecordException;
  GURL _startURL;
  NSMutableArray<AutomationAction*>* _actions;
}
@end

@implementation AutofillAutomationTestCase

// Retrieves the path to the recipe file from the command line.
+ (const base::FilePath)recipePath {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::CommandLine* commandLine(base::CommandLine::ForCurrentProcess());
  GREYAssert(commandLine->HasSwitch(kAutofillAutomationSwitch),
             @"Missing command line switch %s.", kAutofillAutomationSwitch);

  base::FilePath path(
      commandLine->GetSwitchValuePath(kAutofillAutomationSwitch));
  GREYAssert(!path.empty(),
             @"A file name must be specified for command line switch %s.",
             kAutofillAutomationSwitch);
  GREYAssert(path.IsAbsolute(),
             @"A fully qualified file name must be specified for command "
             @"line switch %s.",
             kAutofillAutomationSwitch);
  GREYAssert(base::PathExists(path), @"File not found for switch %s.",
             kAutofillAutomationSwitch);

  return path;
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(
      autofill::features::kAutofillShowTypePredictions);
  return config;
}

- (void)setUp {
  self->_shouldRecordException = true;

  [super setUp];

  const base::FilePath recipePath = [[self class] recipePath];
  std::string recipeJSONText = ReadRecipeJsonFromPath(recipePath);
  base::Value recipeRoot = RecipeJsonToValue(recipeJSONText);

  NSError* error = SetAutofillAutomationProfile(recipeJSONText);
  GREYAssertNil(error, error.localizedDescription);

  // Extract the starting URL.
  base::Value* startURLValue =
      recipeRoot.FindKeyOfType("startingURL", base::Value::Type::STRING);
  GREYAssert(startURLValue, @"Test file is missing startingURL.");

  const std::string startURLString(startURLValue->GetString());
  GREYAssert(!startURLString.empty(), @"startingURL is an empty value.");

  _startURL = GURL(startURLString);

  // Extract the actions.
  base::Value* actionValue =
      recipeRoot.FindKeyOfType("actions", base::Value::Type::LIST);
  GREYAssert(actionValue, @"Test file is missing actions.");

  base::Value::ConstListView actionsValues(actionValue->GetListDeprecated());
  GREYAssert(actionsValues.size(), @"Test file has empty actions.");

  _actions = [[NSMutableArray alloc] initWithCapacity:actionsValues.size()];
  for (auto const& actionValue : actionsValues) {
    GREYAssert(actionValue.is_dict(),
               @"Expecting each action to be a dictionary in the JSON file.");
    [_actions addObject:[AutomationAction
                            actionWithValueDictionary:
                                static_cast<const base::DictionaryValue&>(
                                    actionValue)]];
  }
}

// Override the XCTestCase method that records a failure due to an exception.
// This way it can be chosen whether to report failures during multiple runs of
// a recipe, and only fail the test if all the runs of the recipe fail.
// Will still print the failure even when it is not reported.
- (void)recordFailureWithDescription:(NSString*)description
                              inFile:(NSString*)filePath
                              atLine:(NSUInteger)lineNumber
                            expected:(BOOL)expected {
  if (self->_shouldRecordException) {
    [super recordFailureWithDescription:description
                                 inFile:filePath
                                 atLine:lineNumber
                               expected:expected];
  } else {
    NSLog(@"%@", description);
  }
}

// Runs the recipe provided multiple times.
// If any of the runs succeed, the test will be reported as a success.
- (void)testActions {
  for (int i = 0; i < kRecipeRetryLimit; i++) {
    // Only actually report the exception on the last run.
    // This is because any exception reporting will fail the test.
    NSLog(@"================================================================");
    NSLog(@"RECIPE ATTEMPT %d of %d for %@", (i + 1), kRecipeRetryLimit,
          base::SysUTF8ToNSString(_startURL.GetContent()));

    self->_shouldRecordException = (i == (kRecipeRetryLimit - 1));

    if ([self runActionsOnce]) {
      return;
    }
  }
}

// Tries running the recipe against the target website once.
// Returns true if the entire recipe succeeds.
// Returns false if an assertion is raised due to a failure.
- (bool)runActionsOnce {
  @try {
    // Load the initial page of the recipe.
    [ChromeEarlGrey loadURL:_startURL];

    for (AutomationAction* action in _actions) {
      [action execute];
    }
  } @catch (NSException* e) {
    return false;
  }

  return true;
}

@end
