// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <string>

#import "base/functional/bind.h"
#import "base/strings/stringprintf.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/web/model/lookalike_url_app_interface.h"
#import "ios/chrome/browser/web/model/lookalike_url_constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/embedded_test_server_handlers.h"
#import "ios/web/common/features.h"
#import "ios/web/public/test/element_selector.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "net/test/embedded_test_server/request_handler_util.h"
#import "ui/base/l10n/l10n_util.h"

using chrome_test_util::BackButton;
using chrome_test_util::ForwardButton;
using chrome_test_util::Omnibox;
using chrome_test_util::OmniboxText;

namespace {
// Relative paths used for a page that opens a lookalike in a new tab.
const char kLookalikeInNewTab[] = "/lookalike-newtab.html";

// Text that is found on the lookalike page.
const char kLookalikeContent[] = "Lookalike - Safety warning bypassed";
// Text that is found on a page that opens a lookalike in a new tab.
const char kLookalikeInNewTabContent[] = "New tab";
}  // namespace

// Tests lookalike URL blocking.
@interface LookalikeUrlTestCase : ChromeTestCase {
  // A URL that is treated as a lookalike.
  GURL _lookalikeURL;
  // A URL that is treated as a safe page.
  GURL _safeURL;
  // Text that is found on the safe page.
  std::string _safeContent;
  // Text that is found on the lookalike interstitial.
  std::string _lookalikeBlockingPageContent;
  // Text that is found on the lookalike interstitial with no suggestion.
  std::string _lookalikeBlockingPageNoSuggestionContent;
}
@end

@implementation LookalikeUrlTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.relaunch_policy = NoForceRelaunchAndResetState;
  return config;
}

- (void)setUp {
  [super setUp];
  [LookalikeUrlAppInterface setUpLookalikeUrlDeciderForWebState];
  std::string lookalikeHTML =
      base::StringPrintf("<html><body>%s</body></html>", kLookalikeContent);
  self.testServer->RegisterRequestHandler(base::BindRepeating(
      &net::test_server::HandlePrefixedRequest, kLookalikePagePathForTesting,
      base::BindRepeating(&testing::HandlePageWithHtml, lookalikeHTML)));
  self.testServer->RegisterRequestHandler(base::BindRepeating(
      &net::test_server::HandlePrefixedRequest,
      kLookalikePageEmptyUrlPathForTesting,
      base::BindRepeating(&testing::HandlePageWithHtml, lookalikeHTML)));
  std::string lookalikeNewTabHTML = base::StringPrintf(
      "<html><body><a target=\"_blank\" href=\"%s\" "
      "id=\"lookalike-newtab\">%s</a></body></html>",
      kLookalikePageEmptyUrlPathForTesting, kLookalikeInNewTabContent);
  self.testServer->RegisterRequestHandler(base::BindRepeating(
      &net::test_server::HandlePrefixedRequest, kLookalikeInNewTab,
      base::BindRepeating(&testing::HandlePageWithHtml, lookalikeNewTabHTML)));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  _safeURL = self.testServer->GetURL("/echo");
  _safeContent = "Echo";
  _lookalikeURL = self.testServer->GetURL(kLookalikePagePathForTesting);
  _lookalikeBlockingPageContent =
      l10n_util::GetStringUTF8(IDS_LOOKALIKE_URL_PRIMARY_PARAGRAPH);
  _lookalikeBlockingPageNoSuggestionContent = l10n_util::GetStringUTF8(
      IDS_LOOKALIKE_URL_PRIMARY_PARAGRAPH_NO_SUGGESTED_URL);

  if (@available(iOS 15.1, *)) {
  } else {
    // Workaround https://bugs.webkit.org/show_bug.cgi?id=226323, which breaks
    // some back/forward navigations between pages that share a renderer
    // process. Use 'localhost' instead of '127.0.0.1' for the safe URL to
    // prevent sharing renderer processes with unsafe URLs.
    GURL::Replacements replacements;
    replacements.SetHostStr("localhost");
    _safeURL = _safeURL.ReplaceComponents(replacements);
  }
}

- (void)tearDown {
  [LookalikeUrlAppInterface tearDownLookalikeUrlDeciderForWebState];
  [super tearDown];
}

// Tests that non-lookalike URLs are not blocked.
- (void)testSafePage {
  [ChromeEarlGrey loadURL:_safeURL];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent];
}

// Tests that a lookalike URL navigation is blocked, and the Go to suggested
// site button works. Also tests that navigating back to the site shows the
// interstitial and that navigating forward again works.
- (void)testLookalikeUrlPage {
  // Load the lookalike page and verify a warning is shown.
  [ChromeEarlGrey loadURL:_lookalikeURL];
  [ChromeEarlGrey waitForWebStateContainingText:_lookalikeBlockingPageContent];
  // Lookalike URL blocking pages should not display URL.
  [[EarlGrey selectElementWithMatcher:OmniboxText(_lookalikeURL.GetContent())]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:Omnibox()]
      assertWithMatcher:OmniboxText("")];

  // Tap on the "Go to" button and verify that the suggested page
  // contents are loaded.
  [ChromeEarlGrey tapWebStateElementWithID:@"primary-button"];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent];

  // Verify that the warning is shown when navigating back and that safe
  // content is shown when navigating forward again.
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:_lookalikeBlockingPageContent];
  [[EarlGrey selectElementWithMatcher:ForwardButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent];
  [[EarlGrey selectElementWithMatcher:OmniboxText(_safeURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that a lookalike URL navigation is blocked, and the text link for
// suggested site works. Also tests that navigating back to the site shows
// the interstitial and that navigating forward again works.
- (void)testLookalikeUrlPageSiteLink {
  // Load the lookalike page and verify a warning is shown.
  [ChromeEarlGrey loadURL:_lookalikeURL];
  [ChromeEarlGrey waitForWebStateContainingText:_lookalikeBlockingPageContent];
  // Lookalike URL blocking pages should not display URL.
  [[EarlGrey selectElementWithMatcher:OmniboxText(_lookalikeURL.GetContent())]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:Omnibox()]
      assertWithMatcher:OmniboxText("")];

  // Tap on the site suggestion link and verify that the suggested page
  // contents are loaded.
  [ChromeEarlGrey tapWebStateElementWithID:@"dont-proceed-link"];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent];
  [[EarlGrey selectElementWithMatcher:OmniboxText(_safeURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Verify that the warning is shown when navigating back and that safe
  // content is shown when navigating forward again.
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:_lookalikeBlockingPageContent];
  [[EarlGrey selectElementWithMatcher:ForwardButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent];
}

// Tests that Back to safety works when there is no suggested URL. Also tests
// that navigating forward to the site shows the interstitial and that
// navigating back again works.
- (void)testLookalikeUrlPageNoSuggestion {
  // Navigate to safe page first to enable later verification of
  // back/forward navigation.
  [ChromeEarlGrey loadURL:_safeURL];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent];

  // Navigate to a lookalike page with no suggestion and verify that a warning
  // and the correct button is shown.
  [ChromeEarlGrey
      loadURL:self.testServer->GetURL(kLookalikePageEmptyUrlPathForTesting)];
  [ChromeEarlGrey
      waitForWebStateContainingText:_lookalikeBlockingPageNoSuggestionContent];
  [ChromeEarlGrey
      waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                        IDS_LOOKALIKE_URL_BACK_TO_SAFETY)];
  // Lookalike URL blocking pages should not display URL.
  [[EarlGrey selectElementWithMatcher:OmniboxText(_lookalikeURL.GetContent())]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:Omnibox()]
      assertWithMatcher:OmniboxText("")];

  // Tap on the "Back to safety" button and verify that the safe content
  // is loaded.
  [ChromeEarlGrey tapWebStateElementWithID:@"primary-button"];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent];
  [[EarlGrey selectElementWithMatcher:OmniboxText(_safeURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Verify that the warning is shown when navigating forward and that safe
  // content is shown when navigating back again.
  [[EarlGrey selectElementWithMatcher:ForwardButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForWebStateContainingText:_lookalikeBlockingPageNoSuggestionContent];
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent];
}

// Tests that Close page works when there is no suggested URL and unable
// to go back.
- (void)testLookalikeUrlPageNoSuggestionClosePage {
  // First navigate to a page that will open the lookalike URL in a new tab,
  // then open the lookalike page.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kLookalikeInNewTab)];
  [ChromeEarlGrey tapWebStateElementWithID:@"lookalike-newtab"];

  // Verify that the new tab has loaded before setting up the policy decider
  // for the new web state. Then reload to make sure the interstitial is
  // displayed.
  [ChromeEarlGrey waitForWebStateContainingText:kLookalikeContent];
  [LookalikeUrlAppInterface setUpLookalikeUrlDeciderForWebState];
  [ChromeEarlGrey reload];

  // Verify that a warning and the correct button is shown.
  [ChromeEarlGrey
      waitForWebStateContainingText:_lookalikeBlockingPageNoSuggestionContent];
  [ChromeEarlGrey
      waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                        IDS_LOOKALIKE_URL_CLOSE_PAGE)];
  // Lookalike URL blocking pages should not display URL.
  [[EarlGrey selectElementWithMatcher:OmniboxText(_lookalikeURL.GetContent())]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:Omnibox()]
      assertWithMatcher:OmniboxText("")];

  // Tap on the "Close" button and verify that the page closes.
  [ChromeEarlGrey tapWebStateElementWithID:@"primary-button"];
  [ChromeEarlGrey waitForWebStateContainingText:kLookalikeInNewTabContent];
}

// Tests proceeding past the lookalike warning and that opening the page in
// a new tab will bypass the warning.
- (void)testProceedingPastLookalikeUrlWarning {
  // Load the lookalike page and verify a warning is shown.
  [ChromeEarlGrey loadURL:_lookalikeURL];
  [ChromeEarlGrey waitForWebStateContainingText:_lookalikeBlockingPageContent];
  // Lookalike URL blocking pages should not display URL.
  [[EarlGrey selectElementWithMatcher:OmniboxText(_lookalikeURL.GetContent())]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:Omnibox()]
      assertWithMatcher:OmniboxText("")];

  // Tap on the link to ignore the warning, and verify that the page is loaded.
  [ChromeEarlGrey tapWebStateElementWithID:@"proceed-button"];
  [ChromeEarlGrey waitForWebStateContainingText:kLookalikeContent];
  [[EarlGrey selectElementWithMatcher:OmniboxText(_lookalikeURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // In a new tab, the warning should not be shown.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:_lookalikeURL];
  [ChromeEarlGrey waitForWebStateContainingText:kLookalikeContent];
}

// Tests displaying a warning for an lookalike URL, proceeding past the warning,
// and navigating back/forward, in incognito.
- (void)testProceedingPastLookalikeWarningInIncognito {
  [ChromeEarlGrey openNewIncognitoTab];
  [LookalikeUrlAppInterface setUpLookalikeUrlDeciderForWebState];

  // Navigate to safe page first to enable later verification of
  // back/forward navigation.
  [ChromeEarlGrey loadURL:_safeURL];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent];

  // Load the lookalike page and verify a warning is shown.
  [ChromeEarlGrey loadURL:_lookalikeURL];
  [ChromeEarlGrey waitForWebStateContainingText:_lookalikeBlockingPageContent];
  // Lookalike URL blocking pages should not display URL.
  [[EarlGrey selectElementWithMatcher:OmniboxText(_lookalikeURL.GetContent())]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:Omnibox()]
      assertWithMatcher:OmniboxText("")];

  // Tap on the link to ignore the warning, and verify that the page is loaded.
  [ChromeEarlGrey tapWebStateElementWithID:@"proceed-button"];
  [ChromeEarlGrey waitForWebStateContainingText:kLookalikeContent];
  [[EarlGrey selectElementWithMatcher:OmniboxText(_lookalikeURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Verify that no warning is shown when navigating back and then forward to
  // the unsafe page.
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent];
  [[EarlGrey selectElementWithMatcher:ForwardButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kLookalikeContent];
}

// Tests that performing session restoration to a lookalike URL warning page
// preserves navigation history.
- (void)testRestoreToWarningPagePreservesHistory {
  // Build up navigation history that consists of a safe URL, a warning page,
  // and the suggested safe URL.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echoall")];
  std::string safeContent2 = "Request Body";
  [ChromeEarlGrey waitForWebStateContainingText:safeContent2];

  // Load the lookalike URL page and verify a warning is shown.
  [ChromeEarlGrey loadURL:_lookalikeURL];
  [ChromeEarlGrey waitForWebStateContainingText:_lookalikeBlockingPageContent];
  // Lookalike URL blocking pages should not display URL.
  [[EarlGrey selectElementWithMatcher:OmniboxText(_lookalikeURL.GetContent())]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:Omnibox()]
      assertWithMatcher:OmniboxText("")];

  // Tap on the "Go to" button and verify that the suggested page contents
  // are loaded.
  [ChromeEarlGrey tapWebStateElementWithID:@"primary-button"];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent];

  // Navigate back to the interstitial. Now both the back list and the forward
  // list are non-empty.
  [ChromeEarlGrey goBack];
  [ChromeEarlGrey waitForWebStateContainingText:_lookalikeBlockingPageContent];

  // Do a session restoration and verify that all navigation history is
  // preserved. For this test, the policy decider doesn't get installed for
  // the first page load, so goForward first and install the policy decider
  // after a load.
  [[EarlGrey selectElementWithMatcher:ForwardButton()]
      performAction:grey_tap()];
  [self triggerRestoreByRestartingApplication];
  [LookalikeUrlAppInterface setUpLookalikeUrlDeciderForWebState];
  [ChromeEarlGrey goBack];
  [ChromeEarlGrey waitForWebStateContainingText:_lookalikeBlockingPageContent];

  [ChromeEarlGrey goBack];
  [ChromeEarlGrey waitForWebStateContainingText:safeContent2];

  // The policy decider will trigger at this point, so the warning should
  // be shown.
  [ChromeEarlGrey goForward];
  [ChromeEarlGrey waitForWebStateContainingText:_lookalikeBlockingPageContent];

  [ChromeEarlGrey goForward];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent];
}

@end
