// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search_engines/model/search_engine_tab_helper.h"

#import "base/containers/contains.h"
#import "base/functional/bind.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/bind.h"
#import "base/test/ios/wait_util.h"
#import "components/favicon/core/favicon_service.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/search_engines/search_engines_test_environment.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/favicon/model/favicon_service_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/web/model/chrome_web_client.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForJSCompletionTimeout;
using web::test::SubmitWebViewFormWithId;

namespace {
const char kOpenSearchXmlFilePath[] =
    "/ios/testing/data/http_server_files/opensearch.xml";
const char kPonyHtmlFilePath[] =
    "/ios/testing/data/http_server_files/pony.html";
}

// Test fixture for SearchEngineTabHelper class.
class SearchEngineTabHelperTest : public PlatformTest {
 public:
  SearchEngineTabHelperTest(const SearchEngineTabHelperTest&) = delete;
  SearchEngineTabHelperTest& operator=(const SearchEngineTabHelperTest&) =
      delete;

 protected:
  SearchEngineTabHelperTest()
      : web_client_(std::make_unique<ChromeWebClient>()) {}

  void SetUp() override {
    PlatformTest::SetUp();

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        base::BindLambdaForTesting(
            [this](web::BrowserState*) -> std::unique_ptr<KeyedService> {
              std::unique_ptr<TemplateURLService> model =
                  search_engines_test_environment_.ReleaseTemplateURLService();
              return model;
            }));

    profile_ = std::move(builder).Build();
    web::WebState::CreateParams params(profile_.get());
    web_state_ = web::WebState::Create(params);
    web_state_->GetView();
    web_state_->SetKeepRenderProcessAlive(true);

    favicon::WebFaviconDriver::CreateForWebState(
        web_state(), ios::FaviconServiceFactory::GetForProfile(
                         profile_.get(), ServiceAccessType::IMPLICIT_ACCESS));
    SearchEngineTabHelper::CreateForWebState(web_state());
    server_.ServeFilesFromSourceDirectory(".");
    ASSERT_TRUE(server_.Start());
    template_url_service()->Load();
  }

  // Returns the testing TemplateURLService.
  TemplateURLService* template_url_service() {
    ProfileIOS* profile = ProfileIOS::FromBrowserState(profile_.get());
    return ios::TemplateURLServiceFactory::GetForProfile(profile);
  }

  web::WebState* web_state() { return web_state_.get(); }

  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
  web::ScopedTestingWebClient web_client_;
  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::MainThreadType::IO};
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::WebState> web_state_;

  net::EmbeddedTestServer server_;
};

// Tests that SearchEngineTabHelper can add TemplateURL to TemplateURLService
// when a OSDD <link> is found in web page.
TEST_F(SearchEngineTabHelperTest, AddTemplateURLByOpenSearch) {
  GURL page_url("https://chromium.test");
  GURL osdd_url = server_.GetURL(kOpenSearchXmlFilePath);

  // Record the original TemplateURLs in TemplateURLService.
  std::vector<raw_ptr<TemplateURL, VectorExperimental>> old_urls =
      template_url_service()->GetTemplateURLs();

  // Load an empty page, and send a message of openSearchUrl from Js.
  web::test::LoadHtml(@"<html></html>", page_url, web_state());
  SearchEngineTabHelper::FromWebState(web_state())
      ->AddTemplateURLByOSDD(page_url, osdd_url);

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
    if (!base::Contains(old_urls, url)) {
      ASSERT_FALSE(new_url);
      new_url = url;
    }
  }
  ASSERT_TRUE(new_url);
  EXPECT_EQ(u"chromium.test", new_url->data().keyword());
  EXPECT_EQ(u"Chrooome", new_url->data().short_name());
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
  std::vector<raw_ptr<TemplateURL, VectorExperimental>> old_urls =
      template_url_service()->GetTemplateURLs();

  // Load an empty page, and send a message of openSearchUrl from Js.
  web::test::LoadHtml(html, page_url, web_state());
  SearchEngineTabHelper::FromWebState(web_state())
      ->SetSearchableUrl(searchable_url);
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
    if (!base::Contains(old_urls, url)) {
      ASSERT_FALSE(new_url);
      new_url = url;
    }
  }
  ASSERT_TRUE(new_url);
  EXPECT_EQ(u"chromium.test", new_url->data().keyword());
  EXPECT_EQ(u"chromium.test", new_url->data().short_name());
  EXPECT_EQ(searchable_url.spec(), new_url->data().url());
  const GURL expected_favicon_url = GURL(page_url.spec() + "favicon.ico");
  EXPECT_EQ(expected_favicon_url, new_url->data().favicon_url);
}

// Test fixture for SearchEngineTabHelper class in incognito mode.
class SearchEngineTabHelperIncognitoTest : public SearchEngineTabHelperTest {
 public:
  SearchEngineTabHelperIncognitoTest(
      const SearchEngineTabHelperIncognitoTest&) = delete;
  SearchEngineTabHelperIncognitoTest& operator=(
      const SearchEngineTabHelperIncognitoTest&) = delete;

 protected:
  SearchEngineTabHelperIncognitoTest() {}

  void SetUp() override {
    SearchEngineTabHelperTest::SetUp();

    ProfileIOS* incognito_profile = profile_->GetOffTheRecordProfile();

    // TemplateURLServiceFactory redirects to the original profile, so it
    // doesn't really matter which profile is used in tests to interact
    // with TemplateURLService.
    ASSERT_EQ(template_url_service(),
              ios::TemplateURLServiceFactory::GetForProfile(incognito_profile));

    web::WebState::CreateParams params(incognito_profile);
    incognito_web_state_ = web::WebState::Create(params);
    incognito_web_state_->SetKeepRenderProcessAlive(true);

    // SearchEngineTabHelper depends on WebFaviconDriver, which must be created
    // before and using the original (non-incognito) profile, in
    // consistency with the logic in AttachTabHelpers().
    favicon::WebFaviconDriver::CreateForWebState(
        incognito_web_state(),
        ios::FaviconServiceFactory::GetForProfile(
            profile_.get(), ServiceAccessType::IMPLICIT_ACCESS));
    SearchEngineTabHelper::CreateForWebState(incognito_web_state());
  }

  web::WebState* incognito_web_state() { return incognito_web_state_.get(); }

 private:
  std::unique_ptr<web::WebState> incognito_web_state_;
};

// Tests that SearchEngineTabHelper doesn't add TemplateURL to
// TemplateURLService when a OSDD <link> is found in web page under incognito
// mode.
TEST_F(SearchEngineTabHelperIncognitoTest,
       NotAddTemplateURLByOpenSearchUnderIncognito) {
  GURL page_url("https://chromium.test");
  GURL osdd_url = server_.GetURL(kOpenSearchXmlFilePath);

  // Record the original TemplateURLs in TemplateURLService.
  std::vector<raw_ptr<TemplateURL, VectorExperimental>> old_urls =
      template_url_service()->GetTemplateURLs();

  // Load an empty page, and send a message of openSearchUrl from Js.
  web::test::LoadHtml(@"<html></html>", page_url, incognito_web_state());
  SearchEngineTabHelper::FromWebState(incognito_web_state())
      ->AddTemplateURLByOSDD(page_url, osdd_url);

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
  std::vector<raw_ptr<TemplateURL, VectorExperimental>> old_urls =
      template_url_service()->GetTemplateURLs();

  // Load an empty page, and send a message of openSearchUrl from Js.
  web::test::LoadHtml(html, page_url, incognito_web_state());
  SearchEngineTabHelper::FromWebState(incognito_web_state())
      ->SetSearchableUrl(searchable_url);
  SubmitWebViewFormWithId(incognito_web_state(), "f");

  // Wait for TemplateURL added to TemplateURLService.
  ASSERT_FALSE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return template_url_service()->GetTemplateURLs().size() ==
           old_urls.size() + 1;
  }));
  EXPECT_EQ(old_urls, template_url_service()->GetTemplateURLs());
}
