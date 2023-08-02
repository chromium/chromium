// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/crw_error_page_helper.h"

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/public/test/web_view_content_test_util.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"
#import "ios/web/public/web_state.h"
#import "ios/web/test/web_int_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "url/url_canon.h"

namespace {
const char kInitialBody[] = "This is the initial body.";
const char kOriginalBody[] = "Body of the original page.";
const char kInjectedBody[] = "New injected body";
const char kSecondPageBody[] = "Second Page Body";
}  // namespace

namespace web {

// Class for the Error Page test.
class CRWErrorPageHelperIntTest : public WebIntTest {
 protected:
  CRWErrorPageHelperIntTest() {
    server_.RegisterRequestHandler(base::BindRepeating(
        &CRWErrorPageHelperIntTest::HandleRequest, base::Unretained(this)));
    EXPECT_TRUE(server_.Start()) << "Server didn't start";
  }

  // Returns an error page helper initialized with `url` as the url of the
  // failing page (original page).
  CRWErrorPageHelper* HelperForUrl(const std::string& url) {
    NSString* url_string = base::SysUTF8ToNSString(url);
    NSError* error = [NSError
        errorWithDomain:NSURLErrorDomain
                   code:NSURLErrorBadURL
               userInfo:@{NSURLErrorFailingURLStringErrorKey : url_string}];

    return [[CRWErrorPageHelper alloc] initWithError:error];
  }

  // Returns the initial url. This url can be seen as the url of the page loaded
  // if the original load failed during the provisional navigation.
  GURL GetInitialUrl() { return server_.GetURL("/error_page.html"); }

  // Returns the second url, only used to navigate back.
  GURL GetSecondUrl() { return server_.GetURL("/second_page.html"); }

  // Returns the original url. This is the url of the page which load failed.
  GURL GetOriginalUrl() { return server_.GetURL("/original_page.html"); }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);
    http_response->set_content_type("text/html");
    if (request.GetURL() == GetInitialUrl()) {
      http_response->set_content("<head></head><body>" +
                                 std::string(kInitialBody) + "</body>");
      return http_response;
    } else if (request.GetURL() == GetSecondUrl()) {
      http_response->set_content(kSecondPageBody);
      return http_response;
    } else if (request.GetURL() == GetOriginalUrl()) {
      http_response->set_content(kOriginalBody);
      return http_response;
    }
    return nullptr;
  }

  // Returns the html to be injected.
  NSString* GetInjectedHtml() {
    return [NSString
        stringWithFormat:@"<head></head><body>%s</body>", kInjectedBody];
  }

 private:
  net::EmbeddedTestServer server_;
};

// Tests that injecting HTML with Reload = YES is replacing the content of the
// page with the injected HTML and navigating back reload the original URL.
TEST_F(CRWErrorPageHelperIntTest, InjectHTMLAndReload) {
  CRWErrorPageHelper* helper = HelperForUrl(GetOriginalUrl().spec());

  // Load the initial error page.
  ASSERT_TRUE(LoadUrl(GetInitialUrl()));
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), kInitialBody));

  // Inject the HTML and check that it is replacing the content.
  web::test::ExecuteJavaScript(
      web_state(),
      base::SysNSStringToUTF8([helper scriptForInjectingHTML:GetInjectedHtml()
                                          addAutomaticReload:YES]));

  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), kInjectedBody));
  ASSERT_TRUE(test::WaitForWebViewNotContainingText(web_state(), kInitialBody));

  EXPECT_EQ(GetInitialUrl(), web_state()->GetVisibleURL());

  // Load a new page and trigger a back navigation.
  ASSERT_TRUE(LoadUrl(GetSecondUrl()));
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), kSecondPageBody));
  navigation_manager()->GoBack();

  // Check that the original page is loaded.
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), kOriginalBody));
  ASSERT_TRUE(
      test::WaitForWebViewNotContainingText(web_state(), kInjectedBody));
  ASSERT_TRUE(test::WaitForWebViewNotContainingText(web_state(), kInitialBody));
}

// Tests that injecting HTML with Reload = NO is replacing the content of the
// page with the injected HTML and navigating back hit the cache.
TEST_F(CRWErrorPageHelperIntTest, InjectHTMLWithoutReload) {
  CRWErrorPageHelper* helper = HelperForUrl(GetOriginalUrl().spec());

  // Load the initial error page.
  ASSERT_TRUE(LoadUrl(GetInitialUrl()));
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), kInitialBody));

  // Inject the HTML and check that it is replacing the content.
  web::test::ExecuteJavaScript(
      web_state(),
      base::SysNSStringToUTF8([helper scriptForInjectingHTML:GetInjectedHtml()
                                          addAutomaticReload:NO]));

  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), kInjectedBody));
  ASSERT_TRUE(test::WaitForWebViewNotContainingText(web_state(), kInitialBody));

  EXPECT_EQ(GetInitialUrl(), web_state()->GetVisibleURL());

  // Load a new page and trigger a back navigation.
  ASSERT_TRUE(LoadUrl(GetSecondUrl()));
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), kSecondPageBody));
  navigation_manager()->GoBack();

  // Check that the original page is loaded.
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), kInjectedBody));
  ASSERT_TRUE(test::WaitForWebViewNotContainingText(web_state(), kInitialBody));
  ASSERT_TRUE(
      test::WaitForWebViewNotContainingText(web_state(), kOriginalBody));
}

}  // namespace web
