// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_track_processor.h"

#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_reader.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track_generator.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_sink.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_source.h"
#include "third_party/blink/renderer/modules/mediastream/pushable_media_stream_video_source.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"

using testing::_;

namespace blink {

namespace {

PushableMediaStreamVideoSource* CreatePushableVideoSource() {
  PushableMediaStreamVideoSource* pushable_video_source =
      new PushableMediaStreamVideoSource();
  MediaStreamSource* media_stream_source =
      MakeGarbageCollected<MediaStreamSource>(
          "source_id", MediaStreamSource::kTypeVideo, "source_name",
          /*remote=*/false);
  media_stream_source->SetPlatformSource(
      base::WrapUnique(pushable_video_source));
  return pushable_video_source;
}

MediaStreamTrack* CreateVideoMediaStreamTrack(ExecutionContext* context,
                                              MediaStreamVideoSource* source) {
  return MakeGarbageCollected<MediaStreamTrack>(
      context, MediaStreamVideoTrack::CreateVideoTrack(
                   source, MediaStreamVideoSource::ConstraintsOnceCallback(),
                   /*enabled=*/true));
}

MediaStreamTrack* CreateAudioMediaStreamTrack(ExecutionContext* context) {
  std::unique_ptr<MediaStreamAudioSource> audio_source =
      std::make_unique<MediaStreamAudioSource>(
          blink::scheduler::GetSingleThreadTaskRunnerForTesting(),
          /*is_local_source=*/false);
  MediaStreamSource* media_stream_source =
      MakeGarbageCollected<MediaStreamSource>(
          "source_id", MediaStreamSource::kTypeAudio, "source_name",
          /*is_remote=*/false);
  media_stream_source->SetPlatformSource(std::move(audio_source));
  std::unique_ptr<MediaStreamAudioTrack> audio_track =
      std::make_unique<MediaStreamAudioTrack>(/*is_local_track=*/false);
  MediaStreamComponent* component =
      MakeGarbageCollected<MediaStreamComponent>(media_stream_source);
  component->SetPlatformTrack(std::move(audio_track));
  return MakeGarbageCollected<MediaStreamTrack>(context, component);
}

}  // namespace

class MediaStreamTrackProcessorTest : public testing::Test {
 public:
  ~MediaStreamTrackProcessorTest() override {
    platform_->RunUntilIdle();
    WebHeap::CollectAllGarbageForTesting();
  }

 protected:
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;
};

TEST_F(MediaStreamTrackProcessorTest, VideoFramesAreExposed) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();
  PushableMediaStreamVideoSource* pushable_video_source =
      CreatePushableVideoSource();
  MediaStreamTrackProcessor* track_processor =
      MediaStreamTrackProcessor::Create(
          script_state,
          CreateVideoMediaStreamTrack(v8_scope.GetExecutionContext(),
                                      pushable_video_source),
          exception_state);
  EXPECT_FALSE(exception_state.HadException());
  EXPECT_EQ(track_processor->input_track()->Source()->GetPlatformSource(),
            pushable_video_source);

  MockMediaStreamVideoSink mock_video_sink;
  mock_video_sink.ConnectToTrack(
      WebMediaStreamTrack(track_processor->input_track()));
  EXPECT_EQ(mock_video_sink.number_of_frames(), 0);
  EXPECT_EQ(mock_video_sink.last_frame(), nullptr);

  auto* reader =
      track_processor->readable(script_state)
          ->GetDefaultReaderForTesting(script_state, exception_state);
  EXPECT_FALSE(exception_state.HadException());

  // Deliver a frame
  base::RunLoop sink_loop;
  EXPECT_CALL(mock_video_sink, OnVideoFrame(_))
      .WillOnce(base::test::RunOnceClosure(sink_loop.QuitClosure()));
  scoped_refptr<media::VideoFrame> frame =
      media::VideoFrame::CreateBlackFrame(gfx::Size(10, 5));
  pushable_video_source->PushFrame(frame, base::TimeTicks());

  ScriptPromiseTester read_tester(script_state,
                                  reader->read(script_state, exception_state));
  EXPECT_FALSE(read_tester.IsFulfilled());
  read_tester.WaitUntilSettled();
  EXPECT_FALSE(exception_state.HadException());
  EXPECT_TRUE(read_tester.IsFulfilled());
  EXPECT_TRUE(read_tester.Value().IsObject());
  sink_loop.Run();
  EXPECT_EQ(mock_video_sink.number_of_frames(), 1);
  EXPECT_EQ(mock_video_sink.last_frame(), frame);
}

TEST_F(MediaStreamTrackProcessorTest, CanceledReadableDisconnects) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  PushableMediaStreamVideoSource* pushable_video_source =
      CreatePushableVideoSource();
  ExceptionState& exception_state = v8_scope.GetExceptionState();
  MediaStreamTrackProcessor* track_processor =
      MediaStreamTrackProcessor::Create(
          script_state,
          CreateVideoMediaStreamTrack(v8_scope.GetExecutionContext(),
                                      pushable_video_source),
          exception_state);

  // Initially the track has no sinks.
  MediaStreamVideoTrack* video_track =
      MediaStreamVideoTrack::From(track_processor->input_track());
  EXPECT_EQ(video_track->CountSinks(), 0u);

  MockMediaStreamVideoSink mock_video_sink;
  mock_video_sink.ConnectToTrack(
      WebMediaStreamTrack(track_processor->input_track()));
  EXPECT_EQ(mock_video_sink.number_of_frames(), 0);
  EXPECT_EQ(mock_video_sink.last_frame(), nullptr);
  EXPECT_EQ(video_track->CountSinks(), 1u);

  // Accessing the readable connects it to the track
  auto* readable = track_processor->readable(script_state);
  EXPECT_EQ(video_track->CountSinks(), 2u);

  ScriptPromiseTester cancel_tester(
      script_state, readable->cancel(script_state, exception_state));
  cancel_tester.WaitUntilSettled();
  EXPECT_FALSE(exception_state.HadException());
  EXPECT_EQ(video_track->CountSinks(), 1u);

  // Cancelling the readable does not stop the track.
  // Push a frame and expect delivery to the mock sink.
  base::RunLoop sink_loop;
  EXPECT_CALL(mock_video_sink, OnVideoFrame(_))
      .WillOnce(base::test::RunOnceClosure(sink_loop.QuitClosure()));
  scoped_refptr<media::VideoFrame> frame =
      media::VideoFrame::CreateBlackFrame(gfx::Size(10, 5));
  pushable_video_source->PushFrame(frame, base::TimeTicks());

  sink_loop.Run();
  EXPECT_EQ(mock_video_sink.number_of_frames(), 1);
  EXPECT_EQ(mock_video_sink.last_frame(), frame);
}

TEST_F(MediaStreamTrackProcessorTest, ProcessorConnectsToGenerator) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();

  // Create a processor connected to a pushable source.
  PushableMediaStreamVideoSource* pushable_video_source =
      CreatePushableVideoSource();
  ExceptionState& exception_state = v8_scope.GetExceptionState();
  MediaStreamTrackProcessor* track_processor =
      MediaStreamTrackProcessor::Create(
          script_state,
          CreateVideoMediaStreamTrack(v8_scope.GetExecutionContext(),
                                      pushable_video_source),
          exception_state);

  // Create generator and connect it to a mock sink.
  MediaStreamTrackGenerator* track_generator =
      MakeGarbageCollected<MediaStreamTrackGenerator>(
          script_state, MediaStreamSource::kTypeVideo, "track_id");
  MockMediaStreamVideoSink mock_video_sink;
  mock_video_sink.ConnectToTrack(
      WebMediaStreamTrack(track_generator->Component()));
  EXPECT_EQ(mock_video_sink.number_of_frames(), 0);
  EXPECT_EQ(mock_video_sink.last_frame(), nullptr);

  // Connect the processor to the generator
  track_processor->readable(script_state)
      ->pipeTo(script_state, track_generator->writable(script_state),
               exception_state);

  // Push a frame and verify that it makes it to the sink at the end of the
  // chain.
  base::RunLoop sink_loop;
  EXPECT_CALL(mock_video_sink, OnVideoFrame(_))
      .WillOnce(base::test::RunOnceClosure(sink_loop.QuitClosure()));
  scoped_refptr<media::VideoFrame> frame =
      media::VideoFrame::CreateBlackFrame(gfx::Size(10, 5));
  pushable_video_source->PushFrame(frame, base::TimeTicks());

  sink_loop.Run();
  EXPECT_EQ(mock_video_sink.number_of_frames(), 1);
  EXPECT_EQ(mock_video_sink.last_frame(), frame);
}

TEST_F(MediaStreamTrackProcessorTest, NullInputTrack) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();
  MediaStreamTrackProcessor* track_processor =
      MediaStreamTrackProcessor::Create(script_state, nullptr, exception_state);

  EXPECT_EQ(track_processor, nullptr);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(static_cast<DOMExceptionCode>(v8_scope.GetExceptionState().Code()),
            DOMExceptionCode::kOperationError);
}

// TODO(crbug.com/1142955): Add support for audio.
TEST_F(MediaStreamTrackProcessorTest, Audio) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();
  MediaStreamTrack* media_stream_track =
      CreateAudioMediaStreamTrack(v8_scope.GetExecutionContext());
  MediaStreamTrackProcessor* track_processor =
      MediaStreamTrackProcessor::Create(script_state, media_stream_track,
                                        exception_state);

  EXPECT_EQ(track_processor, nullptr);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(static_cast<DOMExceptionCode>(v8_scope.GetExceptionState().Code()),
            DOMExceptionCode::kNotSupportedError);
}

}  // namespace blink
