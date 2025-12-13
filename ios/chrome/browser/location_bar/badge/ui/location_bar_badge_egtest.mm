// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/command_line.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "components/omnibox/browser/omnibox_pref_names.h"
#import "components/optimization_guide/core/hints/optimization_metadata.h"
#import "components/optimization_guide/core/optimization_guide_switches.h"
#import "components/optimization_guide/proto/contextual_cueing_metadata.pb.h"
#import "components/optimization_guide/proto/hints.pb.h"
#import "components/signin/internal/identity_manager/account_capabilities_constants.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey_app_interface.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/page_action_menu/utils/ai_hub_constants.h"
#import "ios/chrome/browser/location_bar/badge/ui/location_bar_badge_constants.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_test_app_interface.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Returns the matcher for the Ask Gemini chip.
id<GREYMatcher> AskGeminiChipMatcher() {
  return grey_accessibilityID(kLocationBarBadgeLabelIdentifier);
}

std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
    const net::test_server::HttpRequest& request) {
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_content(request.relative_url);
  http_response->set_content_type("text/html");
  return http_response;
}

}  // anonymous namespace

@interface LocationBarBadgeTestCase : ChromeTestCase
@end

@implementation LocationBarBadgeTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled_and_params.push_back({kPageActionMenu, {}});

  if ([self isRunningTest:@selector
            (testAskGeminiChipDoesNotShowForNonConsentedUsers)]) {
    config.features_enabled_and_params.push_back({kAskGeminiChip, {}});
  } else {
    config.features_enabled_and_params.push_back(
        {kAskGeminiChip, {{kAskGeminiChipIgnoreCriteria, "true"}}});
  }

  return config;
}

- (void)setUp {
  [super setUp];
  [self setupIdentity];
  [self setupPrefsForTestCase];
  [self setupTestServer];
  [self setupOptimizationGuide];
}

#pragma mark - Helpers

// Sets up a fake identity for the test.
- (void)setupIdentity {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyAppInterface
       addFakeIdentity:fakeIdentity
      withCapabilities:@{@(kCanUseModelExecutionFeaturesName) : @YES}];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
  [ChromeEarlGrey setIntegerValue:0 forUserPref:prefs::kGeminiEnabledByPolicy];
}

// Sets up test case specific pref values.
- (void)setupPrefsForTestCase {
  if ([self isRunningTest:@selector
            (testAskGeminiChipDoesNotShowForNonConsentedUsers)] ||
      [self isRunningTest:@selector(testAskGeminiChipIgnoreCriteria)]) {
    [ChromeEarlGrey setBoolValue:NO forUserPref:prefs::kIOSBwgConsent];
  }
  if ([self isRunningTest:@selector(testAskGeminiChipThrottling)]) {
    [ChromeEarlGrey setBoolValue:YES forUserPref:prefs::kIOSBwgConsent];
  }
  if ([self isRunningTest:@selector(testAskGeminiChipWithBottomOmnibox)]) {
    [ChromeEarlGrey setBoolValue:YES
               forLocalStatePref:omnibox::kIsOmniboxInBottomPosition];
  }
}

// Sets up the optimization guide for the test.
- (void)setupOptimizationGuide {
  [OptimizationGuideTestAppInterface
      registerOptimizationType:optimization_guide::proto::
                                   GLIC_CONTEXTUAL_CUEING];

  optimization_guide::proto::GlicContextualCueingMetadata metadata;
  metadata.add_cueing_configurations()->set_cue_label("Ask Gemini");
  NSData* metadata_data =
      [NSData dataWithBytes:metadata.SerializeAsString().c_str()
                     length:metadata.ByteSizeLong()];

  [self addAskGeminiHintForURL:"/echo" data:metadata_data];
  [self addAskGeminiHintForURL:"/echo_safe_page" data:metadata_data];
}

// Adds a hint for `relative_url` with the Gemini contextual cue.
- (void)addAskGeminiHintForURL:(const std::string&)relative_url
                          data:(NSData*)metadata {
  [OptimizationGuideTestAppInterface
      addHintForTesting:base::SysUTF8ToNSString(
                            self.testServer->GetURL(relative_url).spec())
                   type:optimization_guide::proto::GLIC_CONTEXTUAL_CUEING
         serialized_any:metadata
               type_url:
                   @"type.googleapis.com/"
                   @"optimization_guide.proto.GlicContextualCueingMetadata"];
}

// Sets up the test server.
- (void)setupTestServer {
  self.testServer->RegisterRequestHandler(base::BindRepeating(&HandleRequest));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

- (void)tearDownHelper {
  [SigninEarlGrey signOut];
  [super tearDownHelper];
}

// Tests that the Ask Gemini chip appears and displays correct text.
- (void)testAskGeminiChipAppears {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];
  [ChromeEarlGrey waitForWebStateContainingText:"/echo"];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:AskGeminiChipMatcher()];
}

// Tests that the Ask Gemini chip does not show in Incognito mode.
- (void)testAskGeminiChipDoesNotShowInIncognito {
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];
  [ChromeEarlGrey waitForWebStateContainingText:"/echo"];
  [ChromeEarlGrey
      waitForNotSufficientlyVisibleElementWithMatcher:AskGeminiChipMatcher()];
}

// Tests that the Ask Gemini chip appears when the omnibox is at the bottom.
- (void)testAskGeminiChipWithBottomOmnibox {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];
  [ChromeEarlGrey waitForWebStateContainingText:"/echo"];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:AskGeminiChipMatcher()];
}

// Tests that the Ask Gemini chip does not show for non-consented users.
- (void)testAskGeminiChipDoesNotShowForNonConsentedUsers {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];
  [ChromeEarlGrey waitForWebStateContainingText:"/echo"];
  [ChromeEarlGrey
      waitForNotSufficientlyVisibleElementWithMatcher:AskGeminiChipMatcher()];
}

// Tests that the "Ignore Criteria" flag forces the Ask Gemini chip to show.
- (void)testAskGeminiChipIgnoreCriteria {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];
  [ChromeEarlGrey waitForWebStateContainingText:"/echo"];

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:AskGeminiChipMatcher()];
}

// Tests time-based throttling for the Ask Gemini chip (2-hour cooldown).
- (void)testAskGeminiChipThrottling {
  // Set the last displayed time to a long time ago to ensure the chip is not
  // throttled at the beginning of the test.
  base::Time time = base::Time::Now() - base::Days(1);
  [ChromeEarlGrey
      setTimeValue:time
       forUserPref:prefs::kLastGeminiContextualChipDisplayedTimestamp];

  // Show the chip once.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];
  [ChromeEarlGrey waitForWebStateContainingText:"/echo"];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:AskGeminiChipMatcher()];

  // Try to show the chip again, it should be throttled.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo_safe_page")];
  [ChromeEarlGrey waitForWebStateContainingText:"/echo_safe_page"];
  [ChromeEarlGrey
      waitForNotSufficientlyVisibleElementWithMatcher:AskGeminiChipMatcher()];

  // Set the last displayed time to more than 2 hours ago.
  time = base::Time::Now() - base::Hours(3);
  [ChromeEarlGrey
      setTimeValue:time
       forUserPref:prefs::kLastGeminiContextualChipDisplayedTimestamp];

  // The chip should be shown again.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];
  [ChromeEarlGrey waitForWebStateContainingText:"/echo"];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:AskGeminiChipMatcher()];
}

@end
