// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediarecorder/media_recorder.h"

#include <memory>

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/modules/mediarecorder/blob_event.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track_impl.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_registry.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_source.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component_impl.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {
namespace {

MediaStream* CreateMediaStream(V8TestingScope* scope) {
  auto native_source = std::make_unique<MockMediaStreamVideoSource>();
  MockMediaStreamVideoSource* native_source_ptr = native_source.get();
  auto* source = MakeGarbageCollected<MediaStreamSource>(
      "video source id", MediaStreamSource::kTypeVideo, "video source name",
      false /* remote */, std::move(native_source));
  auto* component = MakeGarbageCollected<MediaStreamComponentImpl>(
      source,
      std::make_unique<MediaStreamVideoTrack>(
          native_source_ptr, MediaStreamVideoSource::ConstraintsOnceCallback(),
          true /* enabled */));
  auto* track = MakeGarbageCollected<MediaStreamTrackImpl>(
      scope->GetExecutionContext(), component);
  return MediaStream::Create(scope->GetExecutionContext(),
                             MediaStreamTrackVector{track});
}
}  // namespace

// This is a regression test for crbug.com/1040339
TEST(MediaRecorderTest,
     AcceptsAllTracksEndedEventWhenExecutionContextDestroyed) {
  test::TaskEnvironment task_environment;
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform;
  {
    V8TestingScope scope;
    MediaStream* stream = CreateMediaStream(&scope);
    MediaRecorder* recorder = MakeGarbageCollected<MediaRecorder>(
        scope.GetExecutionContext(), stream, MediaRecorderOptions::Create(),
        scope.GetExceptionState());
    recorder->start(scope.GetExceptionState());
    for (const auto& track : stream->getTracks())
      track->Component()->GetPlatformTrack()->Stop();
  }
  platform->RunUntilIdle();
  WebHeap::CollectAllGarbageForTesting();
}

// This is a regression test for crbug.com/1179312
TEST(MediaRecorderTest, ReturnsNoPendingActivityAfterRecorderStopped) {
  test::TaskEnvironment task_environment;
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform;
  V8TestingScope scope;
  MediaStream* stream = CreateMediaStream(&scope);
  MediaRecorder* recorder = MakeGarbageCollected<MediaRecorder>(
      scope.GetExecutionContext(), stream, MediaRecorderOptions::Create(),
      scope.GetExceptionState());
  recorder->start(scope.GetExceptionState());
  EXPECT_TRUE(recorder->HasPendingActivity());
  recorder->stop(scope.GetExceptionState());
  EXPECT_FALSE(recorder->HasPendingActivity());
}

TEST(MediaRecorderTest, BlobEventTimecodeIsCoarsened) {
  class TimecodeCaptureListener : public NativeEventListener {
   public:
    explicit TimecodeCaptureListener(base::OnceClosure quit_closure)
        : quit_closure_(std::move(quit_closure)) {}

    void Invoke(ExecutionContext*, Event* event) override {
      if (event->type() == event_type_names::kDataavailable) {
        last_timecode_ = static_cast<BlobEvent*>(event)->timecode();
        if (quit_closure_) {
          std::move(quit_closure_).Run();
        }
      }
    }
    double last_timecode() const { return last_timecode_; }
    void reset(base::OnceClosure quit_closure) {
      quit_closure_ = std::move(quit_closure);
    }

   private:
    double last_timecode_ = 0;
    base::OnceClosure quit_closure_;
  };
  test::TaskEnvironment task_environment(
      test::TaskEnvironment::TimeSource::MOCK_TIME);
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform;
  V8TestingScope scope;
  MediaStream* stream = CreateMediaStream(&scope);
  MediaRecorder* recorder = MakeGarbageCollected<MediaRecorder>(
      scope.GetExecutionContext(), stream, MediaRecorderOptions::Create(),
      scope.GetExceptionState());
  base::RunLoop run_loop1;
  auto* listener =
      MakeGarbageCollected<TimecodeCaptureListener>(run_loop1.QuitClosure());
  recorder->addEventListener(event_type_names::kDataavailable, listener);
  recorder->start(scope.GetExceptionState());
  const base::TimeTicks t0 = base::TimeTicks::Now();
  recorder->WriteData(base::span<const uint8_t>(), /*last_in_slice=*/true,
                      /*error_event=*/nullptr);
  run_loop1.Run();
  EXPECT_EQ(listener->last_timecode(), 0.0);
  base::RunLoop run_loop2;
  listener->reset(run_loop2.QuitClosure());
  task_environment.FastForwardBy(base::Microseconds(110));
  recorder->WriteData(base::span<const uint8_t>(), /*last_in_slice=*/true,
                      /*error_event=*/nullptr);
  run_loop2.Run();

  // To prevent test flakiness, we query the expected coarsened duration using
  // the exact same WindowPerformance instance. Blink's TimeClamper implements
  // security mitigations by applying pseudorandom jitter using a randomized
  // seed (secret_) generated at startup, even in the unit test environment.
  LocalDOMWindow* window = To<LocalDOMWindow>(scope.GetExecutionContext());
  WindowPerformance* performance = DOMWindowPerformance::performance(*window);
  double expected_timecode =
      performance->MonotonicTimeToDOMHighResTimeStamp(t0 +
                                                      base::Microseconds(110)) -
      performance->MonotonicTimeToDOMHighResTimeStamp(t0);
  EXPECT_EQ(listener->last_timecode(), expected_timecode);
}

}  // namespace blink
