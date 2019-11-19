// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/find_in_page/find_in_page_model.h"
#import "ios/chrome/browser/find_in_page/js_findinpage_manager.h"
#import "ios/chrome/browser/web/chrome_web_test.h"
#import "ios/web/public/deprecated/crw_js_injection_receiver.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Unit tests for the find_in_page.js JavaScript file.

namespace {

// JavaScript invocation format string, with one NSString placeholder for the
// search target and timeout set to 1000ms.
NSString* kJavaScriptSearchCallFormat =
    @"__gCrWeb.findInPage.highlightWord('%@', 1000)";

// Other JavaScript functions invoked by the tests.
NSString* kJavaScriptIncrementIndex = @"__gCrWeb.findInPage.incrementIndex()";
NSString* kJavaScriptDecrementIndex = @"__gCrWeb.findInPage.decrementIndex()";
NSString* kJavaScriptGoNext = @"__gCrWeb.findInPage.goNext()";
NSString* kJavaScriptGoPrev = @"__gCrWeb.findInPage.goPrev()";

// JavaScript variables accessed by the tests.
NSString* kJavaScriptIndex = @"__gCrWeb.findInPage.selectedMatchIndex";
NSString* kJavaScriptSpansLength = @"__gCrWeb.findInPage.matches.length";

// HTML that contains several occurrences of the string 'foo', some visible and
// some not visible (the first 'foo' is hidden, the next is visible, the next is
// hidden and so on until the final 'foo' which is hidden.
NSString* kHtmlWithFoos = @"<html><body>"
                           "  <span style='display:none'>foo</span>"
                           "  <span>foo</span>"
                           "  <span style='display:none'>foo</span>"
                           "  <span>foo</span>"
                           "  <span style='display:none'>foo</span>"
                           "  <span>foo</span>"
                           "  <span style='display:none'>foo</span>"
                           "</body></html>";

// The number of times 'foo' occurs in |kHtmlWithFoos| (hidden and visible).
const int kNumberOfFoosInHtml = 7;

// HTML that contains several occurrences of the string 'foo', none visible.
NSString* kHtmlWithNoVisibleFoos = @"<html><body>"
                                    "  <span style='display:none'>foo</span>"
                                    "  <span style='display:none'>foo</span>"
                                    "  <span style='display:none'>foo</span>"
                                    "  <span style='display:none'>foo</span>"
                                    "  <span style='display:none'>foo</span>"
                                    "  <span style='display:none'>foo</span>"
                                    "</body></html>";

// Test fixture to test Find In Page JS.
class FindInPageJsTest : public ChromeWebTest {
 public:
  // Loads the given HTML, then loads the |findInPage| JavaScript.
  void LoadHtml(NSString* html) {
    ChromeWebTest::LoadHtml(html);

    // Inject and initialize the find in page javascript.
    [findInPageJsManager_ inject];
    CGRect frame = [web_state()->GetWebViewProxy() bounds];
    [findInPageJsManager_ setWidth:frame.size.width height:frame.size.height];
  }

  // Runs the given JavaScript and asserts that the result matches the given
  // |expected_value|.
  void AssertJavaScriptValue(NSString* script, int expected_value) {
    id result = ExecuteJavaScript(script);
    EXPECT_TRUE(result) << " in script: " << base::SysNSStringToUTF8(script);
    EXPECT_EQ(expected_value, [result intValue])
        << " in script: " << base::SysNSStringToUTF8(script);
  }

  // Loads the test HTML containing 'foo' strings and invokes the JavaScript
  // necessary to search for and highlight any matches. Note that the JavaScript
  // sets the current index to the first visible occurrence of 'foo'.
  void SearchForFoo() {
    LoadHtml(kHtmlWithFoos);

    // Assert the index and span count contain their initialized values
    AssertJavaScriptValue(kJavaScriptIndex, -1);
    AssertJavaScriptValue(kJavaScriptSpansLength, 0);

    // Search for 'foo'. Performing the search sets the index to point to the
    // first visible occurrence of 'foo'.
    ExecuteJavaScript(
        [NSString stringWithFormat:kJavaScriptSearchCallFormat, @"foo"]);
    AssertJavaScriptValue(kJavaScriptIndex, 1);
    AssertJavaScriptValue(kJavaScriptSpansLength, kNumberOfFoosInHtml);
  }

  void SetUp() override {
    ChromeWebTest::SetUp();
    findInPageModel_ = [[FindInPageModel alloc] init];
    findInPageJsManager_ = base::mac::ObjCCastStrict<JsFindinpageManager>(
        [web_state()->GetJSInjectionReceiver()
            instanceOfClass:[JsFindinpageManager class]]);
    findInPageJsManager_.findInPageModel = findInPageModel_;
  }

  FindInPageModel* findInPageModel_;
  JsFindinpageManager* findInPageJsManager_;
};

// Performs a search, then calls |incrementIndex| to loop through the
// matches, ensuring that when the end is reached the index wraps back to zero.
TEST_F(FindInPageJsTest, IncrementIndex) {
  SearchForFoo();

  // Increment index until it hits the max index.
  for (int i = 2; i < kNumberOfFoosInHtml; i++) {
    ExecuteJavaScript(kJavaScriptIncrementIndex);
    AssertJavaScriptValue(kJavaScriptIndex, i);
  }

  // Increment index one more time and it should wrap back to zero.
  ExecuteJavaScript(kJavaScriptIncrementIndex);
  AssertJavaScriptValue(kJavaScriptIndex, 0);
}

// Performs a search, then calls |decrementIndex| to loop through the
// matches, ensuring that when the beginning is reached the index wraps back to
// the end of the page.
TEST_F(FindInPageJsTest, DecrementIndex) {
  SearchForFoo();

  // Since the first visible 'foo' is at index 1, decrement once to get to zero.
  ExecuteJavaScript(kJavaScriptDecrementIndex);
  AssertJavaScriptValue(kJavaScriptIndex, 0);

  // Decrement index until it hits zero again.  Note that the first time
  // |decrementIndex| is called the index wraps from zero to the max index.
  for (int i = kNumberOfFoosInHtml - 1; i >= 0; i--) {
    ExecuteJavaScript(kJavaScriptDecrementIndex);
    AssertJavaScriptValue(kJavaScriptIndex, i);
  }
}

// Performs a search, then calls |goNext| to loop through the visible matches,
// ensuring that hidden matches are skipped and that when the end is reached the
// index wraps back to the beginning of the page.
TEST_F(FindInPageJsTest, GoNext) {
  SearchForFoo();

  // Since the first visible 'foo' is at index 1, and every other 'foo' is
  // hidden, after calling goNext the index should be at 3.
  ExecuteJavaScript(kJavaScriptGoNext);
  AssertJavaScriptValue(kJavaScriptIndex, 3);

  // The next visible 'foo' is at index 5.
  ExecuteJavaScript(kJavaScriptGoNext);
  AssertJavaScriptValue(kJavaScriptIndex, 5);

  // Calling |goNext| again wraps around to the first visible foo.
  ExecuteJavaScript(kJavaScriptGoNext);
  AssertJavaScriptValue(kJavaScriptIndex, 1);
}

// Performs a search, then calls |goPrev| to loop through the visible matches,
// ensuring that hidden matches are skipped and that when the beginning is
// reached the index wraps back to the end of the page.
TEST_F(FindInPageJsTest, GoPrev) {
  SearchForFoo();

  // Calling |goPrev| will wrap around to the end of the page, and since the
  // last 'foo' is hidden, we want |kNumberOfFoosInHtml| - 2.
  ExecuteJavaScript(kJavaScriptGoPrev);
  AssertJavaScriptValue(kJavaScriptIndex, 5);

  // Since every other 'foo' is hidden, the prior visible 'foo' is at index 3.
  ExecuteJavaScript(kJavaScriptGoPrev);
  AssertJavaScriptValue(kJavaScriptIndex, 3);
}

TEST_F(FindInPageJsTest, NoneVisible) {
  LoadHtml(kHtmlWithNoVisibleFoos);

  // Assert the index and span count contain their initialized values
  AssertJavaScriptValue(kJavaScriptIndex, -1);
  AssertJavaScriptValue(kJavaScriptSpansLength, 0);

  // Search for 'foo'. Performing the search sets the index to point to 0 since
  // there are no visible occurrences of 'foo'.
  ExecuteJavaScript(
      [NSString stringWithFormat:kJavaScriptSearchCallFormat, @"foo"]);
  AssertJavaScriptValue(kJavaScriptIndex, 0);
  AssertJavaScriptValue(kJavaScriptSpansLength, 6);

  ExecuteJavaScript(kJavaScriptGoPrev);
  AssertJavaScriptValue(kJavaScriptIndex, 0);

  ExecuteJavaScript(kJavaScriptGoNext);
  AssertJavaScriptValue(kJavaScriptIndex, 0);
}

TEST_F(FindInPageJsTest, SearchForNonAscii) {
  NSString* const kNonAscii = @"á";
  NSString* const htmlFormat = @"<html>"
                                "<meta charset=\"UTF-8\">"
                                "<body>%@</body>"
                                "</html>";
  LoadHtml([NSString stringWithFormat:htmlFormat, kNonAscii]);
  // Assert the index and span count contain their initialized values.
  AssertJavaScriptValue(kJavaScriptIndex, -1);
  AssertJavaScriptValue(kJavaScriptSpansLength, 0);

  // Search for the non-Ascii value. Performing the search sets the index to
  // point to the first visible occurrence of the non-Ascii.
  NSString* result = ExecuteJavaScript(
      [NSString stringWithFormat:kJavaScriptSearchCallFormat, kNonAscii]);
  ASSERT_TRUE(result);
  AssertJavaScriptValue(kJavaScriptIndex, 0);
  AssertJavaScriptValue(kJavaScriptSpansLength, 1);
}

TEST_F(FindInPageJsTest, SearchForWhitespace) {
  LoadHtml(@"<html><body> <div> </div> <h1> </h1><p> <span> </span> </p> "
           @"</body></html>");
  // Assert the index and span count contain their initialized values.
  AssertJavaScriptValue(kJavaScriptIndex, -1);
  AssertJavaScriptValue(kJavaScriptSpansLength, 0);

  // Search for space. Performing the search sets the index to
  // point to the first visible occurrence of the whitespace.
  NSString* result = ExecuteJavaScript(
      [NSString stringWithFormat:kJavaScriptSearchCallFormat, @" "]);
  ASSERT_TRUE(result);
  AssertJavaScriptValue(kJavaScriptIndex, 0);
  AssertJavaScriptValue(kJavaScriptSpansLength, 8);
}

// Tests that FindInPage works when match results cover mutiple HTML Nodes.
TEST_F(FindInPageJsTest, SearchOverMultipleNodes) {
  LoadHtml(@"<html><body>"
           @"<p>xx1<span>2</span>3<a>4512345xxx12</a>34<a>5xxx12345xx</p>"
           @"</body></html>");
  // Assert the index and span count contain their initialized values.
  AssertJavaScriptValue(kJavaScriptIndex, -1);
  AssertJavaScriptValue(kJavaScriptSpansLength, 0);

  // Search for "12345". Performing the search sets the index to
  // point to the first visible occurrence of "12345".
  NSString* result = ExecuteJavaScript(
      [NSString stringWithFormat:kJavaScriptSearchCallFormat, @"12345"]);
  ASSERT_TRUE(result);
  AssertJavaScriptValue(kJavaScriptIndex, 0);
  AssertJavaScriptValue(kJavaScriptSpansLength, 4);
}

}  // namespace
