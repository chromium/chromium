// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/suggestion_controller_java_script_feature.h"

#import <Foundation/Foundation.h>

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/test_timeouts.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/web/model/chrome_web_client.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

// Test fixture to test suggestions.
class SuggestionControllerJavaScriptFeatureTest : public PlatformTest {
 protected:
  SuggestionControllerJavaScriptFeatureTest()
      : web_client_(std::make_unique<ChromeWebClient>()) {
    profile_ = TestProfileIOS::Builder().Build();

    web::WebState::CreateParams params(profile_.get());
    web_state_ = web::WebState::Create(params);
    web_state_->GetView();
    web_state_->SetKeepRenderProcessAlive(true);
  }
  // Returns the main frame of `web_state()`'s current page.
  web::WebFrame* GetMainFrame();
  // Helper method that initializes a form with three fields. Can be used to
  // test whether adding an attribute on the second field causes it to be
  // skipped (or not, as is appropriate) by selectNextElement.
  void SequentialNavigationSkipCheck(NSString* attribute, BOOL shouldSkip);
  // Executes JavaScript in the content world associated with
  // SuggestionControllerJavaScriptFeature.
  id ExecuteJavaScript(NSString* java_script) {
    autofill::SuggestionControllerJavaScriptFeature* feature =
        autofill::SuggestionControllerJavaScriptFeature::GetInstance();
    return web::test::ExecuteJavaScriptForFeature(web_state(), java_script,
                                                  feature);
  }
  // Returns the active element name from the JS side.
  NSString* GetActiveElementName() {
    return ExecuteJavaScript(@"document.activeElement.name");
  }
  // Waits until the active element is `name`.
  BOOL WaitUntilElementSelected(NSString* name) {
    return base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForJSCompletionTimeout, ^bool {
          return [GetActiveElementName() isEqualToString:name];
        });
  }

 protected:
  web::WebState* web_state() { return web_state_.get(); }

  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  web::ScopedTestingWebClient web_client_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::WebState> web_state_;
};

web::WebFrame* SuggestionControllerJavaScriptFeatureTest::GetMainFrame() {
  autofill::SuggestionControllerJavaScriptFeature* feature =
      autofill::SuggestionControllerJavaScriptFeature::GetInstance();
  web::WebFramesManager* manager = feature->GetWebFramesManager(web_state());
  return manager->GetMainWebFrame();
}

TEST_F(SuggestionControllerJavaScriptFeatureTest, InitAndInject) {
  web::test::LoadHtml(@"<html></html>", web_state());
  EXPECT_NSEQ(@"object", ExecuteJavaScript(@"typeof __gCrWeb.suggestion"));
}

TEST_F(SuggestionControllerJavaScriptFeatureTest, SelectElementInTabOrder) {
  NSString* htmlFragment =
      @"<html> <body>"
       "<input id='1 (0)' tabIndex=1 href='http://www.w3schools.com'>1 (0)</a>"
       "<input id='0 (0)' tabIndex=0 href='http://www.w3schools.com'>0 (0)</a>"
       "<input id='2' tabIndex=2 href='http://www.w3schools.com'>2</a>"
       "<input id='0 (1)' tabIndex=0 href='http://www.w3schools.com'>0 (1)</a>"
       "<input id='-2' tabIndex=-2 href='http://www.w3schools.com'>-2</a>"
       "<a href='http://www.w3schools.com'></a>"
       "<input id='-1 (0)' tabIndex=-1 href='http://www.w3schools.com'>-1</a>"
       "<input id='-2 (2)' tabIndex=-2 href='http://www.w3schools.com'>-2</a>"
       "<input id='0 (2)' tabIndex=0 href='http://www.w3schools.com'>0 - 2</a>"
       "<input id='3' tabIndex=3 href='http://www.w3schools.com'>3</a>"
       "<input id='1 (1)' tabIndex=1 href='http://www.w3schools.com'>1 (1)</a>"
       "<input id='-1 (1)' tabIndex=-1 href='http://www.w3schools.com'>-1 </a>"
       "<input id='0 (3)' tabIndex=0 href='http://www.w3schools.com'>0 (3)</a>"
       "</body></html>";
  web::test::LoadHtml(htmlFragment, web_state());

  // clang-format off
  NSDictionary* next_expected_ids = @ {
      @"1 (0)"  : @"1 (1)",
      @"0 (0)"  : @"0 (1)",
      @"2"      : @"3",
      @"0 (1)"  : @"0 (2)",
      @"-2"     : @"0 (2)",
      @"-1 (0)" : @"0 (2)",
      @"-2 (2)" : @"0 (2)",
      @"0 (2)"  : @"0 (3)",
      @"3"      : @"0 (0)",
      @"1 (1)"  : @"2",
      @"-1 (1)" : @"0 (3)",
      @"0 (3)"  : @"null"
  };
  // clang-format on

  for (NSString* element_id : next_expected_ids) {
    NSString* expected_id = [next_expected_ids objectForKey:element_id];
    NSString* script = [NSString
        stringWithFormat:
            @"var elements=document.getElementsByTagName('input');"
             "var element=document.getElementById('%@');"
             "var next = __gCrWeb.suggestion.getNextElementInTabOrder("
             "    element, elements);"
             "next ? next.id : 'null';",
            element_id];
    EXPECT_NSEQ(expected_id, ExecuteJavaScript(script))
        << "Wrong when selecting next element of element with element id "
        << base::SysNSStringToUTF8(element_id);
  }
  EXPECT_NSEQ(@YES,
              ExecuteJavaScript(
                  @"var elements=document.getElementsByTagName('input');"
                   "var element=document.getElementsByTagName('a')[0];"
                   "var next = __gCrWeb.suggestion.getNextElementInTabOrder("
                   "    element, elements); next===null"))
      << "Wrong when selecting the next element of an element not in the "
      << "element list.";

  for (NSString* element_id : next_expected_ids) {
    NSString* expected_id = [next_expected_ids objectForKey:element_id];
    if ([expected_id isEqualToString:@"null"]) {
      // If the expected next element is null, the focus is not moved.
      expected_id = element_id;
    }
    NSString* script =
        [NSString stringWithFormat:@"document.getElementById('%@').focus();"
                                    "__gCrWeb.suggestion.selectNextElement();"
                                    "document.activeElement.id",
                                   element_id];
    EXPECT_NSEQ(expected_id, ExecuteJavaScript(script))
        << "Wrong when selecting next element with active element "
        << base::SysNSStringToUTF8(element_id);
  }

  for (NSString* element_id : next_expected_ids) {
    // If the expected next element is null, there is no next element.
    BOOL expected = ![next_expected_ids[element_id] isEqualToString:@"null"];
    NSString* script =
        [NSString stringWithFormat:@"document.getElementById('%@').focus();"
                                    "__gCrWeb.suggestion.hasNextElement()",
                                   element_id];
    EXPECT_NSEQ(@(expected), ExecuteJavaScript(script))
        << "Wrong when checking hasNextElement() for "
        << base::SysNSStringToUTF8(element_id);
  }

  // clang-format off
  NSDictionary* prev_expected_ids = @{
      @"1 (0)" : @"null",
      @"0 (0)" : @"3",
      @"2"     : @"1 (1)",
      @"0 (1)" : @"0 (0)",
      @"-2"    : @"0 (1)",
      @"-1 (0)": @"0 (1)",
      @"-2 (2)": @"0 (1)",
      @"0 (2)" : @"0 (1)",
      @"3"     : @"2",
      @"1 (1)" : @"1 (0)",
      @"-1 (1)": @"1 (1)",
      @"0 (3)" : @"0 (2)",
  };
  // clang-format on

  for (NSString* element_id : prev_expected_ids) {
    NSString* expected_id = [prev_expected_ids objectForKey:element_id];
    NSString* script = [NSString
        stringWithFormat:
            @"var elements=document.getElementsByTagName('input');"
             "var element=document.getElementById('%@');"
             "var prev = __gCrWeb.suggestion.getPreviousElementInTabOrder("
             "    element, elements);"
             "prev ? prev.id : 'null';",
            element_id];
    EXPECT_NSEQ(expected_id, ExecuteJavaScript(script))
        << "Wrong when selecting prev element of element with element id "
        << base::SysNSStringToUTF8(element_id);
  }
  EXPECT_NSEQ(
      @YES, ExecuteJavaScript(
                @"var elements=document.getElementsByTagName('input');"
                 "var element=document.getElementsByTagName('a')[0];"
                 "var prev = __gCrWeb.suggestion.getPreviousElementInTabOrder("
                 "    element, elements); prev===null"))
      << "Wrong when selecting the previous element of an element not in the "
      << "element list";

  for (NSString* element_id : prev_expected_ids) {
    NSString* expected_id = [prev_expected_ids objectForKey:element_id];
    if ([expected_id isEqualToString:@"null"]) {
      // If the expected previous element is null, the focus is not moved.
      expected_id = element_id;
    }
    NSString* script = [NSString
        stringWithFormat:@"document.getElementById('%@').focus();"
                          "__gCrWeb.suggestion.selectPreviousElement();"
                          "document.activeElement.id",
                         element_id];
    EXPECT_NSEQ(expected_id, ExecuteJavaScript(script))
        << "Wrong when selecting previous element with active element "
        << base::SysNSStringToUTF8(element_id);
  }

  for (NSString* element_id : prev_expected_ids) {
    // If the expected next element is null, there is no next element.
    BOOL expected = ![prev_expected_ids[element_id] isEqualToString:@"null"];
    NSString* script =
        [NSString stringWithFormat:@"document.getElementById('%@').focus();"
                                    "__gCrWeb.suggestion.hasPreviousElement()",
                                   element_id];
    EXPECT_NSEQ(@(expected), ExecuteJavaScript(script))
        << "Wrong when checking hasPreviousElement() for "
        << base::SysNSStringToUTF8(element_id);
  }
}

TEST_F(SuggestionControllerJavaScriptFeatureTest, SequentialNavigation) {
  web::test::LoadHtml(@"<html><body><form name='testform' method='post'>"
                       "<input type='text' name='firstname'/>"
                       "<input type='text' name='lastname'/>"
                       "<input type='email' name='email'/>"
                       "</form></body></html>",
                      web_state());

  ExecuteJavaScript(@"document.getElementsByName('firstname')[0].focus()");

  autofill::SuggestionControllerJavaScriptFeature::GetInstance()
      ->SelectNextElementInFrame(GetMainFrame());
  EXPECT_TRUE(WaitUntilElementSelected(@"lastname"));
  __block BOOL block_was_called = NO;
  autofill::SuggestionControllerJavaScriptFeature::GetInstance()
      ->FetchPreviousAndNextElementsPresenceInFrame(
          GetMainFrame(), base::BindOnce(^void(bool has_previous_element,
                                               bool has_next_element) {
            block_was_called = YES;
            EXPECT_TRUE(has_previous_element);
            EXPECT_TRUE(has_next_element);
          }));
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      TestTimeouts::action_timeout(), ^bool() {
        return block_was_called;
      }));
  autofill::SuggestionControllerJavaScriptFeature::GetInstance()
      ->SelectNextElementInFrame(GetMainFrame());
  EXPECT_TRUE(WaitUntilElementSelected(@"email"));
  autofill::SuggestionControllerJavaScriptFeature::GetInstance()
      ->SelectPreviousElementInFrame(GetMainFrame());
  EXPECT_TRUE(WaitUntilElementSelected(@"lastname"));
}

void SuggestionControllerJavaScriptFeatureTest::SequentialNavigationSkipCheck(
    NSString* attribute,
    BOOL shouldSkip) {
  web::test::LoadHtml(
      [NSString stringWithFormat:@"<html><body>"
                                  "<form name='testform' method='post'>"
                                  "<input type='text' name='firstname'/>"
                                  "<%@ name='middlename'/>"
                                  "<input type='text' name='lastname'/>"
                                  "</form></body></html>",
                                 attribute],
      web_state());
  ExecuteJavaScript(@"document.getElementsByName('firstname')[0].focus()");
  EXPECT_NSEQ(@"firstname", GetActiveElementName());
  autofill::SuggestionControllerJavaScriptFeature::GetInstance()
      ->SelectNextElementInFrame(GetMainFrame());
  if (shouldSkip)
    EXPECT_TRUE(WaitUntilElementSelected(@"lastname"));
  else
    EXPECT_TRUE(WaitUntilElementSelected(@"middlename"));
}

TEST_F(SuggestionControllerJavaScriptFeatureTest,
       SequentialNavigationNoSkipText) {
  SequentialNavigationSkipCheck(@"input type='text'", NO);
}

TEST_F(SuggestionControllerJavaScriptFeatureTest,
       SequentialNavigationNoSkipTextArea) {
  SequentialNavigationSkipCheck(@"input type='textarea'", NO);
}

TEST_F(SuggestionControllerJavaScriptFeatureTest,
       SequentialNavigationOverInvisibleElement) {
  SequentialNavigationSkipCheck(@"input type='text' style='display:none'", YES);
}

TEST_F(SuggestionControllerJavaScriptFeatureTest,
       SequentialNavigationOverHiddenElement) {
  SequentialNavigationSkipCheck(@"input type='text' style='visibility:hidden'",
                                YES);
}

TEST_F(SuggestionControllerJavaScriptFeatureTest,
       SequentialNavigationOverDisabledElement) {
  SequentialNavigationSkipCheck(@"type='text' disabled", YES);
}

TEST_F(SuggestionControllerJavaScriptFeatureTest,
       SequentialNavigationNoSkipPassword) {
  SequentialNavigationSkipCheck(@"input type='password'", NO);
}

TEST_F(SuggestionControllerJavaScriptFeatureTest,
       SequentialNavigationSkipSubmit) {
  SequentialNavigationSkipCheck(@"input type='submit'", YES);
}

TEST_F(SuggestionControllerJavaScriptFeatureTest,
       SequentialNavigationSkipImage) {
  SequentialNavigationSkipCheck(@"input type='image'", YES);
}

TEST_F(SuggestionControllerJavaScriptFeatureTest,
       SequentialNavigationSkipButton) {
  SequentialNavigationSkipCheck(@"input type='button'", YES);
}

TEST_F(SuggestionControllerJavaScriptFeatureTest,
       SequentialNavigationSkipRange) {
  SequentialNavigationSkipCheck(@"input type='range'", YES);
}

TEST_F(SuggestionControllerJavaScriptFeatureTest,
       SequentialNavigationSkipRadio) {
  SequentialNavigationSkipCheck(@"type='radio'", YES);
}

TEST_F(SuggestionControllerJavaScriptFeatureTest,
       SequentialNavigationSkipCheckbox) {
  SequentialNavigationSkipCheck(@"type='checkbox'", YES);
}

// Test fixture to test
// `FetchPreviousAndNextElementsPresenceInFrameWithID`.
class FetchPreviousAndNextExceptionTest
    : public SuggestionControllerJavaScriptFeatureTest {
 public:
  void SetUp() override {
    SuggestionControllerJavaScriptFeatureTest::SetUp();
    web::test::LoadHtml(@"<html></html>", web_state());
  }

 protected:
  // Evaluates JS and tests that the completion handler passed to
  // `FetchPreviousAndNextElementsPresenceInFrameWithID` is called with
  // (false, false) indicating no previous and next element.
  void EvaluateJavaScriptAndExpectNoPreviousAndNextElement(NSString* js) {
    ExecuteJavaScript(js);
    __block BOOL block_was_called = NO;
    autofill::SuggestionControllerJavaScriptFeature::GetInstance()
        ->FetchPreviousAndNextElementsPresenceInFrame(
            GetMainFrame(),
            base::BindOnce(^(bool hasPreviousElement, bool hasNextElement) {
              EXPECT_FALSE(hasPreviousElement);
              EXPECT_FALSE(hasNextElement);
              block_was_called = YES;
            }));
    ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
        TestTimeouts::action_timeout(), ^bool() {
          base::RunLoop().RunUntilIdle();
          return block_was_called;
        }));
  }
};

// Tests that `fetchPreviousAndNextElementsPresenceWithCompletionHandler` works
// when `__gCrWeb.suggestion.hasPreviousElement` throws an exception.
TEST_F(FetchPreviousAndNextExceptionTest, HasPreviousElementException) {
  EvaluateJavaScriptAndExpectNoPreviousAndNextElement(
      @"__gCrWeb.suggestion.hasPreviousElement = function() { bar.foo1; }");
}

// Tests that `fetchPreviousAndNextElementsPresenceWithCompletionHandler` works
// when `__gCrWeb.suggestion.hasNextElement` throws an exception.
TEST_F(FetchPreviousAndNextExceptionTest, HasNextElementException) {
  EvaluateJavaScriptAndExpectNoPreviousAndNextElement(
      @"__gCrWeb.suggestion.hasNextElement = function() { bar.foo1; }");
}

// Tests that `fetchPreviousAndNextElementsPresenceWithCompletionHandler` works
// when `Array.toString` has been overridden to return a malformed string
// without a ",".
TEST_F(FetchPreviousAndNextExceptionTest, HasPreviousElementNull) {
  EvaluateJavaScriptAndExpectNoPreviousAndNextElement(
      @"Array.prototype.toString = function() { return 'Hello'; }");
}

}  // namespace
