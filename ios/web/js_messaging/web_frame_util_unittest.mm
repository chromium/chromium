// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/js_messaging/web_frame_util.h"

#include "base/test/gtest_util.h"
#include "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

class WebFrameUtilTest : public PlatformTest {
 public:
  WebFrameUtilTest() {
    auto frames_manager = std::make_unique<FakeWebFramesManager>();
    fake_web_frames_manager_ = frames_manager.get();
    test_web_state_.SetWebFramesManager(std::move(frames_manager));
  }

 protected:
  TestWebState test_web_state_;
  FakeWebFramesManager* fake_web_frames_manager_;
};

// Tests the GetMainWebFrame function.
TEST_F(WebFrameUtilTest, GetMainWebFrame) {
  // Still no main frame.
  EXPECT_EQ(nullptr, test_web_state_.GetWebFramesManager()->GetMainWebFrame());
  auto iframe =
      std::make_unique<FakeWebFrame>("iframe", false, GURL::EmptyGURL());
  fake_web_frames_manager_->AddWebFrame(std::move(iframe));
  // Still no main frame.
  EXPECT_EQ(nullptr, test_web_state_.GetWebFramesManager()->GetMainWebFrame());

  auto main_frame =
      std::make_unique<FakeWebFrame>("main_frame", true, GURL::EmptyGURL());
  FakeWebFrame* main_frame_ptr = main_frame.get();
  fake_web_frames_manager_->AddWebFrame(std::move(main_frame));
  // Now there is a main frame.
  EXPECT_EQ(main_frame_ptr,
            test_web_state_.GetWebFramesManager()->GetMainWebFrame());

  fake_web_frames_manager_->RemoveWebFrame(main_frame_ptr->GetFrameId());
  // Now there is no main frame.
  EXPECT_EQ(nullptr, test_web_state_.GetWebFramesManager()->GetMainWebFrame());
}

// Tests the GetMainWebFrameId function.
TEST_F(WebFrameUtilTest, GetMainWebFrameId) {
  // Still no main frame.
  EXPECT_TRUE(GetMainWebFrameId(&test_web_state_).empty());
  auto iframe =
      std::make_unique<FakeWebFrame>("iframe", false, GURL::EmptyGURL());
  fake_web_frames_manager_->AddWebFrame(std::move(iframe));
  // Still no main frame.
  EXPECT_TRUE(GetMainWebFrameId(&test_web_state_).empty());

  auto main_frame =
      std::make_unique<FakeWebFrame>("main_frame", true, GURL::EmptyGURL());
  FakeWebFrame* main_frame_ptr = main_frame.get();
  fake_web_frames_manager_->AddWebFrame(std::move(main_frame));
  // Now there is a main frame.
  EXPECT_EQ("main_frame", GetMainWebFrameId(&test_web_state_));

  fake_web_frames_manager_->RemoveWebFrame(main_frame_ptr->GetFrameId());
  // Now there is no main frame.
  EXPECT_TRUE(GetMainWebFrameId(&test_web_state_).empty());
}

// Tests the GetWebFrameWithId function.
TEST_F(WebFrameUtilTest, GetWebFrameWithId) {
  // Still no main frame.
  EXPECT_EQ(nullptr, GetWebFrameWithId(&test_web_state_, "iframe"));
  EXPECT_EQ(nullptr, GetWebFrameWithId(&test_web_state_, "main_frame"));
  EXPECT_EQ(nullptr, GetWebFrameWithId(&test_web_state_, "unused"));
  auto iframe =
      std::make_unique<FakeWebFrame>("iframe", false, GURL::EmptyGURL());
  FakeWebFrame* iframe_ptr = iframe.get();
  fake_web_frames_manager_->AddWebFrame(std::move(iframe));
  // There is an iframe.
  EXPECT_EQ(iframe_ptr, GetWebFrameWithId(&test_web_state_, "iframe"));
  EXPECT_EQ(nullptr, GetWebFrameWithId(&test_web_state_, "main_frame"));
  EXPECT_EQ(nullptr, GetWebFrameWithId(&test_web_state_, "unused"));

  auto main_frame =
      std::make_unique<FakeWebFrame>("main_frame", true, GURL::EmptyGURL());
  FakeWebFrame* main_frame_ptr = main_frame.get();
  fake_web_frames_manager_->AddWebFrame(std::move(main_frame));
  // Now there is a main frame.
  EXPECT_EQ(iframe_ptr, GetWebFrameWithId(&test_web_state_, "iframe"));
  EXPECT_EQ(main_frame_ptr, GetWebFrameWithId(&test_web_state_, "main_frame"));
  EXPECT_EQ(nullptr, GetWebFrameWithId(&test_web_state_, "unused"));

  fake_web_frames_manager_->RemoveWebFrame(main_frame_ptr->GetFrameId());
  // Now there is only an iframe.
  EXPECT_EQ(iframe_ptr, GetWebFrameWithId(&test_web_state_, "iframe"));
  EXPECT_EQ(nullptr, GetWebFrameWithId(&test_web_state_, "main_frame"));
  EXPECT_EQ(nullptr, GetWebFrameWithId(&test_web_state_, "unused"));

  // Now there nothing left.
  fake_web_frames_manager_->RemoveWebFrame(iframe_ptr->GetFrameId());
  EXPECT_EQ(nullptr, GetWebFrameWithId(&test_web_state_, "iframe"));
  EXPECT_EQ(nullptr, GetWebFrameWithId(&test_web_state_, "main_frame"));
  EXPECT_EQ(nullptr, GetWebFrameWithId(&test_web_state_, "unused"));

  // Test that GetWebFrameWithId returns nullptr for the empty string.
  EXPECT_EQ(nullptr, GetWebFrameWithId(&test_web_state_, ""));
}

// Tests the GetWebFrameId GetWebFrameId function.
TEST_F(WebFrameUtilTest, GetWebFrameId) {
  EXPECT_EQ(std::string(), GetWebFrameId(nullptr));
  FakeWebFrame frame("frame", true, GURL::EmptyGURL());
  EXPECT_EQ("frame", GetWebFrameId(&frame));
}

// Tests the GetAllWebFrames function.
TEST_F(WebFrameUtilTest, GetAllWebFrames) {
  EXPECT_EQ(0U,
            test_web_state_.GetWebFramesManager()->GetAllWebFrames().size());
  auto main_frame =
      std::make_unique<FakeWebFrame>("main_frame", true, GURL::EmptyGURL());
  FakeWebFrame* main_frame_ptr = main_frame.get();
  fake_web_frames_manager_->AddWebFrame(std::move(main_frame));
  auto iframe =
      std::make_unique<FakeWebFrame>("iframe", false, GURL::EmptyGURL());
  FakeWebFrame* iframe_ptr = iframe.get();
  fake_web_frames_manager_->AddWebFrame(std::move(iframe));
  std::set<WebFrame*> all_frames =
      test_web_state_.GetWebFramesManager()->GetAllWebFrames();
  // Both frames should be returned
  EXPECT_NE(all_frames.end(), all_frames.find(main_frame_ptr));
  EXPECT_NE(all_frames.end(), all_frames.find(iframe_ptr));
  EXPECT_EQ(2U, all_frames.size());
}

}  // namespace web
