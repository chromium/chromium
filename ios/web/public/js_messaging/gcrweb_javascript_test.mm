// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/javascript_test.h"
#import "ios/web/public/test/js_test_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

// Test fixture for gcrweb.ts.
class CrWebJavaScriptTest : public web::JavascriptTest {
 protected:
  const NSString* unit_tests_api_ = @"__gCrWeb.getRegisteredApi('unit_tests')";

  void SetUp() override {
    JavascriptTest::SetUp();
    AddGCrWebScript();
    AddUserScript(@"gcrweb_test_api");

    ASSERT_TRUE(LoadHtml(@"<p>"));
  }
};

// Tests to retrieve the value of a property exported through gCrWeb API.
TEST_F(CrWebJavaScriptTest, RetrieveExportedProperty) {
  NSString* getExportedProperty = [NSString
      stringWithFormat:@"%@.getProperty('stringProperty')", unit_tests_api_];

  id result = web::test::ExecuteJavaScript(web_view(), getExportedProperty);

  EXPECT_NSEQ(@"Set a property with test purpose.", result);
}

// Tests executing a real function exported through gCrWeb API.
TEST_F(CrWebJavaScriptTest, ExecuteRealFunction) {
  NSString* executeFunction = [NSString
      stringWithFormat:@"%@.getFunction('addNum')(4, 3)", unit_tests_api_];

  id result = web::test::ExecuteJavaScript(web_view(), executeFunction);

  EXPECT_NSEQ(@7, result);
}

// Tests having an array as property and having it updates through gCrWeb API.
TEST_F(CrWebJavaScriptTest, PropertyAsArray) {
  NSString* getArrayProperty = [NSString
      stringWithFormat:@"%@.getProperty('arrayProperty')", unit_tests_api_];

  id initial_size = web::test::ExecuteJavaScript(
      web_view(), [NSString stringWithFormat:@"%@.length", getArrayProperty]);
  ASSERT_NSEQ(@0, initial_size);

  web::test::ExecuteJavaScriptInWebView(
      web_view(), [NSString stringWithFormat:@"%@.push(8)", getArrayProperty]);

  id new_size = web::test::ExecuteJavaScript(
      web_view(), [NSString stringWithFormat:@"%@.length", getArrayProperty]);
  ASSERT_NSEQ(@1, new_size);

  id result_value = web::test::ExecuteJavaScript(
      web_view(), [NSString stringWithFormat:@"%@[0]", getArrayProperty]);
  EXPECT_NSEQ(@8, result_value);
}

// Tests if an exception is thrown when trying to override an API.
TEST_F(CrWebJavaScriptTest, ThrowExceptionInAPICollision) {
  NSError* execution_error = nil;
  web::test::ExecuteJavaScriptInWebView(
      web_view(),
      @"__gCrWeb.registerApi('unit_tests');", &execution_error);

  ASSERT_TRUE(execution_error);
  EXPECT_NSEQ(@"CrWebError: API unit_tests already registered.",
              execution_error.userInfo[@"WKJavaScriptExceptionMessage"]);
}

// Tests if hasRegisteredApi returns true when the API is registered.
TEST_F(CrWebJavaScriptTest, ConfirmRegisteredAPITrue) {
  id result = web::test::ExecuteJavaScript(
      web_view(),
      @"__gCrWeb.hasRegisteredApi('unit_tests');");
  EXPECT_NSEQ(@YES, result);
}

// Tests if hasRegisteredApi returns false when no API is registered.
TEST_F(CrWebJavaScriptTest, ConfirmRegisteredAPIFalse) {
  id result = web::test::ExecuteJavaScript(
      web_view(),
      @"__gCrWeb.hasRegisteredApi('unit_tests_');");
  EXPECT_NSEQ(@NO, result);
}

// Tests if an exception is thrown when trying to get no available API.
TEST_F(CrWebJavaScriptTest, ThrowExceptionForNoRegisteredAPI) {
  NSError* execution_error = nil;
  web::test::ExecuteJavaScriptInWebView(
      web_view(),
      @"__gCrWeb.getRegisteredApi('unit_tests_');",
      &execution_error);

  ASSERT_TRUE(execution_error);
  EXPECT_NSEQ(@"CrWebError: API unit_tests_ is not registered in CrWeb.",
              execution_error.userInfo[@"WKJavaScriptExceptionMessage"]);
}

// Tests that a frameId is created.
TEST_F(CrWebJavaScriptTest, FrameId) {
  id frame_id1 =
      web::test::ExecuteJavaScript(web_view(), @"__gCrWeb.getFrameId()");

  ASSERT_TRUE(frame_id1);
  ASSERT_TRUE([frame_id1 isKindOfClass:[NSString class]]);
  EXPECT_GT([frame_id1 length], 0ul);

  // Validating that once created the frame id remain the same if the page isn't
  // reloaded.
  id frame_id2 =
      web::test::ExecuteJavaScript(web_view(), @"__gCrWeb.getFrameId()");
  EXPECT_NSEQ(frame_id1, frame_id2);
}

// Tests that the frameId is unique between two page loads.
TEST_F(CrWebJavaScriptTest, UniqueFrameID) {
  ASSERT_TRUE(LoadHtml(@"<p>"));
  id frame_id1 =
      web::test::ExecuteJavaScript(web_view(), @"__gCrWeb.getFrameId()");

  ASSERT_TRUE(LoadHtml(@"<p>"));
  id frame_id2 =
      web::test::ExecuteJavaScript(web_view(), @"__gCrWeb.getFrameId()");

  // Validate second frameId.
  ASSERT_TRUE(frame_id2);
  ASSERT_TRUE([frame_id2 isKindOfClass:[NSString class]]);
  EXPECT_GT([frame_id2 length], 0ul);

  EXPECT_NSNE(frame_id1, frame_id2);
}
