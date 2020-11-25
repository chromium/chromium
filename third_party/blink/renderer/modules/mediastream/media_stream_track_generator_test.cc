// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_track_generator.h"

#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_track.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_frame.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_writer.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_sink.h"
#include "third_party/blink/renderer/modules/mediastream/pushable_media_stream_video_source.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"

using testing::_;

namespace blink {

namespace {

ScriptValue CreateVideoFrameChunk(ScriptState* script_state) {
  const scoped_refptr<media::VideoFrame> media_frame =
      media::VideoFrame::CreateBlackFrame(gfx::Size(10, 5));
  VideoFrame* video_frame = MakeGarbageCollected<VideoFrame>(
      std::move(media_frame), ExecutionContext::From(script_state));
  return ScriptValue(script_state->GetIsolate(),
                     ToV8(video_frame, script_state->GetContext()->Global(),
                          script_state->GetIsolate()));
}

}  // namespace

class MediaStreamTrackGeneratorTest : public testing::Test {
 public:
  ~MediaStreamTrackGeneratorTest() override {
    platform_->RunUntilIdle();
    WebHeap::CollectAllGarbageForTesting();
  }

 protected:
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;
};

TEST_F(MediaStreamTrackGeneratorTest, VideoFramesAreWritten) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  MediaStreamTrackGenerator* generator = MediaStreamTrackGenerator::Create(
      script_state, "video", v8_scope.GetExceptionState());

  MockMediaStreamVideoSink media_stream_video_sink;
  media_stream_video_sink.ConnectToTrack(
      WebMediaStreamTrack(generator->Component()));
  EXPECT_EQ(media_stream_video_sink.number_of_frames(), 0);
  EXPECT_EQ(media_stream_video_sink.last_frame(), nullptr);

  base::RunLoop sink_loop;
  EXPECT_CALL(media_stream_video_sink, OnVideoFrame(_))
      .WillOnce(base::test::RunOnceClosure(sink_loop.QuitClosure()));
  ExceptionState& exception_state = v8_scope.GetExceptionState();
  auto* writer = generator->writable(script_state)
                     ->getWriter(script_state, exception_state);
  ScriptPromiseTester write_tester(
      script_state,
      writer->write(script_state, CreateVideoFrameChunk(script_state),
                    exception_state));
  EXPECT_FALSE(write_tester.IsFulfilled());
  write_tester.WaitUntilSettled();
  sink_loop.Run();
  EXPECT_TRUE(write_tester.IsFulfilled());
  EXPECT_FALSE(exception_state.HadException());
  EXPECT_EQ(media_stream_video_sink.number_of_frames(), 1);

  // Closing the writable stream should stop the track.
  writer->releaseLock(script_state);
  EXPECT_FALSE(generator->Ended());
  ScriptPromiseTester close_tester(
      script_state,
      generator->writable(script_state)->close(script_state, exception_state));
  close_tester.WaitUntilSettled();
  EXPECT_FALSE(exception_state.HadException());
  EXPECT_TRUE(generator->Ended());
}

TEST_F(MediaStreamTrackGeneratorTest, FramesDoNotFlowOnStoppedGenerator) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  MediaStreamTrackGenerator* generator = MediaStreamTrackGenerator::Create(
      script_state, "video", v8_scope.GetExceptionState());

  MockMediaStreamVideoSink media_stream_video_sink;
  media_stream_video_sink.ConnectToTrack(
      WebMediaStreamTrack(generator->Component()));
  EXPECT_EQ(media_stream_video_sink.number_of_frames(), 0);
  EXPECT_EQ(media_stream_video_sink.last_frame(), nullptr);
  EXPECT_CALL(media_stream_video_sink, OnVideoFrame(_)).Times(0);

  generator->stopTrack(v8_scope.GetExecutionContext());
  EXPECT_TRUE(generator->Ended());

  ExceptionState& exception_state = v8_scope.GetExceptionState();
  auto* writer = generator->writable(script_state)
                     ->getWriter(script_state, exception_state);
  ScriptPromiseTester write_tester(
      script_state,
      writer->write(script_state, CreateVideoFrameChunk(script_state),
                    exception_state));
  write_tester.WaitUntilSettled();
  EXPECT_FALSE(exception_state.HadException());
  EXPECT_EQ(media_stream_video_sink.number_of_frames(), 0);
}

TEST_F(MediaStreamTrackGeneratorTest, Clone) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  MediaStreamTrackGenerator* original = MediaStreamTrackGenerator::Create(
      script_state, "video", v8_scope.GetExceptionState());
  MediaStreamTrack* clone = original->clone(script_state);
  EXPECT_FALSE(original->Ended());
  EXPECT_FALSE(clone->Ended());

  MockMediaStreamVideoSink original_sink;
  original_sink.ConnectToTrack(WebMediaStreamTrack(original->Component()));
  EXPECT_EQ(original_sink.number_of_frames(), 0);
  EXPECT_EQ(original_sink.last_frame(), nullptr);

  MockMediaStreamVideoSink clone_sink;
  clone_sink.ConnectToTrack(WebMediaStreamTrack(clone->Component()));
  EXPECT_EQ(clone_sink.number_of_frames(), 0);
  EXPECT_EQ(clone_sink.last_frame(), nullptr);

  // Writing to the original writes to the clone.
  base::RunLoop original_sink_loop;
  base::RunLoop clone_sink_loop;
  EXPECT_CALL(original_sink, OnVideoFrame(_))
      .WillOnce(base::test::RunOnceClosure(original_sink_loop.QuitClosure()));
  EXPECT_CALL(clone_sink, OnVideoFrame(_))
      .WillOnce(base::test::RunOnceClosure(clone_sink_loop.QuitClosure()));
  ExceptionState& exception_state = v8_scope.GetExceptionState();
  auto* writer = original->writable(script_state)
                     ->getWriter(script_state, exception_state);
  ScriptPromiseTester write_tester(
      script_state,
      writer->write(script_state, CreateVideoFrameChunk(script_state),
                    exception_state));
  write_tester.WaitUntilSettled();
  original_sink_loop.Run();
  clone_sink_loop.Run();
  EXPECT_FALSE(exception_state.HadException());
  EXPECT_EQ(original_sink.number_of_frames(), 1);
  EXPECT_EQ(clone_sink.number_of_frames(), 1);

  // Stopping the original does not stop the clone
  original->stopTrack(v8_scope.GetExecutionContext());
  EXPECT_TRUE(original->Ended());
  EXPECT_FALSE(clone->Ended());

  // The original has not been closed, so writing again writes to the clone,
  // which is still live.
  base::RunLoop clone_sink_loop2;
  EXPECT_CALL(original_sink, OnVideoFrame(_)).Times(0);
  EXPECT_CALL(clone_sink, OnVideoFrame(_))
      .WillOnce(base::test::RunOnceClosure(clone_sink_loop2.QuitClosure()));
  ScriptPromiseTester write_tester2(
      script_state,
      writer->write(script_state, CreateVideoFrameChunk(script_state),
                    exception_state));
  write_tester2.WaitUntilSettled();
  clone_sink_loop2.Run();
  EXPECT_FALSE(exception_state.HadException());
  EXPECT_EQ(original_sink.number_of_frames(), 1);
  EXPECT_EQ(clone_sink.number_of_frames(), 2);

  // After stopping the clone, pushing more frames does not deliver frames.
  clone->stopTrack(v8_scope.GetExecutionContext());
  EXPECT_TRUE(clone->Ended());

  EXPECT_CALL(original_sink, OnVideoFrame(_)).Times(0);
  EXPECT_CALL(clone_sink, OnVideoFrame(_)).Times(0);
  ScriptPromiseTester write_tester3(
      script_state,
      writer->write(script_state, CreateVideoFrameChunk(script_state),
                    exception_state));
  write_tester3.WaitUntilSettled();
  EXPECT_FALSE(exception_state.HadException());
  EXPECT_EQ(original_sink.number_of_frames(), 1);
  EXPECT_EQ(clone_sink.number_of_frames(), 2);
}

TEST_F(MediaStreamTrackGeneratorTest, CloneStopSource) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  MediaStreamTrackGenerator* original = MediaStreamTrackGenerator::Create(
      script_state, "video", v8_scope.GetExceptionState());
  MediaStreamTrack* clone = original->clone(script_state);
  EXPECT_FALSE(original->Ended());
  EXPECT_FALSE(clone->Ended());

  // Closing writable stops the source and therefore ends all connected tracks.
  ScriptPromiseTester close_tester(
      script_state, original->writable(script_state)
                        ->close(script_state, v8_scope.GetExceptionState()));
  close_tester.WaitUntilSettled();
  EXPECT_FALSE(v8_scope.GetExceptionState().HadException());
  EXPECT_TRUE(original->Ended());
  EXPECT_TRUE(clone->Ended());
}

// TODO(crbug.com/1142955): Add support for audio.
TEST_F(MediaStreamTrackGeneratorTest, Audio) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  MediaStreamTrackGenerator* generator = MediaStreamTrackGenerator::Create(
      script_state, "audio", v8_scope.GetExceptionState());
  EXPECT_EQ(generator, nullptr);
  EXPECT_TRUE(v8_scope.GetExceptionState().HadException());
  EXPECT_EQ(static_cast<DOMExceptionCode>(v8_scope.GetExceptionState().Code()),
            DOMExceptionCode::kNotSupportedError);
}

}  // namespace blink
