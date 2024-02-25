// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediarecorder/media_recorder.h"

#include <memory>

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
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

}  // namespace blink
