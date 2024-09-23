// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The format of these tests are to enqueue a known amount of data and then
// request the exact amount we expect in order to dequeue the known amount of
// data.  This ensures that for any rate we are consuming input data at the
// correct rate.  We always pass in a very large destination buffer with the
// expectation that FillBuffer() will fill as much as it can but no more.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/filters/audio_renderer_algorithm.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>  // For std::min().
#include <cmath>
#include <memory>
#include <vector>

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
      : algorithm_(&media_log_),
        frames_enqueued_(0),
        channels_(0),
        channel_layout_(CHANNEL_LAYOUT_NONE),
        sample_format_(kUnknownSampleFormat),
        samples_per_second_(0),
        bytes_per_sample_(0) {}

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
        NOTREACHED_IN_MIGRATION() << "Unrecognized format " << sample_format_;
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
        ASSERT_FALSE(AudioDataIsMuted(bus.get(), frames_written, dest_offset));
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
    const int kSampleRateHz = 48000;
    constexpr ChannelLayout kChannelLayout = CHANNEL_LAYOUT_STEREO;
    const int kNumFrames = kSampleRateHz / 100;  // 10 milliseconds.

    channels_ = ChannelLayoutToChannelCount(kChannelLayout);
    AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR,
                           ChannelLayoutConfig::FromLayout<kChannelLayout>(),
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
  NullMediaLog media_log_;
  int frames_enqueued_;
  int channels_;
  ChannelLayout channel_layout_;
  SampleFormat sample_format_;
  int samples_per_second_;
  int bytes_per_sample_;
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

  auto dot_prod = std::make_unique<float[]>(kChannels);

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
  auto energies = std::make_unique<float[]>(kChannels * kNumBlocks);
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

  auto energy_target = std::make_unique<float[]>(kChannels);

  internal::MultiChannelDotProduct(target.get(), 0, target.get(), 0,
                                   kFramePerBlock, energy_target.get());

  ASSERT_EQ(3.f, energy_target[0]);
  ASSERT_EQ(2.01f, energy_target[1]);

  const int kNumCandidBlocks = kFramesInSearchRegion - (kFramePerBlock - 1);
  auto energy_candid_blocks =
      std::make_unique<float[]>(kNumCandidBlocks * kChannels);

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
  // Pad the queue capacity so fill requests for all rates below can be fully
  // satisfied.
  algorithm_.IncreasePlaybackThreshold();

  std::unique_ptr<AudioBus> bus = AudioBus::Create(channels_, kFrameSize);

  // Verify that the first half of |bus| remains zero and the last half is
  // filled appropriately at normal, above normal, and below normal.
  const int kHalfSize = kFrameSize / 2;
  const float kAudibleRates[] = {1.0f, 2.0f, 0.5f, 5.0f, 0.25f};
  for (size_t i = 0; i < std::size(kAudibleRates); ++i) {
    SCOPED_TRACE(kAudibleRates[i]);
    bus->Zero();

    const int frames_filled = algorithm_.FillBuffer(
        bus.get(), kHalfSize, kHalfSize, kAudibleRates[i]);
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
  const int kBufferSize = kSamplesPerSecond / 50;
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
  const int kBufferSize = kSamplesPerSecond / 50;
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
  const int kBufferSize = kSamplesPerSecond / 50;
  Initialize(CHANNEL_LAYOUT_STEREO, kSampleFormatS16, kSamplesPerSecond,
             kBufferSize);
  const int default_capacity = algorithm_.QueueCapacity();

  // FlushBuffers to start out empty.
  algorithm_.FlushBuffers();

  // Set a crazy high latency hint.
  algorithm_.SetLatencyHint(base::Seconds(100));

  const base::TimeDelta kDefaultMax = base::Seconds(3);
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
