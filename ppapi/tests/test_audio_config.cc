// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ppapi/tests/test_audio_config.h"

#include <stddef.h>
#include <stdint.h>

#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/cpp/module.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(AudioConfig);

bool TestAudioConfig::Init() {
  audio_config_interface_ = static_cast<const PPB_AudioConfig*>(
      pp::Module::Get()->GetBrowserInterface(PPB_AUDIO_CONFIG_INTERFACE));
  core_interface_ = static_cast<const PPB_Core*>(
      pp::Module::Get()->GetBrowserInterface(PPB_CORE_INTERFACE));
  return audio_config_interface_ && core_interface_;
}

void TestAudioConfig::RunTests(const std::string& filter) {
  RUN_TEST(RecommendSampleRate, filter);
  RUN_TEST(ValidConfigs, filter);
  RUN_TEST(InvalidConfigs, filter);
}

std::string TestAudioConfig::TestRecommendSampleRate() {
  // Ask PPB_AudioConfig about the recommended sample rate.
  PP_AudioSampleRate sample_rate = audio_config_interface_->RecommendSampleRate(
      instance_->pp_instance());
  ASSERT_TRUE(sample_rate == PP_AUDIOSAMPLERATE_NONE ||
              sample_rate == PP_AUDIOSAMPLERATE_44100 ||
              sample_rate == PP_AUDIOSAMPLERATE_48000);

  PASS();
}

std::string TestAudioConfig::TestValidConfigs() {
  static const PP_AudioSampleRate kSampleRates[] = {
    PP_AUDIOSAMPLERATE_44100,
    PP_AUDIOSAMPLERATE_48000
  };
  static const uint32_t kRequestFrameCounts[] = {
    PP_AUDIOMINSAMPLEFRAMECOUNT,
    PP_AUDIOMAXSAMPLEFRAMECOUNT,
    // Include some "okay-looking" frame counts; check their validity below.
    1024,
    2048,
    4096
  };

  for (size_t i = 0; i < sizeof(kSampleRates)/sizeof(kSampleRates[0]); i++) {
    PP_AudioSampleRate sample_rate = kSampleRates[i];

    for (size_t j = 0;
         j < sizeof(kRequestFrameCounts)/sizeof(kRequestFrameCounts[0]);
         j++) {
      uint32_t request_frame_count = kRequestFrameCounts[j];
      ASSERT_TRUE(request_frame_count >= PP_AUDIOMINSAMPLEFRAMECOUNT);
      ASSERT_TRUE(request_frame_count <= PP_AUDIOMAXSAMPLEFRAMECOUNT);

      uint32_t frame_count = audio_config_interface_->RecommendSampleFrameCount(
          instance_->pp_instance(), sample_rate, request_frame_count);
      ASSERT_TRUE(frame_count >= PP_AUDIOMINSAMPLEFRAMECOUNT);
      ASSERT_TRUE(frame_count <= PP_AUDIOMAXSAMPLEFRAMECOUNT);

      PP_Resource ac = audio_config_interface_->CreateStereo16Bit(
          instance_->pp_instance(), sample_rate, frame_count);
      ASSERT_TRUE(ac);
      ASSERT_TRUE(audio_config_interface_->IsAudioConfig(ac));
      ASSERT_EQ(sample_rate, audio_config_interface_->GetSampleRate(ac));
      ASSERT_EQ(frame_count, audio_config_interface_->GetSampleFrameCount(ac));

      core_interface_->ReleaseResource(ac);
    }
  }

  PASS();
}

std::string TestAudioConfig::TestInvalidConfigs() {
  // |PP_AUDIOSAMPLERATE_NONE| is not a valid rate, so this should fail.
  PP_Resource ac = audio_config_interface_->CreateStereo16Bit(
      instance_->pp_instance(),
      PP_AUDIOSAMPLERATE_NONE,
      PP_AUDIOMINSAMPLEFRAMECOUNT);
  ASSERT_EQ(0, ac);

  // Test invalid frame counts.
  ASSERT_TRUE(PP_AUDIOMINSAMPLEFRAMECOUNT >= 1);
  ac = audio_config_interface_->CreateStereo16Bit(
      instance_->pp_instance(),
      PP_AUDIOSAMPLERATE_44100,
      PP_AUDIOMINSAMPLEFRAMECOUNT - 1u);
  ASSERT_EQ(0, ac);
  ac = audio_config_interface_->CreateStereo16Bit(
      instance_->pp_instance(),
      PP_AUDIOSAMPLERATE_44100,
      PP_AUDIOMAXSAMPLEFRAMECOUNT + 1u);
  ASSERT_EQ(0, ac);

  // Test rest of API whose failure cases are defined.
  ASSERT_FALSE(audio_config_interface_->IsAudioConfig(0));
  ASSERT_EQ(PP_AUDIOSAMPLERATE_NONE, audio_config_interface_->GetSampleRate(0));
  ASSERT_EQ(0u, audio_config_interface_->GetSampleFrameCount(0));

  PASS();
}
