// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/command_line.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/json/json_reader.h"
#import "base/strings/sys_string_conversions.h"
#import "base/threading/thread_restrictions.h"
#import "base/values.h"
#import "components/autofill/core/browser/field_types.h"
#import "components/autofill/core/common/autofill_features.h"
#import "ios/chrome/browser/autofill/model/automation/automation_action.h"
#import "ios/chrome/browser/autofill/model/automation/automation_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

namespace {

static const char kAutofillAutomationSwitch[] = "autofillautomation";

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
  std::optional<base::Value> value = base::JSONReader::Read(recipe_json);
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
      autofill::features::test::kAutofillShowTypePredictions);
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
  std::string* startURLValue = recipeRoot.GetDict().FindString("startingURL");
  GREYAssert(startURLValue, @"Test file is missing startingURL.");

  const std::string startURLString(*startURLValue);
  GREYAssert(!startURLString.empty(), @"startingURL is an empty value.");

  _startURL = GURL(startURLString);

  // Extract the actions.
  base::Value::List* actions = recipeRoot.GetDict().FindList("actions");
  GREYAssert(actions, @"Test file is missing actions.");
  GREYAssert(!actions->empty(), @"Test file has empty actions.");

  _actions = [[NSMutableArray alloc] initWithCapacity:actions->size()];
  for (base::Value& action : *actions) {
    GREYAssert(action.is_dict(), @"Expecting each action to be a ...");
    [_actions addObject:[AutomationAction
                            actionWithValueDict:std::move(action.GetDict())]];
  }
}

// Runs the recipe provided.
- (void)testActions {
  [ChromeEarlGrey loadURL:_startURL];

  for (AutomationAction* action in _actions) {
    [action execute];
  }
}

@end
