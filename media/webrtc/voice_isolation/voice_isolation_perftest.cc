// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/voice_isolation/voice_isolation.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <numbers>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gmock_expected_support.h"
#include "base/time/time.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/base/channel_layout.h"
#include "media/webrtc/voice_isolation/band_split_voice_isolation.h"
#include "media/webrtc/voice_isolation/buffered_voice_isolation.h"
#include "media/webrtc/voice_isolation/tflite_voice_isolation.h"
#include "media/webrtc/voice_isolation/voice_isolation_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"
#include "third_party/tflite/src/tensorflow/lite/model_builder.h"

// Local comparison tool for measuring Voice Isolation CPU cost per call. This
// benchmark is not run by continuous integration; by default it runs on the
// small bundled stateful test model, so pass `--model=<path>` to measure
// production model numbers.
namespace media {
namespace {

// Optional command-line switch (`--model=<path>`) to benchmark a custom or
// production TFLite model instead of the bundled stateful test model.
constexpr char kModelSwitch[] = "model";
// Optional command-line switch (`--cache-trash-mb=<mib>`) controlling the size
// in MiB of the scratch slice swept before each benchmarked call to evict CPU
// caches (`0` disables cache trashing; defaults to `0`).
constexpr char kCacheTrashMbSwitch[] = "cache-trash-mb";
constexpr char kDefaultStoryName[] = "stateful_test_model";

constexpr int kWarmupIterations = 20;
// BufferedVoiceIsolation invokes the inner 20 ms STFT + TFLite pipeline on
// every second 10 ms frame, so use an even count of 10 ms frames.
constexpr int kProcessAudioIterations = 160;
static_assert(kProcessAudioIterations %
                  BufferedVoiceIsolation::kNumBufferedFrames ==
              0);
constexpr int kTfLiteProcessAudioIterations = 80;
constexpr int kClearBuffersIterations = 40;

// Stride of the scratch buffer used to evict CPU data caches between
// benchmarked calls, simulating the ~9.65 ms of unrelated Chrome work between
// 10 ms real-time audio callbacks. Each `Trash()` call touches a `slice_bytes_`
// slice at 64-byte cache-line granularity while cycling across a
// `2 * slice_bytes_` buffer.
constexpr size_t kCacheLineBytes = 64;
constexpr size_t kBytesPerMb = 1024 * 1024;

constexpr size_t kNumDftCalls = 16;
constexpr uint64_t kNoiseSeed = 1;

constexpr float kToneFrequencyHz = 1000.0f;
constexpr float kToneAmplitude = 0.5f;

size_t GetCacheTrashSliceBytes() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (!command_line.HasSwitch(kCacheTrashMbSwitch)) {
    return 0;
  }

  size_t slice_mb = 0;
  CHECK(base::StringToSizeT(
      command_line.GetSwitchValueASCII(kCacheTrashMbSwitch), &slice_mb));
  return slice_mb * kBytesPerMb;
}

class CpuCacheTrasher {
 public:
  explicit CpuCacheTrasher(size_t slice_bytes)
      : slice_bytes_(slice_bytes), buffer_(2 * slice_bytes, 1u) {}

  // Touches every cache line in the next `slice_bytes_` slice of `buffer_` to
  // evict the voice isolation pipeline and model state from the CPU caches
  // between calls.
  void Trash() {
    if (slice_bytes_ == 0) {
      return;
    }
    const base::span<uint8_t> slice =
        base::span(buffer_).subspan(offset_, slice_bytes_);
    uint8_t acc = sink_;
    for (size_t i = 0; i < slice.size(); i += kCacheLineBytes) {
      slice[i] += 1u;
      acc ^= slice[i];
    }
    sink_ = acc;
    offset_ = (offset_ + slice_bytes_) % buffer_.size();
  }

 private:
  const size_t slice_bytes_;
  std::vector<uint8_t> buffer_;
  size_t offset_ = 0;
  volatile uint8_t sink_ = 0;
};

struct BenchmarkModel {
  std::unique_ptr<tflite::FlatBufferModel> model;
  std::string story_name;
};

// Loads the model specified by `--model=<path>` and its perf story name, or
// the bundled stateful test model when the flag is not provided.
BenchmarkModel LoadBenchmarkModel() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (!command_line.HasSwitch(kModelSwitch)) {
    return {LoadVoiceIsolationTestModel(), kDefaultStoryName};
  }

  const base::FilePath model_path =
      command_line.GetSwitchValuePath(kModelSwitch);
  return {
      tflite::FlatBufferModel::BuildFromFile(model_path.AsUTF8Unsafe().c_str()),
      model_path.BaseName().AsUTF8Unsafe()};
}

}  // namespace

// Benchmarks end-to-end `VoiceIsolation::ProcessAudio()` (10 ms frames at
// 48 kHz) and `VoiceIsolation::ClearBuffers()` (buffering, 48 kHz STFT,
// 16 kHz speech mask estimator, and TFLite resource variable reset).
TEST(VoiceIsolationPerfTest, EndToEndProcessAudioAndClearBuffers) {
  const BenchmarkModel benchmark_model = LoadBenchmarkModel();
  ASSERT_TRUE(benchmark_model.model);

  const AudioParameters audio_params(
      AudioParameters::AUDIO_PCM_LINEAR, ChannelLayoutConfig::Mono(),
      VoiceIsolation::kSampleRate, VoiceIsolation::kFrameSize);
  std::unique_ptr<VoiceIsolation> voice_isolation =
      VoiceIsolation::Create(benchmark_model.model.get(), audio_params);
  ASSERT_TRUE(voice_isolation);

  std::unique_ptr<AudioBus> input_bus =
      AudioBus::Create(1, VoiceIsolation::kFrameSize);
  std::unique_ptr<AudioBus> output_bus =
      AudioBus::Create(1, VoiceIsolation::kFrameSize);

  // Populate the input bus with a 1 kHz tone so all DSP stages and model
  // state updates operate on non-zero signal values.
  constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;
  base::span<float> input_channel = input_bus->channel(0);
  for (size_t i = 0; i < VoiceIsolation::kFrameSize; ++i) {
    const float time_s =
        static_cast<float>(i) / static_cast<float>(VoiceIsolation::kSampleRate);
    input_channel[i] =
        kToneAmplitude * std::sin(kTwoPi * kToneFrequencyHz * time_s);
  }
  output_bus->Zero();

  for (int i = 0; i < kWarmupIterations; ++i) {
    voice_isolation->ProcessAudio(*input_bus, *output_bus);
  }

  CpuCacheTrasher cache_trasher(GetCacheTrashSliceBytes());

  base::TimeDelta total_process_time;
  for (int i = 0; i < kProcessAudioIterations; ++i) {
    cache_trasher.Trash();
    const base::TimeTicks process_start = base::TimeTicks::Now();
    voice_isolation->ProcessAudio(*input_bus, *output_bus);
    total_process_time += base::TimeTicks::Now() - process_start;
  }
  const double process_us_per_frame =
      total_process_time.InMicrosecondsF() / kProcessAudioIterations;

  base::TimeDelta total_clear_time;
  for (int i = 0; i < kClearBuffersIterations; ++i) {
    cache_trasher.Trash();
    const base::TimeTicks clear_start = base::TimeTicks::Now();
    voice_isolation->ClearBuffers();
    total_clear_time += base::TimeTicks::Now() - clear_start;
  }
  const double clear_us_per_call =
      total_clear_time.InMicrosecondsF() / kClearBuffersIterations;

  perf_test::PerfResultReporter reporter("voice_isolation",
                                         benchmark_model.story_name);
  reporter.RegisterImportantMetric(".process_audio_10ms_frame", "us");
  reporter.RegisterImportantMetric(".clear_buffers", "us");
  reporter.AddResult(".process_audio_10ms_frame", process_us_per_frame);
  reporter.AddResult(".clear_buffers", clear_us_per_call);
}

// Benchmarks `TfLiteVoiceIsolation::ProcessAudio()` (20 ms 16 kHz spectral
// inference + speech mask estimation + output mix) and
// `TfLiteVoiceIsolation::ClearBuffers()` (16 kHz speech mask estimator clear +
// TFLite resource variable reset).
TEST(VoiceIsolationPerfTest, TfLiteProcessAudioAndClearBuffers) {
  const BenchmarkModel benchmark_model = LoadBenchmarkModel();
  ASSERT_TRUE(benchmark_model.model);

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<TfLiteVoiceIsolation> tflite_isolation,
      TfLiteVoiceIsolation::MaybeCreate(benchmark_model.model.get()));
  ASSERT_TRUE(tflite_isolation);

  const size_t dft_size =
      tflite_isolation->FrameSize() / BandSplitVoiceIsolation::kNumDfts;
  const std::vector<std::vector<float>> input_dfts =
      MakeNoiseDfts(kNoiseSeed, dft_size, kNumDftCalls);
  std::vector<float> output_dfts(tflite_isolation->FrameSize(), 0.0f);

  for (int i = 0; i < kWarmupIterations; ++i) {
    const std::vector<float>& dft_pair =
        input_dfts[static_cast<size_t>(i) % input_dfts.size()];
    tflite_isolation->ProcessAudio(dft_pair, output_dfts);
  }

  CpuCacheTrasher cache_trasher(GetCacheTrashSliceBytes());

  base::TimeDelta total_process_time;
  for (int i = 0; i < kTfLiteProcessAudioIterations; ++i) {
    const std::vector<float>& dft_pair =
        input_dfts[static_cast<size_t>(i) % input_dfts.size()];
    cache_trasher.Trash();
    const base::TimeTicks process_start = base::TimeTicks::Now();
    tflite_isolation->ProcessAudio(dft_pair, output_dfts);
    total_process_time += base::TimeTicks::Now() - process_start;
  }
  const double process_us_per_call =
      total_process_time.InMicrosecondsF() / kTfLiteProcessAudioIterations;

  base::TimeDelta total_clear_time;
  for (int i = 0; i < kClearBuffersIterations; ++i) {
    cache_trasher.Trash();
    const base::TimeTicks clear_start = base::TimeTicks::Now();
    tflite_isolation->ClearBuffers();
    total_clear_time += base::TimeTicks::Now() - clear_start;
  }
  const double clear_us_per_call =
      total_clear_time.InMicrosecondsF() / kClearBuffersIterations;

  perf_test::PerfResultReporter reporter("tflite_voice_isolation",
                                         benchmark_model.story_name);
  reporter.RegisterImportantMetric(".process_audio_20ms_frame", "us");
  reporter.RegisterImportantMetric(".clear_buffers", "us");
  reporter.AddResult(".process_audio_20ms_frame", process_us_per_call);
  reporter.AddResult(".clear_buffers", clear_us_per_call);
}

}  // namespace media
