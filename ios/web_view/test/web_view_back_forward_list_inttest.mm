// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <ChromeWebView/ChromeWebView.h>
#import <Foundation/Foundation.h>

#import "base/test/ios/wait_util.h"
#import "ios/web_view/test/observer.h"
#import "ios/web_view/test/web_view_inttest_base.h"
#import "ios/web_view/test/web_view_test_util.h"
#import "net/base/apple/url_conversions.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest_mac.h"
#include "url/gurl.h"

@interface CWVBackForwardListTestNavigationObserver
    : NSObject <CWVNavigationDelegate>

// Whether |webViewDidFinishNavigation| has been called. Initiated as NO.
@property(nonatomic, assign, readonly) BOOL pageLoaded;

- (void)webViewDidFinishNavigation:(CWVWebView*)webView;

@end

@implementation CWVBackForwardListTestNavigationObserver

- (void)webViewDidFinishNavigation:(CWVWebView*)webView {
  _pageLoaded = YES;
}

@end

namespace ios_web_view {

// Tests all CWVBackForwardList-related functionalities.
class WebViewBackForwardListTest : public WebViewInttestBase {
 protected:
  // Lets test_server_ produce some html pages and return the URLs.
  void GenerateTestPageUrls() {
    page1_url_ = GetUrlForPageWithHtml(
        "<html><header><title>page1</title></header><body>1</body></html>");
    page2_url_ = GetUrlForPageWithHtml(
        "<html><header><title>page2</title></header><body>2</body></html>");
    page3_url_ = GetUrlForPageWithHtml(
        "<html><header><title>page3</title></header><body>3</body></html>");
    page4_url_ = GetUrlForPageWithHtml(
        "<html><header><title>page4</title></header><body>4</body></html>");
  }

  // Loads a URL then waits for the load to complete and the page title to
  // update to the |expected| value.
  bool LoadUrlAndWaitForTitle(const GURL& url, NSString* title) {
    bool success = test::LoadUrl(web_view_, net::NSURLWithGURL(url));
    success = success && base::test::ios::WaitUntilConditionOrTimeout(
                             base::test::ios::kWaitForJSCompletionTimeout, ^{
                               return [title isEqualToString:web_view_.title];
                             });
    return success;
  }

  // Waits until web_view_ has loaded a page.
  bool WaitUntilPageLoaded() {
    CWVBackForwardListTestNavigationObserver* observer =
        [[CWVBackForwardListTestNavigationObserver alloc] init];
    web_view_.navigationDelegate = observer;
    bool result = base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForPageLoadTimeout, ^{
          return observer.pageLoaded;
        });
    web_view_.navigationDelegate = nil;
    return result;
  }

  // Waits for the value of executing `document.title` JavaScript to equal
  // `title`.
  bool WaitForJSDocumentTitle(NSString* title) {
    EXPECT_TRUE(WaitUntilPageLoaded());
    return base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForJSCompletionTimeout, ^{
          return [title
              isEqual:test::EvaluateJavaScript(web_view_, @"document.title")];
        });
  }

  GURL page1_url_;
  GURL page2_url_;
  GURL page3_url_;
  GURL page4_url_;
};

// Tests if a CWVBackForwardList can be correctly created from CWVWebView, and
// tests if it can go to the correct page by the generated list items.
TEST_F(WebViewBackForwardListTest,
       GenerateBackForwardListItemAndGoToBackForwardListItem) {
  ASSERT_TRUE(test_server_->Start());
  GenerateTestPageUrls();

  // Go to page3
  ASSERT_TRUE(LoadUrlAndWaitForTitle(page1_url_, @"page1"));
  ASSERT_TRUE(LoadUrlAndWaitForTitle(page2_url_, @"page2"));
  ASSERT_TRUE(LoadUrlAndWaitForTitle(page3_url_, @"page3"));
  // Now it should be in page3
  ASSERT_NSEQ(@"page3", test::EvaluateJavaScript(web_view_, @"document.title"));

  CWVBackForwardList* list = web_view_.backForwardList;
  // Tests |backList|
  ASSERT_EQ(2UL, list.backList.count);
  CWVBackForwardListItem* lastPageItem = list.backList[1];
  EXPECT_NSEQ(net::NSURLWithGURL(page2_url_), lastPageItem.URL);
  EXPECT_NSEQ(@"page2", lastPageItem.title);
  EXPECT_NSEQ(net::NSURLWithGURL(page1_url_), list.backList[0].URL);
  EXPECT_NSEQ(@"page1", list.backList[0].title);
  // Tests |backItem|
  EXPECT_NSEQ(lastPageItem, list.backItem);
  // Tests |currentItem|
  EXPECT_NSEQ(net::NSURLWithGURL(page3_url_), list.currentItem.URL);
  EXPECT_NSEQ(@"page3", list.currentItem.title);
  // Tests |forwardList|
  EXPECT_EQ(0UL, list.forwardList.count);
  // Tests |forwardItem|
  EXPECT_FALSE(list.forwardItem);

  // Go to page2 by |goToBackForwardListItem:|
  ASSERT_TRUE([web_view_ goToBackForwardListItem:lastPageItem]);
  // Now it should be in page2
  ASSERT_TRUE(WaitForJSDocumentTitle(@"page2"));

  // The |list| should always be same as |web_view_.backForwardList|, to be
  // consistent with the API in WKWebView. Instead, the properties of |list|
  // will be changed after navigation operations.
  ASSERT_EQ(web_view_.backForwardList, list);
  // Tests if it is no-op when going to current item.
  EXPECT_FALSE([web_view_ goToBackForwardListItem:list.currentItem]);

  ASSERT_EQ(web_view_.backForwardList, list);
  // Tests |backList|
  ASSERT_EQ(1UL, list.backList.count);
  EXPECT_NSEQ(net::NSURLWithGURL(page1_url_), list.backList[0].URL);
  EXPECT_NSEQ(@"page1", list.backList[0].title);
  // Tests |forwardList|
  ASSERT_EQ(1UL, list.forwardList.count);
  EXPECT_NSEQ(net::NSURLWithGURL(page3_url_), list.forwardList[0].URL);
  EXPECT_NSEQ(@"page3", list.forwardList[0].title);
  // Tests |currentItem|
  EXPECT_NSEQ(net::NSURLWithGURL(page2_url_), list.currentItem.URL);
  EXPECT_NSEQ(@"page2", list.currentItem.title);

  // Go to page1
  ASSERT_TRUE([web_view_ canGoBack]);
  [web_view_ goBack];
  // Now it should be in page1
  ASSERT_TRUE(WaitForJSDocumentTitle(@"page1"));

  ASSERT_EQ(web_view_.backForwardList, list);
  EXPECT_FALSE([web_view_ canGoBack]);
  // Tests |backList|
  EXPECT_EQ(0UL, list.backList.count);
  // Tests |backItem|
  EXPECT_FALSE(list.backItem);
  // Tests |currentItem|
  EXPECT_NSEQ(net::NSURLWithGURL(page1_url_), list.currentItem.URL);
  EXPECT_NSEQ(@"page1", list.currentItem.title);
  // Tests |forwardList|
  ASSERT_EQ(2UL, list.forwardList.count);
  CWVBackForwardListItem* topPageItem = list.forwardList[1];
  EXPECT_NSEQ(net::NSURLWithGURL(page3_url_), topPageItem.URL);
  EXPECT_NSEQ(@"page3", topPageItem.title);
  // Tests |forwardItem|
  CWVBackForwardListItem* nextPageItem = list.forwardList[0];
  EXPECT_NSEQ(nextPageItem, list.forwardItem);
  EXPECT_NSEQ(net::NSURLWithGURL(page2_url_), nextPageItem.URL);
  EXPECT_NSEQ(@"page2", nextPageItem.title);

  // Go to page3 and tests going forward by
  // |goToBackForwardListItem:|
  ASSERT_TRUE([web_view_ goToBackForwardListItem:topPageItem]);
  // Now it should be in page3
  ASSERT_TRUE(WaitForJSDocumentTitle(@"page3"));

  // Go back to page1 and then go to page4 to make the items of page2 and page3
  // exipred
  ASSERT_TRUE([web_view_ goToBackForwardListItem:list.backList[0]]);
  // Now it should be in page1
  ASSERT_TRUE(WaitForJSDocumentTitle(@"page1"));
  // Go to page4 then
  ASSERT_TRUE(LoadUrlAndWaitForTitle(page4_url_, @"page4"));
  // Now it should be in page4
  ASSERT_NSEQ(@"page4", test::EvaluateJavaScript(web_view_, @"document.title"));
  EXPECT_EQ(1UL, list.backList.count);
  EXPECT_EQ(0UL, list.forwardList.count);

  // The page2 is expired now so |goToBackForwardListItem:| should do nothing
  // and return NO in this case.
  EXPECT_FALSE([web_view_ goToBackForwardListItem:lastPageItem]);
  EXPECT_NSEQ(@"page4", test::EvaluateJavaScript(web_view_, @"document.title"));
}

// Tests if a CWVBackForwardList can be correctly created from CWVWebView, and
// to see if |itemAtIndex:| works fine.
TEST_F(WebViewBackForwardListTest, TestBackForwardListItemAtIndex) {
  ASSERT_TRUE(test_server_->Start());
  GenerateTestPageUrls();

  // Go to page3
  ASSERT_TRUE(LoadUrlAndWaitForTitle(page1_url_, @"page1"));
  ASSERT_TRUE(LoadUrlAndWaitForTitle(page2_url_, @"page2"));
  ASSERT_TRUE(LoadUrlAndWaitForTitle(page3_url_, @"page3"));
  // Now it should be in page3
  ASSERT_NSEQ(@"page3", test::EvaluateJavaScript(web_view_, @"document.title"));

  CWVBackForwardList* list = web_view_.backForwardList;
  ASSERT_EQ(2UL, list.backList.count);
  EXPECT_EQ(0UL, list.forwardList.count);
  EXPECT_FALSE([list itemAtIndex:-3]);
  EXPECT_NSEQ(list.backList[0], [list itemAtIndex:-2]);
  EXPECT_NSEQ(list.backList[1], [list itemAtIndex:-1]);
  EXPECT_NSEQ(list.currentItem, [list itemAtIndex:0]);
  EXPECT_FALSE([list itemAtIndex:1]);

  // Go to page2
  ASSERT_TRUE([web_view_ goToBackForwardListItem:list.backList[1]]);
  // Now it should be in page2
  ASSERT_TRUE(WaitForJSDocumentTitle(@"page2"));

  list = web_view_.backForwardList;
  EXPECT_EQ(1UL, list.backList.count);
  EXPECT_EQ(1UL, list.forwardList.count);
  EXPECT_FALSE([list itemAtIndex:-2]);
  ASSERT_TRUE([list itemAtIndex:-1]);
  EXPECT_NSEQ(@"page1", [list itemAtIndex:-1].title);
  ASSERT_TRUE([list itemAtIndex:0]);
  EXPECT_NSEQ(@"page2", [list itemAtIndex:0].title);
  ASSERT_TRUE([list itemAtIndex:1]);
  EXPECT_NSEQ(@"page3", [list itemAtIndex:1].title);
  EXPECT_FALSE([list itemAtIndex:2]);

  // Go to page1
  ASSERT_TRUE([web_view_ canGoBack]);
  [web_view_ goBack];
  // Now it should be in page1
  ASSERT_TRUE(WaitForJSDocumentTitle(@"page1"));

  list = web_view_.backForwardList;
  ASSERT_EQ(2UL, list.forwardList.count);
  EXPECT_EQ(0UL, list.backList.count);
  EXPECT_FALSE([list itemAtIndex:-1]);
  EXPECT_NSEQ(list.currentItem, [list itemAtIndex:0]);
  EXPECT_NSEQ(list.forwardList[0], [list itemAtIndex:1]);
  EXPECT_NSEQ(list.forwardList[1], [list itemAtIndex:2]);
  EXPECT_FALSE([list itemAtIndex:3]);
}

// Tests if a CWVBackForwardListItemArray can be correctly iterated using
// a for-in statement.
TEST_F(WebViewBackForwardListTest, TestCWVBackForwardListItemArrayForInLoop) {
  ASSERT_TRUE(test_server_->Start());
  GenerateTestPageUrls();

  // Go to page3
  ASSERT_TRUE(LoadUrlAndWaitForTitle(page1_url_, @"page1"));
  ASSERT_TRUE(LoadUrlAndWaitForTitle(page2_url_, @"page2"));
  ASSERT_TRUE(LoadUrlAndWaitForTitle(page3_url_, @"page3"));
  // Now it should be in page3
  ASSERT_NSEQ(@"page3", test::EvaluateJavaScript(web_view_, @"document.title"));

  CWVBackForwardList* list = web_view_.backForwardList;
  ASSERT_EQ(2UL, list.backList.count);
  size_t i = 0;
  for (CWVBackForwardListItem* item in list.backList) {
    EXPECT_NSEQ(list.backList[i], item);
    ++i;
  }
  EXPECT_EQ(i, list.backList.count);
  i = 0;
  for (CWVBackForwardListItem* _ __unused in list.forwardList) {
    ++i;
  }
  EXPECT_EQ(i, list.forwardList.count);
}

}  // namespace ios_web_view
