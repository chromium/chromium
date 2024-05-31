// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/content_settings/core/browser/content_settings_uma_util.h"
#import "components/content_settings/core/common/content_settings_types.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/optimization_guide/core/optimization_guide_switches.h"
#import "components/page_info/core/page_info_action.h"
#import "components/strings/grit/components_branded_strings.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/alert_constants.h"
#import "ios/chrome/browser/ui/page_info/features.h"
#import "ios/chrome/browser/ui/page_info/page_info_app_interface.h"
#import "ios/chrome/browser/ui/page_info/page_info_constants.h"
#import "ios/chrome/browser/ui/permissions/permissions_app_interface.h"
#import "ios/chrome/browser/ui/permissions/permissions_constants.h"
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

namespace {

using ::base::test::ios::kWaitForUIElementTimeout;
using ::base::test::ios::WaitUntilConditionOrTimeout;

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
// TODO(crbug.com/40222316): Test fails on device due to asking for microphone
// permission.
#if !TARGET_IPHONE_SIMULATOR
#define MAYBE_testShowOneAccessiblePermissionInPageInfo \
  DISABLED_testShowOneAccessiblePermissionInPageInfo
#else
#define MAYBE_testShowOneAccessiblePermissionInPageInfo \
  testShowOneAccessiblePermissionInPageInfo
#endif
- (void)MAYBE_testShowOneAccessiblePermissionInPageInfo {
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
// TODO(crbug.com/40222316): Test fails on device due to asking for microphone
// permission.
#if !TARGET_IPHONE_SIMULATOR
#define MAYBE_testShowTwoAccessiblePermissionsInPageInfo \
  DISABLED_testShowTwoAccessiblePermissionsInPageInfo
#else
#define MAYBE_testShowTwoAccessiblePermissionsInPageInfo \
  testShowTwoAccessiblePermissionsInPageInfo
#endif
- (void)MAYBE_testShowTwoAccessiblePermissionsInPageInfo {
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
