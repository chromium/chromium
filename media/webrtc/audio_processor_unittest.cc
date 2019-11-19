// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/aligned_memory.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/webrtc/audio_processor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::Return;

namespace media {

namespace {

const int kAudioProcessingSampleRate = 48000;
const int kAudioProcessingNumberOfChannels = 1;

// The number of packers used for testing.
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

}  // namespace

class WebRtcAudioProcessorTest : public ::testing::Test {
 public:
  WebRtcAudioProcessorTest()
      : params_(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                media::CHANNEL_LAYOUT_STEREO,
                48000,
                480) {}

 protected:
  // Helper method to save duplicated code.
  void ProcessDataAndVerifyFormat(AudioProcessor* audio_processor,
                                  int expected_output_sample_rate,
                                  int expected_output_channels,
                                  int expected_output_buffer_size) {
    // Read the audio data from a file.
    const media::AudioParameters& params = audio_processor->audio_parameters_;
    const int packet_size = params.frames_per_buffer() * 2 * params.channels();
    const size_t length = packet_size * kNumberOfPacketsForTest;
    std::unique_ptr<char[]> capture_data(new char[length]);
    ReadDataFromSpeechFile(capture_data.get(), length);
    const int16_t* data_ptr =
        reinterpret_cast<const int16_t*>(capture_data.get());
    std::unique_ptr<media::AudioBus> data_bus =
        media::AudioBus::Create(params.channels(), params.frames_per_buffer());

    // |data_bus_playout| is used if the capture channels include a keyboard
    // channel. |data_bus_playout_to_use| points to the AudioBus to use, either
    // |data_bus| or |data_bus_playout|.
    std::unique_ptr<media::AudioBus> data_bus_playout;
    media::AudioBus* data_bus_playout_to_use = data_bus.get();
    media::AudioParameters playout_params = params;
    const bool has_keyboard_mic = params.channel_layout() ==
                                  media::CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC;
    if (has_keyboard_mic) {
      data_bus_playout = media::AudioBus::CreateWrapper(2);
      data_bus_playout->set_frames(params.frames_per_buffer());
      data_bus_playout_to_use = data_bus_playout.get();
      playout_params.Reset(params.format(), CHANNEL_LAYOUT_STEREO,
                           params.sample_rate(), params.frames_per_buffer());
    }

    const base::TimeDelta input_capture_delay =
        base::TimeDelta::FromMilliseconds(20);
    for (int i = 0; i < kNumberOfPacketsForTest; ++i) {
      data_bus->FromInterleaved<SignedInt16SampleTypeTraits>(
          data_ptr, data_bus->frames());
      // |audio_processor| does nothing when the audio processing is off in
      // the processor.
      webrtc::AudioProcessing* ap = audio_processor->audio_processing_.get();
      const bool is_aec_enabled = ap && ap->GetConfig().echo_canceller.enabled;
      if (is_aec_enabled) {
        if (has_keyboard_mic) {
          for (int i = 0; i < data_bus_playout->channels(); ++i) {
            data_bus_playout->SetChannelData(
                i, const_cast<float*>(data_bus->channel(i)));
          }
        }
        base::TimeTicks in_the_future =
            base::TimeTicks::Now() + base::TimeDelta::FromMilliseconds(10);
        audio_processor->AnalyzePlayout(*data_bus_playout_to_use,
                                        playout_params, in_the_future);
      }

      auto result = audio_processor->ProcessCapture(
          *data_bus, base::TimeTicks::Now() - input_capture_delay, 1.0, false);
      data_ptr += params.frames_per_buffer() * params.channels();
    }
  }

  void VerifyEnabledComponents(AudioProcessor* audio_processor) {
    webrtc::AudioProcessing* audio_processing =
        audio_processor->audio_processing_.get();
    webrtc::AudioProcessing::Config ap_config = audio_processing->GetConfig();
    EXPECT_TRUE(ap_config.echo_canceller.enabled);
    EXPECT_FALSE(ap_config.echo_canceller.mobile_mode);
    EXPECT_TRUE(ap_config.high_pass_filter.enabled);
    EXPECT_TRUE(ap_config.gain_controller1.enabled);
    EXPECT_EQ(ap_config.gain_controller1.mode,
              ap_config.gain_controller1.kAdaptiveAnalog);
    EXPECT_TRUE(ap_config.noise_suppression.enabled);
    EXPECT_EQ(ap_config.noise_suppression.level,
              ap_config.noise_suppression.kHigh);
    EXPECT_TRUE(ap_config.voice_detection.enabled);
  }

  AudioProcessingSettings GetEnabledAudioProcessingSettings() const {
    AudioProcessingSettings settings;
    settings.echo_cancellation = EchoCancellationType::kAec3;
    settings.noise_suppression = NoiseSuppressionType::kExperimental;
    settings.automatic_gain_control = AutomaticGainControlType::kExperimental;
    settings.high_pass_filter = true;
    settings.typing_detection = true;
    return settings;
  }

  base::test::TaskEnvironment task_environment_;
  media::AudioParameters params_;
};

TEST_F(WebRtcAudioProcessorTest, WithAudioProcessing) {
  AudioProcessor audio_processor(params_, GetEnabledAudioProcessingSettings());
  VerifyEnabledComponents(&audio_processor);
  ProcessDataAndVerifyFormat(&audio_processor, kAudioProcessingSampleRate,
                             kAudioProcessingNumberOfChannels,
                             kAudioProcessingSampleRate / 100);
}

TEST_F(WebRtcAudioProcessorTest, WithoutAnyProcessing) {
  // All processing settings are disabled by default
  AudioProcessingSettings settings;
  const media::AudioParameters source_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::CHANNEL_LAYOUT_STEREO, kAudioProcessingSampleRate,
      kAudioProcessingSampleRate / 100);
  AudioProcessor audio_processor(source_params, settings);

  ProcessDataAndVerifyFormat(&audio_processor, params_.sample_rate(),
                             params_.channels(), params_.sample_rate() / 100);
}

TEST_F(WebRtcAudioProcessorTest, TestAllSampleRates) {
  for (int sample_rate : {8000, 16000, 32000, 44100, 48000}) {
    int buffer_size = sample_rate / 100;
    media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                  media::CHANNEL_LAYOUT_STEREO, sample_rate,
                                  buffer_size);
    AudioProcessor audio_processor(params, GetEnabledAudioProcessingSettings());

    VerifyEnabledComponents(&audio_processor);

    ProcessDataAndVerifyFormat(&audio_processor, kAudioProcessingSampleRate,
                               kAudioProcessingNumberOfChannels,
                               kAudioProcessingSampleRate / 100);
  }
}

TEST_F(WebRtcAudioProcessorTest, TestStereoAudio) {
  // All processing settings are disabled by default
  AudioProcessingSettings settings;
  settings.stereo_mirroring = true;
  const media::AudioParameters source_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::CHANNEL_LAYOUT_STEREO, kAudioProcessingSampleRate,
      kAudioProcessingSampleRate / 100);
  AudioProcessor audio_processor(source_params, settings);

  // Construct left and right channels, and assign different values to the
  // first data of the left channel and right channel.
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
  float* left_channel_ptr = left_channel.get();
  left_channel_ptr[0] = 1.0f;

  // Run the test consecutively to make sure the stereo channels are not
  // flipped back and forth.
  static const int kNumberOfPacketsForTest = 100;
  const base::TimeDelta pushed_capture_delay =
      base::TimeDelta::FromMilliseconds(42);
  for (int i = 0; i < kNumberOfPacketsForTest; ++i) {
    auto result = audio_processor.ProcessCapture(
        *wrapper, base::TimeTicks::Now() + pushed_capture_delay, 1.0, false);

    EXPECT_EQ(result.audio.channel(0)[0], 0);
    EXPECT_NE(result.audio.channel(1)[0], 0);
  }
}

TEST_F(WebRtcAudioProcessorTest, TestWithKeyboardMicChannel) {
  AudioProcessingSettings settings = GetEnabledAudioProcessingSettings();
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC,
                                kAudioProcessingSampleRate,
                                kAudioProcessingSampleRate / 100);
  AudioProcessor audio_processor(params, settings);
  ProcessDataAndVerifyFormat(&audio_processor, kAudioProcessingSampleRate,
                             kAudioProcessingNumberOfChannels,
                             kAudioProcessingSampleRate / 100);
}

TEST_F(WebRtcAudioProcessorTest, StartStopAecDump) {
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  base::FilePath temp_file_path;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_directory.GetPath(),
                                             &temp_file_path));
  {
    AudioProcessor audio_processor(params_,
                                   GetEnabledAudioProcessingSettings());

    // Start and stop recording.
    audio_processor.StartEchoCancellationDump(base::File(
        temp_file_path, base::File::FLAG_WRITE | base::File::FLAG_OPEN));
    audio_processor.StopEchoCancellationDump();

    // Start and wait for d-tor.
    audio_processor.StartEchoCancellationDump(base::File(
        temp_file_path, base::File::FLAG_WRITE | base::File::FLAG_OPEN));
  }

  // Check that dump file is non-empty after audio processor has been
  // destroyed. Note that this test fails when compiling WebRTC
  // without protobuf support, rtc_enable_protobuf=false.
  std::string output;
  ASSERT_TRUE(base::ReadFileToString(temp_file_path, &output));
  ASSERT_FALSE(output.empty());
  // The temporary file is deleted when temp_directory exists scope.
}

}  // namespace media
