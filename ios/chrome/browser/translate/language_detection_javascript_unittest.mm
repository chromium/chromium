// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <string.h>

#import <memory>
#import <vector>

#import "base/functional/bind.h"
#import "base/test/ios/wait_util.h"
#import "base/values.h"
#import "components/language/ios/browser/ios_language_detection_tab_helper.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "components/translate/core/browser/translate_pref_names.h"
#import "components/translate/core/common/language_detection_details.h"
#import "components/translate/core/language_detection/language_detection_model.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/web/chrome_web_client.h"
#import "ios/chrome/common/string_util.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForJSCompletionTimeout;

namespace {

const char kExpectedLanguage[] = "Foo";

// Returns an NSString filled with the char 'a' of length `length`.
NSString* GetLongString(NSUInteger length) {
  NSMutableData* data = [[NSMutableData alloc] initWithLength:length];
  memset([data mutableBytes], 'a', length);
  NSString* long_string = [[NSString alloc] initWithData:data
                                                encoding:NSASCIIStringEncoding];
  return long_string;
}

// A fake observer to track LanguageDetectionDetails received in
// LanguageDetectionTabHelper::Observer.
class FakeLanguageDetectionTabHelperObserver
    : public language::IOSLanguageDetectionTabHelper::Observer {
 public:
  FakeLanguageDetectionTabHelperObserver() {}
  ~FakeLanguageDetectionTabHelperObserver() override {}

  FakeLanguageDetectionTabHelperObserver(
      const FakeLanguageDetectionTabHelperObserver&) = delete;
  FakeLanguageDetectionTabHelperObserver& operator=(
      const FakeLanguageDetectionTabHelperObserver&) = delete;

  // Returns the received LanguageDetectionDetails objects received from
  // language::IOSLanguageDetectionTabHelper::Observer::OnLanguageDetermined
  const std::vector<translate::LanguageDetectionDetails> received_details() {
    return received_details_;
  }

  // Removes all items, if any, from `received_details`.
  void ClearReceivedDetails() { received_details_.clear(); }

 private:
  // language::IOSLanguageDetectionTabHelper::Observer::OnLanguageDetermined:
  void OnLanguageDetermined(
      const translate::LanguageDetectionDetails& details) override {
    received_details_.push_back(details);
  }
  void IOSLanguageDetectionTabHelperWasDestroyed(
      language::IOSLanguageDetectionTabHelper* tab_helper) override {}

  std::vector<translate::LanguageDetectionDetails> received_details_;
};

}  // namespace

// Test fixture to test language detection.
class JsLanguageDetectionManagerTest : public PlatformTest {
 protected:
  JsLanguageDetectionManagerTest()
      : web_client_(std::make_unique<ChromeWebClient>()) {
    browser_state_ = TestChromeBrowserState::Builder().Build();

    web::WebState::CreateParams params(browser_state_.get());
    web_state_ = web::WebState::Create(params);
    web_state_->GetView();
    web_state_->SetKeepRenderProcessAlive(true);
  }
  ~JsLanguageDetectionManagerTest() override {}

  // Injects JS, waits for the completion handler and verifies if the result
  // was what was expected.
  void InjectJsAndVerify(NSString* js, id expected_result) {
    EXPECT_NSEQ(expected_result, web::test::ExecuteJavaScript(js, web_state()));
  }

  // Injects JS, and spins the run loop until `condition` block returns true
  void InjectJSAndWaitUntilCondition(NSString* js, ConditionBlock condition) {
    web::test::ExecuteJavaScript(js, web_state());
    base::test::ios::WaitUntilCondition(^bool() {
      return condition();
    });
  }

  // Verifies if the notranslate meta tag is present or not on the page based on
  // `expected_value`.
  void ExpectHasNoTranslate(BOOL expected_value) {
    InjectJsAndVerify(@"__gCrWeb.languageDetection.hasNoTranslate();",
                      @(expected_value));
  }

  // Verifies if `lang` attribute of the HTML tag is the `expected_html_lang`,
  void ExpectHtmlLang(NSString* expected_html_lang) {
    InjectJsAndVerify(@"document.documentElement.lang;", expected_html_lang);
  }

  // Verifies if the value of the `Content-Language` meta tag is the same as
  // `expected_http_content_language`.
  void ExpectHttpContentLanguage(NSString* expected_http_content_language) {
    NSString* const kMetaTagContentJS =
        @"__gCrWeb.languageDetection.getMetaContentByHttpEquiv("
        @"'content-language');";
    InjectJsAndVerify(kMetaTagContentJS, expected_http_content_language);
  }

  // Verifies if `__gCrWeb.languageDetection.getTextContent` correctly extracts
  // the text content from an HTML page.
  void ExpectTextContent(NSString* expected_text_content) {
    NSString* script = [[NSString alloc]
        initWithFormat:
            @"__gCrWeb.languageDetection.getTextContent(document.body, %lu);",
            language::kMaxIndexChars];
    InjectJsAndVerify(script, expected_text_content);
  }

  web::WebState* web_state() { return web_state_.get(); }

  web::ScopedTestingWebClient web_client_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;
  std::unique_ptr<web::WebState> web_state_;
};

// Tests `__gCrWeb.languageDetection.hasNoTranslate` JS call.
TEST_F(JsLanguageDetectionManagerTest, PageHasNoTranslate) {
  web::test::LoadHtml(@"<html></html>", web_state());
  ExpectHasNoTranslate(NO);

  web::test::LoadHtml(@"<html><head>"
                       "<meta name='google' content='notranslate'>"
                       "</head></html>",
                      web_state());
  ExpectHasNoTranslate(YES);

  web::test::LoadHtml(@"<html><head>"
                       "<meta name='google' value='notranslate'>"
                       "</head></html>",
                      web_state());
  ExpectHasNoTranslate(YES);
}

// Tests correctness of `document.documentElement.lang` attribute.
TEST_F(JsLanguageDetectionManagerTest, HtmlLang) {
  NSString* html;
  // Non-empty attribute.
  html = [[NSString alloc]
      initWithFormat:@"<html lang='%s'></html>", kExpectedLanguage];
  web::test::LoadHtml(html, web_state());
  ExpectHtmlLang(@(kExpectedLanguage));

  // Empty attribute.
  web::test::LoadHtml(@"<html></html>", web_state());
  ExpectHtmlLang(@"");

  // Test with mixed case.
  html = [[NSString alloc]
      initWithFormat:@"<html lAnG='%s'></html>", kExpectedLanguage];
  web::test::LoadHtml(html, web_state());
  ExpectHtmlLang(@(kExpectedLanguage));
}

// Tests `__gCrWeb.languageDetection.getMetaContentByHttpEquiv` JS call.
TEST_F(JsLanguageDetectionManagerTest, HttpContentLanguage) {
  // No content language.
  web::test::LoadHtml(@"<html></html>", web_state());
  ExpectHttpContentLanguage(@"");
  NSString* html;

  // Some content language.
  html = ([[NSString alloc]
      initWithFormat:@"<html><head>"
                      "<meta http-equiv='content-language' content='%s'>"
                      "</head></html>",
                     kExpectedLanguage]);
  web::test::LoadHtml(html, web_state());
  ExpectHttpContentLanguage(@(kExpectedLanguage));

  // Test with mixed case.
  html = ([[NSString alloc]
      initWithFormat:@"<html><head>"
                      "<meta http-equiv='cOnTenT-lAngUAge' content='%s'>"
                      "</head></html>",
                     kExpectedLanguage]);
  web::test::LoadHtml(html, web_state());
  ExpectHttpContentLanguage(@(kExpectedLanguage));
}

// Tests `__gCrWeb.languageDetection.getTextContent` JS call.
TEST_F(JsLanguageDetectionManagerTest, ExtractTextContent) {
  web::test::LoadHtml(
      @"<html><body>"
       "<script>var text = 'No scripts!'</script>"
       "<p style='display: none;'>Not displayed!</p>"
       "<p style='visibility: hidden;'>Hidden!</p>"
       "<div>Some <span>text here <b>and</b></span> there.</div>"
       "</body></html>",
      web_state());

  ExpectTextContent(@"\nSome text here and there.");
}

// Tests that `__gCrWeb.languageDetection.getTextContent` correctly truncates
// text.
TEST_F(JsLanguageDetectionManagerTest, Truncation) {
  web::test::LoadHtml(
      @"<html><body>"
       "<script>var text = 'No scripts!'</script>"
       "<p style='display: none;'>Not displayed!</p>"
       "<p style='visibility: hidden;'>Hidden!</p>"
       "<div>Some <span>text here <b>and</b></span> there.</div>"
       "</body></html>",
      web_state());
  NSString* const kTextContentJS =
      @"__gCrWeb.languageDetection.getTextContent(document.body, 13)";
  InjectJsAndVerify(kTextContentJS, @"\nSome text he");
}

// HTML elements introduce a line break, except inline ones.
TEST_F(JsLanguageDetectionManagerTest, ExtractWhitespace) {
  // `b` and `span` do not break lines.
  // `br` and `div` do.
  web::test::LoadHtml(@"<html><body>"
                       "O<b>n</b>e<br>Two\tT<span>hr</span>ee<div>Four</div>"
                       "</body></html>",
                      web_state());
  ExpectTextContent(@"One\nTwo\tThree\nFour");

  // `a` does not break lines.
  // `li`, `p` and `ul` do.
  web::test::LoadHtml(
      @"<html><body>"
       "<ul><li>One</li><li>T<a href='foo'>wo</a></li></ul><p>Three</p>"
       "</body></html>",
      web_state());
  ExpectTextContent(@"\n\nOne\nTwo\nThree");
}

// Tests that `__gCrWeb.languageDetection.getTextContent` returns only until the
// kMaxIndexChars number of characters even if the text content is very large.
TEST_F(JsLanguageDetectionManagerTest, LongTextContent) {
  // Very long string.
  NSUInteger kLongStringLength = language::kMaxIndexChars - 5;
  NSMutableString* long_string = [GetLongString(kLongStringLength) mutableCopy];
  [long_string appendString:@" b cdefghijklmnopqrstuvwxyz"];

  // The string should be cut at the last whitespace, after the 'b' character.
  NSString* html = [[NSString alloc]
      initWithFormat:@"<html><body>%@</html></body>", long_string];
  web::test::LoadHtml(html, web_state());

  NSString* script = [[NSString alloc]
      initWithFormat:
          @"__gCrWeb.languageDetection.getTextContent(document.body, %lu);",
          language::kMaxIndexChars];
  NSString* result = web::test::ExecuteJavaScript(script, web_state());
  EXPECT_EQ(language::kMaxIndexChars, [result length]);
}

// Tests if `__gCrWeb.languageDetection.retrieveBufferedTextContent` correctly
// retrieves the cache and then purges it.
TEST_F(JsLanguageDetectionManagerTest, RetrieveBufferedTextContent) {
  web::test::LoadHtml(@"<html></html>", web_state());
  // Set some cached text content.
  web::test::ExecuteJavaScript(
      @"__gCrWeb.languageDetection.bufferedTextContent = 'foo'", web_state());
  web::test::ExecuteJavaScript(@"__gCrWeb.languageDetection.activeRequests = 1",
                               web_state());
  NSString* const kRetrieveBufferedTextContentJS =
      @"__gCrWeb.languageDetection.retrieveBufferedTextContent()";
  InjectJsAndVerify(kRetrieveBufferedTextContentJS, @"foo");

  // Verify cache is purged.
  InjectJsAndVerify(@"__gCrWeb.languageDetection.bufferedTextContent",
                    [NSNull null]);
}

// Test fixture to test `__gCrWeb.languageDetection.detectLanguage`.
class JsLanguageDetectionManagerDetectLanguageTest
    : public JsLanguageDetectionManagerTest {
 public:
  void SetUp() override {
    JsLanguageDetectionManagerTest::SetUp();

    pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    pref_service_->registry()->RegisterBooleanPref(
        translate::prefs::kOfferTranslateEnabled, true);
    language::IOSLanguageDetectionTabHelper::CreateForWebState(
        web_state(), /*url_language_histogram=*/nullptr, &model_,
        pref_service_.get());

    language::IOSLanguageDetectionTabHelper::FromWebState(web_state())
        ->AddObserver(&observer_);
  }

  void TearDown() override {
    language::IOSLanguageDetectionTabHelper::FromWebState(web_state())
        ->RemoveObserver(&observer_);
    JsLanguageDetectionManagerTest::TearDown();
  }

 protected:
  translate::LanguageDetectionModel model_;
  FakeLanguageDetectionTabHelperObserver observer_;
};

// Tests if `__gCrWeb.languageDetection.hasNoTranslate` correctly informs the
// native side when the notranslate meta tag is specified.
TEST_F(JsLanguageDetectionManagerDetectLanguageTest,
       DetectLanguageWithNoTranslateMeta) {
  // A simple page using the notranslate meta tag.
  NSString* html = @"<html><head>"
                   @"<meta http-equiv='content-language' content='en'>"
                   @"<meta name='google' content='notranslate'>"
                   @"</head></html>";
  web::test::LoadHtml(html, web_state());
  web::test::ExecuteJavaScript(@"__gCrWeb.languageDetection.detectLanguage()",
                               web_state());

  // Wait until the original injection has received a command.
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      kWaitForJSCompletionTimeout, ^bool() {
        return !observer_.received_details().empty();
      }));

  ASSERT_EQ(1U, observer_.received_details().size());

  observer_.ClearReceivedDetails();

  web::test::ExecuteJavaScript(@"__gCrWeb.languageDetection.detectLanguage()",
                               web_state());
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      kWaitForJSCompletionTimeout, ^bool() {
        return !observer_.received_details().empty();
      }));
  ASSERT_EQ(1U, observer_.received_details().size());
  translate::LanguageDetectionDetails details = observer_.received_details()[0];
  ASSERT_TRUE(details.has_notranslate);
}

// Tests if `__gCrWeb.languageDetection.detectLanguage` correctly informs the
// native side when no notranslate meta tag is specified.
TEST_F(JsLanguageDetectionManagerDetectLanguageTest,
       DetectLanguageWithoutNoTranslateMeta) {
  // A simple page with no notranslate meta tag.
  NSString* html = @"<html><head>"
                   @"<meta http-equiv='content-language' content='en'>"
                   @"</head></html>";
  web::test::LoadHtml(html, web_state());
  web::test::ExecuteJavaScript(@"__gCrWeb.languageDetection.detectLanguage()",
                               web_state());
  // Wait until the original injection has received a command.
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      kWaitForJSCompletionTimeout, ^bool() {
        return !observer_.received_details().empty();
      }));

  observer_.ClearReceivedDetails();

  web::test::ExecuteJavaScript(@"__gCrWeb.languageDetection.detectLanguage()",
                               web_state());
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      kWaitForJSCompletionTimeout, ^bool() {
        return !observer_.received_details().empty();
      }));
  ASSERT_EQ(1U, observer_.received_details().size());
  translate::LanguageDetectionDetails details = observer_.received_details()[0];
  ASSERT_FALSE(details.has_notranslate);
}
