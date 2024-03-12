// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/containers/contains.h"
#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/search_engines/search_engines_switches.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/settings/settings_app_interface.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/disabled_test_macros.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"

namespace {

const char kPageURL[] = "/";
const char kOpenSearch[] = "/opensearch.xml";
const char kSearchURL[] = "/search?q=";
const char kCustomSearchEngineName[] = "Custom Search Engine";
const char kGoogleURL[] = "google";
const char kYahooURL[] = "yahoo";

std::string GetSearchExample() {
  return std::string(kSearchURL) + "example";
}

// Responses for different search engine. The name of the search engine is
// displayed on the page.
std::unique_ptr<net::test_server::HttpResponse> SearchResponse(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  if (base::Contains(request.GetURL().path(), kGoogleURL)) {
    http_response->set_content("<body>" + std::string(kGoogleURL) + "</body>");
  } else if (base::Contains(request.GetURL().path(), kYahooURL)) {
    http_response->set_content("<body>" + std::string(kYahooURL) + "</body>");
  }
  return std::move(http_response);
}

// Responses for the test http server. `server_url` is the URL of the server,
// used for absolute URL in the response. `open_search_queried` is set to true
// when the OpenSearchDescription is queried.
std::unique_ptr<net::test_server::HttpResponse> StandardResponse(
    std::string* server_url,
    bool* open_search_queried,
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);

  if (request.relative_url == kPageURL) {
    http_response->set_content("<head><link rel=\"search\" "
                               "type=\"application/opensearchdescription+xml\" "
                               "title=\"Custom Search Engine\" href=\"" +
                               std::string(kOpenSearch) +
                               "\"></head><body>Test Search</body>");
  } else if (request.relative_url == kOpenSearch) {
    *open_search_queried = true;
    http_response->set_content(
        "<OpenSearchDescription xmlns=\"http://a9.com/-/spec/opensearch/1.1/\">"
        "<ShortName>" +
        std::string(kCustomSearchEngineName) +
        "</ShortName>"
        "<Description>Description</Description>"
        "<Url type=\"text/html\" method=\"get\" template=\"" +
        *server_url + kSearchURL +
        "{searchTerms}\"/>"
        "</OpenSearchDescription>");
  } else if (request.relative_url == GetSearchExample()) {
    http_response->set_content("<head><body>Search Result</body>");

  } else {
    return nullptr;
  }
  return std::move(http_response);
}

}  // namespace

@interface SearchEngineSettingsChoiceScreenTestCase : ChromeTestCase {
  std::string _serverURL;
  bool _openSearchCalled;
}

@end

@implementation SearchEngineSettingsChoiceScreenTestCase

- (void)setUp {
  [super setUp];
  [SettingsAppInterface resetSearchEngine];
}

- (void)tearDown {
  [SettingsAppInterface resetSearchEngine];
  [super tearDown];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.additional_args.push_back(
      std::string("--") + switches::kSearchEngineChoiceCountry + "=FR");
  config.features_enabled.push_back(switches::kSearchEngineChoiceTrigger);
  return config;
}

// Tests that when changing the default search engine, the URL used for the
// search is updated.
- (void)testChangeSearchEngine {
  self.testServer->RegisterRequestHandler(base::BindRepeating(&SearchResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  GURL url = self.testServer->GetURL(kPageURL);
  NSString* port = base::SysUTF8ToNSString(url.port());

  NSArray<NSString*>* hosts = @[
    base::SysUTF8ToNSString(kGoogleURL), base::SysUTF8ToNSString(kYahooURL)
  ];

  [SettingsAppInterface addURLRewriterForHosts:hosts onPort:port];

  // Search on Google.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_replaceText(@"test")];
  // TODO(crbug.com/1454516): Use simulatePhysicalKeyboardEvent until
  // replaceText can properly handle \n.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"\n" flags:0];

  [ChromeEarlGrey waitForWebStateContainingText:kGoogleURL];

  // Change default search engine to Yahoo.
  [ChromeEarlGreyUI openSettingsMenu];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsSearchEngineButton()]
      performAction:grey_tap()];

  NSString* yahooSearchEngineName =
      [SettingsAppInterface frYahooSearchEngineName];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(yahooSearchEngineName)]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];

  [SettingsAppInterface addURLRewriterForHosts:hosts onPort:port];

  // Search on Yahoo.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_replaceText(@"test")];
  // TODO(crbug.com/1454516): Use simulatePhysicalKeyboardEvent until
  // replaceText can properly handle \n.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"\n" flags:0];

  [ChromeEarlGrey waitForWebStateContainingText:kYahooURL];
}

// Deletes a custom search engine by swiping and tapping on the "Delete" button.
- (void)testDeleteCustomSearchEngineSwipeAndTap {
  [self enterSettingsWithCustomSearchEngine];

  [[SearchEngineChoiceEarlGreyUI
      interactionForSettingsCustomSearchEngineWithName:kCustomSearchEngineName]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Swipe all the way to the left, to delete the custom search engine.
  id<GREYMatcher> customSearchEngineCell = [SearchEngineChoiceEarlGreyUI
      settingsCustomSearchEngineAccessibilityLabelWithName:
          kCustomSearchEngineName];
  [[EarlGrey selectElementWithMatcher:customSearchEngineCell]
      performAction:grey_swipeSlowInDirectionWithStartPoint(kGREYDirectionLeft,
                                                            0.9, 0.5)];

  [[EarlGrey selectElementWithMatcher:customSearchEngineCell]
      assertWithMatcher:grey_nil()];
}

// Deletes a custom engine by swiping it.
- (void)testDeleteCustomSearchEngineSwipe {
  [self enterSettingsWithCustomSearchEngine];
  [[SearchEngineChoiceEarlGreyUI
      interactionForSettingsCustomSearchEngineWithName:kCustomSearchEngineName]
      performAction:grey_swipeSlowInDirectionWithStartPoint(kGREYDirectionLeft,
                                                            0.9, 0.5)];
  id<GREYMatcher> customSearchEngineCell = [SearchEngineChoiceEarlGreyUI
      settingsCustomSearchEngineAccessibilityLabelWithName:
          kCustomSearchEngineName];
  [[EarlGrey selectElementWithMatcher:customSearchEngineCell]
      assertWithMatcher:grey_nil()];
}

// Tests that the selected custom search engine cannot be deleted.
- (void)testRefuseToDeleteSelectedCustomSearchEngineBySwipe {
  [self enterSettingsWithCustomSearchEngine];
  [[SearchEngineChoiceEarlGreyUI
      interactionForSettingsCustomSearchEngineWithName:kCustomSearchEngineName]
      performAction:grey_tap()];
  [[SearchEngineChoiceEarlGreyUI
      interactionForSettingsCustomSearchEngineWithName:kCustomSearchEngineName]
      performAction:grey_swipeSlowInDirectionWithStartPoint(kGREYDirectionLeft,
                                                            0.9, 0.5)];
  id<GREYMatcher> customSearchEngineCell = [SearchEngineChoiceEarlGreyUI
      settingsCustomSearchEngineAccessibilityLabelWithName:
          kCustomSearchEngineName];
  [[EarlGrey selectElementWithMatcher:customSearchEngineCell]
      assertWithMatcher:grey_notNil()];
}

// Deletes a custom search engine by entering edit mode.
- (void)testDeleteCustomSearchEngineEdit {
  [self enterSettingsWithCustomSearchEngine];

  id<GREYMatcher> editButton = grey_allOf(
      chrome_test_util::ButtonWithAccessibilityLabelId(
          IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON),
      grey_not(chrome_test_util::TabGridEditButton()),
      grey_not(grey_accessibilityTrait(UIAccessibilityTraitNotEnabled)), nil);
  [[EarlGrey selectElementWithMatcher:editButton] performAction:grey_tap()];

  id<GREYMatcher> customSearchEngineCell = [SearchEngineChoiceEarlGreyUI
      settingsCustomSearchEngineAccessibilityLabelWithName:
          kCustomSearchEngineName];
  [[SearchEngineChoiceEarlGreyUI
      interactionForSettingsCustomSearchEngineWithName:kCustomSearchEngineName]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:customSearchEngineCell]
      performAction:grey_tap()];

  id<GREYMatcher> deleteButton = grey_allOf(
      grey_accessibilityLabel(@"Delete"),
      grey_accessibilityTrait(UIAccessibilityTraitButton),
      grey_not(grey_accessibilityTrait(UIAccessibilityTraitNotEnabled)), nil);
  [[EarlGrey selectElementWithMatcher:deleteButton] performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:customSearchEngineCell]
      assertWithMatcher:grey_nil()];
}

#pragma mark - helpers

// Adds a custom search engine by navigating to a fake search engine page, then
// enters the search engine screen in Settings.
- (void)enterSettingsWithCustomSearchEngine {
  _openSearchCalled = false;
  self.testServer->RegisterRequestHandler(base::BindRepeating(
      &StandardResponse, &(_serverURL), &(_openSearchCalled)));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL pageURL = self.testServer->GetURL(kPageURL);
  _serverURL = pageURL.spec();
  // Remove trailing "/".
  _serverURL.pop_back();

  [ChromeEarlGrey loadURL:pageURL];

  __weak __typeof(self) weakSelf = self;
  GREYCondition* openSearchQuery =
      [GREYCondition conditionWithName:@"Wait for Open Search query"
                                 block:^BOOL {
                                   return [weakSelf wasOpenSearchCalled];
                                 }];
  // Wait for the
  GREYAssertTrue(
      [openSearchQuery waitWithTimeout:base::test::ios::kWaitForPageLoadTimeout
                                           .InSecondsF()],
      @"The open search XML hasn't been queried.");

  [ChromeEarlGrey loadURL:self.testServer->GetURL(GetSearchExample())];

  [ChromeEarlGreyUI openSettingsMenu];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsSearchEngineButton()]
      performAction:grey_tap()];
}

- (BOOL)wasOpenSearchCalled {
  return _openSearchCalled;
}

@end
