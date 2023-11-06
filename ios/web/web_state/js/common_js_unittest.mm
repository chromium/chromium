// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>
#import <stddef.h>

#import "base/strings/sys_string_conversions.h"
#import "ios/web/public/test/javascript_test.h"
#import "ios/web/public/test/js_test_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

namespace {

// Struct for isTextField() test data.
struct TextFieldTestElement {
  // The element name.
  const char* element_name;
  // The index of this element in those that have the same name.
  const int element_index;
  // True if this is expected to be a text field.
  const bool expected_is_text_field;
};

// Struct for stringify() test data.
struct TestScriptAndExpectedValue {
  NSString* test_script;
  id expected_value;
};

}  // namespace

namespace web {

// Test fixture to test common.js.
class CommonJsTest : public web::JavascriptTest {
 protected:
  CommonJsTest() {}
  ~CommonJsTest() override {}

  void SetUp() override {
    web::JavascriptTest::SetUp();

    AddGCrWebScript();
    AddCommonScript();
  }
};

// Tests __gCrWeb.common.isTextField JavaScript API.
TEST_F(CommonJsTest, IsTestField) {
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

  static const struct TextFieldTestElement testElements[] = {
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
      {"submit", 0, false}};
  for (size_t i = 0; i < std::size(testElements); ++i) {
    TextFieldTestElement element = testElements[i];
    id result = web::test::ExecuteJavaScript(
        web_view(),
        [NSString
            stringWithFormat:@"__gCrWeb.common.isTextField("
                              "window.document.getElementsByName('%s')[%d])",
                             element.element_name, element.element_index]);
    EXPECT_NSEQ(element.expected_is_text_field ? @YES : @NO, result)
        << element.element_name << " with index " << element.element_index
        << " isTextField(): " << element.expected_is_text_field;
  }
}

// Tests __gCrWeb.stringify JavaScript API.
TEST_F(CommonJsTest, Stringify) {
  TestScriptAndExpectedValue test_data[] = {
      // Stringify a string that contains various characters that must
      // be escaped.
      {@"__gCrWeb.stringify('a\\u000a\\t\\b\\\\\\\"Z')",
       @"\"a\\n\\t\\b\\\\\\\"Z\""},
      // Stringify a number.
      {@"__gCrWeb.stringify(77.7)", @"77.7"},
      // Stringify an array.
      {@"__gCrWeb.stringify(['a','b'])", @"[\"a\",\"b\"]"},
      // Stringify an object.
      {@"__gCrWeb.stringify({'a':'b','c':'d'})", @"{\"a\":\"b\",\"c\":\"d\"}"},
      // Stringify a hierarchy of objects and arrays.
      {@"__gCrWeb.stringify([{'a':['b','c'],'d':'e'},'f'])",
       @"[{\"a\":[\"b\",\"c\"],\"d\":\"e\"},\"f\"]"},
      // Stringify null.
      {@"__gCrWeb.stringify(null)", @"null"},
      // Stringify an object with a toJSON function.
      {@"temp = [1,2];"
        "temp.toJSON = function (key) {return undefined};"
        "__gCrWeb.stringify(temp)",
       @"[1,2]"},
      // Stringify an object with a toJSON property that is not a function.
      {@"temp = [1,2];"
        "temp.toJSON = 42;"
        "__gCrWeb.stringify(temp)",
       @"[1,2]"},
      // Stringify an undefined object.
      {@"__gCrWeb.stringify(undefined)", @"undefined"},
  };

  for (size_t i = 0; i < std::size(test_data); i++) {
    TestScriptAndExpectedValue& data = test_data[i];
    // Load a sample HTML page. As a side-effect, loading HTML via
    // `webController_` will also inject web_bundle.js.
    LoadHtml(@"<p>");
    id result = web::test::ExecuteJavaScript(web_view(), data.test_script);
    EXPECT_NSEQ(data.expected_value, result)
        << " in test " << i << ": "
        << base::SysNSStringToUTF8(data.test_script);
  }
}

TEST_F(CommonJsTest, RemoveQueryAndReferenceFromURL) {
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
        [NSString stringWithFormat:
                      @"__gCrWeb.common.removeQueryAndReferenceFromURL('%@')",
                      data.input_url]);
    EXPECT_NSEQ(data.expected_output, result)
        << " in test " << i << ": " << base::SysNSStringToUTF8(data.input_url);
  }
}

// Tests that removeQueryAndReferenceFromURL() returns an empty string when
// the window.URL prototype was corrupted (i.e. the hosted page replaces the
// prototype by something else).
TEST_F(CommonJsTest,
       RemoveQueryAndReferenceFromURL_WithCorruptedURLPrototype__MissingProperty) {
  LoadHtml(@"<p>");

  // Replace the window.URL prototype.
  web::test::ExecuteJavaScript(
      web_view(), @"window.URL = function() { return { weird_field: 1 }; };");

  id result = web::test::ExecuteJavaScript(
      web_view(),
      @"__gCrWeb.common.removeQueryAndReferenceFromURL('http://foo1.com/bar')");
  EXPECT_NSEQ(@"", result);
}

// Tests that removeQueryAndReferenceFromURL() returns an empty string when
// the window.URL prototype was corrupted (i.e. the hosted page replaces the
// prototype by something else).
TEST_F(CommonJsTest,
       RemoveQueryAndReferenceFromURL_WithCorruptedURLPrototype_WrongType) {
  LoadHtml(@"<p>");

  // Replace the window.URL prototype.
  web::test::ExecuteJavaScript(web_view(),
                               @"window.URL = function() { return {"
                                "origin: 'o', path: 'pa', protocol: 3 }; };");

  id result = web::test::ExecuteJavaScript(
      web_view(),
      @"__gCrWeb.common.removeQueryAndReferenceFromURL('http://foo1.com/bar')");
  EXPECT_NSEQ(@"", result);
}

}  // namespace web
