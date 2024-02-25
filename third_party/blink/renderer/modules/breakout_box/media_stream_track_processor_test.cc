// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/breakout_box/media_stream_track_processor.h"

#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_stream_track_generator_init.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_reader.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_writer.h"
#include "third_party/blink/renderer/modules/breakout_box/media_stream_track_generator.h"
#include "third_party/blink/renderer/modules/breakout_box/metrics.h"
#include "third_party/blink/renderer/modules/breakout_box/pushable_media_stream_audio_source.h"
#include "third_party/blink/renderer/modules/breakout_box/pushable_media_stream_video_source.h"
#include "third_party/blink/renderer/modules/breakout_box/stream_test_utils.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_audio_sink.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_sink.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_source.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component_impl.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

using testing::_;

namespace blink {

namespace {

std::unique_ptr<PushableMediaStreamAudioSource> CreatePushableAudioSource() {
  // Use the IO thread for testing purposes.
  return std::make_unique<PushableMediaStreamAudioSource>(
      scheduler::GetSingleThreadTaskRunnerForTesting(),
      Platform::Current()->GetIOTaskRunner());
}

PushableMediaStreamVideoSource* CreatePushableVideoSource() {
  PushableMediaStreamVideoSource* pushable_video_source =
      new PushableMediaStreamVideoSource(
          scheduler::GetSingleThreadTaskRunnerForTesting());
  // The constructor of MediaStreamSource sets itself as the Owner
  // of the PushableMediaStreamVideoSource, so as long as the test calls
  // CreateVideoMediaStreamTrack() with the returned pushable_video_source,
  // there will be a Member reference to this MediaStreamSource, and we
  // can drop the reference here.
  // TODO(crbug.com/1302689): Fix this ownership nonsense, just have a single
  // class which is GC owned.
  MakeGarbageCollected<MediaStreamSource>(
      "source_id", MediaStreamSource::kTypeVideo, "source_name",
      /*remote=*/false, base::WrapUnique(pushable_video_source));
  return pushable_video_source;
}

MediaStreamTrack* CreateAudioMediaStreamTrack(
    ExecutionContext* context,
    std::unique_ptr<MediaStreamAudioSource> source) {
  auto* source_ptr = source.get();

  MediaStreamSource* media_stream_source =
      MakeGarbageCollected<MediaStreamSource>(
          "source_id", MediaStreamSource::kTypeAudio, "source_name",
          /*remote=*/false, std::move(source));

  MediaStreamComponent* component =
      MakeGarbageCollected<MediaStreamComponentImpl>(
          media_stream_source,
          std::make_unique<MediaStreamAudioTrack>(true /* is_local_track */));

  source_ptr->ConnectToInitializedTrack(component);

  return MakeGarbageCollected<MediaStreamTrackImpl>(context, component);
}

}  // namespace

class MediaStreamTrackProcessorTest : public testing::Test {
 public:
  ~MediaStreamTrackProcessorTest() override {
    RunIOUntilIdle();
    WebHeap::CollectAllGarbageForTesting();
  }

 protected:
  test::TaskEnvironment task_environment_;
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;

 private:
  void RunIOUntilIdle() const {
    // Make sure that tasks on IO thread are completed before moving on.
    base::RunLoop run_loop;
    Platform::Current()->GetIOTaskRunner()->PostTaskAndReply(
        FROM_HERE, base::BindOnce([] {}), run_loop.QuitClosure());
    run_loop.Run();
    base::RunLoop().RunUntilIdle();
  }
};

TEST_F(MediaStreamTrackProcessorTest, VideoFramesAreExposed) {
  base::HistogramTester histogram_tester;
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
  histogram_tester.ExpectUniqueSample("Media.BreakoutBox.Usage",
                                      BreakoutBoxUsage::kReadableVideo, 1);
  histogram_tester.ExpectTotalCount("Media.BreakoutBox.Usage", 1);
}

TEST_F(MediaStreamTrackProcessorTest, AudioDataAreExposed) {
  base::HistogramTester histogram_tester;
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

  // Deliver data.
  base::RunLoop sink_loop;
  EXPECT_CALL(mock_audio_sink, OnData(_, _))
      .WillOnce(base::test::RunOnceClosure(sink_loop.QuitClosure()));
  pushable_source_ptr->PushAudioData(media::AudioBuffer::CreateEmptyBuffer(
      media::ChannelLayout::CHANNEL_LAYOUT_STEREO, /*channel_count=*/2,
      /*sample_rate=*/8000,
      /*frame_count=*/100, base::Seconds(1)));

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
  histogram_tester.ExpectUniqueSample("Media.BreakoutBox.Usage",
                                      BreakoutBoxUsage::kReadableAudio, 1);
  histogram_tester.ExpectTotalCount("Media.BreakoutBox.Usage", 1);
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
  MediaStreamTrackGeneratorInit* init = MediaStreamTrackGeneratorInit::Create();
  init->setKind("video");
  MediaStreamTrackGenerator* track_generator =
      MediaStreamTrackGenerator::Create(script_state, init, exception_state);
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
  MediaStreamTrack* track = nullptr;
  MediaStreamTrackProcessor* track_processor =
      MediaStreamTrackProcessor::Create(script_state, track, exception_state);

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

TEST_F(MediaStreamTrackProcessorTest, VideoCloseOnTrackEnd) {
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

#if BUILDFLAG(IS_FUCHSIA)
// TODO(https://crbug.com/1234343): Test seems flaky on Fuchsia, enable once
// flakiness has been investigated.
#define MAYBE_VideoNoCloseOnTrackDisable DISABLED_VideoNoCloseOnTrackDisable
#else
#define MAYBE_VideoNoCloseOnTrackDisable VideoNoCloseOnTrackDisable
#endif

TEST_F(MediaStreamTrackProcessorTest, MAYBE_VideoNoCloseOnTrackDisable) {
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

TEST_F(MediaStreamTrackProcessorTest, AudioCloseOnTrackEnd) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();
  std::unique_ptr<PushableMediaStreamAudioSource> pushable_audio_source =
      CreatePushableAudioSource();
  MediaStreamTrack* track = CreateAudioMediaStreamTrack(
      v8_scope.GetExecutionContext(), std::move(pushable_audio_source));

  MediaStreamTrackProcessor* track_processor =
      MediaStreamTrackProcessor::Create(script_state, track, exception_state);
  ReadableStream* readable = track_processor->readable(script_state);
  EXPECT_FALSE(readable->IsClosed());

  track->stopTrack(v8_scope.GetExecutionContext());

  EXPECT_TRUE(readable->IsClosed());
}

TEST_F(MediaStreamTrackProcessorTest, AudioNoCloseOnTrackDisable) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();
  std::unique_ptr<PushableMediaStreamAudioSource> pushable_audio_source =
      CreatePushableAudioSource();
  MediaStreamTrack* track = CreateAudioMediaStreamTrack(
      v8_scope.GetExecutionContext(), std::move(pushable_audio_source));

  MediaStreamTrackProcessor* track_processor =
      MediaStreamTrackProcessor::Create(script_state, track, exception_state);
  ReadableStream* readable = track_processor->readable(script_state);
  EXPECT_FALSE(readable->IsClosed());

  track->setEnabled(false);

  EXPECT_FALSE(readable->IsClosed());
}

}  // namespace blink
