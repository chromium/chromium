// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/html_media_element.h"

#include <algorithm>
#include <memory>

#include "base/time/time.h"
#include "base/time/time_override.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_fullscreen_video_status.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html/media/media_controls.h"
#include "third_party/blink/renderer/core/html/media/media_custom_controls_fullscreen_detector.h"
#include "third_party/blink/renderer/core/html/track/text_track.h"
#include "third_party/blink/renderer/core/html/track/text_track_cue_list.h"
#include "third_party/blink/renderer/core/html/track/vtt/vtt_cue.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/testing/empty_web_media_player.h"
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
  FakeWebMediaPlayer(WebMediaPlayerClient* client,
                     ExecutionContext* context,
                     double duration)
      : client_(client), context_(context), duration_(duration) {}

  MOCK_METHOD1(SetIsEffectivelyFullscreen,
               void(blink::WebFullscreenVideoStatus));

  double CurrentTime() const override {
    return current_time_;
  }

  // Establish a large so tests can attempt seeking.
  double Duration() const override { return duration_; }

  WebTimeRanges Seekable() const override {
    WebTimeRange single_range[] = {WebTimeRange(0, Duration())};

    return WebTimeRanges(single_range, 1);
  }

  void Seek(double seconds) override { last_seek_time_ = seconds; }

  void Play() override {
    playing_ = true;
    ScheduleTimeIncrement();
  }
  void Pause() override { playing_ = false; }
  bool Paused() const override { return !playing_; }
  bool IsEnded() const override { return current_time_ == duration_; }

  void FinishSeek() {
    ASSERT_GE(last_seek_time_, 0);
    current_time_ = last_seek_time_;
    last_seek_time_ = -1;

    client_->TimeChanged();
    if (playing_)
      ScheduleTimeIncrement();
  }

  void SetAutoIncrementTimeDelta(std::optional<base::TimeDelta> delta) {
    auto_time_increment_delta_ = delta;
    ScheduleTimeIncrement();
  }

 private:
  void ScheduleTimeIncrement() {
    if (scheduled_time_increment_) {
      return;
    }
    if (!auto_time_increment_delta_.has_value()) {
      return;
    }

    context_->GetTaskRunner(TaskType::kInternalMediaRealTime)
        ->PostDelayedTask(FROM_HERE,
                          WTF::BindOnce(&FakeWebMediaPlayer::AutoTimeIncrement,
                                        WTF::Unretained(this),
                                        auto_time_increment_delta_.value()),
                          auto_time_increment_delta_.value());
    scheduled_time_increment_ = true;
  }

  void AutoTimeIncrement(base::TimeDelta time_delta) {
    // If time increments have been disabled since posting the task, bail out
    if (!auto_time_increment_delta_.has_value() || !playing_) {
      return;
    }

    scheduled_time_increment_ = false;
    current_time_ += time_delta.InSecondsF();

    // Notify the client if we've reached the end of the set duration
    if (current_time_ >= duration_) {
      current_time_ = duration_;
      client_->TimeChanged();
    } else {
      ScheduleTimeIncrement();
    }

    // Run V8 Microtasks (update OfficialPlaybackPosition)
    context_->GetAgent()->event_loop()->PerformMicrotaskCheckpoint();
  }

  WebMediaPlayerClient* client_;
  WeakPersistent<ExecutionContext> context_;
  mutable double current_time_ = 0;
  bool playing_ = false;
  std::optional<base::TimeDelta> auto_time_increment_delta_ =
      base::Milliseconds(33);
  bool scheduled_time_increment_ = false;
  double last_seek_time_ = -1;
  const double duration_;
};

class MediaStubLocalFrameClient : public EmptyLocalFrameClient {
 public:
  std::unique_ptr<WebMediaPlayer> CreateWebMediaPlayer(
      HTMLMediaElement& element,
      const WebMediaPlayerSource&,
      WebMediaPlayerClient* client) override {
    return std::make_unique<FakeWebMediaPlayer>(
        client, element.GetExecutionContext(), media_duration_);
  }

  void SetMediaDuration(double media_duration) {
    media_duration_ = media_duration;
  }

 private:
  double media_duration_ = 1000000;
};

using testing::_;
using testing::AtLeast;
using testing::Return;

}  // anonymous namespace

class HTMLMediaElementEventListenersTest : public PageTestBase {
 protected:
  HTMLMediaElementEventListenersTest() = default;
  HTMLMediaElementEventListenersTest(
      base::test::TaskEnvironment::TimeSource time_source)
      : PageTestBase(time_source) {}
  void SetUp() override {
    SetupPageWithClients(nullptr,
                         MakeGarbageCollected<MediaStubLocalFrameClient>());
  }

  void DestroyDocument() { PageTestBase::TearDown(); }

  HTMLVideoElement* Video() {
    return To<HTMLVideoElement>(
        GetDocument().QuerySelector(AtomicString("video")));
  }

  FakeWebMediaPlayer* WebMediaPlayer() {
    return static_cast<FakeWebMediaPlayer*>(Video()->GetWebMediaPlayer());
  }

  MediaStubLocalFrameClient* LocalFrameClient() {
    return static_cast<MediaStubLocalFrameClient*>(GetFrame().Client());
  }

  void SetMediaDuration(double duration) {
    LocalFrameClient()->SetMediaDuration(duration);
  }

  MediaControls* Controls() { return Video()->GetMediaControls(); }

  void SimulateReadyState(HTMLMediaElement::ReadyState state) {
    Video()->SetReadyState(state);
  }

  void SimulateNetworkState(HTMLMediaElement::NetworkState state) {
    Video()->SetNetworkState(state);
  }

  MediaCustomControlsFullscreenDetector* FullscreenDetector() {
    return Video()->custom_controls_fullscreen_detector_.Get();
  }
};

TEST_F(HTMLMediaElementEventListenersTest, RemovingFromDocumentCollectsAll) {
  EXPECT_EQ(Video(), nullptr);
  GetDocument().body()->setInnerHTML("<video controls></video>");
  EXPECT_NE(Video(), nullptr);
  EXPECT_TRUE(Video()->HasEventListeners());
  EXPECT_NE(Controls(), nullptr);
  EXPECT_TRUE(GetDocument().HasEventListeners());

  WeakPersistent<HTMLVideoElement> weak_persistent_video = Video();
  WeakPersistent<MediaControls> weak_persistent_controls = Controls();
  {
    Persistent<HTMLVideoElement> persistent_video = Video();
    GetDocument().body()->setInnerHTML("");

    // When removed from the document, the event listeners should have been
    // dropped.
    EXPECT_FALSE(GetDocument().HasEventListeners());
    // The video element should still have some event listeners.
    EXPECT_TRUE(persistent_video->HasEventListeners());
  }

  UpdateAllLifecyclePhasesForTest();
  test::RunPendingTasks();

  ThreadState::Current()->CollectAllGarbageForTesting();

  // They have been GC'd.
  EXPECT_EQ(weak_persistent_video, nullptr);
  EXPECT_EQ(weak_persistent_controls, nullptr);
}

TEST_F(HTMLMediaElementEventListenersTest,
       ReInsertingInDocumentCollectsControls) {
  EXPECT_EQ(Video(), nullptr);
  GetDocument().body()->setInnerHTML("<video controls></video>");
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
  EXPECT_EQ(Video(), nullptr);
  GetDocument().body()->setInnerHTML("<video></video>");
  Video()->SetSrc(AtomicString("http://example.com"));

  test::RunPendingTasks();

  EXPECT_NE(WebMediaPlayer(), nullptr);

  // Set ReadyState as HaveMetadata and go fullscreen, so the timer is fired.
  EXPECT_NE(Video(), nullptr);
  SimulateReadyState(HTMLMediaElement::kHaveMetadata);
  LocalFrame::NotifyUserActivation(
      GetDocument().GetFrame(), mojom::UserActivationNotificationType::kTest);
  Fullscreen::RequestFullscreen(*Video());
  Fullscreen::DidResolveEnterFullscreenRequest(GetDocument(),
                                               true /* granted */);

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

static const base::TickClock* s_platform_clock_;

class HTMLMediaElementWithMockSchedulerTest
    : public HTMLMediaElementEventListenersTest {
 protected:
  // We want total control over when to advance the clock. This also allows
  // us to call platform()->RunUntilIdle() to run all pending tasks without
  // fear of looping forever.
  HTMLMediaElementWithMockSchedulerTest()
      : HTMLMediaElementEventListenersTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    EnablePlatform();

    s_platform_clock_ = GetTickClock();

    // DocumentParserTiming has DCHECKS to make sure time > 0.0.
    AdvanceClock(base::Seconds(1));
    // Tests rely on start time being a multiple of 250ms.
    auto start = base::TimeTicks::Now().SnappedToNextTick(base::TimeTicks(),
                                                          base::Seconds(1));
    AdvanceClock(start - base::TimeTicks::Now());

    HTMLMediaElementEventListenersTest::SetUp();
  }

  static base::TimeTicks Now() { return s_platform_clock_->NowTicks(); }

 private:
  std::unique_ptr<base::subtle::ScopedTimeClockOverrides> time_overrides_;
};

TEST_F(HTMLMediaElementWithMockSchedulerTest, OneTimeupdatePerSeek) {
  testing::InSequence dummy;
  GetDocument().body()->setInnerHTML("<video></video>");

  // Set a src to trigger WebMediaPlayer creation.
  Video()->SetSrc(AtomicString("http://example.com"));

  platform()->RunUntilIdle();
  ASSERT_NE(WebMediaPlayer(), nullptr);

  auto* timeupdate_handler = MakeGarbageCollected<MockEventListener>();
  Video()->addEventListener(event_type_names::kTimeupdate, timeupdate_handler);
  testing::Mock::VerifyAndClearExpectations(timeupdate_handler);

  // Simulate conditions where playback is possible.
  SimulateNetworkState(HTMLMediaElement::kNetworkIdle);
  SimulateReadyState(HTMLMediaElement::kHaveFutureData);

  // Simulate advancing playback time.
  WebMediaPlayer()->SetAutoIncrementTimeDelta(base::Milliseconds(33));
  Video()->Play();

  // While playing, timeupdate should fire every 250 ms -> 4x per second as long
  // as media player's CurrentTime continues to advance.
  EXPECT_CALL(*timeupdate_handler, Invoke(_, _)).Times(4);
  FastForwardBy(base::Seconds(1));
  testing::Mock::VerifyAndClearExpectations(timeupdate_handler);

  // If media playback time is fixed, periodic timeupdate's should not continue
  // to fire.
  WebMediaPlayer()->SetAutoIncrementTimeDelta(std::nullopt);
  EXPECT_CALL(*timeupdate_handler, Invoke(_, _)).Times(0);
  FastForwardBy(base::Seconds(1));
  testing::Mock::VerifyAndClearExpectations(timeupdate_handler);

  // Per spec, pausing should fire `timeupdate`
  EXPECT_CALL(*timeupdate_handler, Invoke(_, _)).Times(1);
  Video()->pause();
  platform()->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(timeupdate_handler);

  // Seek to some time in the past. A completed seek while paused should trigger
  // a *single* timeupdate.
  EXPECT_CALL(*timeupdate_handler, Invoke(_, _)).Times(1);

  // The WebMediaPlayer current time should have progressed to almost 1 second
  // (Actually 0.99 due to |kFakeMediaPlayerAutoIncrementTimeDelta|).
  ASSERT_GE(WebMediaPlayer()->CurrentTime(), 0.95);
  Video()->setCurrentTime(0.5);

  // Fake the callback from WebMediaPlayer to complete the seek.
  WebMediaPlayer()->FinishSeek();

  // Give the scheduled timeupdate a chance to fire.
  platform()->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(timeupdate_handler);
}

TEST_F(HTMLMediaElementWithMockSchedulerTest, PeriodicTimeupdateAfterSeek) {
  testing::InSequence dummy;
  GetDocument().body()->setInnerHTML("<video></video>");

  // Set a src to trigger WebMediaPlayer creation.
  Video()->SetSrc(AtomicString("http://example.com"));

  platform()->RunUntilIdle();
  EXPECT_NE(WebMediaPlayer(), nullptr);

  auto* timeupdate_handler = MakeGarbageCollected<MockEventListener>();
  Video()->addEventListener(event_type_names::kTimeupdate, timeupdate_handler);

  // Simulate conditions where playback is possible.
  SimulateNetworkState(HTMLMediaElement::kNetworkIdle);
  SimulateReadyState(HTMLMediaElement::kHaveFutureData);

  // Simulate advancing playback time to enable periodic timeupdates.
  WebMediaPlayer()->SetAutoIncrementTimeDelta(base::Milliseconds(8));
  Video()->Play();

  // Advance a full periodic timeupdate interval (250 ms) and expect a single
  // timeupdate.
  EXPECT_CALL(*timeupdate_handler, Invoke(_, _)).Times(1);
  FastForwardBy(base::Seconds(.250));
  // The event is scheduled, but needs one more scheduler cycle to fire.
  platform()->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(timeupdate_handler);

  // Now advance 125 ms to reach the middle of the periodic timeupdate interval.
  // no additional timeupdate should trigger.
  EXPECT_CALL(*timeupdate_handler, Invoke(_, _)).Times(0);
  FastForwardBy(base::Seconds(.125));
  platform()->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(timeupdate_handler);

  // While still in the middle of the periodic timeupdate interval, start and
  // complete a seek and verify that a *non-periodic* timeupdate is fired.
  EXPECT_CALL(*timeupdate_handler, Invoke(_, _)).Times(1);
  ASSERT_GE(WebMediaPlayer()->CurrentTime(), 0.3);
  Video()->setCurrentTime(0.2);
  WebMediaPlayer()->FinishSeek();

  // Advancing the remainder of the last periodic timeupdate interval should be
  // insufficient to trigger a new timeupdate event because the seek's
  // timeupdate occurred only 125ms ago. We desire to fire periodic timeupdates
  // exactly every 250ms from the last timeupdate, and the seek's timeupdate
  // should reset that 250ms ms countdown.
  EXPECT_CALL(*timeupdate_handler, Invoke(_, _)).Times(0);
  FastForwardBy(base::Seconds(.125));
  platform()->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(timeupdate_handler);

  // Advancing another 125ms, we should expect a new timeupdate because we are
  // now 250ms from the seek's timeupdate.
  EXPECT_CALL(*timeupdate_handler, Invoke(_, _)).Times(1);
  FastForwardBy(base::Seconds(.125));
  platform()->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(timeupdate_handler);

  // Advancing 250ms further, we should expect yet another timeupdate because
  // this represents a full periodic timeupdate interval with no interruptions
  // (e.g. no-seeks).
  EXPECT_CALL(*timeupdate_handler, Invoke(_, _)).Times(1);
  FastForwardBy(base::Seconds(.250));
  platform()->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(timeupdate_handler);
}

TEST_F(HTMLMediaElementWithMockSchedulerTest, ShowPosterFlag_FalseAfterLoop) {
  testing::InSequence dummy;

  // Adjust the duration of the media to something we can reasonably loop
  SetMediaDuration(10.0);

  // Create a looping video with a source
  GetDocument().body()->setInnerHTML(
      "<video loop src=\"http://example.com\"></video>");
  platform()->RunUntilIdle();
  EXPECT_NE(WebMediaPlayer(), nullptr);
  EXPECT_EQ(WebMediaPlayer()->Duration(), 10.0);
  EXPECT_TRUE(Video()->Loop());

  SimulateNetworkState(HTMLMediaElement::kNetworkIdle);
  SimulateReadyState(HTMLMediaElement::kHaveEnoughData);

  // Simulate advancing playback time to enable periodic timeupdates.
  WebMediaPlayer()->SetAutoIncrementTimeDelta(base::Milliseconds(8));
  Video()->Play();

  // Ensure the 'seeking' and 'seeked' events are fired, so we know a loop
  // occurred
  auto* seeking_handler = MakeGarbageCollected<MockEventListener>();
  EXPECT_CALL(*seeking_handler, Invoke(_, _)).Times(1);
  Video()->addEventListener(event_type_names::kSeeking, seeking_handler);
  FastForwardBy(base::Seconds(15));
  testing::Mock::VerifyAndClearExpectations(seeking_handler);

  auto* seeked_handler = MakeGarbageCollected<MockEventListener>();
  EXPECT_CALL(*seeked_handler, Invoke(_, _)).Times(1);
  Video()->addEventListener(event_type_names::kSeeked, seeked_handler);
  WebMediaPlayer()->FinishSeek();
  platform()->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(seeked_handler);

  // ShowPosterFlag should be false after looping
  EXPECT_FALSE(Video()->IsShowPosterFlagSet());
}

TEST_F(HTMLMediaElementWithMockSchedulerTest, ShowPosterFlag_FalseAfterEnded) {
  testing::InSequence dummy;

  // Adjust the duration of the media to something we can reach the end of
  SetMediaDuration(10.0);

  // Create a video with a source
  GetDocument().body()->setInnerHTML(
      "<video src=\"http://example.com\"></video>");
  platform()->RunUntilIdle();
  EXPECT_NE(WebMediaPlayer(), nullptr);
  EXPECT_EQ(WebMediaPlayer()->Duration(), 10.0);

  SimulateNetworkState(HTMLMediaElement::kNetworkIdle);
  SimulateReadyState(HTMLMediaElement::kHaveEnoughData);

  // Simulate advancing playback time to enable periodic timeupdates.
  WebMediaPlayer()->SetAutoIncrementTimeDelta(base::Milliseconds(8));
  Video()->Play();

  // Ensure the 'ended' event is fired
  auto* ended_handler = MakeGarbageCollected<MockEventListener>();
  Video()->addEventListener(event_type_names::kEnded, ended_handler);

  EXPECT_CALL(*ended_handler, Invoke(_, _)).Times(1);
  FastForwardBy(base::Seconds(15));
  testing::Mock::VerifyAndClearExpectations(ended_handler);

  // ShowPosterFlag should be false even after ending
  EXPECT_FALSE(Video()->IsShowPosterFlagSet());
}

struct TestCue {
  double start_time;
  double end_time;
  char const* text;
};

constexpr TestCue kTestCueData[] = {
    {15.000, 17.950, "At the left we can see..."},
    {18.160, 20.080, "At the right we can see the..."},
    {20.110, 21.960, "...the head-snarlers"},
    {21.990, 24.360, "Everything is safe.\nPerfectly safe."},
    {24.580, 27.030, "Emo?"},
    {28.200, 29.990, "Watch out!"},
    {47.030, 48.490, "Are you hurt?"},
    {51.990, 53.940, "I don't think so.\nYou?"},
    {55.160, 56.980, "I'm Ok."},
    {57.110, 61.110, "Get up.\nEmo, it's not safe here."},
    {62.030, 63.570, "Let's go."},
};
constexpr base::TimeDelta kTestCueDataLength = base::Seconds(65);

class CueEventListener final : public NativeEventListener {
 public:
  void Invoke(ExecutionContext* ctx, Event* event) override {
    if (event->type() == event_type_names::kEnter) {
      EXPECT_TRUE(event->target()->GetWrapperTypeInfo()->Equals(
          VTTCue::GetStaticWrapperTypeInfo()));
      auto* const cue = static_cast<VTTCue*>(event->target());
      auto* const media_element = cue->track()->MediaElement();

      OnCueEnter(media_element, cue);
      return;
    } else if (event->type() == event_type_names::kExit) {
      EXPECT_TRUE(event->target()->GetWrapperTypeInfo()->Equals(
          VTTCue::GetStaticWrapperTypeInfo()));
      auto* const cue = static_cast<VTTCue*>(event->target());
      auto* const media_element = cue->track()->MediaElement();

      OnCueExit(media_element, cue);
      return;
    }

    // The above checks should be exhaustive
    FAIL();
  }

  void ExpectAllEventsFiredWithinMargin(base::TimeDelta margin) const {
    for (auto const& delta : cue_event_deltas_) {
      ASSERT_TRUE(delta.enter_time_delta.has_value());
      EXPECT_LE(delta.enter_time_delta.value(), margin);
      EXPECT_GE(delta.enter_time_delta.value(), base::TimeDelta());
      ASSERT_TRUE(delta.exit_time_delta.has_value());
      EXPECT_GE(delta.exit_time_delta.value(), base::TimeDelta());
      EXPECT_LE(delta.exit_time_delta.value(), margin);
    }
  }

 private:
  struct CueChangeEventTimeDelta {
    // The difference between when the cue was scheduled to begin and when the
    // |kEnter| event was fired. The optional will be empty if the |kEnter|
    // event was never fired.
    std::optional<base::TimeDelta> enter_time_delta;

    // The difference between when the cue was scheduled to end and when the
    // |kExit| event fired. The optional will be empty if the |kExit| event
    // was never fired.
    std::optional<base::TimeDelta> exit_time_delta;
  };

  void OnCueEnter(HTMLMediaElement* media_element, VTTCue* cue) {
    auto const cue_index = cue->CueIndex();
    EXPECT_LE(cue_index, cue_event_deltas_.size());
    EXPECT_FALSE(cue_event_deltas_[cue_index].enter_time_delta.has_value());

    // Get the start time delta
    double const diff_seconds = media_element->currentTime() - cue->startTime();
    cue_event_deltas_[cue_index].enter_time_delta = base::Seconds(diff_seconds);
  }

  void OnCueExit(HTMLMediaElement* media_element, VTTCue* cue) {
    auto const cue_index = cue->CueIndex();
    EXPECT_LE(cue_index, cue_event_deltas_.size());
    EXPECT_FALSE(cue_event_deltas_[cue_index].exit_time_delta.has_value());

    // Get the end time delta
    double const diff_seconds =
        std::fabs(media_element->currentTime() - cue->endTime());
    cue_event_deltas_[cue_index].exit_time_delta = base::Seconds(diff_seconds);
  }

  std::array<CueChangeEventTimeDelta, std::size(kTestCueData)>
      cue_event_deltas_;
};

TEST_F(HTMLMediaElementWithMockSchedulerTest, CueEnterExitEventLatency) {
  testing::InSequence dummy;
  GetDocument().body()->setInnerHTML("<video></video>");

  // Set a src to trigger WebMediaPlayer creation.
  Video()->SetSrc(AtomicString("http://example.com"));

  platform()->RunUntilIdle();
  ASSERT_NE(WebMediaPlayer(), nullptr);

  // Create a text track, and fill it with cue data
  auto* text_track =
      Video()->addTextTrack(V8TextTrackKind(V8TextTrackKind::Enum::kSubtitles),
                            g_empty_atom, g_empty_atom, ASSERT_NO_EXCEPTION);

  auto* listener = MakeGarbageCollected<CueEventListener>();
  for (auto cue_data : kTestCueData) {
    VTTCue* cue = MakeGarbageCollected<VTTCue>(
        GetDocument(), cue_data.start_time, cue_data.end_time, cue_data.text);
    text_track->addCue(cue);
    cue->setOnenter(listener);
    cue->setOnexit(listener);
  }

  // Simulate conditions where playback is possible.
  SimulateNetworkState(HTMLMediaElement::kNetworkIdle);
  SimulateReadyState(HTMLMediaElement::kHaveFutureData);

  // Simulate advancing playback time to enable periodic timeupdates.
  WebMediaPlayer()->SetAutoIncrementTimeDelta(base::Milliseconds(8));
  Video()->Play();

  FastForwardBy(kTestCueDataLength);
  platform()->RunUntilIdle();

  // Ensure all cue events fired when expected with a 20ms tolerance
  // As suggested by the spec:
  // https://html.spec.whatwg.org/multipage/media.html#playing-the-media-resource:current-playback-position-13
  listener->ExpectAllEventsFiredWithinMargin(base::Milliseconds(20));
}

}  // namespace blink
