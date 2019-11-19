// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/time/time.h"
#include "media/base/audio_converter.h"
#include "media/base/fake_audio_render_callback.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

namespace media {

static const int kBenchmarkIterations = 200000;

// InputCallback that zero's out the provided AudioBus.
class NullInputProvider : public AudioConverter::InputCallback {
 public:
  NullInputProvider() = default;
  ~NullInputProvider() override = default;

  double ProvideInput(AudioBus* audio_bus, uint32_t frames_delayed) override {
    audio_bus->Zero();
    return 1;
  }
};

void RunConvertBenchmark(const AudioParameters& in_params,
                         const AudioParameters& out_params,
                         bool fifo,
                         const std::string& trace_name) {
  NullInputProvider fake_input1;
  NullInputProvider fake_input2;
  NullInputProvider fake_input3;
  std::unique_ptr<AudioBus> output_bus = AudioBus::Create(out_params);

  AudioConverter converter(in_params, out_params, !fifo);
  converter.AddInput(&fake_input1);
  converter.AddInput(&fake_input2);
  converter.AddInput(&fake_input3);

  base::TimeTicks start = base::TimeTicks::Now();
  for (int i = 0; i < kBenchmarkIterations; ++i) {
    converter.Convert(output_bus.get());
  }
  double runs_per_second = kBenchmarkIterations /
                           (base::TimeTicks::Now() - start).InSecondsF();
  perf_test::PerfResultReporter reporter("audio_converter", trace_name);
  reporter.RegisterImportantMetric("", "runs/s");
  reporter.AddResult("", runs_per_second);
}

TEST(AudioConverterPerfTest, ConvertBenchmark) {
  // Create input and output parameters to convert between the two most common
  // sets of parameters (as indicated via UMA data).
  AudioParameters input_params(AudioParameters::AUDIO_PCM_LINEAR,
                               CHANNEL_LAYOUT_MONO, 48000, 2048);
  AudioParameters output_params(AudioParameters::AUDIO_PCM_LINEAR,
                                CHANNEL_LAYOUT_STEREO, 44100, 440);

  RunConvertBenchmark(input_params, output_params, false, "convert");
}

TEST(AudioConverterPerfTest, ConvertBenchmarkFIFO) {
  // Create input and output parameters to convert between common buffer sizes
  // without any resampling for the FIFO vs no FIFO benchmarks.
  AudioParameters input_params(AudioParameters::AUDIO_PCM_LINEAR,
                               CHANNEL_LAYOUT_STEREO,
                               44100,
                               2048);
  AudioParameters output_params(AudioParameters::AUDIO_PCM_LINEAR,
                                CHANNEL_LAYOUT_STEREO, 44100, 440);

  RunConvertBenchmark(input_params, output_params, true, "convert_fifo_only");
  RunConvertBenchmark(input_params, output_params, false,
                      "convert_pass_through");
}

} // namespace media
