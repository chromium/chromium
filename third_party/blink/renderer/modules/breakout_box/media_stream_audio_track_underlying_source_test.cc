// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/breakout_box/media_stream_audio_track_underlying_source.h"

#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_timestamp_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_track.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"
#include "third_party/blink/renderer/modules/breakout_box/pushable_media_stream_audio_source.h"
#include "third_party/blink/renderer/modules/breakout_box/stream_test_utils.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track_impl.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_audio_sink.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_data.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component_impl.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

using testing::_;
using testing::AnyNumber;

namespace blink {

namespace {
constexpr int kSampleRate = 8000;
constexpr int kFramesPerBuffer = 800;
constexpr int kNumFrames = 10;
const media::AudioParameters kMonoParams =
    media::AudioParameters(media::AudioParameters::AUDIO_PCM_LINEAR,
                           media::ChannelLayoutConfig::Mono(),
                           kSampleRate,
                           kFramesPerBuffer);

const media::AudioParameters kStereoParams =
    media::AudioParameters(media::AudioParameters::AUDIO_PCM_LINEAR,
                           media::ChannelLayoutConfig::Stereo(),
                           kSampleRate,
                           kFramesPerBuffer);

}  // namespace

class MediaStreamAudioTrackUnderlyingSourceTest : public testing::Test {
 public:
  ~MediaStreamAudioTrackUnderlyingSourceTest() override {
    platform_->RunUntilIdle();
    WebHeap::CollectAllGarbageForTesting();
  }

  MediaStreamTrack* CreateTrack(ExecutionContext* execution_context) {
    auto pushable_audio_source =
        std::make_unique<PushableMediaStreamAudioSource>(
            scheduler::GetSingleThreadTaskRunnerForTesting(),
            platform_->GetIOTaskRunner());
    PushableMediaStreamAudioSource* pushable_audio_source_ptr =
        pushable_audio_source.get();
    MediaStreamSource* media_stream_source =
        MakeGarbageCollected<MediaStreamSource>(
            "dummy_source_id", MediaStreamSource::kTypeAudio,
            "dummy_source_name", false /* remote */,
            std::move(pushable_audio_source));
    MediaStreamComponent* component =
        MakeGarbageCollected<MediaStreamComponentImpl>(
            String::FromUTF8("audio_track"), media_stream_source,
            std::make_unique<MediaStreamAudioTrack>(true /* is_local_track */));
    pushable_audio_source_ptr->ConnectToInitializedTrack(component);

    return MakeGarbageCollected<MediaStreamTrackImpl>(execution_context,
                                                      component);
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
  // Pushes data into |track|. |timestamp| is the reference time at the
  // beginning of the audio data to be pushed into |track|.
  void PushData(
      MediaStreamTrack* track,
      const std::optional<base::TimeDelta>& timestamp = std::nullopt) {
    auto data = media::AudioBuffer::CreateEmptyBuffer(
        media::ChannelLayout::CHANNEL_LAYOUT_STEREO, /*channel_count=*/2,
        kSampleRate, kNumFrames, timestamp.value_or(base::Seconds(1)));
    PushableMediaStreamAudioSource* pushable_audio_source =
        static_cast<PushableMediaStreamAudioSource*>(
            MediaStreamAudioSource::From(track->Component()->Source()));
    pushable_audio_source->PushAudioData(std::move(data));
    platform_->RunUntilIdle();
  }

  void SetChannelData(media::AudioBus* bus, int channel, float value) {
    ASSERT_LE(channel, bus->channels());

    float* bus_channel = bus->channel(channel);
    for (int i = 0; i < bus->frames(); ++i) {
      bus_channel[i] = value;
    }
  }

  bool DataMatches(scoped_refptr<media::AudioBuffer> buffer,
                   const media::AudioBus& bus) {
    EXPECT_EQ(bus.channels(), buffer->channel_count());
    EXPECT_EQ(bus.frames(), buffer->frame_count());

    for (int ch = 0; ch < bus.channels(); ch++) {
      const float* bus_channel = bus.channel(ch);
      const float* buffer_channel =
          reinterpret_cast<float*>(buffer->channel_data()[ch]);
      for (int i = 0; i < bus.frames(); ++i) {
        if (bus_channel[i] != buffer_channel[i]) {
          return false;
        }
      }
    }

    return true;
  }

  std::unique_ptr<media::AudioBus> CreateTestData(
      const media::AudioParameters params,
      float channel_value_increment) {
    auto audio_bus = media::AudioBus::Create(params);
    for (int ch = 0; ch < audio_bus->channels(); ++ch) {
      SetChannelData(audio_bus.get(), ch, (ch + 1) * channel_value_increment);
    }
    return audio_bus;
  }

  test::TaskEnvironment task_environment_;
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;
};

TEST_F(MediaStreamAudioTrackUnderlyingSourceTest,
       AudioDataFlowsThroughStreamAndCloses) {
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
  PushData(track);
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
  auto* stream =
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
    PushData(track, timestamp);
    sink_loop.Run();
  };

  for (wtf_size_t i = 0; i < buffer_size; ++i) {
    base::TimeDelta timestamp = base::Seconds(i);
    push_frame_sync(timestamp);
  }

  // Push another frame while the queue is full.
  push_frame_sync(base::Seconds(buffer_size));

  // Since the queue was full, the oldest frame from the queue (timestamp 0)
  // should have been dropped.
  NonThrowableExceptionState exception_state;
  auto* reader =
      stream->GetDefaultReaderForTesting(script_state, exception_state);
  for (wtf_size_t i = 1; i <= buffer_size; ++i) {
    AudioData* audio_data = ReadObjectFromStream<AudioData>(v8_scope, reader);
    EXPECT_EQ(base::Microseconds(audio_data->timestamp()), base::Seconds(i));
  }

  // Pulling causes a pending pull since there are no frames available for
  // reading.
  EXPECT_EQ(source->NumPendingPullsForTesting(), 0);
  source->Pull(script_state, ASSERT_NO_EXCEPTION);
  EXPECT_EQ(source->NumPendingPullsForTesting(), 1);

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

TEST_F(MediaStreamAudioTrackUnderlyingSourceTest, BufferPooling_Simple) {
  V8TestingScope v8_scope;
  auto* track = CreateTrack(v8_scope.GetExecutionContext());
  auto* source = CreateSource(v8_scope.GetScriptState(), track, 0u);
  auto* buffer_pool = source->GetAudioBufferPoolForTesting();

  // Needs to be called before CopyIntoAudioBuffer().
  buffer_pool->SetFormat(kStereoParams);

  // Create fake data with distinct channels.
  auto audio_bus = CreateTestData(kStereoParams, 0.25);
  base::TimeTicks now = base::TimeTicks::Now();

  // Send data to the pool.
  auto buffer = buffer_pool->CopyIntoAudioBuffer(*audio_bus, now);

  // Verify returned data.
  EXPECT_TRUE(DataMatches(buffer, *audio_bus));
  EXPECT_EQ(buffer->timestamp(), now - base::TimeTicks());

  source->Close();
  track->stopTrack(v8_scope.GetExecutionContext());
}

TEST_F(MediaStreamAudioTrackUnderlyingSourceTest, BufferPooling_BufferReuse) {
  V8TestingScope v8_scope;
  auto* track = CreateTrack(v8_scope.GetExecutionContext());
  auto* source = CreateSource(v8_scope.GetScriptState(), track, 0u);
  auto* buffer_pool = source->GetAudioBufferPoolForTesting();

  // Needs to be called before CopyIntoAudioBuffer().
  buffer_pool->SetFormat(kStereoParams);

  EXPECT_EQ(buffer_pool->GetSizeForTesting(), 0);

  // Create fake data with distinct channels.
  auto audio_bus = CreateTestData(kStereoParams, 0.25);
  auto other_audio_bus = CreateTestData(kStereoParams, 0.33);
  base::TimeTicks now = base::TimeTicks::Now();

  // Send data to the pool.
  auto buffer = buffer_pool->CopyIntoAudioBuffer(*audio_bus, now);

  // Verify returned data.
  EXPECT_TRUE(DataMatches(buffer, *audio_bus));

  // We should have allocated a single buffer.
  EXPECT_EQ(buffer_pool->GetSizeForTesting(), 1);

  // Release all references to `buffer`. The pool should still keep one.
  buffer.reset();
  EXPECT_EQ(buffer_pool->GetSizeForTesting(), 1);

  // Request a new buffer.
  auto other_buffer = buffer_pool->CopyIntoAudioBuffer(*other_audio_bus, now);

  // There should be no extra allocation since `buffer` was cleared.
  EXPECT_TRUE(DataMatches(other_buffer, *other_audio_bus));
  EXPECT_EQ(buffer_pool->GetSizeForTesting(), 1);

  // Request another buffer, without releasing `other_buffer`.
  buffer = buffer_pool->CopyIntoAudioBuffer(*audio_bus, now);

  // There should be two allocated buffers now.
  EXPECT_EQ(buffer_pool->GetSizeForTesting(), 2);

  // Make sure we didn't overwrite any data.
  EXPECT_TRUE(DataMatches(buffer, *audio_bus));
  EXPECT_TRUE(DataMatches(other_buffer, *other_audio_bus));

  source->Close();
  track->stopTrack(v8_scope.GetExecutionContext());
}

TEST_F(MediaStreamAudioTrackUnderlyingSourceTest, BufferPooling_FormatChange) {
  V8TestingScope v8_scope;
  auto* track = CreateTrack(v8_scope.GetExecutionContext());
  auto* source = CreateSource(v8_scope.GetScriptState(), track, 0u);
  auto* buffer_pool = source->GetAudioBufferPoolForTesting();

  EXPECT_EQ(buffer_pool->GetSizeForTesting(), 0);

  // Needs to be called before CopyIntoAudioBuffer().
  buffer_pool->SetFormat(kStereoParams);

  // Create fake data with distinct channels.
  auto stereo_audio_bus = CreateTestData(kStereoParams, 0.25);
  auto mono_audio_bus = CreateTestData(kMonoParams, 0.33);
  base::TimeTicks now = base::TimeTicks::Now();

  // Send data to the pool.
  auto buffer_a = buffer_pool->CopyIntoAudioBuffer(*stereo_audio_bus, now);
  auto buffer_b = buffer_pool->CopyIntoAudioBuffer(*stereo_audio_bus, now);

  // We should have allocated 2 buffers.
  EXPECT_EQ(buffer_pool->GetSizeForTesting(), 2);

  // Sending an identical formats should not clear the pool.
  buffer_pool->SetFormat(kStereoParams);
  EXPECT_EQ(buffer_pool->GetSizeForTesting(), 2);

  // Sending a different format should clear the pool.
  buffer_pool->SetFormat(kMonoParams);
  EXPECT_EQ(buffer_pool->GetSizeForTesting(), 0);

  // Make sure the references we are holding are still valid.
  EXPECT_TRUE(DataMatches(buffer_a, *stereo_audio_bus));
  EXPECT_TRUE(DataMatches(buffer_b, *stereo_audio_bus));

  // Send data in the new format to the pool.
  auto mono_buffer = buffer_pool->CopyIntoAudioBuffer(*mono_audio_bus, now);

  // The pool should allocate new buffers.
  EXPECT_EQ(buffer_pool->GetSizeForTesting(), 1);

  source->Close();
  track->stopTrack(v8_scope.GetExecutionContext());
}

}  // namespace blink
