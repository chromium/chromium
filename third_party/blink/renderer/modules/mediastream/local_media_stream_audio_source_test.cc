#include "third_party/blink/renderer/modules/mediastream/local_media_stream_audio_source.h"

#include <memory>

#include "media/base/audio_parameters.h"
#include "media/base/media_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {
namespace {

enum class SystemAec { kNotSupported, kSupported };

int GetPlatformEffects(SystemAec aec_mode) {
  if (aec_mode == SystemAec::kSupported) {
    return media::AudioParameters::ECHO_CANCELLER;
  }
  return 0;
}

// Creates an audio source from a device with AEC support specified by
// |aec_mode| and requested AEC effect specified by |enable_system_aec|.
std::unique_ptr<LocalMediaStreamAudioSource> CreateLocalMediaStreamAudioSource(
    int platform_effects,
    bool enable_system_aec) {
  MediaStreamDevice device{};
  device.input =
      media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                             media::ChannelLayoutConfig::Stereo(), 48000, 512);
  device.input.set_effects(platform_effects);
  return std::make_unique<LocalMediaStreamAudioSource>(
      /*consumer_frame*/ nullptr, device,
      /*requested_local_buffer_size*/ nullptr,
      /*disable_local_echo*/ false,
      MediaStreamAudioProcessingLayout::MakeForUnprocessedLocalSourceForTests(
          enable_system_aec, device.input.effects()),
      LocalMediaStreamAudioSource::ConstraintsRepeatingCallback(),
      blink::scheduler::GetSingleThreadTaskRunnerForTesting());
}

TEST(LocalMediaStreamAudioSourceAecTest, SupportsUnsupportedSystemAec) {
  test::TaskEnvironment task_environment;
  int platform_effects = GetPlatformEffects(SystemAec::kNotSupported);
  std::unique_ptr<LocalMediaStreamAudioSource> source =
      CreateLocalMediaStreamAudioSource(platform_effects,
                                        /*enable_system_aec=*/false);
  std::optional<AudioProcessingProperties> properties =
      source->GetAudioProcessingProperties();
  ASSERT_TRUE(properties.has_value());

  EXPECT_FALSE(EchoCanceller::From(*properties, platform_effects).IsEnabled());
  EXPECT_FALSE(source->GetAudioParameters().effects() &
               media::AudioParameters::ECHO_CANCELLER);
}

TEST(LocalMediaStreamAudioSourceAecTest, CanDisableSystemAec) {
  test::TaskEnvironment task_environment;
  int platform_effects = GetPlatformEffects(SystemAec::kSupported);
  std::unique_ptr<LocalMediaStreamAudioSource> source =
      CreateLocalMediaStreamAudioSource(platform_effects,
                                        /*enable_system_aec=*/false);
  std::optional<AudioProcessingProperties> properties =
      source->GetAudioProcessingProperties();
  ASSERT_TRUE(properties.has_value());

  EXPECT_FALSE(EchoCanceller::From(*properties, platform_effects).IsEnabled());
  EXPECT_FALSE(source->GetAudioParameters().effects() &
               media::AudioParameters::ECHO_CANCELLER);
}

TEST(LocalMediaStreamAudioSourceAecTest, CanEnableSystemAec) {
  if (media::IsSystemLoopbackAsAecReferenceEnabled()) {
    // Platform AEC is not used if Loopback AEC is enabled, see
    // EchoCanceller::GetSystemWideAec().
    return;
  }

  test::TaskEnvironment task_environment;
  int platform_effects = GetPlatformEffects(SystemAec::kSupported);
  std::unique_ptr<LocalMediaStreamAudioSource> source =
      CreateLocalMediaStreamAudioSource(platform_effects,
                                        /*enable_system_aec=*/true);
  std::optional<AudioProcessingProperties> properties =
      source->GetAudioProcessingProperties();
  ASSERT_TRUE(properties.has_value());

  EXPECT_TRUE(
      EchoCanceller::From(*properties, platform_effects).IsPlatformProvided());
  EXPECT_TRUE(source->GetAudioParameters().effects() &
              media::AudioParameters::ECHO_CANCELLER);
}

}  // namespace
}  // namespace blink
