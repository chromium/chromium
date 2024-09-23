// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/mediastream/media_stream_audio_processor.h"

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/aligned_memory.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/webrtc/constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/renderer/modules/webrtc/webrtc_audio_device_impl.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_processor_options.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/webrtc/api/media_stream_interface.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::Return;

using media::AudioParameters;

using AnalogGainController =
    webrtc::AudioProcessing::Config::GainController1::AnalogGainController;

namespace blink {
namespace {

using MockProcessedCaptureCallback =
    base::MockRepeatingCallback<void(const media::AudioBus& audio_bus,
                                     base::TimeTicks audio_capture_time,
                                     std::optional<double> new_volume)>;

// The number of packets used for testing.
const int kNumberOfPacketsForTest = 100;

void ReadDataFromSpeechFile(base::HeapArray<int16_t>& data) {
  base::FilePath file;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &file));
  file = file.Append(FILE_PATH_LITERAL("media"))
             .Append(FILE_PATH_LITERAL("test"))
             .Append(FILE_PATH_LITERAL("data"))
             .Append(FILE_PATH_LITERAL("speech_16b_stereo_48kHz.raw"));
  DCHECK(base::PathExists(file));
  int64_t data_file_size64 = 0;
  DCHECK(base::GetFileSize(file, &data_file_size64));
  auto bytes = base::as_writable_chars(data.as_span());
  EXPECT_EQ(base::checked_cast<int>(bytes.size_bytes()),
            base::ReadFile(file, bytes));
  DCHECK(data_file_size64 > base::checked_cast<int64_t>(data.size()));
}

}  // namespace

class MediaStreamAudioProcessorTest : public ::testing::Test {
 public:
  MediaStreamAudioProcessorTest()
      : params_(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                media::ChannelLayoutConfig::Stereo(),
                48000,
                480) {}

 protected:
  // Helper method to save duplicated code.
  static void ProcessDataAndVerifyFormat(
      MediaStreamAudioProcessor& audio_processor,
      MockProcessedCaptureCallback& mock_capture_callback,
      int expected_output_sample_rate,
      int expected_output_channels,
      int expected_output_buffer_size) {
    // Read the audio data from a file.
    const media::AudioParameters& params =
        audio_processor.GetInputFormatForTesting();
    const int frames_per_packet =
        params.frames_per_buffer() * params.channels();
    const size_t length = frames_per_packet * kNumberOfPacketsForTest;
    auto capture_data = base::HeapArray<int16_t>::Uninit(length);
    ReadDataFromSpeechFile(capture_data);
    const int16_t* data_ptr =
        reinterpret_cast<const int16_t*>(capture_data.data());
    std::unique_ptr<media::AudioBus> data_bus =
        media::AudioBus::Create(params.channels(), params.frames_per_buffer());

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
        audio_processor.OnPlayoutData(data_bus.get(), params.sample_rate(),
                                      base::Milliseconds(10));
      }

      // 2. Set up expectations and process captured audio.
      EXPECT_CALL(mock_capture_callback, Run(_, _, _))
          .WillRepeatedly([&](const media::AudioBus& processed_audio,
                              base::TimeTicks audio_capture_time,
                              std::optional<double> new_volume) {
            EXPECT_EQ(audio_capture_time, input_capture_time);
          });
      audio_processor.ProcessCapturedAudio(*data_bus, input_capture_time,
                                           num_preferred_channels, 1.0, false);
      EXPECT_EQ(expected_output_sample_rate,
                audio_processor.output_format().sample_rate());
      EXPECT_EQ(expected_output_channels,
                audio_processor.output_format().channels());
      EXPECT_EQ(expected_output_buffer_size,
                audio_processor.output_format().frames_per_buffer());

      data_ptr += params.frames_per_buffer() * params.channels();

      // Test different values of num_preferred_channels.
      if (++num_preferred_channels > 5) {
        num_preferred_channels = 0;
      }
    }
  }

  // TODO(bugs.webrtc.org/7494): Remove/reduce duplication with
  // `CreateWebRtcAudioProcessingModuleTest.CheckDefaultAudioProcessingConfig`.
  void VerifyDefaultComponents(MediaStreamAudioProcessor& audio_processor) {
    ASSERT_TRUE(audio_processor.has_webrtc_audio_processing());
    const webrtc::AudioProcessing::Config config =
        *audio_processor.GetAudioProcessingModuleConfigForTesting();

    EXPECT_TRUE(config.high_pass_filter.enabled);
    EXPECT_FALSE(config.pre_amplifier.enabled);
    EXPECT_TRUE(config.echo_canceller.enabled);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    EXPECT_FALSE(config.gain_controller1.enabled);
    EXPECT_TRUE(config.gain_controller2.enabled);
#elif BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
    EXPECT_FALSE(config.gain_controller1.enabled);
    EXPECT_TRUE(config.gain_controller2.enabled);
#elif BUILDFLAG(IS_CASTOS) || BUILDFLAG(IS_CAST_ANDROID)
    EXPECT_TRUE(config.gain_controller1.enabled);
    EXPECT_FALSE(config.gain_controller2.enabled);
#elif BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    EXPECT_FALSE(config.gain_controller1.enabled);
    EXPECT_TRUE(config.gain_controller2.enabled);
#else
    GTEST_FAIL() << "Undefined expectation.";
#endif

    EXPECT_TRUE(config.noise_suppression.enabled);
    EXPECT_EQ(config.noise_suppression.level,
              webrtc::AudioProcessing::Config::NoiseSuppression::kHigh);
    EXPECT_FALSE(config.transient_suppression.enabled);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    // Android uses echo cancellation optimized for mobiles.
    EXPECT_TRUE(config.echo_canceller.mobile_mode);
#else
    EXPECT_FALSE(config.echo_canceller.mobile_mode);
#endif
  }

  test::TaskEnvironment task_environment_;
  media::AudioParameters params_;
  MockProcessedCaptureCallback mock_capture_callback_;
};

class MediaStreamAudioProcessorTestMultichannel
    : public MediaStreamAudioProcessorTest,
      public ::testing::WithParamInterface<bool> {};

// Test crashing with ASAN on Android. crbug.com/468762
#if BUILDFLAG(IS_ANDROID) && defined(ADDRESS_SANITIZER)
#define MAYBE_WithAudioProcessing DISABLED_WithAudioProcessing
#else
#define MAYBE_WithAudioProcessing WithAudioProcessing
#endif
TEST_P(MediaStreamAudioProcessorTestMultichannel, MAYBE_WithAudioProcessing) {
  const bool use_multichannel_processing = GetParam();
  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new rtc::RefCountedObject<WebRtcAudioDeviceImpl>());
  blink::AudioProcessingProperties properties;
  scoped_refptr<MediaStreamAudioProcessor> audio_processor(
      new rtc::RefCountedObject<MediaStreamAudioProcessor>(
          mock_capture_callback_.Get(),
          properties.ToAudioProcessingSettings(use_multichannel_processing),
          params_, webrtc_audio_device));
  EXPECT_TRUE(audio_processor->has_webrtc_audio_processing());
  VerifyDefaultComponents(*audio_processor);

  const int expected_output_channels =
      use_multichannel_processing ? params_.channels() : 1;
  ProcessDataAndVerifyFormat(*audio_processor, mock_capture_callback_,
                             media::WebRtcAudioProcessingSampleRateHz(),
                             expected_output_channels,
                             media::WebRtcAudioProcessingSampleRateHz() / 100);

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
          mock_capture_callback_.Get(),
          properties.ToAudioProcessingSettings(
              /*multi_channel_capture_processing=*/true),
          params_, webrtc_audio_device));
  EXPECT_FALSE(audio_processor->has_webrtc_audio_processing());

  ProcessDataAndVerifyFormat(*audio_processor, mock_capture_callback_,
                             params_.sample_rate(), params_.channels(),
                             params_.sample_rate() / 100);

  // Stop |audio_processor| so that it removes itself from
  // |webrtc_audio_device| and clears its pointer to it.
  audio_processor->Stop();
}

// Test crashing with ASAN on Android. crbug.com/468762
#if BUILDFLAG(IS_ANDROID) && defined(ADDRESS_SANITIZER)
#define MAYBE_TestAllSampleRates DISABLED_TestAllSampleRates
#else
#define MAYBE_TestAllSampleRates TestAllSampleRates
#endif
TEST_P(MediaStreamAudioProcessorTestMultichannel, MAYBE_TestAllSampleRates) {
  const bool use_multichannel_processing = GetParam();
  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new rtc::RefCountedObject<WebRtcAudioDeviceImpl>());
  blink::AudioProcessingProperties properties;

  // TODO(crbug.com/1334991): Clarify WebRTC audio processing support for 96 kHz
  // input.
  static const int kSupportedSampleRates[] = {
    8000,
    16000,
    22050,
    32000,
    44100,
    48000
#if BUILDFLAG(IS_CASTOS) || BUILDFLAG(IS_CAST_ANDROID)
    ,
    96000
#endif  // BUILDFLAG(IS_CASTOS) || BUILDFLAG(IS_CAST_ANDROID)
  };
  for (int sample_rate : kSupportedSampleRates) {
    SCOPED_TRACE(testing::Message() << "sample_rate=" << sample_rate);
    int buffer_size = sample_rate / 100;
    media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                  media::ChannelLayoutConfig::Stereo(),
                                  sample_rate, buffer_size);
    scoped_refptr<MediaStreamAudioProcessor> audio_processor(
        new rtc::RefCountedObject<MediaStreamAudioProcessor>(
            mock_capture_callback_.Get(),
            properties.ToAudioProcessingSettings(use_multichannel_processing),
            params, webrtc_audio_device));
    EXPECT_TRUE(audio_processor->has_webrtc_audio_processing());
    VerifyDefaultComponents(*audio_processor);

    // TODO(crbug.com/1336055): Investigate why chromecast devices need special
    // logic here.
    int expected_sample_rate =
#if BUILDFLAG(IS_CASTOS) || BUILDFLAG(IS_CAST_ANDROID)
        std::min(sample_rate, media::WebRtcAudioProcessingSampleRateHz());
#else
        media::WebRtcAudioProcessingSampleRateHz();
#endif  // BUILDFLAG(IS_CASTOS) || BUILDFLAG(IS_CAST_ANDROID)
    const int expected_output_channels =
        use_multichannel_processing ? params_.channels() : 1;
    ProcessDataAndVerifyFormat(*audio_processor, mock_capture_callback_,
                               expected_sample_rate, expected_output_channels,
                               expected_sample_rate / 100);

    // Stop |audio_processor| so that it removes itself from
    // |webrtc_audio_device| and clears its pointer to it.
    audio_processor->Stop();
  }
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
  media::AudioParameters params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::FromLayout<
          media::CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC>(),
      48000, 480);
  {
    scoped_refptr<MediaStreamAudioProcessor> audio_processor(
        new rtc::RefCountedObject<MediaStreamAudioProcessor>(
            mock_capture_callback_.Get(),
            properties.ToAudioProcessingSettings(
                /*multi_channel_capture_processing=*/true),
            params, webrtc_audio_device));

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
  SCOPED_TRACE(testing::Message() << "use_multichannel_processing="
                                  << use_multichannel_processing);
  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new rtc::RefCountedObject<WebRtcAudioDeviceImpl>());
  const media::AudioParameters source_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Stereo(), 48000, 480);

  // Construct a stereo audio bus and fill the left channel with content.
  std::unique_ptr<media::AudioBus> data_bus =
      media::AudioBus::Create(params_.channels(), params_.frames_per_buffer());
  data_bus->Zero();
  for (int i = 0; i < data_bus->frames(); ++i) {
    data_bus->channel(0)[i] = (i % 11) * 0.1f - 0.5f;
  }

  // Test without and with audio processing enabled.
  constexpr bool kUseApmValues[] =
#if BUILDFLAG(IS_IOS)
      // TODO(https://crbug.com/1417474): `false` fails on ios-blink platform
      // due to a special case for iOS in settings.NeedWebrtcAudioProcessing()
      {true};
#else
      {false, true};
#endif
  for (bool use_apm : kUseApmValues) {
    // No need to test stereo with APM if disabled.
    if (use_apm && !use_multichannel_processing) {
      continue;
    }
    SCOPED_TRACE(testing::Message() << "use_apm=" << use_apm);

    blink::AudioProcessingProperties properties;
    if (!use_apm) {
      // Turn off the audio processing.
      properties.DisableDefaultProperties();
    }
    // Turn on the stereo channels mirroring.
    properties.goog_audio_mirroring = true;
    scoped_refptr<MediaStreamAudioProcessor> audio_processor(
        new rtc::RefCountedObject<MediaStreamAudioProcessor>(
            mock_capture_callback_.Get(),
            properties.ToAudioProcessingSettings(use_multichannel_processing),
            source_params, webrtc_audio_device));
    EXPECT_EQ(audio_processor->has_webrtc_audio_processing(), use_apm);
    // There's no sense in continuing if this fails.
    ASSERT_EQ(2, audio_processor->output_format().channels());

    // Run the test consecutively to make sure the stereo channels are not
    // flipped back and forth.
    const base::TimeTicks pushed_capture_time = base::TimeTicks::Now();

    for (int num_preferred_channels = 0; num_preferred_channels <= 5;
         ++num_preferred_channels) {
      SCOPED_TRACE(testing::Message()
                   << "num_preferred_channels=" << num_preferred_channels);
      for (int i = 0; i < kNumberOfPacketsForTest; ++i) {
        SCOPED_TRACE(testing::Message() << "packet index i=" << i);
        EXPECT_CALL(mock_capture_callback_, Run(_, _, _)).Times(1);
        // Pass audio for processing.
        audio_processor->ProcessCapturedAudio(
            *data_bus, pushed_capture_time, num_preferred_channels, 0.0, false);
      }
      // At this point, the audio processing algorithms have gotten past any
      // initial buffer silence generated from resamplers, FFTs, and whatnot.
      // Set up expectations via the mock callback:
      EXPECT_CALL(mock_capture_callback_, Run(_, _, _))
          .WillRepeatedly([&](const media::AudioBus& processed_audio,
                              base::TimeTicks audio_capture_time,
                              std::optional<double> new_volume) {
            EXPECT_EQ(audio_capture_time, pushed_capture_time);
            if (!use_apm) {
              EXPECT_FALSE(new_volume.has_value());
            }
            float left_channel_energy = 0.0f;
            float right_channel_energy = 0.0f;
            for (int i = 0; i < processed_audio.frames(); ++i) {
              left_channel_energy +=
                  processed_audio.channel(0)[i] * processed_audio.channel(0)[i];
              right_channel_energy +=
                  processed_audio.channel(1)[i] * processed_audio.channel(1)[i];
            }
            if (use_apm && num_preferred_channels <= 1) {
              // Mono output. Output channels are averaged.
              EXPECT_NE(left_channel_energy, 0);
              EXPECT_NE(right_channel_energy, 0);
            } else {
              // Stereo output. Output channels are independent.
              // Note that after stereo mirroring, the _right_ channel is
              // non-zero.
              EXPECT_EQ(left_channel_energy, 0);
              EXPECT_NE(right_channel_energy, 0);
            }
          });
      // Process one more frame of audio.
      audio_processor->ProcessCapturedAudio(*data_bus, pushed_capture_time,
                                            num_preferred_channels, 0.0, false);
    }

    // Stop |audio_processor| so that it removes itself from
    // |webrtc_audio_device| and clears its pointer to it.
    audio_processor->Stop();
  }
}

// Ensure that discrete channel layouts do not crash with audio processing
// enabled.
TEST_F(MediaStreamAudioProcessorTest, DiscreteChannelLayout) {
  blink::AudioProcessingProperties properties;
  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new rtc::RefCountedObject<WebRtcAudioDeviceImpl>());

  // Test both 1 and 2 discrete channels.
  for (int channels = 1; channels <= 2; ++channels) {
    media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                  {media::CHANNEL_LAYOUT_DISCRETE, channels},
                                  48000, 480);
    scoped_refptr<MediaStreamAudioProcessor> audio_processor(
        new rtc::RefCountedObject<MediaStreamAudioProcessor>(
            mock_capture_callback_.Get(),
            properties.ToAudioProcessingSettings(
                /*multi_channel_capture_processing==*/true),
            params, webrtc_audio_device));
    EXPECT_TRUE(audio_processor->has_webrtc_audio_processing());
    audio_processor->Stop();
  }
}

INSTANTIATE_TEST_SUITE_P(MediaStreamAudioProcessorMultichannelAffectedTests,
                         MediaStreamAudioProcessorTestMultichannel,
                         ::testing::Values(false, true));

// When audio processing is performed, processed audio should be delivered as
// soon as 10 ms of audio has been received.
TEST(MediaStreamAudioProcessorCallbackTest,
     ProcessedAudioIsDeliveredAsSoonAsPossibleWithShortBuffers) {
  test::TaskEnvironment task_environment_;
  MockProcessedCaptureCallback mock_capture_callback;
  blink::AudioProcessingProperties properties;
  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new rtc::RefCountedObject<WebRtcAudioDeviceImpl>());
  // Set buffer size to 4 ms.
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::ChannelLayoutConfig::Stereo(), 48000,
                                48000 * 4 / 1000);
  scoped_refptr<MediaStreamAudioProcessor> audio_processor(
      new rtc::RefCountedObject<MediaStreamAudioProcessor>(
          mock_capture_callback.Get(),
          properties.ToAudioProcessingSettings(
              /*multi_channel_capture_processing=*/true),
          params, webrtc_audio_device));
  ASSERT_TRUE(audio_processor->has_webrtc_audio_processing());

  int output_sample_rate = audio_processor->output_format().sample_rate();
  std::unique_ptr<media::AudioBus> data_bus =
      media::AudioBus::Create(params.channels(), params.frames_per_buffer());
  data_bus->Zero();

  auto check_audio_length = [&](const media::AudioBus& processed_audio,
                                base::TimeTicks, std::optional<double>) {
    EXPECT_EQ(processed_audio.frames(), output_sample_rate * 10 / 1000);
  };

  // 4 ms of data: Not enough to process.
  EXPECT_CALL(mock_capture_callback, Run(_, _, _)).Times(0);
  audio_processor->ProcessCapturedAudio(*data_bus, base::TimeTicks::Now(), -1,
                                        1.0, false);
  // 8 ms of data: Not enough to process.
  EXPECT_CALL(mock_capture_callback, Run(_, _, _)).Times(0);
  audio_processor->ProcessCapturedAudio(*data_bus, base::TimeTicks::Now(), -1,
                                        1.0, false);
  // 12 ms of data: Should trigger callback, with 2 ms left in the processor.
  EXPECT_CALL(mock_capture_callback, Run(_, _, _))
      .Times(1)
      .WillOnce(check_audio_length);
  audio_processor->ProcessCapturedAudio(*data_bus, base::TimeTicks::Now(), -1,
                                        1.0, false);
  // 2 + 4 ms of data: Not enough to process.
  EXPECT_CALL(mock_capture_callback, Run(_, _, _)).Times(0);
  audio_processor->ProcessCapturedAudio(*data_bus, base::TimeTicks::Now(), -1,
                                        1.0, false);
  // 10 ms of data: Should trigger callback.
  EXPECT_CALL(mock_capture_callback, Run(_, _, _))
      .Times(1)
      .WillOnce(check_audio_length);
  audio_processor->ProcessCapturedAudio(*data_bus, base::TimeTicks::Now(), -1,
                                        1.0, false);

  audio_processor->Stop();
}

// When audio processing is performed, input containing 10 ms several times over
// should trigger a comparable number of processing callbacks.
TEST(MediaStreamAudioProcessorCallbackTest,
     ProcessedAudioIsDeliveredAsSoonAsPossibleWithLongBuffers) {
  test::TaskEnvironment task_environment_;
  MockProcessedCaptureCallback mock_capture_callback;
  blink::AudioProcessingProperties properties;
  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new rtc::RefCountedObject<WebRtcAudioDeviceImpl>());
  // Set buffer size to 35 ms.
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::ChannelLayoutConfig::Stereo(), 48000,
                                48000 * 35 / 1000);
  scoped_refptr<MediaStreamAudioProcessor> audio_processor(
      new rtc::RefCountedObject<MediaStreamAudioProcessor>(
          mock_capture_callback.Get(),
          properties.ToAudioProcessingSettings(
              /*multi_channel_capture_processing=*/true),
          params, webrtc_audio_device));
  ASSERT_TRUE(audio_processor->has_webrtc_audio_processing());

  int output_sample_rate = audio_processor->output_format().sample_rate();
  std::unique_ptr<media::AudioBus> data_bus =
      media::AudioBus::Create(params.channels(), params.frames_per_buffer());
  data_bus->Zero();

  auto check_audio_length = [&](const media::AudioBus& processed_audio,
                                base::TimeTicks, std::optional<double>) {
    EXPECT_EQ(processed_audio.frames(), output_sample_rate * 10 / 1000);
  };

  // 35 ms of audio --> 3 chunks of 10 ms, and 5 ms left in the processor.
  EXPECT_CALL(mock_capture_callback, Run(_, _, _))
      .Times(3)
      .WillRepeatedly(check_audio_length);
  audio_processor->ProcessCapturedAudio(*data_bus, base::TimeTicks::Now(), -1,
                                        1.0, false);
  // 5 + 35 ms of audio --> 4 chunks of 10 ms.
  EXPECT_CALL(mock_capture_callback, Run(_, _, _))
      .Times(4)
      .WillRepeatedly(check_audio_length);
  audio_processor->ProcessCapturedAudio(*data_bus, base::TimeTicks::Now(), -1,
                                        1.0, false);

  audio_processor->Stop();
}

// When no audio processing is performed, audio is delivered immediately. Note
// that unlike the other cases, unprocessed audio input of less than 10 ms is
// forwarded directly instead of collecting chunks of 10 ms.
TEST(MediaStreamAudioProcessorCallbackTest,
     UnprocessedAudioIsDeliveredImmediatelyWithShortBuffers) {
  test::TaskEnvironment task_environment_;
  MockProcessedCaptureCallback mock_capture_callback;
  blink::AudioProcessingProperties properties;
  properties.DisableDefaultProperties();
  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new rtc::RefCountedObject<WebRtcAudioDeviceImpl>());
  // Set buffer size to 4 ms.
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::ChannelLayoutConfig::Stereo(), 48000,
                                48000 * 4 / 1000);
  scoped_refptr<MediaStreamAudioProcessor> audio_processor(
      new rtc::RefCountedObject<MediaStreamAudioProcessor>(
          mock_capture_callback.Get(),
          properties.ToAudioProcessingSettings(
              /*multi_channel_capture_processing=*/true),
          params, webrtc_audio_device));
  ASSERT_FALSE(audio_processor->has_webrtc_audio_processing());

  int output_sample_rate = audio_processor->output_format().sample_rate();
  std::unique_ptr<media::AudioBus> data_bus =
      media::AudioBus::Create(params.channels(), params.frames_per_buffer());
  data_bus->Zero();

  auto check_audio_length = [&](const media::AudioBus& processed_audio,
                                base::TimeTicks, std::optional<double>) {
    EXPECT_EQ(processed_audio.frames(), output_sample_rate * 4 / 1000);
  };

  EXPECT_CALL(mock_capture_callback, Run(_, _, _))
      .Times(1)
      .WillOnce(check_audio_length);
  audio_processor->ProcessCapturedAudio(*data_bus, base::TimeTicks::Now(), -1,
                                        1.0, false);
  EXPECT_CALL(mock_capture_callback, Run(_, _, _))
      .Times(1)
      .WillOnce(check_audio_length);
  audio_processor->ProcessCapturedAudio(*data_bus, base::TimeTicks::Now(), -1,
                                        1.0, false);

  audio_processor->Stop();
}

// When no audio processing is performed, audio is delivered immediately. Chunks
// greater than 10 ms are delivered in chunks of 10 ms.
TEST(MediaStreamAudioProcessorCallbackTest,
     UnprocessedAudioIsDeliveredImmediatelyWithLongBuffers) {
  test::TaskEnvironment task_environment_;
  MockProcessedCaptureCallback mock_capture_callback;
  blink::AudioProcessingProperties properties;
  properties.DisableDefaultProperties();
  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new rtc::RefCountedObject<WebRtcAudioDeviceImpl>());
  // Set buffer size to 35 ms.
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::ChannelLayoutConfig::Stereo(), 48000,
                                48000 * 35 / 1000);
  scoped_refptr<MediaStreamAudioProcessor> audio_processor(
      new rtc::RefCountedObject<MediaStreamAudioProcessor>(
          mock_capture_callback.Get(),
          properties.ToAudioProcessingSettings(
              /*multi_channel_capture_processing=*/true),
          params, webrtc_audio_device));
  ASSERT_FALSE(audio_processor->has_webrtc_audio_processing());

  int output_sample_rate = audio_processor->output_format().sample_rate();
  std::unique_ptr<media::AudioBus> data_bus =
      media::AudioBus::Create(params.channels(), params.frames_per_buffer());
  data_bus->Zero();

  auto check_audio_length = [&](const media::AudioBus& processed_audio,
                                base::TimeTicks, std::optional<double>) {
    EXPECT_EQ(processed_audio.frames(), output_sample_rate * 10 / 1000);
  };

  // 35 ms of audio --> 3 chunks of 10 ms, and 5 ms left in the processor.
  EXPECT_CALL(mock_capture_callback, Run(_, _, _))
      .Times(3)
      .WillRepeatedly(check_audio_length);
  audio_processor->ProcessCapturedAudio(*data_bus, base::TimeTicks::Now(), -1,
                                        1.0, false);
  // 5 + 35 ms of audio --> 4 chunks of 10 ms.
  EXPECT_CALL(mock_capture_callback, Run(_, _, _))
      .Times(4)
      .WillRepeatedly(check_audio_length);
  audio_processor->ProcessCapturedAudio(*data_bus, base::TimeTicks::Now(), -1,
                                        1.0, false);

  audio_processor->Stop();
}

namespace {
scoped_refptr<MediaStreamAudioProcessor> CreateAudioProcessorWithProperties(
    AudioProcessingProperties properties) {
  MockProcessedCaptureCallback mock_capture_callback;
  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new rtc::RefCountedObject<WebRtcAudioDeviceImpl>());
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::ChannelLayoutConfig::Stereo(), 48000,
                                480);
  scoped_refptr<MediaStreamAudioProcessor> audio_processor(
      new rtc::RefCountedObject<MediaStreamAudioProcessor>(
          mock_capture_callback.Get(),
          properties.ToAudioProcessingSettings(
              /*multi_channel_capture_processing=*/true),
          params, webrtc_audio_device));
  return audio_processor;
}
}  // namespace

TEST(MediaStreamAudioProcessorWouldModifyAudioTest, TrueByDefault) {
  test::TaskEnvironment task_environment;
  blink::AudioProcessingProperties properties;
  EXPECT_TRUE(MediaStreamAudioProcessor::WouldModifyAudio(properties));

  scoped_refptr<MediaStreamAudioProcessor> audio_processor =
      CreateAudioProcessorWithProperties(properties);
  EXPECT_TRUE(audio_processor->has_webrtc_audio_processing());
}

TEST(MediaStreamAudioProcessorWouldModifyAudioTest,
     FalseWhenEverythingIsDisabled) {
  test::TaskEnvironment task_environment_;
  blink::AudioProcessingProperties properties;
  properties.DisableDefaultProperties();
  EXPECT_FALSE(MediaStreamAudioProcessor::WouldModifyAudio(properties));

  scoped_refptr<MediaStreamAudioProcessor> audio_processor =
      CreateAudioProcessorWithProperties(properties);
  EXPECT_FALSE(audio_processor->has_webrtc_audio_processing());
}

TEST(MediaStreamAudioProcessorWouldModifyAudioTest,
     FalseWhenOnlyHardwareEffectsAreUsed) {
  test::TaskEnvironment task_environment_;
  blink::AudioProcessingProperties properties;
  properties.DisableDefaultProperties();
  properties.echo_cancellation_type =
      AudioProcessingProperties::EchoCancellationType::kEchoCancellationSystem;
  properties.system_gain_control_activated = true;
  properties.system_noise_suppression_activated = true;
  EXPECT_FALSE(MediaStreamAudioProcessor::WouldModifyAudio(properties));

  scoped_refptr<MediaStreamAudioProcessor> audio_processor =
      CreateAudioProcessorWithProperties(properties);
  EXPECT_FALSE(audio_processor->has_webrtc_audio_processing());
}

#if BUILDFLAG(IS_IOS)
// TODO(https://crbug.com/1417474): Remove legacy iOS case in
// AudioProcessingSettings::NeedWebrtcAudioProcessing().
#define MAYBE_TrueWhenSoftwareEchoCancellationIsEnabled \
  DISABLED_TrueWhenSoftwareEchoCancellationIsEnabled
#else
#define MAYBE_TrueWhenSoftwareEchoCancellationIsEnabled \
  TrueWhenSoftwareEchoCancellationIsEnabled
#endif  // BUILDFLAG(IS_IOS)
TEST(MediaStreamAudioProcessorWouldModifyAudioTest,
     MAYBE_TrueWhenSoftwareEchoCancellationIsEnabled) {
  test::TaskEnvironment task_environment_;
  blink::AudioProcessingProperties properties;
  properties.DisableDefaultProperties();
  properties.echo_cancellation_type =
      AudioProcessingProperties::EchoCancellationType::kEchoCancellationAec3;
  // WouldModifyAudio overrides this effect on iOS, but not the audio processor.
  // TODO(https://crbug.com/1269364): Make these functions behave consistently.
#if !BUILDFLAG(IS_IOS)
  EXPECT_TRUE(MediaStreamAudioProcessor::WouldModifyAudio(properties));
#else
  EXPECT_FALSE(MediaStreamAudioProcessor::WouldModifyAudio(properties));
#endif

  scoped_refptr<MediaStreamAudioProcessor> audio_processor =
      CreateAudioProcessorWithProperties(properties);
  EXPECT_TRUE(audio_processor->has_webrtc_audio_processing());
}

#if BUILDFLAG(IS_IOS)
// TODO(https://crbug.com/1417474): Remove legacy iOS case in
// AudioProcessingSettings::NeedWebrtcAudioProcessing().
#define MAYBE_TrueWhenStereoMirroringIsEnabled \
  DISABLED_TrueWhenStereoMirroringIsEnabled
#else
#define MAYBE_TrueWhenStereoMirroringIsEnabled TrueWhenStereoMirroringIsEnabled
#endif  // BUILDFLAG(IS_IOS)
TEST(MediaStreamAudioProcessorWouldModifyAudioTest,
     MAYBE_TrueWhenStereoMirroringIsEnabled) {
  test::TaskEnvironment task_environment_;
  blink::AudioProcessingProperties properties;
  properties.DisableDefaultProperties();
  properties.goog_audio_mirroring = true;
  EXPECT_TRUE(MediaStreamAudioProcessor::WouldModifyAudio(properties));

  // Unlike other settings, audio mirroring is not a WebRTC effect.
  // This is really an implementation detail of the audio processor, not part of
  // the API, but it is necessary to have test coverage to avoid inconsistencies
  // between the static MSAP::WouldModifyAudio and the internals of the audio
  // processor.
  scoped_refptr<MediaStreamAudioProcessor> audio_processor =
      CreateAudioProcessorWithProperties(properties);
  EXPECT_FALSE(audio_processor->has_webrtc_audio_processing());
}

#if BUILDFLAG(IS_IOS)
// TODO(https://crbug.com/1417474): Remove legacy iOS case in
// AudioProcessingSettings::NeedWebrtcAudioProcessing().
#define MAYBE_TrueWhenGainControlIsEnabled DISABLED_TrueWhenGainControlIsEnabled
#else
#define MAYBE_TrueWhenGainControlIsEnabled TrueWhenGainControlIsEnabled
#endif  // BUILDFLAG(IS_IOS)
TEST(MediaStreamAudioProcessorWouldModifyAudioTest,
     MAYBE_TrueWhenGainControlIsEnabled) {
  test::TaskEnvironment task_environment_;
  blink::AudioProcessingProperties properties;
  properties.DisableDefaultProperties();
  properties.goog_auto_gain_control = true;
  // WouldModifyAudio overrides this effect on iOS, but not the audio processor.
  // TODO(https://crbug.com/1269364): Make these functions behave consistently.
#if !BUILDFLAG(IS_IOS)
  EXPECT_TRUE(MediaStreamAudioProcessor::WouldModifyAudio(properties));
#else
  EXPECT_FALSE(MediaStreamAudioProcessor::WouldModifyAudio(properties));
#endif

  scoped_refptr<MediaStreamAudioProcessor> audio_processor =
      CreateAudioProcessorWithProperties(properties);
  EXPECT_TRUE(audio_processor->has_webrtc_audio_processing());
}

#if BUILDFLAG(IS_IOS)
// TODO(https://crbug.com/1417474): Remove legacy iOS case in
// AudioProcessingSettings::NeedWebrtcAudioProcessing().
#define MAYBE_TrueWhenExperimentalEchoCancellationIsEnabled \
  DISABLED_TrueWhenExperimentalEchoCancellationIsEnabled
#else
#define MAYBE_TrueWhenExperimentalEchoCancellationIsEnabled \
  TrueWhenExperimentalEchoCancellationIsEnabled
#endif  // BUILDFLAG(IS_IOS)
// "Experimental echo cancellation" does not map to any real effect, but still
// enables audio processing.
// TODO(https://crbug.com/1269723): Remove the experimental AEC option. This
// test documents *current* behavior, not *desired* behavior.
TEST(MediaStreamAudioProcessorWouldModifyAudioTest,
     MAYBE_TrueWhenExperimentalEchoCancellationIsEnabled) {
  test::TaskEnvironment task_environment_;
  blink::AudioProcessingProperties properties;
  properties.DisableDefaultProperties();
  properties.goog_experimental_echo_cancellation = true;
  // WouldModifyAudio overrides this effect on iOS and Android.
#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(MediaStreamAudioProcessor::WouldModifyAudio(properties));
#else
  EXPECT_FALSE(MediaStreamAudioProcessor::WouldModifyAudio(properties));
#endif

  scoped_refptr<MediaStreamAudioProcessor> audio_processor =
      CreateAudioProcessorWithProperties(properties);
  // WouldModifyAudio overrides this effect on iOS and Android.
#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(audio_processor->has_webrtc_audio_processing());
#else
  EXPECT_FALSE(audio_processor->has_webrtc_audio_processing());
#endif
}

TEST(MediaStreamAudioProcessorWouldModifyAudioTest,
     TrueWhenNoiseSuppressionIsEnabled) {
  test::TaskEnvironment task_environment_;
  blink::AudioProcessingProperties properties;
  properties.DisableDefaultProperties();
  properties.goog_noise_suppression = true;
  EXPECT_TRUE(MediaStreamAudioProcessor::WouldModifyAudio(properties));

  scoped_refptr<MediaStreamAudioProcessor> audio_processor =
      CreateAudioProcessorWithProperties(properties);
  EXPECT_TRUE(audio_processor->has_webrtc_audio_processing());
}

TEST(MediaStreamAudioProcessorWouldModifyAudioTest,
     TrueWhenExperimentalNoiseSuppression) {
  test::TaskEnvironment task_environment_;
  blink::AudioProcessingProperties properties;
  properties.DisableDefaultProperties();
  properties.goog_experimental_noise_suppression = true;
  EXPECT_TRUE(MediaStreamAudioProcessor::WouldModifyAudio(properties));

  scoped_refptr<MediaStreamAudioProcessor> audio_processor =
      CreateAudioProcessorWithProperties(properties);
  EXPECT_TRUE(audio_processor->has_webrtc_audio_processing());
}

TEST(MediaStreamAudioProcessorWouldModifyAudioTest,
     TrueWhenHighpassFilterIsEnabled) {
  test::TaskEnvironment task_environment_;
  blink::AudioProcessingProperties properties;
  properties.DisableDefaultProperties();
  properties.goog_highpass_filter = true;
  EXPECT_TRUE(MediaStreamAudioProcessor::WouldModifyAudio(properties));

  scoped_refptr<MediaStreamAudioProcessor> audio_processor =
      CreateAudioProcessorWithProperties(properties);
  EXPECT_TRUE(audio_processor->has_webrtc_audio_processing());
}

}  // namespace blink
