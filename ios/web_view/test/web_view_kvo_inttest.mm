// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <ChromeWebView/ChromeWebView.h>
#import <Foundation/Foundation.h>

#include "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/url_formatter/elide_url.h"
#import "ios/web_view/test/observer.h"
#import "ios/web_view/test/web_view_inttest_base.h"
#import "ios/web_view/test/web_view_test_util.h"
#import "net/base/apple/url_conversions.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest_mac.h"
#include "url/gurl.h"

namespace ios_web_view {

// Tests that the KVO compliant properties of CWVWebView correctly report
// changes.
typedef ios_web_view::WebViewInttestBase WebViewKvoTest;

// Tests that CWVWebView correctly reports |canGoBack| and |canGoForward| state.
TEST_F(WebViewKvoTest, CanGoBackForward) {
  ASSERT_TRUE(test_server_->Start());

  Observer* back_observer = [[Observer alloc] init];
  [back_observer setObservedObject:web_view_ keyPath:@"canGoBack"];

  Observer* forward_observer = [[Observer alloc] init];
  [forward_observer setObservedObject:web_view_ keyPath:@"canGoForward"];

  ASSERT_FALSE(back_observer.lastValue);
  ASSERT_FALSE(forward_observer.lastValue);

  // Define pages in reverse order so the links can reference the "next" page.
  GURL page_3_url = GetUrlForPageWithTitleAndBody("Page 3", "Body 3");

  std::string page_2_html =
      "<a id='link_2' href='" + page_3_url.spec() + "'>Link 2</a>";
  GURL page_2_url = GetUrlForPageWithHtmlBody(page_2_html);

  std::string page_1_html =
      "<a id='link_1' href='" + page_2_url.spec() + "'>Link 1</a>";
  GURL page_1_url = GetUrlForPageWithHtmlBody(page_1_html);

  ASSERT_TRUE(test::LoadUrl(web_view_, net::NSURLWithGURL(page_1_url)));
  // Loading initial URL should not affect back/forward navigation state.
  EXPECT_FALSE([back_observer.lastValue boolValue]);
  EXPECT_FALSE([forward_observer.lastValue boolValue]);

  // Navigate to page 2.
  EXPECT_TRUE(test::TapWebViewElementWithId(web_view_, @"link_1"));
  ASSERT_TRUE(
      test::WaitForWebViewContainingTextOrTimeout(web_view_, @"Link 2"));
  EXPECT_TRUE([back_observer.lastValue boolValue]);
  EXPECT_FALSE([forward_observer.lastValue boolValue]);

  // Navigate back to page 1.
  [web_view_ goBack];
  ASSERT_TRUE(
      test::WaitForWebViewContainingTextOrTimeout(web_view_, @"Link 1"));
  EXPECT_FALSE([back_observer.lastValue boolValue]);
  EXPECT_TRUE([forward_observer.lastValue boolValue]);

  // Navigate forward to page 2.
  [web_view_ goForward];
  ASSERT_TRUE(
      test::WaitForWebViewContainingTextOrTimeout(web_view_, @"Link 2"));
  EXPECT_TRUE([back_observer.lastValue boolValue]);
  EXPECT_FALSE([forward_observer.lastValue boolValue]);

  // Navigate to page 3.
  EXPECT_TRUE(test::TapWebViewElementWithId(web_view_, @"link_2"));
  ASSERT_TRUE(
      test::WaitForWebViewContainingTextOrTimeout(web_view_, @"Body 3"));
  EXPECT_TRUE([back_observer.lastValue boolValue]);
  EXPECT_FALSE([forward_observer.lastValue boolValue]);

  // Navigate back to page 2.
  [web_view_ goBack];
  ASSERT_TRUE(
      test::WaitForWebViewContainingTextOrTimeout(web_view_, @"Link 2"));
  EXPECT_TRUE([back_observer.lastValue boolValue]);
  EXPECT_TRUE([forward_observer.lastValue boolValue]);
}

// Tests that CWVWebView correctly reports current |title|.
TEST_F(WebViewKvoTest, Title) {
  ASSERT_TRUE(test_server_->Start());

  Observer* observer = [[Observer alloc] init];
  [observer setObservedObject:web_view_ keyPath:@"title"];

  NSString* page_2_title = @"Page 2";
  GURL page_2_url = GetUrlForPageWithTitleAndBody(
      base::SysNSStringToUTF8(page_2_title), "Body 2");

  NSString* page_1_title = @"Page 1";
  std::string page_1_html = base::StringPrintf(
      "<a id='link_1' href='%s'>Link 1</a>", page_2_url.spec().c_str());
  GURL page_1_url = GetUrlForPageWithTitleAndBody(
      base::SysNSStringToUTF8(page_1_title), page_1_html);

  ASSERT_TRUE(test::LoadUrl(web_view_, net::NSURLWithGURL(page_1_url)));
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, ^{
        return [page_1_title isEqualToString:web_view_.title];
      }));
  EXPECT_NSEQ(page_1_title, observer.lastValue);

  // Navigate to page 2.
  EXPECT_TRUE(test::TapWebViewElementWithId(web_view_, @"link_1"));
  ASSERT_TRUE(
      test::WaitForWebViewContainingTextOrTimeout(web_view_, @"Body 2"));
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, ^{
        return [page_2_title isEqualToString:web_view_.title];
      }));
  EXPECT_NSEQ(page_2_title, observer.lastValue);

  // Navigate back to page 1.
  [web_view_ goBack];
  ASSERT_TRUE(
      test::WaitForWebViewContainingTextOrTimeout(web_view_, @"Link 1"));
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, ^{
        return [page_1_title isEqualToString:web_view_.title];
      }));
  EXPECT_NSEQ(page_1_title, observer.lastValue);
}

// Tests that CWVWebView correctly reports |isLoading| value.
TEST_F(WebViewKvoTest, Loading) {
  ASSERT_TRUE(test_server_->Start());

  Observer* observer = [[Observer alloc] init];
  [observer setObservedObject:web_view_ keyPath:@"loading"];

  GURL page_2_url = GetUrlForPageWithTitleAndBody("Page 2", "Body 2");

  std::string page_1_html = base::StringPrintf(
      "<a id='link_1' href='%s'>Link 1</a>", page_2_url.spec().c_str());
  GURL page_1_url = GetUrlForPageWithTitleAndBody("Page 1", page_1_html);

  ASSERT_TRUE(test::LoadUrl(web_view_, net::NSURLWithGURL(page_1_url)));
  EXPECT_TRUE([observer.previousValue boolValue]);
  EXPECT_FALSE([observer.lastValue boolValue]);

  // Navigate to page 2.
  EXPECT_TRUE(test::TapWebViewElementWithId(web_view_, @"link_1"));
  ASSERT_TRUE(
      test::WaitForWebViewContainingTextOrTimeout(web_view_, @"Body 2"));
  EXPECT_TRUE([observer.previousValue boolValue]);
  EXPECT_FALSE([observer.lastValue boolValue]);

  // Navigate back to page 1.
  [web_view_ goBack];
  ASSERT_TRUE(
      test::WaitForWebViewContainingTextOrTimeout(web_view_, @"Link 1"));
  EXPECT_TRUE([observer.previousValue boolValue]);
  EXPECT_FALSE([observer.lastValue boolValue]);
}

// Tests that CWVWebView correctly reports |visibleURL| and |lastCommittedURL|.
TEST_F(WebViewKvoTest, URLs) {
  ASSERT_TRUE(test_server_->Start());

  Observer* last_committed_url_observer = [[Observer alloc] init];
  [last_committed_url_observer setObservedObject:web_view_
                                         keyPath:@"lastCommittedURL"];

  Observer* visible_url_observer = [[Observer alloc] init];
  [visible_url_observer setObservedObject:web_view_ keyPath:@"visibleURL"];

  Observer* visible_location_string_observer = [[Observer alloc] init];
  [visible_location_string_observer setObservedObject:web_view_
                                              keyPath:@"visibleLocationString"];

  GURL page_2 = GetUrlForPageWithTitleAndBody("Page 2", "Body 2");
  NSURL* page_2_url = net::NSURLWithGURL(page_2);
  NSString* page_2_location_string = base::SysUTF16ToNSString(
      url_formatter::FormatUrlForSecurityDisplay(page_2));

  std::string page_1_html = base::StringPrintf(
      "<a id='link_1' href='%s'>Link 1</a>", page_2.spec().c_str());
  GURL page_1 = GetUrlForPageWithTitleAndBody("Page 1", page_1_html);
  NSURL* page_1_url = net::NSURLWithGURL(page_1);
  NSString* page_1_location_string = base::SysUTF16ToNSString(
      url_formatter::FormatUrlForSecurityDisplay(page_1));

  [web_view_ loadRequest:[NSURLRequest requestWithURL:page_1_url]];

  // |visibleURL| will update immediately
  EXPECT_NSEQ(page_1_url, visible_url_observer.lastValue);
  EXPECT_NSEQ(page_1_location_string,
              visible_location_string_observer.lastValue);

  ASSERT_TRUE(
      test::WaitForWebViewContainingTextOrTimeout(web_view_, @"Link 1"));
  EXPECT_NSEQ(page_1_url, last_committed_url_observer.lastValue);
  EXPECT_NSEQ(page_1_url, visible_url_observer.lastValue);
  EXPECT_NSEQ(page_1_location_string,
              visible_location_string_observer.lastValue);

  // Navigate to page 2.
  EXPECT_TRUE(test::TapWebViewElementWithId(web_view_, @"link_1"));
  ASSERT_TRUE(
      test::WaitForWebViewContainingTextOrTimeout(web_view_, @"Body 2"));
  EXPECT_NSEQ(page_2_url, last_committed_url_observer.lastValue);
  EXPECT_NSEQ(page_2_url, visible_url_observer.lastValue);
  EXPECT_NSEQ(page_2_location_string,
              visible_location_string_observer.lastValue);

  // Navigate back to page 1.
  [web_view_ goBack];
  ASSERT_TRUE(
      test::WaitForWebViewContainingTextOrTimeout(web_view_, @"Link 1"));
  EXPECT_NSEQ(page_1_url, last_committed_url_observer.lastValue);
  EXPECT_NSEQ(page_1_url, visible_url_observer.lastValue);
  EXPECT_NSEQ(page_1_location_string,
              visible_location_string_observer.lastValue);
}

}  // namespace ios_web_view
