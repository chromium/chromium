// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <array>

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/public/test/javascript_test.h"
#import "ios/web/public/test/js_test_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "url/gurl.h"

NSString* const kUtilsSampleMessageHandlerName =
    @"UtilsSampleMessageHandlerName";

// A WKScriptMessageHandler which stores the last received WKScriptMessage;
@interface FakeScriptMessageHandler : NSObject <WKScriptMessageHandler>

@property(nonatomic, strong) WKScriptMessage* lastReceivedMessage;

@end

@implementation FakeScriptMessageHandler

- (void)userContentController:(WKUserContentController*)userContentController
      didReceiveScriptMessage:(WKScriptMessage*)message {
  _lastReceivedMessage = message;
}

@end

// Test fixture for utils.ts.
class UtilsJavaScriptTest : public web::JavascriptTest {
 protected:
  UtilsJavaScriptTest() : handler_([[FakeScriptMessageHandler alloc] init]) {
    [web_view().configuration.userContentController
        addScriptMessageHandler:handler_
                           name:kUtilsSampleMessageHandlerName];
  }
  ~UtilsJavaScriptTest() override {}

  void SetUp() override {
    JavascriptTest::SetUp();

    AddGCrWebScript();
    AddUserScript(@"utils_test_api");

    ASSERT_TRUE(LoadHtml(@"<p>"));
  }

 protected:
  FakeScriptMessageHandler* handler_;
};

// Tests that removeQueryAndReferenceFromURL works as expected
TEST_F(UtilsJavaScriptTest, RemoveQueryAndReferenceFromURL) {
  struct TestData {
    NSString* input_url;
    NSString* expected_output;
  };

  const auto kTestData = std::to_array<TestData>({
      {@"http://foo1.com/bar", @"http://foo1.com/bar"},
      {@"http://foo2.com/bar#baz", @"http://foo2.com/bar"},
      {@"http://foo3.com/bar?baz", @"http://foo3.com/bar"},
      // Order of fragment and query string does not matter.
      {@"http://foo4.com/bar#baz?blech", @"http://foo4.com/bar"},
      {@"http://foo5.com/bar?baz#blech", @"http://foo5.com/bar"},
      // Truncates on the first fragment mark.
      {@"http://foo6.com/bar/#baz#blech", @"http://foo6.com/bar/"},
      // Poorly formed URLs are normalized.
      {@"http:///foo7.com//bar?baz", @"http://foo7.com//bar"},
      // Non-http protocols.
      {@"data:abc", @"data:abc"},
      {@"javascript:login()", @"javascript:login()"},
  });

  for (const TestData& data : kTestData) {
    LoadHtml(@"<p>");
    id result = web::test::ExecuteJavaScript(
        web_view(),
        [NSString stringWithFormat:
                      @"__gCrWeb.getRegisteredApi('utils_tests').getFunction('"
                      @"removeQueryAndReferenceFromURL')('%@')",
                      data.input_url]);
    EXPECT_NSEQ(data.expected_output, result)
        << " with input: " << base::SysNSStringToUTF8(data.input_url);
  }
}

// Tests that removeQueryAndReferenceFromURL() returns an empty string when
// the window.URL prototype was corrupted (i.e. the hosted page replaces the
// prototype by something else).
TEST_F(
    UtilsJavaScriptTest,
    RemoveQueryAndReferenceFromURL_WithCorruptedURLPrototype_MissingProperty) {
  // Replace the window.URL prototype.
  web::test::ExecuteJavaScriptInWebView(
      web_view(), @"window.URL = function() { return { weird_field: 1 }; };");

  NSString* apiCall = @"__gCrWeb.getRegisteredApi('utils_tests').getFunction('"
                      @"removeQueryAndReferenceFromURL')('%@')";
  NSString* url = @"http://foo1.com/bar";
  NSString* js = [NSString stringWithFormat:apiCall, url];
  id result = web::test::ExecuteJavaScript(web_view(), js);
  EXPECT_NSEQ(@"", result);
}

// Tests that removeQueryAndReferenceFromURL() returns an empty string when
// the window.URL prototype was corrupted (i.e. the hosted page replaces the
// prototype by something else).
TEST_F(UtilsJavaScriptTest,
       RemoveQueryAndReferenceFromURL_WithCorruptedURLPrototype_WrongType) {
  // Replace the window.URL prototype.
  web::test::ExecuteJavaScriptInWebView(
      web_view(), @"window.URL = function() { return {"
                   "origin: 'o', path: 'pa', protocol: 3 }; };");

  NSString* apiCall = @"__gCrWeb.getRegisteredApi('utils_tests').getFunction('"
                      @"removeQueryAndReferenceFromURL')('%@')";
  NSString* url = @"http://foo1.com/bar";
  NSString* js = [NSString stringWithFormat:apiCall, url];
  id result = web::test::ExecuteJavaScript(web_view(), js);
  EXPECT_NSEQ(@"", result);
}

// Tests that removeQueryAndReferenceFromURL doesn't throw an exception on
// invalid input.
TEST_F(UtilsJavaScriptTest, RemoveQueryAndReferenceFromURL_InvalidInput) {
  struct TestData {
    NSString* input;
    NSString* expected_output;
  };

  const auto kTestData = std::to_array<TestData>({
      {@"undefined", @""},
      {@"null", @""},
      {@"function() {}", @""},
      {@"'stringButNotURL'", @""},
  });

  for (const TestData& data : kTestData) {
    NSString* js = [NSString
        stringWithFormat:@"__gCrWeb.getRegisteredApi('utils_tests')."
                         @"getFunction('removeQueryAndReferenceFromURL')(%@)",
                         data.input];
    id result = web::test::ExecuteJavaScript(web_view(), js);

    EXPECT_NSEQ(data.expected_output, result)
        << " with input: " << base::SysNSStringToUTF8(data.input);
  }
}

// Tests that sendWebKitMessage works as expected
TEST_F(UtilsJavaScriptTest, SendWebKitMessage) {
  struct TestData {
    NSString* input;
    id expected_output;
  };

  const auto kTestData = std::to_array<TestData>({
      {@"1", @1},
      {@"1.5", @1.5},
      {@"'String data'", @"String data"},
      {@"['a', 'b', 'c']", @[ @"a", @"b", @"c" ]},
      {@"{'a' : 'x', 'b' : 'y', 'c' : 'z'}",
       @{@"a" : @"x", @"b" : @"y", @"c" : @"z"}},
  });

  for (const TestData& data : kTestData) {
    // Reset value to ensure wait below stops at correct time.
    handler_.lastReceivedMessage = nil;

    NSString* js = [NSString
        stringWithFormat:@"__gCrWeb.getRegisteredApi('utils_tests')."
                         @"getFunction('sendWebKitMessage')('%@', %@)",
                         kUtilsSampleMessageHandlerName, data.input];
    web::test::ExecuteJavaScriptInWebView(web_view(), js);

    ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForJSCompletionTimeout, ^bool() {
          return handler_.lastReceivedMessage;
        }));

    EXPECT_NSEQ(data.expected_output, handler_.lastReceivedMessage.body)
        << " with input: " << base::SysNSStringToUTF8(data.input);
  }
}

// Tests that trim works as expected
TEST_F(UtilsJavaScriptTest, Trim) {
  struct TestData {
    NSString* input_string;
    NSString* expected_output;
  };

  const auto kTestData = std::to_array<TestData>({
      {@"'  content'", @"content"},
      {@"'content  '", @"content"},
      {@"'  content  '", @"content"},
      {@"'  cont   ent  '", @"cont   ent"},
      {@"null", @""},
      {@"undefined", @""},
  });

  for (const TestData& data : kTestData) {
    NSString* js = [NSString
        stringWithFormat:
            @"__gCrWeb.getRegisteredApi('utils_tests').getFunction('trim')(%@)",
            data.input_string];
    id result = web::test::ExecuteJavaScript(web_view(), js);

    EXPECT_NSEQ(data.expected_output, result)
        << " with input: " << base::SysNSStringToUTF8(data.input_string);
  }
}

// Tests that isTextField from utils.ts works as expected.
TEST_F(UtilsJavaScriptTest, IsTextField) {
  // Struct for isTextField() test data.
  struct TextFieldTestElement {
    // The element name.
    const char* element_name;
    // The index of this element in those that have the same name.
    const int element_index;
    // True if this is expected to be a text field.
    const bool expected_is_text_field;
  };

  LoadHtml(@"<html><body>"
            "<input type='text' name='firstname'>"
            "<input type='text' name='lastname'>"
            "<input type='email' name='email'>"
            "<input type='tel' name='phone'>"
            "<input type='url' name='blog'>"
            "<input type='number' name='expected number of clicks'>"
            "<input type='password' name='pwd'>"
            "<input type='checkbox' name='vehicle' value='Bike'>"
            "<input type='checkbox' name='vehicle' value='Car'>"
            "<input type='checkbox' name='vehicle' value='Rocket'>"
            "<input type='radio' name='boolean' value='true'>"
            "<input type='radio' name='boolean' value='false'>"
            "<input type='radio' name='boolean' value='other'>"
            "<select name='state'>"
            "  <option value='CA'>CA</option>"
            "  <option value='MA'>MA</option>"
            "</select>"
            "<select name='cars' multiple>"
            "  <option value='volvo'>Volvo</option>"
            "  <option value='saab'>Saab</option>"
            "  <option value='opel'>Opel</option>"
            "  <option value='audi'>Audi</option>"
            "</select>"
            "<input type='submit' name='submit' value='Submit'>"
            "</body></html>");

  const auto kTestElements = std::to_array<TextFieldTestElement>({
      {"firstname", 0, true},
      {"lastname", 0, true},
      {"email", 0, true},
      {"phone", 0, true},
      {"blog", 0, true},
      {"expected number of clicks", 0, true},
      {"pwd", 0, true},
      {"vehicle", 0, false},
      {"vehicle", 1, false},
      {"vehicle", 2, false},
      {"boolean", 0, false},
      {"boolean", 1, false},
      {"boolean", 2, false},
      {"state", 0, false},
      {"cars", 0, false},
      {"submit", 0, false},
  });

  for (const TextFieldTestElement& element : kTestElements) {
    id result = web::test::ExecuteJavaScript(
        web_view(),
        [NSString
            stringWithFormat:@"__gCrWeb.getRegisteredApi('utils_tests')."
                             @"getFunction('isTextField')("
                              "window.document.getElementsByName('%s')[%d])",
                             element.element_name, element.element_index]);
    EXPECT_NSEQ(element.expected_is_text_field ? @YES : @NO, result)
        << element.element_name << " with index " << element.element_index
        << " isTextField(): " << element.expected_is_text_field;
  }
}
