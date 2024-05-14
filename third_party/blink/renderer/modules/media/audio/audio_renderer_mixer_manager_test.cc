// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media/audio/audio_renderer_mixer_manager.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_parameters.h"
#include "media/base/fake_audio_render_callback.h"
#include "media/base/mock_audio_renderer_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/modules/media/audio/audio_renderer_mixer.h"
#include "third_party/blink/renderer/modules/media/audio/audio_renderer_mixer_input.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {
constexpr int kSampleRate = 48000;
constexpr int kBufferSize = 8192;
constexpr int kHardwareSampleRate = 44100;
constexpr int kHardwareBufferSize = 128;
constexpr media::ChannelLayout kChannelLayout = media::CHANNEL_LAYOUT_STEREO;
constexpr media::ChannelLayout kAnotherChannelLayout =
    media::CHANNEL_LAYOUT_2_1;
const char* const kDefaultDeviceId =
    media::AudioDeviceDescription::kDefaultDeviceId;
constexpr char kAnotherDeviceId[] = "another-device-id";
constexpr char kMatchedDeviceId[] = "matched-device-id";
constexpr char kNonexistentDeviceId[] = "nonexistent-device-id";

const LocalFrameToken kLocalFrameToken;
const FrameToken kFrameToken;
const FrameToken kAnotherFrameToken;
}  // namespace

using media::AudioLatency;
using media::AudioParameters;

class AudioRendererMixerManagerTest : public testing::Test {
 public:
  AudioRendererMixerManagerTest()
      : manager_(new AudioRendererMixerManager(
            base::BindRepeating(&AudioRendererMixerManagerTest::GetPlainSink,
                                base::Unretained(this)))) {}

  AudioRendererMixerManagerTest(const AudioRendererMixerManagerTest&) = delete;
  AudioRendererMixerManagerTest& operator=(
      const AudioRendererMixerManagerTest&) = delete;

  scoped_refptr<media::MockAudioRendererSink> CreateNormalSink(
      const std::string& device_id = std::string(kDefaultDeviceId)) {
    auto sink = base::MakeRefCounted<media::MockAudioRendererSink>(
        device_id, media::OUTPUT_DEVICE_STATUS_OK,
        AudioParameters(
            AudioParameters::AUDIO_PCM_LINEAR,
            media::ChannelLayoutConfig::FromLayout<kChannelLayout>(),
            kHardwareSampleRate, kHardwareBufferSize));
    EXPECT_CALL(*sink, Stop()).Times(1);
    return sink;
  }

  scoped_refptr<media::MockAudioRendererSink> CreateNoDeviceSink() {
    return base::MakeRefCounted<media::MockAudioRendererSink>(
        kNonexistentDeviceId, media::OUTPUT_DEVICE_STATUS_ERROR_NOT_FOUND);
  }

  scoped_refptr<media::MockAudioRendererSink> CreateMatchedDeviceSink() {
    auto sink = base::MakeRefCounted<media::MockAudioRendererSink>(
        kMatchedDeviceId, media::OUTPUT_DEVICE_STATUS_OK);
    EXPECT_CALL(*sink, Stop()).Times(1);
    return sink;
  }

  enum class SinkUseState { kExistingSink, kNewSink };
  AudioRendererMixer* GetMixer(const LocalFrameToken& source_frame_token,
                               const FrameToken& main_frame_token,
                               const media::AudioParameters& params,
                               AudioLatency::Type latency,
                               const std::string& device_id,
                               SinkUseState sink_state) {
    auto sink = GetSink(
        source_frame_token,
        media::AudioSinkParameters(base::UnguessableToken(), device_id));
    auto device_info = sink->GetOutputDeviceInfo();
    if (sink_state == SinkUseState::kNewSink) {
      EXPECT_CALL(*sink, Start()).Times(1);
    }
    return manager_->GetMixer(main_frame_token, params, latency, device_info,
                              std::move(sink));
  }

  void ReturnMixer(AudioRendererMixer* mixer) {
    return manager_->ReturnMixer(mixer);
  }

  scoped_refptr<AudioRendererMixerInput> CreateInputHelper(
      const LocalFrameToken& source_frame_token,
      const FrameToken& main_frame_token,
      const base::UnguessableToken& session_id,
      const std::string& device_id,
      media::AudioLatency::Type latency,
      const media::AudioParameters params,
      media::AudioRendererSink::RenderCallback* callback) {
    auto input = manager_->CreateInput(source_frame_token, main_frame_token,
                                       session_id, device_id, latency);
    input->GetOutputDeviceInfoAsync(
        base::DoNothing());  // Primes input, needed for tests.
    base::RunLoop().RunUntilIdle();
    input->Initialize(params, callback);
    return input;
  }

  // Number of instantiated mixers.
  size_t mixer_count() { return manager_->mixers_.size(); }
  size_t dead_mixer_count() { return manager_->dead_mixers_.size(); }

 protected:
  scoped_refptr<media::MockAudioRendererSink> GetSink(
      const LocalFrameToken& source_frame_token,
      const media::AudioSinkParameters& params) {
    if ((params.device_id == kDefaultDeviceId) ||
        (params.device_id == kAnotherDeviceId)) {
      return mock_sink_ ? std::move(mock_sink_)
                        : CreateNormalSink(params.device_id);
    }
    if (params.device_id == kNonexistentDeviceId) {
      return CreateNoDeviceSink();
    }
    if (params.device_id.empty()) {
      // The sink used to get device ID from session ID if it's not empty
      return params.session_id
                 ? CreateMatchedDeviceSink()
                 : (mock_sink_ ? std::move(mock_sink_)
                               : CreateNormalSink(params.device_id));
    }
    if (params.device_id == kMatchedDeviceId) {
      return CreateMatchedDeviceSink();
    }

    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }

  test::TaskEnvironment task_environment_;
  std::unique_ptr<AudioRendererMixerManager> manager_;
  scoped_refptr<media::MockAudioRendererSink> mock_sink_;

 private:
  scoped_refptr<media::AudioRendererSink> GetPlainSink(
      const LocalFrameToken& source_frame_token,
      const media::AudioSinkParameters& params) {
    return GetSink(source_frame_token, params);
  }
};

// Verify GetMixer() and ReturnMixer() both work as expected; particularly with
// respect to the explicit ref counting done.
TEST_F(AudioRendererMixerManagerTest, GetReturnMixer) {
  // There should be no mixers outstanding to start with.
  EXPECT_EQ(0u, mixer_count());

  media::AudioParameters params1(
      media::AudioParameters::AUDIO_PCM_LINEAR,
      media::ChannelLayoutConfig::FromLayout<kChannelLayout>(), kSampleRate,
      kBufferSize);

  AudioRendererMixer* mixer1 = GetMixer(
      kLocalFrameToken, kFrameToken, params1, AudioLatency::Type::kPlayback,
      kDefaultDeviceId, SinkUseState::kNewSink);
  ASSERT_TRUE(mixer1);
  EXPECT_EQ(1u, mixer_count());

  // The same parameters should return the same mixer1.
  EXPECT_EQ(mixer1, GetMixer(kLocalFrameToken, kFrameToken, params1,
                             AudioLatency::Type::kPlayback, kDefaultDeviceId,
                             SinkUseState::kExistingSink));
  EXPECT_EQ(1u, mixer_count());

  // Return the extra mixer we just acquired.
  ReturnMixer(mixer1);
  EXPECT_EQ(1u, mixer_count());

  media::AudioParameters params2(
      AudioParameters::AUDIO_PCM_LINEAR,
      media::ChannelLayoutConfig::FromLayout<kAnotherChannelLayout>(),
      kSampleRate * 2, kBufferSize * 2);
  AudioRendererMixer* mixer2 = GetMixer(
      kLocalFrameToken, kFrameToken, params2, AudioLatency::Type::kPlayback,
      kDefaultDeviceId, SinkUseState::kNewSink);
  ASSERT_TRUE(mixer2);
  EXPECT_EQ(2u, mixer_count());

  // Different parameters should result in a different mixer1.
  EXPECT_NE(mixer1, mixer2);

  // Return both outstanding mixers.
  ReturnMixer(mixer1);
  EXPECT_EQ(1u, mixer_count());
  ReturnMixer(mixer2);
  EXPECT_EQ(0u, mixer_count());
}

TEST_F(AudioRendererMixerManagerTest, ReturnMixerWithError) {
  mock_sink_ = CreateNormalSink();
  auto* local_sink = mock_sink_.get();

  // There should be no mixers outstanding to start with.
  EXPECT_EQ(0u, mixer_count());

  media::AudioParameters params1(
      media::AudioParameters::AUDIO_PCM_LINEAR,
      media::ChannelLayoutConfig::FromLayout<kChannelLayout>(), kSampleRate,
      kBufferSize);

  AudioRendererMixer* mixer1 = GetMixer(
      kLocalFrameToken, kFrameToken, params1, AudioLatency::Type::kPlayback,
      kDefaultDeviceId, SinkUseState::kNewSink);
  ASSERT_TRUE(mixer1);
  EXPECT_EQ(1u, mixer_count());

  // The same parameters should return the same mixer1.
  EXPECT_EQ(mixer1, GetMixer(kLocalFrameToken, kFrameToken, params1,
                             AudioLatency::Type::kPlayback, kDefaultDeviceId,
                             SinkUseState::kExistingSink));
  EXPECT_EQ(1u, mixer_count());

  // Trigger an error in mixer1.
  local_sink->callback()->OnRenderError();

  // Return the extra mixer we just acquired, it should not be deleted, but put
  // into the dead mixer map.
  ReturnMixer(mixer1);
  EXPECT_EQ(0u, mixer_count());
  EXPECT_EQ(1u, dead_mixer_count());

  // Using the same params should create a new mixer due to the error.
  mock_sink_ = CreateNormalSink();
  local_sink = mock_sink_.get();
  AudioRendererMixer* mixer2 = GetMixer(
      kLocalFrameToken, kFrameToken, params1, AudioLatency::Type::kPlayback,
      kDefaultDeviceId, SinkUseState::kNewSink);
  ASSERT_TRUE(mixer2);
  EXPECT_EQ(1u, mixer_count());
  EXPECT_EQ(1u, dead_mixer_count());
  EXPECT_NE(mixer1, mixer2);

  // Trigger an error in mixer2 now.
  local_sink->callback()->OnRenderError();

  // Ensure we end up with two dead mixers and not just one in this case.
  AudioRendererMixer* mixer3 = GetMixer(
      kLocalFrameToken, kFrameToken, params1, AudioLatency::Type::kPlayback,
      kDefaultDeviceId, SinkUseState::kNewSink);
  ASSERT_TRUE(mixer3);
  EXPECT_EQ(1u, mixer_count());
  EXPECT_EQ(2u, dead_mixer_count());
  EXPECT_NE(mixer1, mixer2);

  // Return all outstanding mixers.
  ReturnMixer(mixer1);
  EXPECT_EQ(1u, dead_mixer_count());
  ReturnMixer(mixer2);
  EXPECT_EQ(0u, dead_mixer_count());
  ReturnMixer(mixer3);
  EXPECT_EQ(0u, mixer_count());
}

// Verify GetMixer() correctly deduplicates mixer with irrelevant AudioParameter
// differences.
TEST_F(AudioRendererMixerManagerTest, MixerReuse) {
  EXPECT_EQ(0u, mixer_count());

  media::AudioParameters params1(
      AudioParameters::AUDIO_PCM_LINEAR,
      media::ChannelLayoutConfig::FromLayout<kChannelLayout>(), kSampleRate,
      kBufferSize);
  AudioRendererMixer* mixer1 = GetMixer(
      kLocalFrameToken, kFrameToken, params1, AudioLatency::Type::kPlayback,
      kDefaultDeviceId, SinkUseState::kNewSink);
  ASSERT_TRUE(mixer1);
  EXPECT_EQ(1u, mixer_count());

  // Different sample rates, formats, bit depths, and buffer sizes should not
  // result in a different mixer.
  media::AudioParameters params2(
      AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::FromLayout<kChannelLayout>(), kSampleRate * 2,
      kBufferSize * 2);
  AudioRendererMixer* mixer2 = GetMixer(
      kLocalFrameToken, kFrameToken, params2, AudioLatency::Type::kPlayback,
      kDefaultDeviceId, SinkUseState::kExistingSink);
  EXPECT_EQ(mixer1, mixer2);
  EXPECT_EQ(1u, mixer_count());
  ReturnMixer(mixer2);
  EXPECT_EQ(1u, mixer_count());

  // Modify some parameters that do matter: channel layout
  media::AudioParameters params3(
      AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::FromLayout<kAnotherChannelLayout>(),
      kSampleRate, kBufferSize);
  ASSERT_NE(params3.channel_layout(), params1.channel_layout());
  AudioRendererMixer* mixer3 = GetMixer(
      kLocalFrameToken, kFrameToken, params3, AudioLatency::Type::kPlayback,
      kDefaultDeviceId, SinkUseState::kNewSink);
  EXPECT_NE(mixer1, mixer3);
  EXPECT_EQ(2u, mixer_count());
  ReturnMixer(mixer3);
  EXPECT_EQ(1u, mixer_count());

  // Return final mixer.
  ReturnMixer(mixer1);
  EXPECT_EQ(0u, mixer_count());
}

// Verify CreateInput() provides AudioRendererMixerInput with the appropriate
// callbacks and they are working as expected.  Also, verify that separate
// mixers are created for separate RenderFrames, even though the
// AudioParameters are the same.
TEST_F(AudioRendererMixerManagerTest, CreateInput) {
  media::AudioParameters params(
      AudioParameters::AUDIO_PCM_LINEAR,
      media::ChannelLayoutConfig::FromLayout<kChannelLayout>(), kSampleRate,
      kBufferSize);

  // Create two mixer inputs and ensure this doesn't instantiate any mixers yet.
  EXPECT_EQ(0u, mixer_count());
  media::FakeAudioRenderCallback callback(0, kSampleRate);
  mock_sink_ = CreateNormalSink();
  EXPECT_CALL(*mock_sink_, Start()).Times(1);
  auto input = CreateInputHelper(
      kLocalFrameToken, kFrameToken, base::UnguessableToken(), kDefaultDeviceId,
      AudioLatency::Type::kPlayback, params, &callback);
  EXPECT_EQ(0u, mixer_count());
  ASSERT_EQ(mock_sink_, nullptr);  // Sink is consumed by CreateInputHelper.

  // Despite being from another local frame, this input has the same main frame
  // so should share the previously created mixer.
  media::FakeAudioRenderCallback another_callback(1, kSampleRate);
  auto another_input = CreateInputHelper(
      LocalFrameToken(), kFrameToken, base::UnguessableToken(),
      kDefaultDeviceId, AudioLatency::Type::kPlayback, params,
      &another_callback);
  EXPECT_EQ(0u, mixer_count());

  // Since this input uses a another main frame token, it should not be shared.
  media::FakeAudioRenderCallback another_callback2(1, kSampleRate);
  mock_sink_ = CreateNormalSink(kDefaultDeviceId);
  EXPECT_CALL(*mock_sink_, Start()).Times(1);
  auto another_input2 = CreateInputHelper(
      kLocalFrameToken, kAnotherFrameToken, base::UnguessableToken(),
      kDefaultDeviceId, AudioLatency::Type::kPlayback, params,
      &another_callback2);
  EXPECT_EQ(0u, mixer_count());
  ASSERT_EQ(mock_sink_, nullptr);  // Sink is consumed by CreateInputHelper.

  // Implicitly test that AudioRendererMixerInput was provided with the expected
  // callbacks needed to acquire an AudioRendererMixer and return it.
  input->Start();
  EXPECT_EQ(1u, mixer_count());
  another_input->Start();
  EXPECT_EQ(1u, mixer_count());
  another_input2->Start();
  EXPECT_EQ(2u, mixer_count());

  // Destroying the inputs should destroy the mixers.
  input->Stop();
  input = nullptr;
  EXPECT_EQ(2u, mixer_count());
  another_input->Stop();
  another_input = nullptr;
  EXPECT_EQ(1u, mixer_count());
  another_input2->Stop();
  another_input2 = nullptr;
  EXPECT_EQ(0u, mixer_count());
}

// Verify CreateInput() provided with session id creates AudioRendererMixerInput
// with the appropriate callbacks and they are working as expected.
//
// TODO(grunell): |session_id| support currently requires calling the
// synchronous GetOutputDeviceInfo call, this is not allowed. So this test is
// disabled. This should be deleted in the future, https://crbug.com/870836.
TEST_F(AudioRendererMixerManagerTest, DISABLED_CreateInputWithSessionId) {
  media::AudioParameters params(
      AudioParameters::AUDIO_PCM_LINEAR,
      media::ChannelLayoutConfig::FromLayout<kChannelLayout>(), kSampleRate,
      kBufferSize);
  media::FakeAudioRenderCallback callback(0, kSampleRate);
  EXPECT_EQ(0u, mixer_count());

  // Empty device id, zero session id;
  auto input_to_default_device = CreateInputHelper(
      kLocalFrameToken, kFrameToken, base::UnguessableToken(),  // session_id
      std::string(), AudioLatency::Type::kPlayback, params, &callback);
  EXPECT_EQ(0u, mixer_count());

  // Specific device id, zero session id;
  auto input_to_another_device = CreateInputHelper(
      kLocalFrameToken, kFrameToken, base::UnguessableToken(),  // session_id
      kMatchedDeviceId, AudioLatency::Type::kPlayback, params, &callback);
  EXPECT_EQ(0u, mixer_count());

  // Specific device id, non-zero session id (to be ignored);
  auto input_to_matched_device = CreateInputHelper(
      kLocalFrameToken, kFrameToken,
      base::UnguessableToken::Create(),  // session id
      kAnotherDeviceId, AudioLatency::Type::kPlayback, params, &callback);
  EXPECT_EQ(0u, mixer_count());

  // Empty device id, non-zero session id;
  auto input_to_matched_device_with_session_id = CreateInputHelper(
      kLocalFrameToken, kFrameToken,
      base::UnguessableToken::Create(),  // session id
      std::string(), AudioLatency::Type::kPlayback, params, &callback);
  EXPECT_EQ(0u, mixer_count());

  // Implicitly test that AudioRendererMixerInput was provided with the expected
  // callbacks needed to acquire an AudioRendererMixer and return it.
  input_to_default_device->Start();
  EXPECT_EQ(1u, mixer_count());

  input_to_another_device->Start();
  EXPECT_EQ(2u, mixer_count());

  input_to_matched_device->Start();
  EXPECT_EQ(3u, mixer_count());

  // Should go to the same device as the input above.
  input_to_matched_device_with_session_id->Start();
  EXPECT_EQ(3u, mixer_count());

  // Destroying the inputs should destroy the mixers.
  input_to_default_device->Stop();
  input_to_default_device = nullptr;
  EXPECT_EQ(2u, mixer_count());
  input_to_another_device->Stop();
  input_to_another_device = nullptr;
  EXPECT_EQ(1u, mixer_count());
  input_to_matched_device->Stop();
  input_to_matched_device = nullptr;
  EXPECT_EQ(1u, mixer_count());
  input_to_matched_device_with_session_id->Stop();
  input_to_matched_device_with_session_id = nullptr;
  EXPECT_EQ(0u, mixer_count());
}

// Verify GetMixer() correctly creates different mixers with the same
// parameters, but different device ID.
TEST_F(AudioRendererMixerManagerTest, MixerDevices) {
  EXPECT_EQ(0u, mixer_count());

  media::AudioParameters params(
      AudioParameters::AUDIO_PCM_LINEAR,
      media::ChannelLayoutConfig::FromLayout<kChannelLayout>(), kSampleRate,
      kBufferSize);
  AudioRendererMixer* mixer1 = GetMixer(
      kLocalFrameToken, kFrameToken, params, AudioLatency::Type::kPlayback,
      kDefaultDeviceId, SinkUseState::kNewSink);
  ASSERT_TRUE(mixer1);
  EXPECT_EQ(1u, mixer_count());

  AudioRendererMixer* mixer2 = GetMixer(
      kLocalFrameToken, kFrameToken, params, AudioLatency::Type::kPlayback,
      kAnotherDeviceId, SinkUseState::kNewSink);
  ASSERT_TRUE(mixer2);
  EXPECT_EQ(2u, mixer_count());
  EXPECT_NE(mixer1, mixer2);

  ReturnMixer(mixer1);
  EXPECT_EQ(1u, mixer_count());
  ReturnMixer(mixer2);
  EXPECT_EQ(0u, mixer_count());
}

// Verify GetMixer() correctly deduplicate mixers with the same
// parameters and default device ID, even if one is "" and one is "default".
TEST_F(AudioRendererMixerManagerTest, OneMixerDifferentDefaultDeviceIDs) {
  EXPECT_EQ(0u, mixer_count());

  media::AudioParameters params(
      AudioParameters::AUDIO_PCM_LINEAR,
      media::ChannelLayoutConfig::FromLayout<kChannelLayout>(), kSampleRate,
      kBufferSize);
  AudioRendererMixer* mixer1 = GetMixer(
      kLocalFrameToken, kFrameToken, params, AudioLatency::Type::kPlayback,
      kDefaultDeviceId, SinkUseState::kNewSink);
  ASSERT_TRUE(mixer1);
  EXPECT_EQ(1u, mixer_count());

  AudioRendererMixer* mixer2 = GetMixer(
      kLocalFrameToken, kFrameToken, params, AudioLatency::Type::kPlayback,
      std::string(), SinkUseState::kExistingSink);
  ASSERT_TRUE(mixer2);
  EXPECT_EQ(1u, mixer_count());
  EXPECT_EQ(mixer1, mixer2);

  ReturnMixer(mixer1);
  EXPECT_EQ(1u, mixer_count());
  ReturnMixer(mixer2);
  EXPECT_EQ(0u, mixer_count());
}

// Verify that GetMixer() correctly returns a null mixer and an appropriate
// status code when a nonexistent device is requested.
TEST_F(AudioRendererMixerManagerTest, NonexistentDevice) {
  EXPECT_EQ(0u, mixer_count());

  media::AudioParameters params(
      AudioParameters::AUDIO_PCM_LINEAR,
      media::ChannelLayoutConfig::FromLayout<kChannelLayout>(), kSampleRate,
      kBufferSize);

  auto sink = GetSink(kLocalFrameToken,
                      media::AudioSinkParameters(base::UnguessableToken(),
                                                 kNonexistentDeviceId));
  auto device_info = sink->GetOutputDeviceInfo();

  EXPECT_EQ(media::OUTPUT_DEVICE_STATUS_ERROR_NOT_FOUND,
            device_info.device_status());
  EXPECT_EQ(0u, mixer_count());
}

// Verify GetMixer() correctly deduplicate mixers basing on latency
// requirements.
TEST_F(AudioRendererMixerManagerTest, LatencyMixing) {
  EXPECT_EQ(0u, mixer_count());

  media::AudioParameters params(
      AudioParameters::AUDIO_PCM_LINEAR,
      media::ChannelLayoutConfig::FromLayout<kChannelLayout>(), kSampleRate,
      kBufferSize);
  AudioRendererMixer* mixer1 = GetMixer(
      kLocalFrameToken, kFrameToken, params, AudioLatency::Type::kPlayback,
      kDefaultDeviceId, SinkUseState::kNewSink);
  ASSERT_TRUE(mixer1);
  EXPECT_EQ(1u, mixer_count());

  AudioRendererMixer* mixer2 = GetMixer(
      kLocalFrameToken, kFrameToken, params, AudioLatency::Type::kPlayback,
      kDefaultDeviceId, SinkUseState::kExistingSink);
  ASSERT_TRUE(mixer2);
  EXPECT_EQ(mixer1, mixer2);  // Same latency => same mixer.
  EXPECT_EQ(1u, mixer_count());

  AudioRendererMixer* mixer3 =
      GetMixer(kLocalFrameToken, kFrameToken, params, AudioLatency::Type::kRtc,
               kDefaultDeviceId, SinkUseState::kNewSink);
  ASSERT_TRUE(mixer3);
  EXPECT_NE(mixer1, mixer3);
  EXPECT_EQ(2u, mixer_count());  // Another latency => another mixer.

  AudioRendererMixer* mixer4 =
      GetMixer(kLocalFrameToken, kFrameToken, params, AudioLatency::Type::kRtc,
               kDefaultDeviceId, SinkUseState::kExistingSink);
  EXPECT_EQ(mixer3, mixer4);
  EXPECT_EQ(2u, mixer_count());  // Same latency => same mixer.

  AudioRendererMixer* mixer5 = GetMixer(
      kLocalFrameToken, kFrameToken, params, AudioLatency::Type::kInteractive,
      kDefaultDeviceId, SinkUseState::kNewSink);
  ASSERT_TRUE(mixer5);
  EXPECT_EQ(3u, mixer_count());  // Another latency => another mixer.

  AudioRendererMixer* mixer6 = GetMixer(
      kLocalFrameToken, kFrameToken, params, AudioLatency::Type::kInteractive,
      kDefaultDeviceId, SinkUseState::kExistingSink);
  EXPECT_EQ(mixer5, mixer6);
  EXPECT_EQ(3u, mixer_count());  // Same latency => same mixer.

  ReturnMixer(mixer1);
  EXPECT_EQ(3u, mixer_count());
  ReturnMixer(mixer2);
  EXPECT_EQ(2u, mixer_count());
  ReturnMixer(mixer3);
  EXPECT_EQ(2u, mixer_count());
  ReturnMixer(mixer4);
  EXPECT_EQ(1u, mixer_count());
  ReturnMixer(mixer5);
  EXPECT_EQ(1u, mixer_count());
  ReturnMixer(mixer6);
  EXPECT_EQ(0u, mixer_count());
}

// Verify GetMixer() correctly deduplicate mixers basing on the effects
// requirements.
TEST_F(AudioRendererMixerManagerTest, EffectsMixing) {
  EXPECT_EQ(0u, mixer_count());

  media::AudioParameters params(
      AudioParameters::AUDIO_PCM_LINEAR,
      media::ChannelLayoutConfig::FromLayout<kChannelLayout>(), kSampleRate,
      kBufferSize);
  params.set_effects(1);
  AudioRendererMixer* mixer1 = GetMixer(
      kLocalFrameToken, kFrameToken, params, AudioLatency::Type::kPlayback,
      kDefaultDeviceId, SinkUseState::kNewSink);
  ASSERT_TRUE(mixer1);
  EXPECT_EQ(1u, mixer_count());

  AudioRendererMixer* mixer2 = GetMixer(
      kLocalFrameToken, kFrameToken, params, AudioLatency::Type::kPlayback,
      kDefaultDeviceId, SinkUseState::kExistingSink);
  ASSERT_TRUE(mixer2);
  EXPECT_EQ(mixer1, mixer2);  // Same effects => same mixer.
  EXPECT_EQ(1u, mixer_count());

  params.set_effects(2);
  AudioRendererMixer* mixer3 = GetMixer(
      kLocalFrameToken, kFrameToken, params, AudioLatency::Type::kPlayback,
      kDefaultDeviceId, SinkUseState::kNewSink);
  ASSERT_TRUE(mixer3);
  EXPECT_NE(mixer1, mixer3);
  EXPECT_EQ(2u, mixer_count());  // Another effects => another mixer.

  AudioRendererMixer* mixer4 = GetMixer(
      kLocalFrameToken, kFrameToken, params, AudioLatency::Type::kPlayback,
      kDefaultDeviceId, SinkUseState::kExistingSink);
  EXPECT_EQ(mixer3, mixer4);
  EXPECT_EQ(2u, mixer_count());  // Same effects => same mixer.

  params.set_effects(3);
  AudioRendererMixer* mixer5 = GetMixer(
      kLocalFrameToken, kFrameToken, params, AudioLatency::Type::kPlayback,
      kDefaultDeviceId, SinkUseState::kNewSink);
  ASSERT_TRUE(mixer5);
  EXPECT_EQ(3u, mixer_count());  // Another effects => another mixer.

  AudioRendererMixer* mixer6 = GetMixer(
      kLocalFrameToken, kFrameToken, params, AudioLatency::Type::kPlayback,
      kDefaultDeviceId, SinkUseState::kExistingSink);
  EXPECT_EQ(mixer5, mixer6);
  EXPECT_EQ(3u, mixer_count());  // Same effects => same mixer.

  ReturnMixer(mixer1);
  EXPECT_EQ(3u, mixer_count());
  ReturnMixer(mixer2);
  EXPECT_EQ(2u, mixer_count());
  ReturnMixer(mixer3);
  EXPECT_EQ(2u, mixer_count());
  ReturnMixer(mixer4);
  EXPECT_EQ(1u, mixer_count());
  ReturnMixer(mixer5);
  EXPECT_EQ(1u, mixer_count());
  ReturnMixer(mixer6);
  EXPECT_EQ(0u, mixer_count());
}

// Verify output bufer size of the mixer is correctly adjusted for Playback
// latency.
TEST_F(AudioRendererMixerManagerTest, MixerParamsLatencyPlayback) {
  mock_sink_ = CreateNormalSink();

  // Expecting hardware buffer size of 128 frames
  EXPECT_EQ(44100,
            mock_sink_->GetOutputDeviceInfo().output_params().sample_rate());
  // Expecting hardware buffer size of 128 frames
  EXPECT_EQ(
      128,
      mock_sink_->GetOutputDeviceInfo().output_params().frames_per_buffer());

  media::AudioParameters params(
      AudioParameters::AUDIO_PCM_LINEAR,
      media::ChannelLayoutConfig::FromLayout<kChannelLayout>(), 32000, 512);
  params.set_latency_tag(AudioLatency::Type::kPlayback);

  AudioRendererMixer* mixer =
      GetMixer(kLocalFrameToken, kFrameToken, params, params.latency_tag(),
               kDefaultDeviceId, SinkUseState::kNewSink);

  if (AudioLatency::IsResamplingPassthroughSupported(params.latency_tag())) {
    // Expecting input sample rate
    EXPECT_EQ(32000, mixer->get_output_params_for_testing().sample_rate());
#if BUILDFLAG(IS_FUCHSIA)
    // On Fuchsia use 80ms buffer.
    EXPECT_EQ(2560, mixer->get_output_params_for_testing().frames_per_buffer());
#else
    // Round up 20 ms (640) to the power of 2.
    EXPECT_EQ(1024, mixer->get_output_params_for_testing().frames_per_buffer());
#endif
  } else {
    // Expecting hardware sample rate
    EXPECT_EQ(44100, mixer->get_output_params_for_testing().sample_rate());

// 20 ms at 44100 is 882 frames per buffer.
#if BUILDFLAG(IS_WIN)
    // Round up 882 to the nearest multiple of the output buffer size (128).
    // which is 7 * 128 = 896
    EXPECT_EQ(896, mixer->get_output_params_for_testing().frames_per_buffer());
#else
    // Round up 882 to the power of 2.
    EXPECT_EQ(1024, mixer->get_output_params_for_testing().frames_per_buffer());
#endif  // BUILDFLAG(IS_WIN)
  }

  ReturnMixer(mixer);
}

// Verify output bufer size of the mixer is correctly adjusted for Playback
// latency when the device buffer size exceeds 20 ms.
TEST_F(AudioRendererMixerManagerTest,
       MixerParamsLatencyPlaybackLargeDeviceBufferSize) {
  mock_sink_ = new media::MockAudioRendererSink(
      std::string(), media::OUTPUT_DEVICE_STATUS_OK,
      AudioParameters(AudioParameters::AUDIO_PCM_LINEAR,
                      media::ChannelLayoutConfig::FromLayout<kChannelLayout>(),
                      44100, 2048));
  EXPECT_CALL(*mock_sink_, Stop()).Times(1);

  media::AudioParameters params(
      AudioParameters::AUDIO_PCM_LINEAR,
      media::ChannelLayoutConfig::FromLayout<kChannelLayout>(), 32000, 512);
  params.set_latency_tag(AudioLatency::Type::kPlayback);

  AudioRendererMixer* mixer =
      GetMixer(kLocalFrameToken, kFrameToken, params, params.latency_tag(),
               kDefaultDeviceId, SinkUseState::kNewSink);

  // 20 ms at 44100 is 882 frames per buffer.
  if (AudioLatency::IsResamplingPassthroughSupported(params.latency_tag())) {
    // Expecting input sample rate
    EXPECT_EQ(32000, mixer->get_output_params_for_testing().sample_rate());
  } else {
    // Expecting hardware sample rate
    EXPECT_EQ(44100, mixer->get_output_params_for_testing().sample_rate());
  }

#if BUILDFLAG(IS_FUCHSIA)
  // On Fuchsia the buffer is rounder to a whole numbem of audio scheduling
  // periods.
  EXPECT_EQ(2560, mixer->get_output_params_for_testing().frames_per_buffer());
#else
  // Prefer device buffer size (2048) if is larger than 20 ms buffer size.
  EXPECT_EQ(2048, mixer->get_output_params_for_testing().frames_per_buffer());
#endif

  ReturnMixer(mixer);
}

// Verify output bufer size of the mixer is correctly adjusted for Playback
// latency when output audio is fake.
TEST_F(AudioRendererMixerManagerTest, MixerParamsLatencyPlaybackFakeAudio) {
  mock_sink_ = new media::MockAudioRendererSink(
      std::string(), media::OUTPUT_DEVICE_STATUS_OK,
      AudioParameters(AudioParameters::AUDIO_FAKE,
                      media::ChannelLayoutConfig::FromLayout<kChannelLayout>(),
                      44100, 2048));
  EXPECT_CALL(*mock_sink_, Stop()).Times(1);

  media::AudioParameters params(
      AudioParameters::AUDIO_PCM_LINEAR,
      media::ChannelLayoutConfig::FromLayout<kChannelLayout>(), 32000, 512);

  AudioRendererMixer* mixer = GetMixer(
      kLocalFrameToken, kFrameToken, params, AudioLatency::Type::kPlayback,
      kDefaultDeviceId, SinkUseState::kNewSink);

  // Expecting input sample rate
  EXPECT_EQ(32000, mixer->get_output_params_for_testing().sample_rate());

  // 20 ms at 32000 is 640 frames per buffer.
#if BUILDFLAG(IS_WIN)
  // Use 20 ms buffer.
  EXPECT_EQ(640, mixer->get_output_params_for_testing().frames_per_buffer());
#elif BUILDFLAG(IS_FUCHSIA)
  // Use 80 ms buffer.
  EXPECT_EQ(2560, mixer->get_output_params_for_testing().frames_per_buffer());
#else
  // Ignore device buffer size, round up 640 to the power of 2.
  EXPECT_EQ(1024, mixer->get_output_params_for_testing().frames_per_buffer());
#endif  // BUILDFLAG(IS_WIN)

  ReturnMixer(mixer);
}

// Verify output bufer size of the mixer is correctly adjusted for RTC latency.
TEST_F(AudioRendererMixerManagerTest, MixerParamsLatencyRtc) {
  mock_sink_ = CreateNormalSink();

  // Expecting hardware buffer size of 128 frames
  EXPECT_EQ(44100,
            mock_sink_->GetOutputDeviceInfo().output_params().sample_rate());
  // Expecting hardware buffer size of 128 frames
  EXPECT_EQ(
      128,
      mock_sink_->GetOutputDeviceInfo().output_params().frames_per_buffer());

  media::AudioParameters params(
      AudioParameters::AUDIO_PCM_LINEAR,
      media::ChannelLayoutConfig::FromLayout<kChannelLayout>(), 32000, 512);
  params.set_latency_tag(AudioLatency::Type::kRtc);

  AudioRendererMixer* mixer =
      GetMixer(kLocalFrameToken, kFrameToken, params, params.latency_tag(),
               kDefaultDeviceId, SinkUseState::kNewSink);

  int output_sample_rate =
      AudioLatency::IsResamplingPassthroughSupported(params.latency_tag())
          ? 32000
          : 44100;

  EXPECT_EQ(output_sample_rate,
            mixer->get_output_params_for_testing().sample_rate());

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_APPLE) || \
    BUILDFLAG(IS_FUCHSIA)
  // Use 10 ms buffer (441 frames per buffer).
  EXPECT_EQ(output_sample_rate / 100,
            mixer->get_output_params_for_testing().frames_per_buffer());
#elif BUILDFLAG(IS_ANDROID)
  // If hardware buffer size (128) is less than 20 ms (882), use 20 ms buffer
  // (otherwise, use hardware buffer).
  EXPECT_EQ(882, mixer->get_output_params_for_testing().frames_per_buffer());
#else
  // Use hardware buffer size (128).
  EXPECT_EQ(128, mixer->get_output_params_for_testing().frames_per_buffer());
#endif

  ReturnMixer(mixer);
}

// Verify output bufer size of the mixer is correctly adjusted for RTC latency
// when output audio is fake.
TEST_F(AudioRendererMixerManagerTest, MixerParamsLatencyRtcFakeAudio) {
  mock_sink_ = new media::MockAudioRendererSink(
      std::string(), media::OUTPUT_DEVICE_STATUS_OK,
      AudioParameters(AudioParameters::AUDIO_FAKE,
                      media::ChannelLayoutConfig::FromLayout<kChannelLayout>(),
                      44100, 128));
  EXPECT_CALL(*mock_sink_, Stop()).Times(1);

  media::AudioParameters params(
      AudioParameters::AUDIO_PCM_LINEAR,
      media::ChannelLayoutConfig::FromLayout<kChannelLayout>(), 32000, 512);

  AudioRendererMixer* mixer =
      GetMixer(kLocalFrameToken, kFrameToken, params, AudioLatency::Type::kRtc,
               kDefaultDeviceId, SinkUseState::kNewSink);

  // Expecting input sample rate.
  EXPECT_EQ(32000, mixer->get_output_params_for_testing().sample_rate());

  // 10 ms at 32000 is 320 frames per buffer. Expect it on all the platforms for
  // fake audio output.
  EXPECT_EQ(320, mixer->get_output_params_for_testing().frames_per_buffer());

  ReturnMixer(mixer);
}

// Verify output bufer size of the mixer is correctly adjusted for Interactive
// latency.
TEST_F(AudioRendererMixerManagerTest, MixerParamsLatencyInteractive) {
  mock_sink_ = CreateNormalSink();

  // Expecting hardware buffer size of 128 frames
  EXPECT_EQ(44100,
            mock_sink_->GetOutputDeviceInfo().output_params().sample_rate());
  // Expecting hardware buffer size of 128 frames
  EXPECT_EQ(
      128,
      mock_sink_->GetOutputDeviceInfo().output_params().frames_per_buffer());

  media::AudioParameters params(
      AudioParameters::AUDIO_PCM_LINEAR,
      media::ChannelLayoutConfig::FromLayout<kChannelLayout>(), 32000, 512);
  params.set_latency_tag(AudioLatency::Type::kInteractive);

  AudioRendererMixer* mixer =
      GetMixer(kLocalFrameToken, kFrameToken, params, params.latency_tag(),
               kDefaultDeviceId, SinkUseState::kNewSink);

  if (AudioLatency::IsResamplingPassthroughSupported(params.latency_tag())) {
    // Expecting input sample rate.
    EXPECT_EQ(32000, mixer->get_output_params_for_testing().sample_rate());
  } else {
    // Expecting hardware sample rate.
    EXPECT_EQ(44100, mixer->get_output_params_for_testing().sample_rate());
  }

  // Expect hardware buffer size.
  EXPECT_EQ(128, mixer->get_output_params_for_testing().frames_per_buffer());

  ReturnMixer(mixer);
}

// Verify output parameters are the same as input properties for bitstream
// formats.
TEST_F(AudioRendererMixerManagerTest, MixerParamsBitstreamFormat) {
  mock_sink_ = new media::MockAudioRendererSink(
      std::string(), media::OUTPUT_DEVICE_STATUS_OK,
      AudioParameters(AudioParameters::AUDIO_PCM_LINEAR,
                      media::ChannelLayoutConfig::FromLayout<kChannelLayout>(),
                      44100, 2048));
  EXPECT_CALL(*mock_sink_, Stop()).Times(1);

  media::AudioParameters params(
      AudioParameters::AUDIO_BITSTREAM_EAC3,
      media::ChannelLayoutConfig::FromLayout<kAnotherChannelLayout>(), 32000,
      512);
  params.set_latency_tag(AudioLatency::Type::kPlayback);

  AudioRendererMixer* mixer =
      GetMixer(kLocalFrameToken, kFrameToken, params, params.latency_tag(),
               kDefaultDeviceId, SinkUseState::kNewSink);

  // Output parameters should be the same as input properties for bitstream
  // formats.
  EXPECT_EQ(params.format(), mixer->get_output_params_for_testing().format());
  EXPECT_EQ(params.channel_layout(),
            mixer->get_output_params_for_testing().channel_layout());
  EXPECT_EQ(params.sample_rate(),
            mixer->get_output_params_for_testing().sample_rate());
  EXPECT_EQ(params.frames_per_buffer(),
            mixer->get_output_params_for_testing().frames_per_buffer());

  ReturnMixer(mixer);
}

}  // namespace blink
