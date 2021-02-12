// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/video_track_signal_underlying_sink.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_stream_track_signal.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_writer.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track_signal.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_source.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"

using testing::_;

namespace blink {

class VideoTrackSignalUnderlyingSinkTest : public testing::Test {
 public:
  VideoTrackSignalUnderlyingSinkTest() {
    mock_video_source_ = new MockMediaStreamVideoSource();
    media_stream_source_ = MakeGarbageCollected<MediaStreamSource>(
        "dummy_source_id", MediaStreamSource::kTypeVideo, "dummy_source_name",
        /*remote=*/false);
    media_stream_source_->SetPlatformSource(
        base::WrapUnique(mock_video_source_));
    web_track_ = MediaStreamVideoTrack::CreateVideoTrack(
        mock_video_source_, MediaStreamVideoSource::ConstraintsOnceCallback(),
        true);
    mock_video_source_->StartMockedSource();
  }

  ~VideoTrackSignalUnderlyingSinkTest() override {
    platform_->RunUntilIdle();
    mock_video_source_->StopSource();
    base::RunLoop run_loop;
    platform_->GetIOTaskRunner()->PostTask(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
    WebHeap::CollectAllGarbageForTesting();
  }

  MediaStreamTrack* CreateTrack(ExecutionContext* context) const {
    return MakeGarbageCollected<MediaStreamTrack>(context, web_track_);
  }
  VideoTrackSignalUnderlyingSink* CreateUnderlyingSink(
      MediaStreamTrack* track) {
    return MakeGarbageCollected<VideoTrackSignalUnderlyingSink>(track);
  }

  ScriptValue CreateSignalChunk(ScriptState* script_state,
                                const String& signal_name) {
    MediaStreamTrackSignal* signal = MediaStreamTrackSignal::Create();
    signal->setSignalType(signal_name);
    return ScriptValue(script_state->GetIsolate(),
                       ToV8(signal, script_state->GetContext()->Global(),
                            script_state->GetIsolate()));
  }

  ScriptValue CreateRequestFrameChunk(ScriptState* script_state) {
    return CreateSignalChunk(script_state, "request-frame");
  }

  ScriptValue CreateSetMinFrameRateChunk(
      ScriptState* script_state,
      const base::Optional<double>& frame_rate = 10.0) {
    MediaStreamTrackSignal* signal = MediaStreamTrackSignal::Create();
    signal->setSignalType("set-min-frame-rate");
    if (frame_rate)
      signal->setFrameRate(*frame_rate);
    return ScriptValue(script_state->GetIsolate(),
                       ToV8(signal, script_state->GetContext()->Global(),
                            script_state->GetIsolate()));
  }

 protected:
  Persistent<MediaStreamSource> media_stream_source_;
  WebMediaStreamTrack web_track_;
  MockMediaStreamVideoSource* mock_video_source_;
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;
};

TEST_F(VideoTrackSignalUnderlyingSinkTest,
       WriteRequestFrameToStreamForwardsToVideoSource) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* track = CreateTrack(v8_scope.GetExecutionContext());
  auto* underlying_sink = CreateUnderlyingSink(track);
  auto* writable_stream = WritableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_sink, 1u);

  NonThrowableExceptionState exception_state;
  auto* writer = writable_stream->getWriter(script_state, exception_state);

  auto request_frame_chunk = CreateRequestFrameChunk(script_state);
  EXPECT_CALL(*mock_video_source_, OnRequestRefreshFrame());
  ScriptPromiseTester write_tester(
      script_state,
      writer->write(script_state, request_frame_chunk, exception_state));
  write_tester.WaitUntilSettled();

  const double frame_rate = 14.8;
  auto set_min_frame_rate_chunk =
      CreateSetMinFrameRateChunk(script_state, frame_rate);
  MediaStreamVideoTrack* video_track =
      MediaStreamVideoTrack::From(track->Component());
  EXPECT_FALSE(video_track->min_frame_rate().has_value());
  ScriptPromiseTester write_tester2(
      script_state,
      writer->write(script_state, set_min_frame_rate_chunk, exception_state));
  write_tester2.WaitUntilSettled();
  EXPECT_TRUE(video_track->min_frame_rate().has_value());
  EXPECT_EQ(video_track->min_frame_rate().value(), frame_rate);

  writer->releaseLock(script_state);
  ScriptPromiseTester close_tester(
      script_state, writable_stream->close(script_state, exception_state));
  close_tester.WaitUntilSettled();

  MediaStreamTrack* clone = track->clone(script_state);
  track->stopTrack(v8_scope.GetExecutionContext());

  // Writing to the sink after the track closes should fail, even if the source
  // is active.
  EXPECT_TRUE(mock_video_source_->IsRunning());
  DummyExceptionStateForTesting dummy_exception_state;
  underlying_sink->write(script_state, CreateRequestFrameChunk(script_state),
                         nullptr, dummy_exception_state);
  EXPECT_TRUE(dummy_exception_state.HadException());
  EXPECT_EQ(dummy_exception_state.Code(),
            static_cast<ExceptionCode>(DOMExceptionCode::kInvalidStateError));

  clone->stopTrack(v8_scope.GetExecutionContext());
  EXPECT_FALSE(mock_video_source_->IsRunning());
  // Writing to the sink after the source closes should fail.
  dummy_exception_state.ClearException();
  underlying_sink->write(script_state, CreateRequestFrameChunk(script_state),
                         nullptr, dummy_exception_state);
  EXPECT_TRUE(dummy_exception_state.HadException());
  EXPECT_EQ(dummy_exception_state.Code(),
            static_cast<ExceptionCode>(DOMExceptionCode::kInvalidStateError));
}

TEST_F(VideoTrackSignalUnderlyingSinkTest, WriteInvalidDataFails) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* track = CreateTrack(v8_scope.GetExecutionContext());
  auto* underlying_sink = CreateUnderlyingSink(track);

  MediaStreamVideoTrack* video_track =
      MediaStreamVideoTrack::From(track->Component());
  EXPECT_FALSE(video_track->min_frame_rate().has_value());

  DummyExceptionStateForTesting exception_state;
  auto set_min_frame_rate_chunk =
      CreateSetMinFrameRateChunk(script_state, base::nullopt);
  underlying_sink->write(script_state, set_min_frame_rate_chunk, nullptr,
                         exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_FALSE(video_track->min_frame_rate().has_value());

  exception_state.ClearException();
  EXPECT_FALSE(exception_state.HadException());
  underlying_sink->write(script_state,
                         CreateSignalChunk(script_state, "invalid-signal"),
                         nullptr, exception_state);
  EXPECT_TRUE(exception_state.HadException());

  // Writing null fails
  exception_state.ClearException();
  EXPECT_FALSE(exception_state.HadException());
  underlying_sink->write(script_state,
                         ScriptValue::CreateNull(v8_scope.GetIsolate()),
                         nullptr, exception_state);
  EXPECT_TRUE(exception_state.HadException());

  // Writing an intenger fails
  exception_state.ClearException();
  EXPECT_FALSE(exception_state.HadException());
  underlying_sink->write(script_state, ScriptValue::From(script_state, 5),
                         nullptr, exception_state);
  EXPECT_TRUE(exception_state.HadException());

  track->stopTrack(v8_scope.GetExecutionContext());
}

TEST_F(VideoTrackSignalUnderlyingSinkTest, WriteToClosedSinkFails) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* track = CreateTrack(v8_scope.GetExecutionContext());
  auto* underlying_sink = CreateUnderlyingSink(track);

  auto* writable_stream = WritableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_sink, 1u);

  NonThrowableExceptionState exception_state;
  ScriptPromiseTester abort_tester(
      script_state, writable_stream->close(script_state, exception_state));
  abort_tester.WaitUntilSettled();

  // Writing to the sink after the stream closes should fail.
  DummyExceptionStateForTesting dummy_exception_state;
  underlying_sink->write(script_state, CreateRequestFrameChunk(script_state),
                         nullptr, dummy_exception_state);
  EXPECT_TRUE(dummy_exception_state.HadException());
  EXPECT_EQ(dummy_exception_state.Code(),
            static_cast<ExceptionCode>(DOMExceptionCode::kInvalidStateError));

  track->stopTrack(v8_scope.GetExecutionContext());
}

TEST_F(VideoTrackSignalUnderlyingSinkTest, WriteToAbortedSinkFails) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* track = CreateTrack(v8_scope.GetExecutionContext());
  auto* underlying_sink = CreateUnderlyingSink(track);

  auto* writable_stream = WritableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_sink, 1u);

  NonThrowableExceptionState exception_state;
  ScriptPromiseTester abort_tester(
      script_state, writable_stream->abort(script_state, exception_state));
  abort_tester.WaitUntilSettled();

  // Writing to the sink after the stream aborts should fail.
  DummyExceptionStateForTesting dummy_exception_state;
  underlying_sink->write(script_state, CreateRequestFrameChunk(script_state),
                         nullptr, dummy_exception_state);
  EXPECT_TRUE(dummy_exception_state.HadException());
  EXPECT_EQ(dummy_exception_state.Code(),
            static_cast<ExceptionCode>(DOMExceptionCode::kInvalidStateError));

  track->stopTrack(v8_scope.GetExecutionContext());
}

}  // namespace blink
