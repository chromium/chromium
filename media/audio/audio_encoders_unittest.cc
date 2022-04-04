// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/audio/audio_opus_encoder.h"
#include "media/audio/simple_sources.h"
#include "media/base/audio_encoder.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/status.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/opus/src/include/opus.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/scoped_com_initializer.h"
#include "base/win/windows_version.h"
#include "media/gpu/windows/mf_audio_encoder.h"
#endif  // BUILDFLAG(IS_WIN)

namespace media {

namespace {

constexpr int kAudioSampleRateWithDelay = 647744;

// This is the preferred opus buffer duration (60 ms), which corresponds to a
// value of 2880 frames per buffer at a sample rate of 48 khz.
constexpr base::TimeDelta kOpusBufferDuration = base::Milliseconds(60);

#if BUILDFLAG(IS_WIN)
// AAC puts 1024 PCM samples into each AAC frame, which corresponds to a
// duration of 21 and 1/3 milliseconds at a sample rate of 48 khz.
constexpr int kAacFramesPerBuffer = 1024;
#endif  // BUILDFLAG(IS_WIN)

struct TestAudioParams {
  const AudioCodec codec;
  const int channels;
  const int sample_rate;
};

constexpr TestAudioParams kTestAudioParamsOpus[] = {
    {AudioCodec::kOpus, 2, 48000},
    // Change to mono:
    {AudioCodec::kOpus, 1, 48000},
    // Different sampling rate as well:
    {AudioCodec::kOpus, 1, 24000},
    {AudioCodec::kOpus, 2, 8000},
    // Using a non-default Opus sampling rate (48, 24, 16, 12, or 8 kHz).
    {AudioCodec::kOpus, 1, 22050},
    {AudioCodec::kOpus, 2, 44100},
    {AudioCodec::kOpus, 2, 96000},
    {AudioCodec::kOpus, 1, 48000},
    {AudioCodec::kOpus, 2, 48000},
    {AudioCodec::kOpus, 2, kAudioSampleRateWithDelay},
};

#if BUILDFLAG(IS_WIN)
constexpr TestAudioParams kTestAudioParamsAAC[] = {
    {AudioCodec::kAAC, 2, 48000}, {AudioCodec::kAAC, 6, 48000},
    {AudioCodec::kAAC, 1, 48000}, {AudioCodec::kAAC, 2, 44100},
    {AudioCodec::kAAC, 6, 44100}, {AudioCodec::kAAC, 1, 44100},
};
#endif  // BUILDFLAG(IS_WIN)

std::string EncoderStatusCodeToString(EncoderStatus::Codes code) {
  switch (code) {
    case EncoderStatus::Codes::kOk:
      return "kOk";
    case EncoderStatus::Codes::kEncoderInitializeNeverCompleted:
      return "kEncoderInitializeNeverCompleted";
    case EncoderStatus::Codes::kEncoderInitializeTwice:
      return "kEncoderInitializeTwice";
    case EncoderStatus::Codes::kEncoderFailedEncode:
      return "kEncoderFailedEncode";
    case EncoderStatus::Codes::kEncoderUnsupportedProfile:
      return "kEncoderUnsupportedProfile";
    case EncoderStatus::Codes::kEncoderUnsupportedCodec:
      return "kEncoderUnsupportedCodec";
    case EncoderStatus::Codes::kEncoderUnsupportedConfig:
      return "kEncoderUnsupportedConfig";
    case EncoderStatus::Codes::kEncoderInitializationError:
      return "kEncoderInitializationError";
    case EncoderStatus::Codes::kEncoderFailedFlush:
      return "kEncoderFailedFlush";
    case EncoderStatus::Codes::kEncoderMojoConnectionError:
      return "kEncoderMojoConnectionError";
    default:
      NOTREACHED();
      return "default";
  }
}

bool TimesAreNear(base::TimeTicks t1,
                  base::TimeTicks t2,
                  base::TimeDelta error) {
  return (t1 - t2).magnitude() <= error;
}

}  // namespace

class AudioEncodersTest : public ::testing::TestWithParam<TestAudioParams> {
 public:
  AudioEncodersTest()
      : audio_source_(GetParam().channels,
                      /*freq=*/440,
                      GetParam().sample_rate) {
    options_.codec = GetParam().codec;
    options_.sample_rate = GetParam().sample_rate;
    options_.channels = GetParam().channels;
  }
  AudioEncodersTest(const AudioEncodersTest&) = delete;
  AudioEncodersTest& operator=(const AudioEncodersTest&) = delete;
  ~AudioEncodersTest() override = default;

  using MaybeDesc = absl::optional<AudioEncoder::CodecDescription>;

  AudioEncoder* encoder() const { return encoder_.get(); }

  bool EncoderHasDelay() const {
    return options_.sample_rate == kAudioSampleRateWithDelay;
  }

  void SetUp() override {
    if (options_.codec == AudioCodec::kOpus) {
      encoder_ = std::make_unique<AudioOpusEncoder>();
      buffer_duration_ = kOpusBufferDuration;
      frames_per_buffer_ = AudioTimestampHelper::TimeToFrames(
          buffer_duration_, options_.sample_rate);
    } else if (options_.codec == AudioCodec::kAAC) {
#if BUILDFLAG(IS_WIN)
      EXPECT_TRUE(com_initializer_.Succeeded());
      if (options_.channels == 6 &&
          base::win::GetVersion() < base::win::Version::WIN10) {
        GTEST_SKIP() << "5.1 channel audio is not supported by the MF AAC "
                        "encoder on versions below Win10.";
      }
      ASSERT_TRUE(base::SequencedTaskRunnerHandle::IsSet());
      encoder_ = std::make_unique<MFAudioEncoder>(
          base::SequencedTaskRunnerHandle::Get());
      frames_per_buffer_ = kAacFramesPerBuffer;
      buffer_duration_ = AudioTimestampHelper::FramesToTime(
          frames_per_buffer_, options_.sample_rate);
#else
      NOTREACHED();
#endif
    } else {
      NOTREACHED();
    }
  }

  void InitializeEncoder(
      AudioEncoder::OutputCB output_cb = base::NullCallback()) {
    if (!output_cb) {
      output_cb =
          base::BindLambdaForTesting([&](EncodedAudioBuffer output, MaybeDesc) {
            observed_output_duration_ += output.duration;
          });
    }

    bool called_done = false;
    AudioEncoder::EncoderStatusCB done_cb =
        base::BindLambdaForTesting([&](EncoderStatus error) {
          if (!error.is_ok())
            FAIL() << "Error code: " << EncoderStatusCodeToString(error.code())
                   << "\nError message: " << error.message();
          called_done = true;
        });

    encoder_->Initialize(options_, std::move(output_cb), std::move(done_cb));

    RunLoop();
    EXPECT_TRUE(called_done);
  }

  // Produces an audio data with |num_frames| frames. The produced data is sent
  // to |encoder_| to be encoded, and the number of frames generated is
  // returned.
  int ProduceAudioAndEncode(
      base::TimeTicks timestamp = base::TimeTicks::Now(),
      int num_frames = 0,
      AudioEncoder::EncoderStatusCB done_cb = base::NullCallback()) {
    DCHECK(encoder_);
    if (num_frames == 0)
      num_frames = frames_per_buffer_;

    auto audio_bus = AudioBus::Create(options_.channels, num_frames);
    audio_source_.OnMoreData(base::TimeDelta(), timestamp, 0, audio_bus.get());

    DoEncode(std::move(audio_bus), timestamp, std::move(done_cb));

    return num_frames;
  }

  void DoEncode(std::unique_ptr<AudioBus> audio_bus,
                base::TimeTicks timestamp,
                AudioEncoder::EncoderStatusCB done_cb = base::NullCallback()) {
    if (!done_cb) {
      pending_callback_results_.emplace_back();
      done_cb = base::BindLambdaForTesting([&](EncoderStatus error) {
        if (!error.is_ok())
          FAIL() << "Error code: " << EncoderStatusCodeToString(error.code())
                 << "\nError message: " << error.message();

        pending_callback_results_[pending_callback_count_].status_code =
            error.code();
        pending_callback_results_[pending_callback_count_].completed = true;
        pending_callback_count_++;
      });
    }

    int num_frames = audio_bus->frames();

    encoder_->Encode(std::move(audio_bus), timestamp, std::move(done_cb));
    expected_output_duration_ +=
        AudioTimestampHelper::FramesToTime(num_frames, options_.sample_rate);
  }

  void FlushAndVerifyStatus(
      EncoderStatus::Codes status_code = EncoderStatus::Codes::kOk) {
    bool flush_done = false;
    auto flush_done_cb = base::BindLambdaForTesting([&](EncoderStatus error) {
      if (error.code() != status_code)
        FAIL() << "Expected " << EncoderStatusCodeToString(status_code)
               << " but got " << EncoderStatusCodeToString(error.code());
      flush_done = true;
    });
    encoder()->Flush(std::move(flush_done_cb));
    RunLoop();
    EXPECT_TRUE(flush_done);
  }

  void ValidateDoneCallbacksRun() {
    for (auto callback_result : pending_callback_results_) {
      EXPECT_TRUE(callback_result.completed);
      EXPECT_EQ(callback_result.status_code, EncoderStatus::Codes::kOk);
    }
  }

  void ValidateOutputDuration() {
    base::TimeDelta delta =
        (expected_output_duration_ - observed_output_duration_).magnitude();
    EXPECT_LE(delta, base::Microseconds(10));
  }

  void RunLoop() { run_loop_.RunUntilIdle(); }

  base::test::TaskEnvironment task_environment_;
  base::RunLoop run_loop_;

#if BUILDFLAG(IS_WIN)
  ::base::win::ScopedCOMInitializer com_initializer_;
#endif  // BUILDFLAG(IS_WIN)

  // The input params as initialized from the test's parameter.
  AudioEncoder::Options options_;

  // The audio source used to generate data to give to the |encoder_|.
  SineWaveAudioSource audio_source_;

  // The encoder the test is verifying.
  std::unique_ptr<AudioEncoder> encoder_;

  base::TimeDelta buffer_duration_;
  int frames_per_buffer_;

  base::TimeDelta expected_output_duration_;
  base::TimeDelta observed_output_duration_;

  struct CallbackResult {
    bool completed = false;
    EncoderStatus::Codes status_code;
  };

  int pending_callback_count_ = 0;
  std::vector<CallbackResult> pending_callback_results_;
};

TEST_P(AudioEncodersTest, InitializeTwice) {
  InitializeEncoder();
  bool called_done = false;
  auto done_cb = base::BindLambdaForTesting([&](EncoderStatus error) {
    if (error.code() != EncoderStatus::Codes::kEncoderInitializeTwice)
      FAIL() << "Expected kEncoderInitializeTwice error but got "
             << EncoderStatusCodeToString(error.code());
    called_done = true;
  });

  encoder_->Initialize(options_, base::DoNothing(), std::move(done_cb));

  RunLoop();
  EXPECT_TRUE(called_done);
}

TEST_P(AudioEncodersTest, EncodeWithoutInitialize) {
  bool called_done = false;
  auto done_cb = base::BindLambdaForTesting([&](EncoderStatus error) {
    if (error.code() != EncoderStatus::Codes::kEncoderInitializeNeverCompleted)
      FAIL() << "Expected kEncoderInitializeNeverCompleted error but got "
             << EncoderStatusCodeToString(error.code());
    called_done = true;
  });

  auto audio_bus = AudioBus::Create(options_.channels, /*num_frames=*/1);
  encoder()->Encode(std::move(audio_bus), base::TimeTicks::Now(),
                    std::move(done_cb));

  RunLoop();
  EXPECT_TRUE(called_done);
}

TEST_P(AudioEncodersTest, FlushWithoutInitialize) {
  FlushAndVerifyStatus(EncoderStatus::Codes::kEncoderInitializeNeverCompleted);
}

TEST_P(AudioEncodersTest, FlushWithNoInput) {
  InitializeEncoder();

  FlushAndVerifyStatus();
}

TEST_P(AudioEncodersTest, EncodeAndFlush) {
  if (EncoderHasDelay())
    return;

  InitializeEncoder();
  ProduceAudioAndEncode();
  ProduceAudioAndEncode();
  ProduceAudioAndEncode();

  FlushAndVerifyStatus();

  ValidateDoneCallbacksRun();
  ValidateOutputDuration();
}

TEST_P(AudioEncodersTest, EncodeAndFlushTwice) {
  if (EncoderHasDelay())
    return;

  InitializeEncoder();
  ProduceAudioAndEncode();
  ProduceAudioAndEncode();
  ProduceAudioAndEncode();

  bool called_flush1 = false;
  auto flush_cb = base::BindLambdaForTesting([&](EncoderStatus error) {
    if (error.code() != EncoderStatus::Codes::kOk)
      FAIL() << "Expected kOk but got "
             << EncoderStatusCodeToString(error.code());
    called_flush1 = true;
  });

  encoder()->Flush(std::move(flush_cb));
  ProduceAudioAndEncode();
  ProduceAudioAndEncode();
  ProduceAudioAndEncode();

  bool called_flush2 = false;
  flush_cb = base::BindLambdaForTesting([&](EncoderStatus error) {
    if (error.code() != EncoderStatus::Codes::kOk)
      FAIL() << "Expected kOk but got "
             << EncoderStatusCodeToString(error.code());
    called_flush2 = true;
  });

  RunLoop();
  EXPECT_TRUE(called_flush1);

  encoder()->Flush(std::move(flush_cb));

  RunLoop();
  EXPECT_TRUE(called_flush2);
  ValidateDoneCallbacksRun();
  ValidateOutputDuration();
}

// Instead of synchronously calling `Encode`, wait until `done_cb` is invoked
// before we provide more input.
TEST_P(AudioEncodersTest, ProvideInputAfterDoneCb) {
  if (EncoderHasDelay())
    return;

  InitializeEncoder();

  bool called_done = false;
  auto done_lambda = [&](EncoderStatus error) {
    if (error.code() != EncoderStatus::Codes::kOk)
      FAIL() << "Expected kOk but got "
             << EncoderStatusCodeToString(error.code());
    called_done = true;
  };
  AudioEncoder::EncoderStatusCB done_cb =
      base::BindLambdaForTesting(done_lambda);
  ProduceAudioAndEncode(base::TimeTicks(), frames_per_buffer_,
                        std::move(done_cb));
  RunLoop();
  EXPECT_TRUE(called_done);

  called_done = false;
  done_cb = base::BindLambdaForTesting(done_lambda);
  ProduceAudioAndEncode(base::TimeTicks(), frames_per_buffer_,
                        std::move(done_cb));
  RunLoop();
  EXPECT_TRUE(called_done);

  called_done = false;
  done_cb = base::BindLambdaForTesting(done_lambda);
  ProduceAudioAndEncode(base::TimeTicks(), frames_per_buffer_,
                        std::move(done_cb));
  RunLoop();
  EXPECT_TRUE(called_done);

  FlushAndVerifyStatus();

  ValidateOutputDuration();
}

// Check that the encoder's timestamps don't drift from the expected timestamp
// over multiple inputs.
TEST_P(AudioEncodersTest, Timestamps) {
  if (EncoderHasDelay())
    return;

  std::vector<base::TimeTicks> timestamps;
  AudioEncoder::OutputCB output_cb =
      base::BindLambdaForTesting([&](EncodedAudioBuffer output, MaybeDesc) {
        timestamps.push_back(output.timestamp);
      });

  InitializeEncoder(std::move(output_cb));
  constexpr int kCount = 12;

  // Try to encode buffers of different durations. The timestamps of each output
  // should increase by `buffer_duration_` regardless of the size of the input.
  for (base::TimeDelta duration :
       {buffer_duration_ * 10, buffer_duration_, buffer_duration_ * 2 / 3}) {
    timestamps.clear();
    int num_frames =
        AudioTimestampHelper::TimeToFrames(duration, options_.sample_rate);

    // The encoder will have multiple outputs per input if `num_frames` is
    // larger than `frames_per_buffer_`, and fewer outputs per input if it is
    // smaller.
    size_t expected_outputs = (num_frames * kCount) / frames_per_buffer_;

    // Round up if the division truncated. This is because the encoder will pad
    // the final buffer to produce output, even if there aren't
    // `frames_per_buffer_` left.
    if ((num_frames * kCount) % frames_per_buffer_ != 0)
      expected_outputs++;

    base::TimeTicks current_timestamp;
    for (int i = 0; i < kCount; ++i) {
      ProduceAudioAndEncode(current_timestamp, num_frames);
      current_timestamp += duration;
    }

    FlushAndVerifyStatus();

    ValidateDoneCallbacksRun();
    EXPECT_EQ(expected_outputs, timestamps.size());

    // We must use an `AudioTimestampHelper` to verify the returned timestamps
    // to avoid rounding errors.
    AudioTimestampHelper timestamp_tracker(options_.sample_rate);
    timestamp_tracker.SetBaseTimestamp(base::Microseconds(0));
    for (auto& observed_ts : timestamps) {
      base::TimeTicks expected_ts =
          timestamp_tracker.GetTimestamp() + base::TimeTicks();
      EXPECT_TRUE(
          TimesAreNear(expected_ts, observed_ts, base::Microseconds(1)));
      timestamp_tracker.AddFrames(frames_per_buffer_);
    }
  }
}

// Check how the encoder reacts to breaks in continuity of incoming sound.
// Under normal circumstances capture times are expected to be exactly
// a buffer's duration apart, but if they are not, the encoder just ignores
// incoming capture times. In other words the only capture times that matter
// are
//   1. timestamp of the first encoded buffer
//   2. timestamps of buffers coming immediately after Flush() calls.
TEST_P(AudioEncodersTest, TimeContinuityBreak) {
  if (EncoderHasDelay())
    return;

  std::vector<base::TimeTicks> timestamps;
  AudioEncoder::OutputCB output_cb =
      base::BindLambdaForTesting([&](EncodedAudioBuffer output, MaybeDesc) {
        timestamps.push_back(output.timestamp);
      });

  InitializeEncoder(std::move(output_cb));

  // Encode first normal buffer.
  base::TimeTicks current_timestamp = base::TimeTicks::Now();
  auto ts0 = current_timestamp;
  ProduceAudioAndEncode(current_timestamp);
  current_timestamp += buffer_duration_;

  // Encode another buffer after a large gap, output timestamp should
  // disregard the gap.
  auto ts1 = current_timestamp;
  current_timestamp += base::Microseconds(1500);
  ProduceAudioAndEncode(current_timestamp);
  current_timestamp += buffer_duration_;

  // Another buffer without a gap.
  auto ts2 = ts1 + buffer_duration_;
  ProduceAudioAndEncode(current_timestamp);

  FlushAndVerifyStatus();

  ASSERT_EQ(3u, timestamps.size());
  EXPECT_TRUE(TimesAreNear(ts0, timestamps[0], base::Microseconds(1)));
  EXPECT_TRUE(TimesAreNear(ts1, timestamps[1], base::Microseconds(1)));
  EXPECT_TRUE(TimesAreNear(ts2, timestamps[2], base::Microseconds(1)));

  // Reset output timestamp after Flush(), the encoder should start producing
  // timestamps from new base 0.
  current_timestamp = base::TimeTicks();
  auto ts3 = current_timestamp;
  ProduceAudioAndEncode(current_timestamp);
  current_timestamp += buffer_duration_;

  auto ts4 = current_timestamp;
  ProduceAudioAndEncode(current_timestamp);

  FlushAndVerifyStatus();

  ASSERT_EQ(5u, timestamps.size());
  EXPECT_TRUE(TimesAreNear(ts3, timestamps[3], base::Microseconds(1)));
  EXPECT_TRUE(TimesAreNear(ts4, timestamps[4], base::Microseconds(1)));
  ValidateDoneCallbacksRun();
}

INSTANTIATE_TEST_SUITE_P(Opus,
                         AudioEncodersTest,
                         testing::ValuesIn(kTestAudioParamsOpus));

#if BUILDFLAG(IS_WIN)
INSTANTIATE_TEST_SUITE_P(AAC,
                         AudioEncodersTest,
                         testing::ValuesIn(kTestAudioParamsAAC));
#endif  // BUILDFLAG(IS_WIN)

class AudioOpusEncoderTest : public AudioEncodersTest {
 public:
  AudioOpusEncoderTest() { options_.codec = AudioCodec::kOpus; }
  AudioOpusEncoderTest(const AudioOpusEncoderTest&) = delete;
  AudioOpusEncoderTest& operator=(const AudioOpusEncoderTest&) = delete;
  ~AudioOpusEncoderTest() override = default;

  void SetUp() override { AudioEncodersTest::SetUp(); }
};

TEST_P(AudioOpusEncoderTest, ExtraData) {
  if (EncoderHasDelay())
    return;

  std::vector<uint8_t> extra;
  AudioEncoder::OutputCB output_cb = base::BindLambdaForTesting(
      [&](EncodedAudioBuffer output, MaybeDesc desc) {
        DCHECK(desc.has_value());
        extra = desc.value();
      });

  InitializeEncoder(std::move(output_cb));
  ProduceAudioAndEncode();
  RunLoop();

  ASSERT_GT(extra.size(), 0u);
  EXPECT_EQ(extra[0], 'O');
  EXPECT_EQ(extra[1], 'p');
  EXPECT_EQ(extra[2], 'u');
  EXPECT_EQ(extra[3], 's');

  uint16_t* sample_rate_ptr = reinterpret_cast<uint16_t*>(extra.data() + 12);
  if (options_.sample_rate < std::numeric_limits<uint16_t>::max())
    EXPECT_EQ(*sample_rate_ptr, options_.sample_rate);
  else
    EXPECT_EQ(*sample_rate_ptr, 48000);

  uint8_t* channels_ptr = reinterpret_cast<uint8_t*>(extra.data() + 9);
  EXPECT_EQ(*channels_ptr, options_.channels);

  uint16_t* skip_ptr = reinterpret_cast<uint16_t*>(extra.data() + 10);
  EXPECT_GT(*skip_ptr, 0);
}

TEST_P(AudioOpusEncoderTest, FullCycleEncodeDecode) {
  const int kOpusDecoderSampleRate = 48000;
  const int kOpusDecoderFramesPerBuffer = kOpusBufferDuration.InMicroseconds() *
                                          kOpusDecoderSampleRate /
                                          base::Time::kMicrosecondsPerSecond;
  int error;
  OpusDecoder* opus_decoder =
      opus_decoder_create(kOpusDecoderSampleRate, options_.channels, &error);
  ASSERT_TRUE(error == OPUS_OK && opus_decoder);

  int encode_callback_count = 0;
  std::vector<float> buffer(kOpusDecoderFramesPerBuffer * options_.channels);
  auto verify_opus_encoding = [&](EncodedAudioBuffer output, MaybeDesc) {
    ++encode_callback_count;

    // Use the libopus decoder to decode the |encoded_data| and check we
    // get the expected number of frames per buffer.
    EXPECT_EQ(kOpusDecoderFramesPerBuffer,
              opus_decode_float(opus_decoder, output.encoded_data.get(),
                                output.encoded_data_size, buffer.data(),
                                kOpusDecoderFramesPerBuffer, 0));
  };

  InitializeEncoder(base::BindLambdaForTesting(verify_opus_encoding));

  base::TimeTicks time;
  int total_frames = 0;
  while (total_frames < frames_per_buffer_) {
    total_frames += ProduceAudioAndEncode(time);
    time += buffer_duration_;
  }

  RunLoop();

  // If there are remaining frames in the opus encoder FIFO, we need to flush
  // them before we destroy the encoder. Also flush any encoders with delays.
  bool needs_flushing = total_frames > frames_per_buffer_ || EncoderHasDelay();

  if (!EncoderHasDelay())
    EXPECT_EQ(1, encode_callback_count);

  // Flushing should trigger the encode callback and we should be able to decode
  // the resulting encoded frames.
  if (needs_flushing) {
    FlushAndVerifyStatus();

    EXPECT_EQ(2, encode_callback_count);
  }

  opus_decoder_destroy(opus_decoder);
  opus_decoder = nullptr;
}

TEST_P(AudioOpusEncoderTest, VariableChannelCounts) {
  constexpr int kTestToneFrequency = 440;
  SineWaveAudioSource sources[] = {
      SineWaveAudioSource(1, kTestToneFrequency, options_.sample_rate),
      SineWaveAudioSource(2, kTestToneFrequency, options_.sample_rate),
      SineWaveAudioSource(3, kTestToneFrequency, options_.sample_rate)};

  const int num_frames = options_.sample_rate * buffer_duration_.InSecondsF();

  auto generate_audio = [&sources, &num_frames](
                            int channel_count,
                            base::TimeTicks current_timestamp) {
    auto audio_bus = AudioBus::Create(channel_count, num_frames);
    sources[channel_count - 1].OnMoreData(base::TimeDelta(), current_timestamp,
                                          0, audio_bus.get());
    return audio_bus;
  };

  // Superpermutation of {1, 2, 3}, covering all transitions between upmixing,
  // downmixing and not mixing.
  const int kChannelCountSequence[] = {1, 2, 3, 1, 2, 2, 1, 3, 2, 1};

  // Override |GetParam().channels|, to ensure that we can both upmix and
  // downmix.
  options_.channels = 2;

  auto empty_output_cb =
      base::BindLambdaForTesting([&](EncodedAudioBuffer output, MaybeDesc) {});

  InitializeEncoder(std::move(empty_output_cb));

  base::TimeTicks current_timestamp;
  for (const int& ch : kChannelCountSequence) {
    // Encode, using a different number of channels each time.
    DoEncode(generate_audio(ch, current_timestamp), current_timestamp);
    current_timestamp += buffer_duration_;
  }

  FlushAndVerifyStatus();
}

INSTANTIATE_TEST_SUITE_P(Opus,
                         AudioOpusEncoderTest,
                         testing::ValuesIn(kTestAudioParamsOpus));

#if BUILDFLAG(IS_WIN)
class MFAudioEncoderTest : public AudioEncodersTest {
 public:
  MFAudioEncoderTest() = default;
  MFAudioEncoderTest(const MFAudioEncoderTest&) = delete;
  MFAudioEncoderTest& operator=(const MFAudioEncoderTest&) = delete;
  ~MFAudioEncoderTest() override = default;
};

// `MFAudioEncoder` requires `kMinSamplesForOutput` before `Flush` can be called
// successfully.
TEST_P(MFAudioEncoderTest, FlushWithTooLittleInput) {
  InitializeEncoder(base::DoNothing());
  ProduceAudioAndEncode();

  FlushAndVerifyStatus(EncoderStatus::Codes::kEncoderFailedFlush);

  ValidateDoneCallbacksRun();
}

INSTANTIATE_TEST_SUITE_P(AAC,
                         MFAudioEncoderTest,
                         testing::ValuesIn(kTestAudioParamsAAC));
#endif  // BUILDFLAG(IS_WIN)

}  // namespace media
