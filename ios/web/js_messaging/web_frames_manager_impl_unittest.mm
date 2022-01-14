// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/js_messaging/web_frames_manager_impl.h"

#import "base/strings/string_number_conversions.h"
#include "ios/web/js_messaging/web_frame_impl.h"
#include "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/web_state/web_state_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

class WebFramesManagerImplTest : public WebTestWithWebState {
 protected:
  // Notifies |web_state()| of a newly available |web_frame|.
  void SendFrameBecameAvailableMessage(std::unique_ptr<WebFrame> web_frame) {
    WebStateImpl* web_state_impl = static_cast<WebStateImpl*>(web_state());
    web_state_impl->WebFrameBecameAvailable(std::move(web_frame));
  }

  // Notifies |web_state()| that the web frame with |frame_id| will become
  // unavailable.
  void SendFrameBecameUnavailableMessage(const std::string& frame_id) {
    WebStateImpl* web_state_impl = static_cast<WebStateImpl*>(web_state());
    web_state_impl->WebFrameBecameUnavailable(frame_id);
  }

  WebFramesManagerImpl& GetWebFramesManager() {
    WebStateImpl* web_state_impl = static_cast<WebStateImpl*>(web_state());
    return web_state_impl->GetWebFramesManagerImpl();
  }
};

// Tests main web frame construction/destruction.
TEST_F(WebFramesManagerImplTest, MainWebFrame) {
  auto frame = FakeWebFrame::Create(kMainFakeFrameId,
                                    /*is_main_frame=*/true,
                                    GURL("https://www.main.test"));
  FakeWebFrame* main_frame_ptr = frame.get();

  SendFrameBecameAvailableMessage(std::move(frame));

  EXPECT_EQ(1ul, GetWebFramesManager().GetAllWebFrames().size());
  WebFrame* main_frame = GetWebFramesManager().GetMainWebFrame();
  WebFrame* main_frame_by_id =
      GetWebFramesManager().GetFrameWithId(kMainFakeFrameId);
  ASSERT_TRUE(main_frame);
  ASSERT_TRUE(main_frame_by_id);
  EXPECT_EQ(main_frame, main_frame_by_id);
  EXPECT_EQ(main_frame_ptr, main_frame_by_id);
  EXPECT_TRUE(main_frame->IsMainFrame());
  EXPECT_EQ(main_frame_ptr->GetSecurityOrigin(),
            main_frame->GetSecurityOrigin());

  SendFrameBecameUnavailableMessage(kMainFakeFrameId);
  EXPECT_EQ(0ul, GetWebFramesManager().GetAllWebFrames().size());
  EXPECT_FALSE(GetWebFramesManager().GetMainWebFrame());
  EXPECT_FALSE(GetWebFramesManager().GetFrameWithId(kMainFakeFrameId));
}

// Tests duplicate registration of the main web frame.
TEST_F(WebFramesManagerImplTest, DuplicateMainWebFrame) {
  auto frame = FakeWebFrame::Create(kMainFakeFrameId,
                                    /*is_main_frame=*/true,
                                    GURL("https://www.main.test"));
  FakeWebFrame* main_frame_ptr = frame.get();

  auto second_main_frame = FakeWebFrame::Create(kChildFakeFrameId,
                                                /*is_main_frame=*/true,
                                                GURL("https://www.main2.test"));

  SendFrameBecameAvailableMessage(std::move(frame));
  SendFrameBecameAvailableMessage(std::move(second_main_frame));

  // Validate that |frame| remains the main frame and |second_main_frame| is
  // ignored.
  EXPECT_EQ(1ul, GetWebFramesManager().GetAllWebFrames().size());
  WebFrame* main_frame = GetWebFramesManager().GetMainWebFrame();
  WebFrame* main_frame_by_id =
      GetWebFramesManager().GetFrameWithId(kMainFakeFrameId);
  ASSERT_TRUE(main_frame);
  ASSERT_TRUE(main_frame_by_id);
  EXPECT_EQ(main_frame, main_frame_by_id);
  EXPECT_EQ(main_frame_ptr, main_frame_by_id);
  EXPECT_TRUE(main_frame->IsMainFrame());
  EXPECT_EQ(main_frame_ptr->GetSecurityOrigin(),
            main_frame->GetSecurityOrigin());
}

// Tests WebStateImpl::RemoveAllWebFrames. Removing all frames must go through
// the web state in order to notify observers.
TEST_F(WebFramesManagerImplTest, RemoveAllWebFrames) {
  SendFrameBecameAvailableMessage(FakeWebFrame::Create(
      kMainFakeFrameId,
      /*is_main_frame=*/true, GURL("https://www.main.test")));
  SendFrameBecameAvailableMessage(FakeWebFrame::Create(
      kChildFakeFrameId,
      /*is_main_frame=*/false, GURL("https://www.frame1.test")));
  SendFrameBecameAvailableMessage(FakeWebFrame::Create(
      kChildFakeFrameId2,
      /*is_main_frame=*/false, GURL("https://www.frame2.test")));
  EXPECT_EQ(3ul, GetWebFramesManager().GetAllWebFrames().size());

  WebStateImpl* web_state_impl = static_cast<WebStateImpl*>(web_state());
  web_state_impl->RemoveAllWebFrames();
  EXPECT_EQ(0ul, GetWebFramesManager().GetAllWebFrames().size());
  // Check main frame.
  EXPECT_FALSE(GetWebFramesManager().GetMainWebFrame());
  EXPECT_FALSE(GetWebFramesManager().GetFrameWithId(kMainFakeFrameId));
  // Check frame 1.
  EXPECT_FALSE(GetWebFramesManager().GetFrameWithId(kChildFakeFrameId));
  // Check frame 2.
  EXPECT_FALSE(GetWebFramesManager().GetFrameWithId(kChildFakeFrameId2));
}

// Tests removing a frame which doesn't exist.
TEST_F(WebFramesManagerImplTest, RemoveNonexistantFrame) {
  auto frame = FakeWebFrame::Create(kMainFakeFrameId,
                                    /*is_main_frame=*/true,
                                    GURL("https://www.main.test"));
  FakeWebFrame* main_frame_ptr = frame.get();
  SendFrameBecameAvailableMessage(std::move(frame));

  SendFrameBecameUnavailableMessage(kChildFakeFrameId);
  EXPECT_EQ(1ul, GetWebFramesManager().GetAllWebFrames().size());
  WebFrame* main_frame = GetWebFramesManager().GetMainWebFrame();
  WebFrame* main_frame_by_id =
      GetWebFramesManager().GetFrameWithId(kMainFakeFrameId);
  ASSERT_TRUE(main_frame);
  ASSERT_TRUE(main_frame_by_id);
  EXPECT_EQ(main_frame, main_frame_by_id);
  EXPECT_EQ(main_frame_ptr, main_frame_by_id);
  EXPECT_TRUE(main_frame->IsMainFrame());
  EXPECT_EQ(main_frame_ptr->GetSecurityOrigin(),
            main_frame->GetSecurityOrigin());
}

}  // namespace web
