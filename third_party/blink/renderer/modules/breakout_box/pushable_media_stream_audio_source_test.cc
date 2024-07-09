// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/breakout_box/pushable_media_stream_audio_source.h"

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-blink.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_audio_sink.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component_impl.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {

class FakeMediaStreamAudioSink : public WebMediaStreamAudioSink {
 public:
  FakeMediaStreamAudioSink(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread,
      scoped_refptr<base::SingleThreadTaskRunner> audio_thread)
      : main_task_runner_(std::move(main_thread)),
        audio_task_runner_(std::move(audio_thread)) {}
  ~FakeMediaStreamAudioSink() override = default;

  void SetupNewAudioParameterExpectations(int channels,
                                          int frames,
                                          int sample_rate) {
    expected_channels_ = channels;
    expected_frames_ = frames;
    expected_sample_rate_ = sample_rate;
  }

  void SetDataTimeExpectation(base::TimeTicks time,
                              media::AudioBus* expected_data,
                              bool expect_data_on_audio_task_runner,
                              base::OnceClosure on_data) {
    DCHECK(!on_data_);

    expected_time_ = time;
    expected_data_ = expected_data;
    expect_data_on_audio_task_runner_ = expect_data_on_audio_task_runner;

    on_data_ = std::move(on_data);
  }

  void OnData(const media::AudioBus& data, base::TimeTicks time) override {
    // Make sure the source delivered audio data on the right thread.
    EXPECT_EQ(audio_task_runner_->BelongsToCurrentThread(),
              expect_data_on_audio_task_runner_);

    EXPECT_EQ(time, expected_time_);
    EXPECT_EQ(data.channels(), expected_channels_);
    EXPECT_EQ(data.frames(), expected_frames_);

    if (expected_data_) {
      bool unexpected_data = false;

      for (int ch = 0; ch < data.channels(); ++ch) {
        const float* actual_channel_data = data.channel(ch);
        const float* expected_channel_data = expected_data_->channel(ch);

        for (int i = 0; i < data.frames(); ++i) {
          // If we use ASSERT_EQ here, the test will hang, since |on_data_| will
          // never be called.
          EXPECT_EQ(actual_channel_data[i], expected_channel_data[i]);

          // Force an early exit to prevent log spam from EXPECT_EQ.
          if (actual_channel_data[i] != expected_channel_data[i]) {
            unexpected_data = true;
            break;
          }
        }

        if (unexpected_data)
          break;
      }
    }

    // Call this after all expectations are checked, to prevent test from
    // setting new expectations on the main thread.
    std::move(on_data_).Run();
  }

  void OnSetFormat(const media::AudioParameters& params) override {
    // Make sure the source changed parameters data on the right thread.
    if (expect_data_on_audio_task_runner_) {
      EXPECT_TRUE(audio_task_runner_->BelongsToCurrentThread());
    } else {
      EXPECT_TRUE(main_task_runner_->BelongsToCurrentThread());
    }

    // Make sure that the audio thread is different from the main thread (it
    // would be a test error if it wasn't, as it would be impossible for the
    // check above to fail).
    ASSERT_NE(audio_task_runner_->BelongsToCurrentThread(),
              main_task_runner_->BelongsToCurrentThread());

    // This should only be called once per format change.
    EXPECT_FALSE(did_receive_format_change_);

    EXPECT_EQ(params.sample_rate(), expected_sample_rate_);
    EXPECT_EQ(params.channels(), expected_channels_);
    EXPECT_EQ(params.frames_per_buffer(), expected_frames_);

    did_receive_format_change_ = true;
  }

  void ClearDidReceiveFormatChange() { did_receive_format_change_ = false; }

  bool did_receive_format_change() const { return did_receive_format_change_; }

 public:
  int expected_channels_ = 0;
  int expected_frames_ = 0;
  int expected_sample_rate_ = 0;
  bool expect_data_on_audio_task_runner_ = true;
  raw_ptr<media::AudioBus, DanglingUntriaged> expected_data_ = nullptr;
  base::TimeTicks expected_time_;

  bool did_receive_format_change_ = false;

  base::OnceClosure on_data_;

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner_;
};

}  // namespace

class PushableMediaStreamAudioSourceTest
    : public ::testing::TestWithParam<bool> {
 public:
  PushableMediaStreamAudioSourceTest() {
    // Use the IO thread for testing purposes. This is stricter than an audio
    // sequenced task runner needs to be.
    audio_task_runner_ = platform_->GetIOTaskRunner();
    main_task_runner_ = scheduler::GetSingleThreadTaskRunnerForTesting();

    auto pushable_audio_source =
        std::make_unique<PushableMediaStreamAudioSource>(main_task_runner_,
                                                         audio_task_runner_);
    pushable_audio_source_ = pushable_audio_source.get();
    broker_ = pushable_audio_source->GetBroker();
    broker_->SetShouldDeliverAudioOnAudioTaskRunner(
        ShouldDeliverAudioOnAudioTaskRunner());
    stream_source_ = MakeGarbageCollected<MediaStreamSource>(
        "dummy_source_id", MediaStreamSource::kTypeAudio, "dummy_source_name",
        false /* remote */, std::move(pushable_audio_source));
    stream_component_ = MakeGarbageCollected<MediaStreamComponentImpl>(
        stream_source_->Id(), stream_source_,
        std::make_unique<MediaStreamAudioTrack>(true /* is_local_track */));
  }

  void TearDown() override {
    stream_source_ = nullptr;
    stream_component_ = nullptr;
    WebHeap::CollectAllGarbageForTesting();
  }

  bool ConnectSourceToTrack() {
    return pushable_audio_source_->ConnectToInitializedTrack(stream_component_);
  }

  void SendEmptyBufferAndVerifyParams(FakeMediaStreamAudioSink* fake_sink,
                                      int channels,
                                      int frames,
                                      int sample_rate,
                                      bool expect_format_change) {
    SendDataAndVerifyParams(fake_sink, channels, frames, sample_rate,
                            expect_format_change, nullptr, nullptr);
  }

  void SendDataAndVerifyParams(FakeMediaStreamAudioSink* fake_sink,
                               int channels,
                               int frames,
                               int sample_rate,
                               bool expect_format_change,
                               scoped_refptr<media::AudioBuffer> buffer,
                               media::AudioBus* expected_data) {
    fake_sink->ClearDidReceiveFormatChange();

    if (expect_format_change) {
      fake_sink->SetupNewAudioParameterExpectations(channels, frames,
                                                    sample_rate);
    }

    if (!buffer) {
      base::TimeTicks timestamp = base::TimeTicks::Now();
      buffer = media::AudioBuffer::CreateEmptyBuffer(
          media::GuessChannelLayout(channels), channels, sample_rate, frames,
          timestamp - base::TimeTicks());
    }

    base::RunLoop run_loop;
    fake_sink->SetDataTimeExpectation(
        base::TimeTicks() + buffer->timestamp(), expected_data,
        ShouldDeliverAudioOnAudioTaskRunner(), run_loop.QuitClosure());
    broker_->PushAudioData(std::move(buffer));
    run_loop.Run();

    EXPECT_EQ(fake_sink->did_receive_format_change(), expect_format_change);
  }

  bool ShouldDeliverAudioOnAudioTaskRunner() const { return GetParam(); }

 protected:
  test::TaskEnvironment task_environment_;

  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;

  Persistent<MediaStreamSource> stream_source_;
  Persistent<MediaStreamComponent> stream_component_;

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner_;

  raw_ptr<PushableMediaStreamAudioSource, DanglingUntriaged>
      pushable_audio_source_;
  scoped_refptr<PushableMediaStreamAudioSource::Broker> broker_;
};

TEST_P(PushableMediaStreamAudioSourceTest, ConnectAndStop) {
  EXPECT_EQ(MediaStreamSource::kReadyStateLive,
            stream_source_->GetReadyState());
  EXPECT_FALSE(pushable_audio_source_->IsRunning());

  EXPECT_TRUE(ConnectSourceToTrack());
  EXPECT_EQ(MediaStreamSource::kReadyStateLive,
            stream_source_->GetReadyState());
  EXPECT_TRUE(pushable_audio_source_->IsRunning());

  // If the pushable source stops, the MediaStreamSource should stop.
  pushable_audio_source_->StopSource();
  EXPECT_EQ(MediaStreamSource::kReadyStateEnded,
            stream_source_->GetReadyState());
  EXPECT_FALSE(pushable_audio_source_->IsRunning());
}

TEST_P(PushableMediaStreamAudioSourceTest, FramesPropagateToSink) {
  EXPECT_TRUE(ConnectSourceToTrack());
  FakeMediaStreamAudioSink fake_sink(main_task_runner_, audio_task_runner_);

  WebMediaStreamAudioSink::AddToAudioTrack(
      &fake_sink, WebMediaStreamTrack(stream_component_.Get()));

  constexpr int kChannels = 1;
  constexpr int kFrames = 256;
  constexpr int kSampleRate = 8000;

  // The first audio data pushed should trigger a call to OnSetFormat().
  SendEmptyBufferAndVerifyParams(&fake_sink, kChannels, kFrames, kSampleRate,
                                 /*expect_format_change=*/true);

  // Using the same audio parameters should not trigger OnSetFormat().
  SendEmptyBufferAndVerifyParams(&fake_sink, kChannels, kFrames, kSampleRate,
                                 /*expect_format_change=*/false);

  // Format changes should trigger OnSetFormat().
  SendEmptyBufferAndVerifyParams(&fake_sink, kChannels * 2, kFrames * 4,
                                 /*sample_rate=*/44100,
                                 /*expect_format_change=*/true);

  WebMediaStreamAudioSink::RemoveFromAudioTrack(
      &fake_sink, WebMediaStreamTrack(stream_component_.Get()));
}

TEST_P(PushableMediaStreamAudioSourceTest, ConvertsFormatInternally) {
  EXPECT_TRUE(ConnectSourceToTrack());
  FakeMediaStreamAudioSink fake_sink(main_task_runner_, audio_task_runner_);

  WebMediaStreamAudioSink::AddToAudioTrack(
      &fake_sink, WebMediaStreamTrack(stream_component_.Get()));

  constexpr media::ChannelLayout kChannelLayout =
      media::ChannelLayout::CHANNEL_LAYOUT_STEREO;
  constexpr int kChannels = 2;
  constexpr int kSampleRate = 8000;
  constexpr int kFrames = 256;
  constexpr base::TimeDelta kDefaultTimeStamp = base::Milliseconds(123);

  auto interleaved_buffer = media::AudioBuffer::CreateBuffer(
      media::SampleFormat::kSampleFormatF32, kChannelLayout, kChannels,
      kSampleRate, kFrames);
  interleaved_buffer->set_timestamp(kDefaultTimeStamp);

  // Create interleaved data, with negative values on the second channel.
  float* interleaved_buffer_data =
      reinterpret_cast<float*>(interleaved_buffer->channel_data()[0]);
  for (int i = 0; i < kFrames; ++i) {
    float value = static_cast<float>(i) / kFrames;

    interleaved_buffer_data[0] = value;
    interleaved_buffer_data[1] = -value;
    interleaved_buffer_data += 2;
  }

  // Create reference planar data.
  auto expected_data = media::AudioBus::Create(kChannels, kFrames);
  float* bus_data_ch_0 = expected_data->channel(0);
  float* bus_data_ch_1 = expected_data->channel(1);
  for (int i = 0; i < kFrames; ++i) {
    float value = static_cast<float>(i) / kFrames;
    bus_data_ch_0[i] = value;
    bus_data_ch_1[i] = -value;
  }

  // Sanity check.
  DCHECK(!expected_data->AreFramesZero());

  // Send the data to the pushable source, which should internally convert the
  // interleaved data to planar data before delivering it to sinks.
  SendDataAndVerifyParams(&fake_sink, kChannels, kFrames, kSampleRate,
                          /*expect_format_change=*/true,
                          std::move(interleaved_buffer), expected_data.get());

  auto planar_buffer = media::AudioBuffer::CopyFrom(
      kSampleRate, kDefaultTimeStamp, expected_data.get());

  // The pushable source shouldn't have to convert data internally, and should
  // just wrap it.
  SendDataAndVerifyParams(&fake_sink, kChannels, kFrames, kSampleRate,
                          /*expect_format_change=*/false,
                          std::move(planar_buffer), expected_data.get());

  WebMediaStreamAudioSink::RemoveFromAudioTrack(
      &fake_sink, WebMediaStreamTrack(stream_component_.Get()));
}

// Tests with audio delivered on a dedicated audio task (GetParam() == true) and
// using the calling task (GetParam() == false).
INSTANTIATE_TEST_SUITE_P(/*no prefix*/,
                         PushableMediaStreamAudioSourceTest,
                         ::testing::Bool());

}  // namespace blink
