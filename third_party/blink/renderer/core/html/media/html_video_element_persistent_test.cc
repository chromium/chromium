// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/html_video_element.h"

#include <memory>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_fullscreen_options.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

class FullscreenMockChromeClient : public EmptyChromeClient {
 public:
  MOCK_METHOD3(EnterFullscreen,
               void(LocalFrame&,
                    const FullscreenOptions*,
                    FullscreenRequestType));
  MOCK_METHOD1(ExitFullscreen, void(LocalFrame&));
};

using testing::_;
using testing::Sequence;

}  // anonymous namespace

class HTMLVideoElementPersistentTest : public PageTestBase {
 protected:
  void SetUp() override {
    chrome_client_ = MakeGarbageCollected<FullscreenMockChromeClient>();
    PageTestBase::SetupPageWithClients(chrome_client_);
    GetDocument().body()->setInnerHTML(
        "<body><div><video></video></div></body>");
  }

  HTMLVideoElement* VideoElement() {
    return To<HTMLVideoElement>(
        GetDocument().QuerySelector(AtomicString("video")));
  }

  HTMLDivElement* DivElement() {
    return To<HTMLDivElement>(GetDocument().QuerySelector(AtomicString("div")));
  }

  Element* FullscreenElement() {
    return Fullscreen::FullscreenElementFrom(GetDocument());
  }

  FullscreenMockChromeClient& GetMockChromeClient() { return *chrome_client_; }

  void SimulateDidEnterFullscreen() {
    Fullscreen::DidResolveEnterFullscreenRequest(GetDocument(),
                                                 true /* granted */);
  }

  void SimulateDidExitFullscreen() {
    Fullscreen::DidExitFullscreen(GetDocument());
  }

  void SimulateBecamePersistentVideo(bool value) {
    VideoElement()->SetPersistentState(value);
  }

 private:
  Persistent<FullscreenMockChromeClient> chrome_client_;
};

TEST_F(HTMLVideoElementPersistentTest, nothingIsFullscreen) {
  Sequence s;

  EXPECT_EQ(FullscreenElement(), nullptr);

  // Making the video persistent should be a no-op.
  SimulateBecamePersistentVideo(true);
  EXPECT_EQ(FullscreenElement(), nullptr);
  EXPECT_FALSE(VideoElement()->IsPersistent());
  EXPECT_FALSE(DivElement()->ContainsPersistentVideo());
  EXPECT_FALSE(VideoElement()->ContainsPersistentVideo());

  // Making the video not persitent should also be a no-op.
  SimulateBecamePersistentVideo(false);
  EXPECT_EQ(FullscreenElement(), nullptr);
  EXPECT_FALSE(VideoElement()->IsPersistent());
  EXPECT_FALSE(DivElement()->ContainsPersistentVideo());
  EXPECT_FALSE(VideoElement()->ContainsPersistentVideo());
}

TEST_F(HTMLVideoElementPersistentTest, videoIsFullscreen) {
  EXPECT_EQ(FullscreenElement(), nullptr);

  EXPECT_CALL(GetMockChromeClient(), EnterFullscreen(_, _, _)).Times(1);
  EXPECT_CALL(GetMockChromeClient(), ExitFullscreen(_)).Times(0);

  LocalFrame::NotifyUserActivation(
      GetDocument().GetFrame(), mojom::UserActivationNotificationType::kTest);
  Fullscreen::RequestFullscreen(*VideoElement());
  SimulateDidEnterFullscreen();
  EXPECT_EQ(FullscreenElement(), VideoElement());

  // This should be no-op.
  SimulateBecamePersistentVideo(true);
  EXPECT_EQ(FullscreenElement(), VideoElement());
  EXPECT_FALSE(VideoElement()->IsPersistent());
  EXPECT_FALSE(DivElement()->ContainsPersistentVideo());
  EXPECT_FALSE(VideoElement()->ContainsPersistentVideo());

  // This should be no-op.
  SimulateBecamePersistentVideo(false);
  EXPECT_EQ(FullscreenElement(), VideoElement());
  EXPECT_FALSE(VideoElement()->IsPersistent());
  EXPECT_FALSE(DivElement()->ContainsPersistentVideo());
  EXPECT_FALSE(VideoElement()->ContainsPersistentVideo());
}

TEST_F(HTMLVideoElementPersistentTest, divIsFullscreen) {
  EXPECT_EQ(FullscreenElement(), nullptr);

  EXPECT_CALL(GetMockChromeClient(), EnterFullscreen(_, _, _)).Times(1);
  EXPECT_CALL(GetMockChromeClient(), ExitFullscreen(_)).Times(0);

  LocalFrame::NotifyUserActivation(
      GetDocument().GetFrame(), mojom::UserActivationNotificationType::kTest);
  Fullscreen::RequestFullscreen(*DivElement());
  SimulateDidEnterFullscreen();
  EXPECT_EQ(FullscreenElement(), DivElement());

  // Make the video persistent.
  SimulateBecamePersistentVideo(true);
  EXPECT_EQ(FullscreenElement(), DivElement());
  EXPECT_TRUE(VideoElement()->IsPersistent());
  EXPECT_TRUE(DivElement()->ContainsPersistentVideo());
  EXPECT_TRUE(VideoElement()->ContainsPersistentVideo());

  // This should be no-op.
  SimulateBecamePersistentVideo(true);
  EXPECT_EQ(FullscreenElement(), DivElement());
  EXPECT_TRUE(VideoElement()->IsPersistent());
  EXPECT_TRUE(DivElement()->ContainsPersistentVideo());
  EXPECT_TRUE(VideoElement()->ContainsPersistentVideo());

  // Make the video not persistent.
  SimulateBecamePersistentVideo(false);
  EXPECT_EQ(FullscreenElement(), DivElement());
  EXPECT_FALSE(VideoElement()->IsPersistent());
  EXPECT_FALSE(DivElement()->ContainsPersistentVideo());
  EXPECT_FALSE(VideoElement()->ContainsPersistentVideo());
}

TEST_F(HTMLVideoElementPersistentTest, exitFullscreenBeforePersistence) {
  EXPECT_EQ(FullscreenElement(), nullptr);

  EXPECT_CALL(GetMockChromeClient(), EnterFullscreen(_, _, _)).Times(1);
  EXPECT_CALL(GetMockChromeClient(), ExitFullscreen(_)).Times(1);

  LocalFrame::NotifyUserActivation(
      GetDocument().GetFrame(), mojom::UserActivationNotificationType::kTest);
  Fullscreen::RequestFullscreen(*DivElement());
  SimulateDidEnterFullscreen();
  EXPECT_EQ(FullscreenElement(), DivElement());

  SimulateBecamePersistentVideo(true);

  Fullscreen::FullyExitFullscreen(GetDocument());
  SimulateDidExitFullscreen();
  EXPECT_EQ(FullscreenElement(), nullptr);

  // Video persistence states should still apply.
  EXPECT_TRUE(VideoElement()->IsPersistent());
  EXPECT_TRUE(DivElement()->ContainsPersistentVideo());
  EXPECT_TRUE(VideoElement()->ContainsPersistentVideo());

  // Make the video not persistent, cleaned up.
  SimulateBecamePersistentVideo(false);
  EXPECT_FALSE(VideoElement()->IsPersistent());
  EXPECT_FALSE(DivElement()->ContainsPersistentVideo());
  EXPECT_FALSE(VideoElement()->ContainsPersistentVideo());
}

TEST_F(HTMLVideoElementPersistentTest, internalPseudoClassOnlyUAStyleSheet) {
  EXPECT_EQ(FullscreenElement(), nullptr);

  EXPECT_CALL(GetMockChromeClient(), EnterFullscreen(_, _, _)).Times(1);
  EXPECT_CALL(GetMockChromeClient(), ExitFullscreen(_)).Times(0);

  EXPECT_FALSE(DivElement()->matches(AtomicString(":fullscreen")));

  {
    DummyExceptionStateForTesting exception_state;
    EXPECT_FALSE(DivElement()->matches(
        AtomicString(":-internal-video-persistent-ancestor"), exception_state));
    EXPECT_TRUE(exception_state.HadException());
  }
  {
    DummyExceptionStateForTesting exception_state;
    EXPECT_FALSE(VideoElement()->matches(
        AtomicString(":-internal-video-persistent"), exception_state));
    EXPECT_TRUE(exception_state.HadException());
  }
  {
    DummyExceptionStateForTesting exception_state;
    EXPECT_FALSE(VideoElement()->matches(
        AtomicString(":-internal-video-persistent-ancestor"), exception_state));
    EXPECT_TRUE(exception_state.HadException());
  }

  LocalFrame::NotifyUserActivation(
      GetDocument().GetFrame(), mojom::UserActivationNotificationType::kTest);
  Fullscreen::RequestFullscreen(*DivElement());
  SimulateDidEnterFullscreen();
  SimulateBecamePersistentVideo(true);

  EXPECT_EQ(FullscreenElement(), DivElement());
  EXPECT_TRUE(VideoElement()->IsPersistent());
  EXPECT_TRUE(DivElement()->ContainsPersistentVideo());
  EXPECT_TRUE(VideoElement()->ContainsPersistentVideo());

  {
    DummyExceptionStateForTesting exception_state;
    // The :internal-* rules apply only from the UA stylesheet.
    EXPECT_TRUE(DivElement()->matches(AtomicString(":fullscreen")));
    EXPECT_FALSE(DivElement()->matches(
        AtomicString(":-internal-video-persistent-ancestor"), exception_state));
    EXPECT_TRUE(exception_state.HadException());
  }
  {
    DummyExceptionStateForTesting exception_state;
    EXPECT_FALSE(VideoElement()->matches(
        AtomicString(":-internal-video-persistent"), exception_state));
    EXPECT_TRUE(exception_state.HadException());
  }
  {
    DummyExceptionStateForTesting exception_state;
    EXPECT_FALSE(VideoElement()->matches(
        AtomicString(":-internal-video-persistent-ancestor"), exception_state));
    EXPECT_TRUE(exception_state.HadException());
  }
}

TEST_F(HTMLVideoElementPersistentTest, removeContainerWhilePersisting) {
  EXPECT_EQ(FullscreenElement(), nullptr);

  EXPECT_CALL(GetMockChromeClient(), EnterFullscreen(_, _, _)).Times(1);
  EXPECT_CALL(GetMockChromeClient(), ExitFullscreen(_)).Times(1);

  LocalFrame::NotifyUserActivation(
      GetDocument().GetFrame(), mojom::UserActivationNotificationType::kTest);
  Fullscreen::RequestFullscreen(*DivElement());
  SimulateDidEnterFullscreen();
  EXPECT_EQ(FullscreenElement(), DivElement());

  SimulateBecamePersistentVideo(true);
  Persistent<HTMLDivElement> div = DivElement();
  Persistent<HTMLVideoElement> video = VideoElement();
  GetDocument().body()->RemoveChild(DivElement());

  EXPECT_FALSE(video->IsPersistent());
  EXPECT_FALSE(div->ContainsPersistentVideo());
  EXPECT_FALSE(video->ContainsPersistentVideo());
}

TEST_F(HTMLVideoElementPersistentTest, removeVideoWhilePersisting) {
  EXPECT_EQ(FullscreenElement(), nullptr);

  EXPECT_CALL(GetMockChromeClient(), EnterFullscreen(_, _, _)).Times(1);
  EXPECT_CALL(GetMockChromeClient(), ExitFullscreen(_)).Times(0);

  LocalFrame::NotifyUserActivation(
      GetDocument().GetFrame(), mojom::UserActivationNotificationType::kTest);
  Fullscreen::RequestFullscreen(*DivElement());
  SimulateDidEnterFullscreen();
  EXPECT_EQ(FullscreenElement(), DivElement());

  SimulateBecamePersistentVideo(true);
  Persistent<HTMLVideoElement> video = VideoElement();
  DivElement()->RemoveChild(VideoElement());

  EXPECT_FALSE(video->IsPersistent());
  EXPECT_FALSE(DivElement()->ContainsPersistentVideo());
  EXPECT_FALSE(video->ContainsPersistentVideo());
}

TEST_F(HTMLVideoElementPersistentTest, removeVideoWithLayerWhilePersisting) {
  EXPECT_EQ(FullscreenElement(), nullptr);

  // Inserting a <span> between the <div> and <video>.
  Persistent<Element> span =
      GetDocument().CreateRawElement(html_names::kSpanTag);
  DivElement()->AppendChild(span);
  span->AppendChild(VideoElement());

  EXPECT_CALL(GetMockChromeClient(), EnterFullscreen(_, _, _)).Times(1);
  EXPECT_CALL(GetMockChromeClient(), ExitFullscreen(_)).Times(0);

  LocalFrame::NotifyUserActivation(
      GetDocument().GetFrame(), mojom::UserActivationNotificationType::kTest);
  Fullscreen::RequestFullscreen(*DivElement());
  SimulateDidEnterFullscreen();
  EXPECT_EQ(FullscreenElement(), DivElement());

  SimulateBecamePersistentVideo(true);
  Persistent<HTMLVideoElement> video = VideoElement();
  span->RemoveChild(VideoElement());

  EXPECT_FALSE(video->IsPersistent());
  EXPECT_FALSE(DivElement()->ContainsPersistentVideo());
  EXPECT_FALSE(video->ContainsPersistentVideo());
  EXPECT_FALSE(span->ContainsPersistentVideo());
}

TEST_F(HTMLVideoElementPersistentTest, containsPersistentVideoScopedToFS) {
  EXPECT_EQ(FullscreenElement(), nullptr);

  EXPECT_CALL(GetMockChromeClient(), EnterFullscreen(_, _, _)).Times(1);
  EXPECT_CALL(GetMockChromeClient(), ExitFullscreen(_)).Times(0);

  LocalFrame::NotifyUserActivation(
      GetDocument().GetFrame(), mojom::UserActivationNotificationType::kTest);
  Fullscreen::RequestFullscreen(*DivElement());
  SimulateDidEnterFullscreen();
  EXPECT_EQ(FullscreenElement(), DivElement());

  SimulateBecamePersistentVideo(true);
  EXPECT_FALSE(GetDocument().body()->ContainsPersistentVideo());
}

}  // namespace blink
