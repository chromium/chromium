// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/mediastream/webaudio_media_stream_audio_sink.h"

#include <stddef.h>

#include <memory>

#include "base/test/bind.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_latency.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_pull_fifo.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component_impl.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

class WebAudioMediaStreamAudioSinkTest : public testing::Test {
 public:
  void TearDown() override {
    source_provider_.reset();
    component_ = nullptr;
    WebHeap::CollectAllGarbageForTesting();
  }

 protected:
  void Configure(int source_sample_rate,
                 int source_buffer_size,
                 int context_sample_rate,
                 base::TimeDelta platform_buffer_duration) {
    source_params_.Reset(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         media::ChannelLayoutConfig::Mono(), source_sample_rate,
                         source_buffer_size);
    sink_params_.Reset(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                       media::ChannelLayoutConfig::Stereo(),
                       context_sample_rate,
                       WebAudioMediaStreamAudioSink::kWebAudioRenderBufferSize);
    sink_bus_ = media::AudioBus::Create(sink_params_);
    auto* audio_source = MakeGarbageCollected<MediaStreamSource>(
        String::FromUTF8("dummy_source_id"), MediaStreamSource::kTypeAudio,
        String::FromUTF8("dummy_source_name"), /*remote=*/false,
        /*platform_source=*/nullptr);
    component_ = MakeGarbageCollected<MediaStreamComponentImpl>(
        String::FromUTF8("audio_track"), audio_source,
        std::make_unique<MediaStreamAudioTrack>(true));
    source_provider_ = std::make_unique<WebAudioMediaStreamAudioSink>(
        component_, context_sample_rate, platform_buffer_duration);
    source_provider_->OnSetFormat(source_params_);
  }

  test::TaskEnvironment task_environment_;
  media::AudioParameters source_params_;
  media::AudioParameters sink_params_;
  std::unique_ptr<media::AudioBus> sink_bus_;
  Persistent<MediaStreamComponent> component_;
  std::unique_ptr<WebAudioMediaStreamAudioSink> source_provider_;
};

TEST_F(WebAudioMediaStreamAudioSinkTest, VerifyDataFlow) {
  Configure(/*source_sample_rate=*/48000, /*source_buffer_size=*/480,
            /*context_sample_rate=*/44100,
            /*platform_buffer_duration=*/base::Milliseconds(10));

  // Point the WebVector into memory owned by |sink_bus_|.
  WebVector<float*> audio_data(static_cast<size_t>(sink_bus_->channels()));
  for (int i = 0; i < sink_bus_->channels(); ++i)
    audio_data[i] = sink_bus_->channel(i);

  // Enable the |source_provider_| by asking for data. This will inject
  // source_params_.frames_per_buffer() of zero into the resampler since there
  // no available data in the FIFO.
  source_provider_->ProvideInput(audio_data, sink_params_.frames_per_buffer());
  EXPECT_EQ(0, sink_bus_->channel(0)[0]);

  // Create a source AudioBus with channel data filled with non-zero values.
  const std::unique_ptr<media::AudioBus> source_bus =
      media::AudioBus::Create(source_params_);
  std::fill(source_bus->channel(0),
            source_bus->channel(0) + source_bus->frames(), 0.5f);

  // Deliver data to |source_provider_|.
  base::TimeTicks estimated_capture_time = base::TimeTicks::Now();
  source_provider_->OnData(*source_bus, estimated_capture_time);

  // Consume the first packet in the resampler, which contains only zeros.
  // And the consumption of the data will trigger pulling the real packet from
  // the source provider FIFO into the resampler.
  // Note that we need to count in the provideInput() call a few lines above.
  for (int i = sink_params_.frames_per_buffer();
       i < source_params_.frames_per_buffer();
       i += sink_params_.frames_per_buffer()) {
    sink_bus_->Zero();
    source_provider_->ProvideInput(audio_data,
                                   sink_params_.frames_per_buffer());
    EXPECT_DOUBLE_EQ(0.0, sink_bus_->channel(0)[0]);
    EXPECT_DOUBLE_EQ(0.0, sink_bus_->channel(1)[0]);
  }

  // Make a second data delivery.
  estimated_capture_time +=
      source_bus->frames() * base::Seconds(1) / source_params_.sample_rate();
  source_provider_->OnData(*source_bus, estimated_capture_time);

  // Verify that non-zero data samples are present in the results of the
  // following calls to provideInput().
  for (int i = 0; i < source_params_.frames_per_buffer();
       i += sink_params_.frames_per_buffer()) {
    sink_bus_->Zero();
    source_provider_->ProvideInput(audio_data,
                                   sink_params_.frames_per_buffer());
    EXPECT_NEAR(0.5f, sink_bus_->channel(0)[0], 0.001f);
    EXPECT_NEAR(0.5f, sink_bus_->channel(1)[0], 0.001f);
    EXPECT_DOUBLE_EQ(sink_bus_->channel(0)[0], sink_bus_->channel(1)[0]);
  }
}

TEST_F(WebAudioMediaStreamAudioSinkTest,
       DeleteSourceProviderBeforeStoppingTrack) {
  Configure(/*source_sample_rate=*/48000, /*source_buffer_size=*/480,
            /*context_sample_rate=*/44100,
            /*platform_buffer_duration=*/base::Milliseconds(10));

  source_provider_.reset();

  // Stop the audio track.
  MediaStreamAudioTrack::From(component_.Get())->Stop();
}

TEST_F(WebAudioMediaStreamAudioSinkTest,
       StopTrackBeforeDeletingSourceProvider) {
  Configure(/*source_sample_rate=*/48000, /*source_buffer_size=*/480,
            /*context_sample_rate=*/44100,
            /*platform_buffer_duration=*/base::Milliseconds(10));

  // Stop the audio track.
  MediaStreamAudioTrack::From(component_.Get())->Stop();

  // Delete the source provider.
  source_provider_.reset();
}

class WebAudioMediaStreamAudioSinkFifoTest
    : public WebAudioMediaStreamAudioSinkTest,
      public testing::WithParamInterface<
          std::tuple<int, int, float, float, int>> {};

TEST_P(WebAudioMediaStreamAudioSinkFifoTest, VerifyFifo) {
  int source_sample_rate = std::get<0>(GetParam());
  int context_sample_rate = std::get<1>(GetParam());
  float device_callback_irregularity_coefficient = std::get<2>(GetParam());
  float produce_offset_coefficient = std::get<3>(GetParam());
  int source_buffer_size = std::get<4>(GetParam());

  int context_buffer_size =
      media::AudioLatency::GetHighLatencyBufferSize(context_sample_rate, 0);

  Configure(
      source_sample_rate, source_buffer_size, context_sample_rate,
      audio_utilities::FramesToTime(context_buffer_size, context_sample_rate));

  // 1. Source preparation.
  std::unique_ptr<media::AudioBus> source_bus =
      media::AudioBus::Create(source_params_);
  source_bus->Zero();

  // 2. Sink preparation.

  // Point the WebVector into memory owned by |sink_bus_|.
  WebVector<float*> audio_data(static_cast<size_t>(sink_bus_->channels()));
  for (int i = 0; i < sink_bus_->channels(); ++i) {
    audio_data[i] = sink_bus_->channel(i);
  }

  // FIFO simulating callbacks from AudioContext output.
  auto pull_cb = base::BindLambdaForTesting(
      [&](int frame_delay, media::AudioBus* audio_bus) {
        source_provider_->ProvideInput(audio_data,
                                       sink_params_.frames_per_buffer());
        sink_bus_->CopyTo(audio_bus);
      });
  media::AudioPullFifo pull_fifo(sink_params_.channels(),
                                 sink_params_.frames_per_buffer(), pull_cb);

  media::AudioParameters output_params(
      sink_params_.format(), sink_params_.channel_layout_config(),
      sink_params_.sample_rate(), context_buffer_size);

  std::unique_ptr<media::AudioBus> output_bus =
      media::AudioBus::Create(output_params);

  // 3. Testing.

  // Enable the |source_provider_| by asking for data. This will result in FIFO
  // underruns, since the source data has been rejected until now.
  pull_fifo.Consume(output_bus.get(), output_params.frames_per_buffer());

  // Calculating time in integers, rather than TimeDelta, to avoid rounding
  // errors.
  uint64_t counts_in_second =
      static_cast<uint64_t>(source_params_.sample_rate()) *
      output_params.sample_rate();

  // Values below are, in other words, frames_per_buffer() * counts_in_second /
  // sample_rate().
  uint64_t produce_step =
      static_cast<uint64_t>(source_params_.frames_per_buffer()) *
      output_params.sample_rate();
  uint64_t consume_step =
      static_cast<uint64_t>(output_params.frames_per_buffer()) *
      source_params_.sample_rate();

  uint64_t consume_counter = consume_step;
  uint64_t consume_delay =
      (1 + device_callback_irregularity_coefficient) * consume_step;
  uint64_t counter = produce_offset_coefficient * produce_step;

  uint64_t test_duration_seconds = 5;
  uint64_t max_count = test_duration_seconds * counts_in_second;

  // Enable FIFO stats.
  source_provider_->ResetFifoStatsForTesting();

  // Note: this is an artifitical perfect scheduling; in general,
  // `source_provider_` is not resilient to underruns, and in extreme cases - to
  // overruns.
  for (; counter < max_count; counter += produce_step) {
    // Produce.
    source_provider_->OnData(*source_bus, base::TimeTicks::Min());

    if (consume_counter + consume_delay > counter) {
      continue;
    }

    // It's time to consume!
    while (consume_counter <= counter) {
      pull_fifo.Consume(output_bus.get(), output_params.frames_per_buffer());
      consume_counter += consume_step;
    }  // while
  }    // for

  EXPECT_EQ(0, source_provider_->GetFifoStatsForTesting().underruns);
  EXPECT_EQ(0, source_provider_->GetFifoStatsForTesting().overruns);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WebAudioMediaStreamAudioSinkFifoTest,
    testing::Combine(
        // source_sample_rate
        testing::ValuesIn({16000, 44100, 48000, 96000}),
        // context_sample_rate; 41000 may cause underruns on platforms which
        // do not use power of 2 as a high latency buffer size, since the
        // scheduling in tests won't be ideal.
        testing::ValuesIn({16000, 48000, 96000}),
        // device_callback_irregularity_coefficient
        testing::ValuesIn({0.0f, 1.5f}),
        // produce_offset_coefficient, 0..1
        testing::ValuesIn({0.0f, 0.1f}),
        // source_buffer_size
        testing::ValuesIn({128, 512, 480})));

}  // namespace blink
