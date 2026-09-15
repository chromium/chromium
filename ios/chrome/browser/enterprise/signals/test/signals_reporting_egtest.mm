// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import <optional>
#import <string>
#import <string_view>

#import "base/json/json_reader.h"
#import "base/strings/strcat.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/values.h"
#import "components/enterprise/browser/reporting/reporting_features.h"
#import "components/policy/policy_constants.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/policy/model/policy_app_interface.h"
#import "ios/chrome/browser/policy/model/policy_earl_grey_utils.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/element_selector.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

using policy_test_utils::MergeCloudUserPolicy;
using policy_test_utils::MergePolicy;
using policy_test_utils::SetPolicy;

namespace {

// Ids of the device signals disclosure bullets in chrome://management. They
// are only un-hidden when `deviceSignalsDisclosureEnabled` is true, which
// requires both the iOS signal sharing feature and a reporting policy.
const char kBrowserSignalsDisclosureId[] = "browser-signals-disclosure";
const char kProfileSignalsDisclosureId[] = "profile-signals-disclosure";

// Id of the profile reporting section in chrome://management. Used as a
// "control" element to make sure the page rendered before asserting that the
// disclosure is absent.
const char kProfileReportingInfoId[] = "profile-reporting-info";

// Id of the wrapper around every managed-profile section in
// chrome://management. It is un-hidden by the page script once it knows the
// profile is managed.
const char kManagedInfoId[] = "managed-info";

// Returns a selector matching an element which exists and does not have the
// "hidden" class.
ElementSelector* VisibleElementSelector(std::string_view element_id) {
  return [ElementSelector
      selectorWithCSSSelector:base::StrCat({"#", element_id, ":not(.hidden)"})];
}

// Returns the chrome://connectors-internals URL.
GURL ConnectorsInternalsURL() {
  return GURL(base::StrCat({"chrome://", kChromeUIConnectorsInternalsHost}));
}

// Returns a selector matching the element matching `css_selector` inside the
// "Signals Reporting" tab of chrome://connectors-internals. The tab is built
// out of nested custom elements, so the shadow roots have to be traversed.
ElementSelector* SignalsReportingElementSelector(
    std::string_view css_selector) {
  return [ElementSelector
      selectorWithCSSSelector:base::StrCat({"connectors-internals-app",
                                            kElementSelectorShadowDelimiter,
                                            "connectors-tabs",
                                            kElementSelectorShadowDelimiter,
                                            "signals-reporting",
                                            kElementSelectorShadowDelimiter,
                                            css_selector})];
}

// Returns the JavaScript evaluating to the `innerText` of the element matching
// `css_selector` in the "Signals Reporting" tab, or to an empty string while
// the tab is still initializing and the element does not exist yet.
NSString* SignalsReportingFieldTextScript(std::string_view css_selector) {
  return
      [NSString stringWithFormat:@"(function() {"
                                  "  const element = %@;"
                                  "  return element ? element.innerText : '';"
                                  "})()",
                                 SignalsReportingElementSelector(css_selector)
                                     .selectorScript];
}

// Evaluates `script` in the current web state and returns its result, or an
// empty string if it did not evaluate to a string.
NSString* EvaluateStringScript(NSString* script) {
  base::Value value = [ChromeEarlGrey evaluateJavaScript:script];
  return value.is_string() ? base::SysUTF8ToNSString(value.GetString()) : @"";
}

// Returns the `innerText` of the element matching `css_selector` in the
// "Signals Reporting" tab.
NSString* SignalsReportingFieldText(std::string_view css_selector) {
  return EvaluateStringScript(SignalsReportingFieldTextScript(css_selector));
}
// Waits until the element matching `css_selector` in the "Signals Reporting"
// tab has a non-empty `innerText`, and returns it.
NSString* WaitForSignalsReportingFieldText(std::string_view css_selector) {
  NSString* script = SignalsReportingFieldTextScript(css_selector);
  __block NSString* field_text = @"";
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(

                 base::test::ios::kWaitForActionTimeout,
                 ^{
                   field_text = EvaluateStringScript(script);
                   return field_text.length > 0;
                 }),
             @"Timed out waiting for the signals reporting tab to render.");
  return field_text;
}

// Returns the value of `key` in `signals`, or nil if it is absent. Signals are
// returned as NSString so that they can be compared with the object-based
// GREYAssert macros; the scalar ones do not support C++ types.
NSString* SignalValue(const base::DictValue& signals, const char* key) {
  const std::string* value = signals.FindString(key);
  return value ? base::SysUTF8ToNSString(*value) : nil;
}

}  // namespace

// Common setup for the tests of the user-visible surfaces of the iOS
// enterprise signal sharing feature: the device signals disclosure in
// chrome://management and the "Signals Reporting" tab of
// chrome://connectors-internals. See go/cep-se-ios-signals-sharing-dd.
//
// The state of the signal sharing feature is part of the app launch
// configuration and cannot be changed while the app is running, so the tests
// are split between the two subclasses below, which each pin the feature to a
// single state for all of their tests.
@interface SignalsReportingTestCaseBase : ChromeTestCase
@end

@implementation SignalsReportingTestCaseBase

// Prevents this base class from being run directly by the XCTest runner.
- (void)invokeTest {
  if ([self isMemberOfClass:[SignalsReportingTestCaseBase class]]) {
    return;
  }
  [super invokeTest];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.relaunch_policy = NoForceRelaunchAndResetState;
  return config;
}

- (void)tearDownHelper {
  [PolicyAppInterface clearPolicies];
  [super tearDownHelper];
}

@end

// Tests the signal sharing surfaces with the iOS signal sharing feature
// enabled.
@interface SignalsReportingTestCase : SignalsReportingTestCaseBase
@end

@implementation SignalsReportingTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_enabled.push_back(
      enterprise_reporting::kIOSSignalSharingEnabled);
  return config;
}

#pragma mark - chrome://management

// Tests that the device signals disclosure is shown in the profile reporting
// section when profile reporting and security signals reporting are enabled.
- (void)testManagementPageProfileSignalsDisclosure {
  MergeCloudUserPolicy(true, policy::key::kCloudProfileReportingEnabled);
  MergeCloudUserPolicy(true, policy::key::kUserSecuritySignalsReporting);

  [ChromeEarlGrey loadURL:GURL(kChromeUIManagementURL)];
  // `waitForWebStateContainingText:` matches the whole document's textContent,
  // including hidden nodes, so assert on the element's visibility instead.
  [ChromeEarlGrey
      waitForWebStateContainingElement:VisibleElementSelector(
                                           kProfileReportingInfoId)];

  [ChromeEarlGrey
      waitForWebStateContainingElement:VisibleElementSelector(
                                           kProfileSignalsDisclosureId)];
  [ChromeEarlGrey
      waitForWebStateContainingText:
          l10n_util::GetStringUTF8(IDS_MANAGEMENT_DEVICE_SIGNALS_DISCLOSURE)];
}

// Tests that the device signals disclosure is shown in the browser reporting
// section when browser level reporting is enabled.
- (void)testManagementPageBrowserSignalsDisclosure {
  MergePolicy(true, policy::key::kCloudReportingEnabled);

  [ChromeEarlGrey loadURL:GURL(kChromeUIManagementURL)];
  [ChromeEarlGrey waitForWebStateContainingText:
                      l10n_util::GetStringUTF8(
                          IDS_MANAGEMENT_BROWSER_REPORTING_EXPLANATION)];

  [ChromeEarlGrey
      waitForWebStateContainingElement:VisibleElementSelector(
                                           kBrowserSignalsDisclosureId)];
}

// Tests that no device signals disclosure is shown for a managed profile that
// has no reporting policy enabled.
- (void)testManagementPageNoSignalsDisclosureWithoutReporting {
  // Any policy makes the browser managed, which is required for the reporting
  // sections to be evaluated at all.
  SetPolicy(false, policy::key::kTranslateEnabled);

  [ChromeEarlGrey loadURL:GURL(kChromeUIManagementURL)];
  // Wait for the page script to reveal the managed section, otherwise the
  // assertions below could pass simply because nothing has been rendered yet.
  [ChromeEarlGrey
      waitForWebStateContainingElement:VisibleElementSelector(kManagedInfoId)];

  GREYAssertFalse(
      [ChromeEarlGrey webStateContainsElement:VisibleElementSelector(
                                                  kProfileSignalsDisclosureId)],
      @"Profile signals disclosure is unexpectedly visible.");
  GREYAssertFalse(
      [ChromeEarlGrey webStateContainsElement:VisibleElementSelector(
                                                  kBrowserSignalsDisclosureId)],
      @"Browser signals disclosure is unexpectedly visible.");
}

#pragma mark - chrome://connectors-internals

// Tests that the signals reporting tab reports that security signals reporting
// is disabled when the UserSecuritySignalsReporting policy is not set.
- (void)testConnectorsInternalsSignalsReportingDisabledWithoutPolicy {
  MergeCloudUserPolicy(true, policy::key::kCloudProfileReportingEnabled);

  [ChromeEarlGrey loadURL:ConnectorsInternalsURL()];

  NSString* enabled_text =
      WaitForSignalsReportingFieldText("#signals-reporting-enabled");
  GREYAssertEqualObjects(@"false", enabled_text,
                         @"Security signals reporting should be disabled.");

  // No signals should have been collected.
  GREYAssertEqualObjects(@"{}", SignalsReportingFieldText("#signals-json"),
                         @"Signals were unexpectedly collected.");
}

// Tests that enabling the UserSecuritySignalsReporting policy makes Chrome
// collect the iOS device signals described in the design doc, and that the
// collected values have the expected format.
- (void)testConnectorsInternalsCollectsIOSSignals {
  MergeCloudUserPolicy(true, policy::key::kCloudProfileReportingEnabled);
  MergeCloudUserPolicy(true, policy::key::kUserSecuritySignalsReporting);

  [ChromeEarlGrey loadURL:ConnectorsInternalsURL()];

  NSString* enabled_text =
      WaitForSignalsReportingFieldText("#signals-reporting-enabled");
  GREYAssertEqualObjects(@"true", enabled_text,
                         @"Security signals reporting should be enabled.");

  NSString* signals_json = WaitForSignalsReportingFieldText("#signals-json");
  std::optional<base::DictValue> parsed_signals = base::JSONReader::ReadDict(
      base::SysNSStringToUTF8(signals_json), base::JSON_PARSE_RFC);
  GREYAssertTrue(parsed_signals.has_value(),
                 @"Signals are not a dictionary: %@", signals_json);
  const base::DictValue& signals_dict = parsed_signals.value();

  // The client reports raw, unmapped values; the server is responsible for
  // normalizing them (e.g. "iOS" -> "IOS", "Apple Inc." -> "Apple", and the
  // machine identifier -> a readable model name). Do not "fix" these strings
  // to match the design doc's table: changing them would silently break the
  // server-side mapping.

  // `osType` is hardcoded on iOS.
  GREYAssertEqualObjects(@"iOS", SignalValue(signals_dict, "operating_system"),
                         @"Unexpected operating_system value.");

  // `deviceManufacturer` is hardcoded to "Apple Inc." by
  // base::SysInfo::GetHardwareInfoSync() on iOS.
  GREYAssertEqualObjects(@"Apple Inc.",
                         SignalValue(signals_dict, "device_manufacturer"),
                         @"Unexpected device_manufacturer value.");

  // `osVersion` and `deviceModel` come from the device. `deviceModel` is the
  // raw machine identifier (e.g. "iPhone14,3"), or "iOS Simulator (<model>)"
  // when running on a simulator, so only assert that it is populated.
  GREYAssertTrue(SignalValue(signals_dict, "os_version").length > 0,
                 @"os_version is missing or empty.");
  GREYAssertTrue(SignalValue(signals_dict, "device_model").length > 0,
                 @"device_model is missing or empty.");

  // `vendorId` is the device identifier reported to EMM. It must never be
  // empty, otherwise the report cannot be joined with the device record.
  GREYAssertTrue(SignalValue(signals_dict, "vendor_id").length > 0,
                 @"vendor_id is missing or empty.");

  // `diskEncryption` mirrors `screenLockSecured` on iOS.
  NSString* screen_lock_secured =
      SignalValue(signals_dict, "screen_lock_secured");
  GREYAssertTrue(screen_lock_secured.length > 0,
                 @"screen_lock_secured is missing.");
  GREYAssertEqualObjects(screen_lock_secured,
                         SignalValue(signals_dict, "disk_encryption"),
                         @"disk_encryption should mirror screen_lock_secured.");

  // `deviceName` is not collected on iOS for privacy reasons.
  GREYAssertTrue(SignalValue(signals_dict, "display_name").length == 0,
                 @"display_name should not be reported on iOS.");
}

@end

// Tests the signal sharing surfaces with the iOS signal sharing feature
// disabled. The feature is disabled by default, but disable it explicitly so
// that the tests keep testing the disabled behavior once it is enabled by
// default or by a field trial testing config.
@interface SignalsReportingFeatureDisabledTestCase
    : SignalsReportingTestCaseBase
@end

@implementation SignalsReportingFeatureDisabledTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_disabled.push_back(
      enterprise_reporting::kIOSSignalSharingEnabled);
  return config;
}

#pragma mark - chrome://management

// Tests that the device signals disclosure stays hidden when the iOS signal
// sharing feature is disabled, even if reporting policies are enabled.
- (void)testManagementPageNoSignalsDisclosure {
  MergeCloudUserPolicy(true, policy::key::kCloudProfileReportingEnabled);
  MergeCloudUserPolicy(true, policy::key::kUserSecuritySignalsReporting);

  [ChromeEarlGrey loadURL:GURL(kChromeUIManagementURL)];
  // The profile reporting section is still shown, only the signals disclosure
  // bullet is missing.
  [ChromeEarlGrey
      waitForWebStateContainingElement:VisibleElementSelector(
                                           kProfileReportingInfoId)];

  GREYAssertFalse(
      [ChromeEarlGrey webStateContainsElement:VisibleElementSelector(
                                                  kProfileSignalsDisclosureId)],
      @"Profile signals disclosure is unexpectedly visible.");
}

#pragma mark - chrome://connectors-internals

// Tests that the signals reporting tab reports that the feature is
// unsupported when the iOS signal sharing feature is disabled.
- (void)testConnectorsInternalsUnsupported {
  MergeCloudUserPolicy(true, policy::key::kUserSecuritySignalsReporting);

  [ChromeEarlGrey loadURL:ConnectorsInternalsURL()];

  NSString* error_text = WaitForSignalsReportingFieldText("#error-text");
  GREYAssertTrue(
      [error_text containsString:@"unsupported on the current platform"],
      @"Unexpected error text: %@", error_text);
}

@end
