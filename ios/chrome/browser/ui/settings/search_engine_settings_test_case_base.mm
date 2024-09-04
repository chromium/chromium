// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/search_engine_settings_test_case_base.h"

#import "base/containers/contains.h"
#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/search_engines/prepopulated_engines.h"
#import "components/search_engines/search_engines_switches.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/settings/search_engine_settings_test_case_base.h"
#import "ios/chrome/browser/ui/settings/settings_app_interface.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_constants.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/testing/earl_grey/disabled_test_macros.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"

NSString* const kCustomSearchEngineName = @"Custom Search Engine";

namespace {

const char kPageURL[] = "/";
const char kOpenSearch[] = "/opensearch.xml";
const char kSearchURL[] = "/search?q=";

std::string GetSearchExample() {
  return std::string(kSearchURL) + "example";
}

// Responses for different search engine. The name of the search engine is
// displayed on the page.
std::unique_ptr<net::test_server::HttpResponse> SearchResponse(
    const TemplateURLPrepopulateData::PrepopulatedEngine*
        secondPrepopulatedSearchEngine,
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  const std::string googleSearchEngineKeyword(
      base::UTF16ToUTF8(TemplateURLPrepopulateData::google.keyword));
  const std::string secondSearchEngineKeyword(
      base::UTF16ToUTF8(secondPrepopulatedSearchEngine->keyword));
  if (base::Contains(request.GetURL().path(), googleSearchEngineKeyword)) {
    http_response->set_content(
        "<body>" + std::string(googleSearchEngineKeyword) + "</body>");
  } else if (base::Contains(request.GetURL().path(),
                            secondSearchEngineKeyword)) {
    http_response->set_content(
        "<body>" + std::string(secondSearchEngineKeyword) + "</body>");
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
        base::SysNSStringToUTF8(kCustomSearchEngineName) +
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

@implementation SearchEngineSettingsTestCaseBase {
  std::string _serverURL;
  bool _openSearchCalled;
}

+ (const char*)countryForTestCase {
  return "";
}

+ (const TemplateURLPrepopulateData::PrepopulatedEngine*)
    secondPrepopulatedSearchEngine {
  return nil;
}

+ (id<GREYMatcher>)editButtonMatcherWithEnabled:(BOOL)enabled {
  id<GREYMatcher> enabledMatcher =
      enabled
          ? grey_not(grey_accessibilityTrait(UIAccessibilityTraitNotEnabled))
          : grey_accessibilityTrait(UIAccessibilityTraitNotEnabled);
  return grey_allOf(chrome_test_util::ButtonWithAccessibilityLabelId(
                        IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON),
                    grey_accessibilityID(kSettingsToolbarEditButtonId),
                    enabledMatcher, nil);
}

- (void)startHTTPServer {
  const TemplateURLPrepopulateData::PrepopulatedEngine*
      secondPrepopulatedSearchEngine =
          [self.class secondPrepopulatedSearchEngine];
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&SearchResponse, secondPrepopulatedSearchEngine));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

- (void)addURLRewriter {
  GURL url = self.testServer->GetURL(kPageURL);
  NSString* port = base::SysUTF8ToNSString(url.port());

  const std::string googleSearchEngineKeyword(
      base::UTF16ToUTF8(TemplateURLPrepopulateData::google.keyword));
  const TemplateURLPrepopulateData::PrepopulatedEngine*
      secondPrepopulatedSearchEngine =
          [self.class secondPrepopulatedSearchEngine];
  const std::string secondSearchEngineKeyword(
      base::UTF16ToUTF8(secondPrepopulatedSearchEngine->keyword));
  NSArray<NSString*>* hosts = @[
    base::SysUTF8ToNSString(googleSearchEngineKeyword),
    base::SysUTF8ToNSString(secondSearchEngineKeyword)
  ];

  [SettingsAppInterface addURLRewriterForHosts:hosts onPort:port];
}

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

  [SearchEngineChoiceEarlGreyUI openSearchEngineSettings];
}

#pragma mark - ChromeTestCase

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
  std::string country = [self.class countryForTestCase];
  config.additional_args.push_back(
      std::string("--") + switches::kSearchEngineChoiceCountry + "=" + country);
  return config;
}

#pragma mark - Private

- (BOOL)wasOpenSearchCalled {
  return _openSearchCalled;
}

@end
