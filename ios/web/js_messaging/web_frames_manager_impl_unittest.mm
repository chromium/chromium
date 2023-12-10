// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/web_frames_manager_impl.h"

#import "base/strings/string_number_conversions.h"
#import "ios/web/js_messaging/web_frame_impl.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/web_state/web_state_impl.h"
#import "testing/gtest/include/gtest/gtest.h"

namespace web {

namespace {

const std::string kLowercaseFrameId = "abba1234beef1234cafe1234deed1234";
const std::string kUppercaseFrameId = "ABBA1234BEEF1234CAFE1234DEED1234";

class FakeWebFramesManagerObserver : public WebFramesManagerImpl::Observer {
 public:
  // The current available frames as tracked by the WebFramesManage Observer
  // calls.
  const std::map<std::string, WebFrame*> frames() const { return frames_; }

  // WebFramesManagerImpl::Observer
  void WebFrameBecameAvailable(WebFramesManager* web_frames_manager,
                               WebFrame* web_frame) override {
    frames_[web_frame->GetFrameId()] = web_frame;
  }

  void WebFrameBecameUnavailable(WebFramesManager* web_frames_manager,
                                 const std::string& frame_id) override {
    frames_.erase(frame_id);
  }

 private:
  std::map<std::string, WebFrame*> frames_;
};

}  // namespace

class WebFramesManagerImplTest : public WebTestWithWebState {
 protected:
  void SetUp() override {
    WebTestWithWebState::SetUp();

    GetPageWorldWebFramesManager().AddObserver(&observer_);
  }

  void TearDown() override {
    GetPageWorldWebFramesManager().RemoveObserver(&observer_);

    WebTestWithWebState::TearDown();
  }

  // Notifies `web_state()` of a newly available `web_frame`.
  void SendFrameBecameAvailableMessage(std::unique_ptr<WebFrame> web_frame) {
    GetPageWorldWebFramesManager().AddFrame(std::move(web_frame));
  }

  // Notifies `web_state()` that the web frame with `frame_id` will become
  // unavailable.
  void SendFrameBecameUnavailableMessage(const std::string& frame_id) {
    GetPageWorldWebFramesManager().RemoveFrameWithId(frame_id);
  }

  WebFramesManagerImpl& GetPageWorldWebFramesManager() {
    WebStateImpl* web_state_impl = WebStateImpl::FromWebState(web_state());
    return web_state_impl->GetWebFramesManagerImpl(
        ContentWorld::kPageContentWorld);
  }

 protected:
  FakeWebFramesManagerObserver observer_;
};

// Tests main web frame construction/destruction.
TEST_F(WebFramesManagerImplTest, MainWebFrame) {
  auto frame = FakeWebFrame::Create(kMainFakeFrameId,
                                    /*is_main_frame=*/true,
                                    GURL("https://www.main.test"));
  FakeWebFrame* main_frame_ptr = frame.get();

  SendFrameBecameAvailableMessage(std::move(frame));

  EXPECT_EQ(1ul, GetPageWorldWebFramesManager().GetAllWebFrames().size());
  WebFrame* main_frame = GetPageWorldWebFramesManager().GetMainWebFrame();
  WebFrame* main_frame_by_id =
      GetPageWorldWebFramesManager().GetFrameWithId(kMainFakeFrameId);
  ASSERT_TRUE(main_frame);
  ASSERT_TRUE(main_frame_by_id);
  EXPECT_EQ(main_frame, main_frame_by_id);
  EXPECT_EQ(main_frame_ptr, main_frame_by_id);
  EXPECT_TRUE(main_frame->IsMainFrame());
  EXPECT_EQ(main_frame_ptr->GetSecurityOrigin(),
            main_frame->GetSecurityOrigin());

  const std::map<std::string, WebFrame*> observed_frames = observer_.frames();
  ASSERT_EQ(1ul, observed_frames.size());
  auto main_frame_it = observed_frames.find(kMainFakeFrameId);
  ASSERT_NE(main_frame_it, observed_frames.end());
  WebFrame* observed_main_frame = main_frame_it->second;
  EXPECT_TRUE(observed_main_frame);
  EXPECT_EQ(main_frame, observed_main_frame);

  SendFrameBecameUnavailableMessage(kMainFakeFrameId);
  EXPECT_EQ(0ul, GetPageWorldWebFramesManager().GetAllWebFrames().size());
  EXPECT_FALSE(GetPageWorldWebFramesManager().GetMainWebFrame());
  EXPECT_FALSE(GetPageWorldWebFramesManager().GetFrameWithId(kMainFakeFrameId));

  EXPECT_EQ(0ul, observer_.frames().size());
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

  // Validate that `frame` remains the main frame and `second_main_frame` is
  // ignored.
  EXPECT_EQ(1ul, GetPageWorldWebFramesManager().GetAllWebFrames().size());
  WebFrame* main_frame = GetPageWorldWebFramesManager().GetMainWebFrame();
  WebFrame* main_frame_by_id =
      GetPageWorldWebFramesManager().GetFrameWithId(kMainFakeFrameId);
  ASSERT_TRUE(main_frame);
  ASSERT_TRUE(main_frame_by_id);
  EXPECT_EQ(main_frame, main_frame_by_id);
  EXPECT_EQ(main_frame_ptr, main_frame_by_id);
  EXPECT_TRUE(main_frame->IsMainFrame());
  EXPECT_EQ(main_frame_ptr->GetSecurityOrigin(),
            main_frame->GetSecurityOrigin());

  const std::map<std::string, WebFrame*> observed_frames = observer_.frames();
  ASSERT_EQ(1ul, observed_frames.size());
  auto main_frame_it = observed_frames.find(kMainFakeFrameId);
  ASSERT_NE(main_frame_it, observed_frames.end());
  WebFrame* observed_main_frame = main_frame_it->second;
  EXPECT_TRUE(observed_main_frame);
  EXPECT_EQ(main_frame, observed_main_frame);
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
  EXPECT_EQ(3ul, GetPageWorldWebFramesManager().GetAllWebFrames().size());

  WebStateImpl* web_state_impl = WebStateImpl::FromWebState(web_state());
  web_state_impl->RemoveAllWebFrames();
  EXPECT_EQ(0ul, GetPageWorldWebFramesManager().GetAllWebFrames().size());
  // Check main frame.
  EXPECT_FALSE(GetPageWorldWebFramesManager().GetMainWebFrame());
  EXPECT_FALSE(GetPageWorldWebFramesManager().GetFrameWithId(kMainFakeFrameId));
  // Check frame 1.
  EXPECT_FALSE(
      GetPageWorldWebFramesManager().GetFrameWithId(kChildFakeFrameId));
  // Check frame 2.
  EXPECT_FALSE(
      GetPageWorldWebFramesManager().GetFrameWithId(kChildFakeFrameId2));

  const std::map<std::string, WebFrame*> observed_frames = observer_.frames();
  ASSERT_EQ(0ul, observed_frames.size());
  // Check main frame.
  auto main_frame_it = observed_frames.find(kMainFakeFrameId);
  ASSERT_EQ(main_frame_it, observed_frames.end());
  // Check frame 1.
  auto frame_1_it = observed_frames.find(kChildFakeFrameId);
  ASSERT_EQ(frame_1_it, observed_frames.end());
  // Check frame 2.
  auto frame_2_it = observed_frames.find(kChildFakeFrameId2);
  ASSERT_EQ(frame_2_it, observed_frames.end());
}

// Tests removing a frame which doesn't exist.
TEST_F(WebFramesManagerImplTest, RemoveNonexistantFrame) {
  auto frame = FakeWebFrame::Create(kMainFakeFrameId,
                                    /*is_main_frame=*/true,
                                    GURL("https://www.main.test"));
  FakeWebFrame* main_frame_ptr = frame.get();
  SendFrameBecameAvailableMessage(std::move(frame));

  SendFrameBecameUnavailableMessage(kChildFakeFrameId);
  EXPECT_EQ(1ul, GetPageWorldWebFramesManager().GetAllWebFrames().size());
  WebFrame* main_frame = GetPageWorldWebFramesManager().GetMainWebFrame();
  WebFrame* main_frame_by_id =
      GetPageWorldWebFramesManager().GetFrameWithId(kMainFakeFrameId);
  ASSERT_TRUE(main_frame);
  ASSERT_TRUE(main_frame_by_id);
  EXPECT_EQ(main_frame, main_frame_by_id);
  EXPECT_EQ(main_frame_ptr, main_frame_by_id);
  EXPECT_TRUE(main_frame->IsMainFrame());
  EXPECT_EQ(main_frame_ptr->GetSecurityOrigin(),
            main_frame->GetSecurityOrigin());

  const std::map<std::string, WebFrame*> observed_frames = observer_.frames();
  ASSERT_EQ(1ul, observed_frames.size());
  auto main_frame_it = observed_frames.find(kMainFakeFrameId);
  ASSERT_NE(main_frame_it, observed_frames.end());
  WebFrame* observed_main_frame = main_frame_it->second;
  EXPECT_TRUE(observed_main_frame);
  EXPECT_EQ(main_frame, observed_main_frame);
}

// Tests that frame lookup is not case-sensitive.
TEST_F(WebFramesManagerImplTest, CaseInsensitiveLookup) {
  auto frame = FakeWebFrame::Create(kLowercaseFrameId,
                                    /*is_main_frame=*/true,
                                    GURL("https://www.main.test"));
  SendFrameBecameAvailableMessage(std::move(frame));

  EXPECT_EQ(1ul, GetPageWorldWebFramesManager().GetAllWebFrames().size());

  WebFrame* frame_by_uppercase_id =
      GetPageWorldWebFramesManager().GetFrameWithId(kUppercaseFrameId);
  EXPECT_TRUE(frame_by_uppercase_id);

  WebFrame* frame_by_lowercase_id =
      GetPageWorldWebFramesManager().GetFrameWithId(kLowercaseFrameId);
  EXPECT_TRUE(frame_by_lowercase_id);

  EXPECT_EQ(frame_by_uppercase_id, frame_by_lowercase_id);
}

// By convention, the frame ID should be stored in lowercase internally, even if
// it was passed as uppercase at construct-time.
TEST_F(WebFramesManagerImplTest, CaseInsensitiveConstruct) {
  auto frame_with_uppercase_id = FakeWebFrame::Create(
      kUppercaseFrameId, /*is_main_frame=*/true, GURL("https://www.main.test"));

  SendFrameBecameAvailableMessage(std::move(frame_with_uppercase_id));

  WebFrame* frame_by_lowercase_id =
      GetPageWorldWebFramesManager().GetFrameWithId(kLowercaseFrameId);
  ASSERT_TRUE(frame_by_lowercase_id);

  EXPECT_EQ(frame_by_lowercase_id->GetFrameId(), kLowercaseFrameId);
}

}  // namespace web
