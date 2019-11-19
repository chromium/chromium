// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include <cmath>
#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/stl_util.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_bus.h"
#include "media/base/channel_layout.h"
#include "media/base/test_helpers.h"
#include "media/base/timestamp_constants.h"
#include "media/filters/wsola_internals.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

const int kFrameSize = 250;
const int kSamplesPerSecond = 3000;
const int kOutputDurationInSec = 10;

static void FillWithSquarePulseTrain(
    int half_pulse_width, int offset, int num_samples, float* data) {
  ASSERT_GE(offset, 0);
  ASSERT_LE(offset, num_samples);

  // Fill backward from |offset| - 1 toward zero, starting with -1, alternating
  // between -1 and 1 every |pulse_width| samples.
  float pulse = -1.0f;
  for (int n = offset - 1, k = 0; n >= 0; --n, ++k) {
    if (k >= half_pulse_width) {
      pulse = -pulse;
      k = 0;
    }
    data[n] = pulse;
  }

  // Fill forward from |offset| towards the end, starting with 1, alternating
  // between 1 and -1 every |pulse_width| samples.
  pulse = 1.0f;
  for (int n = offset, k = 0; n < num_samples; ++n, ++k) {
    if (k >= half_pulse_width) {
      pulse = -pulse;
      k = 0;
    }
    data[n] = pulse;
  }
}

static void FillWithSquarePulseTrain(
    int half_pulse_width, int offset, int channel, AudioBus* audio_bus) {
  FillWithSquarePulseTrain(half_pulse_width, offset, audio_bus->frames(),
                           audio_bus->channel(channel));
}

class AudioRendererAlgorithmTest : public testing::Test {
 public:
  AudioRendererAlgorithmTest()
      : frames_enqueued_(0),
        channels_(0),
        channel_layout_(CHANNEL_LAYOUT_NONE),
        sample_format_(kUnknownSampleFormat),
        samples_per_second_(0),
        bytes_per_sample_(0) {
  }

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

    AudioParameters params(format, channel_layout, samples_per_second,
                           frames_per_buffer);
    bool is_encrypted = false;
    algorithm_.Initialize(params, is_encrypted);
    algorithm_.SetChannelMask(std::move(channel_mask));
    FillAlgorithmQueue();
  }

  void FillAlgorithmQueue() {
    // The value of the data is meaningless; we just want non-zero data to
    // differentiate it from muted data.
    scoped_refptr<AudioBuffer> buffer;
    while (!algorithm_.IsQueueFull()) {
      switch (sample_format_) {
        case kSampleFormatAc3:
        case kSampleFormatEac3:
          buffer = MakeBitstreamAudioBuffer(
              sample_format_, channel_layout_,
              ChannelLayoutToChannelCount(channel_layout_), samples_per_second_,
              1, 1, kFrameSize, kFrameSize, kNoTimestamp);
          break;
        case kSampleFormatU8:
          buffer = MakeAudioBuffer<uint8_t>(
              sample_format_, channel_layout_,
              ChannelLayoutToChannelCount(channel_layout_), samples_per_second_,
              1, 1, kFrameSize, kNoTimestamp);
          break;
        case kSampleFormatS16:
          buffer = MakeAudioBuffer<int16_t>(
              sample_format_, channel_layout_,
              ChannelLayoutToChannelCount(channel_layout_), samples_per_second_,
              1, 1, kFrameSize, kNoTimestamp);
          break;
        case kSampleFormatS32:
          buffer = MakeAudioBuffer<int32_t>(
              sample_format_, channel_layout_,
              ChannelLayoutToChannelCount(channel_layout_), samples_per_second_,
              1, 1, kFrameSize, kNoTimestamp);
          break;
        default:
          NOTREACHED() << "Unrecognized format " << sample_format_;
      }
      algorithm_.EnqueueBuffer(buffer);
      frames_enqueued_ += kFrameSize;
    }
  }

  bool VerifyAudioData(AudioBus* bus, int offset, int frames, float value) {
    for (int ch = 0; ch < bus->channels(); ++ch) {
      for (int i = offset; i < offset + frames; ++i) {
        if (bus->channel(ch)[i] != value)
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
    int buffered_delta = algorithm_.frames_buffered() - initial_frames_buffered;
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
    int initial_frames_buffered = algorithm_.frames_buffered();

    std::unique_ptr<AudioBus> bus =
        AudioBus::Create(channels_, buffer_size_in_frames);
    bus->ZeroFrames(dest_offset);

    if (playback_rate == 0.0) {
      int frames_written = algorithm_.FillBuffer(
          bus.get(), 0, buffer_size_in_frames, playback_rate);
      EXPECT_EQ(0, frames_written);
      return;
    }

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
        ASSERT_FALSE(AudioDataIsMuted(bus.get(), frames_written, dest_offset));
      first_fill_buffer = false;
      frames_remaining -= frames_written;

      FillAlgorithmQueue();
    }

    EXPECT_EQ(algorithm_.frames_buffered() * channels_ * sizeof(float),
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

  void TestPlaybackRateWithUnderflow(double playback_rate, bool end_of_stream) {
    if (playback_rate > AudioRendererAlgorithm::kUpperResampleThreshold ||
        playback_rate < AudioRendererAlgorithm::kLowerResampleThreshold) {
      // This test is only used for the range in which we resample data instead
      // of using WSOLA.
      return;
    }

    if (end_of_stream) {
      algorithm_.MarkEndOfStream();
    } else {
      algorithm_.FlushBuffers();
    }

    const int buffer_size_in_frames = algorithm_.samples_per_second() / 10;
    const int initial_frames_enqueued = frames_enqueued_;

    std::unique_ptr<AudioBus> bus =
        AudioBus::Create(channels_, buffer_size_in_frames);

    FillAlgorithmQueue();

    int frames_written;
    int total_frames_written = 0;
    do {
      frames_written = algorithm_.FillBuffer(
          bus.get(), 0, buffer_size_in_frames, playback_rate);

      total_frames_written += frames_written;
    } while (frames_written && algorithm_.frames_buffered() > 0);

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
    const int kSampleRateHz = 48000;
    const ChannelLayout kChannelLayout = CHANNEL_LAYOUT_STEREO;
    const int kNumFrames = kSampleRateHz / 100;  // 10 milliseconds.

    channels_ = ChannelLayoutToChannelCount(kChannelLayout);
    AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR, kChannelLayout,
                           kSampleRateHz, kNumFrames);
    bool is_encrypted = false;
    algorithm_.Initialize(params, is_encrypted);

    // A pulse is 6 milliseconds (even number of samples).
    const int kPulseWidthSamples = 6 * kSampleRateHz / 1000;
    const int kHalfPulseWidthSamples = kPulseWidthSamples / 2;

    // For the ease of implementation get 1 frame every call to FillBuffer().
    std::unique_ptr<AudioBus> output = AudioBus::Create(channels_, 1);

    // Input buffer to inject pulses.
    scoped_refptr<AudioBuffer> input =
        AudioBuffer::CreateBuffer(kSampleFormatPlanarF32,
                                  kChannelLayout,
                                  channels_,
                                  kSampleRateHz,
                                  kPulseWidthSamples);

    const std::vector<uint8_t*>& channel_data = input->channel_data();

    // Fill |input| channels.
    FillWithSquarePulseTrain(kHalfPulseWidthSamples, 0, kPulseWidthSamples,
                             reinterpret_cast<float*>(channel_data[0]));
    FillWithSquarePulseTrain(kHalfPulseWidthSamples, kHalfPulseWidthSamples,
                             kPulseWidthSamples,
                             reinterpret_cast<float*>(channel_data[1]));

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
          const float* pulse_ch = pulse_buffer->channel(m);

          // Because of overlap-and-add we might have round off error.
          for (int k = 0; k < kPulseWidthSamples; ++k) {
            ASSERT_NEAR(reinterpret_cast<float*>(channel_data[m])[k],
                        pulse_ch[k], kTolerance) << " loop " << n
                                << " channel/sample " << m << "/" << k;
          }
        }
      }

      // Zero out the buffer to be sure the next comparison is relevant.
      pulse_buffer->Zero();
    }
  }

 protected:
  AudioRendererAlgorithm algorithm_;
  int frames_enqueued_;
  int channels_;
  ChannelLayout channel_layout_;
  SampleFormat sample_format_;
  int samples_per_second_;
  int bytes_per_sample_;
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
  TestPlaybackRate(0.94);  // WSOLA.
  TestPlaybackRate(AudioRendererAlgorithm::kLowerResampleThreshold);
  TestPlaybackRate(0.97);
  TestPlaybackRate(1.00);
  TestPlaybackRate(1.04);
  TestPlaybackRate(AudioRendererAlgorithm::kUpperResampleThreshold);
  TestPlaybackRate(1.07);  // WSOLA.
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
  TestPlaybackRateWithUnderflow(AudioRendererAlgorithm::kLowerResampleThreshold,
                                true);
  TestPlaybackRateWithUnderflow(AudioRendererAlgorithm::kLowerResampleThreshold,
                                false);
  TestPlaybackRateWithUnderflow(AudioRendererAlgorithm::kUpperResampleThreshold,
                                true);
  TestPlaybackRateWithUnderflow(AudioRendererAlgorithm::kUpperResampleThreshold,
                                false);
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
  static const int kBufferSizeInFrames = 1;
  static const int kFramesRequested = kOutputDurationInSec * kSamplesPerSecond;
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
  const int kChannels = 3;
  const int kFrames = 20;
  const int kHalfPulseWidth = 2;

  std::unique_ptr<AudioBus> a = AudioBus::Create(kChannels, kFrames);
  std::unique_ptr<AudioBus> b = AudioBus::Create(kChannels, kFrames);

  std::unique_ptr<float[]> dot_prod(new float[kChannels]);

  FillWithSquarePulseTrain(kHalfPulseWidth, 0, 0, a.get());
  FillWithSquarePulseTrain(kHalfPulseWidth, 1, 1, a.get());
  FillWithSquarePulseTrain(kHalfPulseWidth, 2, 2, a.get());

  FillWithSquarePulseTrain(kHalfPulseWidth, 0, 0, b.get());
  FillWithSquarePulseTrain(kHalfPulseWidth, 0, 1, b.get());
  FillWithSquarePulseTrain(kHalfPulseWidth, 0, 2, b.get());

  internal::MultiChannelDotProduct(a.get(), 0, b.get(), 0, kFrames,
                                   dot_prod.get());

  EXPECT_FLOAT_EQ(kFrames, dot_prod[0]);
  EXPECT_FLOAT_EQ(0, dot_prod[1]);
  EXPECT_FLOAT_EQ(-kFrames, dot_prod[2]);

  internal::MultiChannelDotProduct(a.get(), 4, b.get(), 8, kFrames / 2,
                                   dot_prod.get());

  EXPECT_FLOAT_EQ(kFrames / 2, dot_prod[0]);
  EXPECT_FLOAT_EQ(0, dot_prod[1]);
  EXPECT_FLOAT_EQ(-kFrames / 2, dot_prod[2]);
}

TEST_F(AudioRendererAlgorithmTest, MovingBlockEnergy) {
  const int kChannels = 2;
  const int kFrames = 20;
  const int kFramesPerBlock = 3;
  const int kNumBlocks = kFrames - (kFramesPerBlock - 1);
  std::unique_ptr<AudioBus> a = AudioBus::Create(kChannels, kFrames);
  std::unique_ptr<float[]> energies(new float[kChannels * kNumBlocks]);
  float* ch_left = a->channel(0);
  float* ch_right = a->channel(1);

  // Fill up both channels.
  for (int n = 0; n < kFrames; ++n) {
    ch_left[n] = n;
    ch_right[n] = kFrames - 1 - n;
  }

  internal::MultiChannelMovingBlockEnergies(a.get(), kFramesPerBlock,
                                            energies.get());

  // Check if the energy of candidate blocks of each channel computed correctly.
  for (int n = 0; n < kNumBlocks; ++n) {
    float expected_energy = 0;
    for (int k = 0; k < kFramesPerBlock; ++k)
      expected_energy += ch_left[n + k] * ch_left[n + k];

    // Left (first) channel.
    EXPECT_FLOAT_EQ(expected_energy, energies[2 * n]);

    expected_energy = 0;
    for (int k = 0; k < kFramesPerBlock; ++k)
      expected_energy += ch_right[n + k] * ch_right[n + k];

    // Second (right) channel.
    EXPECT_FLOAT_EQ(expected_energy, energies[2 * n + 1]);
  }
}

TEST_F(AudioRendererAlgorithmTest, FullAndDecimatedSearch) {
  const int kFramesInSearchRegion = 12;
  const int kChannels = 2;
  float ch_0[] = {
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f };
  float ch_1[] = {
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.1f, 1.0f, 0.1f, 0.0f, 0.0f };
  ASSERT_EQ(sizeof(ch_0), sizeof(ch_1));
  ASSERT_EQ(static_cast<size_t>(kFramesInSearchRegion),
            sizeof(ch_0) / sizeof(*ch_0));
  std::unique_ptr<AudioBus> search_region =
      AudioBus::Create(kChannels, kFramesInSearchRegion);
  float* ch = search_region->channel(0);
  memcpy(ch, ch_0, sizeof(float) * kFramesInSearchRegion);
  ch = search_region->channel(1);
  memcpy(ch, ch_1, sizeof(float) * kFramesInSearchRegion);

  const int kFramePerBlock = 4;
  float target_0[] = { 1.0f, 1.0f, 1.0f, 0.0f };
  float target_1[] = { 0.0f, 1.0f, 0.1f, 1.0f };
  ASSERT_EQ(sizeof(target_0), sizeof(target_1));
  ASSERT_EQ(static_cast<size_t>(kFramePerBlock),
            sizeof(target_0) / sizeof(*target_0));

  std::unique_ptr<AudioBus> target =
      AudioBus::Create(kChannels, kFramePerBlock);
  ch = target->channel(0);
  memcpy(ch, target_0, sizeof(float) * kFramePerBlock);
  ch = target->channel(1);
  memcpy(ch, target_1, sizeof(float) * kFramePerBlock);

  std::unique_ptr<float[]> energy_target(new float[kChannels]);

  internal::MultiChannelDotProduct(target.get(), 0, target.get(), 0,
                                   kFramePerBlock, energy_target.get());

  ASSERT_EQ(3.f, energy_target[0]);
  ASSERT_EQ(2.01f, energy_target[1]);

  const int kNumCandidBlocks = kFramesInSearchRegion - (kFramePerBlock - 1);
  std::unique_ptr<float[]> energy_candid_blocks(
      new float[kNumCandidBlocks * kChannels]);

  internal::MultiChannelMovingBlockEnergies(
      search_region.get(), kFramePerBlock, energy_candid_blocks.get());

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
  internal::Interval exclude_interval = std::make_pair(-100, -10);
  EXPECT_EQ(5, internal::FullSearch(
      0, kNumCandidBlocks - 1, exclude_interval, target.get(),
      search_region.get(), energy_target.get(), energy_candid_blocks.get()));

  // Exclude the the best match.
  exclude_interval = std::make_pair(2, 5);
  EXPECT_EQ(7, internal::FullSearch(
      0, kNumCandidBlocks - 1, exclude_interval, target.get(),
      search_region.get(), energy_target.get(), energy_candid_blocks.get()));

  // An interval which is of no effect.
  exclude_interval = std::make_pair(-100, -10);
  EXPECT_EQ(4, internal::DecimatedSearch(
      4, exclude_interval, target.get(), search_region.get(),
      energy_target.get(), energy_candid_blocks.get()));

  EXPECT_EQ(5, internal::OptimalIndex(search_region.get(), target.get(),
                                      exclude_interval));
}

TEST_F(AudioRendererAlgorithmTest, QuadraticInterpolation) {
  // Arbitrary coefficients.
  const float kA = 0.7f;
  const float kB = 1.2f;
  const float kC = 0.8f;

  float y_values[3];
  y_values[0] = kA - kB + kC;
  y_values[1] = kC;
  y_values[2] = kA + kB + kC;

  float extremum;
  float extremum_value;

  internal::QuadraticInterpolation(y_values, &extremum, &extremum_value);

  float x_star = -kB / (2.f * kA);
  float y_star = kA * x_star * x_star + kB * x_star + kC;

  EXPECT_FLOAT_EQ(x_star, extremum);
  EXPECT_FLOAT_EQ(y_star, extremum_value);
}

TEST_F(AudioRendererAlgorithmTest, QuadraticInterpolation_Colinear) {
  float y_values[3];
  y_values[0] = 1.0;
  y_values[1] = 1.0;
  y_values[2] = 1.0;

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
  algorithm_.IncreaseQueueCapacity();

  std::unique_ptr<AudioBus> bus = AudioBus::Create(channels_, kFrameSize);

  // Verify that the first half of |bus| remains zero and the last half is
  // filled appropriately at normal, above normal, and below normal.
  const int kHalfSize = kFrameSize / 2;
  const float kAudibleRates[] = {1.0f, 2.0f, 0.5f, 5.0f, 0.25f};
  for (size_t i = 0; i < base::size(kAudibleRates); ++i) {
    SCOPED_TRACE(kAudibleRates[i]);
    bus->Zero();

    const int frames_filled = algorithm_.FillBuffer(
        bus.get(), kHalfSize, kHalfSize, kAudibleRates[i]);
    ASSERT_EQ(kHalfSize, frames_filled);
    ASSERT_TRUE(VerifyAudioData(bus.get(), 0, kHalfSize, 0));
    ASSERT_FALSE(VerifyAudioData(bus.get(), kHalfSize, kHalfSize, 0));
    FillAlgorithmQueue();
  }
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_ChannelMask) {
  // Setup a quad channel layout where even channels are always muted.
  Initialize(CHANNEL_LAYOUT_QUAD, kSampleFormatS16, 44100, 441,
             {true, false, true, false});

  std::unique_ptr<AudioBus> bus = AudioBus::Create(channels_, kFrameSize);
  int frames_filled = algorithm_.FillBuffer(bus.get(), 0, kFrameSize, 2.0);
  ASSERT_GT(frames_filled, 0);

  // Verify the channels are muted appropriately; even though the created buffer
  // actually has audio data in it.
  for (int ch = 0; ch < bus->channels(); ++ch) {
    double sum = 0;
    for (int i = 0; i < bus->frames(); ++i)
      sum += bus->channel(ch)[i];
    if (ch % 2 == 1)
      ASSERT_EQ(sum, 0);
    else
      ASSERT_NE(sum, 0);
  }

  // Update the channel mask and verify it's reflected correctly.
  algorithm_.SetChannelMask({true, true, true, true});
  frames_filled = algorithm_.FillBuffer(bus.get(), 0, kFrameSize, 2.0);
  ASSERT_GT(frames_filled, 0);

  // Verify no channels are muted now.
  for (int ch = 0; ch < bus->channels(); ++ch) {
    double sum = 0;
    for (int i = 0; i < bus->frames(); ++i)
      sum += bus->channel(ch)[i];
    ASSERT_NE(sum, 0);
  }
}

}  // namespace media
