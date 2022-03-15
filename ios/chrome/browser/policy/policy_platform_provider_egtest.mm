// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/testing/earl_grey/earl_grey_test.h"

#include <memory>

#include "base/json/json_string_value_serializer.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/enterprise/browser/enterprise_switches.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#include "components/policy/policy_constants.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/chrome_switches.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/policy/policy_app_interface.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#include "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "ios/testing/earl_grey/app_launch_configuration.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Returns the value of a given policy, looked up in the current platform policy
// provider.
std::unique_ptr<base::Value> GetPlatformPolicy(const std::string& key) {
  std::string json_representation = base::SysNSStringToUTF8(
      [PolicyAppInterface valueForPlatformPolicy:base::SysUTF8ToNSString(key)]);
  JSONStringValueDeserializer deserializer(json_representation);
  return deserializer.Deserialize(/*error_code=*/nullptr,
                                  /*error_message=*/nullptr);
}

// Returns an AppLaunchConfiguration containing the given policy data.
// |policy_data| must be in XML format. |policy_data| is passed to the
// application regardless of whether |disable_policy| is true or false..
AppLaunchConfiguration GenerateAppLaunchConfiguration(std::string policy_data,
                                                      bool disable_policy,
                                                      bool enable_cbcm) {
  AppLaunchConfiguration config;

  if (disable_policy) {
    config.additional_args.push_back(std::string("--") +
                                     switches::kDisableEnterprisePolicy);
  } else {
    config.additional_args.push_back(std::string("--") +
                                     switches::kEnableEnterprisePolicy);
  }

  if (enable_cbcm) {
    config.additional_args.push_back(
        std::string("--") + switches::kEnableChromeBrowserCloudManagement);
  }

  // Remove whitespace from the policy data, because the XML parser does not
  // tolerate newlines.
  base::RemoveChars(policy_data, base::kWhitespaceASCII, &policy_data);

  // Commandline flags that start with a single "-" are automatically added to
  // the NSArgumentDomain in NSUserDefaults. Set fake policy data that can be
  // read by the production platform policy provider.
  config.additional_args.push_back(
      "-" + base::SysNSStringToUTF8(kPolicyLoaderIOSConfigurationKey));
  config.additional_args.push_back(policy_data);

  config.relaunch_policy = NoForceRelaunchAndResetState;
  return config;
}

}  // namespace

// Test case that uses the production platform policy provider.
@interface PolicyPlatformProviderTestCase : ChromeTestCase
@end

@implementation PolicyPlatformProviderTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  std::string policyData = "<dict>"
                           "    <key>EnableExperimentalPolicies</key>"
                           "    <array>"
                           "      <string>DefaultSearchProviderName</string>"
                           "      <string>SearchSuggestEnabled</string>"
                           "    </array>"
                           "    <key>DefaultSearchProviderName</key>"
                           "    <string>Test</string>"
                           "    <key>NotARegisteredPolicy</key>"
                           "    <string>Unknown</string>"
                           "    <key>SavingBrowserHistoryDisabled</key>"
                           "    <true/>"
                           "    <key>SearchSuggestEnabled</key>"
                           "    <false/>"
                           "</dict>";
  return GenerateAppLaunchConfiguration(policyData, /*disable_policy=*/false,
                                        /*enable_cbcm*/ false);
}

// Tests the values of policies that were explicitly set.
- (void)testPolicyExplicitlySet {
  std::unique_ptr<base::Value> searchValue =
      GetPlatformPolicy(policy::key::kDefaultSearchProviderName);
  GREYAssertTrue(searchValue && searchValue->is_string(),
                 @"searchValue was not of type string");
  GREYAssertEqual(searchValue->GetString(), "Test",
                  @"searchValue had an unexpected value");

  std::unique_ptr<base::Value> suggestValue =
      GetPlatformPolicy(policy::key::kSearchSuggestEnabled);
  GREYAssertTrue(suggestValue && suggestValue->is_bool(),
                 @"suggestValue was not of type bool");
  GREYAssertFalse(suggestValue->GetBool(),
                  @"suggestValue had an unexpected value");
}

// Test the value of a policy that exists in the schema but was not explicitly
// set.
- (void)testPolicyNotSet {
  std::unique_ptr<base::Value> blocklistValue =
      GetPlatformPolicy(policy::key::kURLBlocklist);
  GREYAssertTrue(blocklistValue && blocklistValue->is_none(),
                 @"blocklistValue was unexpectedly present");
}

// Test the value of a policy that was set in the configuration but is unknown
// to the policy system.
- (void)testPolicyUnknown {
  std::unique_ptr<base::Value> unknownValue =
      GetPlatformPolicy("NotARegisteredPolicy");
  GREYAssertTrue(unknownValue && unknownValue->is_string(),
                 @"unknownValue was not of type string");
  GREYAssertEqual(unknownValue->GetString(), "Unknown",
                  @"unknownValue had an unexpected value");
}

@end

// Test case that uses the production platform policy provider and explicitly
// disables policy.
@interface PolicyDisabledTestCase : ChromeTestCase
@end

@implementation PolicyDisabledTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  std::string policyData = "<dict>"
                           "    <key>EnableExperimentalPolicies</key>"
                           "    <array>"
                           "      <string>DefaultSearchProviderName</string>"
                           "    </array>"
                           "    <key>DefaultSearchProviderName</key>"
                           "    <string>Test</string>"
                           "</dict>";
  return GenerateAppLaunchConfiguration(policyData, /*disable_policy=*/true,
                                        /*enable_cbcm*/ false);
}

// Tests that about:policy is not available when policy is disabled. Also serves
// as a test that the browser does not crash on startup with policy disabled.
- (void)testAboutPolicyNotAvailable {
  [ChromeEarlGrey loadURL:GURL(kChromeUIPolicyURL)];
  [ChromeEarlGrey
      waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                        IDS_ERRORPAGES_HEADING_NOT_AVAILABLE)];
}

// Tests that policies are not loaded when policy is disabled.
- (void)testPoliciesAreNotLoaded {
  std::unique_ptr<base::Value> searchValue =
      GetPlatformPolicy(policy::key::kDefaultSearchProviderName);
  GREYAssertTrue(searchValue && searchValue->is_none(),
                 @"searchValue was unexpectedly set");
}

@end

// Test case that enables CBCM.
@interface CBCMEnabledTestCase : ChromeTestCase
@end

@implementation CBCMEnabledTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  std::string policyData = "<dict>"
                           "    <key>SearchSuggestEnabled</key>"
                           "    <false/>"
                           "</dict>";
  return GenerateAppLaunchConfiguration(policyData, /*disable_policy=*/false,
                                        /*enable_cbcm=*/true);
}

// Ensure that policies can still be correctly set, and that the browser is
// working normally by visiting the about:policy page.
- (void)testPoliciesWork {
  std::unique_ptr<base::Value> suggestValue =
      GetPlatformPolicy(policy::key::kSearchSuggestEnabled);
  GREYAssertTrue(suggestValue && suggestValue->is_bool(),
                 @"suggestValue was not of type bool");
  GREYAssertFalse(suggestValue->GetBool(),
                  @"suggestValue had an unexpected value");

  [ChromeEarlGrey loadURL:GURL(kChromeUIPolicyURL)];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_POLICY_SHOW_UNSET)];
}

@end

// Test case that disables CBCM.
@interface CBCMDisabledTestCase : ChromeTestCase
@end

@implementation CBCMDisabledTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  std::string policyData = "<dict>"
                           "    <key>SearchSuggestEnabled</key>"
                           "    <false/>"
                           "</dict>";
  return GenerateAppLaunchConfiguration(policyData, /*disable_policy=*/false,
                                        /*enable_cbcm=*/false);
}

// Ensure that policies can still be correctly set, and that the browser is
// working normally by visiting the about:policy page.
- (void)testPoliciesWork {
  std::unique_ptr<base::Value> suggestValue =
      GetPlatformPolicy(policy::key::kSearchSuggestEnabled);
  GREYAssertTrue(suggestValue && suggestValue->is_bool(),
                 @"suggestValue was not of type bool");
  GREYAssertFalse(suggestValue->GetBool(),
                  @"suggestValue had an unexpected value");

  [ChromeEarlGrey loadURL:GURL(kChromeUIPolicyURL)];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_POLICY_SHOW_UNSET)];
}

@end
