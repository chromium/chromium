// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track_underlying_sink.h"

#include "base/memory/scoped_refptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_frame.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_writer.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_sink.h"
#include "third_party/blink/renderer/modules/mediastream/pushable_media_stream_video_source.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"

using testing::_;

namespace blink {

class MediaStreamVideoTrackUnderlyingSinkTest : public testing::Test {
 public:
  MediaStreamVideoTrackUnderlyingSinkTest() {
    pushable_video_source_ = new PushableMediaStreamVideoSource();
    media_stream_source_ = MakeGarbageCollected<MediaStreamSource>(
        "dummy_source_id", MediaStreamSource::kTypeVideo, "dummy_source_name",
        /*remote=*/false);
    media_stream_source_->SetPlatformSource(
        base::WrapUnique(pushable_video_source_));
  }

  ~MediaStreamVideoTrackUnderlyingSinkTest() override {
    platform_->RunUntilIdle();
    WebHeap::CollectAllGarbageForTesting();
  }

  MediaStreamVideoTrackUnderlyingSink* CreateUnderlyingSink(
      ScriptState* script_state) {
    return MakeGarbageCollected<MediaStreamVideoTrackUnderlyingSink>(
        pushable_video_source_);
  }

  WebMediaStreamTrack CreateTrack() {
    return MediaStreamVideoTrack::CreateVideoTrack(
        pushable_video_source_,
        MediaStreamVideoSource::ConstraintsOnceCallback(),
        /*enabled=*/true);
  }

  ScriptValue CreateVideoFrameChunk(ScriptState* script_state) {
    const scoped_refptr<media::VideoFrame> media_frame =
        media::VideoFrame::CreateBlackFrame(gfx::Size(100, 50));
    VideoFrame* video_frame =
        MakeGarbageCollected<VideoFrame>(std::move(media_frame));
    return ScriptValue(script_state->GetIsolate(),
                       ToV8(video_frame, script_state->GetContext()->Global(),
                            script_state->GetIsolate()));
  }

 protected:
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;
  Persistent<MediaStreamSource> media_stream_source_;
  PushableMediaStreamVideoSource* pushable_video_source_;
};

TEST_F(MediaStreamVideoTrackUnderlyingSinkTest,
       WriteToStreamForwardsToMediaStreamSink) {
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

  EXPECT_CALL(media_stream_video_sink, OnVideoFrame(_));
  ScriptPromiseTester write_tester(
      script_state,
      writer->write(script_state, CreateVideoFrameChunk(script_state),
                    exception_state));
  EXPECT_FALSE(write_tester.IsFulfilled());

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
  ScriptValue v8_integer = ScriptValue::From(script_state, 0);

  // Writing something that is not a VideoFrame to the sink should fail.
  DummyExceptionStateForTesting dummy_exception_state;
  sink->write(script_state, v8_integer, nullptr, dummy_exception_state);
  EXPECT_TRUE(dummy_exception_state.HadException());
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

}  // namespace blink
