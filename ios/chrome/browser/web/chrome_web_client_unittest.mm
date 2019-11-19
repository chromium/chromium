// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/web/chrome_web_client.h"

#import <UIKit/UIKit.h>

#include <memory>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/passwords/password_manager_features.h"
#import "ios/chrome/browser/web/error_page_util.h"
#import "ios/web/common/web_view_creation_util.h"
#import "ios/web/public/test/error_test_util.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#import "ios/web/public/test/js_test_util.h"
#include "ios/web/public/test/scoped_testing_web_client.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForActionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace {
const char kTestUrl[] = "http://chromium.test";

// Error used to test PrepareErrorPage method.
NSError* CreateTestError() {
  return web::testing::CreateTestNetError([NSError
      errorWithDomain:NSURLErrorDomain
                 code:NSURLErrorNetworkConnectionLost
             userInfo:@{
               NSURLErrorFailingURLStringErrorKey :
                   base::SysUTF8ToNSString(kTestUrl)
             }]);
}
}  // namespace

class ChromeWebClientTest : public PlatformTest {
 public:
  ChromeWebClientTest() {
    browser_state_ = TestChromeBrowserState::Builder().Build();
  }

  ~ChromeWebClientTest() override = default;

  ios::ChromeBrowserState* browser_state() { return browser_state_.get(); }

 private:
  base::test::TaskEnvironment environment_;
  std::unique_ptr<ios::ChromeBrowserState> browser_state_;

  DISALLOW_COPY_AND_ASSIGN(ChromeWebClientTest);
};

TEST_F(ChromeWebClientTest, UserAgent) {
  std::vector<std::string> pieces;

  // Check if the pieces of the user agent string come in the correct order.
  ChromeWebClient web_client;
  std::string buffer = web_client.GetUserAgent(web::UserAgentType::MOBILE);

  pieces = base::SplitStringUsingSubstr(
      buffer, "Mozilla/5.0 (", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  ASSERT_EQ(2u, pieces.size());
  buffer = pieces[1];
  EXPECT_EQ("", pieces[0]);

  pieces = base::SplitStringUsingSubstr(
      buffer, ") AppleWebKit/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  ASSERT_EQ(2u, pieces.size());
  buffer = pieces[1];
  std::string os_str = pieces[0];

  pieces =
      base::SplitStringUsingSubstr(buffer, " (KHTML, like Gecko) ",
                                   base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  ASSERT_EQ(2u, pieces.size());
  buffer = pieces[1];
  std::string webkit_version_str = pieces[0];

  pieces = base::SplitStringUsingSubstr(
      buffer, " Safari/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  ASSERT_EQ(2u, pieces.size());
  std::string product_str = pieces[0];
  std::string safari_version_str = pieces[1];

  // Not sure what can be done to better check the OS string, since it's highly
  // platform-dependent.
  EXPECT_FALSE(os_str.empty());

  EXPECT_FALSE(webkit_version_str.empty());
  EXPECT_FALSE(safari_version_str.empty());

  EXPECT_EQ(0u, product_str.find("CriOS/"));
}

// Tests that ChromeWebClient provides accessibility script for WKWebView.
TEST_F(ChromeWebClientTest, WKWebViewEarlyPageScriptAccessibility) {
  // Chrome scripts rely on __gCrWeb object presence.
  WKWebView* web_view = web::BuildWKWebView(CGRectZero, browser_state());
  web::test::ExecuteJavaScript(web_view, @"__gCrWeb = {};");

  web::ScopedTestingWebClient web_client(std::make_unique<ChromeWebClient>());
  NSString* script =
      web_client.Get()->GetDocumentStartScriptForAllFrames(browser_state());
  web::test::ExecuteJavaScript(web_view, script);
  EXPECT_NSEQ(@"object", web::test::ExecuteJavaScript(
                             web_view, @"typeof __gCrWeb.accessibility"));
}

// Tests that ChromeWebClient provides print script for WKWebView.
TEST_F(ChromeWebClientTest, WKWebViewEarlyPageScriptPrint) {
  // Chrome scripts rely on __gCrWeb object presence.
  WKWebView* web_view = web::BuildWKWebView(CGRectZero, browser_state());
  web::test::ExecuteJavaScript(web_view, @"__gCrWeb = {};");

  web::ScopedTestingWebClient web_client(std::make_unique<ChromeWebClient>());
  NSString* script =
      web_client.Get()->GetDocumentStartScriptForAllFrames(browser_state());
  web::test::ExecuteJavaScript(web_view, script);
  EXPECT_NSEQ(@"object",
              web::test::ExecuteJavaScript(web_view, @"typeof __gCrWeb.print"));
}

// Tests that ChromeWebClient provides autofill controller script for WKWebView.
TEST_F(ChromeWebClientTest, WKWebViewEarlyPageScriptAutofillController) {
  // Chrome scripts rely on __gCrWeb object presence.
  WKWebView* web_view = web::BuildWKWebView(CGRectZero, browser_state());
  web::test::ExecuteJavaScript(web_view, @"__gCrWeb = {};");

  web::ScopedTestingWebClient web_client(std::make_unique<ChromeWebClient>());
  NSString* script =
      web_client.Get()->GetDocumentStartScriptForAllFrames(browser_state());
  web::test::ExecuteJavaScript(web_view, script);
  EXPECT_NSEQ(@"object", web::test::ExecuteJavaScript(
                             web_view, @"typeof __gCrWeb.autofill"));
}

// Tests that ChromeWebClient provides credential manager script for WKWebView
// if and only if the feature is enabled.
TEST_F(ChromeWebClientTest, WKWebViewEarlyPageScriptCredentialManager) {
  // Chrome scripts rely on __gCrWeb object presence.
  WKWebView* web_view = web::BuildWKWebView(CGRectZero, browser_state());
  web::test::ExecuteJavaScript(web_view, @"__gCrWeb = {};");

  web::ScopedTestingWebClient web_client(std::make_unique<ChromeWebClient>());
  NSString* script =
      web_client.Get()->GetDocumentStartScriptForMainFrame(browser_state());
  web::test::ExecuteJavaScript(web_view, script);
  EXPECT_NSEQ(@"undefined", web::test::ExecuteJavaScript(
                                web_view, @"typeof navigator.credentials"));

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kCredentialManager);
  script =
      web_client.Get()->GetDocumentStartScriptForMainFrame(browser_state());
  web::test::ExecuteJavaScript(web_view, script);
  EXPECT_NSEQ(@"object", web::test::ExecuteJavaScript(
                             web_view, @"typeof navigator.credentials"));
}

// Tests that ChromeWebClient provides payment request script for WKWebView.
TEST_F(ChromeWebClientTest, WKWebViewEarlyPageScriptPaymentRequest) {
  // Chrome scripts rely on __gCrWeb object presence.
  WKWebView* web_view = web::BuildWKWebView(CGRectZero, browser_state());
  web::test::ExecuteJavaScript(web_view, @"__gCrWeb = {};");

  web::ScopedTestingWebClient web_client(std::make_unique<ChromeWebClient>());
  NSString* script =
      web_client.Get()->GetDocumentStartScriptForMainFrame(browser_state());
  web::test::ExecuteJavaScript(web_view, script);
  EXPECT_NSEQ(@"function", web::test::ExecuteJavaScript(
                               web_view, @"typeof window.PaymentRequest"));
}

// Tests PrepareErrorPage wth non-post, not Off The Record error.
TEST_F(ChromeWebClientTest, PrepareErrorPageNonPostNonOtr) {
  ChromeWebClient web_client;
  NSError* error = CreateTestError();
  __block bool callback_called = false;
  __block NSString* page = nil;
  base::OnceCallback<void(NSString*)> callback =
      base::BindOnce(^(NSString* error_html) {
        callback_called = true;
        page = error_html;
      });
  web::TestWebState test_web_state;
  web_client.PrepareErrorPage(&test_web_state, GURL(kTestUrl), error,
                              /*is_post=*/false,
                              /*is_off_the_record=*/false,
                              /*info=*/base::nullopt,
                              /*navigation_id=*/0, std::move(callback));
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return callback_called;
  }));
  EXPECT_NSEQ(GetErrorPage(GURL(kTestUrl), error, /*is_post=*/false,
                           /*is_off_the_record=*/false),
              page);
}

// Tests PrepareErrorPage with post, not Off The Record error.
TEST_F(ChromeWebClientTest, PrepareErrorPagePostNonOtr) {
  ChromeWebClient web_client;
  NSError* error = CreateTestError();
  __block bool callback_called = false;
  __block NSString* page = nil;
  base::OnceCallback<void(NSString*)> callback =
      base::BindOnce(^(NSString* error_html) {
        callback_called = true;
        page = error_html;
      });
  web::TestWebState test_web_state;
  web_client.PrepareErrorPage(&test_web_state, GURL(kTestUrl), error,
                              /*is_post=*/true,
                              /*is_off_the_record=*/false,
                              /*info=*/base::nullopt,
                              /*navigation_id=*/0, std::move(callback));
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return callback_called;
  }));
  EXPECT_NSEQ(GetErrorPage(GURL(kTestUrl), error, /*is_post=*/true,
                           /*is_off_the_record=*/false),
              page);
}

// Tests PrepareErrorPage with non-post, Off The Record error.
TEST_F(ChromeWebClientTest, PrepareErrorPageNonPostOtr) {
  ChromeWebClient web_client;
  NSError* error = CreateTestError();
  __block bool callback_called = false;
  __block NSString* page = nil;
  base::OnceCallback<void(NSString*)> callback =
      base::BindOnce(^(NSString* error_html) {
        callback_called = true;
        page = error_html;
      });
  web::TestWebState test_web_state;
  web_client.PrepareErrorPage(&test_web_state, GURL(kTestUrl), error,
                              /*is_post=*/false,
                              /*is_off_the_record=*/true,
                              /*info=*/base::nullopt,
                              /*navigation_id=*/0, std::move(callback));
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return callback_called;
  }));
  EXPECT_NSEQ(GetErrorPage(GURL(kTestUrl), error, /*is_post=*/false,
                           /*is_off_the_record=*/true),
              page);
}

// Tests PrepareErrorPage with post, Off The Record error.
TEST_F(ChromeWebClientTest, PrepareErrorPagePostOtr) {
  ChromeWebClient web_client;
  NSError* error = CreateTestError();
  __block bool callback_called = false;
  __block NSString* page = nil;
  base::OnceCallback<void(NSString*)> callback =
      base::BindOnce(^(NSString* error_html) {
        callback_called = true;
        page = error_html;
      });
  web::TestWebState test_web_state;
  web_client.PrepareErrorPage(&test_web_state, GURL(kTestUrl), error,
                              /*is_post=*/true,
                              /*is_off_the_record=*/true,
                              /*info=*/base::nullopt,
                              /*navigation_id=*/0, std::move(callback));
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return callback_called;
  }));
  EXPECT_NSEQ(GetErrorPage(GURL(kTestUrl), error, /*is_post=*/true,
                           /*is_off_the_record=*/true),
              page);
}

// Tests PrepareErrorPage with SSLInfo, which results in an SSL committed
// interstitial.
TEST_F(ChromeWebClientTest, PrepareErrorPageWithSSLInfo) {
  net::SSLInfo info;
  info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  info.is_fatal_cert_error = false;
  info.cert_status = net::CERT_STATUS_COMMON_NAME_INVALID;
  base::Optional<net::SSLInfo> ssl_info =
      base::make_optional<net::SSLInfo>(info);
  ChromeWebClient web_client;
  NSError* error =
      [NSError errorWithDomain:NSURLErrorDomain
                          code:NSURLErrorServerCertificateHasUnknownRoot
                      userInfo:nil];
  __block bool callback_called = false;
  __block NSString* page = nil;
  base::OnceCallback<void(NSString*)> callback =
      base::BindOnce(^(NSString* error_html) {
        callback_called = true;
        page = error_html;
      });
  web::TestWebState test_web_state;
  test_web_state.SetBrowserState(browser_state());
  web_client.PrepareErrorPage(&test_web_state, GURL(kTestUrl), error,
                              /*is_post=*/false,
                              /*is_off_the_record=*/false,
                              /*info=*/ssl_info,
                              /*navigation_id=*/0, std::move(callback));
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return callback_called;
  }));
  NSString* error_string = base::SysUTF8ToNSString(
      net::ErrorToShortString(net::ERR_CERT_COMMON_NAME_INVALID));
  EXPECT_TRUE([page containsString:error_string]);
}
