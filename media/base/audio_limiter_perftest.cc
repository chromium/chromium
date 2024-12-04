// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_limiter.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "media/audio/simple_sources.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/fake_audio_render_callback.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/41494069): Update these tests once AudioBus is spanified..
#pragma allow_unsafe_buffers
#endif

namespace media {

constexpr int kSampleRate = 48000;
constexpr int kBenchmarkIterations = 20000;
constexpr base::TimeDelta kBufferDuration = base::Milliseconds(20);

void RunConvertBenchmark(const AudioParameters& params,
                         float amplitude,
                         const std::string& trace_name) {
  constexpr int kFrequency = 440;

  SineWaveAudioSource sine_source(params.channels(), kFrequency,
                                  params.sample_rate());

  auto input_bus = AudioBus::Create(params);
  auto output_bus = AudioBus::Create(params);

  sine_source.OnMoreData(base::TimeDelta(), base::TimeTicks(), {},
                         input_bus.get());

  for (int ch = 0; ch < input_bus->channels(); ++ch) {
    float* channel_data = input_bus->channel(ch);
    for (int i = 0; i < input_bus->frames(); ++i) {
      channel_data[i] *= amplitude;
    }
  }

  AudioLimiter::OutputChannels output_channels;
  for (int ch = 0; ch < output_bus->channels(); ++ch) {
    output_channels.emplace_back(
        reinterpret_cast<uint8_t*>(output_bus->channel(ch)),
        output_bus->frames() * sizeof(float));
  }

  AudioLimiter limiter(params.sample_rate(), params.channels());

  base::TimeTicks start = base::TimeTicks::Now();
  for (int i = 0; i < kBenchmarkIterations; ++i) {
    limiter.LimitPeaks(*input_bus, output_channels, base::DoNothing());
  }

  base::TimeDelta elapsed_time = base::TimeTicks::Now() - start;
  base::TimeDelta benchmark_data_duration =
      kBenchmarkIterations * kBufferDuration;
  // How may ms of data can the AudioLimiter process in 1ms. Higher is better.
  double processing_ratio = std::round(benchmark_data_duration / elapsed_time);
  perf_test::PerfResultReporter reporter("audio_limiter", trace_name);
  reporter.RegisterImportantMetric("", "ms_of_data/ms");
  reporter.AddResult("", processing_ratio);
}

TEST(AudioLimiterBenchmark, LimitPeaksBenchmark_NoLimiting) {
  // Create input and output parameters to convert between the two most common
  // sets of parameters (as indicated via UMA data).
  AudioParameters input_params(
      AudioParameters::AUDIO_PCM_LINEAR, media::ChannelLayoutConfig::Mono(),
      kSampleRate,
      AudioTimestampHelper::TimeToFrames(kBufferDuration, kSampleRate));

  RunConvertBenchmark(input_params, 0.5, "NoLimitting");
}

TEST(AudioLimiterBenchmark, LimitPeaksBenchmark_Limiting) {
  // Create input and output parameters to convert between the two most common
  // sets of parameters (as indicated via UMA data).
  AudioParameters input_params(
      AudioParameters::AUDIO_PCM_LINEAR, media::ChannelLayoutConfig::Mono(),
      kSampleRate,
      AudioTimestampHelper::TimeToFrames(kBufferDuration, kSampleRate));

  RunConvertBenchmark(input_params, 2.0, "Limitting");
}

}  // namespace media
