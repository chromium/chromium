// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <string>

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/json/json_reader.h"
#import "base/json/json_writer.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/policy/core/common/policy_pref_names.h"
#import "components/policy/policy_constants.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/policy/model/policy_app_interface.h"
#import "ios/chrome/browser/policy/model/policy_earl_grey_utils.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/base/net_errors.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Waits until `URL` has the expected blocked state in the given mode.
void WaitForURLBlockedStatus(const GURL& URL,
                             bool blocked,
                             bool incognito = false) {
  NSString* nsURL = base::SysUTF8ToNSString(URL.spec());
  // TODO(crbug.com/361151875): Fix this long delay and revert this timeout back
  // to base::test::ios::kWaitForActionTimeout
  GREYAssertTrue(base::test::ios::WaitUntilConditionOrTimeout(
                     base::Seconds(25),
                     ^{
                       return [PolicyAppInterface
                                  isURLBlocked:nsURL
                                   inIncognito:incognito] == blocked;
                     }),
                 @"Waiting for policy URL blocklist to update.");
}

// Sets the policy value for the given policy key and waits until the new value
// is applied.
[[nodiscard]] bool SetPolicy(const base::Value& value, const char* policy_key) {
  const std::optional<std::string> json_value = base::WriteJson(value);
  if (!json_value) {
    return false;
  }
  [PolicyAppInterface mergePolicyValue:base::SysUTF8ToNSString(*json_value)
                                forKey:base::SysUTF8ToNSString(policy_key)];
  return base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(10), ^{
    std::optional<base::Value> probed_value = base::JSONReader::Read(
        policy_test_utils::GetValueForPlatformPolicy(policy_key),
        base::JSON_PARSE_CHROMIUM_EXTENSIONS);
    return probed_value && value == *probed_value;
  });
}

// Sets the URLBlocklist policy value for controlling a list of URLs.
[[nodiscard]] bool SetURLBlocklist(const base::Value& value) {
  return SetPolicy(value, policy::key::kURLBlocklist);
}

// Sets the URLAllowlist policy value for controlling a list of URLs.
[[nodiscard]] bool SetURLAllowlist(const base::Value& value) {
  return SetPolicy(value, policy::key::kURLAllowlist);
}

// Sets the IncognitoModeUrlBlocklist policy value for controlling a list of
// URLs.
[[nodiscard]] bool SetIncognitoURLBlocklist(const base::Value& value) {
  return SetPolicy(value, policy::key::kIncognitoModeUrlBlocklist);
}

// Sets the IncognitoModeUrlAllowlist policy value for controlling a list of
// URLs.
[[nodiscard]] bool SetIncognitoURLAllowlist(const base::Value& value) {
  return SetPolicy(value, policy::key::kIncognitoModeUrlAllowlist);
}

// Sets the IncognitoModeUrlAllowlist policy value for controlling a list of
// URLs.
[[nodiscard]] bool SetIncognitoAvailability(const base::Value& value) {
  return SetPolicy(value, policy::key::kIncognitoModeAvailability);
}

}  // namespace

// Tests the URLBlocklist and URLAllowlist enterprise policies.
@interface PolicyURLBlockingTestCase : ChromeTestCase
@end

@implementation PolicyURLBlockingTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  // Use commandline args to insert fake policy data into NSUserDefaults. To the
  // app, this policy data will appear under the
  // "com.apple.configuration.managed" key.
  AppLaunchConfiguration config;
  config.relaunch_policy = NoForceRelaunchAndResetState;
  return config;
}

- (void)setUp {
  [super setUp];
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  // Check that the policy blocklist is reset.
  WaitForURLBlockedStatus(self.testServer->GetURL("/echo"), false);
  WaitForURLBlockedStatus(self.testServer->GetURL("/testpage"), false);
}

- (void)tearDownHelper {
  [PolicyAppInterface clearPolicies];
  [super tearDownHelper];
}

// Tests that pages are not blocked when the blocklist exists, but is empty.
- (void)testEmptyBlocklist {
  GREYAssertTrue(SetURLBlocklist(base::Value(base::ListValue())),
                 @"policy value couldn't be set");

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];

  [ChromeEarlGrey waitForWebStateContainingText:"Echo"];
}

// Tests that a page load is blocked when the URLBlocklist policy is set to
// block all URLs.
- (void)testWildcardBlocklist {
  GREYAssertTrue(SetURLBlocklist(base::Value(base::ListValue().Append("*"))),
                 @"policy value couldn't be set");
  WaitForURLBlockedStatus(self.testServer->GetURL("/echo"), true);

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];

  [ChromeEarlGrey waitForWebStateContainingText:
                      l10n_util::GetStringUTF8(
                          IDS_ERRORPAGES_SUMMARY_BLOCKED_BY_ADMINISTRATOR)];
}

// Tests that the NTP is not blocked by the wildcard blocklist.
- (void)testNTPIsNotBlocked {
  GREYAssertTrue(SetURLBlocklist(base::Value(base::ListValue().Append("*"))),
                 @"policy value couldn't be set");
  WaitForURLBlockedStatus(self.testServer->GetURL("/echo"), true);

  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that a page is blocked when the URLBlocklist policy is set to block a
// specific URL.
- (void)testExplicitBlocklist {
  GREYAssertTrue(
      SetURLBlocklist(base::Value(base::ListValue().Append("*/echo"))),
      @"policy value couldn't be set");
  WaitForURLBlockedStatus(self.testServer->GetURL("/echo"), true);

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];

  [ChromeEarlGrey waitForWebStateContainingText:
                      l10n_util::GetStringUTF8(
                          IDS_ERRORPAGES_SUMMARY_BLOCKED_BY_ADMINISTRATOR)];
}

// Tests that pages are loaded when explicitly listed in the URLAllowlist.
- (void)testAllowlist {
  // The URLBlocklistPolicyHandler will discard policy updates that occur while
  // it is already computing a new blocklist, so wait between calls to set new
  // policy values.
  GREYAssertTrue(SetURLBlocklist(base::Value(base::ListValue().Append("*"))),
                 @"policy value couldn't be set");

  WaitForURLBlockedStatus(self.testServer->GetURL("/testpage"), true);

  GREYAssertTrue(
      SetURLAllowlist(base::Value(base::ListValue().Append("*/echo"))),
      @"policy value couldn't be set");
  WaitForURLBlockedStatus(self.testServer->GetURL("/echo"), false);

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];

  [ChromeEarlGrey waitForWebStateContainingText:"Echo"];
}

// Tests that URLs can be blocklisted only in Incognito mode.
- (void)testIncognitoBlocklist {
  GURL URL = self.testServer->GetURL("/echo");

  // Set a blocklist for Incognito mode.
  GREYAssertTrue(
      SetIncognitoURLBlocklist(base::Value(base::ListValue().Append("*"))),
      @"policy value couldn't be set");

  // Need an incognito tab to check the incognito blocklist.
  [ChromeEarlGrey openNewIncognitoTab];
  WaitForURLBlockedStatus(URL, /*blocked=*/true, /*incognito=*/true);

  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey
      waitForWebStateContainingText:
          l10n_util::GetStringUTF8(
              IDS_ERRORPAGES_HEADING_BLOCKED_IN_INCOGNITO_BY_ADMINISTRATOR)];

  // Check that the URL is NOT blocked in regular mode.
  [ChromeEarlGrey openNewTab];
  WaitForURLBlockedStatus(URL, /*blocked=*/false, /*incognito=*/false);

  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:"Echo"];
}

// Tests that the allowlist works as an exception to the blocklist in Incognito.
- (void)testIncognitoAllBlocklistAndAllowlist {
  GURL allowed_URL = self.testServer->GetURL("/echo");
  GURL blocked_URL = self.testServer->GetURL("/testpage");

  // Set a blocklist that blocks everything in Incognito.
  GREYAssertTrue(
      SetIncognitoURLBlocklist(base::Value(base::ListValue().Append("*"))),
      @"policy value couldn't be set");

  // Set an allowlist that allows one URL in Incognito.
  GREYAssertTrue(
      SetIncognitoURLAllowlist(base::Value(base::ListValue().Append("*/echo"))),
      @"policy value couldn't be set");

  [ChromeEarlGrey openNewIncognitoTab];
  WaitForURLBlockedStatus(allowed_URL, /*blocked=*/false, /*incognito=*/true);
  WaitForURLBlockedStatus(blocked_URL, /*blocked=*/true, /*incognito=*/true);

  [ChromeEarlGrey loadURL:allowed_URL];
  [ChromeEarlGrey waitForWebStateContainingText:"Echo"];

  [ChromeEarlGrey loadURL:blocked_URL];
  [ChromeEarlGrey
      waitForWebStateContainingText:
          l10n_util::GetStringUTF8(
              IDS_ERRORPAGES_HEADING_BLOCKED_IN_INCOGNITO_BY_ADMINISTRATOR)];
}

// Tests that the Incognito blocklist takes precedence over the URL allowlist in
// Incognito mode.
- (void)testIncognitoBlocklistAndUrlAllowlist {
  GURL URL = self.testServer->GetURL("/echo");

  // Set a blocklist for Incognito mode that blocks the URL.
  GREYAssertTrue(
      SetIncognitoURLBlocklist(base::Value(base::ListValue().Append("*/echo"))),
      @"policy value couldn't be set");

  // Set a general URL allowlist that allows the URL.
  GREYAssertTrue(
      SetURLAllowlist(base::Value(base::ListValue().Append("*/echo"))),
      @"policy value couldn't be set");

  // In Incognito, it should be blocked because Incognito blocklist takes
  // precedence.
  [ChromeEarlGrey openNewIncognitoTab];
  WaitForURLBlockedStatus(URL, /*blocked=*/true, /*incognito=*/true);

  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey
      waitForWebStateContainingText:
          l10n_util::GetStringUTF8(
              IDS_ERRORPAGES_HEADING_BLOCKED_IN_INCOGNITO_BY_ADMINISTRATOR)];

  // In regular mode, it should be allowed by URLAllowlist.
  [ChromeEarlGrey openNewTab];
  WaitForURLBlockedStatus(URL, /*blocked=*/false, /*incognito=*/false);

  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:"Echo"];
}

// Tests that the Incognito allowlist works as an exception to the URL
// blocklist, but only in Incognito mode.
- (void)testUrlBlocklistAndIncognitoAllowlist {
  GURL URL = self.testServer->GetURL("/echo");

  // Set a general blocklist that blocks the URL.
  GREYAssertTrue(
      SetURLBlocklist(base::Value(base::ListValue().Append("*/echo"))),
      @"policy value couldn't be set");

  // Set an Incognito allowlist that allows the URL.
  GREYAssertTrue(
      SetIncognitoURLAllowlist(base::Value(base::ListValue().Append("*/echo"))),
      @"policy value couldn't be set");

  // In Incognito, it should be allowed.
  [ChromeEarlGrey openNewIncognitoTab];
  WaitForURLBlockedStatus(URL, /*blocked=*/false, /*incognito=*/true);

  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:"Echo"];

  // In regular mode, it should be blocked.
  [ChromeEarlGrey openNewTab];
  WaitForURLBlockedStatus(URL, /*blocked=*/true, /*incognito=*/false);

  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:
                      l10n_util::GetStringUTF8(
                          IDS_ERRORPAGES_SUMMARY_BLOCKED_BY_ADMINISTRATOR)];
}

// Tests that setting only the Incognito allowlist allows specific URLs and
// blocks others in Incognito mode.
- (void)testIncognitoAllowlistOnly {
  GURL allowed_URL = self.testServer->GetURL("/echo");
  GURL blocked_URL = self.testServer->GetURL("/testpage");

  // Only set the Incognito allowlist.
  GREYAssertTrue(
      SetIncognitoURLAllowlist(base::Value(base::ListValue().Append("*/echo"))),
      @"policy value couldn't be set");

  [ChromeEarlGrey openNewIncognitoTab];
  WaitForURLBlockedStatus(allowed_URL, /*blocked=*/false, /*incognito=*/true);
  WaitForURLBlockedStatus(blocked_URL, /*blocked=*/true, /*incognito=*/true);

  [ChromeEarlGrey loadURL:allowed_URL];
  [ChromeEarlGrey waitForWebStateContainingText:"Echo"];

  [ChromeEarlGrey loadURL:blocked_URL];
  [ChromeEarlGrey
      waitForWebStateContainingText:
          l10n_util::GetStringUTF8(
              IDS_ERRORPAGES_HEADING_BLOCKED_IN_INCOGNITO_BY_ADMINISTRATOR)];
}

// Tests that the Incognito allowlist allows specific URLs even if
// IncognitoModeAvailability is set to disabled.
- (void)testIncognitoAllowlistAndIncognitoDisabled {
  GURL allowed_URL = self.testServer->GetURL("/echo");
  GURL blocked_URL = self.testServer->GetURL("/testpage");

  // Disable Incognito mode generally.
  GREYAssertTrue(SetIncognitoAvailability(base::Value(static_cast<int>(
                     policy::IncognitoModeAvailability::kDisabled))),
                 @"policy value couldn't be set");

  // Set an Incognito allowlist.
  GREYAssertTrue(
      SetIncognitoURLAllowlist(base::Value(base::ListValue().Append("*/echo"))),
      @"policy value couldn't be set");
  // Even if disabled, the explicitly allowed URLs should be accessible in
  // Incognito mode.
  [ChromeEarlGrey openNewIncognitoTab];
  WaitForURLBlockedStatus(allowed_URL, /*blocked=*/false, /*incognito=*/true);
  WaitForURLBlockedStatus(blocked_URL, /*blocked=*/true, /*incognito=*/true);

  [ChromeEarlGrey loadURL:allowed_URL];
  [ChromeEarlGrey waitForWebStateContainingText:"Echo"];

  [ChromeEarlGrey loadURL:blocked_URL];
  [ChromeEarlGrey
      waitForWebStateContainingText:
          l10n_util::GetStringUTF8(
              IDS_ERRORPAGES_HEADING_BLOCKED_IN_INCOGNITO_BY_ADMINISTRATOR)];
}

// Checks the blocking rules for both regular and Incognito mode blocklists.
- (void)testUrlBlocklistAndIncognitoBlocklist {
  GURL regular_blocked_URL = self.testServer->GetURL("/testpage");
  GURL incognito_blocked_URL = self.testServer->GetURL("/echo");

  // Set a URLBlocklist that blocks `regular_blocked_URL`.
  GREYAssertTrue(
      SetURLBlocklist(base::Value(base::ListValue().Append("*/testpage"))),
      @"policy value couldn't be set");

  // Set an Incognito blocklist that blocks `incognito_blocked_URL`.
  GREYAssertTrue(
      SetIncognitoURLBlocklist(base::Value(base::ListValue().Append("*/echo"))),
      @"policy value couldn't be set");

  // Regular blocklist blocks in both modes.
  [ChromeEarlGrey openNewTab];
  WaitForURLBlockedStatus(regular_blocked_URL, /*blocked=*/true,
                          /*incognito=*/false);
  [ChromeEarlGrey openNewIncognitoTab];
  WaitForURLBlockedStatus(regular_blocked_URL, /*blocked=*/true,
                          /*incognito=*/true);

  // Incognito blocklist blocks only in Incognito mode.
  [ChromeEarlGrey openNewTab];
  WaitForURLBlockedStatus(incognito_blocked_URL, /*blocked=*/false,
                          /*incognito=*/false);
  [ChromeEarlGrey openNewIncognitoTab];
  WaitForURLBlockedStatus(incognito_blocked_URL, /*blocked=*/true,
                          /*incognito=*/true);
}

// Checks that the Incognito allowlist allows specific URLs even if
// IncognitoModeAvailability is set to disabled, and blocklist is set.
- (void)testIncognitoAllowlistBlocklistAndIncognitoDisabled {
  GURL allowed_URL = self.testServer->GetURL("/echo");
  GURL blocked_URL = self.testServer->GetURL("/testpage");

  // Disable Incognito mode generally.
  GREYAssertTrue(SetIncognitoAvailability(base::Value(static_cast<int>(
                     policy::IncognitoModeAvailability::kDisabled))),
                 @"policy value couldn't be set");

  // Set an Incognito allowlist.
  GREYAssertTrue(
      SetIncognitoURLAllowlist(base::Value(base::ListValue().Append("*/echo"))),
      @"policy value couldn't be set");

  // Set an Incognito blocklist.
  GREYAssertTrue(SetIncognitoURLBlocklist(
                     base::Value(base::ListValue().Append("*/testpage"))),
                 @"policy value couldn't be set");

  [ChromeEarlGrey openNewIncognitoTab];
  WaitForURLBlockedStatus(allowed_URL, /*blocked=*/false, /*incognito=*/true);
  WaitForURLBlockedStatus(blocked_URL, /*blocked=*/true, /*incognito=*/true);

  [ChromeEarlGrey loadURL:allowed_URL];
  [ChromeEarlGrey waitForWebStateContainingText:"Echo"];

  [ChromeEarlGrey loadURL:blocked_URL];
  [ChromeEarlGrey
      waitForWebStateContainingText:
          l10n_util::GetStringUTF8(
              IDS_ERRORPAGES_HEADING_BLOCKED_IN_INCOGNITO_BY_ADMINISTRATOR)];
}

@end
