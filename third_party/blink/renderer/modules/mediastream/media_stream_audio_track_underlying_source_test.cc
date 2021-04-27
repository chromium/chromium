// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_audio_track_underlying_source.h"

#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "media/base/audio_timestamp_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_track.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_audio_sink.h"
#include "third_party/blink/renderer/modules/mediastream/pushable_media_stream_audio_source.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_frame_serialization_data.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

using testing::_;
using testing::AnyNumber;

namespace blink {

class MediaStreamAudioTrackUnderlyingSourceTest : public testing::Test {
 public:
  ~MediaStreamAudioTrackUnderlyingSourceTest() override {
    platform_->RunUntilIdle();
    WebHeap::CollectAllGarbageForTesting();
  }

  MediaStreamTrack* CreateTrack(ExecutionContext* execution_context) {
    MediaStreamSource* media_stream_source =
        MakeGarbageCollected<MediaStreamSource>(
            "dummy_source_id", MediaStreamSource::kTypeAudio,
            "dummy_source_name", false /* remote */);
    PushableMediaStreamAudioSource* pushable_audio_source =
        new PushableMediaStreamAudioSource(
            Thread::MainThread()->GetTaskRunner(),
            Platform::Current()->GetIOTaskRunner());
    media_stream_source->SetPlatformSource(
        base::WrapUnique(pushable_audio_source));
    MediaStreamComponent* component =
        MakeGarbageCollected<MediaStreamComponent>(
            String::FromUTF8("audio_track"), media_stream_source);
    pushable_audio_source->ConnectToTrack(component);

    return MakeGarbageCollected<MediaStreamTrack>(execution_context, component);
  }

  MediaStreamAudioTrackUnderlyingSource* CreateSource(ScriptState* script_state,
                                                      MediaStreamTrack* track,
                                                      wtf_size_t buffer_size) {
    return MakeGarbageCollected<MediaStreamAudioTrackUnderlyingSource>(
        script_state, track->Component(), nullptr, buffer_size);
  }

  MediaStreamAudioTrackUnderlyingSource* CreateSource(ScriptState* script_state,
                                                      MediaStreamTrack* track) {
    return CreateSource(script_state, track, 1u);
  }

 protected:
  static const int kSampleRate = 8000;
  static const int kNumFrames = 10;

  // Pushes a frame into |track|. |timestamp| is the reference time at the
  // beginning of the audio data to be pushed into |track|.
  void PushFrame(
      MediaStreamTrack* track,
      const base::Optional<base::TimeDelta>& timestamp = base::nullopt) {
    auto data = AudioFrameSerializationData::Wrap(
        media::AudioBus::Create(/*channels=*/2, kNumFrames), kSampleRate,
        timestamp.value_or(base::TimeDelta::FromSeconds(1)));
    PushableMediaStreamAudioSource* pushable_audio_source =
        static_cast<PushableMediaStreamAudioSource*>(
            MediaStreamAudioSource::From(track->Component()->Source()));
    pushable_audio_source->PushAudioData(std::move(data));
    platform_->RunUntilIdle();
  }

  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;
};

TEST_F(MediaStreamAudioTrackUnderlyingSourceTest,
       AudioFrameFlowsThroughStreamAndCloses) {
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
  PushFrame(track);
  read_tester.WaitUntilSettled();
  EXPECT_TRUE(read_tester.IsFulfilled());

  source->Close();
  track->stopTrack(v8_scope.GetExecutionContext());
}

TEST_F(MediaStreamAudioTrackUnderlyingSourceTest,
       CancelStreamDisconnectsFromTrack) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* track = CreateTrack(v8_scope.GetExecutionContext());
  auto* source = CreateSource(script_state, track);
  auto* stream =
      ReadableStream::CreateWithCountQueueingStrategy(script_state, source, 0);

  // The stream is connected to a sink.
  EXPECT_TRUE(source->Track());

  NonThrowableExceptionState exception_state;
  stream->cancel(script_state, exception_state);

  // Canceling the stream disconnects it from the track.
  EXPECT_FALSE(source->Track());
  track->stopTrack(v8_scope.GetExecutionContext());
}

TEST_F(MediaStreamAudioTrackUnderlyingSourceTest,
       DropOldFramesWhenQueueIsFull) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  const wtf_size_t buffer_size = 5;
  auto* track = CreateTrack(v8_scope.GetExecutionContext());
  auto* source = CreateSource(script_state, track, buffer_size);
  EXPECT_EQ(source->MaxQueueSize(), buffer_size);
  // Create a stream to ensure there is a controller associated to the source.
  ReadableStream::CreateWithCountQueueingStrategy(script_state, source, 0);

  // Add a sink to the track to make it possible to wait until a pushed frame
  // is delivered to sinks, including |source|, which is a sink of the track.
  MockMediaStreamAudioSink mock_sink;
  WebMediaStreamAudioSink::AddToAudioTrack(
      &mock_sink, WebMediaStreamTrack(track->Component()));

  auto push_frame_sync = [&mock_sink, track,
                          this](const base::TimeDelta timestamp) {
    base::RunLoop sink_loop;
    EXPECT_CALL(mock_sink, OnData(_, _))
        .WillOnce(base::test::RunOnceClosure(sink_loop.QuitClosure()));
    PushFrame(track, timestamp);
    sink_loop.Run();
  };

  const auto& queue = source->QueueForTesting();
  for (wtf_size_t i = 0; i < buffer_size; ++i) {
    EXPECT_EQ(queue.size(), i);
    base::TimeDelta timestamp = base::TimeDelta::FromSeconds(i);
    push_frame_sync(timestamp);
    EXPECT_EQ(queue.back()->timestamp(), timestamp);
    EXPECT_EQ(queue.front()->timestamp(), base::TimeDelta());
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

  WebMediaStreamAudioSink::RemoveFromAudioTrack(
      &mock_sink, WebMediaStreamTrack(track->Component()));
  track->stopTrack(v8_scope.GetExecutionContext());
}

TEST_F(MediaStreamAudioTrackUnderlyingSourceTest,
       BypassQueueAfterPullWithEmptyBuffer) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* track = CreateTrack(v8_scope.GetExecutionContext());
  auto* source = CreateSource(script_state, track);
  // Create a stream to ensure there is a controller associated to the source.
  ReadableStream::CreateWithCountQueueingStrategy(script_state, source, 0);

  MockMediaStreamAudioSink mock_sink;
  WebMediaStreamAudioSink::AddToAudioTrack(
      &mock_sink, WebMediaStreamTrack(track->Component()));

  auto push_frame_sync = [&mock_sink, track, this]() {
    base::RunLoop sink_loop;
    EXPECT_CALL(mock_sink, OnData(_, _))
        .WillOnce(base::test::RunOnceClosure(sink_loop.QuitClosure()));
    PushFrame(track);
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
  WebMediaStreamAudioSink::RemoveFromAudioTrack(
      &mock_sink, WebMediaStreamTrack(track->Component()));
  track->stopTrack(v8_scope.GetExecutionContext());
}

TEST_F(MediaStreamAudioTrackUnderlyingSourceTest, QueueSizeCannotBeZero) {
  V8TestingScope v8_scope;
  auto* track = CreateTrack(v8_scope.GetExecutionContext());
  auto* source = CreateSource(v8_scope.GetScriptState(), track, 0u);
  // Queue size is always at least 1, even if 0 is requested.
  EXPECT_EQ(source->MaxQueueSize(), 1u);
  source->Close();
  track->stopTrack(v8_scope.GetExecutionContext());
}

TEST_F(MediaStreamAudioTrackUnderlyingSourceTest, PlatformSourceAliveAfterGC) {
  // This persistent is used to make |track->Component()| (and its
  // MediaStreamAudioTrack) outlive |v8_scope| and stay alive after GC.
  Persistent<MediaStreamComponent> component;
  {
    V8TestingScope v8_scope;
    auto* track = CreateTrack(v8_scope.GetExecutionContext());
    component = track->Component();
    auto* source = CreateSource(v8_scope.GetScriptState(), track);
    ReadableStream::CreateWithCountQueueingStrategy(v8_scope.GetScriptState(),
                                                    source, 0);
    // |source| is a sink of |track|.
    EXPECT_TRUE(source->Track());
  }
  blink::WebHeap::CollectAllGarbageForTesting();
  // At this point, if |source| were still a sink of the MediaStreamAudioTrack
  // owned by |component|, the MediaStreamAudioTrack cleanup would crash since
  // it would try to access |source|, which has been garbage collected.
  // A scenario like this one could occur when an execution context is detached.
}

}  // namespace blink
