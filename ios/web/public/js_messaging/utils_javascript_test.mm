// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/public/test/javascript_test.h"
#import "ios/web/public/test/js_test_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "url/gurl.h"

NSString* kUtilsSampleMessageHandlerName = @"UtilsSampleMessageHandlerName";

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
  } test_data[] = {
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
  };
  for (size_t i = 0; i < std::size(test_data); i++) {
    LoadHtml(@"<p>");
    TestData& data = test_data[i];
    id result = web::test::ExecuteJavaScript(
        web_view(),
        [NSString
            stringWithFormat:
                @"__gCrWeb.utils_tests.removeQueryAndReferenceFromURL('%@')",
                data.input_url]);
    EXPECT_NSEQ(data.expected_output, result)
        << " in test " << i << ": " << base::SysNSStringToUTF8(data.input_url);
  }
}

// Tests that removeQueryAndReferenceFromURL() returns an empty string when
// the window.URL prototype was corrupted (i.e. the hosted page replaces the
// prototype by something else).
TEST_F(
    UtilsJavaScriptTest,
    RemoveQueryAndReferenceFromURL_WithCorruptedURLPrototype_MissingProperty) {
  // Replace the window.URL prototype.
  web::test::ExecuteJavaScript(
      web_view(), @"window.URL = function() { return { weird_field: 1 }; };");

  NSString* apiCall =
      @"__gCrWeb.utils_tests.removeQueryAndReferenceFromURL('%@')";
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
  web::test::ExecuteJavaScript(web_view(),
                               @"window.URL = function() { return {"
                                "origin: 'o', path: 'pa', protocol: 3 }; };");

  NSString* apiCall =
      @"__gCrWeb.utils_tests.removeQueryAndReferenceFromURL('%@')";
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
  } test_data[] = {
      {@"undefined", @""},
      {@"null", @""},
      {@"function() {}", @""},
      {@"'stringButNotURL'", @""},
  };
  for (size_t i = 0; i < std::size(test_data); i++) {
    TestData& data = test_data[i];
    NSString* js = [NSString
        stringWithFormat:
            @"__gCrWeb.utils_tests.removeQueryAndReferenceFromURL(%@)",
            data.input];
    id result = web::test::ExecuteJavaScript(web_view(), js);

    EXPECT_NSEQ(data.expected_output, result)
        << " in test " << i << ": " << base::SysNSStringToUTF8(data.input);
  }
}

// Tests that sendWebKitMessage works as expected
TEST_F(UtilsJavaScriptTest, SendWebKitMessage) {
  struct TestData {
    NSString* input;
    id expected_output;
  } test_data[] = {
      {@"1", @1},
      {@"1.5", @1.5},
      {@"'String data'", @"String data"},
      {@"['a', 'b', 'c']", @[ @"a", @"b", @"c" ]},
      {@"{'a' : 'x', 'b' : 'y', 'c' : 'z'}",
       @{@"a" : @"x", @"b" : @"y", @"c" : @"z"}},
  };
  for (size_t i = 0; i < std::size(test_data); i++) {
    TestData& data = test_data[i];
    // Reset value to ensure wait below stops at correct time.
    handler_.lastReceivedMessage = nil;

    NSString* js = [NSString
        stringWithFormat:@"__gCrWeb.utils_tests.sendWebKitMessage('%@', %@)",
                         kUtilsSampleMessageHandlerName, data.input];
    web::test::ExecuteJavaScript(web_view(), js);

    ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForJSCompletionTimeout, ^bool() {
          return handler_.lastReceivedMessage;
        }));

    EXPECT_NSEQ(data.expected_output, handler_.lastReceivedMessage.body)
        << " in test " << i << ": " << base::SysNSStringToUTF8(data.input);
  }
}

// Tests that trim works as expected
TEST_F(UtilsJavaScriptTest, Trim) {
  struct TestData {
    NSString* input_string;
    NSString* expected_output;
  } test_data[] = {
      {@"  content", @"content"},
      {@"content  ", @"content"},
      {@"  content  ", @"content"},
      {@"  cont   ent  ", @"cont   ent"},
  };
  for (size_t i = 0; i < std::size(test_data); i++) {
    TestData& data = test_data[i];
    NSString* js = [NSString
        stringWithFormat:@"__gCrWeb.utils_tests.trim('%@')", data.input_string];
    id result = web::test::ExecuteJavaScript(web_view(), js);

    EXPECT_NSEQ(data.expected_output, result)
        << " in test " << i << ": "
        << base::SysNSStringToUTF8(data.input_string);
  }
}
