// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/ui/assistant_container_constants.h"
#import "ios/chrome/browser/scene/ui/scene_ui_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/third_party/earl_grey2/src/CommonLib/Matcher/GREYLayoutConstraint.h"  // nogncheck
#import "net/test/embedded_test_server/embedded_test_server.h"

namespace {

// Returns a simple response for the test server.
std::unique_ptr<net::test_server::HttpResponse> SimpleResponse(
    const net::test_server::HttpRequest& request) {
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content("<html><body>Assistant Test</body></html>");
  return http_response;
}

// Returns a constraint that ensures the element is to the right of the
// reference element (Left attribute >= Right attribute of reference).
GREYLayoutConstraint* RightOf() {
  return [GREYLayoutConstraint
      layoutConstraintWithAttribute:kGREYLayoutAttributeLeft
                          relatedBy:kGREYLayoutRelationGreaterThanOrEqual
               toReferenceAttribute:kGREYLayoutAttributeRight
                         multiplier:1.0
                           constant:0.0];
}

// Long presses the omnibox and taps "Ask Gemini" to open the assistant.
void OpenAssistantFromOmnibox() {
  // Long press the omnibox steady view.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
      performAction:grey_longPress()];

  // Tap "Ask Gemini".
  id<GREYMatcher> askGeminiMatcher =
      chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
          IDS_IOS_APP_BAR_ASK_GEMINI);
  [[EarlGrey selectElementWithMatcher:askGeminiMatcher]
      performAction:grey_tap()];
}

}  // namespace

// Test suite for the Assistant container UI component.
@interface AssistantTestCase : ChromeTestCase
@end

@implementation AssistantTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];

  // When `kAssistantContainerParamDebug` is set, the assistant container should
  // always be shown. If this test suite fails in the future, avoid adding new
  // flags or workarounds to make it pass; instead, ensure the debug behavior is
  // correctly preserved.
  config.features_enabled_and_params.push_back(
      {kAssistantContainer,
       {{kAssistantContainerParam, kAssistantContainerParamDebug}}});
  return config;
}

- (void)setUp {
  [super setUp];
  self.testServer->RegisterRequestHandler(base::BindRepeating(&SimpleResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

// Tests that long pressing the omnibox and tapping "Ask Gemini" opens the
// assistant container.
- (void)testShowAssistantOnOmniboxLongPress {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];

  OpenAssistantFromOmnibox();

  // Verify assistant container is shown.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          grey_accessibilityID(kAssistantContainerAccessibilityIdentifier)];
}

@end

// Test suite for the Assistant side panel.
// Inherits all tests from AssistantTestCase and runs them with the side panel
// flag enabled.
@interface AssistantSidePanelTestCase : AssistantTestCase
@end

@implementation AssistantSidePanelTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_enabled.push_back(kAssistantSidePanel);
  config.features_enabled.push_back(kChromeNextIa);
  config.features_enabled.push_back(kFullscreenRefactoring);
  if (![self isRunningTest:@selector(testOmniboxVisibleInSidePanel)]) {
    config.features_enabled.push_back(kComposeboxIpad);
  }

  return config;
}

// Tests that the Assistant side panel and the app content do not overlap.
- (void)testSidePanelAndAppContentSideBySide {
  if (![ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Test only supported on iPad.");
  }

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];

  OpenAssistantFromOmnibox();

  // Verify assistant container is shown.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          grey_accessibilityID(kAssistantContainerAccessibilityIdentifier)];

  // Verify that the app content is to the right of the Assistant Container.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kAppContentAccessibilityIdentifier)]
      assertWithMatcher:grey_layout(
                            @[ RightOf() ],
                            grey_accessibilityID(
                                kAssistantContainerAccessibilityIdentifier))];
}

// Tests that the omnibox remains visible when the assistant side panel is
// shown.
- (void)testOmniboxVisibleInSidePanel {
  if (![ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Test only supported on iPad.");
  }

  // TODO(crbug.com/521688883): Disabled on iOS 18 and below iPad.
  if (!@available(iOS 26, *)) {
    EARL_GREY_TEST_DISABLED(@"Disabled on iOS 18- iPad.");
  }

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];

  OpenAssistantFromOmnibox();

  // Verify assistant container is shown.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          grey_accessibilityID(kAssistantContainerAccessibilityIdentifier)];

  // Verify that the omnibox is still visible.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end
