// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import <string>
#import <string_view>

#import "base/check.h"
#import "base/strings/strcat.h"
#import "base/strings/sys_string_conversions.h"
#import "components/enterprise/browser/enterprise_switches.h"
#import "components/enterprise/connectors/core/common.h"
#import "components/enterprise/connectors/core/connectors_prefs.h"
#import "components/enterprise/connectors/core/realtime_reporting_test_environment.h"
#import "components/enterprise/net/core/features.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "components/policy/core/common/policy_types.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/enterprise/connectors/analysis/test/analysis_connectors_app_interface.h"
#import "ios/chrome/browser/policy/model/policy_app_interface.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/components/enterprise/analysis/features.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/element_selector.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {

// Policy name and value for setting a fake enterprise enrollment token.
constexpr std::string_view kEnrollmentTokenPolicyName =
    "CloudManagementEnrollmentToken";
constexpr std::string_view kEnrollmentToken = "fake-enrollment-token";

// DOM element IDs on `chrome://management`.
constexpr std::string_view kConnectorsSectionId = "connectors-info";
constexpr std::string_view kSecureGatewaySectionId = "secure-gateway-section";
constexpr std::string_view kThreatProtectionMoreButtonId =
    "threat-protection-more";
constexpr std::string_view kSecurityEventSectionId = "security-event-section";
constexpr std::string_view kPageVisitEventSectionId =
    "page-visit-event-section";
constexpr std::string_view kFileDownloadEventSectionId =
    "file-download-event-section";
constexpr std::string_view kTextEnterEventSectionId =
    "text-enter-event-section";

// Returns an `ElementSelector` matching `#<element_id>:not(.hidden)` for
// enterprise event section.
ElementSelector* VisibleElementSelector(std::string_view element_id) {
  return [ElementSelector
      selectorWithCSSSelector:base::StrCat({"#", element_id, ":not(.hidden)"})];
}

// Returns the expected event title formatted for `event_title_id`.
std::string GetExpectedEventTitle(int event_title_id) {
  return base::StrCat(
      {l10n_util::GetStringUTF8(IDS_MANAGEMENT_CONNECTORS_EVENT), ": ",
       l10n_util::GetStringUTF8(event_title_id)});
}

// Returns the expected event description formatted for `event_description_id`.
std::string GetExpectedEventDescription(int event_description_id) {
  return base::StrCat(
      {l10n_util::GetStringUTF8(IDS_MANAGEMENT_CONNECTORS_VISIBLE_DATA), ": ",
       l10n_util::GetStringUTF8(event_description_id)});
}

// Verifies that `visible_section_id` is visible with the expected title and
// description after expanding the connectors section.
void VerifyVisibleEventSection(std::string_view visible_section_id,
                               int expected_title_id,
                               int expected_description_id) {
  [ChromeEarlGrey waitForWebStateContainingElement:VisibleElementSelector(
                                                       kConnectorsSectionId)];
  [ChromeEarlGrey
      waitForWebStateContainingElement:VisibleElementSelector(
                                           kThreatProtectionMoreButtonId)];
  [ChromeEarlGrey tapWebStateElementWithID:base::SysUTF8ToNSString(
                                               kThreatProtectionMoreButtonId)];
  [ChromeEarlGrey waitForWebStateContainingElement:VisibleElementSelector(
                                                       visible_section_id)];

  std::string expected_title = GetExpectedEventTitle(expected_title_id);
  std::string expected_description =
      GetExpectedEventDescription(expected_description_id);

  [ChromeEarlGrey waitForWebStateContainingText:expected_title];
  [ChromeEarlGrey waitForWebStateContainingText:expected_description];
}

}  // namespace

// Tests the Enterprise Connectors event sections show correct information.
@interface ManagementUITestCase : ChromeTestCase
@end

@implementation ManagementUITestCase {
  std::unique_ptr<enterprise_connectors::test::RealtimeReportingTestEnvironment>
      _reportingEnvironment;
}

#pragma mark - ChromeTestCase

// TODO(crbug.com/577136777): Simplify the setup for the test cases here.
- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;

  if ([self isRunningTest:@selector(testSecureGatewaySection)]) {
    config.features_enabled.push_back(
        enterprise_net::kEnableDynamicRouteFetching);
    config.additional_args.push_back(
        base::StrCat({"--", switches::kEnableChromeBrowserCloudManagement}));
  } else {
    config.additional_args.push_back(base::StrCat(
        {"-", base::SysNSStringToUTF8(kPolicyLoaderIOSConfigurationKey)}));
    config.additional_args.push_back(
        base::StrCat({"<dict><key>", kEnrollmentTokenPolicyName,
                      "</key><string>", kEnrollmentToken, "</string></dict>"}));
  }

  if ([self isRunningTest:@selector(testSecurityEventSection)]) {
    _reportingEnvironment =
        enterprise_connectors::test::RealtimeReportingTestEnvironment::Create();
    CHECK(_reportingEnvironment);
    CHECK(_reportingEnvironment->Start());
    std::vector<std::string> reporting_args =
        _reportingEnvironment->GetArguments();
    config.additional_args.insert(config.additional_args.end(),
                                  reporting_args.begin(), reporting_args.end());
    config.relaunch_policy = ForceRelaunchByCleanShutdown;
  }

  if ([self isRunningTest:@selector(testTextEnterEventSection)]) {
    config.features_enabled.push_back(
        enterprise_connectors::kEnableBulkDataEntryConnectorIOS);
  }

  return config;
}

- (void)tearDownHelper {
  [AnalysisConnectorsAppInterface clearBrowserDMToken];
  [AnalysisConnectorsAppInterface clearDownloadProtectionRules];
  [AnalysisConnectorsAppInterface clearBulkDataEntryRules];
  [PolicyAppInterface clearPolicies];
  [ChromeEarlGrey clearUserPrefWithName:enterprise_connectors::
                                            kEnterpriseRealTimeUrlCheckMode];
  [ChromeEarlGrey clearUserPrefWithName:enterprise_connectors::
                                            kEnterpriseRealTimeUrlCheckScope];
  _reportingEnvironment.reset();
  [super tearDownHelper];
}

#pragma mark - Tests

// Tests that management page displays the Secure Gateway title and
// description when `ProxyProvisioningDomains` is configured.
- (void)testSecureGatewaySection {
  [PolicyAppInterface
      mergeCloudUserPolicyValue:@"[{\"pvd_id\": \"example.com\"}]"
                         forKey:@"ProxyProvisioningDomains"];

  [ChromeEarlGrey loadURL:GURL(kChromeUIManagementURL)];

  [ChromeEarlGrey waitForWebStateContainingElement:VisibleElementSelector(
                                                       kConnectorsSectionId)];
  [ChromeEarlGrey
      waitForWebStateContainingElement:VisibleElementSelector(
                                           kSecureGatewaySectionId)];

  [ChromeEarlGrey
      waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                        IDS_MANAGEMENT_SECURE_GATEWAY_TITLE)];
  [ChromeEarlGrey
      waitForWebStateContainingText:
          l10n_util::GetStringUTF8(IDS_MANAGEMENT_SECURE_GATEWAY_DISCLOSURE)];
}

// Tests that management page displays the Security event title and
// description when `OnSecurityEventEnterpriseConnector` is enabled.
- (void)testSecurityEventSection {
  [ChromeEarlGrey loadURL:GURL(kChromeUIManagementURL)];

  VerifyVisibleEventSection(kSecurityEventSectionId,
                            IDS_MANAGEMENT_ENTERPRISE_REPORTING_EVENT,
                            IDS_MANAGEMENT_ENTERPRISE_REPORTING_VISIBLE_DATA);
}

// Tests that management page displays the Page Visit event title and
// description when `EnterpriseRealTimeUrlCheckMode` is enabled.
- (void)testPageVisitEventSection {
  [AnalysisConnectorsAppInterface setBrowserDMToken];
  [ChromeEarlGrey
      setIntegerValue:enterprise_connectors::
                          REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED
          forUserPref:enterprise_connectors::kEnterpriseRealTimeUrlCheckMode];
  [ChromeEarlGrey
      setIntegerValue:policy::POLICY_SCOPE_MACHINE
          forUserPref:enterprise_connectors::kEnterpriseRealTimeUrlCheckScope];

  [ChromeEarlGrey loadURL:GURL(kChromeUIManagementURL)];

  VerifyVisibleEventSection(kPageVisitEventSectionId,
                            IDS_MANAGEMENT_PAGE_VISITED_EVENT,
                            IDS_MANAGEMENT_PAGE_VISITED_VISIBLE_DATA);
}

// Tests that management page displays the File Download event title and
// description when `OnFileDownloadedEnterpriseConnector` is enabled.
- (void)testFileDownloadEventSection {
  [AnalysisConnectorsAppInterface setDownloadProtectionRules];

  [ChromeEarlGrey loadURL:GURL(kChromeUIManagementURL)];

  VerifyVisibleEventSection(kFileDownloadEventSectionId,
                            IDS_MANAGEMENT_FILE_DOWNLOADED_EVENT,
                            IDS_MANAGEMENT_FILE_DOWNLOADED_VISIBLE_DATA);
}

// Tests that management page displays the Text Enter event title and
// description when `OnBulkDataEntryEnterpriseConnector` is enabled.
- (void)testTextEnterEventSection {
  [AnalysisConnectorsAppInterface setBulkDataEntryRules];

  [ChromeEarlGrey loadURL:GURL(kChromeUIManagementURL)];

  VerifyVisibleEventSection(kTextEnterEventSectionId,
                            IDS_MANAGEMENT_TEXT_ENTERED_EVENT,
                            IDS_MANAGEMENT_TEXT_ENTERED_VISIBLE_DATA);
}

@end
