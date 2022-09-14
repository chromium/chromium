// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/js_messaging/web_frame_util.h"

#import "base/test/gtest_util.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

class WebFrameUtilTest : public PlatformTest {
 public:
  WebFrameUtilTest() {
    auto frames_manager = std::make_unique<FakeWebFramesManager>();
    fake_web_frames_manager_ = frames_manager.get();
    fake_web_state_.SetWebFramesManager(std::move(frames_manager));
  }

 protected:
  FakeWebState fake_web_state_;
  FakeWebFramesManager* fake_web_frames_manager_;
};

// Tests the GetMainFrame function.
TEST_F(WebFrameUtilTest, GetMainFrame) {
  // Still no main frame.
  EXPECT_EQ(nullptr, GetMainFrame(&fake_web_state_));
  auto iframe = FakeWebFrame::CreateChildWebFrame(GURL::EmptyGURL());
  fake_web_frames_manager_->AddWebFrame(std::move(iframe));
  // Still no main frame.
  EXPECT_EQ(nullptr, GetMainFrame(&fake_web_state_));

  auto main_frame = FakeWebFrame::CreateMainWebFrame(GURL::EmptyGURL());
  FakeWebFrame* main_frame_ptr = main_frame.get();
  fake_web_frames_manager_->AddWebFrame(std::move(main_frame));
  // Now there is a main frame.
  EXPECT_EQ(main_frame_ptr, GetMainFrame(&fake_web_state_));

  fake_web_frames_manager_->RemoveWebFrame(main_frame_ptr->GetFrameId());
  // Now there is no main frame.
  EXPECT_EQ(nullptr, GetMainFrame(&fake_web_state_));
}

// Tests the GetMainWebFrameId function.
TEST_F(WebFrameUtilTest, GetMainWebFrameId) {
  // Still no main frame.
  EXPECT_TRUE(GetMainWebFrameId(&fake_web_state_).empty());
  auto iframe = FakeWebFrame::CreateChildWebFrame(GURL::EmptyGURL());
  fake_web_frames_manager_->AddWebFrame(std::move(iframe));
  // Still no main frame.
  EXPECT_TRUE(GetMainWebFrameId(&fake_web_state_).empty());

  auto main_frame = FakeWebFrame::CreateMainWebFrame(GURL::EmptyGURL());
  FakeWebFrame* main_frame_ptr = main_frame.get();
  fake_web_frames_manager_->AddWebFrame(std::move(main_frame));
  // Now there is a main frame.
  EXPECT_EQ(kMainFakeFrameId, GetMainWebFrameId(&fake_web_state_));

  fake_web_frames_manager_->RemoveWebFrame(main_frame_ptr->GetFrameId());
  // Now there is no main frame.
  EXPECT_TRUE(GetMainWebFrameId(&fake_web_state_).empty());
}

// Tests the GetWebFrameWithId function.
TEST_F(WebFrameUtilTest, GetWebFrameWithId) {
  // Still no main frame.
  EXPECT_EQ(nullptr, GetWebFrameWithId(&fake_web_state_, kChildFakeFrameId));
  EXPECT_EQ(nullptr, GetWebFrameWithId(&fake_web_state_, kMainFakeFrameId));
  EXPECT_EQ(nullptr, GetWebFrameWithId(&fake_web_state_, "unused"));
  auto iframe = FakeWebFrame::CreateChildWebFrame(GURL::EmptyGURL());
  FakeWebFrame* iframe_ptr = iframe.get();
  fake_web_frames_manager_->AddWebFrame(std::move(iframe));
  // There is an iframe.
  EXPECT_EQ(iframe_ptr, GetWebFrameWithId(&fake_web_state_, kChildFakeFrameId));
  EXPECT_EQ(nullptr, GetWebFrameWithId(&fake_web_state_, kMainFakeFrameId));
  EXPECT_EQ(nullptr, GetWebFrameWithId(&fake_web_state_, "unused"));

  auto main_frame = FakeWebFrame::CreateMainWebFrame(GURL::EmptyGURL());
  FakeWebFrame* main_frame_ptr = main_frame.get();
  fake_web_frames_manager_->AddWebFrame(std::move(main_frame));
  // Now there is a main frame.
  EXPECT_EQ(iframe_ptr, GetWebFrameWithId(&fake_web_state_, kChildFakeFrameId));
  EXPECT_EQ(main_frame_ptr,
            GetWebFrameWithId(&fake_web_state_, kMainFakeFrameId));
  EXPECT_EQ(nullptr, GetWebFrameWithId(&fake_web_state_, "unused"));

  fake_web_frames_manager_->RemoveWebFrame(main_frame_ptr->GetFrameId());
  // Now there is only an iframe.
  EXPECT_EQ(iframe_ptr, GetWebFrameWithId(&fake_web_state_, kChildFakeFrameId));
  EXPECT_EQ(nullptr, GetWebFrameWithId(&fake_web_state_, kMainFakeFrameId));
  EXPECT_EQ(nullptr, GetWebFrameWithId(&fake_web_state_, "unused"));

  // Now there nothing left.
  fake_web_frames_manager_->RemoveWebFrame(iframe_ptr->GetFrameId());
  EXPECT_EQ(nullptr, GetWebFrameWithId(&fake_web_state_, kChildFakeFrameId));
  EXPECT_EQ(nullptr, GetWebFrameWithId(&fake_web_state_, kMainFakeFrameId));
  EXPECT_EQ(nullptr, GetWebFrameWithId(&fake_web_state_, "unused"));

  // Test that GetWebFrameWithId returns nullptr for the empty string.
  EXPECT_EQ(nullptr, GetWebFrameWithId(&fake_web_state_, ""));
}

// Tests the GetWebFrameId GetWebFrameId function.
TEST_F(WebFrameUtilTest, GetWebFrameId) {
  EXPECT_EQ(std::string(), GetWebFrameId(nullptr));
  auto frame = FakeWebFrame::CreateMainWebFrame(GURL::EmptyGURL());
  EXPECT_EQ(kMainFakeFrameId, GetWebFrameId(frame.get()));
}

// Tests the GetAllWebFrames function.
TEST_F(WebFrameUtilTest, GetAllWebFrames) {
  EXPECT_EQ(0U,
            fake_web_state_.GetWebFramesManager()->GetAllWebFrames().size());
  auto main_frame = FakeWebFrame::CreateMainWebFrame(GURL::EmptyGURL());
  FakeWebFrame* main_frame_ptr = main_frame.get();
  fake_web_frames_manager_->AddWebFrame(std::move(main_frame));
  auto iframe = FakeWebFrame::CreateChildWebFrame(GURL::EmptyGURL());
  FakeWebFrame* iframe_ptr = iframe.get();
  fake_web_frames_manager_->AddWebFrame(std::move(iframe));
  std::set<WebFrame*> all_frames =
      fake_web_state_.GetWebFramesManager()->GetAllWebFrames();
  // Both frames should be returned
  EXPECT_NE(all_frames.end(), all_frames.find(main_frame_ptr));
  EXPECT_NE(all_frames.end(), all_frames.find(iframe_ptr));
  EXPECT_EQ(2U, all_frames.size());
}

}  // namespace web
