// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track_underlying_source.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_track.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/pushable_media_stream_video_source.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

namespace blink {

class MediaStreamVideoTrackUnderlyingSourceTest : public testing::Test {
 public:
  MediaStreamVideoTrackUnderlyingSourceTest()
      : media_stream_source_(MakeGarbageCollected<MediaStreamSource>(
            "dummy_source_id",
            MediaStreamSource::kTypeVideo,
            "dummy_source_name",
            false /* remote */)),
        pushable_video_source_(new PushableMediaStreamVideoSource()) {
    media_stream_source_->SetPlatformSource(
        base::WrapUnique(pushable_video_source_));
  }

  ~MediaStreamVideoTrackUnderlyingSourceTest() override {
    platform_->RunUntilIdle();
    WebHeap::CollectAllGarbageForTesting();
  }

  MediaStreamComponent* CreateTrack(ExecutionContext* execution_context) {
    return MakeGarbageCollected<MediaStreamTrack>(
               execution_context,
               MediaStreamVideoTrack::CreateVideoTrack(
                   pushable_video_source_,
                   MediaStreamVideoSource::ConstraintsOnceCallback(),
                   /*enabled=*/true))
        ->Component();
  }

  MediaStreamVideoTrackUnderlyingSource* CreateSource(
      ScriptState* script_state,
      MediaStreamComponent* track) {
    return MakeGarbageCollected<MediaStreamVideoTrackUnderlyingSource>(
        script_state, track);
  }

  MediaStreamVideoTrackUnderlyingSource* CreateSource(
      ScriptState* script_state) {
    MediaStreamComponent* track =
        CreateTrack(ExecutionContext::From(script_state));
    return MakeGarbageCollected<MediaStreamVideoTrackUnderlyingSource>(
        script_state, track);
  }

 protected:
  void PushFrame() {
    const scoped_refptr<media::VideoFrame> frame =
        media::VideoFrame::CreateBlackFrame(gfx::Size(10, 5));
    pushable_video_source_->PushFrame(frame, base::TimeTicks());
    platform_->RunUntilIdle();
  }

  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;
  const Persistent<MediaStreamSource> media_stream_source_;
  PushableMediaStreamVideoSource* const pushable_video_source_;
};

TEST_F(MediaStreamVideoTrackUnderlyingSourceTest,
       VideoFrameFlowsThroughStreamAndCloses) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* source = CreateSource(script_state);
  auto* stream =
      ReadableStream::CreateWithCountQueueingStrategy(script_state, source, 0);

  NonThrowableExceptionState exception_state;
  auto* reader =
      stream->GetDefaultReaderForTesting(script_state, exception_state);

  ScriptPromiseTester read_tester(script_state,
                                  reader->read(script_state, exception_state));
  EXPECT_FALSE(read_tester.IsFulfilled());
  PushFrame();
  read_tester.WaitUntilSettled();
  EXPECT_TRUE(read_tester.IsFulfilled());

  source->Close();
}

TEST_F(MediaStreamVideoTrackUnderlyingSourceTest,
       CancelStreamDisconnectsFromTrack) {
  V8TestingScope v8_scope;
  MediaStreamComponent* track = CreateTrack(v8_scope.GetExecutionContext());
  MediaStreamVideoTrack* video_track = MediaStreamVideoTrack::From(track);
  // Initially the track has no sinks.
  EXPECT_EQ(video_track->CountSinks(), 0u);

  auto* source = CreateSource(v8_scope.GetScriptState(), track);
  auto* stream = ReadableStream::CreateWithCountQueueingStrategy(
      v8_scope.GetScriptState(), source, 0);

  // The stream is a sink to the track.
  EXPECT_EQ(video_track->CountSinks(), 1u);

  NonThrowableExceptionState exception_state;
  stream->cancel(v8_scope.GetScriptState(), exception_state);

  // Canceling the stream disconnects it from the track.
  EXPECT_EQ(video_track->CountSinks(), 0u);
}

// crbug.com/1153092: flaky on several platforms.
TEST_F(MediaStreamVideoTrackUnderlyingSourceTest, DISABLED_FramesAreDropped) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* source = CreateSource(script_state);
  // Create a stream, to ensure there is a controller associated to the source.
  ReadableStream::CreateWithCountQueueingStrategy(v8_scope.GetScriptState(),
                                                  source, 0);
  // The controller initially has no frames.
  EXPECT_EQ(source->Controller()->DesiredSize(), 0);

  // Push a frame. DesiredSize() decreases since the frame is not consumed.
  PushFrame();
  EXPECT_EQ(source->Controller()->DesiredSize(), -1);

  // Push an extra frame. DesiredSize() does not change this time because the
  // frame is dropped.
  PushFrame();
  EXPECT_EQ(source->Controller()->DesiredSize(), -1);

  source->Close();
}

}  // namespace blink
