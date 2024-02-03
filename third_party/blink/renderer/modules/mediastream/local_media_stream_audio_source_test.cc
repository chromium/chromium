#include "third_party/blink/renderer/modules/mediastream/local_media_stream_audio_source.h"

#include <memory>

#include "media/base/audio_parameters.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {
namespace {

enum class SystemAec { kNotSupported, kSupported, kExperimentallySupported };

// Creates an audio source from a device with AEC support specified by
// |aec_mode| and requested AEC effect specified by |enable_system_aec|.
std::unique_ptr<LocalMediaStreamAudioSource> CreateLocalMediaStreamAudioSource(
    SystemAec aec_mode,
    bool enable_system_aec) {
  MediaStreamDevice device{};
  device.input =
      media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                             media::ChannelLayoutConfig::Stereo(), 48000, 512);
  if (aec_mode == SystemAec::kSupported) {
    device.input.set_effects(media::AudioParameters::ECHO_CANCELLER);
  } else if (aec_mode == SystemAec::kExperimentallySupported) {
    device.input.set_effects(
        media::AudioParameters::EXPERIMENTAL_ECHO_CANCELLER);
  }
  return std::make_unique<LocalMediaStreamAudioSource>(
      /*consumer_frame*/ nullptr, device,
      /*requested_local_buffer_size*/ nullptr,
      /*disable_local_echo*/ false, enable_system_aec,
      LocalMediaStreamAudioSource::ConstraintsRepeatingCallback(),
      blink::scheduler::GetSingleThreadTaskRunnerForTesting());
}

TEST(LocalMediaStreamAudioSourceAecTest, SupportsUnsupportedSystemAec) {
  test::TaskEnvironment task_environment;
  std::unique_ptr<LocalMediaStreamAudioSource> source =
      CreateLocalMediaStreamAudioSource(SystemAec::kNotSupported,
                                        /*enable_system_aec*/ false);
  std::optional<AudioProcessingProperties> properties =
      source->GetAudioProcessingProperties();
  ASSERT_TRUE(properties.has_value());

  EXPECT_EQ(properties->echo_cancellation_type,
            AudioProcessingProperties::EchoCancellationType::
                kEchoCancellationDisabled);
  EXPECT_FALSE(source->GetAudioParameters().effects() &
               media::AudioParameters::ECHO_CANCELLER);
}

TEST(LocalMediaStreamAudioSourceAecTest, CanDisableSystemAec) {
  test::TaskEnvironment task_environment;
  std::unique_ptr<LocalMediaStreamAudioSource> source =
      CreateLocalMediaStreamAudioSource(SystemAec::kSupported,
                                        /*enable_system_aec*/ false);
  std::optional<AudioProcessingProperties> properties =
      source->GetAudioProcessingProperties();
  ASSERT_TRUE(properties.has_value());

  EXPECT_EQ(properties->echo_cancellation_type,
            AudioProcessingProperties::EchoCancellationType::
                kEchoCancellationDisabled);
  EXPECT_FALSE(source->GetAudioParameters().effects() &
               media::AudioParameters::ECHO_CANCELLER);
}

TEST(LocalMediaStreamAudioSourceAecTest, CanDisableExperimentalSystemAec) {
  test::TaskEnvironment task_environment;
  std::unique_ptr<LocalMediaStreamAudioSource> source =
      CreateLocalMediaStreamAudioSource(SystemAec::kExperimentallySupported,
                                        /*enable_system_aec*/ false);
  std::optional<AudioProcessingProperties> properties =
      source->GetAudioProcessingProperties();
  ASSERT_TRUE(properties.has_value());

  EXPECT_EQ(properties->echo_cancellation_type,
            AudioProcessingProperties::EchoCancellationType::
                kEchoCancellationDisabled);
  EXPECT_FALSE(source->GetAudioParameters().effects() &
               media::AudioParameters::ECHO_CANCELLER);
}

TEST(LocalMediaStreamAudioSourceAecTest, CanEnableSystemAec) {
  test::TaskEnvironment task_environment;
  std::unique_ptr<LocalMediaStreamAudioSource> source =
      CreateLocalMediaStreamAudioSource(SystemAec::kSupported,
                                        /*enable_system_aec*/ true);
  std::optional<AudioProcessingProperties> properties =
      source->GetAudioProcessingProperties();
  ASSERT_TRUE(properties.has_value());

  EXPECT_EQ(
      properties->echo_cancellation_type,
      AudioProcessingProperties::EchoCancellationType::kEchoCancellationSystem);
  EXPECT_TRUE(source->GetAudioParameters().effects() &
              media::AudioParameters::ECHO_CANCELLER);
}

TEST(LocalMediaStreamAudioSourceAecTest, CanEnableExperimentalSystemAec) {
  test::TaskEnvironment task_environment;
  std::unique_ptr<LocalMediaStreamAudioSource> source =
      CreateLocalMediaStreamAudioSource(SystemAec::kExperimentallySupported,
                                        /*enable_system_aec*/ true);
  std::optional<AudioProcessingProperties> properties =
      source->GetAudioProcessingProperties();
  ASSERT_TRUE(properties.has_value());

  EXPECT_EQ(
      properties->echo_cancellation_type,
      AudioProcessingProperties::EchoCancellationType::kEchoCancellationSystem);
  EXPECT_TRUE(source->GetAudioParameters().effects() &
              media::AudioParameters::ECHO_CANCELLER);
}

}  // namespace
}  // namespace blink
