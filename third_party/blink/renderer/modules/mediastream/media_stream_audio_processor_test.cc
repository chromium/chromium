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
#include "base/memory/aligned_memory.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/webrtc/webrtc_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_audio_processor.h"
#include "third_party/blink/renderer/modules/mediastream/mock_constraint_factory.h"
#include "third_party/blink/renderer/modules/webrtc/webrtc_audio_device_impl.h"
#include "third_party/blink/renderer/platform/mediastream/media_constraints.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_processor_options.h"
#include "third_party/webrtc/api/media_stream_interface.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::Return;

using media::AudioParameters;

namespace blink {

namespace {

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

class MediaStreamAudioProcessorTest : public ::testing::Test {
 public:
  MediaStreamAudioProcessorTest()
      : params_(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                media::CHANNEL_LAYOUT_STEREO,
                48000,
                480) {}

 protected:
  // Helper method to save duplicated code.
  static void ProcessDataAndVerifyFormat(
      blink::MediaStreamAudioProcessor* audio_processor,
      int expected_output_sample_rate,
      int expected_output_channels,
      int expected_output_buffer_size) {
    // Read the audio data from a file.
    const media::AudioParameters& params = audio_processor->InputFormat();
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
    const bool has_keyboard_mic = params.channel_layout() ==
                                  media::CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC;
    if (has_keyboard_mic) {
      data_bus_playout = media::AudioBus::CreateWrapper(2);
      data_bus_playout->set_frames(params.frames_per_buffer());
      data_bus_playout_to_use = data_bus_playout.get();
    }

    const base::TimeDelta input_capture_delay =
        base::TimeDelta::FromMilliseconds(20);
    const base::TimeDelta output_buffer_duration =
        expected_output_buffer_size * base::TimeDelta::FromSeconds(1) /
        expected_output_sample_rate;
    for (int i = 0; i < kNumberOfPacketsForTest; ++i) {
      data_bus->FromInterleaved<media::SignedInt16SampleTypeTraits>(
          data_ptr, data_bus->frames());
      audio_processor->PushCaptureData(*data_bus, input_capture_delay);

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
        audio_processor->OnPlayoutData(data_bus_playout_to_use,
                                       params.sample_rate(), 10);
      }

      media::AudioBus* processed_data = nullptr;
      base::TimeDelta capture_delay;
      int new_volume = 0;
      int num_preferred_channels = -1;
      while (audio_processor->ProcessAndConsumeData(
          255, num_preferred_channels, false, &processed_data, &capture_delay,
          &new_volume)) {
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

      // Test different values of num_preferred_channels.
      if (++num_preferred_channels > 5) {
        num_preferred_channels = 0;
      }
    }
  }

  void VerifyDefaultComponents(MediaStreamAudioProcessor* audio_processor) {
    webrtc::AudioProcessing* audio_processing =
        audio_processor->audio_processing_.get();
    const webrtc::AudioProcessing::Config config =
        audio_processing->GetConfig();
    EXPECT_TRUE(config.echo_canceller.enabled);
    EXPECT_TRUE(config.gain_controller1.enabled);
    EXPECT_TRUE(config.high_pass_filter.enabled);
    EXPECT_TRUE(config.noise_suppression.enabled);
    EXPECT_EQ(config.noise_suppression.level, config.noise_suppression.kHigh);
    EXPECT_FALSE(config.voice_detection.enabled);
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
};

class MediaStreamAudioProcessorTestMultichannel
    : public MediaStreamAudioProcessorTest,
      public ::testing::WithParamInterface<bool> {};

// Test crashing with ASAN on Android. crbug.com/468762
#if defined(OS_ANDROID) && defined(ADDRESS_SANITIZER)
#define MAYBE_WithAudioProcessing DISABLED_WithAudioProcessing
#else
#define MAYBE_WithAudioProcessing WithAudioProcessing
#endif
TEST_P(MediaStreamAudioProcessorTestMultichannel, MAYBE_WithAudioProcessing) {
  const bool use_multichannel_processing = GetParam();
  base::test::ScopedFeatureList features;
  features.InitWithFeatureState(features::kWebRtcEnableCaptureMultiChannelApm,
                                use_multichannel_processing);
  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new rtc::RefCountedObject<WebRtcAudioDeviceImpl>());
  blink::AudioProcessingProperties properties;
  scoped_refptr<MediaStreamAudioProcessor> audio_processor(
      new rtc::RefCountedObject<MediaStreamAudioProcessor>(
          properties, webrtc_audio_device.get()));
  EXPECT_TRUE(audio_processor->has_audio_processing());
  audio_processor->OnCaptureFormatChanged(params_);
  VerifyDefaultComponents(audio_processor.get());

  const int expected_output_channels =
      use_multichannel_processing ? params_.channels() : 1;
  ProcessDataAndVerifyFormat(
      audio_processor.get(), blink::kAudioProcessingSampleRate,
      expected_output_channels, blink::kAudioProcessingSampleRate / 100);

  // Stop |audio_processor| so that it removes itself from
  // |webrtc_audio_device| and clears its pointer to it.
  audio_processor->Stop();
}

TEST_F(MediaStreamAudioProcessorTest, TurnOffDefaultConstraints) {
  blink::AudioProcessingProperties properties;
  // Turn off the default constraints and pass it to MediaStreamAudioProcessor.
  properties.DisableDefaultProperties();
  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new rtc::RefCountedObject<WebRtcAudioDeviceImpl>());
  scoped_refptr<MediaStreamAudioProcessor> audio_processor(
      new rtc::RefCountedObject<MediaStreamAudioProcessor>(
          properties, webrtc_audio_device.get()));
  EXPECT_FALSE(audio_processor->has_audio_processing());
  audio_processor->OnCaptureFormatChanged(params_);

  ProcessDataAndVerifyFormat(audio_processor.get(), params_.sample_rate(),
                             params_.channels(), params_.sample_rate() / 100);

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
TEST_P(MediaStreamAudioProcessorTestMultichannel, MAYBE_TestAllSampleRates) {
  const bool use_multichannel_processing = GetParam();
  base::test::ScopedFeatureList features;
  features.InitWithFeatureState(features::kWebRtcEnableCaptureMultiChannelApm,
                                use_multichannel_processing);
  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new rtc::RefCountedObject<WebRtcAudioDeviceImpl>());
  blink::AudioProcessingProperties properties;
  scoped_refptr<MediaStreamAudioProcessor> audio_processor(
      new rtc::RefCountedObject<MediaStreamAudioProcessor>(
          properties, webrtc_audio_device.get()));
  EXPECT_TRUE(audio_processor->has_audio_processing());

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
  for (size_t i = 0; i < base::size(kSupportedSampleRates); ++i) {
    int buffer_size = kSupportedSampleRates[i] / 100;
    media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                  media::CHANNEL_LAYOUT_STEREO,
                                  kSupportedSampleRates[i], buffer_size);
    audio_processor->OnCaptureFormatChanged(params);
    VerifyDefaultComponents(audio_processor.get());

    int expected_sample_rate =
#if BUILDFLAG(IS_CHROMECAST)
        std::min(kSupportedSampleRates[i], blink::kAudioProcessingSampleRate);
#else
        blink::kAudioProcessingSampleRate;
#endif  // BUILDFLAG(IS_CHROMECAST)
    const int expected_output_channels =
        use_multichannel_processing ? params_.channels() : 1;
    ProcessDataAndVerifyFormat(audio_processor.get(), expected_sample_rate,
                               expected_output_channels,
                               expected_sample_rate / 100);
  }

  // Stop |audio_processor| so that it removes itself from
  // |webrtc_audio_device| and clears its pointer to it.
  audio_processor->Stop();
}

TEST_F(MediaStreamAudioProcessorTest, StartStopAecDump) {
  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new rtc::RefCountedObject<WebRtcAudioDeviceImpl>());
  blink::AudioProcessingProperties properties;

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
    audio_processor->OnStartDump(base::File(
        temp_file_path, base::File::FLAG_WRITE | base::File::FLAG_OPEN));
    audio_processor->OnStopDump();

    // Start and wait for d-tor.
    audio_processor->OnStartDump(base::File(
        temp_file_path, base::File::FLAG_WRITE | base::File::FLAG_OPEN));
  }

  // Check that dump file is non-empty after audio processor has been
  // destroyed. Note that this test fails when compiling WebRTC
  // without protobuf support, rtc_enable_protobuf=false.
  std::string output;
  ASSERT_TRUE(base::ReadFileToString(temp_file_path, &output));
  ASSERT_FALSE(output.empty());
  // The tempory file is deleted when temp_directory exists scope.
}

TEST_P(MediaStreamAudioProcessorTestMultichannel, TestStereoAudio) {
  const bool use_multichannel_processing = GetParam();
  base::test::ScopedFeatureList features;
  features.InitWithFeatureState(features::kWebRtcEnableCaptureMultiChannelApm,
                                use_multichannel_processing);
  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new rtc::RefCountedObject<WebRtcAudioDeviceImpl>());

  const media::AudioParameters source_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::CHANNEL_LAYOUT_STEREO, 48000, 480);

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

  // Test without and with audio processing enabled.
  for (int use_apm = 0; use_apm <= 1; ++use_apm) {
    // No need to test stereo with APM if disabled.
    if (use_apm && !use_multichannel_processing) {
      continue;
    }

    blink::AudioProcessingProperties properties;
    if (!use_apm) {
      // Turn off the audio processing.
      properties.DisableDefaultProperties();
    }
    // Turn on the stereo channels mirroring.
    properties.goog_audio_mirroring = true;
    scoped_refptr<MediaStreamAudioProcessor> audio_processor(
        new rtc::RefCountedObject<MediaStreamAudioProcessor>(
            properties, webrtc_audio_device.get()));
    EXPECT_EQ(audio_processor->has_audio_processing(), use_apm);
    audio_processor->OnCaptureFormatChanged(source_params);
    // There's no sense in continuing if this fails.
    ASSERT_EQ(2, audio_processor->OutputFormat().channels());

    // Run the test consecutively to make sure the stereo channels are not
    // flipped back and forth.
    static const int kNumberOfPacketsForTest = 100;
    const base::TimeDelta pushed_capture_delay =
        base::TimeDelta::FromMilliseconds(42);
    media::AudioBus* processed_data = nullptr;

    for (int num_preferred_channels = 0; num_preferred_channels <= 5;
         ++num_preferred_channels) {
      for (int i = 0; i < kNumberOfPacketsForTest; ++i) {
        audio_processor->PushCaptureData(*wrapper, pushed_capture_delay);

        base::TimeDelta capture_delay;
        int new_volume = 0;
        EXPECT_TRUE(audio_processor->ProcessAndConsumeData(
            0, num_preferred_channels, false, &processed_data, &capture_delay,
            &new_volume));
        EXPECT_TRUE(processed_data);
        EXPECT_EQ(pushed_capture_delay, capture_delay);
      }
      if (use_apm && num_preferred_channels <= 1) {
        // Mono output. Output channels are averaged.
        EXPECT_NE(processed_data->channel(0)[0], 0);
        EXPECT_NE(processed_data->channel(1)[0], 0);
      } else {
        // Stereo output. Output channels are independent.
        EXPECT_EQ(processed_data->channel(0)[0], 0);
        EXPECT_NE(processed_data->channel(1)[0], 0);
      }
    }

    // Stop |audio_processor| so that it removes itself from
    // |webrtc_audio_device| and clears its pointer to it.
    audio_processor->Stop();
  }
}

// Disabled on android clang builds due to crbug.com/470499
#if defined(__clang__) && defined(OS_ANDROID)
#define MAYBE_TestWithKeyboardMicChannel DISABLED_TestWithKeyboardMicChannel
#else
#define MAYBE_TestWithKeyboardMicChannel TestWithKeyboardMicChannel
#endif
TEST_P(MediaStreamAudioProcessorTestMultichannel,
       MAYBE_TestWithKeyboardMicChannel) {
  const bool use_multichannel_processing = GetParam();
  base::test::ScopedFeatureList features;
  features.InitWithFeatureState(features::kWebRtcEnableCaptureMultiChannelApm,
                                use_multichannel_processing);
  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new rtc::RefCountedObject<WebRtcAudioDeviceImpl>());
  blink::AudioProcessingProperties properties;
  scoped_refptr<MediaStreamAudioProcessor> audio_processor(
      new rtc::RefCountedObject<MediaStreamAudioProcessor>(
          properties, webrtc_audio_device.get()));
  EXPECT_TRUE(audio_processor->has_audio_processing());

  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC,
                                48000, 480);
  audio_processor->OnCaptureFormatChanged(params);

  const int expected_output_channels =
      use_multichannel_processing ? params_.channels() : 1;

  ProcessDataAndVerifyFormat(
      audio_processor.get(), blink::kAudioProcessingSampleRate,
      expected_output_channels, blink::kAudioProcessingSampleRate / 100);

  // Stop |audio_processor| so that it removes itself from
  // |webrtc_audio_device| and clears its pointer to it.
  audio_processor->Stop();
}

TEST_F(MediaStreamAudioProcessorTest, TestAgcEnableDefaultAgc1) {
  ::base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kWebRtcHybridAgc);

  blink::AudioProcessingProperties properties;
  properties.goog_auto_gain_control = true;
  properties.goog_experimental_auto_gain_control = false;

  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new rtc::RefCountedObject<WebRtcAudioDeviceImpl>());
  scoped_refptr<MediaStreamAudioProcessor> audio_processor(
      new rtc::RefCountedObject<MediaStreamAudioProcessor>(
          properties, webrtc_audio_device.get()));

  base::Optional<webrtc::AudioProcessing::Config> apm_config =
      audio_processor->GetAudioProcessingModuleConfig();
  ASSERT_TRUE(apm_config);

  EXPECT_TRUE(apm_config->gain_controller1.enabled);
  EXPECT_EQ(
      apm_config->gain_controller1.mode,
#if defined(OS_ANDROID)
      webrtc::AudioProcessing::Config::GainController1::Mode::kFixedDigital);
#else
      webrtc::AudioProcessing::Config::GainController1::Mode::kAdaptiveAnalog);
#endif  // defined(OS_ANDROID)
}

TEST_F(MediaStreamAudioProcessorTest, TestAgcEnableHybridAgc) {
  ::base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kWebRtcHybridAgc,
      {{"vad_probability_attack", "0.2"},
       {"use_peaks_not_rms", "true"},
       {"level_estimator_speech_frames_threshold", "3"},
       {"initial_saturation_margin", "10"},
       {"extra_saturation_margin", "11"},
       {"gain_applier_speech_frames_threshold", "5"},
       {"max_gain_change_db_per_second", "4"},
       {"max_output_noise_level_dbfs", "-22"},
       {"sse2_allowed", "true"},
       {"avx2_allowed", "true"},
       {"neon_allowed", "true"}});

  blink::AudioProcessingProperties properties;
  properties.goog_auto_gain_control = true;
  properties.goog_experimental_auto_gain_control = true;

  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new rtc::RefCountedObject<WebRtcAudioDeviceImpl>());
  scoped_refptr<MediaStreamAudioProcessor> audio_processor(
      new rtc::RefCountedObject<MediaStreamAudioProcessor>(
          properties, webrtc_audio_device.get()));

  base::Optional<webrtc::AudioProcessing::Config> apm_config =
      audio_processor->GetAudioProcessingModuleConfig();
  ASSERT_TRUE(apm_config);

  EXPECT_TRUE(apm_config->gain_controller1.enabled);
  EXPECT_EQ(
      apm_config->gain_controller1.mode,
#if defined(OS_ANDROID)
      webrtc::AudioProcessing::Config::GainController1::Mode::kFixedDigital);
#else
      webrtc::AudioProcessing::Config::GainController1::Mode::kAdaptiveAnalog);
#endif  // defined(OS_ANDROID)
  EXPECT_TRUE(apm_config->gain_controller2.enabled);
  // `compression_gain_db` has no effect when hybrid AGC is active.
  EXPECT_EQ(apm_config->gain_controller2.fixed_digital.gain_db, 0);

  const auto& adaptive_digital = apm_config->gain_controller2.adaptive_digital;
  EXPECT_TRUE(adaptive_digital.enabled);
  EXPECT_FLOAT_EQ(adaptive_digital.vad_probability_attack, 0.2f);
  EXPECT_EQ(
      adaptive_digital.level_estimator,
      webrtc::AudioProcessing::Config::GainController2::LevelEstimator::kPeak);
  EXPECT_EQ(adaptive_digital.level_estimator_adjacent_speech_frames_threshold,
            3);
  EXPECT_FLOAT_EQ(adaptive_digital.initial_saturation_margin_db, 10);
  EXPECT_FLOAT_EQ(adaptive_digital.extra_saturation_margin_db, 11);
  EXPECT_EQ(adaptive_digital.gain_applier_adjacent_speech_frames_threshold, 5);
  EXPECT_FLOAT_EQ(adaptive_digital.max_gain_change_db_per_second, 4);
  EXPECT_FLOAT_EQ(adaptive_digital.max_output_noise_level_dbfs, -22);
  EXPECT_TRUE(adaptive_digital.sse2_allowed);
  EXPECT_TRUE(adaptive_digital.avx2_allowed);
  EXPECT_TRUE(adaptive_digital.neon_allowed);
}

TEST_F(MediaStreamAudioProcessorTest, TestAgcEnableHybridAgcSimdNotAllowed) {
  ::base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(features::kWebRtcHybridAgc,
                                                  {{"sse2_allowed", "false"},
                                                   {"avx2_allowed", "false"},
                                                   {"neon_allowed", "false"}});

  blink::AudioProcessingProperties properties;
  properties.goog_auto_gain_control = true;
  properties.goog_experimental_auto_gain_control = true;

  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new rtc::RefCountedObject<WebRtcAudioDeviceImpl>());
  scoped_refptr<MediaStreamAudioProcessor> audio_processor(
      new rtc::RefCountedObject<MediaStreamAudioProcessor>(
          properties, webrtc_audio_device.get()));

  base::Optional<webrtc::AudioProcessing::Config> apm_config =
      audio_processor->GetAudioProcessingModuleConfig();
  ASSERT_TRUE(apm_config);

  EXPECT_FALSE(apm_config->gain_controller2.adaptive_digital.sse2_allowed);
  EXPECT_FALSE(apm_config->gain_controller2.adaptive_digital.avx2_allowed);
  EXPECT_FALSE(apm_config->gain_controller2.adaptive_digital.neon_allowed);
}

// Ensure that discrete channel layouts do not crash with audio processing
// enabled.
TEST_F(MediaStreamAudioProcessorTest, DiscreteChannelLayout) {
  blink::AudioProcessingProperties properties;
  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new rtc::RefCountedObject<WebRtcAudioDeviceImpl>());
  scoped_refptr<MediaStreamAudioProcessor> audio_processor(
      new rtc::RefCountedObject<MediaStreamAudioProcessor>(
          properties, webrtc_audio_device.get()));
  EXPECT_TRUE(audio_processor->has_audio_processing());

  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::CHANNEL_LAYOUT_DISCRETE, 48000, 480);
  // Test both 1 and 2 discrete channels.
  for (int channels = 1; channels <= 2; ++channels) {
    params.set_channels_for_discrete(channels);
    audio_processor->OnCaptureFormatChanged(params);
  }

  audio_processor->Stop();
}

INSTANTIATE_TEST_CASE_P(MediaStreamAudioProcessorMultichannelAffectedTests,
                        MediaStreamAudioProcessorTestMultichannel,
                        ::testing::Values(false, true));
}  // namespace blink
