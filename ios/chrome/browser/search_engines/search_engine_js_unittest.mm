// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/public/test/web_js_test.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"
#import "ios/web/public/web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForJSCompletionTimeout;
using web::test::TapWebViewElementWithId;
using web::test::SelectWebViewElementWithId;

namespace {
const char kCommandPrefix[] = "searchEngine";
const char kCommandOpenSearch[] = "searchEngine.openSearch";
const char kOpenSearchPageUrlKey[] = "pageUrl";
const char kOpenSearchOsddUrlKey[] = "osddUrl";
const char kCommandSearchableUrl[] = "searchEngine.searchableUrl";
const char kSearchableUrlUrlKey[] = "url";
// This is for cases where no message should be sent back from Js.
const NSTimeInterval kWaitForJsNotReturnTimeout = 0.5;

NSString* kSearchableForm =
    @"<html>"
    @"  <form id='f' action='index.html' method='get'>"
    @"    <input type='search' name='q'>"
    @"    <input type='hidden' name='hidden' value='i1'>"
    @"    <input type='hidden' name='disabled' value='i2' disabled>"
    @"    <input id='r1' type='radio' name='radio' value='r1' checked>"
    @"    <input id='r2' type='radio' name='radio' value='r2'>"
    @"    <input id='c1' type='checkbox' name='check' value='c1'>"
    @"    <input id='c2' type='checkbox' name='check' value='c2' checked>"
    @"    <select name='select' name='select'>"
    @"      <option id='op1' value='op1'>op1</option>"
    @"      <option id='op2' value='op2' selected>op2</option>"
    @"      <option id='op3' value='op3'>op3</option>"
    @"    </select>"
    @"    <input id='btn1' type='submit' name='btn1' value='b1'>"
    @"    <button id='btn2' name='btn2' value='b2'>"
    @"  </form>"
    @"  <input type='hidden' form='f' name='outside form' value='i3'>"
    @"</html>";
}

// Test fixture for search_engine.js testing.
class SearchEngineJsTest : public web::WebJsTest<web::WebTestWithWebState> {
 protected:
  SearchEngineJsTest()
      : web::WebJsTest<web::WebTestWithWebState>(@[ @"search_engine" ]) {}

  void SetUp() override {
    WebTestWithWebState::SetUp();
    subscription_ = web_state()->AddScriptCommandCallback(
        base::BindRepeating(&SearchEngineJsTest::OnMessageFromJavaScript,
                            base::Unretained(this)),
        kCommandPrefix);
  }

  void TearDown() override {
    WebTestWithWebState::TearDown();
  }

  void OnMessageFromJavaScript(const base::DictionaryValue& message,
                               const GURL& page_url,
                               bool user_is_interacting,
                               web::WebFrame* sender_frame) {
    message_received_ = true;
    message_ = message.Clone();
  }

  base::Value message_;
  bool message_received_ = false;

  // Subscription for JS message.
  std::unique_ptr<web::WebState::ScriptCommandSubscription> subscription_;

  DISALLOW_COPY_AND_ASSIGN(SearchEngineJsTest);
};

// Tests that if a OSDD <link> is found in page, __gCrWeb.searchEngine will
// send a message containing the page's URL and OSDD's URL.
TEST_F(SearchEngineJsTest, TestGetOpenSearchDescriptionDocumentUrlSucceed) {
  LoadHtmlAndInject(
      @"<html><link rel='search' type='application/opensearchdescription+xml' "
      @"title='Chromium Code Search' "
      @"href='//cs.chromium.org/codesearch/first_opensearch.xml' />"
      @"<link rel='search' type='application/opensearchdescription+xml' "
      @"title='Chromium Code Search 2' "
      @"href='//cs.chromium.org/codesearch/second_opensearch.xml' />"
      @"<link href='/favicon.ico' rel='shortcut icon' "
      @"type='image/x-icon'></html>",
      GURL("https://cs.chromium.org"));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return message_received_;
  }));
  const base::Value* cmd = message_.FindKey("command");
  ASSERT_TRUE(cmd);
  ASSERT_TRUE(cmd->is_string());
  EXPECT_EQ(kCommandOpenSearch, cmd->GetString());
  const base::Value* page_url = message_.FindKey(kOpenSearchPageUrlKey);
  ASSERT_TRUE(page_url);
  ASSERT_TRUE(page_url->is_string());
  EXPECT_EQ("https://cs.chromium.org/", page_url->GetString());
  const base::Value* osdd_url = message_.FindKey(kOpenSearchOsddUrlKey);
  ASSERT_TRUE(osdd_url);
  ASSERT_TRUE(osdd_url->is_string());
  EXPECT_EQ("https://cs.chromium.org/codesearch/first_opensearch.xml",
            osdd_url->GetString());
}

// Tests that if no OSDD <link> is found in page, __gCrWeb.searchEngine will
// not send a message about OSDD.
TEST_F(SearchEngineJsTest, TestGetOpenSearchDescriptionDocumentUrlFail) {
  LoadHtmlAndInject(
      @"<html><link href='/favicon.ico' rel='shortcut icon' "
      @"type='image/x-icon'></html>",
      GURL("https://cs.chromium.org"));
  ASSERT_FALSE(WaitUntilConditionOrTimeout(kWaitForJsNotReturnTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return message_received_;
  }));
}

// Tests that __gCrWeb.searchEngine generates and sends back a searchable
// URL when <form> is submitted by click on the first button in <form>.
TEST_F(SearchEngineJsTest,
       GenerateSearchableUrlForValidFormSubmittedByFirstButton) {
  LoadHtmlAndInject(kSearchableForm, GURL("https://abc.com"));
  ASSERT_TRUE(TapWebViewElementWithId(web_state(), "btn1"));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return message_received_;
  }));
  const base::Value* cmd = message_.FindKey("command");
  ASSERT_TRUE(cmd);
  ASSERT_TRUE(cmd->is_string());
  EXPECT_EQ(kCommandSearchableUrl, cmd->GetString());
  const base::Value* url = message_.FindKey(kSearchableUrlUrlKey);
  ASSERT_TRUE(url);
  ASSERT_TRUE(url->is_string());
  EXPECT_EQ(
      "https://abc.com/"
      "index.html?q={searchTerms}&hidden=i1&radio=r1&check=c2&select=op2&btn1="
      "b1&outside+form=i3",
      url->GetString());
}

// Tests that __gCrWeb.searchEngine generates and sends back a searchable
// URL when <form> is submitted by click on a non-first button in <form>.
TEST_F(SearchEngineJsTest,
       GenerateSearchableUrlForValidFormSubmittedByNonFirstButton) {
  LoadHtmlAndInject(kSearchableForm, GURL("https://abc.com"));
  ASSERT_TRUE(TapWebViewElementWithId(web_state(), "btn2"));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return message_received_;
  }));
  const base::Value* url = message_.FindKey("url");
  ASSERT_TRUE(url);
  ASSERT_TRUE(url->is_string());
  EXPECT_EQ(
      "https://abc.com/"
      "index.html?q={searchTerms}&hidden=i1&radio=r1&check=c2&select=op2&btn2="
      "b2&outside+form=i3",
      url->GetString());
}

// Tests that __gCrWeb.searchEngine doesn't generate and send back a searchable
// URL for <form> with <textarea>.
TEST_F(SearchEngineJsTest, GenerateSearchableUrlForInvalidFormWithTextArea) {
  LoadHtmlAndInject(
      @"<html><form><input type='search' name='q'><textarea "
      @"name='a'></textarea><input id='btn' type='submit'></form></html>");
  ASSERT_TRUE(TapWebViewElementWithId(web_state(), "btn"));
  ASSERT_FALSE(WaitUntilConditionOrTimeout(kWaitForJsNotReturnTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return message_received_;
  }));
}

// Tests that __gCrWeb.searchEngine doesn't generate and send back a searchable
// URL for <form> with <input type="password">.
TEST_F(SearchEngineJsTest,
       GenerateSearchableUrlForInvalidFormWithInputPassword) {
  LoadHtmlAndInject(
      @"<html><form><input type='search' name='q'><input "
      @"type='password' name='a'><input id='btn' type='submit'></form></html>");
  ASSERT_TRUE(TapWebViewElementWithId(web_state(), "btn"));
  ASSERT_FALSE(WaitUntilConditionOrTimeout(kWaitForJsNotReturnTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return message_received_;
  }));
}

// Tests that __gCrWeb.searchEngine doesn't generate and send back a searchable
// URL for <form> with <input type="file">.
TEST_F(SearchEngineJsTest, GenerateSearchableUrlForInvalidFormWithInputFile) {
  LoadHtmlAndInject(
      @"<html><form><input type='search' name='q'><input "
      @"type='file' name='a'><input id='btn' type='submit'</form></html>");
  ASSERT_TRUE(TapWebViewElementWithId(web_state(), "btn"));
  ASSERT_FALSE(WaitUntilConditionOrTimeout(kWaitForJsNotReturnTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return message_received_;
  }));
}

// Tests that __gCrWeb.searchEngine doesn't generate and send back a searchable
// URL for <form> without <input type="email|search|tel|text|url|number">.
TEST_F(SearchEngineJsTest, GenerateSearchableUrlForInvalidFormWithNoTextInput) {
  LoadHtmlAndInject(
      @"<html><form id='f'><input type='hidden' name='q' "
      @"value='v'><input id='btn' type='submit'></form></html>");
  ASSERT_TRUE(TapWebViewElementWithId(web_state(), "btn"));
  ASSERT_FALSE(WaitUntilConditionOrTimeout(kWaitForJsNotReturnTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return message_received_;
  }));
}

// Tests that __gCrWeb.searchEngine doesn't generate and send back a searchable
// URL for <form> with more than 1 <input
// type="email|search|tel|text|url|number">.
TEST_F(SearchEngineJsTest,
       GenerateSearchableUrlForInvalidFormWithMoreThanOneTextInput) {
  LoadHtmlAndInject(
      @"<html><form id='f'><input type='search' name='q'><input "
      @"type='text' name='q2'><input id='btn' type='submit'></form></html>");
  ASSERT_TRUE(TapWebViewElementWithId(web_state(), "btn"));
  ASSERT_FALSE(WaitUntilConditionOrTimeout(kWaitForJsNotReturnTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return message_received_;
  }));
}

// Tests that __gCrWeb.searchEngine doesn't generate and send back a searchable
// URL for <form> with <input type='radio'> in non-default state.
TEST_F(SearchEngineJsTest,
       GenerateSearchableUrlForInvalidFormWithNonDefaultRadio) {
  LoadHtmlAndInject(kSearchableForm);
  ASSERT_TRUE(TapWebViewElementWithId(web_state(), "r2"));
  ASSERT_TRUE(TapWebViewElementWithId(web_state(), "btn1"));
  ASSERT_FALSE(WaitUntilConditionOrTimeout(kWaitForJsNotReturnTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return message_received_;
  }));
}

// Tests that __gCrWeb.searchEngine doesn't generate and send back a searchable
// URL for <form> with <input type='checkbox'> in non-default state.
TEST_F(SearchEngineJsTest,
       GenerateSearchableUrlForInvalidFormWithNonDefaultCheckbox) {
  LoadHtmlAndInject(kSearchableForm);
  ASSERT_TRUE(TapWebViewElementWithId(web_state(), "c1"));
  ASSERT_TRUE(TapWebViewElementWithId(web_state(), "btn1"));
  ASSERT_FALSE(WaitUntilConditionOrTimeout(kWaitForJsNotReturnTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return message_received_;
  }));
}

// Tests that __gCrWeb.searchEngine.generateSearchableUrl returns undefined
// for <form> with <select> in non-default state.
TEST_F(SearchEngineJsTest,
       GenerateSearchableUrlForInvalidFormWithNonDefaultSelect) {
  LoadHtmlAndInject(kSearchableForm);
  ASSERT_TRUE(SelectWebViewElementWithId(web_state(), "op1"));
  ASSERT_TRUE(TapWebViewElementWithId(web_state(), "btn1"));
  ASSERT_FALSE(WaitUntilConditionOrTimeout(kWaitForJsNotReturnTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return message_received_;
  }));
}
