// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <cstring>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/audio/audio_opus_encoder.h"
#include "media/audio/simple_sources.h"
#include "media/base/audio_encoder.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/converting_audio_fifo.h"
#include "media/base/status.h"
#include "media/media_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/opus/src/include/opus.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/scoped_com_initializer.h"
#include "media/gpu/windows/mf_audio_encoder.h"

// The AAC tests are failing on Arm64. Disable the AAC part of these tests until
// those failures can be fixed. TODO(https://crbug.com/1424215): Fix tests,
// and/or investigate if AAC support should be turned off in Chrome for Arm64
// Windows, or if these are an issue with the tests.
#if !defined(ARCH_CPU_ARM64)
#define HAS_AAC_ENCODER 1
#endif

#endif  // IS_WIN

#if BUILDFLAG(IS_MAC) && BUILDFLAG(USE_PROPRIETARY_CODECS)
#include "media/filters/mac/audio_toolbox_audio_encoder.h"
#define HAS_AAC_ENCODER 1
#endif

#if BUILDFLAG(IS_ANDROID) && BUILDFLAG(USE_PROPRIETARY_CODECS)
#include "media/gpu/android/ndk_audio_encoder.h"
#define HAS_AAC_ENCODER 1
#endif

#if HAS_AAC_ENCODER
#include "media/base/audio_decoder.h"
#include "media/base/channel_layout.h"
#include "media/base/decoder_status.h"
#include "media/base/mock_media_log.h"
#include "media/filters/ffmpeg_audio_decoder.h"
#endif

namespace media {

namespace {

constexpr int kAudioSampleRateWithDelay = 647744;

// This is the preferred opus buffer duration (20 ms), which corresponds to a
// value of 960 frames per buffer at a sample rate of 48 khz.
constexpr base::TimeDelta kOpusBufferDuration = base::Milliseconds(20);

#if HAS_AAC_ENCODER
// AAC puts 1024 PCM samples into each AAC frame, which corresponds to a
// duration of 21 and 1/3 milliseconds at a sample rate of 48 khz.
constexpr int kAacFramesPerBuffer = 1024;
#endif  //  HAS_AAC_ENCODER

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
    {AudioCodec::kOpus, 2, kAudioSampleRateWithDelay},
};

#if HAS_AAC_ENCODER
constexpr TestAudioParams kTestAudioParamsAAC[] = {
    {AudioCodec::kAAC, 2, 48000}, {AudioCodec::kAAC, 6, 48000},
    {AudioCodec::kAAC, 1, 48000}, {AudioCodec::kAAC, 2, 44100},
    {AudioCodec::kAAC, 6, 44100}, {AudioCodec::kAAC, 1, 44100},
};
#endif  // HAS_AAC_ENCODER

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
    expected_duration_helper_ =
        std::make_unique<AudioTimestampHelper>(options_.sample_rate);
    expected_duration_helper_->SetBaseTimestamp(base::Microseconds(0));
  }
  AudioEncodersTest(const AudioEncodersTest&) = delete;
  AudioEncodersTest& operator=(const AudioEncodersTest&) = delete;
  ~AudioEncodersTest() override = default;

  using MaybeDesc = std::optional<AudioEncoder::CodecDescription>;

  AudioEncoder* encoder() const { return encoder_.get(); }

  bool EncoderHasDelay() const {
    return options_.sample_rate == kAudioSampleRateWithDelay;
  }

  void SetUp() override { CreateEncoder(); }

  void CreateEncoder() {
    if (options_.codec == AudioCodec::kOpus) {
      encoder_ = std::make_unique<AudioOpusEncoder>();
      buffer_duration_ = kOpusBufferDuration;
      frames_per_buffer_ = AudioTimestampHelper::TimeToFrames(
          buffer_duration_, options_.sample_rate);
    } else if (options_.codec == AudioCodec::kAAC) {
#if BUILDFLAG(IS_WIN) && HAS_AAC_ENCODER
      EXPECT_TRUE(com_initializer_.Succeeded());
      ASSERT_TRUE(base::SequencedTaskRunner::HasCurrentDefault());
      encoder_ = std::make_unique<MFAudioEncoder>(
          base::SequencedTaskRunner::GetCurrentDefault());
      frames_per_buffer_ = kAacFramesPerBuffer;
      buffer_duration_ = AudioTimestampHelper::FramesToTime(
          frames_per_buffer_, options_.sample_rate);
#elif HAS_AAC_ENCODER && BUILDFLAG(IS_MAC)
      encoder_ = std::make_unique<AudioToolboxAudioEncoder>();
      frames_per_buffer_ = kAacFramesPerBuffer;
      buffer_duration_ = AudioTimestampHelper::FramesToTime(
          frames_per_buffer_, options_.sample_rate);
#elif HAS_AAC_ENCODER && BUILDFLAG(IS_ANDROID)
      if (__builtin_available(android NDK_MEDIA_CODEC_MIN_API, *)) {
        encoder_ = std::make_unique<NdkAudioEncoder>(
            base::SequencedTaskRunner::GetCurrentDefault());
        frames_per_buffer_ = kAacFramesPerBuffer;
        buffer_duration_ = AudioTimestampHelper::FramesToTime(
            frames_per_buffer_, options_.sample_rate);
      } else {
        GTEST_SKIP() << "NDK AAC encoder not supported. Skipping test.";
        // GTEST_SKIP() returns.
      }
#else
      NOTREACHED_IN_MIGRATION();
#endif
    } else {
      NOTREACHED_IN_MIGRATION();
    }

    min_number_input_frames_needed_ = frames_per_buffer_;
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
          if (!error.is_ok()) {
            FAIL() << "Error code: " << EncoderStatusCodeToString(error.code())
                   << "\nError message: " << error.message();
          }
          called_done = true;
        });

    encoder_->Initialize(options_, std::move(output_cb), std::move(done_cb));

    task_environment_.RunUntilIdle();
    EXPECT_TRUE(called_done);

    if (options_.codec == AudioCodec::kOpus) {
      min_number_input_frames_needed_ =
          reinterpret_cast<AudioOpusEncoder*>(encoder_.get())
              ->fifo_->min_number_input_frames_needed_for_testing();
    }
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
    audio_source_.OnMoreData(base::TimeDelta(), timestamp, {}, audio_bus.get());

    DoEncode(std::move(audio_bus), timestamp, std::move(done_cb));

    return num_frames;
  }

  void DoEncode(std::unique_ptr<AudioBus> audio_bus,
                base::TimeTicks timestamp,
                AudioEncoder::EncoderStatusCB done_cb = base::NullCallback()) {
    if (!done_cb) {
      pending_callback_results_.emplace_back();
      done_cb = base::BindLambdaForTesting([&](EncoderStatus error) {
        if (!error.is_ok()) {
          FAIL() << "Error code: " << EncoderStatusCodeToString(error.code())
                 << "\nError message: " << error.message();
        }

        pending_callback_results_[pending_callback_count_].status_code =
            error.code();
        pending_callback_results_[pending_callback_count_].completed = true;
        pending_callback_count_++;
      });
    }

    int num_frames = audio_bus->frames();

    encoder_->Encode(std::move(audio_bus), timestamp, std::move(done_cb));
    expected_output_duration_ +=
        expected_duration_helper_->GetFrameDuration(num_frames);
    expected_duration_helper_->AddFrames(num_frames);
  }

  void FlushAndVerifyStatus(
      EncoderStatus::Codes status_code = EncoderStatus::Codes::kOk) {
    base::RunLoop run_loop;

    bool flush_done = false;
    auto flush_done_cb = base::BindLambdaForTesting([&](EncoderStatus error) {
      if (error.code() != status_code) {
        FAIL() << "Expected " << EncoderStatusCodeToString(status_code)
               << " but got " << EncoderStatusCodeToString(error.code());
      }
      flush_done = true;
    });
    encoder()->Flush(
        std::move(flush_done_cb).Then(run_loop.QuitWhenIdleClosure()));

    run_loop.Run();
    EXPECT_TRUE(flush_done);
  }

  void ValidateDoneCallbacksRun() {
    for (auto callback_result : pending_callback_results_) {
      EXPECT_TRUE(callback_result.completed);
      EXPECT_EQ(callback_result.status_code, EncoderStatus::Codes::kOk);
    }
  }

  // The amount of front padding that the encoder emits.
  size_t GetExpectedPadding() {
#if BUILDFLAG(IS_MAC)
    if (options_.codec == AudioCodec::kAAC)
      return 2112;
#endif
    return 0;
  }

  void ValidateOutputDuration(int64_t flush_count = 1) {
    // Since encoders can only output buffers of size `frames_per_buffer_`, the
    // number of outputs will be larger than the number of inputs.
    int64_t frame_remainder =
        frames_per_buffer_ -
        (expected_duration_helper_->frame_count() % frames_per_buffer_);

    int64_t amount_of_padding = GetExpectedPadding() + frame_remainder;

    // Padding is re-emitted after each flush.
    amount_of_padding *= flush_count;

    int64_t number_of_outputs = std::ceil(
        (expected_duration_helper_->frame_count() + amount_of_padding) /
        static_cast<double>(frames_per_buffer_));
    int64_t duration_of_padding_us =
        number_of_outputs * AudioTimestampHelper::FramesToTime(
                                frames_per_buffer_, options_.sample_rate)
                                .InMicroseconds();
    int64_t acceptable_diff = duration_of_padding_us + 10;
    EXPECT_NEAR(expected_output_duration_.InMicroseconds(),
                observed_output_duration_.InMicroseconds(), acceptable_diff);
  }

  base::test::TaskEnvironment task_environment_;

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
  int min_number_input_frames_needed_;

  std::unique_ptr<AudioTimestampHelper> expected_duration_helper_;
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

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(called_done);
}

TEST_P(AudioEncodersTest, StopCallbackWrapping) {
  bool called_done = false;
  AudioEncoder::EncoderStatusCB done_cb = base::BindLambdaForTesting(
      [&](EncoderStatus error) { called_done = true; });

  encoder_->DisablePostedCallbacks();
  encoder_->Initialize(options_, base::DoNothing(), std::move(done_cb));
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

  auto audio_bus = AudioBus::Create(options_.channels, /*frames=*/1);
  encoder()->Encode(std::move(audio_bus), base::TimeTicks::Now(),
                    std::move(done_cb));

  task_environment_.RunUntilIdle();
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

  constexpr int kEncodeFlushCycles = 2;

  for (int cycle = 0; cycle < kEncodeFlushCycles; ++cycle) {
    ProduceAudioAndEncode();
    ProduceAudioAndEncode();
    ProduceAudioAndEncode();

    {
      base::RunLoop run_loop;
      bool called_flush = false;
      auto flush_cb = base::BindLambdaForTesting([&](EncoderStatus error) {
        if (error.code() != EncoderStatus::Codes::kOk) {
          FAIL() << "Expected kOk but got "
                 << EncoderStatusCodeToString(error.code());
        }
        called_flush = true;
      });

      encoder()->Flush(
          std::move(flush_cb).Then(run_loop.QuitWhenIdleClosure()));
      run_loop.Run();
      EXPECT_TRUE(called_flush);
    }
  }

  ValidateDoneCallbacksRun();
  ValidateOutputDuration(/*flush_count=*/kEncodeFlushCycles);
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
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(called_done);

  called_done = false;
  done_cb = base::BindLambdaForTesting(done_lambda);
  ProduceAudioAndEncode(base::TimeTicks(), frames_per_buffer_,
                        std::move(done_cb));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(called_done);

  called_done = false;
  done_cb = base::BindLambdaForTesting(done_lambda);
  ProduceAudioAndEncode(base::TimeTicks(), frames_per_buffer_,
                        std::move(done_cb));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(called_done);

  FlushAndVerifyStatus();

  ValidateOutputDuration();
}

TEST_P(AudioEncodersTest, ManySmallInputs) {
  if (EncoderHasDelay())
    return;

  InitializeEncoder();
  base::TimeTicks timestamp = base::TimeTicks::Now();
  int frame_count = frames_per_buffer_ / 10;
  for (int i = 0; i < 100; i++)
    ProduceAudioAndEncode(timestamp, frame_count);

  FlushAndVerifyStatus();

  ValidateDoneCallbacksRun();
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

    size_t total_frames = num_frames * kCount;

#if HAS_AAC_ENCODER
    if (options_.codec == AudioCodec::kAAC &&
        total_frames % kAacFramesPerBuffer) {
      // We send data in chunks of kAacFramesPerBuffer to the encoder, padding
      // it with silence when flushing.
      // Round `total_frames` up to the nearest multiple of kAacFramesPerBuffer.
      int chunks = (total_frames / kAacFramesPerBuffer) + 1;
      total_frames = chunks * kAacFramesPerBuffer;
    }
#endif

    total_frames += GetExpectedPadding();

    base::TimeTicks current_timestamp;
    for (int i = 0; i < kCount; ++i) {
      ProduceAudioAndEncode(current_timestamp, num_frames);
      current_timestamp += duration;
    }

    FlushAndVerifyStatus();

    ValidateDoneCallbacksRun();

    // The encoder will have multiple outputs per input if `num_frames` is
    // larger than `frames_per_buffer_`, and fewer outputs per input if it is
    // smaller.
    size_t expected_outputs = total_frames / frames_per_buffer_;

    // The encoder might output an extra buffer, due to padding.
    EXPECT_TRUE(timestamps.size() == expected_outputs ||
                timestamps.size() == expected_outputs + 1);

    // We must use an `AudioTimestampHelper` to verify the returned timestamps
    // to avoid rounding errors.
    AudioTimestampHelper timestamp_tracker(options_.sample_rate);
    timestamp_tracker.SetBaseTimestamp(base::Microseconds(0));
    for (auto& observed_ts : timestamps) {
      base::TimeTicks expected_ts =
          timestamp_tracker.GetTimestamp() + base::TimeTicks();
      EXPECT_TRUE(TimesAreNear(expected_ts, observed_ts, base::Microseconds(1)))
          << "expected_ts: " << expected_ts << ", observed_ts: " << observed_ts;
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

  ASSERT_LE(3u, timestamps.size());
  EXPECT_TRUE(TimesAreNear(ts0, timestamps[0], base::Microseconds(1)));
  EXPECT_TRUE(TimesAreNear(ts1, timestamps[1], base::Microseconds(1)));
  EXPECT_TRUE(TimesAreNear(ts2, timestamps[2], base::Microseconds(1)));
  timestamps.clear();

  // Reset output timestamp after Flush(), the encoder should start producing
  // timestamps from new base 0.
  current_timestamp = base::TimeTicks();
  auto ts3 = current_timestamp;
  ProduceAudioAndEncode(current_timestamp);
  current_timestamp += buffer_duration_;

  auto ts4 = current_timestamp;
  ProduceAudioAndEncode(current_timestamp);

  FlushAndVerifyStatus();

  ASSERT_LE(2u, timestamps.size());
  EXPECT_TRUE(TimesAreNear(ts3, timestamps[0], base::Microseconds(1)));
  EXPECT_TRUE(TimesAreNear(ts4, timestamps[1], base::Microseconds(1)));
  ValidateDoneCallbacksRun();
}

INSTANTIATE_TEST_SUITE_P(Opus,
                         AudioEncodersTest,
                         testing::ValuesIn(kTestAudioParamsOpus));

#if HAS_AAC_ENCODER
INSTANTIATE_TEST_SUITE_P(AAC,
                         AudioEncodersTest,
                         testing::ValuesIn(kTestAudioParamsAAC));
#endif  // HAS_AAC_ENCODER

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
  ProduceAudioAndEncode(base::TimeTicks::Now(),
                        min_number_input_frames_needed_);
  task_environment_.RunUntilIdle();

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
  const int kOpusDecoderFramesPerBuffer = AudioTimestampHelper::TimeToFrames(
      kOpusBufferDuration, kOpusDecoderSampleRate);

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
              opus_decode_float(opus_decoder, output.encoded_data.data(),
                                output.encoded_data.size(), buffer.data(),
                                kOpusDecoderFramesPerBuffer, 0));
  };

  InitializeEncoder(base::BindLambdaForTesting(verify_opus_encoding));

  base::TimeTicks time;
  int total_frames = 0;

  // Push data until we have a decoded output.
  while (total_frames < min_number_input_frames_needed_) {
    total_frames += ProduceAudioAndEncode(time);
    time += buffer_duration_;

    task_environment_.RunUntilIdle();
  }

  EXPECT_GE(total_frames, frames_per_buffer_);
  EXPECT_EQ(1, encode_callback_count);

  // Flush the leftover data in the encoder, due to encoder delay.
  FlushAndVerifyStatus();

  opus_decoder_destroy(opus_decoder);
  opus_decoder = nullptr;
}

// Tests we can configure the AudioOpusEncoder's bitrate mode.
TEST_P(AudioOpusEncoderTest, FullCycleEncodeDecode_BitrateMode) {
  constexpr AudioEncoder::BitrateMode kTestOpusBitrateMode[] = {
      AudioEncoder::BitrateMode::kConstant,
      AudioEncoder::BitrateMode::kVariable};

  for (const AudioEncoder::BitrateMode& bitrate_mode : kTestOpusBitrateMode) {
    constexpr int kOpusDecoderSampleRate = 48000;
    const int kOpusDecoderFramesPerBuffer = AudioTimestampHelper::TimeToFrames(
        kOpusBufferDuration, kOpusDecoderSampleRate);

    // Override the work done in CreateEncoder().
    encoder_ = std::make_unique<AudioOpusEncoder>();
    options_.bitrate_mode = bitrate_mode;

    int error;
    OpusDecoder* opus_decoder =
        opus_decoder_create(kOpusDecoderSampleRate, options_.channels, &error);
    ASSERT_TRUE(error == OPUS_OK && opus_decoder);

    std::vector<float> buffer(kOpusDecoderFramesPerBuffer * options_.channels);
    auto verify_opus_encoding = [&](EncodedAudioBuffer output, MaybeDesc) {
      // Use the libopus decoder to decode the |encoded_data| and check we
      // get the expected number of frames per buffer.
      EXPECT_EQ(kOpusDecoderFramesPerBuffer,
                opus_decode_float(opus_decoder, output.encoded_data.data(),
                                  output.encoded_data.size(), buffer.data(),
                                  kOpusDecoderFramesPerBuffer, 0));
    };

    InitializeEncoder(base::BindLambdaForTesting(verify_opus_encoding));

    base::TimeTicks time;
    int total_frames = 0;

    // Push data until we have a decoded output.
    while (total_frames < min_number_input_frames_needed_) {
      total_frames += ProduceAudioAndEncode(time);
      time += buffer_duration_;

      task_environment_.RunUntilIdle();
    }

    EXPECT_GE(total_frames, frames_per_buffer_);
    FlushAndVerifyStatus();

    opus_decoder_destroy(opus_decoder);
    opus_decoder = nullptr;
  }
}

// Tests we can configure the AudioOpusEncoder's extra options.
TEST_P(AudioOpusEncoderTest, FullCycleEncodeDecode_OpusOptions) {
  // TODO(crbug.com/40243924): Test an OpusOptions::frame_duration which forces
  // repacketization.
  constexpr media::AudioEncoder::OpusOptions kTestOpusOptions[] = {
      // Base case
      {.frame_duration = base::Milliseconds(20),
       .complexity = 10,
       .packet_loss_perc = 0,
       .use_in_band_fec = false,
       .use_dtx = false},

      // Use inband-FEC
      {.frame_duration = base::Microseconds(2500),
       .complexity = 0,
       .packet_loss_perc = 10,
       .use_in_band_fec = true,
       .use_dtx = false},

      // Use DTX
      {.frame_duration = base::Milliseconds(60),
       .complexity = 5,
       .packet_loss_perc = 0,
       .use_in_band_fec = false,
       .use_dtx = true},

      // Use inband-FEC and DTX
      {.frame_duration = base::Milliseconds(5),
       .complexity = 5,
       .packet_loss_perc = 20,
       .use_in_band_fec = true,
       .use_dtx = true},
  };

  for (const AudioEncoder::OpusOptions& opus_options : kTestOpusOptions) {
    const int kOpusDecoderSampleRate = 48000;

    // Override the work done in CreateEncoder().
    encoder_ = std::make_unique<AudioOpusEncoder>();
    options_.opus = opus_options;
    buffer_duration_ = opus_options.frame_duration;
    frames_per_buffer_ = AudioTimestampHelper::TimeToFrames(
        buffer_duration_, options_.sample_rate);

    int decoder_frames_per_buffer = AudioTimestampHelper::TimeToFrames(
        buffer_duration_, kOpusDecoderSampleRate);

    int error;
    OpusDecoder* opus_decoder =
        opus_decoder_create(kOpusDecoderSampleRate, options_.channels, &error);
    ASSERT_TRUE(error == OPUS_OK && opus_decoder);

    std::vector<float> buffer(decoder_frames_per_buffer * options_.channels);
    auto verify_opus_encoding = [&](EncodedAudioBuffer output, MaybeDesc) {
      // Use the libopus decoder to decode the |encoded_data| and check we
      // get the expected number of frames per buffer.
      EXPECT_EQ(decoder_frames_per_buffer,
                opus_decode_float(opus_decoder, output.encoded_data.data(),
                                  output.encoded_data.size(), buffer.data(),
                                  decoder_frames_per_buffer, 0));
    };

    InitializeEncoder(base::BindLambdaForTesting(verify_opus_encoding));

    base::TimeTicks time;
    int total_frames = 0;

    // Push data until we have a decoded output.
    while (total_frames < min_number_input_frames_needed_) {
      total_frames += ProduceAudioAndEncode(time);
      time += buffer_duration_;

      task_environment_.RunUntilIdle();
    }

    EXPECT_GE(total_frames, frames_per_buffer_);
    FlushAndVerifyStatus();

    opus_decoder_destroy(opus_decoder);
    opus_decoder = nullptr;
  }
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
                                          {}, audio_bus.get());
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

#if HAS_AAC_ENCODER
class AACAudioEncoderTest : public AudioEncodersTest {
 public:
  AACAudioEncoderTest() = default;
  AACAudioEncoderTest(const AACAudioEncoderTest&) = delete;
  AACAudioEncoderTest& operator=(const AACAudioEncoderTest&) = delete;
  ~AACAudioEncoderTest() override = default;

#if BUILDFLAG(ENABLE_FFMPEG) && BUILDFLAG(USE_PROPRIETARY_CODECS)
  void InitializeDecoder() {
    decoder_ = std::make_unique<FFmpegAudioDecoder>(
        base::SequencedTaskRunner::GetCurrentDefault(), &media_log);
    ChannelLayout channel_layout = CHANNEL_LAYOUT_NONE;
    switch (options_.channels) {
      case 1:
        channel_layout = CHANNEL_LAYOUT_MONO;
        break;
      case 2:
        channel_layout = CHANNEL_LAYOUT_STEREO;
        break;
      case 6:
        channel_layout = CHANNEL_LAYOUT_5_1_BACK;
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }
    AudioDecoderConfig config(AudioCodec::kAAC, SampleFormat::kSampleFormatS16,
                              channel_layout, options_.sample_rate,
                              /*extra_data=*/std::vector<uint8_t>(),
                              EncryptionScheme::kUnencrypted);
    auto init_cb = [](DecoderStatus decoder_status) {
      EXPECT_EQ(decoder_status, DecoderStatus::Codes::kOk);
    };
    auto output_cb = [&](scoped_refptr<AudioBuffer> decoded_buffer) {
      ++decoder_output_callback_count;
      EXPECT_EQ(decoded_buffer->frame_count(), frames_per_buffer_);
    };
    decoder_->Initialize(config, /*cdm_context=*/nullptr,
                         base::BindLambdaForTesting(init_cb),
                         base::BindLambdaForTesting(output_cb),
                         /*waiting_cb=*/base::DoNothing());
  }

 protected:
  MockMediaLog media_log;
  std::unique_ptr<FFmpegAudioDecoder> decoder_;
  int decoder_output_callback_count = 0;
#endif  // BUILDFLAG(ENABLE_FFMPEG) && BUILDFLAG(USE_PROPRIETARY_CODECS)
};

#if BUILDFLAG(IS_WIN)
// `MFAudioEncoder` requires `kMinSamplesForOutput` before `Flush` can be called
// successfully.
TEST_P(AACAudioEncoderTest, FlushWithTooLittleInput) {
  InitializeEncoder(base::DoNothing());
  ProduceAudioAndEncode();

  FlushAndVerifyStatus(EncoderStatus::Codes::kEncoderFailedFlush);

  ValidateDoneCallbacksRun();
}
#endif

#if BUILDFLAG(ENABLE_FFMPEG) && BUILDFLAG(USE_PROPRIETARY_CODECS)
TEST_P(AACAudioEncoderTest, FullCycleEncodeDecode) {
  InitializeDecoder();

  int encode_output_callback_count = 0;
  int decode_status_callback_count = 0;
  auto encode_output_cb = [&](EncodedAudioBuffer output, MaybeDesc) {
    ++encode_output_callback_count;

    auto decode_cb = [&](DecoderStatus status) {
      ++decode_status_callback_count;
      EXPECT_EQ(status, DecoderStatus::Codes::kOk);
    };
    scoped_refptr<DecoderBuffer> decoder_buffer =
        DecoderBuffer::FromArray(std::move(output.encoded_data));
    decoder_->Decode(decoder_buffer, base::BindLambdaForTesting(decode_cb));
  };

  InitializeEncoder(base::BindLambdaForTesting(encode_output_cb));

  ProduceAudioAndEncode();
  ProduceAudioAndEncode();
  ProduceAudioAndEncode();

  FlushAndVerifyStatus();

  // Let the decoder finish decoding.
  task_environment_.RunUntilIdle();

  int expected_outputs = 3 + std::ceil(GetExpectedPadding() /
                                       static_cast<double>(frames_per_buffer_));

  EXPECT_EQ(expected_outputs, encode_output_callback_count);
  EXPECT_EQ(expected_outputs, decode_status_callback_count);
  EXPECT_EQ(expected_outputs, decoder_output_callback_count);
}

TEST_P(AACAudioEncoderTest, FullCycleEncodeDecode_BitrateMode) {
  constexpr AudioEncoder::BitrateMode kTestAacBitrateMode[] = {
      AudioEncoder::BitrateMode::kConstant,
      AudioEncoder::BitrateMode::kVariable};

  for (const AudioEncoder::BitrateMode& bitrate_mode : kTestAacBitrateMode) {
    decoder_output_callback_count = 0;
    options_.bitrate_mode = bitrate_mode;

    // Recreate the encoder to pick up changes to `options_`.
    CreateEncoder();

    InitializeDecoder();

    auto encode_output_cb = [&](EncodedAudioBuffer output, MaybeDesc) {
      auto decode_cb = [&](DecoderStatus status) {
        EXPECT_EQ(status, DecoderStatus::Codes::kOk);
      };
      scoped_refptr<DecoderBuffer> decoder_buffer =
          DecoderBuffer::FromArray(std::move(output.encoded_data));
      decoder_->Decode(decoder_buffer, base::BindLambdaForTesting(decode_cb));
    };

    InitializeEncoder(base::BindLambdaForTesting(encode_output_cb));

    ProduceAudioAndEncode();
    ProduceAudioAndEncode();
    ProduceAudioAndEncode();

    FlushAndVerifyStatus();

    // Let the decoder finish decoding.
    task_environment_.RunUntilIdle();

    int expected_outputs =
        3 + std::ceil(GetExpectedPadding() /
                      static_cast<double>(frames_per_buffer_));

    EXPECT_EQ(expected_outputs, decoder_output_callback_count);
  }
}

// Makes sure we get extradata on the first output when we are using AAC output
// format, and no extradata when we are using ADTS output format.
TEST_P(AACAudioEncoderTest, AacOutputFormat) {
  constexpr AudioEncoder::AacOutputFormat kTestAacOutputFormat[] = {
      AudioEncoder::AacOutputFormat::AAC, AudioEncoder::AacOutputFormat::ADTS};

  for (const auto& output_format : kTestAacOutputFormat) {
    options_.aac = {output_format};

    // Recreate the encoder to pick up changes to `options_`.
    CreateEncoder();

    bool first_output = true;
    const bool needs_description =
        output_format == AudioEncoder::AacOutputFormat::AAC;

    auto encode_output_cb = [&](EncodedAudioBuffer output,
                                MaybeDesc codec_description) {
      if (first_output) {
        first_output = false;
        EXPECT_EQ(codec_description.has_value(), needs_description);
      } else {
        EXPECT_FALSE(codec_description);
      }
    };

    InitializeEncoder(base::BindLambdaForTesting(encode_output_cb));

    ProduceAudioAndEncode();
    ProduceAudioAndEncode();
    ProduceAudioAndEncode();

    FlushAndVerifyStatus();
  }
}
#endif  // BUILDFLAG(ENABLE_FFMPEG) && BUILDFLAG(USE_PROPRIETARY_CODECS)

INSTANTIATE_TEST_SUITE_P(AAC,
                         AACAudioEncoderTest,
                         testing::ValuesIn(kTestAudioParamsAAC));
#endif  // HAS_AAC_ENCODER

}  // namespace media
