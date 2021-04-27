// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track_underlying_source.h"

#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
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
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_sink.h"
#include "third_party/blink/renderer/modules/mediastream/pushable_media_stream_video_source.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

using testing::_;

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

  MediaStreamTrack* CreateTrack(ExecutionContext* execution_context) {
    return MakeGarbageCollected<MediaStreamTrack>(
        execution_context,
        MediaStreamVideoTrack::CreateVideoTrack(
            pushable_video_source_,
            MediaStreamVideoSource::ConstraintsOnceCallback(),
            /*enabled=*/true));
  }

  MediaStreamVideoTrackUnderlyingSource* CreateSource(ScriptState* script_state,
                                                      MediaStreamTrack* track,
                                                      wtf_size_t buffer_size) {
    return MakeGarbageCollected<MediaStreamVideoTrackUnderlyingSource>(
        script_state, track->Component(), nullptr, buffer_size);
  }

  MediaStreamVideoTrackUnderlyingSource* CreateSource(ScriptState* script_state,
                                                      MediaStreamTrack* track) {
    return CreateSource(script_state, track, 1u);
  }

 protected:
  void PushFrame(
      const base::Optional<base::TimeDelta>& timestamp = base::nullopt) {
    const scoped_refptr<media::VideoFrame> frame =
        media::VideoFrame::CreateBlackFrame(gfx::Size(10, 5));
    if (timestamp)
      frame->set_timestamp(*timestamp);
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
  auto* track = CreateTrack(v8_scope.GetExecutionContext());
  auto* source = CreateSource(script_state, track);
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
  track->stopTrack(v8_scope.GetExecutionContext());
}

TEST_F(MediaStreamVideoTrackUnderlyingSourceTest,
       CancelStreamDisconnectsFromTrack) {
  V8TestingScope v8_scope;
  MediaStreamTrack* track = CreateTrack(v8_scope.GetExecutionContext());
  MediaStreamVideoTrack* video_track =
      MediaStreamVideoTrack::From(track->Component());
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
  track->stopTrack(v8_scope.GetExecutionContext());
}

TEST_F(MediaStreamVideoTrackUnderlyingSourceTest,
       DropOldFramesWhenQueueIsFull) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* track = CreateTrack(v8_scope.GetExecutionContext());
  const wtf_size_t buffer_size = 5;
  auto* source = CreateSource(script_state, track, buffer_size);
  EXPECT_EQ(source->MaxQueueSize(), buffer_size);
  // Create a stream to ensure there is a controller associated to the source.
  ReadableStream::CreateWithCountQueueingStrategy(script_state, source, 0);
  // Add a sink to the track to make it possible to wait until a pushed frame
  // is delivered to sinks, including |source|, which is a sink of the track.
  MockMediaStreamVideoSink mock_sink;
  mock_sink.ConnectToTrack(WebMediaStreamTrack(source->Track()));
  auto push_frame_sync = [&mock_sink, this](const base::TimeDelta timestamp) {
    base::RunLoop sink_loop;
    EXPECT_CALL(mock_sink, OnVideoFrame(_))
        .WillOnce(base::test::RunOnceClosure(sink_loop.QuitClosure()));
    PushFrame(timestamp);
    sink_loop.Run();
  };

  const auto& queue = source->QueueForTesting();
  for (wtf_size_t i = 0; i < buffer_size; ++i) {
    EXPECT_EQ(queue.size(), i);
    base::TimeDelta timestamp = base::TimeDelta::FromSeconds(i);
    push_frame_sync(timestamp);
    EXPECT_EQ(queue.back()->timestamp(), timestamp);
    EXPECT_EQ(queue.front()->timestamp(), base::TimeDelta::FromSeconds(0));
  }

  // Push another frame while the queue is full.
  EXPECT_EQ(queue.size(), buffer_size);
  push_frame_sync(base::TimeDelta::FromSeconds(buffer_size));

  // Since the queue was full, the oldest frame from the queue should have been
  // dropped.
  EXPECT_EQ(queue.size(), buffer_size);
  EXPECT_EQ(queue.back()->timestamp(),
            base::TimeDelta::FromSeconds(buffer_size));
  EXPECT_EQ(queue.front()->timestamp(), base::TimeDelta::FromSeconds(1));

  // Pulling with frames in the queue should move the oldest frame in the queue
  // to the stream's controller.
  EXPECT_EQ(source->DesiredSizeForTesting(), 0);
  EXPECT_FALSE(source->IsPendingPullForTesting());
  source->pull(script_state);
  EXPECT_EQ(source->DesiredSizeForTesting(), -1);
  EXPECT_FALSE(source->IsPendingPullForTesting());
  EXPECT_EQ(queue.size(), buffer_size - 1);
  EXPECT_EQ(queue.front()->timestamp(), base::TimeDelta::FromSeconds(2));

  source->Close();
  EXPECT_EQ(queue.size(), 0u);
  track->stopTrack(v8_scope.GetExecutionContext());
}

TEST_F(MediaStreamVideoTrackUnderlyingSourceTest,
       BypassQueueAfterPullWithEmptyBuffer) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* track = CreateTrack(v8_scope.GetExecutionContext());
  auto* source = CreateSource(script_state, track);

  // Create a stream to ensure there is a controller associated to the source.
  ReadableStream::CreateWithCountQueueingStrategy(script_state, source, 0);

  MockMediaStreamVideoSink mock_sink;
  mock_sink.ConnectToTrack(WebMediaStreamTrack(source->Track()));
  auto push_frame_sync = [&mock_sink, this]() {
    base::RunLoop sink_loop;
    EXPECT_CALL(mock_sink, OnVideoFrame(_))
        .WillOnce(base::test::RunOnceClosure(sink_loop.QuitClosure()));
    PushFrame();
    sink_loop.Run();
  };

  // At first, the queue is empty and the desired size is empty as well.
  EXPECT_TRUE(source->QueueForTesting().empty());
  EXPECT_EQ(source->DesiredSizeForTesting(), 0);
  EXPECT_FALSE(source->IsPendingPullForTesting());

  source->pull(script_state);
  EXPECT_TRUE(source->QueueForTesting().empty());
  EXPECT_EQ(source->DesiredSizeForTesting(), 0);
  EXPECT_TRUE(source->IsPendingPullForTesting());
  EXPECT_TRUE(source->HasPendingActivity());

  push_frame_sync();
  // Since a pull was pending, the frame is put directly in the stream
  // controller, bypassing the source queue.
  EXPECT_TRUE(source->QueueForTesting().empty());
  EXPECT_EQ(source->DesiredSizeForTesting(), -1);
  EXPECT_FALSE(source->IsPendingPullForTesting());
  EXPECT_FALSE(source->HasPendingActivity());

  source->Close();
  track->stopTrack(v8_scope.GetExecutionContext());
}

TEST_F(MediaStreamVideoTrackUnderlyingSourceTest, QueueSizeCannotBeZero) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* track = CreateTrack(v8_scope.GetExecutionContext());
  auto* source = CreateSource(script_state, track, 0u);
  // Queue size is always at least 1, even if 0 is requested.
  EXPECT_EQ(source->MaxQueueSize(), 1u);
  source->Close();
  track->stopTrack(v8_scope.GetExecutionContext());
}

TEST_F(MediaStreamVideoTrackUnderlyingSourceTest, PlatformSourceAliveAfterGC) {
  Persistent<MediaStreamComponent> component;
  {
    V8TestingScope v8_scope;
    auto* track = CreateTrack(v8_scope.GetExecutionContext());
    component = track->Component();
    auto* source = CreateSource(v8_scope.GetScriptState(), track, 0u);
    ReadableStream::CreateWithCountQueueingStrategy(v8_scope.GetScriptState(),
                                                    source, 0);
    // |source| is a sink of |track|.
    EXPECT_TRUE(source->Track());
  }
  blink::WebHeap::CollectAllGarbageForTesting();
}

}  // namespace blink
