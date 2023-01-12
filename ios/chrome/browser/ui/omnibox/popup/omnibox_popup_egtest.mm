// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/functional/bind.h"
#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_ui_features.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_accessibility_identifier_constants.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

/// Returns the popup row containing the `url` as suggestion.
id<GREYMatcher> PopupRowWithUrl(GURL url) {
  NSString* urlString = base::SysUTF8ToNSString(url.GetContent());
  id<GREYMatcher> URLMatcher = grey_allOf(
      grey_descendant(
          chrome_test_util::StaticTextWithAccessibilityLabel(urlString)),
      grey_sufficientlyVisible(), nil);
  return grey_allOf(chrome_test_util::OmniboxPopupRow(), URLMatcher, nil);
}

/// Returns the switch to open tab element for the `url`.
id<GREYMatcher> SwitchTabElementForUrl(const GURL& url) {
  return grey_allOf(
      grey_ancestor(PopupRowWithUrl(url)),
      grey_accessibilityID(kOmniboxPopupRowSwitchTabAccessibilityIdentifier),
      grey_interactable(), nil);
}

void TapSwitchToTabButton(const GURL& url) {
  [[EarlGrey selectElementWithMatcher:grey_allOf(SwitchTabElementForUrl(url),
                                                 grey_interactable(), nil)]
      performAction:grey_tap()];
}

void ScrollToSwitchToTabElement(const GURL& url) {
  [[[EarlGrey selectElementWithMatcher:grey_allOf(SwitchTabElementForUrl(url),
                                                  grey_interactable(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:chrome_test_util::OmniboxPopupList()]
      assertWithMatcher:grey_interactable()];
}

// Web page 1.
const char kPage1[] = "This is the first page";
const char kPage1Title[] = "Title 1";
const char kPage1URL[] = "/page1.html";

// Web page 2.
const char kPage2[] = "This is the second page";
const char kPage2Title[] = "Title 2";
const char kPage2URL[] = "/page2.html";

// Web page 2.
const char kPage3[] = "This is the third page";
const char kPage3Title[] = "Title 3";
const char kPage3URL[] = "/page3.html";

/// Provides responses for the different pages.
std::unique_ptr<net::test_server::HttpResponse> StandardResponse(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);

  if (request.relative_url == kPage1URL) {
    http_response->set_content(
        "<html><head><title>" + std::string(kPage1Title) +
        "</title></head><body>" + std::string(kPage1) + "</body></html>");
    return std::move(http_response);
  }

  if (request.relative_url == kPage2URL) {
    http_response->set_content(
        "<html><head><title>" + std::string(kPage2Title) +
        "</title></head><body>" + std::string(kPage2) + "</body></html>");
    return std::move(http_response);
  }

  if (request.relative_url == kPage3URL) {
    http_response->set_content(
        "<html><head><title>" + std::string(kPage3Title) +
        "</title></head><body>" + std::string(kPage3) + "</body></html>");
    return std::move(http_response);
  }

  return nil;
}

}  //  namespace

@interface OmniboxPopupTestCase : ChromeTestCase

@end

@implementation OmniboxPopupTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  auto bundledConfig = std::string("OmniboxBundledExperimentV1");
  config.additional_args.push_back("--enable-features=" + bundledConfig + "<" +
                                   bundledConfig);
  config.additional_args.push_back("--force-fieldtrials=" + bundledConfig +
                                   "/Test");

  // Disable AutocompleteProvider types: TYPE_SEARCH and TYPE_ON_DEVICE_HEAD.
  config.additional_args.push_back(
      "--force-fieldtrial-params=" + bundledConfig +
      ".Test:" + "DisableProviders" + "/" + "1056");

  return config;
}

- (void)setUp {
  [super setUp];

  // Start a server to be able to navigate to a web page.
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  [ChromeEarlGrey clearBrowsingHistory];
}

// Tests that tapping the switch to open tab button, switch to the open tab,
// doesn't close the tab.
- (void)testSwitchToOpenTab {
  // Open the first page.
  GURL firstPageURL = self.testServer->GetURL(kPage1URL);
  [ChromeEarlGrey loadURL:firstPageURL];
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];

  // Open the second page in another tab.
  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPage2URL)];
  [ChromeEarlGrey waitForWebStateContainingText:kPage2];

  // Type the URL of the first page in the omnibox to trigger it as suggestion.
  [ChromeEarlGreyUI focusOmniboxAndType:base::SysUTF8ToNSString(kPage1URL)];

  // Switch to the first tab, scrolling the popup if necessary.
  ScrollToSwitchToTabElement(firstPageURL);
  TapSwitchToTabButton(firstPageURL);

  [ChromeEarlGrey waitForWebStateContainingText:kPage1];

  // Check that both tabs are opened (and that we switched tab and not just
  // navigated.
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(chrome_test_util::StaticTextWithAccessibilityLabel(
                         base::SysUTF8ToNSString(kPage2Title)),
                     grey_ancestor(chrome_test_util::TabGridCellAtIndex(1)),
                     nil)] assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the switch to open tab button isn't displayed for the current tab.
- (void)testNotSwitchButtonOnCurrentTab {
  GURL URL2 = self.testServer->GetURL(kPage2URL);

  // Open the first page.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPage1URL)];
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];

  // Open the second page in another tab.
  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGrey loadURL:URL2];
  [ChromeEarlGrey waitForWebStateContainingText:kPage2];

  // Type the URL of the first page in the omnibox to trigger it as suggestion.
  [ChromeEarlGreyUI focusOmniboxAndType:base::SysUTF8ToNSString(kPage2URL)];

  // Check that we have the suggestion for the second page, but not the switch
  // as it is the current page.

  [[EarlGrey selectElementWithMatcher:PopupRowWithUrl(URL2)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:SwitchTabElementForUrl(URL2)]
      assertWithMatcher:grey_not(grey_interactable())];
}

// Tests that the incognito tabs aren't displayed as "opened" tab in the
// non-incognito suggestions and vice-versa.
- (void)testIncognitoSeparation {
  GURL URL1 = self.testServer->GetURL(kPage1URL);
  GURL URL2 = self.testServer->GetURL(kPage2URL);
  GURL URL3 = self.testServer->GetURL(kPage3URL);

  // Add all the pages to the history.
  [ChromeEarlGrey loadURL:URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];
  [ChromeEarlGrey loadURL:URL2];
  [ChromeEarlGrey waitForWebStateContainingText:kPage2];
  [ChromeEarlGrey loadURL:URL3];
  [ChromeEarlGrey waitForWebStateContainingText:kPage3];
  [[self class] closeAllTabs];

  // Load page 1 in non-incognito and page 2 in incognito.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];

  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:URL2];
  [ChromeEarlGrey waitForWebStateContainingText:kPage2];

  // Open page 3 in non-incognito.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:URL3];
  [ChromeEarlGrey waitForWebStateContainingText:kPage3];

  NSString* omniboxInput =
      [NSString stringWithFormat:@"%@:%@", base::SysUTF8ToNSString(URL3.host()),
                                 base::SysUTF8ToNSString(URL3.port())];
  [ChromeEarlGreyUI focusOmniboxAndType:omniboxInput];

  // Check that we have the switch button for the first page.
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(grey_ancestor(PopupRowWithUrl(URL1)),
                     grey_accessibilityID(
                         kOmniboxPopupRowSwitchTabAccessibilityIdentifier),
                     nil)] assertWithMatcher:grey_sufficientlyVisible()];

  // Check that we have the suggestion for the second page, but not the switch.
  [[EarlGrey selectElementWithMatcher:PopupRowWithUrl(URL2)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:SwitchTabElementForUrl(URL2)]
      assertWithMatcher:grey_nil()];

  // Open page 3 in incognito.
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:URL3];
  [ChromeEarlGrey waitForWebStateContainingText:kPage3];

  [ChromeEarlGreyUI focusOmniboxAndType:base::SysUTF8ToNSString(URL3.host())];

  // Check that we have the switch button for the second page.
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(grey_ancestor(PopupRowWithUrl(URL2)),
                     grey_accessibilityID(
                         kOmniboxPopupRowSwitchTabAccessibilityIdentifier),
                     nil)] assertWithMatcher:grey_sufficientlyVisible()];

  // Check that we have the suggestion for the first page, but not the switch.
  [[EarlGrey selectElementWithMatcher:PopupRowWithUrl(URL1)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:SwitchTabElementForUrl(URL1)]
      assertWithMatcher:grey_nil()];
}

- (void)testCloseNTPWhenSwitching {
  // Open the first page.
  GURL URL1 = self.testServer->GetURL(kPage1URL);
  [ChromeEarlGrey loadURL:URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];

  // Open a new tab and switch to the first tab.
  [ChromeEarlGrey openNewTab];
  NSString* omniboxInput =
      [NSString stringWithFormat:@"%@:%@", base::SysUTF8ToNSString(URL1.host()),
                                 base::SysUTF8ToNSString(URL1.port())];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(omniboxInput)];

  TapSwitchToTabButton(URL1);
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];

  // Check that the other tab is closed.
  [ChromeEarlGrey waitForMainTabCount:1];
}

- (void)testDontCloseNTPWhenSwitchingWithForwardHistory {
  // Open the first page.
  GURL URL1 = self.testServer->GetURL(kPage1URL);
  [ChromeEarlGrey loadURL:URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];

  // Open a new tab, navigate to a page and go back to have forward history.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];
  [ChromeEarlGrey goBack];

  // Navigate to the other tab.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(base::SysUTF8ToNSString(URL1.host()))];

  // Omnibox can reorder itself in multiple animations, so add an extra wait
  // here.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:SwitchTabElementForUrl(
                                                       URL1)];
  [[EarlGrey selectElementWithMatcher:SwitchTabElementForUrl(URL1)]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];

  // Check that the other tab is not closed.
  [ChromeEarlGrey waitForMainTabCount:2];
}

// Tests that switching to closed tab opens the tab in foreground, except if it
// is from NTP without history.
- (void)testSwitchToClosedTab {
  GURL URL1 = self.testServer->GetURL(kPage1URL);

  // Open the first page.
  [ChromeEarlGrey loadURL:URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];

  // Open a new tab and load another URL.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPage2URL)];
  [ChromeEarlGrey waitForWebStateContainingText:kPage2];

  // Start typing url of the first page.
  [ChromeEarlGreyUI focusOmniboxAndType:base::SysUTF8ToNSString(kPage1URL)];

  // Make sure that the "Switch to Open Tab" element is visible, scrolling the
  // popup if necessary.
  ScrollToSwitchToTabElement(URL1);

  // Close the first page.
  [ChromeEarlGrey closeTabAtIndex:0];
  [ChromeEarlGrey waitForMainTabCount:1];

  // Try to switch to the first tab.
  TapSwitchToTabButton(URL1);
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Check that the URL has been opened in a new foreground tab.
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];
  [ChromeEarlGrey waitForMainTabCount:2];
}

// Tests that having multiple suggestions with corresponding opened tabs display
// multiple buttons.
- (void)testMultiplePageOpened {
  // Open the first page.
  GURL URL1 = self.testServer->GetURL(kPage1URL);
  [ChromeEarlGrey loadURL:URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];

  // Open the second page in a new tab.
  [ChromeEarlGrey openNewTab];
  GURL URL2 = self.testServer->GetURL(kPage2URL);
  [ChromeEarlGrey loadURL:URL2];
  [ChromeEarlGrey waitForWebStateContainingText:kPage2];

  // Start typing url of the two opened pages in a new tab.
  [ChromeEarlGrey openNewTab];
  NSString* omniboxInput =
      [NSString stringWithFormat:@"%@:%@", base::SysUTF8ToNSString(URL1.host()),
                                 base::SysUTF8ToNSString(URL1.port())];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(omniboxInput)];

  // Check that both elements are displayed.
  // Omnibox can reorder itself in multiple animations, so add an extra wait
  // here.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:SwitchTabElementForUrl(
                                                       URL1)];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:SwitchTabElementForUrl(
                                                       URL2)];
}

// Test that on iPhones, when the popup is scrolled, the keyboard is dismissed
// but the omnibox is still expanded and the suggestions are visible.
// Test with flag kEnableSuggestionsScrollingOnIPad disabled.
- (void)testScrollingDismissesKeyboardOnPhones {
  [[AppLaunchManager sharedManager]
      ensureAppLaunchedWithFeaturesEnabled:{}
                                  disabled:{kEnableSuggestionsScrollingOnIPad}
                            relaunchPolicy:ForceRelaunchByCleanShutdown];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(@"hello")];

  // Matcher for a URL-what-you-typed suggestion.
  id<GREYMatcher> textMatcher = grey_descendant(
      chrome_test_util::StaticTextWithAccessibilityLabel(@"hello"));
  id<GREYMatcher> row =
      grey_allOf(chrome_test_util::OmniboxPopupRow(), textMatcher, nil);

  // Omnibox can reorder itself in multiple animations, so add an extra wait
  // here.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:row];
  GREYAssertTrue([EarlGrey isKeyboardShownWithError:nil],
                 @"Keyboard Should be Shown");

  // Scroll the popup. This swipes from the point located at 50% of the width of
  // the frame horizontally and most importantly 10% of the height of the frame
  // vertically. This is necessary if the center of the list's accessibility
  // frame is not visible, as it is the default start point.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxPopupList()]
      performAction:grey_swipeFastInDirectionWithStartPoint(kGREYDirectionUp,
                                                            0.5, 0.1)];
  [[EarlGrey selectElementWithMatcher:row]
      assertWithMatcher:grey_interactable()];

  // The keyboard should only be dismissed on phones. Ipads, even in
  // multitasking, are considered tall enough to fit all suggestions.
  if ([ChromeEarlGrey isIPadIdiom]) {
    GREYAssertTrue([EarlGrey isKeyboardShownWithError:nil],
                   @"Keyboard Should be Shown");
  } else {
    GREYAssertFalse([EarlGrey isKeyboardShownWithError:nil],
                    @"Keyboard Should not be Shown");
  }
}

// Test when the popup is scrolled, the keyboard is dismissed
// but the omnibox is still expanded and the suggestions are visible.
// Test with flag kEnableSuggestionsScrollingOnIPad enabled.
- (void)testScrollingDismissesKeyboard {
  [[AppLaunchManager sharedManager]
      ensureAppLaunchedWithFeaturesEnabled:{kEnableSuggestionsScrollingOnIPad}
                                  disabled:{}
                            relaunchPolicy:ForceRelaunchByCleanShutdown];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(@"hello")];

  // Matcher for a URL-what-you-typed suggestion.
  id<GREYMatcher> textMatcher = grey_descendant(
      chrome_test_util::StaticTextWithAccessibilityLabel(@"hello"));
  id<GREYMatcher> row =
      grey_allOf(chrome_test_util::OmniboxPopupRow(), textMatcher,
                 grey_sufficientlyVisible(), nil);

  // Omnibox can reorder itself in multiple animations, so add an extra wait
  // here.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:row];
  GREYAssertTrue([EarlGrey isKeyboardShownWithError:nil],
                 @"Keyboard Should be Shown");

  // Scroll the popup. This swipes from the point located at 50% of the width of
  // the frame horizontally and most importantly 10% of the height of the frame
  // vertically. This is necessary if the center of the list's accessibility
  // frame is not visible, as it is the default start point.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxPopupList()]
      performAction:grey_swipeFastInDirectionWithStartPoint(kGREYDirectionUp,
                                                            0.5, 0.1)];

  [[EarlGrey selectElementWithMatcher:row]
      assertWithMatcher:grey_sufficientlyVisible()];

  // The keyboard should be dismissed.
  GREYAssertFalse([EarlGrey isKeyboardShownWithError:nil],
                  @"Keyboard Should not be Shown");
}

@end
