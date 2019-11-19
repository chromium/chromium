// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/html_media_element.h"

#include <algorithm>
#include <memory>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_fullscreen_video_status.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/dom/user_gesture_indicator.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html/media/media_controls.h"
#include "third_party/blink/renderer/core/html/media/media_custom_controls_fullscreen_detector.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/empty_web_media_player.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

// Most methods are faked rather than mocked. Faking avoids naggy warnings
// about unexpected calls. HTMLMediaElement <-> WebMediaplayer interface is
// highly complex and not generally the focus these tests (with the
// exception of the mocked methods).
class FakeWebMediaPlayer final : public EmptyWebMediaPlayer {
 public:
  FakeWebMediaPlayer(WebMediaPlayerClient* client) : client_(client) {}

  MOCK_METHOD1(SetIsEffectivelyFullscreen,
               void(blink::WebFullscreenVideoStatus));

  double CurrentTime() const override {
    if (auto_advance_current_time_)
      current_time_++;

    return current_time_;
  }

  // Establish a large so tests can attempt seeking.
  double Duration() const override { return 1000000; }

  WebTimeRanges Seekable() const override {
    WebTimeRange single_range[] = {WebTimeRange(0, Duration())};

    return WebTimeRanges(single_range, 1);
  }

  void Seek(double seconds) override { last_seek_time_ = seconds; }

  void FinishSeek() {
    ASSERT_GE(last_seek_time_, 0);
    current_time_ = last_seek_time_;
    last_seek_time_ = -1;

    client_->TimeChanged();
  }

  void SetAutoAdvanceCurrentTime(bool auto_advance) {
    auto_advance_current_time_ = auto_advance;
  }

 private:
  WebMediaPlayerClient* client_;
  mutable double current_time_ = 0;
  bool auto_advance_current_time_ = false;
  double last_seek_time_ = -1;
};

class MediaStubLocalFrameClient : public EmptyLocalFrameClient {
 public:
  std::unique_ptr<WebMediaPlayer> CreateWebMediaPlayer(
      HTMLMediaElement&,
      const WebMediaPlayerSource&,
      WebMediaPlayerClient* client) override {
    return std::make_unique<FakeWebMediaPlayer>(client);
  }
};

using testing::_;
using testing::AtLeast;
using testing::Return;

}  // anonymous namespace

class HTMLMediaElementEventListenersTest : public PageTestBase {
 protected:
  void SetUp() override {
    SetupPageWithClients(nullptr,
                         MakeGarbageCollected<MediaStubLocalFrameClient>());
  }

  void DestroyDocument() { PageTestBase::TearDown(); }
  HTMLVideoElement* Video() {
    return ToHTMLVideoElement(GetDocument().QuerySelector("video"));
  }
  FakeWebMediaPlayer* WebMediaPlayer() {
    return static_cast<FakeWebMediaPlayer*>(Video()->GetWebMediaPlayer());
  }
  MediaControls* Controls() { return Video()->GetMediaControls(); }
  void SimulateReadyState(HTMLMediaElement::ReadyState state) {
    Video()->SetReadyState(state);
  }
  void SimulateNetworkState(HTMLMediaElement::NetworkState state) {
    Video()->SetNetworkState(state);
  }
  MediaCustomControlsFullscreenDetector* FullscreenDetector() {
    return Video()->custom_controls_fullscreen_detector_;
  }
  bool IsCheckViewportIntersectionTimerActive(
      MediaCustomControlsFullscreenDetector* detector) {
    return detector->check_viewport_intersection_timer_.IsActive();
  }
};

TEST_F(HTMLMediaElementEventListenersTest, RemovingFromDocumentCollectsAll) {
  EXPECT_EQ(Video(), nullptr);
  GetDocument().body()->SetInnerHTMLFromString(
      "<body><video controls></video></body>");
  EXPECT_NE(Video(), nullptr);
  EXPECT_TRUE(Video()->HasEventListeners());
  EXPECT_NE(Controls(), nullptr);
  EXPECT_TRUE(GetDocument().HasEventListeners());

  WeakPersistent<HTMLVideoElement> weak_persistent_video = Video();
  WeakPersistent<MediaControls> weak_persistent_controls = Controls();
  {
    Persistent<HTMLVideoElement> persistent_video = Video();
    GetDocument().body()->SetInnerHTMLFromString("");

    // When removed from the document, the event listeners should have been
    // dropped.
    EXPECT_FALSE(GetDocument().HasEventListeners());
    // The video element should still have some event listeners.
    EXPECT_TRUE(persistent_video->HasEventListeners());
  }

  test::RunPendingTasks();

  ThreadState::Current()->CollectAllGarbageForTesting();

  // They have been GC'd.
  EXPECT_EQ(weak_persistent_video, nullptr);
  EXPECT_EQ(weak_persistent_controls, nullptr);
}

TEST_F(HTMLMediaElementEventListenersTest,
       ReInsertingInDocumentCollectsControls) {
  EXPECT_EQ(Video(), nullptr);
  GetDocument().body()->SetInnerHTMLFromString(
      "<body><video controls></video></body>");
  EXPECT_NE(Video(), nullptr);
  EXPECT_TRUE(Video()->HasEventListeners());
  EXPECT_NE(Controls(), nullptr);
  EXPECT_TRUE(GetDocument().HasEventListeners());

  // This should be a no-op. We keep a reference on the VideoElement to avoid an
  // unexpected GC.
  {
    Persistent<HTMLVideoElement> video_holder = Video();
    GetDocument().body()->RemoveChild(Video());
    GetDocument().body()->AppendChild(video_holder.Get());
  }

  EXPECT_TRUE(GetDocument().HasEventListeners());
  EXPECT_TRUE(Video()->HasEventListeners());

  test::RunPendingTasks();

  ThreadState::Current()->CollectAllGarbageForTesting();

  EXPECT_NE(Video(), nullptr);
  EXPECT_NE(Controls(), nullptr);
  EXPECT_EQ(Controls(), Video()->GetMediaControls());
}

TEST_F(HTMLMediaElementEventListenersTest,
       FullscreenDetectorTimerCancelledOnContextDestroy) {
  ScopedVideoFullscreenDetectionForTest video_fullscreen_detection(true);

  EXPECT_EQ(Video(), nullptr);
  GetDocument().body()->SetInnerHTMLFromString("<body><video></video></body>");
  Video()->SetSrc("http://example.com");

  test::RunPendingTasks();

  EXPECT_NE(WebMediaPlayer(), nullptr);

  // Set ReadyState as HaveMetadata and go fullscreen, so the timer is fired.
  EXPECT_NE(Video(), nullptr);
  SimulateReadyState(HTMLMediaElement::kHaveMetadata);
  std::unique_ptr<UserGestureIndicator> gesture_indicator =
      LocalFrame::NotifyUserActivation(GetDocument().GetFrame());
  Fullscreen::RequestFullscreen(*Video());
  Fullscreen::DidEnterFullscreen(GetDocument());

  test::RunPendingTasks();

  Persistent<Document> persistent_document = &GetDocument();
  Persistent<MediaCustomControlsFullscreenDetector> detector =
      FullscreenDetector();

  Vector<blink::WebFullscreenVideoStatus> observed_results;

  ON_CALL(*WebMediaPlayer(), SetIsEffectivelyFullscreen(_))
      .WillByDefault(testing::Invoke(
          [&](blink::WebFullscreenVideoStatus fullscreen_video_status) {
            observed_results.push_back(fullscreen_video_status);
          }));

  DestroyDocument();

  test::RunPendingTasks();

  // Document should not have listeners as the ExecutionContext is destroyed.
  EXPECT_FALSE(persistent_document->HasEventListeners());
  // The timer should be cancelled when the ExecutionContext is destroyed.
  EXPECT_FALSE(IsCheckViewportIntersectionTimerActive(detector));
  // Should only notify the kNotEffectivelyFullscreen value when
  // ExecutionContext is destroyed.
  EXPECT_EQ(1u, observed_results.size());
  EXPECT_EQ(blink::WebFullscreenVideoStatus::kNotEffectivelyFullscreen,
            observed_results[0]);
}

class MockEventListener final : public NativeEventListener {
 public:
  MOCK_METHOD2(Invoke, void(ExecutionContext* executionContext, Event*));
};

class HTMLMediaElementWithMockSchedulerTest
    : public HTMLMediaElementEventListenersTest {
 protected:
  void SetUp() override {
    EnablePlatform();
    // We want total control over when to advance the clock. This also allows
    // us to call platform()->RunUntilIdle() to run all pending tasks without
    // fear of looping forever.
    platform()->SetAutoAdvanceNowToPendingTasks(false);

    // DocumentParserTiming has DCHECKS to make sure time > 0.0.
    platform()->AdvanceClockSeconds(1);

    HTMLMediaElementEventListenersTest::SetUp();
  }
};

TEST_F(HTMLMediaElementWithMockSchedulerTest, OneTimeupdatePerSeek) {
  testing::InSequence dummy;
  GetDocument().body()->SetInnerHTMLFromString("<body><video></video></body>");

  // Set a src to trigger WebMediaPlayer creation.
  Video()->SetSrc("http://example.com");

  platform()->RunUntilIdle();
  ASSERT_NE(WebMediaPlayer(), nullptr);

  auto* timeupdate_handler = MakeGarbageCollected<MockEventListener>();
  Video()->addEventListener(event_type_names::kTimeupdate, timeupdate_handler);

  // Simulate conditions where playback is possible.
  SimulateNetworkState(HTMLMediaElement::kNetworkIdle);
  SimulateReadyState(HTMLMediaElement::kHaveFutureData);

  // Simulate advancing playback time.
  WebMediaPlayer()->SetAutoAdvanceCurrentTime(true);

  Video()->Play();

  // While playing, timeupdate should fire every 250 ms -> 4x per second as long
  // as media player's CurrentTime continues to advance.
  EXPECT_CALL(*timeupdate_handler, Invoke(_, _)).Times(4);
  platform()->RunForPeriodSeconds(1);

  // If media playback time is fixed, periodic timeupdate's should not continue
  // to fire.
  WebMediaPlayer()->SetAutoAdvanceCurrentTime(false);
  EXPECT_CALL(*timeupdate_handler, Invoke(_, _)).Times(0);
  platform()->RunForPeriodSeconds(1);

  EXPECT_CALL(*timeupdate_handler, Invoke(_, _)).Times(1);
  Video()->pause();
  platform()->RunUntilIdle();

  // Seek to some time in the past. A completed seek while paused should trigger
  // a *single* timeupdate.
  EXPECT_CALL(*timeupdate_handler, Invoke(_, _)).Times(1);
  ASSERT_GE(WebMediaPlayer()->CurrentTime(), 1);
  Video()->setCurrentTime(WebMediaPlayer()->CurrentTime() - 1);

  // Fake the callback from WebMediaPlayer to complete the seek.
  WebMediaPlayer()->FinishSeek();

  // Give the scheduled timeupdate a chance to fire.
  platform()->RunUntilIdle();
}

TEST_F(HTMLMediaElementWithMockSchedulerTest, PeriodicTimeupdateAfterSeek) {
  testing::InSequence dummy;
  GetDocument().body()->SetInnerHTMLFromString("<body><video></video></body>");

  // Set a src to trigger WebMediaPlayer creation.
  Video()->SetSrc("http://example.com");

  platform()->RunUntilIdle();
  EXPECT_NE(WebMediaPlayer(), nullptr);

  auto* timeupdate_handler = MakeGarbageCollected<MockEventListener>();
  Video()->addEventListener(event_type_names::kTimeupdate, timeupdate_handler);

  // Simulate conditions where playback is possible.
  SimulateNetworkState(HTMLMediaElement::kNetworkIdle);
  SimulateReadyState(HTMLMediaElement::kHaveFutureData);

  // Simulate advancing playback time to enable periodic timeupdates.
  WebMediaPlayer()->SetAutoAdvanceCurrentTime(true);

  Video()->Play();

  // Advance a full periodic timeupdate interval (250 ms) and expect a single
  // timeupdate.
  EXPECT_CALL(*timeupdate_handler, Invoke(_, _)).Times(1);
  platform()->RunForPeriodSeconds(.250);
  // The event is scheduled, but needs one more scheduler cycle to fire.
  platform()->RunUntilIdle();

  // Now advance 125 ms to reach the middle of the periodic timeupdate interval.
  // no additional timeupdate should trigger.
  EXPECT_CALL(*timeupdate_handler, Invoke(_, _)).Times(0);
  platform()->RunForPeriodSeconds(.125);
  platform()->RunUntilIdle();

  // While still in the middle of the periodic timeupdate interval, start and
  // complete a seek and verify that a *non-periodic* timeupdate is fired.
  EXPECT_CALL(*timeupdate_handler, Invoke(_, _)).Times(1);
  ASSERT_GE(WebMediaPlayer()->CurrentTime(), 1);
  Video()->setCurrentTime(WebMediaPlayer()->CurrentTime() - 1);
  WebMediaPlayer()->FinishSeek();

  // Expect another timeupdate after FinishSeek due to
  // seeking -> begin scrubbing -> pause -> timeupdate.
  EXPECT_CALL(*timeupdate_handler, Invoke(_, _)).Times(1);
  platform()->RunUntilIdle();

  // Advancing the remainder of the last periodic timeupdate interval should be
  // insufficient to trigger a new timeupdate event because the seek's
  // timeupdate occurred only 125ms ago. We desire to fire periodic timeupdates
  // exactly every 250ms from the last timeupdate, and the seek's timeupdate
  // should reset that 250ms ms countdown.
  EXPECT_CALL(*timeupdate_handler, Invoke(_, _)).Times(0);
  platform()->RunForPeriodSeconds(.125);
  platform()->RunUntilIdle();

  // Advancing another 125ms, we should expect a new timeupdate because we are
  // now 250ms from the seek's timeupdate.
  EXPECT_CALL(*timeupdate_handler, Invoke(_, _)).Times(1);
  platform()->RunForPeriodSeconds(.125);
  platform()->RunUntilIdle();

  // Advancing 250ms further, we should expect yet another timeupdate because
  // this represents a full periodic timeupdate interval with no interruptions
  // (e.g. no-seeks).
  EXPECT_CALL(*timeupdate_handler, Invoke(_, _)).Times(1);
  platform()->RunForPeriodSeconds(.250);
  platform()->RunUntilIdle();
}

}  // namespace blink
