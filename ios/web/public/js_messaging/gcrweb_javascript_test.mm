// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/javascript_test.h"
#import "ios/web/public/test/js_test_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

// Test fixture for gcrweb.ts
class CrWebJavaScriptTest : public web::JavascriptTest {
  void SetUp() override {
    JavascriptTest::SetUp();
    AddUserScript(@"gcrweb_test_api");

    ASSERT_TRUE(LoadHtml(@"<p>"));

    web::test::ExecuteJavaScript(web_view(),
                                 @"__gCrWeb.unit_tests.setupRegisterApi()");
  }
};

// Tests to retrieve the value of a property exported through gCrWeb API.
TEST_F(CrWebJavaScriptTest, RetrieveExportedProperty) {
  NSString* getExportedProperty =
      @"__gCrWeb.unit_tests.getRegisteredApi('crWebAPI').getProperty('"
      @"stringProperty')";

  id result = web::test::ExecuteJavaScript(web_view(), getExportedProperty);

  EXPECT_NSEQ(@"Set a property with test purpose.", result);
}

// Tests executing a real function exported through gCrWeb API.
TEST_F(CrWebJavaScriptTest, ExecuteRealFunction) {
  NSString* executeFunction = @"__gCrWeb.unit_tests.getRegisteredApi('crWebAPI'"
                              @").getFunction('addNum')(4, 3)";

  id result = web::test::ExecuteJavaScript(web_view(), executeFunction);

  EXPECT_NSEQ(@7, result);
}

// Tests having an array as property and having it updates through gCrWeb API.
TEST_F(CrWebJavaScriptTest, PropertyAsArray) {
  id result_size_zero = web::test::ExecuteJavaScript(
      web_view(), @"__gCrWeb.unit_tests.getRegisteredApi('crWebAPI')."
                  @"getProperty('arrayProperty').length");

  ASSERT_NSEQ(@0, result_size_zero);

  web::test::ExecuteJavaScript(
      web_view(), @"__gCrWeb.unit_tests.getRegisteredApi('crWebAPI')."
                  @"getProperty('arrayProperty').push(8)");
  id result_size_one = web::test::ExecuteJavaScript(
      web_view(), @"__gCrWeb.unit_tests.getRegisteredApi('crWebAPI')."
                  @"getProperty('arrayProperty').length");

  ASSERT_NSEQ(@1, result_size_one);

  id result_value = web::test::ExecuteJavaScript(
      web_view(), @"__gCrWeb.unit_tests.getRegisteredApi('crWebAPI')."
                  @"getProperty('arrayProperty')[0]");

  EXPECT_NSEQ(@8, result_value);
}

// Tests if an exception is thrown when trying to override an API.
TEST_F(CrWebJavaScriptTest, ThrowExceptionInCollision) {
  NSError* execution_error = nil;
  web::test::ExecuteJavaScript(web_view(),
                               @"__gCrWeb.unit_tests.registerApi('crWebAPI')",
                               &execution_error);

  ASSERT_NE(execution_error, nil);
  EXPECT_NSEQ(@"Error: API crWebAPI already registered.",
              execution_error.userInfo[@"WKJavaScriptExceptionMessage"]);
}

// Tests that a frameId is created.
TEST_F(CrWebJavaScriptTest, FrameId) {
  id frame_id1 = web::test::ExecuteJavaScript(
      web_view(), @"__gCrWeb.unit_tests.getFrameId()");

  ASSERT_TRUE(frame_id1);
  ASSERT_TRUE([frame_id1 isKindOfClass:[NSString class]]);
  EXPECT_GT([frame_id1 length], 0ul);

  // Validating that once created the frame id remain the same if the page isn't
  // reloaded.
  id frame_id2 = web::test::ExecuteJavaScript(
      web_view(), @"__gCrWeb.unit_tests.getFrameId()");
  EXPECT_NSEQ(frame_id1, frame_id2);
}

// Tests that the frameId is unique between two page loads.
TEST_F(CrWebJavaScriptTest, UniqueFrameID) {
  ASSERT_TRUE(LoadHtml(@"<p>"));
  id frame_id1 = web::test::ExecuteJavaScript(
      web_view(), @"__gCrWeb.unit_tests.getFrameId()");

  ASSERT_TRUE(LoadHtml(@"<p>"));
  id frame_id2 = web::test::ExecuteJavaScript(
      web_view(), @"__gCrWeb.unit_tests.getFrameId()");

  // Validate second frameId.
  ASSERT_TRUE(frame_id2);
  ASSERT_TRUE([frame_id2 isKindOfClass:[NSString class]]);
  EXPECT_GT([frame_id2 length], 0ul);

  EXPECT_NSNE(frame_id1, frame_id2);
}
