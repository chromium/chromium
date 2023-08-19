// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "ios/web/public/test/javascript_test.h"
#import "ios/web/public/test/js_test_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

namespace web {

// Test fixture to test message.js.
class MessageJsTest : public web::JavascriptTest {
 protected:
  MessageJsTest() {}
  ~MessageJsTest() override {}

  void SetUp() override {
    web::JavascriptTest::SetUp();

    AddGCrWebScript();
    AddCommonScript();
    AddMessageScript();
  }
};

// Tests that a frameId is created.
TEST_F(MessageJsTest, FrameId) {
  ASSERT_TRUE(LoadHtml(@"<p>"));

  id result = web::test::ExecuteJavaScript(web_view(),
                                           @"__gCrWeb.message.getFrameId()");

  ASSERT_TRUE(result);
  ASSERT_TRUE([result isKindOfClass:[NSString class]]);
  EXPECT_GT([result length], 0ul);
}

// Tests that the frameId is unique between two page loads.
TEST_F(MessageJsTest, UniqueFrameID) {
  ASSERT_TRUE(LoadHtml(@"<p>"));
  id frame_id1 = web::test::ExecuteJavaScript(web_view(),
                                              @"__gCrWeb.message.getFrameId()");

  ASSERT_TRUE(LoadHtml(@"<p>"));
  id frame_id2 = web::test::ExecuteJavaScript(web_view(),
                                              @"__gCrWeb.message.getFrameId()");
  // Validate second frameId.
  ASSERT_TRUE(frame_id2);
  ASSERT_TRUE([frame_id2 isKindOfClass:[NSString class]]);
  EXPECT_GT([frame_id2 length], 0ul);

  EXPECT_NSNE(frame_id1, frame_id2);
}

}  // namespace web
