// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search_engines/search_engine_tab_helper.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/favicon/ios/web_favicon_driver.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/search_engines/template_url_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/favicon/favicon_service_factory.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#include "ios/chrome/browser/web/chrome_web_test.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForJSCompletionTimeout;
using web::test::SubmitWebViewFormWithId;

namespace {
const char kOpenSearchXmlFilePath[] =
    "/ios/testing/data/http_server_files/opensearch.xml";
const char kPonyHtmlFilePath[] =
    "/ios/testing/data/http_server_files/pony.html";

// A BrowserStateKeyedServiceFactory::TestingFactory that creates a testing
// TemplateURLService. The created TemplateURLService may contain some default
// TemplateURLs.
std::unique_ptr<KeyedService> CreateTestingTemplateURLService(
    web::BrowserState*) {
  return std::make_unique<TemplateURLService>(/*initializers=*/nullptr,
                                              /*count=*/0);
}
}

// Test fixture for SearchEngineTabHelper class.
class SearchEngineTabHelperTest : public ChromeWebTest {
 protected:
  SearchEngineTabHelperTest()
      : ChromeWebTest(web::WebTaskEnvironment::Options::IO_MAINLOOP) {}

  void SetUp() override {
    WebTestWithWebState::SetUp();
    favicon::WebFaviconDriver::CreateForWebState(
        web_state(), ios::FaviconServiceFactory::GetForBrowserState(
                         chrome_browser_state_->GetOriginalChromeBrowserState(),
                         ServiceAccessType::IMPLICIT_ACCESS));
    SearchEngineTabHelper::CreateForWebState(web_state());
    server_.ServeFilesFromSourceDirectory(".");
    ASSERT_TRUE(server_.Start());
    // Set the TestingFactory of BrowserState of ios::TemplateURLServiceFactory
    // to provide a testing TemplateURLService. Notice that we should not use
    // GetBrowserState() because it may be overriden to return incognito
    // BrowserState but TemplateURLService is always created on regular
    // BrowserState.
    ios::TemplateURLServiceFactory::GetInstance()->SetTestingFactory(
        chrome_browser_state_->GetOriginalChromeBrowserState(),
        base::BindRepeating(&CreateTestingTemplateURLService));
    template_url_service()->Load();
  }

  // Returns the testing TemplateURLService.
  TemplateURLService* template_url_service() {
    ios::ChromeBrowserState* browser_state =
        ios::ChromeBrowserState::FromBrowserState(GetBrowserState());
    return ios::TemplateURLServiceFactory::GetForBrowserState(browser_state);
  }

  // Sends a message that a OSDD <link> is found in page, with |page_url| and
  // |osdd_url| as message content.
  bool SendMessageOfOpenSearch(const GURL& page_url, const GURL& osdd_url) {
    id result = ExecuteJavaScript([NSString
        stringWithFormat:@"__gCrWeb.message.invokeOnHost({'command': "
                         @"'searchEngine.openSearch', 'pageUrl' : '%s', "
                         @"'osddUrl': '%s'}); true;",
                         page_url.spec().c_str(), osdd_url.spec().c_str()]);
    return [result isEqual:@YES];
  }

  // Sends a message that |searchable_url| is generated from <form> submission.
  bool SendMessageOfSearchableUrl(const GURL& searchable_url) {
    id result = ExecuteJavaScript([NSString
        stringWithFormat:@"__gCrWeb.message.invokeOnHost({'command': "
                         @"'searchEngine.searchableUrl', 'url' : '%s'}); true;",
                         searchable_url.spec().c_str()]);
    return [result isEqual:@YES];
  }

  net::EmbeddedTestServer server_;

  DISALLOW_COPY_AND_ASSIGN(SearchEngineTabHelperTest);
};

// Tests that SearchEngineTabHelper can add TemplateURL to TemplateURLService
// when a OSDD <link> is found in web page.
TEST_F(SearchEngineTabHelperTest, AddTemplateURLByOpenSearch) {
  GURL page_url("https://chromium.test");
  GURL osdd_url = server_.GetURL(kOpenSearchXmlFilePath);

  // Record the original TemplateURLs in TemplateURLService.
  std::vector<TemplateURL*> old_urls =
      template_url_service()->GetTemplateURLs();

  // Load an empty page, and send a message of openSearchUrl from Js.
  LoadHtml(@"<html></html>", page_url);
  ASSERT_TRUE(SendMessageOfOpenSearch(page_url, osdd_url));

  // Wait for TemplateURL added to TemplateURLService.
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return template_url_service()->GetTemplateURLs().size() ==
           old_urls.size() + 1;
  }));

  // TemplateURLService doesn't guarantee that newly added TemplateURL is
  // appended, so we should check in a general way that only one TemplateURL is
  // added and others remain untouched.
  TemplateURL* new_url = nullptr;
  for (TemplateURL* url : template_url_service()->GetTemplateURLs()) {
    if (std::find(old_urls.begin(), old_urls.end(), url) == old_urls.end()) {
      ASSERT_FALSE(new_url);
      new_url = url;
    }
  }
  ASSERT_TRUE(new_url);
  EXPECT_EQ(base::UTF8ToUTF16("chromium.test"), new_url->data().keyword());
  EXPECT_EQ(base::UTF8ToUTF16("Chrooome"), new_url->data().short_name());
  EXPECT_EQ(
      "https://chromium.test/index.php?title=chrooome&search={searchTerms}",
      new_url->data().url());
  EXPECT_EQ(GURL("https://chromium.test/favicon.ico"),
            new_url->data().favicon_url);
}

// Tests that SearchEngineTabHelper can add TemplateURL to TemplateURLService
// when a <form> submission generates a searchable URL and leads to a successful
// navigation.
TEST_F(SearchEngineTabHelperTest, AddTemplateURLBySearchableURL) {
  GURL page_url("https://chromium.test");
  GURL searchable_url(
      "https://chromium.test/index.php?title=chrooome&search={searchTerms}");
  // HTML of the search engine page, with a <form> navigates to pony.html as
  // search result page.
  NSString* html = [NSString
      stringWithFormat:@"<html><form id='f' action='%s'></form></html>",
                       server_.GetURL(kPonyHtmlFilePath).spec().c_str()];

  // Record the original TemplateURLs in TemplateURLService.
  std::vector<TemplateURL*> old_urls =
      template_url_service()->GetTemplateURLs();

  // Load an empty page, and send a message of openSearchUrl from Js.
  LoadHtml(html, page_url);
  SendMessageOfSearchableUrl(searchable_url);
  SubmitWebViewFormWithId(web_state(), "f");

  // Wait for TemplateURL added to TemplateURLService.
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return template_url_service()->GetTemplateURLs().size() ==
           old_urls.size() + 1;
  }));

  // TemplateURLService doesn't guarantee that newly added TemplateURL is
  // appended, so we should check in a general way that only one TemplateURL is
  // added and others remain untouched.
  TemplateURL* new_url = nullptr;
  for (TemplateURL* url : template_url_service()->GetTemplateURLs()) {
    if (std::find(old_urls.begin(), old_urls.end(), url) == old_urls.end()) {
      ASSERT_FALSE(new_url);
      new_url = url;
    }
  }
  ASSERT_TRUE(new_url);
  EXPECT_EQ(base::UTF8ToUTF16("chromium.test"), new_url->data().keyword());
  EXPECT_EQ(base::UTF8ToUTF16("chromium.test"), new_url->data().short_name());
  EXPECT_EQ(searchable_url.spec(), new_url->data().url());
  const GURL expected_favicon_url = GURL(page_url.spec() + "favicon.ico");
  EXPECT_EQ(expected_favicon_url, new_url->data().favicon_url);
}

// Test fixture for SearchEngineTabHelper class in incognito mode.
class SearchEngineTabHelperIncognitoTest : public SearchEngineTabHelperTest {
 protected:
  SearchEngineTabHelperIncognitoTest() {}

  // Overrides WebTest::GetBrowserState.
  web::BrowserState* GetBrowserState() override {
    return chrome_browser_state_->GetOffTheRecordChromeBrowserState();
  }

  DISALLOW_COPY_AND_ASSIGN(SearchEngineTabHelperIncognitoTest);
};

// Tests that SearchEngineTabHelper doesn't add TemplateURL to
// TemplateURLService when a OSDD <link> is found in web page under incognito
// mode.
TEST_F(SearchEngineTabHelperIncognitoTest,
       NotAddTemplateURLByOpenSearchUnderIncognito) {
  GURL page_url("https://chromium.test");
  GURL osdd_url = server_.GetURL(kOpenSearchXmlFilePath);

  // Record the original TemplateURLs in TemplateURLService.
  std::vector<TemplateURL*> old_urls =
      template_url_service()->GetTemplateURLs();

  // Load an empty page, and send a message of openSearchUrl from Js.
  LoadHtml(@"<html></html>", page_url);
  ASSERT_TRUE(SendMessageOfOpenSearch(page_url, osdd_url));

  // No new TemplateURL should be added to TemplateURLService, wait for timeout.
  ASSERT_FALSE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return template_url_service()->GetTemplateURLs().size() ==
           old_urls.size() + 1;
  }));
  EXPECT_EQ(old_urls, template_url_service()->GetTemplateURLs());
}

// Tests that SearchEngineTabHelper doesn't add TemplateURL to
// TemplateURLService when a <form> submission generates a searchable URL and
// leads to a successful navigation under incognito mode.
TEST_F(SearchEngineTabHelperIncognitoTest,
       NotAddTemplateURLBySearchableURLUnderIncognito) {
  GURL page_url("https://chromium.test");
  GURL searchable_url(
      "https://chromium.test/index.php?title=chrooome&search={searchTerms}");
  // HTML of the search engine page, with a <form> navigates to pony.html as
  // search result page.
  NSString* html = [NSString
      stringWithFormat:@"<html><form id='f' action='%s'></form></html>",
                       server_.GetURL(kPonyHtmlFilePath).spec().c_str()];

  // Record the original TemplateURLs in TemplateURLService.
  std::vector<TemplateURL*> old_urls =
      template_url_service()->GetTemplateURLs();

  // Load an empty page, and send a message of openSearchUrl from Js.
  LoadHtml(html, page_url);
  SendMessageOfSearchableUrl(searchable_url);
  SubmitWebViewFormWithId(web_state(), "f");

  // Wait for TemplateURL added to TemplateURLService.
  ASSERT_FALSE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return template_url_service()->GetTemplateURLs().size() ==
           old_urls.size() + 1;
  }));
  EXPECT_EQ(old_urls, template_url_service()->GetTemplateURLs());
}
