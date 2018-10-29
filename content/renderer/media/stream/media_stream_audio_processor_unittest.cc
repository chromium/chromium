// Copyright 2013 The Chromium Authors. All rights reserved.
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
#include "base/message_loop/message_loop_current.h"
#include "base/path_service.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/public/common/media_stream_request.h"
#include "content/renderer/media/stream/media_stream_audio_processor.h"
#include "content/renderer/media/stream/media_stream_audio_processor_options.h"
#include "content/renderer/media/stream/mock_constraint_factory.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_media_constraints.h"
#include "third_party/webrtc/api/mediastreaminterface.h"
#include "third_party/webrtc/rtc_base/refcountedobject.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::Return;

using media::AudioParameters;

namespace content {

namespace {

const int kAudioProcessingNumberOfChannel = 1;

// The number of packers used for testing.
const int kNumberOfPacketsForTest = 100;

const int kMaxNumberOfPlayoutDataChannels = 2;

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

class MediaStreamAudioProcessorTest : public ::testing::Test {
 public:
  MediaStreamAudioProcessorTest()
      : params_(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                media::CHANNEL_LAYOUT_STEREO,
                48000,
                480) {}

 protected:
  // Helper method to save duplicated code.
  void ProcessDataAndVerifyFormat(MediaStreamAudioProcessor* audio_processor,
                                  int expected_output_sample_rate,
                                  int expected_output_channels,
                                  int expected_output_buffer_size) {
    // Read the audio data from a file.
    const media::AudioParameters& params = audio_processor->InputFormat();
    const int packet_size =
        params.frames_per_buffer() * 2 * params.channels();
    const size_t length = packet_size * kNumberOfPacketsForTest;
    std::unique_ptr<char[]> capture_data(new char[length]);
    ReadDataFromSpeechFile(capture_data.get(), length);
    const int16_t* data_ptr =
        reinterpret_cast<const int16_t*>(capture_data.get());
    std::unique_ptr<media::AudioBus> data_bus =
        media::AudioBus::Create(params.channels(), params.frames_per_buffer());

    // |data_bus_playout| is used if the number of capture channels is larger
    // that max allowed playout channels. |data_bus_playout_to_use| points to
    // the AudioBus to use, either |data_bus| or |data_bus_playout|.
    std::unique_ptr<media::AudioBus> data_bus_playout;
    media::AudioBus* data_bus_playout_to_use = data_bus.get();
    if (params.channels() > kMaxNumberOfPlayoutDataChannels) {
      data_bus_playout =
          media::AudioBus::CreateWrapper(kMaxNumberOfPlayoutDataChannels);
      data_bus_playout->set_frames(params.frames_per_buffer());
      data_bus_playout_to_use = data_bus_playout.get();
    }

    const base::TimeDelta input_capture_delay =
        base::TimeDelta::FromMilliseconds(20);
    const base::TimeDelta output_buffer_duration =
        expected_output_buffer_size * base::TimeDelta::FromSeconds(1) /
            expected_output_sample_rate;
    for (int i = 0; i < kNumberOfPacketsForTest; ++i) {
      data_bus->FromInterleaved(data_ptr, data_bus->frames(), 2);
      audio_processor->PushCaptureData(*data_bus, input_capture_delay);

      // |audio_processor| does nothing when the audio processing is off in
      // the processor.
      webrtc::AudioProcessing* ap = audio_processor->audio_processing_.get();
      const bool is_aec_enabled = ap && ap->GetConfig().echo_canceller.enabled;
      if (is_aec_enabled) {
        if (params.channels() > kMaxNumberOfPlayoutDataChannels) {
          for (int i = 0; i < kMaxNumberOfPlayoutDataChannels; ++i) {
            data_bus_playout->SetChannelData(
                i, const_cast<float*>(data_bus->channel(i)));
          }
        }
        audio_processor->OnPlayoutData(data_bus_playout_to_use,
                                       params.sample_rate(), 10);
      }

      media::AudioBus* processed_data = nullptr;
      base::TimeDelta capture_delay;
      int new_volume = 0;
      while (audio_processor->ProcessAndConsumeData(
                 255, false, &processed_data, &capture_delay, &new_volume)) {
        EXPECT_TRUE(processed_data);
        EXPECT_NEAR(input_capture_delay.InMillisecondsF(),
                    capture_delay.InMillisecondsF(),
                    output_buffer_duration.InMillisecondsF());
        EXPECT_EQ(expected_output_sample_rate,
                  audio_processor->OutputFormat().sample_rate());
        EXPECT_EQ(expected_output_channels,
                  audio_processor->OutputFormat().channels());
        EXPECT_EQ(expected_output_buffer_size,
                  audio_processor->OutputFormat().frames_per_buffer());
      }

      data_ptr += params.frames_per_buffer() * params.channels();
    }
  }

  void VerifyDefaultComponents(MediaStreamAudioProcessor* audio_processor) {
    webrtc::AudioProcessing* audio_processing =
        audio_processor->audio_processing_.get();
    const webrtc::AudioProcessing::Config config =
        audio_processing->GetConfig();
    EXPECT_TRUE(config.echo_canceller.enabled);
#if defined(OS_ANDROID)
    EXPECT_TRUE(config.echo_canceller.mobile_mode);
#else
    EXPECT_FALSE(config.echo_canceller.mobile_mode);
#endif
    EXPECT_TRUE(config.high_pass_filter.enabled);

    EXPECT_TRUE(audio_processing->noise_suppression()->is_enabled());
    EXPECT_TRUE(audio_processing->noise_suppression()->level() ==
        webrtc::NoiseSuppression::kHigh);
    EXPECT_TRUE(audio_processing->gain_control()->is_enabled());
#if defined(OS_ANDROID)
    EXPECT_TRUE(audio_processing->gain_control()->mode() ==
        webrtc::GainControl::kFixedDigital);
    EXPECT_FALSE(audio_processing->voice_detection()->is_enabled());
#else
    EXPECT_TRUE(audio_processing->gain_control()->mode() ==
        webrtc::GainControl::kAdaptiveAnalog);
    EXPECT_TRUE(audio_processing->voice_detection()->is_enabled());
    EXPECT_TRUE(audio_processing->voice_detection()->likelihood() ==
        webrtc::VoiceDetection::kVeryLowLikelihood);
#endif
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  media::AudioParameters params_;
};

// Test crashing with ASAN on Android. crbug.com/468762
#if defined(OS_ANDROID) && defined(ADDRESS_SANITIZER)
#define MAYBE_WithAudioProcessing DISABLED_WithAudioProcessing
#else
#define MAYBE_WithAudioProcessing WithAudioProcessing
#endif
TEST_F(MediaStreamAudioProcessorTest, MAYBE_WithAudioProcessing) {
  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new rtc::RefCountedObject<WebRtcAudioDeviceImpl>());
  AudioProcessingProperties properties;
  scoped_refptr<MediaStreamAudioProcessor> audio_processor(
      new rtc::RefCountedObject<MediaStreamAudioProcessor>(
          properties, webrtc_audio_device.get()));
  EXPECT_TRUE(audio_processor->has_audio_processing());
  audio_processor->OnCaptureFormatChanged(params_);
  VerifyDefaultComponents(audio_processor.get());

  ProcessDataAndVerifyFormat(audio_processor.get(),
                             kAudioProcessingSampleRate,
                             kAudioProcessingNumberOfChannel,
                             kAudioProcessingSampleRate / 100);

  // Stop |audio_processor| so that it removes itself from
  // |webrtc_audio_device| and clears its pointer to it.
  audio_processor->Stop();
}

TEST_F(MediaStreamAudioProcessorTest, TurnOffDefaultConstraints) {
  AudioProcessingProperties properties;
  // Turn off the default constraints and pass it to MediaStreamAudioProcessor.
  properties.DisableDefaultProperties();
  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new rtc::RefCountedObject<WebRtcAudioDeviceImpl>());
  scoped_refptr<MediaStreamAudioProcessor> audio_processor(
      new rtc::RefCountedObject<MediaStreamAudioProcessor>(
          properties, webrtc_audio_device.get()));
  EXPECT_FALSE(audio_processor->has_audio_processing());
  audio_processor->OnCaptureFormatChanged(params_);

  ProcessDataAndVerifyFormat(audio_processor.get(),
                             params_.sample_rate(),
                             params_.channels(),
                             params_.sample_rate() / 100);

  // Stop |audio_processor| so that it removes itself from
  // |webrtc_audio_device| and clears its pointer to it.
  audio_processor->Stop();
}

// Test crashing with ASAN on Android. crbug.com/468762
#if defined(OS_ANDROID) && defined(ADDRESS_SANITIZER)
#define MAYBE_TestAllSampleRates DISABLED_TestAllSampleRates
#else
#define MAYBE_TestAllSampleRates TestAllSampleRates
#endif
TEST_F(MediaStreamAudioProcessorTest, MAYBE_TestAllSampleRates) {
  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new rtc::RefCountedObject<WebRtcAudioDeviceImpl>());
  AudioProcessingProperties properties;
  scoped_refptr<MediaStreamAudioProcessor> audio_processor(
      new rtc::RefCountedObject<MediaStreamAudioProcessor>(
          properties, webrtc_audio_device.get()));
  EXPECT_TRUE(audio_processor->has_audio_processing());

  static const int kSupportedSampleRates[] =
      { 8000, 16000, 22050, 32000, 44100, 48000 };
  for (size_t i = 0; i < arraysize(kSupportedSampleRates); ++i) {
    int buffer_size = kSupportedSampleRates[i] / 100;
    media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                  media::CHANNEL_LAYOUT_STEREO,
                                  kSupportedSampleRates[i], buffer_size);
    audio_processor->OnCaptureFormatChanged(params);
    VerifyDefaultComponents(audio_processor.get());

    ProcessDataAndVerifyFormat(audio_processor.get(),
                               kAudioProcessingSampleRate,
                               kAudioProcessingNumberOfChannel,
                               kAudioProcessingSampleRate / 100);
  }

  // Stop |audio_processor| so that it removes itself from
  // |webrtc_audio_device| and clears its pointer to it.
  audio_processor->Stop();
}

// Test that if we have an AEC dump message filter created, we are getting it
// correctly in MSAP. Any IPC messages will be deleted since no sender in the
// filter will be created.
TEST_F(MediaStreamAudioProcessorTest, GetAecDumpMessageFilter) {
  scoped_refptr<AecDumpMessageFilter> aec_dump_message_filter_(
      new AecDumpMessageFilter(
          blink::scheduler::GetSingleThreadTaskRunnerForTesting(),
          blink::scheduler::GetSingleThreadTaskRunnerForTesting()));

  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new rtc::RefCountedObject<WebRtcAudioDeviceImpl>());
  AudioProcessingProperties properties;
  scoped_refptr<MediaStreamAudioProcessor> audio_processor(
      new rtc::RefCountedObject<MediaStreamAudioProcessor>(
          properties, webrtc_audio_device.get()));

  EXPECT_TRUE(audio_processor->aec_dump_message_filter_.get());

  // Stop |audio_processor| so that it removes itself from
  // |webrtc_audio_device| and clears its pointer to it.
  audio_processor->Stop();
}

TEST_F(MediaStreamAudioProcessorTest, StartStopAecDump) {
  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new rtc::RefCountedObject<WebRtcAudioDeviceImpl>());
  AudioProcessingProperties properties;

  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  base::FilePath temp_file_path;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_directory.GetPath(),
                                             &temp_file_path));
  {
    scoped_refptr<MediaStreamAudioProcessor> audio_processor(
        new rtc::RefCountedObject<MediaStreamAudioProcessor>(
            properties, webrtc_audio_device.get()));

    // Start and stop recording.
    audio_processor->OnAecDumpFile(IPC::TakePlatformFileForTransit(base::File(
        temp_file_path, base::File::FLAG_WRITE | base::File::FLAG_OPEN)));
    audio_processor->OnDisableAecDump();

    // Start and wait for d-tor.
    audio_processor->OnAecDumpFile(IPC::TakePlatformFileForTransit(base::File(
        temp_file_path, base::File::FLAG_WRITE | base::File::FLAG_OPEN)));
  }

  // Check that dump file is non-empty after audio processor has been
  // destroyed. Note that this test fails when compiling WebRTC
  // without protobuf support, rtc_enable_protobuf=false.
  std::string output;
  ASSERT_TRUE(base::ReadFileToString(temp_file_path, &output));
  ASSERT_FALSE(output.empty());
  // The tempory file is deleted when temp_directory exists scope.
}

TEST_F(MediaStreamAudioProcessorTest, TestStereoAudio) {
  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new rtc::RefCountedObject<WebRtcAudioDeviceImpl>());
  AudioProcessingProperties properties;
  // Turn off the audio processing and turn on the stereo channels mirroring.
  properties.DisableDefaultProperties();
  properties.goog_audio_mirroring = true;
  scoped_refptr<MediaStreamAudioProcessor> audio_processor(
      new rtc::RefCountedObject<MediaStreamAudioProcessor>(
          properties, webrtc_audio_device.get()));
  EXPECT_FALSE(audio_processor->has_audio_processing());
  const media::AudioParameters source_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::CHANNEL_LAYOUT_STEREO, 48000, 480);
  audio_processor->OnCaptureFormatChanged(source_params);
  // There's no sense in continuing if this fails.
  ASSERT_EQ(2, audio_processor->OutputFormat().channels());

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
    audio_processor->PushCaptureData(*wrapper, pushed_capture_delay);

    media::AudioBus* processed_data = nullptr;
    base::TimeDelta capture_delay;
    int new_volume = 0;
    EXPECT_TRUE(audio_processor->ProcessAndConsumeData(
        0, false, &processed_data, &capture_delay, &new_volume));
    EXPECT_TRUE(processed_data);
    EXPECT_EQ(processed_data->channel(0)[0], 0);
    EXPECT_NE(processed_data->channel(1)[0], 0);
    EXPECT_EQ(pushed_capture_delay, capture_delay);
  }

  // Stop |audio_processor| so that it removes itself from
  // |webrtc_audio_device| and clears its pointer to it.
  audio_processor->Stop();
}

// Disabled on android clang builds due to crbug.com/470499
#if defined(__clang__) && defined(OS_ANDROID)
#define MAYBE_TestWithKeyboardMicChannel DISABLED_TestWithKeyboardMicChannel
#else
#define MAYBE_TestWithKeyboardMicChannel TestWithKeyboardMicChannel
#endif
TEST_F(MediaStreamAudioProcessorTest, MAYBE_TestWithKeyboardMicChannel) {
  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new rtc::RefCountedObject<WebRtcAudioDeviceImpl>());
  AudioProcessingProperties properties;
  scoped_refptr<MediaStreamAudioProcessor> audio_processor(
      new rtc::RefCountedObject<MediaStreamAudioProcessor>(
          properties, webrtc_audio_device.get()));
  EXPECT_TRUE(audio_processor->has_audio_processing());

  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC,
                                48000, 480);
  audio_processor->OnCaptureFormatChanged(params);

  ProcessDataAndVerifyFormat(audio_processor.get(),
                             kAudioProcessingSampleRate,
                             kAudioProcessingNumberOfChannel,
                             kAudioProcessingSampleRate / 100);

  // Stop |audio_processor| so that it removes itself from
  // |webrtc_audio_device| and clears its pointer to it.
  audio_processor->Stop();
}

TEST_F(MediaStreamAudioProcessorTest, GetExtraGainConfigNullOpt) {
  base::Optional<std::string> audio_processing_platform_config_json;
  base::Optional<double> pre_amplifier_fixed_gain_factor,
      gain_control_compression_gain_db;
  GetExtraGainConfig(audio_processing_platform_config_json,
                     &pre_amplifier_fixed_gain_factor,
                     &gain_control_compression_gain_db);
  EXPECT_FALSE(pre_amplifier_fixed_gain_factor);
  EXPECT_FALSE(gain_control_compression_gain_db);
}

TEST_F(MediaStreamAudioProcessorTest, GetExtraGainConfig) {
  base::Optional<std::string> audio_processing_platform_config_json =
      "{\"gain_control_compression_gain_db\": 10}";
  base::Optional<double> pre_amplifier_fixed_gain_factor,
      gain_control_compression_gain_db;
  GetExtraGainConfig(audio_processing_platform_config_json,
                     &pre_amplifier_fixed_gain_factor,
                     &gain_control_compression_gain_db);
  EXPECT_FALSE(pre_amplifier_fixed_gain_factor);
  EXPECT_TRUE(gain_control_compression_gain_db);
  EXPECT_EQ(gain_control_compression_gain_db.value(), 10);
}

}  // namespace content
