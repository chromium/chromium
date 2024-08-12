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

// Waits until `url` has the expected blocked state.
void WaitForURLBlockedStatus(const GURL& url, bool blocked) {
  NSString* nsurl = base::SysUTF8ToNSString(url.spec());
  GREYAssertTrue(base::test::ios::WaitUntilConditionOrTimeout(
                     base::test::ios::kWaitForActionTimeout,
                     ^{
                       return
                           [PolicyAppInterface isURLBlocked:nsurl] == blocked;
                     }),
                 @"Waiting for policy url blocklist to update.");
}

// Sets the policy value for controlling a list of URLs. Sets the blocklist
// policy when `block_url` is true or sets the allowlist policy otherwise.
[[nodiscard]] bool SetPolicyValue(const base::Value& value, bool block_url) {
  const std::optional<std::string> json_value = base::WriteJson(value);
  if (!json_value) {
    return false;
  }
  const char* policy_key =
      block_url ? policy::key::kURLBlocklist : policy::key::kURLAllowlist;
  policy_test_utils::SetPolicy(*json_value, policy_key);
  return base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(10), ^{
    std::optional<base::Value> probed_value = base::JSONReader::Read(
        policy_test_utils::GetValueForPlatformPolicy(policy_key));
    return probed_value && value == *probed_value;
  });
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

- (void)tearDown {
  [PolicyAppInterface clearPolicies];
  [super tearDown];
}

// Tests that pages are not blocked when the blocklist exists, but is empty.
- (void)testEmptyBlocklist {
  GREYAssertTrue(
      SetPolicyValue(base::Value(base::Value::List()), /*block_url=*/true),
      @"policy value couldn't be set");

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];

  [ChromeEarlGrey waitForWebStateContainingText:"Echo"];
}

// Tests that a page load is blocked when the URLBlocklist policy is set to
// block all URLs.
// TODO(crbug.com/356495438): Re-enable when fixed when building with Xcode 16.
- (void)FormDatatestWildcardBlocklist {
  GREYAssertTrue(SetPolicyValue(base::Value(base::Value::List().Append("*")),
                                /*block_url=*/true),
                 @"policy value couldn't be set");
  WaitForURLBlockedStatus(self.testServer->GetURL("/echo"), true);

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];

  [ChromeEarlGrey waitForWebStateContainingText:
                      l10n_util::GetStringUTF8(
                          IDS_ERRORPAGES_SUMMARY_BLOCKED_BY_ADMINISTRATOR)];
}

// Tests that the NTP is not blocked by the wildcard blocklist.
// TODO(crbug.com/356495438): Re-enable when fixed when building with Xcode 16.
- (void)FormDatatestNTPIsNotBlocked {
  GREYAssertTrue(SetPolicyValue(base::Value(base::Value::List().Append("*")),
                                /*block_url=*/true),
                 @"policy value couldn't be set");
  WaitForURLBlockedStatus(self.testServer->GetURL("/echo"), true);

  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that a page is blocked when the URLBlocklist policy is set to block a
// specific URL.
// TODO(crbug.com/356495438): Re-enable when fixed when building with Xcode 16.
- (void)FormDatatestExplicitBlocklist {
  GREYAssertTrue(
      SetPolicyValue(base::Value(base::Value::List().Append("*/echo")),
                     /*block_url=*/true),
      @"policy value couldn't be set");
  WaitForURLBlockedStatus(self.testServer->GetURL("/echo"), true);

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];

  [ChromeEarlGrey waitForWebStateContainingText:
                      l10n_util::GetStringUTF8(
                          IDS_ERRORPAGES_SUMMARY_BLOCKED_BY_ADMINISTRATOR)];
}

// Tests that pages are loaded when explicitly listed in the URLAllowlist.
// TODO(crbug.com/356495438): Re-enable when fixed when building with Xcode 16.
- (void)testAllowlist {
  // The URLBlocklistPolicyHandler will discard policy updates that occur while
  // it is already computing a new blocklist, so wait between calls to set new
  // policy values.
  GREYAssertTrue(SetPolicyValue(base::Value(base::Value::List().Append("*")),
                                /*block_url=*/true),
                 @"policy value couldn't be set");

  WaitForURLBlockedStatus(self.testServer->GetURL("/testpage"), true);

  GREYAssertTrue(
      SetPolicyValue(base::Value(base::Value::List().Append("*/echo")),
                     /*block_url=*/false),
      @"policy value couldn't be set");
  WaitForURLBlockedStatus(self.testServer->GetURL("/echo"), false);

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];

  [ChromeEarlGrey waitForWebStateContainingText:"Echo"];
}

@end
