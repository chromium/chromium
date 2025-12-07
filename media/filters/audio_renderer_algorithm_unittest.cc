// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The format of these tests are to enqueue a known amount of data and then
// request the exact amount we expect in order to dequeue the known amount of
// data.  This ensures that for any rate we are consuming input data at the
// correct rate.  We always pass in a very large destination buffer with the
// expectation that FillBuffer() will fill as much as it can but no more.

#include "media/filters/audio_renderer_algorithm.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>  // For std::min().
#include <array>
#include <cmath>
#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/heap_array.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/channel_layout.h"
#include "media/base/media_util.h"
#include "media/base/test_helpers.h"
#include "media/base/timestamp_constants.h"
#include "media/filters/wsola_internals.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

constexpr int kFrameSize = 250;
constexpr int kSamplesPerSecond = 3000;
constexpr int kOutputDurationInSec = 10;

static void FillWithSquarePulseTrain(size_t half_pulse_width,
                                     size_t offset,
                                     base::span<float> data) {
  ASSERT_LE(offset, data.size());

  // Fill backward from |offset| - 1 toward zero, starting with -1, alternating
  // between -1 and 1 every |pulse_width| samples.
  float pulse = -1.0f;
  size_t k = 0;
  for (int n = offset - 1; n >= 0; --n, ++k) {
    if (k >= half_pulse_width) {
      pulse = -pulse;
      k = 0;
    }
    data[n] = pulse;
  }

  // Fill forward from |offset| towards the end, starting with 1, alternating
  // between 1 and -1 every |pulse_width| samples.
  pulse = 1.0f;
  k = 0;
  for (size_t n = offset; n < data.size(); ++n, ++k) {
    if (k >= half_pulse_width) {
      pulse = -pulse;
      k = 0;
    }
    data[n] = pulse;
  }
}

static void FillWithSquarePulseTrain(size_t half_pulse_width,
                                     size_t offset,
                                     size_t num_samples,
                                     float* data) {
  FillWithSquarePulseTrain(half_pulse_width, offset,
                           UNSAFE_TODO(base::span<float>(data, num_samples)));
}

static void FillWithSquarePulseTrain(size_t half_pulse_width,
                                     size_t offset,
                                     int channel,
                                     AudioBus* audio_bus) {
  FillWithSquarePulseTrain(half_pulse_width, offset,
                           audio_bus->channel_span(channel));
}

class AudioRendererAlgorithmTest : public testing::Test {
 public:
  AudioRendererAlgorithmTest() : algorithm_(&media_log_) {}

  ~AudioRendererAlgorithmTest() override = default;

  void Initialize() {
    Initialize(CHANNEL_LAYOUT_STEREO, kSampleFormatS16, kSamplesPerSecond,
               kSamplesPerSecond / 10);
  }

  void Initialize(ChannelLayout channel_layout,
                  SampleFormat sample_format,
                  int samples_per_second,
                  int frames_per_buffer) {
    Initialize(
        channel_layout, sample_format, samples_per_second, frames_per_buffer,
        std::vector<bool>(ChannelLayoutToChannelCount(channel_layout), true));
  }

  void Initialize(ChannelLayout channel_layout,
                  SampleFormat sample_format,
                  int samples_per_second,
                  int frames_per_buffer,
                  std::vector<bool> channel_mask) {
    channels_ = ChannelLayoutToChannelCount(channel_layout);
    samples_per_second_ = samples_per_second;
    channel_layout_ = channel_layout;
    sample_format_ = sample_format;
    bytes_per_sample_ = SampleFormatToBytesPerChannel(sample_format);

    media::AudioParameters::Format format =
        media::AudioParameters::AUDIO_PCM_LINEAR;
    if (sample_format == kSampleFormatAc3)
      format = media::AudioParameters::AUDIO_BITSTREAM_AC3;
    else if (sample_format == kSampleFormatEac3)
      format = media::AudioParameters::AUDIO_BITSTREAM_EAC3;
    else if (sample_format == kSampleFormatDts)
      format = media::AudioParameters::AUDIO_BITSTREAM_DTS;

    AudioParameters params(format,
                           ChannelLayoutConfig(channel_layout, channels_),
                           samples_per_second, frames_per_buffer);
    is_bitstream_format_ = params.IsBitstreamFormat();
    bool is_encrypted = false;
    algorithm_.Initialize(params, is_encrypted);
    algorithm_.SetChannelMask(std::move(channel_mask));
    FillAlgorithmQueueUntilFull();
  }

  base::TimeDelta BufferedTime() {
    return AudioTimestampHelper::FramesToTime(algorithm_.BufferedFrames(),
                                              samples_per_second_);
  }

  scoped_refptr<AudioBuffer> MakeBuffer(int frame_size) {
    // The value of the data is meaningless; we just want non-zero data to
    // differentiate it from muted data.
    scoped_refptr<AudioBuffer> buffer;
    switch (sample_format_) {
      case kSampleFormatAc3:
      case kSampleFormatEac3:
        buffer = MakeBitstreamAudioBuffer(
            sample_format_, channel_layout_,
            ChannelLayoutToChannelCount(channel_layout_), samples_per_second_,
            1, 1, frame_size, kFrameSize, kNoTimestamp);
        break;
      case kSampleFormatU8:
        buffer = MakeAudioBuffer<uint8_t>(
            sample_format_, channel_layout_,
            ChannelLayoutToChannelCount(channel_layout_), samples_per_second_,
            1, 1, frame_size, kNoTimestamp);
        break;
      case kSampleFormatS16:
        buffer = MakeAudioBuffer<int16_t>(
            sample_format_, channel_layout_,
            ChannelLayoutToChannelCount(channel_layout_), samples_per_second_,
            1, 1, frame_size, kNoTimestamp);
        break;
      case kSampleFormatS32:
        buffer = MakeAudioBuffer<int32_t>(
            sample_format_, channel_layout_,
            ChannelLayoutToChannelCount(channel_layout_), samples_per_second_,
            1, 1, frame_size, kNoTimestamp);
        break;
      case kSampleFormatDts:
      case kSampleFormatDtse:
      case kSampleFormatDtsxP2:
        buffer = MakeBitstreamAudioBuffer(
            sample_format_, channel_layout_,
            ChannelLayoutToChannelCount(channel_layout_), samples_per_second_,
            1, 1, frame_size, kFrameSize, kNoTimestamp);
        break;
      default:
        NOTREACHED() << "Unrecognized format " << sample_format_;
    }
    return buffer;
  }

  void FillAlgorithmQueueUntilAdequate() {
    // Note: "adequate" may be <= "full" depending on current latency hint.
    EXPECT_FALSE(algorithm_.IsQueueFull());
    EXPECT_FALSE(algorithm_.IsQueueAdequateForPlayback());
    while (!algorithm_.IsQueueAdequateForPlayback()) {
      // "Adequate" tests may be sensitive to over-filling. Only add one buffer
      // at a time to trigger "adequate" threshold precisely.
      algorithm_.EnqueueBuffer(MakeBuffer(1));
    }
  }

  void FillAlgorithmQueueUntilFull() {
    while (!algorithm_.IsQueueFull()) {
      algorithm_.EnqueueBuffer(MakeBuffer(kFrameSize));
      frames_enqueued_ += kFrameSize;
    }
  }

  bool VerifyAudioData(AudioBus* bus,
                       size_t offset,
                       size_t frames,
                       const float value) {
    for (auto channel_subspan : bus->AllChannelsSubspan(offset, frames)) {
      if (!std::ranges::all_of(channel_subspan,
                               [=](auto s) { return s == value; })) {
        return false;
      }
    }

    return true;
  }

  bool AudioDataIsMuted(AudioBus* audio_data, int frames_written, int offset) {
    return VerifyAudioData(audio_data, offset, frames_written, 0);
  }

  int ComputeConsumedFrames(int initial_frames_enqueued,
                            int initial_frames_buffered) {
    int frame_delta = frames_enqueued_ - initial_frames_enqueued;
    int buffered_delta = algorithm_.BufferedFrames() - initial_frames_buffered;
    int consumed = frame_delta - buffered_delta;

    CHECK_GE(consumed, 0);
    return consumed;
  }

  void TestPlaybackRate(double playback_rate) {
    const int kDefaultBufferSize = algorithm_.samples_per_second() / 100;
    const int kDefaultFramesRequested = kOutputDurationInSec *
        algorithm_.samples_per_second();

    TestPlaybackRate(playback_rate, kDefaultBufferSize, kDefaultFramesRequested,
                     0);
  }

  void TestPlaybackRate(double playback_rate,
                        int buffer_size_in_frames,
                        int total_frames_requested,
                        int dest_offset) {
    int initial_frames_enqueued = frames_enqueued_;

    std::unique_ptr<AudioBus> bus =
        AudioBus::Create(channels_, buffer_size_in_frames);
    bus->ZeroFrames(dest_offset);

    if (playback_rate == 0.0) {
      int frames_written = algorithm_.FillBuffer(
          bus.get(), 0, buffer_size_in_frames, playback_rate);
      EXPECT_EQ(0, frames_written);
      return;
    }

    if (!is_bitstream_format_) {
      // When we switch playback rates (specifically from non-1.0 to 1.0), the
      // BufferedFrames() can change since some internal buffers are cleared.
      // Fill 0 frames to make sure the BufferedFrames() is correct for the
      // |playback_rate|.
      algorithm_.FillBuffer(bus.get(), 0, 0, playback_rate);
    }
    int initial_frames_buffered = algorithm_.BufferedFrames();

    int frames_remaining = total_frames_requested;
    bool first_fill_buffer = true;
    while (frames_remaining > 0) {
      int frames_requested =
          std::min(buffer_size_in_frames - dest_offset, frames_remaining);
      int frames_written = algorithm_.FillBuffer(
          bus.get(), dest_offset, frames_requested, playback_rate);
      ASSERT_GT(frames_written, 0) << "Requested: " << frames_requested
                                   << ", playing at " << playback_rate;

      // Do not check data if it is first pull out and only one frame written.
      // The very first frame out of WSOLA is always zero because of
      // overlap-and-add window, which is zero for the first sample. Therefore,
      // if at very first buffer-fill only one frame is written, that is zero
      // which might cause exception in CheckFakeData().
      if (!first_fill_buffer || frames_written > 1)
        if (!bus->is_bitstream_format()) {
          ASSERT_FALSE(
              AudioDataIsMuted(bus.get(), frames_written, dest_offset));
        }
      first_fill_buffer = false;
      frames_remaining -= frames_written;

      FillAlgorithmQueueUntilFull();
    }

    EXPECT_EQ(algorithm_.BufferedFrames() * channels_ * sizeof(float),
              static_cast<size_t>(algorithm_.GetMemoryUsage()));

    int frames_consumed =
        ComputeConsumedFrames(initial_frames_enqueued, initial_frames_buffered);

    // If playing back at normal speed, we should always get back the same
    // number of bytes requested.
    if (playback_rate == 1.0) {
      EXPECT_EQ(total_frames_requested, frames_consumed);
      return;
    }

    // Otherwise, allow |kMaxAcceptableDelta| difference between the target and
    // actual playback rate.
    // When |kSamplesPerSecond| and |total_frames_requested| are reasonably
    // large, one can expect less than a 1% difference in most cases. In our
    // current implementation, sped up playback is less accurate than slowed
    // down playback, and for playback_rate > 1, playback rate generally gets
    // less and less accurate the farther it drifts from 1 (though this is
    // nonlinear).
    double actual_playback_rate =
        1.0 * frames_consumed / total_frames_requested;
    EXPECT_NEAR(playback_rate, actual_playback_rate, playback_rate / 100.0);
  }

  void TestResamplingWithUnderflow(double playback_rate, bool end_of_stream) {
    // We are only testing the behavior of the resampling case.
    algorithm_.SetPreservesPitch(false);

    if (end_of_stream) {
      algorithm_.MarkEndOfStream();
    } else {
      algorithm_.FlushBuffers();
    }

    const int buffer_size_in_frames = algorithm_.samples_per_second() / 10;
    const int initial_frames_enqueued = frames_enqueued_;

    std::unique_ptr<AudioBus> bus =
        AudioBus::Create(channels_, buffer_size_in_frames);

    FillAlgorithmQueueUntilFull();

    int frames_written;
    int total_frames_written = 0;
    do {
      frames_written = algorithm_.FillBuffer(
          bus.get(), 0, buffer_size_in_frames, playback_rate);

      total_frames_written += frames_written;
    } while (frames_written && algorithm_.BufferedFrames() > 0);

    int input_frames_enqueued = frames_enqueued_ - initial_frames_enqueued;

    int ouput_frames_available =
        static_cast<int>(input_frames_enqueued / playback_rate + 0.5);

    if (end_of_stream) {
      // If we marked the EOS, all data should we played out, possibly with some
      // extra silence.
      EXPECT_GE(total_frames_written, ouput_frames_available);
    } else {
      // If we don't mark the EOS, we expect to have lost some frames because
      // we don't partially handle requests.
      EXPECT_LE(total_frames_written, ouput_frames_available);
    }
  }

  void WsolaTest(double playback_rate) {
    constexpr int kSampleRateHz = 48000;
    constexpr ChannelLayout kChannelLayout = CHANNEL_LAYOUT_STEREO;
    constexpr int kNumFrames = kSampleRateHz / 100;  // 10 milliseconds.

    channels_ = ChannelLayoutToChannelCount(kChannelLayout);
    AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR,
                           ChannelLayoutConfig::FromLayout<kChannelLayout>(),
                           kSampleRateHz, kNumFrames);
    bool is_encrypted = false;
    algorithm_.Initialize(params, is_encrypted);

    // A pulse is 6 milliseconds (even number of samples).
    constexpr int kPulseWidthSamples = 6 * kSampleRateHz / 1000;
    constexpr int kHalfPulseWidthSamples = kPulseWidthSamples / 2;

    // For the ease of implementation get 1 frame every call to FillBuffer().
    std::unique_ptr<AudioBus> output = AudioBus::Create(channels_, 1);

    // Input buffer to inject pulses.
    scoped_refptr<AudioBuffer> input =
        AudioBuffer::CreateBuffer(kSampleFormatPlanarF32,
                                  kChannelLayout,
                                  channels_,
                                  kSampleRateHz,
                                  kPulseWidthSamples);

    const std::vector<uint8_t*>& channel_pointers = input->channel_data();
    std::vector<base::span<float>> input_data(channels_);
    for (int i = 0; i < channels_; ++i) {
      // TODO(crbug.com/373960632): spanify AudioBuffer.
      UNSAFE_TODO(input_data[i] =
                      base::span(reinterpret_cast<float*>(channel_pointers[i]),
                                 static_cast<size_t>(input->frame_count())));
    }

    // Fill |input| channels.
    FillWithSquarePulseTrain(kHalfPulseWidthSamples, 0, kPulseWidthSamples,
                             input_data[0].data());
    FillWithSquarePulseTrain(kHalfPulseWidthSamples, kHalfPulseWidthSamples,
                             kPulseWidthSamples, input_data[1].data());

    // A buffer for the output until a complete pulse is created. Then
    // reference pulse is compared with this buffer.
    std::unique_ptr<AudioBus> pulse_buffer =
        AudioBus::Create(channels_, kPulseWidthSamples);

    const float kTolerance = 0.000001f;
    // Equivalent of 4 seconds.
    const int kNumRequestedPulses = kSampleRateHz * 4 / kPulseWidthSamples;
    for (int n = 0; n < kNumRequestedPulses; ++n) {
      int num_buffered_frames = 0;
      while (num_buffered_frames < kPulseWidthSamples) {
        int num_samples =
            algorithm_.FillBuffer(output.get(), 0, 1, playback_rate);
        ASSERT_LE(num_samples, 1);
        if (num_samples > 0) {
          output->CopyPartialFramesTo(0, num_samples, num_buffered_frames,
                                      pulse_buffer.get());
          num_buffered_frames++;
        } else {
          algorithm_.EnqueueBuffer(input);
        }
      }

      // Pulses in the first half of WSOLA AOL frame are not constructed
      // perfectly. Do not check them.
      if (n > 3) {
         for (int m = 0; m < channels_; ++m) {
           auto pulse_ch = pulse_buffer->channel_span(m);
           auto input_ch = input_data[m];

           // Because of overlap-and-add we might have round off error.
           for (int k = 0; k < kPulseWidthSamples; ++k) {
             ASSERT_NEAR(input_ch[k], pulse_ch[k], kTolerance)
                 << " loop " << n << " channel/sample " << m << "/" << k;
           }
        }
      }

      // Zero out the buffer to be sure the next comparison is relevant.
      pulse_buffer->Zero();
    }
  }

 protected:
  AudioRendererAlgorithm algorithm_;
  NullMediaLog media_log_;
  int frames_enqueued_ = 0;
  int channels_ = 0;
  ChannelLayout channel_layout_ = CHANNEL_LAYOUT_NONE;
  SampleFormat sample_format_ = kUnknownSampleFormat;
  int samples_per_second_ = 0;
  int bytes_per_sample_ = 0;
  bool is_bitstream_format_;
};

TEST_F(AudioRendererAlgorithmTest, InitializeWithLargeParameters) {
  const int kBufferSize = 0.5 * kSamplesPerSecond;
  Initialize(CHANNEL_LAYOUT_MONO, kSampleFormatU8, kSamplesPerSecond,
             kBufferSize);
  EXPECT_LT(kBufferSize, algorithm_.QueueCapacity());
  algorithm_.FlushBuffers();
  EXPECT_LT(kBufferSize, algorithm_.QueueCapacity());
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_Bitstream) {
  Initialize(CHANNEL_LAYOUT_STEREO, kSampleFormatEac3, kSamplesPerSecond,
             kSamplesPerSecond / 100);
  TestPlaybackRate(1.0, kFrameSize, 16 * kFrameSize, /* dest_offset */ 0);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_NormalRate) {
  Initialize();
  TestPlaybackRate(1.0);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_NearlyNormalFasterRate) {
  Initialize();
  TestPlaybackRate(1.0001);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_NearlyNormalSlowerRate) {
  Initialize();
  TestPlaybackRate(0.9999);
}

// This test verifies that the resampling based time stretch algorithms works.
// The range of playback rates in which we use resampling is [0.95, 1.06].
TEST_F(AudioRendererAlgorithmTest, FillBuffer_ResamplingRates) {
  Initialize();
  // WSOLA.
  TestPlaybackRate(0.50);
  TestPlaybackRate(0.95);
  TestPlaybackRate(1.00);
  TestPlaybackRate(1.05);
  TestPlaybackRate(2.00);

  // Resampling.
  algorithm_.SetPreservesPitch(false);
  TestPlaybackRate(0.50);
  TestPlaybackRate(0.95);
  TestPlaybackRate(1.00);
  TestPlaybackRate(1.05);
  TestPlaybackRate(2.00);
}

// This test verifies that we use the right underlying algorithms based on
// the preserves pitch flag and the playback rate.
TEST_F(AudioRendererAlgorithmTest, FillBuffer_FillModes) {
  Initialize();

  // WSOLA.
  algorithm_.SetPreservesPitch(true);

  // Passthrough data when we are close to a playback rate of 1.0.
  TestPlaybackRate(1.00);
  EXPECT_EQ(algorithm_.last_mode_for_testing(),
            AudioRendererAlgorithm::FillBufferMode::kPassthrough);

  // Use WSOLA when we are not close to 1.0.
  TestPlaybackRate(1.05);
  EXPECT_EQ(algorithm_.last_mode_for_testing(),
            AudioRendererAlgorithm::FillBufferMode::kWSOLA);

  // Return to passthrough.
  TestPlaybackRate(1.00);
  EXPECT_EQ(algorithm_.last_mode_for_testing(),
            AudioRendererAlgorithm::FillBufferMode::kPassthrough);

  // Always use resampling when preservesPitch is false.
  algorithm_.SetPreservesPitch(false);

  TestPlaybackRate(1.00);
  EXPECT_EQ(algorithm_.last_mode_for_testing(),
            AudioRendererAlgorithm::FillBufferMode::kResampler);

  TestPlaybackRate(1.05);
  EXPECT_EQ(algorithm_.last_mode_for_testing(),
            AudioRendererAlgorithm::FillBufferMode::kResampler);

  TestPlaybackRate(1.00);
  EXPECT_EQ(algorithm_.last_mode_for_testing(),
            AudioRendererAlgorithm::FillBufferMode::kResampler);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_WithOffset) {
  Initialize();
  const int kBufferSize = algorithm_.samples_per_second() / 10;
  const int kOffset = kBufferSize / 10;
  const int kFramesRequested =
      kOutputDurationInSec * algorithm_.samples_per_second();

  // No time-strech.
  TestPlaybackRate(1.00, kBufferSize, kFramesRequested, kOffset);

  // Resampling based time-strech.
  TestPlaybackRate(1.05, kBufferSize, kFramesRequested, kOffset);

  // WSOLA based time-strech.
  TestPlaybackRate(1.25, kBufferSize, kFramesRequested, kOffset);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_UnderFlow) {
  Initialize();
  TestResamplingWithUnderflow(0.75, true);
  TestResamplingWithUnderflow(0.75, false);
  TestResamplingWithUnderflow(1.25, true);
  TestResamplingWithUnderflow(1.25, false);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_OneAndAQuarterRate) {
  Initialize();
  TestPlaybackRate(1.25);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_OneAndAHalfRate) {
  Initialize();
  TestPlaybackRate(1.5);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_DoubleRate) {
  Initialize();
  TestPlaybackRate(2.0);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_EightTimesRate) {
  Initialize();
  TestPlaybackRate(8.0);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_ThreeQuartersRate) {
  Initialize();
  TestPlaybackRate(0.75);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_HalfRate) {
  Initialize();
  TestPlaybackRate(0.5);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_QuarterRate) {
  Initialize();
  TestPlaybackRate(0.25);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_Pause) {
  Initialize();
  TestPlaybackRate(0.0);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_SlowDown) {
  Initialize();
  TestPlaybackRate(4.5);
  TestPlaybackRate(3.0);
  TestPlaybackRate(2.0);
  TestPlaybackRate(1.0);
  TestPlaybackRate(0.5);
  TestPlaybackRate(0.25);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_SpeedUp) {
  Initialize();
  TestPlaybackRate(0.25);
  TestPlaybackRate(0.5);
  TestPlaybackRate(1.0);
  TestPlaybackRate(2.0);
  TestPlaybackRate(3.0);
  TestPlaybackRate(4.5);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_JumpAroundSpeeds) {
  Initialize();
  TestPlaybackRate(2.1);
  TestPlaybackRate(0.9);
  TestPlaybackRate(0.6);
  TestPlaybackRate(1.4);
  TestPlaybackRate(0.3);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_SmallBufferSize) {
  Initialize();
  constexpr int kBufferSizeInFrames = 1;
  constexpr int kFramesRequested = kOutputDurationInSec * kSamplesPerSecond;
  TestPlaybackRate(1.0, kBufferSizeInFrames, kFramesRequested, 0);
  TestPlaybackRate(0.5, kBufferSizeInFrames, kFramesRequested, 0);
  TestPlaybackRate(1.5, kBufferSizeInFrames, kFramesRequested, 0);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_LargeBufferSize) {
  Initialize(CHANNEL_LAYOUT_STEREO, kSampleFormatS16, 44100, 441);
  TestPlaybackRate(1.0);
  TestPlaybackRate(0.5);
  TestPlaybackRate(1.5);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_LowerQualityAudio) {
  Initialize(CHANNEL_LAYOUT_MONO, kSampleFormatU8, kSamplesPerSecond,
             kSamplesPerSecond / 100);
  TestPlaybackRate(1.0);
  TestPlaybackRate(0.5);
  TestPlaybackRate(1.5);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_HigherQualityAudio) {
  Initialize(CHANNEL_LAYOUT_STEREO, kSampleFormatS32, kSamplesPerSecond,
             kSamplesPerSecond / 100);
  TestPlaybackRate(1.0);
  TestPlaybackRate(0.5);
  TestPlaybackRate(1.5);
}

TEST_F(AudioRendererAlgorithmTest, DotProduct) {
  constexpr int kChannels = 3;
  constexpr int kFrames = 20;
  constexpr size_t kHalfPulseWidth = 2;

  std::unique_ptr<AudioBus> a = AudioBus::Create(kChannels, kFrames);
  std::unique_ptr<AudioBus> b = AudioBus::Create(kChannels, kFrames);

  auto dot_prod = base::HeapArray<float>::Uninit(kChannels);

  FillWithSquarePulseTrain(kHalfPulseWidth, 0, 0, a.get());
  FillWithSquarePulseTrain(kHalfPulseWidth, 1, 1, a.get());
  FillWithSquarePulseTrain(kHalfPulseWidth, 2, 2, a.get());

  FillWithSquarePulseTrain(kHalfPulseWidth, 0, 0, b.get());
  FillWithSquarePulseTrain(kHalfPulseWidth, 0, 1, b.get());
  FillWithSquarePulseTrain(kHalfPulseWidth, 0, 2, b.get());

  internal::MultiChannelDotProduct(a.get(), 0, b.get(), 0, kFrames, dot_prod);

  EXPECT_FLOAT_EQ(kFrames, dot_prod[0]);
  EXPECT_FLOAT_EQ(0, dot_prod[1]);
  EXPECT_FLOAT_EQ(-kFrames, dot_prod[2]);

  internal::MultiChannelDotProduct(a.get(), 4, b.get(), 8, kFrames / 2,
                                   dot_prod);

  EXPECT_FLOAT_EQ(kFrames / 2, dot_prod[0]);
  EXPECT_FLOAT_EQ(0, dot_prod[1]);
  EXPECT_FLOAT_EQ(-kFrames / 2, dot_prod[2]);
}

TEST_F(AudioRendererAlgorithmTest, MovingBlockEnergy) {
  const size_t kChannels = 2;
  const size_t kFrames = 20;
  const size_t kFramesPerBlock = 3;
  const size_t kNumBlocks = kFrames - (kFramesPerBlock - 1);
  std::unique_ptr<AudioBus> a = AudioBus::Create(kChannels, kFrames);
  auto energies = base::HeapArray<float>::Uninit(kChannels * kNumBlocks);
  auto ch_left = a->channel_span(0);
  auto ch_right = a->channel_span(1);

  // Fill up both channels.
  for (size_t n = 0; n < kFrames; ++n) {
    ch_left[n] = n;
    ch_right[n] = kFrames - 1 - n;
  }

  internal::MultiChannelMovingBlockEnergies(a.get(), kFramesPerBlock, energies);

  // Check if the energy of candidate blocks of each channel computed correctly.
  for (size_t n = 0; n < kNumBlocks; ++n) {
    float expected_energy = 0;
    for (size_t k = 0; k < kFramesPerBlock; ++k) {
      expected_energy += ch_left[n + k] * ch_left[n + k];
    }

    // Left (first) channel.
    EXPECT_FLOAT_EQ(expected_energy, energies[2 * n]);

    expected_energy = 0;
    for (size_t k = 0; k < kFramesPerBlock; ++k) {
      expected_energy += ch_right[n + k] * ch_right[n + k];
    }

    // Second (right) channel.
    EXPECT_FLOAT_EQ(expected_energy, energies[2 * n + 1]);
  }
}

TEST_F(AudioRendererAlgorithmTest, FullAndDecimatedSearch) {
  const size_t kFramesInSearchRegion = 12;
  const size_t kChannels = 2;
  const std::array<float, kFramesInSearchRegion> ch_0 = {
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
  const std::array<float, kFramesInSearchRegion> ch_1 = {
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.1f, 1.0f, 0.1f, 0.0f, 0.0f};
  std::unique_ptr<AudioBus> search_region =
      AudioBus::Create(kChannels, kFramesInSearchRegion);
  search_region->channel_span(0).copy_from_nonoverlapping(ch_0);
  search_region->channel_span(1).copy_from_nonoverlapping(ch_1);

  const size_t kFramePerBlock = 4;
  const std::array<float, kFramePerBlock> target_0 = {1.0f, 1.0f, 1.0f, 0.0f};
  const std::array<float, kFramePerBlock> target_1 = {0.0f, 1.0f, 0.1f, 1.0f};

  std::unique_ptr<AudioBus> target =
      AudioBus::Create(kChannels, kFramePerBlock);
  target->channel_span(0).copy_from_nonoverlapping(target_0);
  target->channel_span(1).copy_from_nonoverlapping(target_1);

  auto energy_target = base::HeapArray<float>::Uninit(kChannels);

  internal::MultiChannelDotProduct(target.get(), 0, target.get(), 0,
                                   kFramePerBlock, energy_target);

  ASSERT_EQ(3.f, energy_target[0]);
  ASSERT_EQ(2.01f, energy_target[1]);

  const size_t kNumCandidBlocks = kFramesInSearchRegion - (kFramePerBlock - 1);
  auto energy_candid_blocks =
      base::HeapArray<float>::Uninit(kNumCandidBlocks * kChannels);

  internal::MultiChannelMovingBlockEnergies(search_region.get(), kFramePerBlock,
                                            energy_candid_blocks);

  // Check the energy of the candidate blocks of the first channel.
  ASSERT_FLOAT_EQ(0, energy_candid_blocks[0]);
  ASSERT_FLOAT_EQ(0, energy_candid_blocks[2]);
  ASSERT_FLOAT_EQ(1, energy_candid_blocks[4]);
  ASSERT_FLOAT_EQ(2, energy_candid_blocks[6]);
  ASSERT_FLOAT_EQ(3, energy_candid_blocks[8]);
  ASSERT_FLOAT_EQ(3, energy_candid_blocks[10]);
  ASSERT_FLOAT_EQ(2, energy_candid_blocks[12]);
  ASSERT_FLOAT_EQ(1, energy_candid_blocks[14]);
  ASSERT_FLOAT_EQ(0, energy_candid_blocks[16]);

  // Check the energy of the candidate blocks of the second channel.
  ASSERT_FLOAT_EQ(0, energy_candid_blocks[1]);
  ASSERT_FLOAT_EQ(0, energy_candid_blocks[3]);
  ASSERT_FLOAT_EQ(0, energy_candid_blocks[5]);
  ASSERT_FLOAT_EQ(0, energy_candid_blocks[7]);
  ASSERT_FLOAT_EQ(0.01f, energy_candid_blocks[9]);
  ASSERT_FLOAT_EQ(1.01f, energy_candid_blocks[11]);
  ASSERT_FLOAT_EQ(1.02f, energy_candid_blocks[13]);
  ASSERT_FLOAT_EQ(1.02f, energy_candid_blocks[15]);
  ASSERT_FLOAT_EQ(1.01f, energy_candid_blocks[17]);

  // An interval which is of no effect.
  internal::Interval exclude_interval =
      std::make_pair(kNumCandidBlocks * 5, kNumCandidBlocks * 7);
  EXPECT_EQ(5u, internal::FullSearch(0, kNumCandidBlocks - 1, exclude_interval,
                                     target.get(), search_region.get(),
                                     energy_target, energy_candid_blocks));

  // Exclude the the best match.
  exclude_interval = std::make_pair(2, 5);
  EXPECT_EQ(7u, internal::FullSearch(0, kNumCandidBlocks - 1, exclude_interval,
                                     target.get(), search_region.get(),
                                     energy_target, energy_candid_blocks));

  // An interval which is of no effect.
  exclude_interval = std::make_pair(kNumCandidBlocks * 5, kNumCandidBlocks * 7);
  EXPECT_EQ(4u, internal::DecimatedSearch(4, exclude_interval, target.get(),
                                          search_region.get(), energy_target,
                                          energy_candid_blocks));

  EXPECT_EQ(5u, internal::OptimalIndex(search_region.get(), target.get(),
                                       exclude_interval));
}

TEST_F(AudioRendererAlgorithmTest, QuadraticInterpolation) {
  // Arbitrary coefficients.
  constexpr float kA = 0.7f;
  constexpr float kB = 1.2f;
  constexpr float kC = 0.8f;

  constexpr std::array<float, 3> y_values = {kA - kB + kC, kC, kA + kB + kC};

  float extremum;
  float extremum_value;

  internal::QuadraticInterpolation(y_values, &extremum, &extremum_value);

  constexpr float x_star = -kB / (2.f * kA);
  constexpr float y_star = kA * x_star * x_star + kB * x_star + kC;

  EXPECT_FLOAT_EQ(x_star, extremum);
  EXPECT_FLOAT_EQ(y_star, extremum_value);
}

TEST_F(AudioRendererAlgorithmTest, QuadraticInterpolation_Colinear) {
  constexpr std::array<float, 3> y_values = {1.0f, 1.0f, 1.0f};

  float extremum;
  float extremum_value;

  internal::QuadraticInterpolation(y_values, &extremum, &extremum_value);

  EXPECT_FLOAT_EQ(extremum, 0.0);
  EXPECT_FLOAT_EQ(extremum_value, 1.0);
}

TEST_F(AudioRendererAlgorithmTest, WsolaSlowdown) {
  WsolaTest(0.6);
}

TEST_F(AudioRendererAlgorithmTest, WsolaSpeedup) {
  WsolaTest(1.6);
}

TEST_F(AudioRendererAlgorithmTest, FillBufferOffset) {
  Initialize();
  // Pad the queue capacity so fill requests for all rates below can be fully
  // satisfied.
  algorithm_.IncreasePlaybackThreshold();

  std::unique_ptr<AudioBus> bus = AudioBus::Create(channels_, kFrameSize);

  // Verify that the first half of |bus| remains zero and the last half is
  // filled appropriately at normal, above normal, and below normal.
  const int kHalfSize = kFrameSize / 2;
  const auto kAudibleRates =
      std::to_array<float>({1.0f, 2.0f, 0.5f, 5.0f, 0.25f});
  for (float rate : kAudibleRates) {
    SCOPED_TRACE(rate);
    bus->Zero();

    const int frames_filled =
        algorithm_.FillBuffer(bus.get(), kHalfSize, kHalfSize, rate);
    ASSERT_EQ(kHalfSize, frames_filled);
    ASSERT_TRUE(VerifyAudioData(bus.get(), 0, kHalfSize, 0));
    ASSERT_FALSE(VerifyAudioData(bus.get(), kHalfSize, kHalfSize, 0));
    FillAlgorithmQueueUntilFull();
  }
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_ChannelMask) {
  // Setup a quad channel layout where even channels are always muted.
  Initialize(CHANNEL_LAYOUT_QUAD, kSampleFormatS16, 44100, 441,
             {true, false, true, false});

  std::unique_ptr<AudioBus> bus = AudioBus::Create(channels_, kFrameSize);
  int frames_filled = algorithm_.FillBuffer(bus.get(), 0, kFrameSize, 2.0);
  ASSERT_GT(frames_filled, 0);

  constexpr auto is_zero = [](float sample) { return sample == 0.0f; };

  // Verify the channels are muted appropriately; even though the created buffer
  // actually has audio data in it.
  for (int ch = 0; ch < bus->channels(); ++ch) {
    if (ch % 2 == 1) {
      ASSERT_TRUE(std::ranges::all_of(bus->channel_span(ch), is_zero));
    } else {
      ASSERT_FALSE(std::ranges::all_of(bus->channel_span(ch), is_zero));
    }
  }

  // Update the channel mask and verify it's reflected correctly.
  algorithm_.SetChannelMask({true, true, true, true});
  frames_filled = algorithm_.FillBuffer(bus.get(), 0, kFrameSize, 2.0);
  ASSERT_GT(frames_filled, 0);

  // Verify no channels are muted now.
  for (auto channel : bus->AllChannels()) {
    ASSERT_FALSE(std::ranges::all_of(channel, is_zero));
  }
}

// The |plabyack_threshold_| should == |capacity_| by default, when no
// |latency_hint_| is set.
TEST_F(AudioRendererAlgorithmTest, NoLatencyHint) {
  // Queue is initially empty. Capacity is unset.
  EXPECT_EQ(algorithm_.BufferedFrames(), 0);
  EXPECT_EQ(algorithm_.QueueCapacity(), 0);

  // Initialize sets capacity fills queue.
  Initialize();
  EXPECT_GT(algorithm_.QueueCapacity(), 0);
  EXPECT_TRUE(algorithm_.IsQueueFull());
  EXPECT_TRUE(algorithm_.IsQueueAdequateForPlayback());

  // No latency hint is set, so playback threshold should == capacity. Observe
  // that the queue is neither "full" nor "adequate for playback" if we are one
  // one frame below the capacity limit.
  std::unique_ptr<AudioBus> bus = AudioBus::Create(channels_, kFrameSize);
  int requested_frames =
      (algorithm_.BufferedFrames() - algorithm_.QueueCapacity()) + 1;
  const int frames_filled =
      algorithm_.FillBuffer(bus.get(), 0, requested_frames, 1);
  EXPECT_EQ(frames_filled, requested_frames);
  EXPECT_EQ(algorithm_.BufferedFrames(), algorithm_.QueueCapacity() - 1);
  EXPECT_FALSE(algorithm_.IsQueueFull());
  EXPECT_FALSE(algorithm_.IsQueueAdequateForPlayback());

  // Queue should again be "adequate for playback" and "full" it we add a single
  // frame such that BufferedFrames() == QueueCapacity().
  DCHECK_EQ(sample_format_, kSampleFormatS16);
  algorithm_.EnqueueBuffer(MakeBuffer(1));
  EXPECT_TRUE(algorithm_.IsQueueFull());
  EXPECT_TRUE(algorithm_.IsQueueAdequateForPlayback());
  EXPECT_EQ(algorithm_.BufferedFrames(), algorithm_.QueueCapacity());

  // Increasing playback threshold should also increase capacity.
  int orig_capacity = algorithm_.QueueCapacity();
  algorithm_.IncreasePlaybackThreshold();
  EXPECT_GT(algorithm_.QueueCapacity(), orig_capacity);
  EXPECT_FALSE(algorithm_.IsQueueFull());
  EXPECT_FALSE(algorithm_.IsQueueAdequateForPlayback());

  // Filling again, 1 frame at a time, we should reach "adequate" and "full" in
  // the same step.
  while (!algorithm_.IsQueueFull()) {
    algorithm_.EnqueueBuffer(MakeBuffer(1));
    EXPECT_EQ(algorithm_.IsQueueFull(),
              algorithm_.IsQueueAdequateForPlayback());
  }

  // Flushing should restore queue capacity and playback threshold to the
  // original value.
  algorithm_.FlushBuffers();
  EXPECT_EQ(algorithm_.QueueCapacity(), orig_capacity);
  EXPECT_FALSE(algorithm_.IsQueueFull());
  EXPECT_FALSE(algorithm_.IsQueueAdequateForPlayback());

  // Filling again, 1 frame at a time, we should reach "adequate" and "full" in
  // the same step.
  while (!algorithm_.IsQueueFull()) {
    algorithm_.EnqueueBuffer(MakeBuffer(1));
    EXPECT_EQ(algorithm_.IsQueueFull(),
              algorithm_.IsQueueAdequateForPlayback());
  }
}

// The |playback_threshold_| should be < |capacity_| when a latency hint is
// set to reduce the playback delay.
TEST_F(AudioRendererAlgorithmTest, LowLatencyHint) {
  // Initialize with a buffer size that leaves some gap between the min capacity
  // (2*buffer_size) and the default capacity (200ms).
  constexpr int kBufferSize = kSamplesPerSecond / 50;
  Initialize(CHANNEL_LAYOUT_STEREO, kSampleFormatS16, kSamplesPerSecond,
             kBufferSize);

  // FlushBuffers to start out empty.
  algorithm_.FlushBuffers();

  EXPECT_GT(algorithm_.QueueCapacity(), 0);
  EXPECT_FALSE(algorithm_.IsQueueFull());
  EXPECT_FALSE(algorithm_.IsQueueAdequateForPlayback());

  // Set a latency hint at half the default capacity.
  const int orig_queue_capcity = algorithm_.QueueCapacity();
  base::TimeDelta low_latency_hint = AudioTimestampHelper::FramesToTime(
      orig_queue_capcity / 2, samples_per_second_);
  algorithm_.SetLatencyHint(low_latency_hint);

  // Hint is less than capacity, so capacity should be unchanged.
  EXPECT_EQ(algorithm_.QueueCapacity(), orig_queue_capcity);

  // Fill until "adequate". Verify "adequate" buffer time reflects the hinted
  // latency, and that "adequate" is less than "full".
  FillAlgorithmQueueUntilAdequate();
  EXPECT_EQ(BufferedTime(), low_latency_hint);
  EXPECT_FALSE(algorithm_.IsQueueFull());

  // Set a new *slightly higher* hint. Verify we're no longer "adequate".
  low_latency_hint += base::Milliseconds(10);
  algorithm_.SetLatencyHint(low_latency_hint);
  EXPECT_FALSE(algorithm_.IsQueueAdequateForPlayback());

  // Fill until "adequate". Verify "adequate" buffer time reflects the
  // *slightly higher* hinted latency, and that "adequate" is less than "full".
  FillAlgorithmQueueUntilAdequate();
  EXPECT_EQ(BufferedTime(), low_latency_hint);
  EXPECT_FALSE(algorithm_.IsQueueFull());

  // Clearing the hint should restore the higher default playback threshold,
  // such that we no longer have enough buffer to be "adequate for playback".
  algorithm_.SetLatencyHint(std::nullopt);
  EXPECT_FALSE(algorithm_.IsQueueAdequateForPlayback());

  // Fill until "full". Verify that "adequate" now matches "full".
  while (!algorithm_.IsQueueFull()) {
    algorithm_.EnqueueBuffer(MakeBuffer(1));
    EXPECT_EQ(algorithm_.IsQueueAdequateForPlayback(),
              algorithm_.IsQueueFull());
  }
}

// Note: the behavior of FlushBuffers() that is not specific to high vs low
// latency hints. Testing it with "high" is slightly more interesting. Testing
// with both "high" and "low" is excessive.
TEST_F(AudioRendererAlgorithmTest, HighLatencyHint) {
  // Initialize with a buffer size that leaves some gap between the min capacity
  // (2*buffer_size) and the default capacity (200ms).
  constexpr int kBufferSize = kSamplesPerSecond / 50;
  Initialize(CHANNEL_LAYOUT_STEREO, kSampleFormatS16, kSamplesPerSecond,
             kBufferSize);
  const int default_capacity = algorithm_.QueueCapacity();

  // FlushBuffers to start out empty.
  algorithm_.FlushBuffers();

  // Set a "high" latency hint.
  const base::TimeDelta high_latency_hint = AudioTimestampHelper::FramesToTime(
      algorithm_.QueueCapacity() * 2, samples_per_second_);
  algorithm_.SetLatencyHint(high_latency_hint);
  const int high_latency_capacity = algorithm_.QueueCapacity();
  EXPECT_GT(high_latency_capacity, default_capacity);

  // Fill until "adequate". Verify it reflects the high latency hint.
  EXPECT_TRUE(BufferedTime().is_zero());
  FillAlgorithmQueueUntilAdequate();
  EXPECT_EQ(BufferedTime(), high_latency_hint);

  // Flush the queue!
  algorithm_.FlushBuffers();

  // Verify |capcity_| was not changed by flush. The latency hint supersedes any
  // automatic queue size adjustments.
  EXPECT_EQ(algorithm_.QueueCapacity(), high_latency_capacity);

  // Similarly, verify that |playback_threshold_| was not changed by refilling
  // and observing that the "adequate" buffered time still matches the hint.
  FillAlgorithmQueueUntilAdequate();
  EXPECT_EQ(BufferedTime(), high_latency_hint);

  // Clearing the hint should restore the lower default playback threshold and
  // capacity.
  algorithm_.SetLatencyHint(std::nullopt);
  EXPECT_EQ(algorithm_.QueueCapacity(), default_capacity);

  // The queue is over-full from our last fill when the hint was set. Flush and
  // refill to the reduced "adequate" threshold.
  algorithm_.FlushBuffers();
  FillAlgorithmQueueUntilAdequate();
  EXPECT_LT(BufferedTime(), high_latency_hint);

  // With latency hint now unset, callers are now free to adjust the queue size
  // (e.g. in response to underflow). Lets increase the threshold!
  algorithm_.IncreasePlaybackThreshold();

  // Verify higher capacity means we're no longer "adequate" nor "full".
  EXPECT_GT(algorithm_.QueueCapacity(), default_capacity);
  EXPECT_FALSE(algorithm_.IsQueueAdequateForPlayback());
  EXPECT_FALSE(algorithm_.IsQueueFull());

  // Flush the queue and verify the increase has been reverted.
  algorithm_.FlushBuffers();
  EXPECT_EQ(algorithm_.QueueCapacity(), default_capacity);

  // Refill to verify "adequate" matches the "full" at the default capacity.
  while (!algorithm_.IsQueueAdequateForPlayback()) {
    algorithm_.EnqueueBuffer(MakeBuffer(1));
    EXPECT_EQ(algorithm_.IsQueueAdequateForPlayback(),
              algorithm_.IsQueueFull());
  }
}

// Algorithm should clam specified hint to a reasonable min/max.
TEST_F(AudioRendererAlgorithmTest, ClampLatencyHint) {
  // Initialize with a buffer size that leaves some gap between the min capacity
  // (2*buffer_size) and the default capacity (200ms).
  constexpr int kBufferSize = kSamplesPerSecond / 50;
  Initialize(CHANNEL_LAYOUT_STEREO, kSampleFormatS16, kSamplesPerSecond,
             kBufferSize);
  const int default_capacity = algorithm_.QueueCapacity();

  // FlushBuffers to start out empty.
  algorithm_.FlushBuffers();

  // Set a crazy high latency hint.
  algorithm_.SetLatencyHint(base::Seconds(100));

  constexpr base::TimeDelta kDefaultMax = base::Seconds(3);
  // Verify "full" and "adequate" thresholds increased, but to a known max well
  // below the hinted value.
  EXPECT_GT(algorithm_.QueueCapacity(), default_capacity);
  FillAlgorithmQueueUntilAdequate();
  EXPECT_EQ(BufferedTime(), kDefaultMax);

  // FlushBuffers to return to empty.
  algorithm_.FlushBuffers();

  // Set an impossibly low latency hint.
  algorithm_.SetLatencyHint(base::Seconds(0));

  // Verify "full" and "adequate" thresholds decreased, but to a known minimum
  // well above the hinted value.
  EXPECT_EQ(algorithm_.QueueCapacity(), default_capacity);
  FillAlgorithmQueueUntilAdequate();
  EXPECT_EQ(algorithm_.BufferedFrames(), 2 * kBufferSize);
}

}  // namespace media
