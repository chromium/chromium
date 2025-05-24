// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#import "base/json/json_reader.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.cc"
#import "base/system/sys_info.h"
#import "base/test/ios/wait_util.h"
#import "base/version_info/version_info.h"
#import "components/enterprise/browser/enterprise_switches.h"
#import "components/grit/policy_resources.h"
#import "components/grit/policy_resources_map.h"
#import "components/policy/test_support/embedded_policy_test_server.h"
#import "components/strings/grit/components_branded_strings.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/policy/model/policy_earl_grey_utils.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/element_selector.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// Ids of elements in chrome://policy
NSString* const kReloadPoliciesButton = @"reload-policies";
NSString* const kExportPoliciesButton = @"export-policies";
NSString* const kViewLogsButton = @"view-logs";
NSString* const kMoreActionsButton = @"more-actions-button";

// Ids of elements in chrome://policy/logs
NSString* const kRefreshLogsButton = @"logs-refresh";
NSString* const kExportLogsButton = @"logs-dump";

// Ids of elements in chrome://policy/test
NSString* const kApplyPoliciesButton = @"apply-policies";

std::vector<std::string> PopulateExpectedPolicy(const std::string& name,
                                                const std::string& value) {
  std::vector<std::string> expected_policy;

  // Populate expected policy column and row fields.
  expected_policy.push_back(name);
  expected_policy.push_back(value);
  expected_policy.push_back("Platform");
  expected_policy.push_back("Machine");
  expected_policy.push_back("Mandatory");
  expected_policy.push_back("OK");

  return expected_policy;
}

void VerifyPolicies(
    const std::vector<std::vector<std::string>>& expected_policies) {
  // Retrieve the text contents of the policy table cells for all policies.
  NSString* javascript = @"var entries = getAllPolicyTables();"
                          "var policies = [];"
                          "for (var i = 0; i < entries.length; ++i) {"
                          "  var items = getAllPolicyRows(entries[i]);"
                          "  for (var j = 0; j < items.length; ++j) {"
                          "    var children = getAllPolicyRowDivs(items[j]);"
                          "    var values = [];"
                          "    for(var k = 0; k < children.length - 1; ++k) {"
                          "      values.push(children[k].textContent.trim());"
                          "    }"
                          "    policies.push(values);"
                          "  }"
                          "}"
                          "JSON.stringify(policies);";

  base::Value policies = [ChromeEarlGrey evaluateJavaScript:javascript];
  std::optional<base::Value> value_ptr =
      base::JSONReader::Read(policies.GetString());
  GREYAssertTrue(value_ptr, @"Expected policies, but there weren't any.");
  GREYAssertTrue(value_ptr->is_list(), @"Value is not a list.");
  const base::Value::List& actual_policies = value_ptr->GetList();

  // Verify that the cells contain the expected strings for all policies.
  for (size_t i = 0; i < expected_policies.size(); ++i) {
    const std::vector<std::string> expected_policy = expected_policies[i];
    const base::Value::List& actual_policy = actual_policies[i].GetList();
    GREYAssertEqual(expected_policy.size(), actual_policy.size(),
                    @"Number of fields in the actual and expected policy row "
                    @"did not match.");
    for (size_t j = 0; j < expected_policy.size(); ++j) {
      const std::string* value = actual_policy[j].GetIfString();
      GREYAssertTrue(value, [NSString stringWithUTF8String:value->c_str()]);
      if (expected_policy[j] != *value) {
        GREYAssertEqual(expected_policy[j], *value,
                        [NSString stringWithUTF8String:value->c_str()]);
      }
    }
  }
}

ElementSelector* ReloadPoliciesButton() {
  return [ElementSelector
      selectorWithElementID:base::SysNSStringToUTF8(kReloadPoliciesButton)];
}

ElementSelector* RefreshLogsButton() {
  return [ElementSelector
      selectorWithElementID:base::SysNSStringToUTF8(kRefreshLogsButton)];
}

ElementSelector* ApplyPoliciesButton() {
  return [ElementSelector
      selectorWithElementID:base::SysNSStringToUTF8(kApplyPoliciesButton)];
}

// Matcher for "Download" button on Download Manager UI.
id<GREYMatcher> DownloadButton() {
  return grey_accessibilityID(
      @"kDownloadManagerDownloadAccessibilityIdentifier");
}

// Waits until Download button is shown.
[[nodiscard]] bool WaitForDownloadButton() {
  return base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        NSError* error = nil;
        [[EarlGrey selectElementWithMatcher:DownloadButton()]
            assertWithMatcher:grey_interactable()
                        error:&error];
        return (error == nil);
      });
}

// Waits until Open in... button is shown on file download.
[[nodiscard]] bool WaitForOpenInButton() {
  // These downloads usually take longer and need a longer timeout.
  constexpr base::TimeDelta kLongDownloadTimeout = base::Minutes(1);
  return base::test::ios::WaitUntilConditionOrTimeout(kLongDownloadTimeout, ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:chrome_test_util::OpenInButton()]
        assertWithMatcher:grey_interactable()
                    error:&error];
    return (error == nil);
  });
}

}  // namespace

// Test case for chrome://policy WebUI pages.
@interface PolicyUITestCase : ChromeTestCase
@end

@implementation PolicyUITestCase

- (void)setUp {
  [super setUp];
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  if ([self isRunningTest:@selector(testPolicyTestPageLoadsCorrectly)]) {
    // Assign the test environment to be on the Canary channel. This ensures
    // the test does not run in a stable channel because chrome://policy/test
    // can only be accessed in non-stable channels.
    config.additional_args = {"--fake-variations-channel=canary"};
  }
  return config;
}

// -----------------------------------------------------------------------------
// Policy Pages Load
// -----------------------------------------------------------------------------

// Tests that chrome://policy loads correctly in both regular and incognito tabs
- (void)testPolicyPageLoadsCorrectly {
  [ChromeEarlGrey loadURL:GURL(kChromeUIPolicyURL)];
  [ChromeEarlGrey waitForWebStateContainingElement:ReloadPoliciesButton()];
  [ChromeEarlGrey tapWebStateElementWithID:kReloadPoliciesButton];

  // Open in new incognito tab.
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey waitForIncognitoTabCount:1];
  [ChromeEarlGrey loadURL:GURL(kChromeUIPolicyURL)];
  [ChromeEarlGrey waitForWebStateContainingElement:ReloadPoliciesButton()];
}

// Tests that chrome://policy/logs in both regular and incognito tabs
- (void)testPolicyLogsPageLoadsCorrectly {
  [ChromeEarlGrey loadURL:GURL(kChromeUIPolicyLogsURL)];
  [ChromeEarlGrey waitForWebStateContainingElement:RefreshLogsButton()];
  [ChromeEarlGrey tapWebStateElementWithID:kRefreshLogsButton];

  // Open in new incognito tab.
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey waitForIncognitoTabCount:1];
  [ChromeEarlGrey loadURL:GURL(kChromeUIPolicyLogsURL)];
  [ChromeEarlGrey waitForWebStateContainingElement:RefreshLogsButton()];
}

// Tests that chrome://policy/test
- (void)testPolicyTestPageLoadsCorrectly {
  [ChromeEarlGrey loadURL:GURL(kChromeUIPolicyTestURL)];
  [ChromeEarlGrey waitForWebStateContainingElement:ApplyPoliciesButton()];
  [ChromeEarlGrey tapWebStateElementWithID:kApplyPoliciesButton];

  // Open in new incognito tab.
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey waitForIncognitoTabCount:1];
  [ChromeEarlGrey loadURL:GURL(kChromeUIPolicyTestURL)];
  [ChromeEarlGrey waitForWebStateContainingElement:ApplyPoliciesButton()];
}

// -----------------------------------------------------------------------------
// Tests for chrome://policy page
// -----------------------------------------------------------------------------

// Tests the chrome://policy page when no policies are set.
- (void)testPolicyPageUnmanaged {
  // Open the policy page and check if the content is expected.
  [ChromeEarlGrey loadURL:GURL(kChromeUIPolicyURL)];
  [ChromeEarlGrey
      waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                        IDS_POLICY_NO_POLICIES_SET)];
}

// Tests the chrome://policy page when there are machine level policies.
- (void)testPolicyPageManagedWithCBCM {
  // Fake browser enrollment with an enrollment token that will start chrome
  // browser cloud management without making network calls.
  AppLaunchConfiguration config;
  config.additional_args.push_back(
      base::StrCat({"--", switches::kEnableChromeBrowserCloudManagement}));
  config.additional_args.push_back("-com.apple.configuration.managed");

  config.additional_args.push_back(
      base::StrCat({"<dict><key>CloudManagementEnrollmentToken</key><string>",
                    policy::kInvalidEnrollmentToken, "</string></dict>"}));
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  // Open the policy page and check that the enrollment token is shown.
  [ChromeEarlGrey loadURL:GURL(kChromeUIPolicyURL)];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_POLICY_STATUS_DEVICE)];
  [ChromeEarlGrey
      waitForWebStateContainingText:
          l10n_util::GetStringUTF8(IDS_POLICY_LABEL_MACHINE_ENROLLMENT_TOKEN)];
}

// Tests the chrome://policy page when there are policies set.
- (void)testPoliciesShowOnPage {
  // Set policies
  policy_test_utils::MergePolicy(false, "AutofillCreditCardEnabled");
  policy_test_utils::MergePolicy(1, "IncognitoModeAvailability");

  // Navigate to chrome://policy.
  [ChromeEarlGrey loadURL:GURL(kChromeUIPolicyURL)];

  // Verify that the policy set is shown on the page.
  std::vector<std::vector<std::string>> expected_policies;
  expected_policies.push_back(
      PopulateExpectedPolicy("AutofillCreditCardEnabled", "false"));
  expected_policies.push_back(
      PopulateExpectedPolicy("IncognitoModeAvailability", "1"));
  VerifyPolicies(expected_policies);
}

// Tests that the "View Logs" button successfully redirects to
// chrome://policy/logs.
- (void)testViewLogsRedirectsToLogsPage {
  [ChromeEarlGrey loadURL:GURL(kChromeUIPolicyURL)];
  // Click the dropdown and wait until the button shows.
  [ChromeEarlGrey tapWebStateElementWithID:kMoreActionsButton];
  // Click "View Logs"
  [ChromeEarlGrey tapWebStateElementWithID:kViewLogsButton];
  // Verify that the logs page is opened.
  [ChromeEarlGrey waitForWebStateContainingElement:RefreshLogsButton()];
}

// -----------------------------------------------------------------------------
// Tests for chrome://policy/logs page
// -----------------------------------------------------------------------------

// Tests that the export button successfully downloads a file.
- (void)testExportLogsToJson {
  [ChromeEarlGrey loadURL:GURL(kChromeUIPolicyLogsURL)];
  // Click "Export Logs to JSON" button
  [ChromeEarlGrey tapWebStateElementWithID:kExportLogsButton];
  // Verify the download button at the bottom shows.
  GREYAssert(WaitForDownloadButton(), @"Download button did not show up");
  [[EarlGrey selectElementWithMatcher:DownloadButton()]
      performAction:grey_tap()];
  GREYAssert(WaitForOpenInButton(), @"Open in... button did not show up");
}

// Tests that the version information displayed is correct.
- (void)testVersionInformationIsCorrect {
  [ChromeEarlGrey loadURL:GURL(kChromeUIPolicyLogsURL)];
  // Verify that app versionis present on the page.
  const std::string version(version_info::GetVersionNumber());
  const std::string last_change(version_info::GetLastChange());

  [ChromeEarlGrey waitForWebStateContainingText:version];
  [ChromeEarlGrey waitForWebStateContainingText:last_change];
  [ChromeEarlGrey waitForWebStateContainingText:"iOS"];
}

// -----------------------------------------------------------------------------
// Tests for chrome://policy/test
// -----------------------------------------------------------------------------
// TODO(crbug.com/346527212: Test chrome://policy/test buttons.

@end
