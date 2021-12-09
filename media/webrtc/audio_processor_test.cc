// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/audio_processor.h"

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/memory/aligned_memory.h"
#include "base/path_service.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_processing.h"
#include "media/webrtc/constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::Return;

namespace media {
namespace {

using MockProcessedCaptureCallback =
    base::MockRepeatingCallback<void(const media::AudioBus& audio_bus,
                                     base::TimeTicks audio_capture_time,
                                     absl::optional<double> new_volume)>;

AudioProcessor::LogCallback LogCallbackForTesting() {
  return base::BindRepeating(
      [](const std::string& message) { VLOG(1) << (message); });
}

// The number of packets used for testing.
const int kNumberOfPacketsForTest = 100;

void ReadDataFromSpeechFile(char* data, int length) {
  base::FilePath file;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &file));
  file = file.Append(FILE_PATH_LITERAL("media"))
             .Append(FILE_PATH_LITERAL("test"))
             .Append(FILE_PATH_LITERAL("data"))
             .Append(FILE_PATH_LITERAL("speech_16b_stereo_48kHz.raw"));
  DCHECK(base::PathExists(file));
  int64_t data_file_size64 = 0;
  DCHECK(base::GetFileSize(file, &data_file_size64));
  EXPECT_EQ(length, base::ReadFile(file, data, length));
  DCHECK(data_file_size64 > length);
}

void DisableDefaultSettings(AudioProcessingSettings& settings) {
  settings.echo_cancellation = false;
  settings.noise_suppression = false;
  settings.transient_noise_suppression = false;
  settings.automatic_gain_control = false;
  settings.experimental_automatic_gain_control = false;
  settings.high_pass_filter = false;
  settings.multi_channel_capture_processing = false;
  settings.stereo_mirroring = false;
  settings.force_apm_creation = false;
}

}  // namespace

class AudioProcessorTest : public ::testing::Test {
 public:
  AudioProcessorTest()
      : params_(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                media::CHANNEL_LAYOUT_STEREO,
                48000,
                480) {}

 protected:
  // Helper method to save duplicated code.
  static void ProcessDataAndVerifyFormat(
      AudioProcessor& audio_processor,
      MockProcessedCaptureCallback& mock_capture_callback,
      int expected_output_sample_rate,
      int expected_output_channels,
      int expected_output_buffer_size) {
    // Read the audio data from a file.
    const media::AudioParameters& params =
        audio_processor.GetInputFormatForTesting();
    const int packet_size = params.frames_per_buffer() * 2 * params.channels();
    const size_t length = packet_size * kNumberOfPacketsForTest;
    std::unique_ptr<char[]> capture_data(new char[length]);
    ReadDataFromSpeechFile(capture_data.get(), static_cast<int>(length));
    const int16_t* data_ptr =
        reinterpret_cast<const int16_t*>(capture_data.get());
    std::unique_ptr<media::AudioBus> data_bus =
        media::AudioBus::Create(params.channels(), params.frames_per_buffer());

    // |data_bus_playout| is used if the capture channels include a keyboard
    // channel. |data_bus_playout_to_use| points to the AudioBus to use, either
    // |data_bus| or |data_bus_playout|.
    std::unique_ptr<media::AudioBus> data_bus_playout;
    media::AudioBus* data_bus_playout_to_use = data_bus.get();
    const bool has_keyboard_mic = params.channel_layout() ==
                                  media::CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC;
    if (has_keyboard_mic) {
      data_bus_playout = media::AudioBus::CreateWrapper(2);
      data_bus_playout->set_frames(params.frames_per_buffer());
      data_bus_playout_to_use = data_bus_playout.get();
    }

    const base::TimeTicks input_capture_time = base::TimeTicks::Now();
    int num_preferred_channels = -1;
    for (int i = 0; i < kNumberOfPacketsForTest; ++i) {
      data_bus->FromInterleaved<media::SignedInt16SampleTypeTraits>(
          data_ptr, data_bus->frames());

      // 1. Provide playout audio, if echo cancellation is enabled.
      const bool is_aec_enabled =
          audio_processor.has_webrtc_audio_processing() &&
          (*audio_processor.GetAudioProcessingModuleConfigForTesting())
              .echo_canceller.enabled;
      if (is_aec_enabled) {
        if (has_keyboard_mic) {
          for (int channel = 0; channel < data_bus_playout->channels();
               ++channel) {
            data_bus_playout->SetChannelData(
                channel, const_cast<float*>(data_bus->channel(channel)));
          }
        }
        audio_processor.OnPlayoutData(data_bus_playout_to_use,
                                      params.sample_rate(),
                                      base::Milliseconds(10));
      }

      // 2. Set up expectations and process captured audio.
      EXPECT_CALL(mock_capture_callback, Run(_, _, _))
          .WillRepeatedly([&](const media::AudioBus& processed_audio,
                              base::TimeTicks audio_capture_time,
                              absl::optional<double> new_volume) {
            EXPECT_EQ(audio_capture_time, input_capture_time);
          });
      audio_processor.ProcessCapturedAudio(*data_bus, input_capture_time,
                                           num_preferred_channels, 1.0, false);
      EXPECT_EQ(expected_output_sample_rate,
                audio_processor.OutputFormat().sample_rate());
      EXPECT_EQ(expected_output_channels,
                audio_processor.OutputFormat().channels());
      EXPECT_EQ(expected_output_buffer_size,
                audio_processor.OutputFormat().frames_per_buffer());

      data_ptr += params.frames_per_buffer() * params.channels();

      // Test different values of num_preferred_channels.
      if (++num_preferred_channels > 5) {
        num_preferred_channels = 0;
      }
    }
  }

  void VerifyDefaultComponents(AudioProcessor& audio_processor) {
    EXPECT_TRUE(audio_processor.has_webrtc_audio_processing());
    const webrtc::AudioProcessing::Config config =
        *audio_processor.GetAudioProcessingModuleConfigForTesting();
    EXPECT_TRUE(config.echo_canceller.enabled);
    EXPECT_TRUE(config.gain_controller1.enabled);
    EXPECT_TRUE(config.high_pass_filter.enabled);
    EXPECT_TRUE(config.noise_suppression.enabled);
    EXPECT_EQ(config.noise_suppression.level, config.noise_suppression.kHigh);
    EXPECT_FALSE(config.voice_detection.enabled);
    EXPECT_FALSE(config.gain_controller1.analog_gain_controller
                     .clipping_predictor.enabled);
#if defined(OS_ANDROID)
    EXPECT_TRUE(config.echo_canceller.mobile_mode);
    EXPECT_EQ(config.gain_controller1.mode,
              config.gain_controller1.kFixedDigital);
#else
    EXPECT_FALSE(config.echo_canceller.mobile_mode);
    EXPECT_EQ(config.gain_controller1.mode,
              config.gain_controller1.kAdaptiveAnalog);
#endif
  }

  media::AudioParameters params_;
  MockProcessedCaptureCallback mock_capture_callback_;
  // Necessary for working with WebRTC task queues.
  base::test::TaskEnvironment task_environment_;
};

class AudioProcessorTestMultichannel
    : public AudioProcessorTest,
      public ::testing::WithParamInterface<bool> {};

// Test crashing with ASAN on Android. crbug.com/468762
#if defined(OS_ANDROID) && defined(ADDRESS_SANITIZER)
#define MAYBE_WithAudioProcessing DISABLED_WithAudioProcessing
#else
#define MAYBE_WithAudioProcessing WithAudioProcessing
#endif
TEST_P(AudioProcessorTestMultichannel, MAYBE_WithAudioProcessing) {
  const bool use_multichannel_processing = GetParam();
  AudioProcessingSettings settings{.multi_channel_capture_processing =
                                       use_multichannel_processing};
  AudioProcessor audio_processor(mock_capture_callback_.Get(),
                                 LogCallbackForTesting(), settings);
  EXPECT_TRUE(audio_processor.has_webrtc_audio_processing());
  audio_processor.OnCaptureFormatChanged(params_);
  VerifyDefaultComponents(audio_processor);

  const int expected_output_channels =
      use_multichannel_processing ? params_.channels() : 1;
  ProcessDataAndVerifyFormat(audio_processor, mock_capture_callback_,
                             media::kAudioProcessingSampleRateHz,
                             expected_output_channels,
                             media::kAudioProcessingSampleRateHz / 100);
}

TEST_F(AudioProcessorTest, TurnOffDefaultConstraints) {
  AudioProcessingSettings settings;
  // Turn off the default settings and pass it to AudioProcessor.
  DisableDefaultSettings(settings);
  AudioProcessor audio_processor(mock_capture_callback_.Get(),
                                 LogCallbackForTesting(), settings);
  EXPECT_FALSE(audio_processor.has_webrtc_audio_processing());
  audio_processor.OnCaptureFormatChanged(params_);

  ProcessDataAndVerifyFormat(audio_processor, mock_capture_callback_,
                             params_.sample_rate(), params_.channels(),
                             params_.sample_rate() / 100);
}

// Test crashing with ASAN on Android. crbug.com/468762
#if defined(OS_ANDROID) && defined(ADDRESS_SANITIZER)
#define MAYBE_TestAllSampleRates DISABLED_TestAllSampleRates
#else
#define MAYBE_TestAllSampleRates TestAllSampleRates
#endif
TEST_P(AudioProcessorTestMultichannel, MAYBE_TestAllSampleRates) {
  const bool use_multichannel_processing = GetParam();
  AudioProcessingSettings settings{.multi_channel_capture_processing =
                                       use_multichannel_processing};
  AudioProcessor audio_processor(mock_capture_callback_.Get(),
                                 LogCallbackForTesting(), settings);
  EXPECT_TRUE(audio_processor.has_webrtc_audio_processing());

  static const int kSupportedSampleRates[] = {
    8000,
    16000,
    22050,
    32000,
    44100,
    48000
#if BUILDFLAG(IS_CHROMECAST)
    ,
    96000
#endif  // BUILDFLAG(IS_CHROMECAST)
  };
  for (int sample_rate : kSupportedSampleRates) {
    SCOPED_TRACE(testing::Message() << "sample_rate=" << sample_rate);
    int buffer_size = sample_rate / 100;
    media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                  media::CHANNEL_LAYOUT_STEREO, sample_rate,
                                  buffer_size);
    audio_processor.OnCaptureFormatChanged(params);
    VerifyDefaultComponents(audio_processor);

    int expected_sample_rate =
#if BUILDFLAG(IS_CHROMECAST)
        std::min(sample_rate, media::kAudioProcessingSampleRateHz);
#else
        media::kAudioProcessingSampleRateHz;
#endif  // BUILDFLAG(IS_CHROMECAST)
    const int expected_output_channels =
        use_multichannel_processing ? params_.channels() : 1;
    ProcessDataAndVerifyFormat(audio_processor, mock_capture_callback_,
                               expected_sample_rate, expected_output_channels,
                               expected_sample_rate / 100);
  }
}

TEST_F(AudioProcessorTest, StartStopAecDump) {
  AudioProcessingSettings settings;

  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  base::FilePath temp_file_path;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_directory.GetPath(),
                                             &temp_file_path));
  {
    AudioProcessor audio_processor(mock_capture_callback_.Get(),
                                   LogCallbackForTesting(), settings);

    // Start and stop recording.
    audio_processor.OnStartDump(base::File(
        temp_file_path, base::File::FLAG_WRITE | base::File::FLAG_OPEN));
    audio_processor.OnStopDump();

    // Start and stop a second recording.
    audio_processor.OnStartDump(base::File(
        temp_file_path, base::File::FLAG_WRITE | base::File::FLAG_OPEN));
    audio_processor.OnStopDump();
  }

  // Check that dump file is non-empty after audio processor has been
  // destroyed. Note that this test fails when compiling WebRTC
  // without protobuf support, rtc_enable_protobuf=false.
  std::string output;
  ASSERT_TRUE(base::ReadFileToString(temp_file_path, &output));
  EXPECT_FALSE(output.empty());
  // The temporary file is deleted when temp_directory exits scope.
}

TEST_F(AudioProcessorTest, StartAecDumpDuringOngoingAecDump) {
  AudioProcessingSettings settings;

  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  base::FilePath temp_file_path_a;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_directory.GetPath(),
                                             &temp_file_path_a));
  base::FilePath temp_file_path_b;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_directory.GetPath(),
                                             &temp_file_path_b));
  {
    AudioProcessor audio_processor(mock_capture_callback_.Get(),
                                   LogCallbackForTesting(), settings);

    // Start a recording.
    audio_processor.OnStartDump(base::File(
        temp_file_path_a, base::File::FLAG_WRITE | base::File::FLAG_OPEN));

    // Start another recording without stopping the previous one.
    audio_processor.OnStartDump(base::File(
        temp_file_path_b, base::File::FLAG_WRITE | base::File::FLAG_OPEN));
    audio_processor.OnStopDump();
  }

  // Check that dump files are non-empty after audio processor has been
  // destroyed. Note that this test fails when compiling WebRTC
  // without protobuf support, rtc_enable_protobuf=false.
  std::string output;
  ASSERT_TRUE(base::ReadFileToString(temp_file_path_a, &output));
  EXPECT_FALSE(output.empty());
  ASSERT_TRUE(base::ReadFileToString(temp_file_path_b, &output));
  EXPECT_FALSE(output.empty());
  // The temporary files are deleted when temp_directory exits scope.
}

TEST_P(AudioProcessorTestMultichannel, TestStereoAudio) {
  const bool use_multichannel_processing = GetParam();
  SCOPED_TRACE(testing::Message() << "use_multichannel_processing="
                                  << use_multichannel_processing);
  const media::AudioParameters source_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::CHANNEL_LAYOUT_STEREO, 48000, 480);

  // Construct left and right channels, and populate each channel with
  // different values.
  const int size = media::AudioBus::CalculateMemorySize(source_params);
  std::unique_ptr<float, base::AlignedFreeDeleter> left_channel(
      static_cast<float*>(base::AlignedAlloc(size, 32)));
  std::unique_ptr<float, base::AlignedFreeDeleter> right_channel(
      static_cast<float*>(base::AlignedAlloc(size, 32)));
  std::unique_ptr<media::AudioBus> wrapper =
      media::AudioBus::CreateWrapper(source_params.channels());
  wrapper->set_frames(source_params.frames_per_buffer());
  wrapper->SetChannelData(0, left_channel.get());
  wrapper->SetChannelData(1, right_channel.get());
  wrapper->Zero();
  wrapper->channel(0)[0] = 1.0f;

  // Test without and with audio processing enabled.
  for (bool use_apm : {false, true}) {
    // No need to test stereo with APM if disabled.
    if (use_apm && !use_multichannel_processing) {
      continue;
    }
    SCOPED_TRACE(testing::Message() << "use_apm=" << use_apm);

    AudioProcessingSettings settings{.multi_channel_capture_processing =
                                         use_multichannel_processing};
    if (!use_apm) {
      // Turn off the audio processing.
      DisableDefaultSettings(settings);
    }
    // Turn on the stereo channels mirroring.
    settings.stereo_mirroring = true;
    AudioProcessor audio_processor(mock_capture_callback_.Get(),
                                   LogCallbackForTesting(), settings);
    EXPECT_EQ(audio_processor.has_webrtc_audio_processing(), use_apm);
    audio_processor.OnCaptureFormatChanged(source_params);
    // There's no sense in continuing if this fails.
    ASSERT_EQ(2, audio_processor.OutputFormat().channels());

    // Run the test consecutively to make sure the stereo channels are not
    // flipped back and forth.
    static const int kNumberOfPacketsForTest = 100;
    const base::TimeTicks pushed_capture_time = base::TimeTicks::Now();

    for (int num_preferred_channels = 0; num_preferred_channels <= 5;
         ++num_preferred_channels) {
      SCOPED_TRACE(testing::Message()
                   << "num_preferred_channels=" << num_preferred_channels);
      for (int i = 0; i < kNumberOfPacketsForTest; ++i) {
        SCOPED_TRACE(testing::Message() << "packet index i=" << i);
        EXPECT_CALL(mock_capture_callback_, Run(_, _, _)).Times(1);
        // Pass audio for processing.
        audio_processor.ProcessCapturedAudio(
            *wrapper, pushed_capture_time, num_preferred_channels, 0.0, false);
      }
      // At this point, the audio processing algorithms have gotten past any
      // initial buffer silence generated from resamplers, FFTs, and whatnot.
      // Set up expectations via the mock callback:
      EXPECT_CALL(mock_capture_callback_, Run(_, _, _))
          .WillRepeatedly([&](const media::AudioBus& processed_audio,
                              base::TimeTicks audio_capture_time,
                              absl::optional<double> new_volume) {
            EXPECT_EQ(audio_capture_time, pushed_capture_time);
            if (!use_apm) {
              EXPECT_FALSE(new_volume.has_value());
            }
            if (use_apm && num_preferred_channels <= 1) {
              // Mono output. Output channels are averaged.
              EXPECT_NE(processed_audio.channel(0)[0], 0);
              EXPECT_NE(processed_audio.channel(1)[0], 0);
            } else {
              // Stereo output. Output channels are independent.
              // Note that after stereo mirroring, the _right_ channel is
              // non-zero.
              EXPECT_EQ(processed_audio.channel(0)[0], 0);
              EXPECT_NE(processed_audio.channel(1)[0], 0);
            }
          });
      // Process one more frame of audio.
      audio_processor.ProcessCapturedAudio(*wrapper, pushed_capture_time,
                                           num_preferred_channels, 0.0, false);
    }
  }
}

// Disabled on android clang builds due to crbug.com/470499
#if defined(__clang__) && defined(OS_ANDROID)
#define MAYBE_TestWithKeyboardMicChannel DISABLED_TestWithKeyboardMicChannel
#else
#define MAYBE_TestWithKeyboardMicChannel TestWithKeyboardMicChannel
#endif
TEST_P(AudioProcessorTestMultichannel, MAYBE_TestWithKeyboardMicChannel) {
  const bool use_multichannel_processing = GetParam();
  AudioProcessingSettings settings{.multi_channel_capture_processing =
                                       use_multichannel_processing};
  AudioProcessor audio_processor(mock_capture_callback_.Get(),
                                 LogCallbackForTesting(), settings);
  EXPECT_TRUE(audio_processor.has_webrtc_audio_processing());

  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC,
                                48000, 480);
  audio_processor.OnCaptureFormatChanged(params);

  const int expected_output_channels =
      use_multichannel_processing ? params_.channels() : 1;

  ProcessDataAndVerifyFormat(audio_processor, mock_capture_callback_,
                             media::kAudioProcessingSampleRateHz,
                             expected_output_channels,
                             media::kAudioProcessingSampleRateHz / 100);
}

// Ensure that discrete channel layouts do not crash with audio processing
// enabled.
TEST_F(AudioProcessorTest, DiscreteChannelLayout) {
  AudioProcessingSettings settings;
  AudioProcessor audio_processor(mock_capture_callback_.Get(),
                                 LogCallbackForTesting(), settings);
  EXPECT_TRUE(audio_processor.has_webrtc_audio_processing());

  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::CHANNEL_LAYOUT_DISCRETE, 48000, 480);
  // Test both 1 and 2 discrete channels.
  for (int channels = 1; channels <= 2; ++channels) {
    params.set_channels_for_discrete(channels);
    audio_processor.OnCaptureFormatChanged(params);
  }
}

INSTANTIATE_TEST_CASE_P(AudioProcessorMultichannelAffectedTests,
                        AudioProcessorTestMultichannel,
                        ::testing::Values(false, true));

// When audio processing is performed, processed audio should be delivered as
// soon as 10 ms of audio has been received.
TEST(AudioProcessorCallbackTest,
     ProcessedAudioIsDeliveredAsSoonAsPossibleWithShortBuffers) {
  MockProcessedCaptureCallback mock_capture_callback;
  AudioProcessingSettings settings;
  AudioProcessor audio_processor(mock_capture_callback.Get(),
                                 LogCallbackForTesting(), settings);
  ASSERT_TRUE(audio_processor.has_webrtc_audio_processing());

  // Set buffer size to 4 ms.
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::CHANNEL_LAYOUT_STEREO, 48000,
                                48000 * 4 / 1000);
  audio_processor.OnCaptureFormatChanged(params);
  int output_sample_rate = audio_processor.OutputFormat().sample_rate();
  std::unique_ptr<media::AudioBus> data_bus =
      media::AudioBus::Create(params.channels(), params.frames_per_buffer());
  data_bus->Zero();

  auto check_audio_length = [&](const media::AudioBus& processed_audio,
                                base::TimeTicks, absl::optional<double>) {
    EXPECT_EQ(processed_audio.frames(), output_sample_rate * 10 / 1000);
  };

  // 4 ms of data: Not enough to process.
  EXPECT_CALL(mock_capture_callback, Run(_, _, _)).Times(0);
  audio_processor.ProcessCapturedAudio(*data_bus, base::TimeTicks::Now(), -1,
                                       1.0, false);
  // 8 ms of data: Not enough to process.
  EXPECT_CALL(mock_capture_callback, Run(_, _, _)).Times(0);
  audio_processor.ProcessCapturedAudio(*data_bus, base::TimeTicks::Now(), -1,
                                       1.0, false);
  // 12 ms of data: Should trigger callback, with 2 ms left in the processor.
  EXPECT_CALL(mock_capture_callback, Run(_, _, _))
      .Times(1)
      .WillOnce(check_audio_length);
  audio_processor.ProcessCapturedAudio(*data_bus, base::TimeTicks::Now(), -1,
                                       1.0, false);
  // 2 + 4 ms of data: Not enough to process.
  EXPECT_CALL(mock_capture_callback, Run(_, _, _)).Times(0);
  audio_processor.ProcessCapturedAudio(*data_bus, base::TimeTicks::Now(), -1,
                                       1.0, false);
  // 10 ms of data: Should trigger callback.
  EXPECT_CALL(mock_capture_callback, Run(_, _, _))
      .Times(1)
      .WillOnce(check_audio_length);
  audio_processor.ProcessCapturedAudio(*data_bus, base::TimeTicks::Now(), -1,
                                       1.0, false);
}

// When audio processing is performed, input containing 10 ms several times over
// should trigger a comparable number of processing callbacks.
TEST(AudioProcessorCallbackTest,
     ProcessedAudioIsDeliveredAsSoonAsPossibleWithLongBuffers) {
  MockProcessedCaptureCallback mock_capture_callback;
  AudioProcessingSettings settings;
  AudioProcessor audio_processor(mock_capture_callback.Get(),
                                 LogCallbackForTesting(), settings);
  ASSERT_TRUE(audio_processor.has_webrtc_audio_processing());

  // Set buffer size to 35 ms.
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::CHANNEL_LAYOUT_STEREO, 48000,
                                48000 * 35 / 1000);
  audio_processor.OnCaptureFormatChanged(params);
  int output_sample_rate = audio_processor.OutputFormat().sample_rate();
  std::unique_ptr<media::AudioBus> data_bus =
      media::AudioBus::Create(params.channels(), params.frames_per_buffer());
  data_bus->Zero();

  auto check_audio_length = [&](const media::AudioBus& processed_audio,
                                base::TimeTicks, absl::optional<double>) {
    EXPECT_EQ(processed_audio.frames(), output_sample_rate * 10 / 1000);
  };

  // 35 ms of audio --> 3 chunks of 10 ms, and 5 ms left in the processor.
  EXPECT_CALL(mock_capture_callback, Run(_, _, _))
      .Times(3)
      .WillRepeatedly(check_audio_length);
  audio_processor.ProcessCapturedAudio(*data_bus, base::TimeTicks::Now(), -1,
                                       1.0, false);
  // 5 + 35 ms of audio --> 4 chunks of 10 ms.
  EXPECT_CALL(mock_capture_callback, Run(_, _, _))
      .Times(4)
      .WillRepeatedly(check_audio_length);
  audio_processor.ProcessCapturedAudio(*data_bus, base::TimeTicks::Now(), -1,
                                       1.0, false);
}

// When no audio processing is performed, audio is delivered immediately. Note
// that unlike the other cases, unprocessed audio input of less than 10 ms is
// forwarded directly instead of collecting chunks of 10 ms.
TEST(AudioProcessorCallbackTest,
     UnprocessedAudioIsDeliveredImmediatelyWithShortBuffers) {
  MockProcessedCaptureCallback mock_capture_callback;
  AudioProcessingSettings settings;
  DisableDefaultSettings(settings);
  AudioProcessor audio_processor(mock_capture_callback.Get(),
                                 LogCallbackForTesting(), settings);
  ASSERT_FALSE(audio_processor.has_webrtc_audio_processing());

  // Set buffer size to 4 ms.
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::CHANNEL_LAYOUT_STEREO, 48000,
                                48000 * 4 / 1000);
  audio_processor.OnCaptureFormatChanged(params);
  int output_sample_rate = audio_processor.OutputFormat().sample_rate();
  std::unique_ptr<media::AudioBus> data_bus =
      media::AudioBus::Create(params.channels(), params.frames_per_buffer());
  data_bus->Zero();

  auto check_audio_length = [&](const media::AudioBus& processed_audio,
                                base::TimeTicks, absl::optional<double>) {
    EXPECT_EQ(processed_audio.frames(), output_sample_rate * 4 / 1000);
  };

  EXPECT_CALL(mock_capture_callback, Run(_, _, _))
      .Times(1)
      .WillOnce(check_audio_length);
  audio_processor.ProcessCapturedAudio(*data_bus, base::TimeTicks::Now(), -1,
                                       1.0, false);
  EXPECT_CALL(mock_capture_callback, Run(_, _, _))
      .Times(1)
      .WillOnce(check_audio_length);
  audio_processor.ProcessCapturedAudio(*data_bus, base::TimeTicks::Now(), -1,
                                       1.0, false);
}

// When no audio processing is performed, audio is delivered immediately. Chunks
// greater than 10 ms are delivered in chunks of 10 ms.
TEST(AudioProcessorCallbackTest,
     UnprocessedAudioIsDeliveredImmediatelyWithLongBuffers) {
  MockProcessedCaptureCallback mock_capture_callback;
  AudioProcessingSettings settings;
  DisableDefaultSettings(settings);
  AudioProcessor audio_processor(mock_capture_callback.Get(),
                                 LogCallbackForTesting(), settings);
  ASSERT_FALSE(audio_processor.has_webrtc_audio_processing());

  // Set buffer size to 35 ms.
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::CHANNEL_LAYOUT_STEREO, 48000,
                                48000 * 35 / 1000);
  audio_processor.OnCaptureFormatChanged(params);
  int output_sample_rate = audio_processor.OutputFormat().sample_rate();
  std::unique_ptr<media::AudioBus> data_bus =
      media::AudioBus::Create(params.channels(), params.frames_per_buffer());
  data_bus->Zero();

  auto check_audio_length = [&](const media::AudioBus& processed_audio,
                                base::TimeTicks, absl::optional<double>) {
    EXPECT_EQ(processed_audio.frames(), output_sample_rate * 10 / 1000);
  };

  // 35 ms of audio --> 3 chunks of 10 ms, and 5 ms left in the processor.
  EXPECT_CALL(mock_capture_callback, Run(_, _, _))
      .Times(3)
      .WillRepeatedly(check_audio_length);
  audio_processor.ProcessCapturedAudio(*data_bus, base::TimeTicks::Now(), -1,
                                       1.0, false);
  // 5 + 35 ms of audio --> 4 chunks of 10 ms.
  EXPECT_CALL(mock_capture_callback, Run(_, _, _))
      .Times(4)
      .WillRepeatedly(check_audio_length);
  audio_processor.ProcessCapturedAudio(*data_bus, base::TimeTicks::Now(), -1,
                                       1.0, false);
}

TEST(AudioProcessorRequiresPlayoutReferenceTest,
     TrueWhenEchoCancellationIsEnabled) {
  MockProcessedCaptureCallback mock_capture_callback;
  AudioProcessingSettings settings;
  DisableDefaultSettings(settings);
  settings.echo_cancellation = true;
  AudioProcessor audio_processor(mock_capture_callback.Get(),
                                 LogCallbackForTesting(), settings);
  EXPECT_TRUE(audio_processor.RequiresPlayoutReference());
}

TEST(AudioProcessorRequiresPlayoutReferenceTest, TrueWhenGainControlIsEnabled) {
  MockProcessedCaptureCallback mock_capture_callback;
  AudioProcessingSettings settings;
  DisableDefaultSettings(settings);
  settings.automatic_gain_control = true;
  AudioProcessor audio_processor(mock_capture_callback.Get(),
                                 LogCallbackForTesting(), settings);
  EXPECT_TRUE(audio_processor.RequiresPlayoutReference());
}

TEST(AudioProcessorRequiresPlayoutReferenceTest, FalseWhenAecAndAgcIsDisabled) {
  MockProcessedCaptureCallback mock_capture_callback;
  AudioProcessingSettings settings;
  // Disable effects that need the playout signal.
  settings.echo_cancellation = false;
  settings.automatic_gain_control = false;
  // Enable all other effects.
  settings.experimental_automatic_gain_control = true;
  settings.noise_suppression = true;
  settings.transient_noise_suppression = true;
  settings.high_pass_filter = true;
  settings.stereo_mirroring = true;
  settings.force_apm_creation = true;
  AudioProcessor audio_processor(mock_capture_callback.Get(),
                                 LogCallbackForTesting(), settings);
  EXPECT_FALSE(audio_processor.RequiresPlayoutReference());
}

}  // namespace media
