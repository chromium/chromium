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
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_stream_track_signal.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_reader.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_writer.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track_generator.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track_signal.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_audio_sink.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_sink.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_source.h"
#include "third_party/blink/renderer/modules/mediastream/pushable_media_stream_audio_source.h"
#include "third_party/blink/renderer/modules/mediastream/pushable_media_stream_video_source.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_frame_serialization_data.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"

using testing::_;

namespace blink {

namespace {

std::unique_ptr<PushableMediaStreamAudioSource> CreatePushableAudioSource() {
  // Use the IO thread for testing purposes.
  return std::make_unique<PushableMediaStreamAudioSource>(
      Thread::MainThread()->GetTaskRunner(),
      Platform::Current()->GetIOTaskRunner());
}

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

MockMediaStreamVideoSource* CreateMockVideoSource() {
  MockMediaStreamVideoSource* mock_video_source =
      new MockMediaStreamVideoSource();
  MediaStreamSource* media_stream_source =
      MakeGarbageCollected<MediaStreamSource>(
          "source_id", MediaStreamSource::kTypeVideo, "source_name",
          /*remote=*/false);
  media_stream_source->SetPlatformSource(base::WrapUnique(mock_video_source));
  return mock_video_source;
}

MediaStreamTrack* CreateVideoMediaStreamTrack(ExecutionContext* context,
                                              MediaStreamVideoSource* source) {
  return MakeGarbageCollected<MediaStreamTrack>(
      context, MediaStreamVideoTrack::CreateVideoTrack(
                   source, MediaStreamVideoSource::ConstraintsOnceCallback(),
                   /*enabled=*/true));
}

MediaStreamTrack* CreateAudioMediaStreamTrack(
    ExecutionContext* context,
    std::unique_ptr<MediaStreamAudioSource> source) {
  auto* source_ptr = source.get();

  MediaStreamSource* media_stream_source =
      MakeGarbageCollected<MediaStreamSource>(
          "source_id", MediaStreamSource::kTypeAudio, "source_name",
          /*remote=*/false);
  media_stream_source->SetPlatformSource(std::move(source));

  MediaStreamComponent* component =
      MakeGarbageCollected<MediaStreamComponent>(media_stream_source);

  source_ptr->ConnectToTrack(component);

  return MakeGarbageCollected<MediaStreamTrack>(context, component);
}

ScriptValue CreateRequestFrameChunk(ScriptState* script_state) {
  MediaStreamTrackSignal* signal = MediaStreamTrackSignal::Create();
  signal->setSignalType("request-frame");
  return ScriptValue(script_state->GetIsolate(),
                     ToV8(signal, script_state->GetContext()->Global(),
                          script_state->GetIsolate()));
}

ScriptValue CreateSetMinFrameRateChunk(ScriptState* script_state,
                                       double frame_rate) {
  MediaStreamTrackSignal* signal = MediaStreamTrackSignal::Create();
  signal->setSignalType("set-min-frame-rate");
  signal->setFrameRate(frame_rate);
  return ScriptValue(script_state->GetIsolate(),
                     ToV8(signal, script_state->GetContext()->Global(),
                          script_state->GetIsolate()));
}

ScriptValue CreateInvalidSignalChunk(ScriptState* script_state) {
  MediaStreamTrackSignal* signal = MediaStreamTrackSignal::Create();
  signal->setSignalType("set-min-frame-rate");
  return ScriptValue(script_state->GetIsolate(),
                     ToV8(signal, script_state->GetContext()->Global(),
                          script_state->GetIsolate()));
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
  EXPECT_EQ(
      track_processor->InputTrack()->Component()->Source()->GetPlatformSource(),
      pushable_video_source);

  MockMediaStreamVideoSink mock_video_sink;
  mock_video_sink.ConnectToTrack(
      WebMediaStreamTrack(track_processor->InputTrack()->Component()));
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

TEST_F(MediaStreamTrackProcessorTest, AudioFramesAreExposed) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();
  std::unique_ptr<PushableMediaStreamAudioSource> pushable_audio_source =
      CreatePushableAudioSource();
  auto* pushable_source_ptr = pushable_audio_source.get();
  MediaStreamTrackProcessor* track_processor =
      MediaStreamTrackProcessor::Create(
          script_state,
          CreateAudioMediaStreamTrack(v8_scope.GetExecutionContext(),
                                      std::move(pushable_audio_source)),
          exception_state);
  EXPECT_FALSE(exception_state.HadException());
  MediaStreamComponent* component = track_processor->InputTrack()->Component();
  EXPECT_EQ(component->Source()->GetPlatformSource(), pushable_source_ptr);

  MockMediaStreamAudioSink mock_audio_sink;
  WebMediaStreamAudioSink::AddToAudioTrack(&mock_audio_sink,
                                           WebMediaStreamTrack(component));

  auto* reader =
      track_processor->readable(script_state)
          ->GetDefaultReaderForTesting(script_state, exception_state);
  EXPECT_FALSE(exception_state.HadException());

  // Deliver a frame.
  base::RunLoop sink_loop;
  EXPECT_CALL(mock_audio_sink, OnData(_, _))
      .WillOnce(base::test::RunOnceClosure(sink_loop.QuitClosure()));
  pushable_source_ptr->PushAudioData(AudioFrameSerializationData::Wrap(
      media::AudioBus::Create(/*channels=*/2, /*frames=*/100),
      /*sample_rate=*/8000, base::TimeDelta::FromSeconds(1)));

  ScriptPromiseTester read_tester(script_state,
                                  reader->read(script_state, exception_state));
  EXPECT_FALSE(read_tester.IsFulfilled());
  read_tester.WaitUntilSettled();
  EXPECT_FALSE(exception_state.HadException());
  EXPECT_TRUE(read_tester.IsFulfilled());
  EXPECT_TRUE(read_tester.Value().IsObject());
  sink_loop.Run();

  WebMediaStreamAudioSink::RemoveFromAudioTrack(&mock_audio_sink,
                                                WebMediaStreamTrack(component));
}

TEST_F(MediaStreamTrackProcessorTest, VideoControlSignalsAreForwarded) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();
  MockMediaStreamVideoSource* mock_video_source = CreateMockVideoSource();
  MediaStreamTrackProcessor* track_processor =
      MediaStreamTrackProcessor::Create(
          script_state,
          CreateVideoMediaStreamTrack(v8_scope.GetExecutionContext(),
                                      mock_video_source),
          exception_state);
  EXPECT_FALSE(exception_state.HadException());
  EXPECT_EQ(
      track_processor->InputTrack()->Component()->Source()->GetPlatformSource(),
      mock_video_source);
  mock_video_source->StartMockedSource();

  EXPECT_CALL(*mock_video_source, OnRequestRefreshFrame());
  auto* writer = track_processor->writableControl(script_state)
                     ->getWriter(script_state, exception_state);
  ScriptPromiseTester request_frame_tester(
      script_state,
      writer->write(script_state, CreateRequestFrameChunk(script_state),
                    exception_state));
  request_frame_tester.WaitUntilSettled();
  EXPECT_TRUE(request_frame_tester.IsFulfilled());
  EXPECT_FALSE(exception_state.HadException());

  MediaStreamVideoTrack* platform_track =
      MediaStreamVideoTrack::From(track_processor->InputTrack()->Component());
  EXPECT_FALSE(platform_track->min_frame_rate().has_value());
  const double min_frame_rate = 15.0;
  ScriptPromiseTester set_min_frame_rate_tester(
      script_state,
      writer->write(script_state,
                    CreateSetMinFrameRateChunk(script_state, min_frame_rate),
                    exception_state));
  set_min_frame_rate_tester.WaitUntilSettled();
  EXPECT_TRUE(set_min_frame_rate_tester.IsFulfilled());
  EXPECT_FALSE(exception_state.HadException());
  EXPECT_TRUE(platform_track->min_frame_rate().has_value());
  EXPECT_EQ(platform_track->min_frame_rate().value(), min_frame_rate);

  ScriptPromiseTester invalid_signal_tester(
      script_state,
      writer->write(script_state, CreateInvalidSignalChunk(script_state),
                    exception_state));
  invalid_signal_tester.WaitUntilSettled();
  EXPECT_TRUE(invalid_signal_tester.IsRejected());
}

TEST_F(MediaStreamTrackProcessorTest, AudioControlSignalsAreRejected) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();
  MediaStreamTrackProcessor* track_processor =
      MediaStreamTrackProcessor::Create(
          script_state,
          CreateAudioMediaStreamTrack(v8_scope.GetExecutionContext(),
                                      std::make_unique<MediaStreamAudioSource>(
                                          Thread::MainThread()->GetTaskRunner(),
                                          /*is_local=*/true)),
          exception_state);

  auto* writer = track_processor->writableControl(script_state)
                     ->getWriter(script_state, exception_state);
  ScriptPromiseTester tester(
      script_state,
      writer->write(script_state, CreateRequestFrameChunk(script_state),
                    exception_state));
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsRejected());
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
      MediaStreamVideoTrack::From(track_processor->InputTrack()->Component());
  EXPECT_EQ(video_track->CountSinks(), 0u);

  MockMediaStreamVideoSink mock_video_sink;
  mock_video_sink.ConnectToTrack(
      WebMediaStreamTrack(track_processor->InputTrack()->Component()));
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
  EXPECT_EQ(static_cast<ESErrorType>(v8_scope.GetExceptionState().Code()),
            ESErrorType::kTypeError);
}

TEST_F(MediaStreamTrackProcessorTest, EndedTrack) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();
  PushableMediaStreamVideoSource* pushable_video_source =
      CreatePushableVideoSource();
  MediaStreamTrack* track = CreateVideoMediaStreamTrack(
      v8_scope.GetExecutionContext(), pushable_video_source);
  track->stopTrack(v8_scope.GetExecutionContext());
  MediaStreamTrackProcessor* track_processor =
      MediaStreamTrackProcessor::Create(script_state, track, exception_state);

  EXPECT_EQ(track_processor, nullptr);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(static_cast<ESErrorType>(v8_scope.GetExceptionState().Code()),
            ESErrorType::kTypeError);
}

TEST_F(MediaStreamTrackProcessorTest, CloseOnTrackEnd) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();
  PushableMediaStreamVideoSource* pushable_video_source =
      CreatePushableVideoSource();
  MediaStreamTrack* track = CreateVideoMediaStreamTrack(
      v8_scope.GetExecutionContext(), pushable_video_source);

  MediaStreamTrackProcessor* track_processor =
      MediaStreamTrackProcessor::Create(script_state, track, exception_state);
  ReadableStream* readable = track_processor->readable(script_state);
  EXPECT_FALSE(readable->IsClosed());

  track->stopTrack(v8_scope.GetExecutionContext());

  EXPECT_TRUE(readable->IsClosed());
}

TEST_F(MediaStreamTrackProcessorTest, NoCloseOnTrackDisable) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();
  PushableMediaStreamVideoSource* pushable_video_source =
      CreatePushableVideoSource();
  MediaStreamTrack* track = CreateVideoMediaStreamTrack(
      v8_scope.GetExecutionContext(), pushable_video_source);

  MediaStreamTrackProcessor* track_processor =
      MediaStreamTrackProcessor::Create(script_state, track, exception_state);
  ReadableStream* readable = track_processor->readable(script_state);
  EXPECT_FALSE(readable->IsClosed());

  track->setEnabled(false);

  EXPECT_FALSE(readable->IsClosed());
}

}  // namespace blink
