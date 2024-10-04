// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/content_settings/core/browser/content_settings_uma_util.h"
#import "components/content_settings/core/common/content_settings_types.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/optimization_guide/core/optimization_guide_switches.h"
#import "components/page_info/core/page_info_action.h"
#import "components/strings/grit/components_branded_strings.h"
#import "components/strings/grit/components_strings.h"
#import "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/history/ui_bundled/history_ui_constants.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/alert_constants.h"
#import "ios/chrome/browser/permissions/ui_bundled/permissions_app_interface.h"
#import "ios/chrome/browser/permissions/ui_bundled/permissions_constants.h"
#import "ios/chrome/browser/ui/menu/menu_action_type.h"
#import "ios/chrome/browser/ui/page_info/features.h"
#import "ios/chrome/browser/ui/page_info/page_info_app_interface.h"
#import "ios/chrome/browser/ui/page_info/page_info_constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/permissions/permissions.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util.h"

using chrome_test_util::DeleteButton;

namespace {

using ::base::test::ios::kWaitForUIElementTimeout;
using ::base::test::ios::WaitUntilConditionOrTimeout;
using chrome_test_util::HistoryEntry;
using chrome_test_util::NavigationBarDoneButton;

// Endpoints for the local server.
char kURL1[] = "/firstURL";
char kURL2[] = "/secondURL";
// Title and content of the external website used for testing.
const char kTitleAndContentOfExternalWebsite[] = "Example Domain";
// URL (as string) of the external website.
const char kURLExternalWebsiteString[] = "https://www.example.com";
// URL of the external website.
const GURL kURLExternalWebsite = GURL(kURLExternalWebsiteString);

// Matcher infobar modal camera permissions switch.
id<GREYMatcher> CameraPermissionsSwitch(BOOL isOn) {
  return chrome_test_util::TableViewSwitchCell(
      kPageInfoCameraSwitchAccessibilityIdentifier, isOn);
}

// Matcher infobar modal microphone permissions switch.
id<GREYMatcher> MicrophonePermissionsSwitch(BOOL isOn) {
  return chrome_test_util::TableViewSwitchCell(
      kPageInfoMicrophoneSwitchAccessibilityIdentifier, isOn);
}

// Matcher for the search button.
id<GREYMatcher> SearchIconButton() {
  return grey_accessibilityID(kHistorySearchControllerSearchBarIdentifier);
}

// Matcher for Security help center link in footer.
id<GREYMatcher> SecurityHelpCenterLink() {
  return grey_allOf(
      // The link is within the security footer with ID
      // `kPageInfoSecurityFooterAccessibilityIdentifier`.
      grey_ancestor(
          grey_accessibilityID(kPageInfoSecurityFooterAccessibilityIdentifier)),
      // UIKit instantiates a `UIAccessibilityLinkSubelement` for the link
      // element in the label with attributed string.
      grey_kindOfClassName(@"UIAccessibilityLinkSubelement"),
      grey_accessibilityTrait(UIAccessibilityTraitLink), nil);
}

void AddAboutThisSiteHint(GURL url) {
  [PageInfoAppInterface
      addAboutThisSiteHintForURL:
          [NSString stringWithCString:url.spec().c_str()
                             encoding:[NSString defaultCStringEncoding]]
                     description:
                         @"A domain used in illustrative examples in documents"
                aboutThisSiteURL:@"https://diner.com"];
}

void ExpectPageInfoActionHistograms(page_info::PageInfoAction action) {
  GREYAssertNil(
      [MetricsAppInterface
           expectCount:1
             forBucket:action
          forHistogram:base::SysUTF8ToNSString(
                           page_info::kWebsiteSettingsActionHistogram)],
      @"WebsiteSettings.Action histogram not logged.");
}

void ExpectPermissionChangedHistograms(ContentSettingsType type) {
  int bucket =
      content_settings_uma_util::ContentSettingTypeToHistogramValue(type);
  GREYAssertNil([MetricsAppInterface
                     expectCount:1
                       forBucket:bucket
                    forHistogram:base::SysUTF8ToNSString(
                                     kOriginInfoPermissionChangedHistogram)],
                @"PermissionChanged histogram not logged.");
  GREYAssertNil(
      [MetricsAppInterface
           expectCount:1
             forBucket:bucket
          forHistogram:base::SysUTF8ToNSString(
                           kOriginInfoPermissionChangedBlockedHistogram)],
      @"PermissionChanged.Blocked histogram not logged.");
  ExpectPageInfoActionHistograms(page_info::PAGE_INFO_CHANGED_PERMISSION);
}

void ExpectContextMenuLastVisitedHistoryEntryActionsHistogram(
    int count,
    MenuActionType action) {
  GREYAssertNil(
      [MetricsAppInterface
           expectCount:count
             forBucket:static_cast<int>(action)
          forHistogram:@"Mobile.ContextMenu.LastVisitedHistoryEntry.Actions"],
      @"Mobile.ContextMenu.LastVisitedHistoryEntry.Actions histogram for the "
      @"%d action "
      @"page entries did not have count %d.",
      static_cast<int>(action), count);
}

}  // namespace

@interface PageInfoTestCase : ChromeTestCase
@end

@implementation PageInfoTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.relaunch_policy = NoForceRelaunchAndResetState;

  config.features_enabled.push_back(
      feature_engagement::kIPHiOSInlineEnhancedSafeBrowsingPromoFeature);
  if ([self isRunningTest:@selector(testLegacySecuritySection)]) {
    config.features_disabled.push_back(kRevampPageInfoIos);
  } else {
    config.features_enabled.push_back(kRevampPageInfoIos);
  }
  config.features_enabled.push_back(kPageInfoLastVisitedIOS);
  config.additional_args.push_back(
      std::string("-") +
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  return config;
}

- (void)setUp {
  [super setUp];
  [ChromeEarlGrey clearBrowsingHistory];
  GREYAssertNil([MetricsAppInterface setupHistogramTester],
                @"Failed to set up histogram tester.");
}

- (void)tearDown {
  [super tearDown];
  GREYAssertNil([MetricsAppInterface releaseHistogramTester],
                @"Cannot reset histogram tester.");
}

// Checks that if the alert for site permissions pops up, and allow it.
- (void)checkAndAllowPermissionAlerts {
  // Allow system permission if shown.
  NSError* systemAlertFoundError = nil;
  [[EarlGrey selectElementWithMatcher:grey_systemAlertViewShown()]
      assertWithMatcher:grey_nil()
                  error:&systemAlertFoundError];
  if (systemAlertFoundError) {
    NSError* acceptAlertError = nil;
    [self grey_acceptSystemDialogWithError:&acceptAlertError];
    GREYAssertNil(acceptAlertError, @"Error accepting system alert.\n%@",
                  acceptAlertError);
  }
  // Allow site permission.
  id<GREYMatcher> dialogMatcher =
      grey_accessibilityID(kPermissionsDialogAccessibilityIdentifier);
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:dialogMatcher]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return !error;
  };
  GREYAssert(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, condition),
             @"Permissions dialog was not shown.");
  NSString* allowButtonText = l10n_util::GetNSString(
      IDS_IOS_PERMISSIONS_ALERT_DIALOG_BUTTON_TEXT_GRANT);

  id<GREYMatcher> allowButtonMatcher = allowButtonMatcher = grey_allOf(
      grey_ancestor(dialogMatcher), grey_accessibilityLabel(allowButtonText),
      grey_accessibilityTrait(UIAccessibilityTraitStaticText), nil);

  [[[EarlGrey selectElementWithMatcher:allowButtonMatcher]
      assertWithMatcher:grey_sufficientlyVisible()] performAction:grey_tap()];
}

// Checks `expectedStatesForPermissions` matches the actual states for
// permissions of the active web state; checks will fail if there is no active
// web state.
- (void)checkStatesForPermissions:
    (NSDictionary<NSNumber*, NSNumber*>*)expectedStatesForPermissions {
  NSDictionary<NSNumber*, NSNumber*>* actualStatesForPermissions =
      [PermissionsAppInterface statesForAllPermissions];
  GREYAssertEqualObjects(
      expectedStatesForPermissions[@(web::PermissionCamera)],
      actualStatesForPermissions[@(web::PermissionCamera)],
      @"Camera state: %@ does not match expected: %@.",
      actualStatesForPermissions[@(web::PermissionCamera)],
      expectedStatesForPermissions[@(web::PermissionCamera)]);
  GREYAssertEqualObjects(
      expectedStatesForPermissions[@(web::PermissionMicrophone)],
      actualStatesForPermissions[@(web::PermissionMicrophone)],
      @"Microphone state: %@ does not match expected: %@.",
      actualStatesForPermissions[@(web::PermissionMicrophone)],
      expectedStatesForPermissions[@(web::PermissionMicrophone)]);
}

// Tests that rotating the device will don't dismiss the page info view.
- (void)testShowPageInfoRotation {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGreyUI openPageInfo];

  ExpectPageInfoActionHistograms(page_info::PAGE_INFO_OPENED);

  // Checks that the page info view has appeared.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPageInfoViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Rotates the device and checks that the page info view is still presented.
  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeRight
                                error:nil];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPageInfoViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Closes the page info using the 'Done' button.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPageInfoViewAccessibilityIdentifier)]
      assertWithMatcher:grey_nil()];
}

// Tests that opening the page info on a Chromium page displays the correct
// information.
- (void)testShowPageInfoChromePage {
  [ChromeEarlGrey loadURL:GURL("chrome://version")];
  [ChromeEarlGreyUI openPageInfo];

  // Checks that the page info view has appeared.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPageInfoViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Checks that “You’re viewing a secure Chrome page.” is displayed.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_PAGE_INFO_INTERNAL_PAGE))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the Permissions section is not displayed, as there isn't any
// accessible permissions.
- (void)testShowPageInfoWithNoAccessiblePermission {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGreyUI openPageInfo];
  // Checks that no permissions are not visible.
  [[EarlGrey
      selectElementWithMatcher:grey_anyOf(CameraPermissionsSwitch(YES),
                                          CameraPermissionsSwitch(NO), nil)]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey
      selectElementWithMatcher:grey_anyOf(MicrophonePermissionsSwitch(YES),
                                          MicrophonePermissionsSwitch(NO), nil)]
      assertWithMatcher:grey_notVisible()];
}

// Tests that single accessible permission is shown in Permissions section with
// toggle.
- (void)testShowOneAccessiblePermissionInPageInfo {
  // Open a page that requests microphone permissions.
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  [ChromeEarlGrey
      loadURL:self.testServer->GetURL("/permissions/microphone_only.html")];
  [self checkAndAllowPermissionAlerts];

  // Check that camera permission item is hidden, and in accordance with the
  // web state permission states.
  [ChromeEarlGreyUI openPageInfo];
  [self checkStatesForPermissions:@{
    @(web::PermissionCamera) : @(web::PermissionStateNotAccessible),
    @(web::PermissionMicrophone) : @(web::PermissionStateAllowed)
  }];
  [[EarlGrey
      selectElementWithMatcher:grey_anyOf(CameraPermissionsSwitch(YES),
                                          CameraPermissionsSwitch(NO), nil)]
      assertWithMatcher:grey_notVisible()];
  // Check that microphone permission item is visible, and turn it off.
  [[EarlGrey selectElementWithMatcher:MicrophonePermissionsSwitch(YES)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];
  [[EarlGrey  // Dismiss view.
      selectElementWithMatcher:grey_accessibilityID(
                                   kPageInfoViewAccessibilityIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
  [self checkStatesForPermissions:@{
    @(web::PermissionCamera) : @(web::PermissionStateNotAccessible),
    @(web::PermissionMicrophone) : @(web::PermissionStateBlocked)
  }];

  // Check that the correct histograms are logged when a camera permission is
  // changed via Page Info.
  ExpectPermissionChangedHistograms(ContentSettingsType::MEDIASTREAM_MIC);
}

// Tests that two accessible permissions are shown in Permissions section with
// toggle.
- (void)testShowTwoAccessiblePermissionsInPageInfo {
  // TODO(crbug.com/342245057): Camera access is broken in the simulator on iOS
  // 17.5.
  if (@available(iOS 17.5, *)) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS 17.5.");
  }
  // Open a page that requests microphone permissions.
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  [ChromeEarlGrey loadURL:self.testServer->GetURL(
                              "/permissions/camera_and_microphone.html")];
  [self checkAndAllowPermissionAlerts];

  // Check that switchs for both permissions are visible.
  [ChromeEarlGreyUI openPageInfo];
  [self checkStatesForPermissions:@{
    @(web::PermissionCamera) : @(web::PermissionStateAllowed),
    @(web::PermissionMicrophone) : @(web::PermissionStateAllowed)
  }];
  // Check that both permission item is visible, and turn off camera
  // permission.
  [[EarlGrey selectElementWithMatcher:MicrophonePermissionsSwitch(YES)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:CameraPermissionsSwitch(YES)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];
  [[EarlGrey  // Dismiss view.
      selectElementWithMatcher:grey_accessibilityID(
                                   kPageInfoViewAccessibilityIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
  [self checkStatesForPermissions:@{
    @(web::PermissionCamera) : @(web::PermissionStateBlocked),
    @(web::PermissionMicrophone) : @(web::PermissionStateAllowed)
  }];

  // Check that the correct histograms are logged when a camera permission is
  // changed via Page Info.
  ExpectPermissionChangedHistograms(ContentSettingsType::MEDIASTREAM_CAMERA);
}

// Tests that rotating the device will not dismiss the navigation bar.
- (void)testShowPageInfoTitleRotation {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGreyUI openPageInfo];

  // Check that the navigation bar is visible.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kPageInfoViewNavigationBarAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check that the navigation bar has both the page info's page title and the
  // page URL.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_PAGE_INFO_SITE_INFORMATION))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 grey_text([NSString
                     stringWithCString:[ChromeEarlGrey webStateVisibleURL]
                                           .host()
                                           .c_str()
                              encoding:[NSString defaultCStringEncoding]])]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Rotate to landscape mode and check the navigation bar is still visible.
  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeRight
                                error:nil];
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kPageInfoViewNavigationBarAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Rotate back to portrait mode and check the navigation bar is still visible.
  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationPortrait error:nil];
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kPageInfoViewNavigationBarAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests the legacy security section by checking that the correct site security
// label and that the security footer are displayed.
- (void)testLegacySecuritySection {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGreyUI openPageInfo];

  // Check that "Site Security | Not secure” is displayed.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_PAGE_INFO_SITE_SECURITY))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_PAGE_INFO_SECURITY_STATUS_NOT_SECURE))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check that the security footer is displayed.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kPageInfoSecurityFooterAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on the Learn more link.
  [[EarlGrey selectElementWithMatcher:SecurityHelpCenterLink()]
      performAction:grey_tap()];

  // Check that the help center article was opened.
  GREYAssertEqual(std::string("support.google.com"),
                  [ChromeEarlGrey webStateVisibleURL].host(),
                  @"Did not navigate to the help center article.");
  ExpectPageInfoActionHistograms(page_info::PAGE_INFO_CONNECTION_HELP_OPENED);
}

// Tests the security section by checking that the correct connection label is
// displayed, that no security footer is displayed and that clicking on the
// security row leads to the security subpage.
- (void)testSecuritySection {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGreyUI openPageInfo];

  // Check that “Connection | Not secure” is displayed.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_PAGE_INFO_CONNECTION))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_PAGE_INFO_SECURITY_STATUS_NOT_SECURE))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check that the security footer is not displayed.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kPageInfoSecurityFooterAccessibilityIdentifier)]
      assertWithMatcher:grey_notVisible()];

  // Check that tapping on the security row leads to the security subpage.
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_PAGE_INFO_SECURITY_STATUS_NOT_SECURE))]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kPageInfoSecurityViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  ExpectPageInfoActionHistograms(page_info::PAGE_INFO_SECURITY_DETAILS_OPENED);
}

// Most of the tests for AboutThisSite section are in chrome-internal
// (chrome/test/external_url/external_url_ssl_app_interface.mm).
// It seems that https pages in egtests always have an invalid certificate,
// NET::ERR_CERT_AUTHORITY_INVALID, which makes the page unsecure. The
// AboutThisSite section should only be available for secure pages. Tests in
// chrome-internal can use real pages and so we are able to test Page Info with
// secure pages.

// Tests that the AboutThisSite section does not appear even if optimization
// guide returns a hint but the connection is HTTP. The AboutThisSite section
// should only appear for secure pages.
- (void)testAboutThisSiteSectionWithHttp {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  GURL url = self.testServer->GetURL("/");

  AddAboutThisSiteHint(url);
  [ChromeEarlGrey loadURL:url];
  [ChromeEarlGreyUI openPageInfo];

  // Check that AboutThisSite section is not displayed.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_PAGE_INFO_ABOUT_THIS_PAGE))]
      assertWithMatcher:grey_nil()];
}

// Tests that the AboutThisSite section does not appear even if the connection
// is HTTPs and optimization guide returns a hint but the certificate is not
// valid. The AboutThisSite section should only appear for secure pages.
- (void)testAboutThisSiteSectionWithHttpsInvalidCert {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  GREYAssertTrue(https_server.Start(), @"Test server failed to start.");
  GURL url = https_server.GetURL("/");

  AddAboutThisSiteHint(url);
  [ChromeEarlGrey loadURL:url];
  [ChromeEarlGreyUI openPageInfo];

  // Check that AboutThisSite section is not displayed.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_PAGE_INFO_ABOUT_THIS_PAGE))]
      assertWithMatcher:grey_nil()];
}

// Tests that the Last Visited section is not displayed when there is no
// previous visit to the current website.
- (void)testLastVisitedSectionWithNoPreviousVisit {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  AddAboutThisSiteHint(kURLExternalWebsite);
  [ChromeEarlGrey loadURL:kURLExternalWebsite];
  [ChromeEarlGreyUI openPageInfo];

  // Check that Last Visited section is not displayed.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_PAGE_INFO_HISTORY))]
      assertWithMatcher:grey_nil()];
}

// Tests that the Last Visited section is displayed when there exists a previous
// visit, and also, it tests that the correct timestamp of the last visit is
// presented.
- (void)testLastVisitedSectionDisplaysYesterday {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  // Create an entry in History which took place one day ago on `URL`.
  const base::Time oneDayAgo = base::Time::Now() - base::Hours(24);
  [ChromeEarlGrey addHistoryServiceTypedURL:kURLExternalWebsite
                             visitTimestamp:oneDayAgo];

  // Visit `url` and open Page Info.
  AddAboutThisSiteHint(kURLExternalWebsite);
  [ChromeEarlGrey loadURL:kURLExternalWebsite];
  [ChromeEarlGreyUI openPageInfo];

  // Wait for the Last Visited row to be displayed.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_text(l10n_util::GetNSString(
                                              IDS_PAGE_INFO_HISTORY))];

  // Check that the Last Visited summary displays "Yesterday".
  [[EarlGrey
      selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                   IDS_PAGE_INFO_HISTORY_LAST_VISIT_YESTERDAY))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that tapping on the Last Visited row reveals the Last Visited subpage.
- (void)testLastVisitedSubpage {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  // Create an entry in History which took place one day ago on
  // `kURLExternalWebsite`.
  const base::Time oneDayAgo = base::Time::Now() - base::Hours(24);
  [ChromeEarlGrey addHistoryServiceTypedURL:kURLExternalWebsite
                             visitTimestamp:oneDayAgo];

  // Visit `URL` and open Page Info.
  AddAboutThisSiteHint(kURLExternalWebsite);
  [ChromeEarlGrey loadURL:kURLExternalWebsite];
  [ChromeEarlGreyUI openPageInfo];

  // Check that tapping on the Last Visited Row leads to the Last Visited
  // subpage.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_PAGE_INFO_HISTORY))]
      performAction:grey_tap()];

  // Assert that Last Visited subpage displays one entry.
  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          kURLExternalWebsite)),
              kTitleAndContentOfExternalWebsite)]
      assertWithMatcher:grey_notNil()];

  // Assert that page_info::PAGE_INFO_HISTORY_OPENED metric was recorded.
  ExpectPageInfoActionHistograms(page_info::PAGE_INFO_HISTORY_OPENED);
}

// Tests that tapping on the show full history button leads to the history page.
// Additionally, it tests that dismissing full history reveals back the Last
// Visited subpage.
- (void)testLastVisitedSubpageOpensFullHistory {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  // Create an entry in History which took place one day ago on
  // `kURLExternalWebsite`.
  const base::Time oneDayAgo = base::Time::Now() - base::Hours(24);
  [ChromeEarlGrey addHistoryServiceTypedURL:kURLExternalWebsite
                             visitTimestamp:oneDayAgo];

  // Visit `URL` and open Page Info.
  AddAboutThisSiteHint(kURLExternalWebsite);
  [ChromeEarlGrey loadURL:kURLExternalWebsite];
  [ChromeEarlGreyUI openPageInfo];

  // Open Last Visited page.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_PAGE_INFO_HISTORY))]
      performAction:grey_tap()];

  // Open full history by pressing on the "Show Full History" button.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kHistoryToolbarShowFullHistoryButtonIdentifier)]
      performAction:grey_tap()];

  // Check that full history page is displayed.
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check that dismissing the full history reveals the Last Visited subpage.
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()
              error:nil];

  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          kURLExternalWebsite)),
              kTitleAndContentOfExternalWebsite)]
      assertWithMatcher:grey_notNil()];

  // Assert that page_info::PAGE_INFO_SHOW_FULL_HISTORY_CLICKED metric was
  // recorded.
  ExpectPageInfoActionHistograms(
      page_info::PAGE_INFO_SHOW_FULL_HISTORY_CLICKED);
}

// Tests that tapping on a history entry from the Last Visited subpage dismisses
// Page Info (which presents the Last Visited subpage) and opens the
// corresponding URL.
- (void)testOpeningURLFromLastVisitedDismissesPageInfo {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  // Create an entry in History which took place one day ago on
  // `kURLExternalWebsite`.
  const base::Time oneDayAgo = base::Time::Now() - base::Hours(24);
  [ChromeEarlGrey addHistoryServiceTypedURL:kURLExternalWebsite
                             visitTimestamp:oneDayAgo];

  // Visit `URL` and open Page Info.
  AddAboutThisSiteHint(kURLExternalWebsite);
  [ChromeEarlGrey loadURL:kURLExternalWebsite];
  [ChromeEarlGreyUI openPageInfo];

  // Open Last Visited page.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_PAGE_INFO_HISTORY))]
      performAction:grey_tap()];

  // Tap on the latest history entry from the Last Visited subpage.
  [[[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          kURLExternalWebsite)),
              kTitleAndContentOfExternalWebsite)] atIndex:0]
      performAction:grey_tap()];

  // Assert that the corresponding URL was opened.
  [ChromeEarlGrey
      waitForWebStateContainingText:kTitleAndContentOfExternalWebsite];

  // Assert that page_info::PAGE_INFO_HISTORY_ENTRY_CLICKED metric was recorded.
  ExpectPageInfoActionHistograms(page_info::PAGE_INFO_HISTORY_ENTRY_CLICKED);
}

// Tests that tapping on a history entry dismisses both full history and the
// underlying Page Info (which presents the Last Visited subpage).
- (void)testOpeningURLFromFullHistoryDismissesPageInfo {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  // Create an entry in History which took place one day ago on
  // `kURLExternalWebsite`.
  const base::Time oneDayAgo = base::Time::Now() - base::Hours(24);
  [ChromeEarlGrey addHistoryServiceTypedURL:kURLExternalWebsite
                             visitTimestamp:oneDayAgo];

  // Visit `URL` and open Page Info.
  AddAboutThisSiteHint(kURLExternalWebsite);
  [ChromeEarlGrey loadURL:kURLExternalWebsite];
  [ChromeEarlGreyUI openPageInfo];

  // Open Last Visited page.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_PAGE_INFO_HISTORY))]
      performAction:grey_tap()];

  // Open full history by pressing on the "Show Full History" button.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kHistoryToolbarShowFullHistoryButtonIdentifier)]
      performAction:grey_tap()];

  // Check that tapping on the older URL (from full history) dismisses both full
  // history and Page Info. `atIndex:1` is required because two entries would be
  // matched (current visit and the visit from one day ago) and we want to
  // select the last one (i.e. the older history entry).
  [[[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          kURLExternalWebsite)),
              kTitleAndContentOfExternalWebsite)] atIndex:1]
      performAction:grey_tap()];

  [ChromeEarlGrey
      waitForWebStateContainingText:kTitleAndContentOfExternalWebsite];
}

// Tests display and selection of 'Open in New Tab' in a context menu on a
// history entry from the Last Visited subpage.
- (void)testContextMenuOpenInNewTab {
  // At the beginning of the test, the Context Menu Last Visited History Entry
  // Actions metric should be empty.
  ExpectContextMenuLastVisitedHistoryEntryActionsHistogram(
      /*count=*/0, /*action=*/MenuActionType::OpenInNewTab);

  // Create an entry in History which took place one day ago on
  // `kURLExternalWebsite`.
  const base::Time oneDayAgo = base::Time::Now() - base::Hours(24);
  [ChromeEarlGrey addHistoryServiceTypedURL:kURLExternalWebsite
                             visitTimestamp:oneDayAgo];

  // Visit `kURLExternalWebsite`.
  AddAboutThisSiteHint(kURLExternalWebsite);
  [ChromeEarlGrey loadURL:kURLExternalWebsite];

  // Open Page Info.
  [ChromeEarlGreyUI openPageInfo];

  // Open Last Visited page.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_PAGE_INFO_HISTORY))]
      performAction:grey_tap()];

  // Long press on the latest history element.
  [[[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          kURLExternalWebsite)),
              kTitleAndContentOfExternalWebsite)] atIndex:0]
      performAction:grey_longPress()];

  // Select "Open in New Tab" and confirm that new tab is opened with selected
  // URL.
  [ChromeEarlGrey verifyOpenInNewTabActionWithURL:kURLExternalWebsiteString];

  // Assert that the Context Menu Last Visited History Entry Actions metric is
  // populated.
  ExpectContextMenuLastVisitedHistoryEntryActionsHistogram(
      /*count=*/1, /*action=*/MenuActionType::OpenInNewTab);
}

// Tests display and selection of 'Open in New Window' in a context menu on a
// history entry from the Last Visited subpage.
- (void)testContextMenuOpenInNewWindow {
  if (![ChromeEarlGrey areMultipleWindowsSupported]) {
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");
  }

  // At the beginning of the test, the Context Menu Last Visited History Entry
  // Actions metric should be empty.
  ExpectContextMenuLastVisitedHistoryEntryActionsHistogram(
      /*count=*/0, /*action=*/MenuActionType::OpenInNewWindow);

  // Create an entry in History which took place one day ago on
  // `kURLExternalWebsite`.
  const base::Time oneDayAgo = base::Time::Now() - base::Hours(24);
  [ChromeEarlGrey addHistoryServiceTypedURL:kURLExternalWebsite
                             visitTimestamp:oneDayAgo];

  // Visit `kURLExternalWebsite`.
  AddAboutThisSiteHint(kURLExternalWebsite);
  [ChromeEarlGrey loadURL:kURLExternalWebsite];

  // Open Page Info.
  [ChromeEarlGreyUI openPageInfo];

  // Open Last Visited page.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_PAGE_INFO_HISTORY))]
      performAction:grey_tap()];

  // Long press on the latest history element.
  [[[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          kURLExternalWebsite)),
              kTitleAndContentOfExternalWebsite)] atIndex:0]
      performAction:grey_longPress()];

  // Select "Open in New Window" and confirm that new window is opened with
  // selected URL.
  [ChromeEarlGrey
      verifyOpenInNewWindowActionWithContent:kTitleAndContentOfExternalWebsite];

  // Assert that the Context Menu History Entry Actions metric is populated.
  ExpectContextMenuLastVisitedHistoryEntryActionsHistogram(
      /*count=*/1, /*action=*/MenuActionType::OpenInNewWindow);
}

// Tests display and selection of 'Open in New Incognito Tab' in a context menu
// on a history entry from the Last Visited subpage.
- (void)testContextMenuOpenInNewIncognitoTab {
  // At the beginning of the test, the Context Menu Last Visited History Entry
  // Actions metric should be empty.
  ExpectContextMenuLastVisitedHistoryEntryActionsHistogram(
      /*count=*/0, /*action=*/MenuActionType::OpenInNewIncognitoTab);

  // Create an entry in History which took place one day ago on
  // `kURLExternalWebsite`.
  const base::Time oneDayAgo = base::Time::Now() - base::Hours(24);
  [ChromeEarlGrey addHistoryServiceTypedURL:kURLExternalWebsite
                             visitTimestamp:oneDayAgo];

  // Visit `kURLExternalWebsite`.
  AddAboutThisSiteHint(kURLExternalWebsite);
  [ChromeEarlGrey loadURL:kURLExternalWebsite];

  // Open Page Info.
  [ChromeEarlGreyUI openPageInfo];

  // Open Last Visited page.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_PAGE_INFO_HISTORY))]
      performAction:grey_tap()];

  // Long press on the latest history element.
  [[[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          kURLExternalWebsite)),
              kTitleAndContentOfExternalWebsite)] atIndex:0]
      performAction:grey_longPress()];

  // Select "Open in New Incognito Tab" and confirm that new tab is opened in
  // incognito with the selected URL.
  [ChromeEarlGrey verifyOpenInIncognitoActionWithURL:kURLExternalWebsiteString];

  // Assert that the Context Menu Last Visited History Entry Actions metric is
  // populated.
  ExpectContextMenuLastVisitedHistoryEntryActionsHistogram(
      /*count=*/1, /*action=*/MenuActionType::OpenInNewIncognitoTab);
}

// Tests display and selection of 'Copy URL' in a context menu on a history
// entry from the Last Visited subpage.
- (void)testContextMenuCopy {
  // At the beginning of the test, the Context Menu Last Visited History Entry
  // Actions metric should be empty.
  ExpectContextMenuLastVisitedHistoryEntryActionsHistogram(
      /*count=*/0, /*action=*/MenuActionType::CopyURL);

  // Create an entry in History which took place one day ago on
  // `kURLExternalWebsite`.
  const base::Time oneDayAgo = base::Time::Now() - base::Hours(24);
  [ChromeEarlGrey addHistoryServiceTypedURL:kURLExternalWebsite
                             visitTimestamp:oneDayAgo];

  // Visit `kURLExternalWebsite`.
  AddAboutThisSiteHint(kURLExternalWebsite);
  [ChromeEarlGrey loadURL:kURLExternalWebsite];

  // Open Page Info.
  [ChromeEarlGreyUI openPageInfo];

  // Open Last Visited page.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_PAGE_INFO_HISTORY))]
      performAction:grey_tap()];

  // Long press on the latest history element.
  [[[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          kURLExternalWebsite)),
              kTitleAndContentOfExternalWebsite)] atIndex:0]
      performAction:grey_longPress()];

  // Tap "Copy URL" and wait for the URL to be copied to the pasteboard.
  [ChromeEarlGrey
      verifyCopyLinkActionWithText:[NSString
                                       stringWithUTF8String:kURLExternalWebsite
                                                                .spec()
                                                                .c_str()]];

  // Assert that the Context Menu Last Visited History Entry Actions metric is
  // populated.
  ExpectContextMenuLastVisitedHistoryEntryActionsHistogram(
      /*count=*/1, /*action=*/MenuActionType::CopyURL);
}

// Tests display and selection of "Share" in the context menu for a history
// entry from the Last Visited subpage.
- (void)testContextMenuShare {
  // At the beginning of the test, the Context Menu Last Visited History Entry
  // Actions metric should be empty.
  ExpectContextMenuLastVisitedHistoryEntryActionsHistogram(
      /*count=*/0, /*action=*/MenuActionType::Share);

  // Create an entry in History which took place one day ago on
  // `kURLExternalWebsite`.
  const base::Time oneDayAgo = base::Time::Now() - base::Hours(24);
  [ChromeEarlGrey addHistoryServiceTypedURL:kURLExternalWebsite
                             visitTimestamp:oneDayAgo];

  // Visit `kURLExternalWebsite`.
  AddAboutThisSiteHint(kURLExternalWebsite);
  [ChromeEarlGrey loadURL:kURLExternalWebsite];

  // Open Page Info.
  [ChromeEarlGreyUI openPageInfo];

  // Open Last Visited page.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_PAGE_INFO_HISTORY))]
      performAction:grey_tap()];

  // Long press on the history element.
  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          kURLExternalWebsite)),
              kTitleAndContentOfExternalWebsite)]
      performAction:grey_longPress()];

  [ChromeEarlGrey
      verifyShareActionWithURL:kURLExternalWebsite
                     pageTitle:[NSString
                                   stringWithUTF8String:
                                       kTitleAndContentOfExternalWebsite]];

  // Assert that the Context Menu Last Visited History Entry Actions metric is
  // populated.
  ExpectContextMenuLastVisitedHistoryEntryActionsHistogram(
      /*count=*/1, /*action=*/MenuActionType::Share);
}

// Tests the Delete context menu action for a History entry from the Last
// Visited subpage.
- (void)testContextMenuDelete {
  // At the beginning of the test, the Context Menu Last Visited History Entry
  // Actions metric should be empty.
  ExpectContextMenuLastVisitedHistoryEntryActionsHistogram(
      /*count=*/0, /*action=*/MenuActionType::Delete);

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL URL1 = self.testServer->GetURL(kURL1);
  const GURL URL2 = self.testServer->GetURL(kURL2);

  // Create two entries in History which took place one day ago and two days
  // ago, respectively.
  const base::Time oneDayAgo = base::Time::Now() - base::Hours(24);
  const base::Time twoDaysAgo = base::Time::Now() - base::Hours(48);
  [ChromeEarlGrey addHistoryServiceTypedURL:URL1 visitTimestamp:oneDayAgo];
  [ChromeEarlGrey addHistoryServiceTypedURL:URL2 visitTimestamp:twoDaysAgo];

  // Visit `URL1`.
  AddAboutThisSiteHint(URL1);
  [ChromeEarlGrey loadURL:URL1];

  // Open Page Info.
  [ChromeEarlGreyUI openPageInfo];

  // Open Last Visited page.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_PAGE_INFO_HISTORY))]
      performAction:grey_tap()];

  // Long press on the history element.
  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          URL1)),
              URL1.GetContent())] performAction:grey_longPress()];

  [[EarlGrey selectElementWithMatcher:DeleteButton()] performAction:grey_tap()];

  // Wait for the animations to be done.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          URL2)),
              URL2.GetContent())];

  // Assert that the deleted entry is gone.
  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          URL1)),
              URL1.GetContent())] assertWithMatcher:grey_nil()];

  // Assert that the Context Menu Last Visited History Entry Actions metric is
  // populated.
  ExpectContextMenuLastVisitedHistoryEntryActionsHistogram(
      /*count=*/1, /*action=*/MenuActionType::Delete);

  // Assert that page_info::PAGE_INFO_HISTORY_ENTRY_REMOVED metric was recorded.
  ExpectPageInfoActionHistograms(page_info::PAGE_INFO_HISTORY_ENTRY_REMOVED);
}

// Tests that we don't crash when showing the page info twice (prevent
// regression from (crbug.com/1486309).
- (void)testShowingPageInfoTwice {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGreyUI openPageInfo];

  // Check that the page info view has appeared.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPageInfoViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPageInfoViewAccessibilityIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Check that the page info view has disappeared.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPageInfoViewAccessibilityIdentifier)]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];

  // Reopen the page.
  [ChromeEarlGreyUI openPageInfo];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPageInfoViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end
