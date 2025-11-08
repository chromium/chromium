// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import <tuple>
#import <utility>

#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/signin/internal/identity_manager/account_capabilities_constants.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/browser_container/ui_bundled/edit_menu_app_interface.h"
#import "ios/chrome/browser/browser_container/ui_bundled/edit_menu_matchers.h"
#import "ios/chrome/browser/explain_with_gemini/coordinator/explain_with_gemini_constants.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/reader_mode/model/features.h"
#import "ios/chrome/browser/reader_mode/ui/constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/element_selector.h"
#import "net/base/url_util.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "ui/base/l10n/l10n_util_mac.h"

using ::base::test::ios::kWaitForUIElementTimeout;
using ::base::test::ios::WaitUntilConditionOrTimeout;

namespace {

const char kCSSSelectorToLongPress[] = "em";

// Returns an ElementSelector for `ElementToLongPress`.
ElementSelector* ElementToLongPressSelector() {
  return [ElementSelector selectorWithCSSSelector:kCSSSelectorToLongPress];
}

// An HTML template that puts some text in a simple span element.
const char kBasicSelectionUrl[] = "/basic";
const char kBasicSelectionHtmlTemplate[] =
    "<html>"
    "  <head>"
    "    <meta name='viewport' content='width=device-width, "
    "      initial-scale=1.0, maximum-scale=1.0, user-scalable=no' "
    "    />"
    "  </head>"
    "  <body>"
    "    Page Loaded <br/><br/>"
    "    This text contains a <em>text</em>.<br/><br/><br/>"
    "    Other very interesting text<br/>"
    "    Other very interesting text<br/>"
    "    Other very interesting text<br/>"
    "    Other very interesting text<br/>"
    "    Other very interesting text<br/>"
    "    Other very interesting text<br/>"
    "    Other very interesting text<br/>"
    "    Other very interesting text<br/>"
    "    Other very interesting text<br/>"
    "    Other very interesting text<br/>"
    "    Other very interesting text<br/>"
    "    Other very interesting text<br/>"
    "  </body>"
    "</html>";

std::unique_ptr<net::test_server::HttpResponse> StandardResponse(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  GURL request_url = request.GetURL();

  if (request_url.path() == kBasicSelectionUrl) {
    http_response->set_content(kBasicSelectionHtmlTemplate);
    return std::move(http_response);
  }
  return nullptr;
}

}  // namespace

// Tests for the Search With Edit menu entry.
@interface ExplainWithGeminiMediatorTestCase : ChromeTestCase
@property(nonatomic, strong) FakeSystemIdentity* fakeIdentity;
@end

@implementation ExplainWithGeminiMediatorTestCase

// TODO(crbug.com/429537743): The test fails on device.
#if TARGET_OS_SIMULATOR
#define MAYBE_testExplainWithGeminiInReadingMode \
  testExplainWithGeminiInReadingMode
#else
#define MAYBE_testExplainWithGeminiInReadingMode \
  DISABLED_testExplainWithGeminiInReadingMode
#endif

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled_and_params.push_back(
      {kExplainGeminiEditMenu, {{{kExplainGeminiEditMenuParams, "2"}}}});
  config.features_enabled_and_params.push_back(
      {kBWGPromoConsent, {{{kBWGPromoConsentParams, "3"}}}});
  if ([self
          isRunningTest:@selector(MAYBE_testExplainWithGeminiInReadingMode)]) {
    config.features_enabled_and_params.push_back({kEnableReaderMode, {}});
    config.features_enabled_and_params.push_back({kEnableReaderModeInUS, {}});
  }
  return config;
}

- (void)setUp {
  [super setUp];
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  self.fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:self.fakeIdentity
                 withCapabilities:@{
                   @(kCanUseModelExecutionFeaturesName) : @YES,
                 }];
  [SigninEarlGrey signinWithFakeIdentity:self.fakeIdentity];
}

// Conveniently load a page that has "text" in a selectable field.
- (void)loadPage {
  const GURL pageURL = self.testServer->GetURL(kBasicSelectionUrl);
  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForWebStateContainingText:"Page Loaded"];
  [ChromeEarlGrey waitForWebStateZoomScale:1.0];
}

// Checks if Explain With Gemini button in the Edit Menu and when clicking on it
// forwards to the Gemini page. This is done with an account matching
// `kCanUseModelExecutionFeaturesName` capability.
- (void)testExplainWithGemini {
  [self loadPage];
  [ChromeEarlGreyUI triggerEditMenu:ElementToLongPressSelector()];
  id<GREYMatcher> matcher = FindEditMenuActionWithAccessibilityLabel([NSString
      stringWithFormat:@"✦ %@", l10n_util::GetNSString(
                                    IDS_IOS_EXPLAIN_GEMINI_EDIT_MENU)]);
  GREYAssertNotEqual(matcher, nil, @"✦ Explain button not found");

  // Scope for the synchronization disabled.
  {
    ScopedSynchronizationDisabler syncDisabler;

    [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];

    ConditionBlock condition = ^{
      NSError* error = nil;

      GREYAssertEqual([ChromeEarlGrey webStateVisibleURL],
                      GURL(kExplainWithGeminiURL),
                      @"Does not redirect to Gemini URL.");
      return !error;
    };
    GREYAssert(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, condition),
               @"Does not redirect to Gemini URL");

    // TODO(crbug.com/409525576): Add waitForWebStateContainingText for `Explain
    // this to me` once rollout is done by Gemini team.
    GREYAssertEqual(2UL, [ChromeEarlGrey mainTabCount],
                    @"Search Should be in new tab");
    [ChromeEarlGrey closeCurrentTab];
  }
  // End of the sync disabler scope.
}

// Checks if Explain With Gemini button does not appear in the Edit Menu in
// incognito mode.
- (void)testExplainWithGeminiIncognito {
  [ChromeEarlGrey openNewIncognitoTab];
  [self loadPage];
  [ChromeEarlGreyUI triggerEditMenu:ElementToLongPressSelector()];
  id<GREYMatcher> matcher = FindEditMenuActionWithAccessibilityLabel([NSString
      stringWithFormat:@"✦ %@", l10n_util::GetNSString(
                                    IDS_IOS_EXPLAIN_GEMINI_EDIT_MENU)]);
  GREYAssertEqual(matcher, nil, @"✦ Explain button was found");
}

// Checks if Explain With Gemini button does not appear in Edit Menu when signed
// out.
- (void)testExplainWithGeminiSignedOut {
  [SigninEarlGrey signOut];
  [SigninEarlGrey verifySignedOut];
  [self loadPage];
  [ChromeEarlGreyUI triggerEditMenu:ElementToLongPressSelector()];
  id<GREYMatcher> matcher = FindEditMenuActionWithAccessibilityLabel([NSString
      stringWithFormat:@"✦ %@", l10n_util::GetNSString(
                                    IDS_IOS_EXPLAIN_GEMINI_EDIT_MENU)]);
  GREYAssertEqual(matcher, nil, @"✦ Explain button was found");
}

// Checks if Explain With Gemini button does not appear in Edit Menu when the
// account does not match `kCanUseModelExecutionFeaturesName` capability.
- (void)testExplainWithGeminiNotMatchingCapability {
  [SigninEarlGrey forgetFakeIdentity:self.fakeIdentity];
  [SigninEarlGrey signOut];
  [SigninEarlGrey verifySignedOut];

  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity withUnknownCapabilities:YES];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];

  [self loadPage];
  [ChromeEarlGreyUI triggerEditMenu:ElementToLongPressSelector()];
  id<GREYMatcher> matcher = FindEditMenuActionWithAccessibilityLabel([NSString
      stringWithFormat:@"✦ %@", l10n_util::GetNSString(
                                    IDS_IOS_EXPLAIN_GEMINI_EDIT_MENU)]);
  GREYAssertEqual(matcher, nil, @"✦ Explain button was found");
}

// Checks if Explain With Gemini button does not appear in Edit Menu with a
// managed account.
- (void)testExplainWithGeminiManagedAccount {
  [SigninEarlGrey forgetFakeIdentity:self.fakeIdentity];
  [SigninEarlGrey signOut];
  [SigninEarlGrey verifySignedOut];
  FakeSystemIdentity* fakeManagedIdentity =
      [FakeSystemIdentity fakeManagedIdentity];
  [SigninEarlGrey
      signinWithFakeManagedIdentityInPersonalProfile:fakeManagedIdentity];
  [self loadPage];
  [ChromeEarlGreyUI triggerEditMenu:ElementToLongPressSelector()];
  id<GREYMatcher> matcher = FindEditMenuActionWithAccessibilityLabel([NSString
      stringWithFormat:@"✦ %@", l10n_util::GetNSString(
                                    IDS_IOS_EXPLAIN_GEMINI_EDIT_MENU)]);
  GREYAssertEqual(matcher, nil, @"✦ Explain button was found");
}

// Checks if Explain With Gemini button is present in Reading Mode.
- (void)MAYBE_testExplainWithGeminiInReadingMode {
  [self loadPage];

  // Open Reader Mode UI.
  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      @"Reader mode content could not be loaded.");
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeViewAccessibilityIdentifier)];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeChipViewAccessibilityIdentifier)];

  [ChromeEarlGreyUI triggerEditMenu:ElementToLongPressSelector()];
  id<GREYMatcher> matcher = FindEditMenuActionWithAccessibilityLabel([NSString
      stringWithFormat:@"✦ %@", l10n_util::GetNSString(
                                    IDS_IOS_EXPLAIN_GEMINI_EDIT_MENU)]);
  GREYAssertNotEqual(matcher, nil, @"✦ Explain button not found");
}

@end
