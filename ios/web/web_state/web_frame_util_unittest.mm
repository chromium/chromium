// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/web_state/web_frame_util.h"

#include "base/test/gtest_util.h"
#include "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

typedef PlatformTest WebFrameUtilTest;

// Tests the GetMainWebFrame function.
TEST_F(WebFrameUtilTest, GetMainWebFrame) {
  TestWebState test_web_state;
  test_web_state.CreateWebFramesManager();
  // Still no main frame.
  EXPECT_EQ(nullptr, GetMainWebFrame(&test_web_state));
  auto iframe =
      std::make_unique<FakeWebFrame>("iframe", false, GURL::EmptyGURL());
  test_web_state.AddWebFrame(std::move(iframe));
  // Still no main frame.
  EXPECT_EQ(nullptr, GetMainWebFrame(&test_web_state));

  auto main_frame =
      std::make_unique<FakeWebFrame>("main_frame", true, GURL::EmptyGURL());
  FakeWebFrame* main_frame_ptr = main_frame.get();
  test_web_state.AddWebFrame(std::move(main_frame));
  // Now there is a main frame.
  EXPECT_EQ(main_frame_ptr, GetMainWebFrame(&test_web_state));

  test_web_state.RemoveWebFrame(main_frame_ptr->GetFrameId());
  // Now there is no main frame.
  EXPECT_EQ(nullptr, GetMainWebFrame(&test_web_state));
}

// Tests the GetMainWebFrameId function.
TEST_F(WebFrameUtilTest, GetMainWebFrameId) {
  TestWebState test_web_state;
  test_web_state.CreateWebFramesManager();
  // Still no main frame.
  EXPECT_TRUE(GetMainWebFrameId(&test_web_state).empty());
  auto iframe =
      std::make_unique<FakeWebFrame>("iframe", false, GURL::EmptyGURL());
  test_web_state.AddWebFrame(std::move(iframe));
  // Still no main frame.
  EXPECT_TRUE(GetMainWebFrameId(&test_web_state).empty());

  auto main_frame =
      std::make_unique<FakeWebFrame>("main_frame", true, GURL::EmptyGURL());
  FakeWebFrame* main_frame_ptr = main_frame.get();
  test_web_state.AddWebFrame(std::move(main_frame));
  // Now there is a main frame.
  EXPECT_EQ("main_frame", GetMainWebFrameId(&test_web_state));

  test_web_state.RemoveWebFrame(main_frame_ptr->GetFrameId());
  // Now there is no main frame.
  EXPECT_TRUE(GetMainWebFrameId(&test_web_state).empty());
}

// Tests the GetWebFrameWithId function.
TEST_F(WebFrameUtilTest, GetWebFrameWithId) {
  TestWebState test_web_state;
  test_web_state.CreateWebFramesManager();
  // Still no main frame.
  EXPECT_EQ(nullptr, GetWebFrameWithId(&test_web_state, "iframe"));
  EXPECT_EQ(nullptr, GetWebFrameWithId(&test_web_state, "main_frame"));
  EXPECT_EQ(nullptr, GetWebFrameWithId(&test_web_state, "unused"));
  auto iframe =
      std::make_unique<FakeWebFrame>("iframe", false, GURL::EmptyGURL());
  FakeWebFrame* iframe_ptr = iframe.get();
  test_web_state.AddWebFrame(std::move(iframe));
  // There is an iframe.
  EXPECT_EQ(iframe_ptr, GetWebFrameWithId(&test_web_state, "iframe"));
  EXPECT_EQ(nullptr, GetWebFrameWithId(&test_web_state, "main_frame"));
  EXPECT_EQ(nullptr, GetWebFrameWithId(&test_web_state, "unused"));

  auto main_frame =
      std::make_unique<FakeWebFrame>("main_frame", true, GURL::EmptyGURL());
  FakeWebFrame* main_frame_ptr = main_frame.get();
  test_web_state.AddWebFrame(std::move(main_frame));
  // Now there is a main frame.
  EXPECT_EQ(iframe_ptr, GetWebFrameWithId(&test_web_state, "iframe"));
  EXPECT_EQ(main_frame_ptr, GetWebFrameWithId(&test_web_state, "main_frame"));
  EXPECT_EQ(nullptr, GetWebFrameWithId(&test_web_state, "unused"));

  test_web_state.RemoveWebFrame(main_frame_ptr->GetFrameId());
  // Now there is only an iframe.
  EXPECT_EQ(iframe_ptr, GetWebFrameWithId(&test_web_state, "iframe"));
  EXPECT_EQ(nullptr, GetWebFrameWithId(&test_web_state, "main_frame"));
  EXPECT_EQ(nullptr, GetWebFrameWithId(&test_web_state, "unused"));

  // Now there nothing left.
  test_web_state.RemoveWebFrame(iframe_ptr->GetFrameId());
  EXPECT_EQ(nullptr, GetWebFrameWithId(&test_web_state, "iframe"));
  EXPECT_EQ(nullptr, GetWebFrameWithId(&test_web_state, "main_frame"));
  EXPECT_EQ(nullptr, GetWebFrameWithId(&test_web_state, "unused"));

  // Test that GetWebFrameWithId returns nullptr for the empty string.
  EXPECT_EQ(nullptr, GetWebFrameWithId(&test_web_state, ""));
}

// Tests the GetWebFrameId GetWebFrameId function.
TEST_F(WebFrameUtilTest, GetWebFrameId) {
  EXPECT_EQ(std::string(), GetWebFrameId(nullptr));
  FakeWebFrame frame("frame", true, GURL::EmptyGURL());
  EXPECT_EQ("frame", GetWebFrameId(&frame));
}

}  // namespace web
