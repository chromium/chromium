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

}  // namespace web
