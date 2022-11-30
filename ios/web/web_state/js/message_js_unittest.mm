// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "ios/web/public/test/web_test_with_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// JavaScript function to return a frame's frameId.
const char kGetFrameIdJsFunction[] = "message.getFrameId";
}  // namespace

namespace web {

// Test fixture to test message.js.
typedef web::WebTestWithWebState MessageJsTest;

// Tests that a frameId is created.
TEST_F(MessageJsTest, FrameId) {
  ASSERT_TRUE(LoadHtml("<p>"));

  auto result = CallJavaScriptFunction(kGetFrameIdJsFunction, {});
  // Validate frameId.
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_string());
  EXPECT_GT(result->GetString().length(), 0ul);
}

// Tests that the frameId is unique between two page loads.
TEST_F(MessageJsTest, UniqueFrameID) {
  ASSERT_TRUE(LoadHtml("<p>"));
  auto frame_id1 = CallJavaScriptFunction(kGetFrameIdJsFunction, {});

  ASSERT_TRUE(LoadHtml("<p>"));
  auto frame_id2 = CallJavaScriptFunction(kGetFrameIdJsFunction, {});
  // Validate second frameId.
  ASSERT_TRUE(frame_id2);
  ASSERT_TRUE(frame_id2->is_string());
  EXPECT_GT(frame_id2->GetString().length(), 0ul);

  EXPECT_NE(frame_id1->GetString(), frame_id2->GetString());
}

}  // namespace web
