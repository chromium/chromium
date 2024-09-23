// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/video_rvfc/video_frame_callback_requester_impl.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "third_party/blink/renderer/core/page/page_animator.h"

#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/scripted_animation_controller.h"
#include "third_party/blink/renderer/core/html/media/html_media_test_helper.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/platform/testing/empty_web_media_player.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

using testing::_;
using testing::ByMove;
using testing::Invoke;
using testing::Return;

namespace blink {

using VideoFramePresentationMetadata =
    WebMediaPlayer::VideoFramePresentationMetadata;

namespace {

class MockWebMediaPlayer : public EmptyWebMediaPlayer {
 public:
  MOCK_METHOD0(UpdateFrameIfStale, void());
  MOCK_METHOD0(RequestVideoFrameCallback, void());
  MOCK_METHOD0(GetVideoFramePresentationMetadata,
               std::unique_ptr<VideoFramePresentationMetadata>());
};

class MockFunction : public ScriptFunction::Callable {
 public:
  MockFunction() = default;

  MOCK_METHOD2(Call, ScriptValue(ScriptState*, ScriptValue));
};

// Helper class to wrap a VideoFramePresentationData, which can't have a copy
// constructor, due to it having a media::VideoFrameMetadata instance.
class MetadataHelper {
 public:
  static const VideoFramePresentationMetadata& GetDefaultMedatada() {
    return metadata_;
  }

  static std::unique_ptr<VideoFramePresentationMetadata> CopyDefaultMedatada() {
    auto copy = std::make_unique<VideoFramePresentationMetadata>();

    copy->presented_frames = metadata_.presented_frames;
    copy->presentation_time = metadata_.presentation_time;
    copy->expected_display_time = metadata_.expected_display_time;
    copy->width = metadata_.width;
    copy->height = metadata_.height;
    copy->media_time = metadata_.media_time;
    copy->metadata.MergeMetadataFrom(metadata_.metadata);

    return copy;
  }

  // This method should be called by each test, passing in its own
  // DocumentLoadTiming::ReferenceMonotonicTime(). Otherwise, we will run into
  // clamping verification test issues, as described below.
  static void ReinitializeFields(base::TimeTicks now) {
    // We don't want any time ticks be a multiple of 5us, otherwise, we couldn't
    // tell whether or not the implementation clamped their values. Therefore,
    // we manually set the values for a deterministic test, and make sure we
    // have sub-microsecond resolution for those values.

    metadata_.presented_frames = 42;
    metadata_.presentation_time = now + base::Milliseconds(10.1234);
    metadata_.expected_display_time = now + base::Milliseconds(26.3467);
    metadata_.width = 320;
    metadata_.height = 480;
    metadata_.media_time = base::Seconds(3.14);
    metadata_.metadata.processing_time = base::Milliseconds(60.982);
    metadata_.metadata.capture_begin_time = now + base::Milliseconds(5.6785);
    metadata_.metadata.receive_time = now + base::Milliseconds(17.1234);
    metadata_.metadata.rtp_timestamp = 12345;
  }

 private:
  static VideoFramePresentationMetadata metadata_;
};

VideoFramePresentationMetadata MetadataHelper::metadata_;

// Helper class that compares the parameters used when invoking a callback, with
// the reference parameters we expect.
class VfcRequesterParameterVerifierCallback
    : public VideoFrameRequestCallbackCollection::VideoFrameCallback {
 public:
  explicit VfcRequesterParameterVerifierCallback(DocumentLoader* loader)
      : loader_(loader) {}
  ~VfcRequesterParameterVerifierCallback() override = default;

  void Invoke(double now, const VideoFrameCallbackMetadata* metadata) override {
    was_invoked_ = true;
    now_ = now;

    auto expected = MetadataHelper::GetDefaultMedatada();
    EXPECT_EQ(expected.presented_frames, metadata->presentedFrames());
    EXPECT_EQ((unsigned int)expected.width, metadata->width());
    EXPECT_EQ((unsigned int)expected.height, metadata->height());
    EXPECT_EQ(expected.media_time.InSecondsF(), metadata->mediaTime());

    EXPECT_EQ(*expected.metadata.rtp_timestamp, metadata->rtpTimestamp());

    // Verify that values were correctly clamped.
    VerifyTicksClamping(expected.presentation_time,
                        metadata->presentationTime(), "presentation_time");
    VerifyTicksClamping(expected.expected_display_time,
                        metadata->expectedDisplayTime(),
                        "expected_display_time");

    VerifyTicksClamping(*expected.metadata.capture_begin_time,
                        metadata->captureTime(), "capture_time");

    VerifyTicksClamping(*expected.metadata.receive_time,
                        metadata->receiveTime(), "receive_time");

    base::TimeDelta processing_time = *expected.metadata.processing_time;
    EXPECT_EQ(ClampElapsedProcessingTime(processing_time),
              metadata->processingDuration());
    EXPECT_NE(processing_time.InSecondsF(), metadata->processingDuration());
  }

  double last_now() const { return now_; }
  bool was_invoked() const { return was_invoked_; }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(loader_);
    VideoFrameRequestCallbackCollection::VideoFrameCallback::Trace(visitor);
  }

 private:
  void VerifyTicksClamping(base::TimeTicks reference,
                           double actual,
                           std::string name) {
    EXPECT_EQ(TicksToClampedMillisecondsF(reference), actual)
        << name << " was not clamped properly.";
    EXPECT_NE(TicksToMillisecondsF(reference), actual)
        << "Did not successfully test clamping for " << name;
  }

  double TicksToClampedMillisecondsF(base::TimeTicks ticks) {
    return Performance::ClampTimeResolution(
        loader_->GetTiming().MonotonicTimeToZeroBasedDocumentTime(ticks),
        /*cross_origin_isolated_capability_=*/false);
  }

  double TicksToMillisecondsF(base::TimeTicks ticks) {
    return loader_->GetTiming()
        .MonotonicTimeToZeroBasedDocumentTime(ticks)
        .InMillisecondsF();
  }

  static double ClampElapsedProcessingTime(base::TimeDelta time) {
    return time.FloorToMultiple(base::Microseconds(100)).InSecondsF();
  }

  double now_;
  bool was_invoked_ = false;
  const Member<DocumentLoader> loader_;
};

}  // namespace

class VideoFrameCallbackRequesterImplTest : public PageTestBase {
 public:
  virtual void SetUpWebMediaPlayer() {
    auto mock_media_player = std::make_unique<MockWebMediaPlayer>();
    media_player_ = mock_media_player.get();
    SetupPageWithClients(nullptr,
                         MakeGarbageCollected<test::MediaStubLocalFrameClient>(
                             std::move(mock_media_player)),
                         nullptr);
  }

  void SetUp() override {
    SetUpWebMediaPlayer();

    video_ = MakeGarbageCollected<HTMLVideoElement>(GetDocument());
    GetDocument().body()->appendChild(video_);

    video()->SetSrc(AtomicString("http://example.com/foo.mp4"));
    test::RunPendingTasks();
    UpdateAllLifecyclePhasesForTest();
  }

  HTMLVideoElement* video() { return video_.Get(); }

  MockWebMediaPlayer* media_player() { return media_player_; }

  VideoFrameCallbackRequesterImpl& vfc_requester() {
    return VideoFrameCallbackRequesterImpl::From(*video());
  }

  void SimulateFramePresented() { video_->OnRequestVideoFrameCallback(); }

  void SimulateVideoFrameCallback(base::TimeTicks now) {
    PageAnimator::ServiceScriptedAnimations(
        now, {{GetDocument().GetScriptedAnimationController(), false}});
  }

  V8VideoFrameRequestCallback* GetCallback(ScriptState* script_state,
                                           MockFunction* function) {
    return V8VideoFrameRequestCallback::Create(
        MakeGarbageCollected<ScriptFunction>(script_state, function)
            ->V8Function());
  }

  void RegisterCallbackDirectly(
      VfcRequesterParameterVerifierCallback* callback) {
    vfc_requester().RegisterCallbackForTest(callback);
  }

 private:
  Persistent<HTMLVideoElement> video_;

  // Owned by HTMLVideoElementFrameClient.
  raw_ptr<MockWebMediaPlayer, DanglingUntriaged> media_player_;
};

class VideoFrameCallbackRequesterImplNullMediaPlayerTest
    : public VideoFrameCallbackRequesterImplTest {
 public:
  void SetUpWebMediaPlayer() override {
    SetupPageWithClients(nullptr,
                         MakeGarbageCollected<test::MediaStubLocalFrameClient>(
                             std::unique_ptr<MockWebMediaPlayer>(),
                             /* allow_empty_client */ true),
                         nullptr);
  }
};

TEST_F(VideoFrameCallbackRequesterImplTest, VerifyRequestVideoFrameCallback) {
  V8TestingScope scope;

  auto* function = MakeGarbageCollected<MockFunction>();

  // Queuing up a video.rVFC call should propagate to the WebMediaPlayer.
  EXPECT_CALL(*media_player(), RequestVideoFrameCallback()).Times(1);
  vfc_requester().requestVideoFrameCallback(
      GetCallback(scope.GetScriptState(), function));

  testing::Mock::VerifyAndClear(media_player());

  // Callbacks should not be run immediately when a frame is presented.
  EXPECT_CALL(*function, Call(_, _)).Times(0);
  SimulateFramePresented();

  testing::Mock::VerifyAndClear(function);

  // Callbacks should be called during the rendering steps.
  auto metadata = std::make_unique<VideoFramePresentationMetadata>();
  metadata->presented_frames = 1;

  EXPECT_CALL(*function, Call(_, _)).Times(1);
  EXPECT_CALL(*media_player(), GetVideoFramePresentationMetadata())
      .WillOnce(Return(ByMove(std::move(metadata))));
  SimulateVideoFrameCallback(base::TimeTicks::Now());

  testing::Mock::VerifyAndClear(function);
}

TEST_F(VideoFrameCallbackRequesterImplTest,
       VerifyCancelVideoFrameCallback_BeforePresentedFrame) {
  V8TestingScope scope;

  auto* function = MakeGarbageCollected<MockFunction>();

  // Queue and cancel a request before a frame is presented.
  int callback_id = vfc_requester().requestVideoFrameCallback(
      GetCallback(scope.GetScriptState(), function));
  vfc_requester().cancelVideoFrameCallback(callback_id);

  EXPECT_CALL(*function, Call(_, _)).Times(0);
  SimulateFramePresented();
  SimulateVideoFrameCallback(base::TimeTicks::Now());

  testing::Mock::VerifyAndClear(function);
}

TEST_F(VideoFrameCallbackRequesterImplTest,
       VerifyCancelVideoFrameCallback_AfterPresentedFrame) {
  V8TestingScope scope;

  auto* function = MakeGarbageCollected<MockFunction>();

  // Queue a request.
  int callback_id = vfc_requester().requestVideoFrameCallback(
      GetCallback(scope.GetScriptState(), function));
  SimulateFramePresented();

  // The callback should be scheduled for execution, but not yet run.
  EXPECT_CALL(*function, Call(_, _)).Times(0);
  vfc_requester().cancelVideoFrameCallback(callback_id);
  SimulateVideoFrameCallback(base::TimeTicks::Now());

  testing::Mock::VerifyAndClear(function);
}

TEST_F(VideoFrameCallbackRequesterImplTest,
       VerifyClearedMediaPlayerCancelsPendingExecution) {
  V8TestingScope scope;

  auto* function = MakeGarbageCollected<MockFunction>();

  // Queue a request.
  vfc_requester().requestVideoFrameCallback(
      GetCallback(scope.GetScriptState(), function));
  SimulateFramePresented();

  // The callback should be scheduled for execution, but not yet run.
  EXPECT_CALL(*function, Call(_, _)).Times(0);

  // Simulate the HTMLVideoElement getting changing its WebMediaPlayer.
  vfc_requester().OnWebMediaPlayerCleared();

  // This should be a no-op, else we could get metadata for a null frame.
  SimulateVideoFrameCallback(base::TimeTicks::Now());

  testing::Mock::VerifyAndClear(function);
}

TEST_F(VideoFrameCallbackRequesterImplTest, VerifyParameters_WindowRaf) {
  DocumentLoader* loader = GetDocument().Loader();
  DocumentLoadTiming& timing = loader->GetTiming();
  MetadataHelper::ReinitializeFields(timing.ReferenceMonotonicTime());

  auto* callback =
      MakeGarbageCollected<VfcRequesterParameterVerifierCallback>(loader);

  // Register the non-V8 callback.
  RegisterCallbackDirectly(callback);

  EXPECT_CALL(*media_player(), GetVideoFramePresentationMetadata())
      .WillOnce(Return(ByMove(MetadataHelper::CopyDefaultMedatada())));

  const double now_ms =
      timing.MonotonicTimeToZeroBasedDocumentTime(base::TimeTicks::Now())
          .InMillisecondsF();

  // Run the callbacks directly, since they weren't scheduled to be run by the
  // ScriptedAnimationController.
  vfc_requester().OnExecution(now_ms);

  EXPECT_EQ(callback->last_now(), now_ms);
  EXPECT_TRUE(callback->was_invoked());

  testing::Mock::VerifyAndClear(media_player());
}

TEST_F(VideoFrameCallbackRequesterImplTest, OnXrFrameData) {
  V8TestingScope scope;

  // New immersive frames should not drive frame updates if we don't have any
  // pending callbacks.
  EXPECT_CALL(*media_player(), UpdateFrameIfStale()).Times(0);

  vfc_requester().OnImmersiveFrame();

  testing::Mock::VerifyAndClear(media_player());

  auto* function = MakeGarbageCollected<MockFunction>();
  vfc_requester().requestVideoFrameCallback(
      GetCallback(scope.GetScriptState(), function));

  // Immersive frames should trigger video frame updates when there are pending
  // callbacks.
  EXPECT_CALL(*media_player(), UpdateFrameIfStale());

  vfc_requester().OnImmersiveFrame();

  testing::Mock::VerifyAndClear(media_player());
}

TEST_F(VideoFrameCallbackRequesterImplNullMediaPlayerTest, VerifyNoCrash) {
  V8TestingScope scope;

  auto* function = MakeGarbageCollected<MockFunction>();

  vfc_requester().requestVideoFrameCallback(
      GetCallback(scope.GetScriptState(), function));

  SimulateFramePresented();
  SimulateVideoFrameCallback(base::TimeTicks::Now());
}

}  // namespace blink
