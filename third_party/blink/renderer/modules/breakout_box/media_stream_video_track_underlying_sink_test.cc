// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/breakout_box/media_stream_video_track_underlying_sink.h"

#include <cstdint>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "media/base/video_frame.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_frame.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_writer.h"
#include "third_party/blink/renderer/modules/breakout_box/frame_queue_underlying_source.h"
#include "third_party/blink/renderer/modules/breakout_box/pushable_media_stream_video_source.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_sink.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_memory_buffer_test_platform.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_video_frame_pool.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

using testing::_;

namespace blink {

namespace {
// Some of these tests rely on Now() being at least 1 minute.
// If this is not the case, affected tests might flake and should not run.
// TODO(crbug.com/343870500): Remove this once capture timestamps are exposed
// as part of WebCodecs VideoFrame metadata.
bool IsTooEarlyForTest() {
  return (base::TimeTicks::Now() - base::TimeTicks()) <= base::Minutes(2);
}
}  // namespace

class MediaStreamVideoTrackUnderlyingSinkTest : public testing::Test {
 public:
  MediaStreamVideoTrackUnderlyingSinkTest() {
    auto pushable_video_source =
        std::make_unique<PushableMediaStreamVideoSource>(
            scheduler::GetSingleThreadTaskRunnerForTesting());
    pushable_video_source_ = pushable_video_source.get();
    media_stream_source_ = MakeGarbageCollected<MediaStreamSource>(
        "dummy_source_id", MediaStreamSource::kTypeVideo, "dummy_source_name",
        /*remote=*/false, std::move(pushable_video_source));
  }

  ~MediaStreamVideoTrackUnderlyingSinkTest() override {
    platform_->RunUntilIdle();
    WebHeap::CollectAllGarbageForTesting();
  }

  MediaStreamVideoTrackUnderlyingSink* CreateUnderlyingSink(
      ScriptState* script_state) {
    return MakeGarbageCollected<MediaStreamVideoTrackUnderlyingSink>(
        pushable_video_source_->GetBroker());
  }

  WebMediaStreamTrack CreateTrack() {
    return MediaStreamVideoTrack::CreateVideoTrack(
        pushable_video_source_,
        MediaStreamVideoSource::ConstraintsOnceCallback(),
        /*enabled=*/true);
  }

  ScriptValue CreateVideoFrameChunk(
      ScriptState* script_state,
      VideoFrame** video_frame_out = nullptr,
      base::TimeDelta timestamp = base::Seconds(2)) {
    const scoped_refptr<media::VideoFrame> media_frame =
        media::VideoFrame::CreateBlackFrame(gfx::Size(100, 50));
    // Set a nonzero timestamp to make it easier to detect certain errors such
    // as unit conversions in Web-exposed VideoFrames which use integer
    // timestamps.
    media_frame->set_timestamp(timestamp);
    VideoFrame* video_frame = MakeGarbageCollected<VideoFrame>(
        std::move(media_frame), ExecutionContext::From(script_state));
    if (video_frame_out)
      *video_frame_out = video_frame;
    return ScriptValue(script_state->GetIsolate(),
                       ToV8Traits<VideoFrame>::ToV8(script_state, video_frame));
  }

 protected:
  test::TaskEnvironment task_environment_;
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;
  Persistent<MediaStreamSource> media_stream_source_;
  raw_ptr<PushableMediaStreamVideoSource> pushable_video_source_;
};

// TODO(1153092): Test flakes, likely due to completing before background
// thread has had chance to call OnVideoFrame().
TEST_F(MediaStreamVideoTrackUnderlyingSinkTest,
       DISABLED_WriteToStreamForwardsToMediaStreamSink) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* underlying_sink = CreateUnderlyingSink(script_state);
  auto* writable_stream = WritableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_sink, 1u);

  auto track = CreateTrack();
  MockMediaStreamVideoSink media_stream_video_sink;
  media_stream_video_sink.ConnectToTrack(track);

  NonThrowableExceptionState exception_state;
  auto* writer = writable_stream->getWriter(script_state, exception_state);

  VideoFrame* video_frame = nullptr;
  auto video_frame_chunk = CreateVideoFrameChunk(script_state, &video_frame);
  EXPECT_NE(video_frame, nullptr);
  EXPECT_NE(video_frame->frame(), nullptr);
  EXPECT_CALL(media_stream_video_sink, OnVideoFrame(_));
  ScriptPromiseTester write_tester(
      script_state,
      writer->write(script_state, video_frame_chunk, exception_state));
  write_tester.WaitUntilSettled();
  // |video_frame| should be invalidated after sending it to the sink.
  EXPECT_EQ(video_frame->frame(), nullptr);

  writer->releaseLock(script_state);
  ScriptPromiseTester close_tester(
      script_state, writable_stream->close(script_state, exception_state));
  close_tester.WaitUntilSettled();

  // Writing to the sink after the stream closes should fail.
  DummyExceptionStateForTesting dummy_exception_state;
  underlying_sink->write(script_state, CreateVideoFrameChunk(script_state),
                         nullptr, dummy_exception_state);
  EXPECT_TRUE(dummy_exception_state.HadException());
  EXPECT_EQ(dummy_exception_state.Code(),
            static_cast<ExceptionCode>(DOMExceptionCode::kInvalidStateError));
}

TEST_F(MediaStreamVideoTrackUnderlyingSinkTest, WriteInvalidDataFails) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* sink = CreateUnderlyingSink(script_state);
  ScriptValue v8_integer =
      ScriptValue(script_state->GetIsolate(),
                  v8::Integer::New(script_state->GetIsolate(), 0));

  // Writing something that is not a VideoFrame to the sink should fail.
  {
    DummyExceptionStateForTesting dummy_exception_state;
    sink->write(script_state, v8_integer, nullptr, dummy_exception_state);
    EXPECT_TRUE(dummy_exception_state.HadException());
  }

  // Writing a null value to the sink should fail.
  {
    DummyExceptionStateForTesting dummy_exception_state;
    EXPECT_FALSE(dummy_exception_state.HadException());
    sink->write(script_state, ScriptValue::CreateNull(v8_scope.GetIsolate()),
                nullptr, dummy_exception_state);
    EXPECT_TRUE(dummy_exception_state.HadException());
  }

  // Writing a destroyed VideoFrame to the sink should fail.
  {
    DummyExceptionStateForTesting dummy_exception_state;
    VideoFrame* video_frame = nullptr;
    auto chunk = CreateVideoFrameChunk(script_state, &video_frame);
    video_frame->close();
    EXPECT_FALSE(dummy_exception_state.HadException());
    sink->write(script_state, chunk, nullptr, dummy_exception_state);
    EXPECT_TRUE(dummy_exception_state.HadException());
  }
}

TEST_F(MediaStreamVideoTrackUnderlyingSinkTest, WriteToAbortedSinkFails) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* underlying_sink = CreateUnderlyingSink(script_state);
  auto* writable_stream = WritableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_sink, 1u);

  NonThrowableExceptionState exception_state;
  ScriptPromiseTester abort_tester(
      script_state, writable_stream->abort(script_state, exception_state));
  abort_tester.WaitUntilSettled();

  // Writing to the sink after the stream closes should fail.
  DummyExceptionStateForTesting dummy_exception_state;
  underlying_sink->write(script_state, CreateVideoFrameChunk(script_state),
                         nullptr, dummy_exception_state);
  EXPECT_TRUE(dummy_exception_state.HadException());
  EXPECT_EQ(dummy_exception_state.Code(),
            static_cast<ExceptionCode>(DOMExceptionCode::kInvalidStateError));
}

TEST_F(MediaStreamVideoTrackUnderlyingSinkTest, GetGmbManager) {
  ScopedTestingPlatformSupport<GpuMemoryBufferTestPlatform> platform_;
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* underlying_sink = CreateUnderlyingSink(script_state);
  EXPECT_EQ(!!underlying_sink->gmb_manager(),
            WebGraphicsContext3DVideoFramePool::
                IsGpuMemoryBufferReadbackFromTextureEnabled());
}

TEST_F(MediaStreamVideoTrackUnderlyingSinkTest,
       DeltaTimestampDoesNotWriteCaptureBeginTime) {
  if (IsTooEarlyForTest()) {
    return;
  }
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* underlying_sink = CreateUnderlyingSink(script_state);
  auto* writable_stream = WritableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_sink, 1u);
  auto track = CreateTrack();

  NonThrowableExceptionState exception_state;
  auto* writer = writable_stream->getWriter(script_state, exception_state);

  VideoFrame* video_frame = nullptr;
  ScriptValue video_frame_chunk =
      CreateVideoFrameChunk(script_state, &video_frame);
  int64_t web_exposed_timestamp = video_frame->timestamp();
  scoped_refptr<media::VideoFrame> media_frame = video_frame->frame();
  EXPECT_EQ(media_frame->timestamp().InMicroseconds(), web_exposed_timestamp);
  EXPECT_FALSE(media_frame->metadata().capture_begin_time.has_value());

  ScriptPromiseTester write_tester(
      script_state,
      writer->write(script_state, video_frame_chunk, exception_state));
  write_tester.WaitUntilSettled();
  EXPECT_EQ(media_frame->timestamp().InMicroseconds(), web_exposed_timestamp);
  // No capture timestamp expected because the timestamp is a regular TimeDelta.
  EXPECT_FALSE(media_frame->metadata().capture_begin_time.has_value());
}

TEST_F(MediaStreamVideoTrackUnderlyingSinkTest,
       CaptureTimestampWritesCaptureBeginTime) {
  if (IsTooEarlyForTest()) {
    return;
  }
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* underlying_sink = CreateUnderlyingSink(script_state);
  auto* writable_stream = WritableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_sink, 1u);
  auto track = CreateTrack();

  NonThrowableExceptionState exception_state;
  auto* writer = writable_stream->getWriter(script_state, exception_state);

  VideoFrame* video_frame = nullptr;
  ScriptValue video_frame_chunk = CreateVideoFrameChunk(
      script_state, &video_frame,
      /*timestamp=*/base::TimeTicks::Now() - base::TimeTicks());
  int64_t web_exposed_timestamp = video_frame->timestamp();
  scoped_refptr<media::VideoFrame> media_frame = video_frame->frame();
  EXPECT_EQ(media_frame->timestamp().InMicroseconds(), web_exposed_timestamp);
  EXPECT_FALSE(media_frame->metadata().capture_begin_time.has_value());

  ScriptPromiseTester write_tester(
      script_state,
      writer->write(script_state, video_frame_chunk, exception_state));
  write_tester.WaitUntilSettled();
  EXPECT_EQ(media_frame->timestamp().InMicroseconds(), web_exposed_timestamp);
  // Capture timestamp expected because the timestamp looks like a capture time.
  ASSERT_TRUE(media_frame->metadata().capture_begin_time.has_value());
  EXPECT_EQ((*media_frame->metadata().capture_begin_time - base::TimeTicks())
                .InMicroseconds(),
            web_exposed_timestamp);
}

TEST_F(MediaStreamVideoTrackUnderlyingSinkTest,
       DecisionToNotWriteCaptureTimeIsSticky) {
  if (IsTooEarlyForTest()) {
    return;
  }
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* underlying_sink = CreateUnderlyingSink(script_state);
  auto* writable_stream = WritableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_sink, 1u);
  auto track = CreateTrack();

  NonThrowableExceptionState exception_state;
  auto* writer = writable_stream->getWriter(script_state, exception_state);

  // Write a frame with a regular timestamp
  {
    VideoFrame* video_frame = nullptr;
    ScriptValue video_frame_chunk =
        CreateVideoFrameChunk(script_state, &video_frame);
    int64_t web_exposed_timestamp = video_frame->timestamp();
    scoped_refptr<media::VideoFrame> media_frame = video_frame->frame();
    EXPECT_EQ(media_frame->timestamp().InMicroseconds(), web_exposed_timestamp);
    EXPECT_FALSE(media_frame->metadata().capture_begin_time.has_value());

    ScriptPromiseTester write_tester(
        script_state,
        writer->write(script_state, video_frame_chunk, exception_state));
    write_tester.WaitUntilSettled();
    EXPECT_EQ(media_frame->timestamp().InMicroseconds(), web_exposed_timestamp);
    // No capture timestamp expected because the timestamp is a regular
    // TimeDelta.
    EXPECT_FALSE(media_frame->metadata().capture_begin_time.has_value());
  }

  // Write a frame with a timestamp that is a TimeTicks capture time.
  {
    VideoFrame* video_frame = nullptr;
    ScriptValue video_frame_chunk = CreateVideoFrameChunk(
        script_state, &video_frame,
        /*timestamp=*/base::TimeTicks::Now() - base::TimeTicks());
    int64_t web_exposed_timestamp = video_frame->timestamp();
    scoped_refptr<media::VideoFrame> media_frame = video_frame->frame();
    EXPECT_EQ(media_frame->timestamp().InMicroseconds(), web_exposed_timestamp);
    EXPECT_FALSE(media_frame->metadata().capture_begin_time.has_value());

    ScriptPromiseTester write_tester(
        script_state,
        writer->write(script_state, video_frame_chunk, exception_state));
    write_tester.WaitUntilSettled();
    EXPECT_EQ(media_frame->timestamp().InMicroseconds(), web_exposed_timestamp);
    // Capture timestamp not expected despite the TimeTicks timestamp because
    // the decision to not write capture times is sticky
    EXPECT_FALSE(media_frame->metadata().capture_begin_time.has_value());
  }
}

}  // namespace blink
