// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include <memory>
#include <vector>

#include "base/bind.h"
#import "base/test/ios/wait_util.h"
#include "base/values.h"
#import "components/translate/ios/browser/language_detection_controller.h"
#import "ios/chrome/browser/web/chrome_web_client.h"
#import "ios/chrome/browser/web/chrome_web_test.h"
#include "ios/chrome/common/string_util.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const char kExpectedLanguage[] = "Foo";

// Returns an NSString filled with the char 'a' of length |length|.
NSString* GetLongString(NSUInteger length) {
  NSMutableData* data = [[NSMutableData alloc] initWithLength:length];
  memset([data mutableBytes], 'a', length);
  NSString* long_string = [[NSString alloc] initWithData:data
                                                encoding:NSASCIIStringEncoding];
  return long_string;
}

}  // namespace

// Test fixture to test language detection.
class JsLanguageDetectionManagerTest : public ChromeWebTest {
 protected:
  JsLanguageDetectionManagerTest()
      : ChromeWebTest(std::make_unique<ChromeWebClient>()) {}
  ~JsLanguageDetectionManagerTest() override {}

  // Injects JS, waits for the completion handler and verifies if the result
  // was what was expected.
  void InjectJsAndVerify(NSString* js, id expected_result) {
    EXPECT_NSEQ(expected_result, ExecuteJavaScript(js));
  }

  // Injects JS, and spins the run loop until |condition| block returns true
  void InjectJSAndWaitUntilCondition(NSString* js, ConditionBlock condition) {
    ExecuteJavaScript(js);
    base::test::ios::WaitUntilCondition(^bool() {
      return condition();
    });
  }

  // Verifies if the notranslate meta tag is present or not on the page based on
  // |expected_value|.
  void ExpectHasNoTranslate(BOOL expected_value) {
    InjectJsAndVerify(@"__gCrWeb.languageDetection.hasNoTranslate();",
                      @(expected_value));
  }

  // Verifies if |lang| attribute of the HTML tag is the |expected_html_lang|,
  void ExpectHtmlLang(NSString* expected_html_lang) {
    InjectJsAndVerify(@"document.documentElement.lang;", expected_html_lang);
  }

  // Verifies if the value of the |Content-Language| meta tag is the same as
  // |expected_http_content_language|.
  void ExpectHttpContentLanguage(NSString* expected_http_content_language) {
    NSString* const kMetaTagContentJS =
        @"__gCrWeb.languageDetection.getMetaContentByHttpEquiv("
        @"'content-language');";
    InjectJsAndVerify(kMetaTagContentJS, expected_http_content_language);
  }

  // Verifies if |__gCrWeb.languageDetection.getTextContent| correctly extracts
  // the text content from an HTML page.
  void ExpectTextContent(NSString* expected_text_content) {
    NSString* script = [[NSString alloc]
        initWithFormat:
            @"__gCrWeb.languageDetection.getTextContent(document.body, %lu);",
            translate::kMaxIndexChars];
    InjectJsAndVerify(script, expected_text_content);
  }
};

// Tests |__gCrWeb.languageDetection.hasNoTranslate| JS call.
TEST_F(JsLanguageDetectionManagerTest, PageHasNoTranslate) {
  LoadHtml(@"<html></html>");
  ExpectHasNoTranslate(NO);

  LoadHtml(@"<html><head>"
            "<meta name='google' content='notranslate'>"
            "</head></html>");
  ExpectHasNoTranslate(YES);

  LoadHtml(@"<html><head>"
            "<meta name='google' value='notranslate'>"
            "</head></html>");
  ExpectHasNoTranslate(YES);
}

// Tests correctness of |document.documentElement.lang| attribute.
TEST_F(JsLanguageDetectionManagerTest, HtmlLang) {
  NSString* html;
  // Non-empty attribute.
  html = [[NSString alloc]
      initWithFormat:@"<html lang='%s'></html>", kExpectedLanguage];
  LoadHtml(html);
  ExpectHtmlLang(@(kExpectedLanguage));

  // Empty attribute.
  LoadHtml(@"<html></html>");
  ExpectHtmlLang(@"");

  // Test with mixed case.
  html = [[NSString alloc]
      initWithFormat:@"<html lAnG='%s'></html>", kExpectedLanguage];
  LoadHtml(html);
  ExpectHtmlLang(@(kExpectedLanguage));
}

// Tests |__gCrWeb.languageDetection.getMetaContentByHttpEquiv| JS call.
TEST_F(JsLanguageDetectionManagerTest, HttpContentLanguage) {
  // No content language.
  LoadHtml(@"<html></html>");
  ExpectHttpContentLanguage(@"");
  NSString* html;

  // Some content language.
  html = ([[NSString alloc]
      initWithFormat:@"<html><head>"
                      "<meta http-equiv='content-language' content='%s'>"
                      "</head></html>",
                     kExpectedLanguage]);
  LoadHtml(html);
  ExpectHttpContentLanguage(@(kExpectedLanguage));

  // Test with mixed case.
  html = ([[NSString alloc]
      initWithFormat:@"<html><head>"
                      "<meta http-equiv='cOnTenT-lAngUAge' content='%s'>"
                      "</head></html>",
                     kExpectedLanguage]);
  LoadHtml(html);
  ExpectHttpContentLanguage(@(kExpectedLanguage));
}

// Tests |__gCrWeb.languageDetection.getTextContent| JS call.
TEST_F(JsLanguageDetectionManagerTest, ExtractTextContent) {
  LoadHtml(@"<html><body>"
            "<script>var text = 'No scripts!'</script>"
            "<p style='display: none;'>Not displayed!</p>"
            "<p style='visibility: hidden;'>Hidden!</p>"
            "<div>Some <span>text here <b>and</b></span> there.</div>"
            "</body></html>");

  ExpectTextContent(@"\nSome text here and there.");
}

// Tests that |__gCrWeb.languageDetection.getTextContent| correctly truncates
// text.
TEST_F(JsLanguageDetectionManagerTest, Truncation) {
  LoadHtml(@"<html><body>"
            "<script>var text = 'No scripts!'</script>"
            "<p style='display: none;'>Not displayed!</p>"
            "<p style='visibility: hidden;'>Hidden!</p>"
            "<div>Some <span>text here <b>and</b></span> there.</div>"
            "</body></html>");
  NSString* const kTextContentJS =
      @"__gCrWeb.languageDetection.getTextContent(document.body, 13)";
  InjectJsAndVerify(kTextContentJS, @"\nSome text he");
}

// HTML elements introduce a line break, except inline ones.
TEST_F(JsLanguageDetectionManagerTest, ExtractWhitespace) {
  // |b| and |span| do not break lines.
  // |br| and |div| do.
  LoadHtml(@"<html><body>"
            "O<b>n</b>e<br>Two\tT<span>hr</span>ee<div>Four</div>"
            "</body></html>");
  ExpectTextContent(@"One\nTwo\tThree\nFour");

  // |a| does not break lines.
  // |li|, |p| and |ul| do.
  LoadHtml(@"<html><body>"
            "<ul><li>One</li><li>T<a href='foo'>wo</a></li></ul><p>Three</p>"
            "</body></html>");
  ExpectTextContent(@"\n\nOne\nTwo\nThree");
}

// Tests that |__gCrWeb.languageDetection.getTextContent| returns only until the
// kMaxIndexChars number of characters even if the text content is very large.
TEST_F(JsLanguageDetectionManagerTest, LongTextContent) {
  // Very long string.
  NSUInteger kLongStringLength = translate::kMaxIndexChars - 5;
  NSMutableString* long_string = [GetLongString(kLongStringLength) mutableCopy];
  [long_string appendString:@" b cdefghijklmnopqrstuvwxyz"];

  // The string should be cut at the last whitespace, after the 'b' character.
  NSString* html = [[NSString alloc]
      initWithFormat:@"<html><body>%@</html></body>", long_string];
  LoadHtml(html);

  NSString* script = [[NSString alloc]
      initWithFormat:
          @"__gCrWeb.languageDetection.getTextContent(document.body, %lu);",
          translate::kMaxIndexChars];
  NSString* result = ExecuteJavaScript(script);
  EXPECT_EQ(translate::kMaxIndexChars, [result length]);
}

// Tests if |__gCrWeb.languageDetection.retrieveBufferedTextContent| correctly
// retrieves the cache and then purges it.
TEST_F(JsLanguageDetectionManagerTest, RetrieveBufferedTextContent) {
  LoadHtml(@"<html></html>");
  // Set some cached text content.
  ExecuteJavaScript(@"__gCrWeb.languageDetection.bufferedTextContent = 'foo'");
  ExecuteJavaScript(@"__gCrWeb.languageDetection.activeRequests = 1");
  NSString* const kRetrieveBufferedTextContentJS =
      @"__gCrWeb.languageDetection.retrieveBufferedTextContent()";
  InjectJsAndVerify(kRetrieveBufferedTextContentJS, @"foo");

  // Verify cache is purged.
  InjectJsAndVerify(@"__gCrWeb.languageDetection.bufferedTextContent",
                    [NSNull null]);
}

// Test fixture to test |__gCrWeb.languageDetection.detectLanguage|.
class JsLanguageDetectionManagerDetectLanguageTest
    : public JsLanguageDetectionManagerTest {
 public:
  void SetUp() override {
    JsLanguageDetectionManagerTest::SetUp();
    auto callback = base::BindRepeating(
        &JsLanguageDetectionManagerDetectLanguageTest::CommandReceived,
        base::Unretained(this));
    subscription_ =
        web_state()->AddScriptCommandCallback(callback, "languageDetection");
  }
  // Called when "languageDetection" command is received.
  void CommandReceived(const base::Value& command,
                       const GURL& url,
                       bool user_is_interacting,
                       web::WebFrame* sender_frame) {
    commands_received_.push_back(command.Clone());
  }

 protected:
  // Received "languageDetection" commands.
  std::vector<base::Value> commands_received_;

  // Subscription for JS message.
  base::CallbackListSubscription subscription_;
};

// Tests if |__gCrWeb.languageDetection.hasNoTranslate| correctly informs the
// native side when the notranslate meta tag is specified.
TEST_F(JsLanguageDetectionManagerDetectLanguageTest,
       DetectLanguageWithNoTranslateMeta) {
  // A simple page using the notranslate meta tag.
  NSString* html = @"<html><head>"
                   @"<meta http-equiv='content-language' content='en'>"
                   @"<meta name='google' content='notranslate'>"
                   @"</head></html>";
  LoadHtml(html);
  ExecuteJavaScript(@"__gCrWeb.languageDetection.detectLanguage()");
  // Wait until the original injection has received a command.
  base::test::ios::WaitUntilCondition(^bool() {
    return !commands_received_.empty();
  });
  ASSERT_EQ(1U, commands_received_.size());

  commands_received_.clear();

  ExecuteJavaScript(@"__gCrWeb.languageDetection.detectLanguage()");
  base::test::ios::WaitUntilCondition(^bool() {
    return !commands_received_.empty();
  });
  ASSERT_EQ(1U, commands_received_.size());
  const base::Value& value = commands_received_[0];
  absl::optional<bool> has_notranslate = value.FindBoolKey("hasNoTranslate");
  ASSERT_TRUE(has_notranslate);
  EXPECT_TRUE(value.FindKey("captureTextTime"));
  EXPECT_TRUE(value.FindKey("htmlLang"));
  EXPECT_TRUE(value.FindKey("httpContentLanguage"));
  EXPECT_TRUE(*has_notranslate);
}

// Tests if |__gCrWeb.languageDetection.detectLanguage| correctly informs the
// native side when no notranslate meta tag is specified.
TEST_F(JsLanguageDetectionManagerDetectLanguageTest,
       DetectLanguageWithoutNoTranslateMeta) {
  // A simple page with no notranslate meta tag.
  NSString* html = @"<html><head>"
                   @"<meta http-equiv='content-language' content='en'>"
                   @"</head></html>";
  LoadHtml(html);
  ExecuteJavaScript(@"__gCrWeb.languageDetection.detectLanguage()");
  // Wait until the original injection has received a command.
  base::test::ios::WaitUntilCondition(^bool() {
    return !commands_received_.empty();
  });
  ASSERT_EQ(1U, commands_received_.size());

  commands_received_.clear();

  ExecuteJavaScript(@"__gCrWeb.languageDetection.detectLanguage()");
  base::test::ios::WaitUntilCondition(^bool() {
    return !commands_received_.empty();
  });
  ASSERT_EQ(1U, commands_received_.size());
  const base::Value& value = commands_received_[0];

  absl::optional<bool> has_notranslate = value.FindBoolKey("hasNoTranslate");

  ASSERT_TRUE(has_notranslate);
  EXPECT_TRUE(value.FindKey("captureTextTime"));
  EXPECT_TRUE(value.FindKey("htmlLang"));
  EXPECT_TRUE(value.FindKey("httpContentLanguage"));
  EXPECT_FALSE(*has_notranslate);
}
