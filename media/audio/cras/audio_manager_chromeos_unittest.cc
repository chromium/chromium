// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "media/audio/cras/audio_manager_chromeos.h"

#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "media/audio/audio_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {
constexpr int kAecTestGroupId = 9;
constexpr int kNoAecFlaggedGroupId = 0;

// Chosen to be the same as in `audio_manager_chromeos.cc`, but any size should
// work since this should not affect the testing done herein.
constexpr int kDefaultInputBufferSize = 1024;

bool ExperimentalAecActive(const AudioParameters& params) {
  return params.effects() & AudioParameters::EXPERIMENTAL_ECHO_CANCELLER;
}

bool AecActive(const AudioParameters& params) {
  return params.effects() & AudioParameters::ECHO_CANCELLER;
}

bool NsActive(const AudioParameters& params) {
  return params.effects() & AudioParameters::NOISE_SUPPRESSION;
}

bool AgcActive(const AudioParameters& params) {
  return params.effects() & AudioParameters::AUTOMATIC_GAIN_CONTROL;
}

bool DspAecAllowed(const AudioParameters& params) {
  return params.effects() & AudioParameters::ALLOW_DSP_ECHO_CANCELLER &&
         params.effects() & AudioParameters::ECHO_CANCELLER;
}

bool DspNsAllowed(const AudioParameters& params) {
  return params.effects() & AudioParameters::ALLOW_DSP_NOISE_SUPPRESSION &&
         params.effects() & AudioParameters::NOISE_SUPPRESSION;
}

bool DspAgcAllowed(const AudioParameters& params) {
  return params.effects() & AudioParameters::ALLOW_DSP_AUTOMATIC_GAIN_CONTROL &&
         params.effects() & AudioParameters::AUTOMATIC_GAIN_CONTROL;
}

}  // namespace

class GetStreamParametersForSystem
    : public ::testing::Test,
      public ::testing::WithParamInterface<
          std::tuple<int, bool, int32_t, bool, bool>> {
 protected:
  // Retrieve test parameter values.
  void GetTestParameters() {
    user_buffer_size_ = std::get<0>(GetParam());
    system_apm_info_.aec_supported = std::get<1>(GetParam());
    system_apm_info_.aec_group_id = std::get<2>(GetParam());
    system_apm_info_.ns_supported = std::get<3>(GetParam());
    system_apm_info_.agc_supported = std::get<4>(GetParam());
  }

  AudioManagerChromeOS::SystemAudioProcessingInfo system_apm_info_;
  size_t user_buffer_size_;
};

INSTANTIATE_TEST_SUITE_P(
    AllInputParameters,
    GetStreamParametersForSystem,
    ::testing::Combine(::testing::Values(512, kDefaultInputBufferSize),
                       ::testing::Values(false, true),
                       ::testing::Values(kNoAecFlaggedGroupId, kAecTestGroupId),
                       ::testing::Values(false, true),
                       ::testing::Values(false, true)));

TEST_P(GetStreamParametersForSystem, DefaultBehavior) {
  GetTestParameters();
  AudioParameters params = AudioManagerChromeOS::GetStreamParametersForSystem(
      user_buffer_size_, system_apm_info_);

  EXPECT_TRUE(ExperimentalAecActive(params));
  EXPECT_EQ(AecActive(params), system_apm_info_.aec_supported);
  if (system_apm_info_.aec_supported) {
    EXPECT_FALSE(NsActive(params));
    EXPECT_FALSE(AgcActive(params));
  } else {
    EXPECT_FALSE(NsActive(params));
    EXPECT_FALSE(AgcActive(params));
  }
}

TEST_P(GetStreamParametersForSystem,
       BehaviorWithCrOSEnforceSystemAecDisallowed) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kCrOSSystemAEC);

  GetTestParameters();
  AudioParameters params = AudioManagerChromeOS::GetStreamParametersForSystem(
      user_buffer_size_, system_apm_info_);

  EXPECT_TRUE(ExperimentalAecActive(params));
  EXPECT_FALSE(AecActive(params));
  EXPECT_FALSE(NsActive(params));
  EXPECT_FALSE(AgcActive(params));
}

TEST_P(GetStreamParametersForSystem, BehaviorWithCrOSEnforceSystemAecNsAgc) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kCrOSEnforceSystemAecNsAgc);

  GetTestParameters();
  AudioParameters params = AudioManagerChromeOS::GetStreamParametersForSystem(
      user_buffer_size_, system_apm_info_);

  EXPECT_TRUE(ExperimentalAecActive(params));
  EXPECT_TRUE(AecActive(params));
  if (system_apm_info_.aec_supported) {
    EXPECT_FALSE(NsActive(params));
    EXPECT_FALSE(AgcActive(params));
  } else {
    EXPECT_TRUE(NsActive(params));
    EXPECT_TRUE(AgcActive(params));
  }
}

// TODO(crbug.com/1216273): DCHECKs are disabled during automated testing on
// CrOS and this test failed when tested on an experimental builder with
// DCHECKs. Revert https://crrev.com/c/2959990 to re-enable it.
// See go/chrome-dcheck-on-cros or http://crbug.com/1113456 for more details.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_P(GetStreamParametersForSystem,
       BehaviorWithCrOSEnforceSystemAecNsAndAecAgc) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kCrOSEnforceSystemAecNs);
  feature_list.InitAndEnableFeature(features::kCrOSEnforceSystemAecAgc);

  GetTestParameters();
  AudioParameters params = AudioManagerChromeOS::GetStreamParametersForSystem(
      user_buffer_size_, system_apm_info_);

  EXPECT_TRUE(ExperimentalAecActive(params));
  EXPECT_TRUE(AecActive(params));
  if (system_apm_info_.aec_supported) {
    EXPECT_FALSE(NsActive(params));
    EXPECT_FALSE(AgcActive(params));
  } else {
    EXPECT_TRUE(NsActive(params));
    EXPECT_TRUE(AgcActive(params));
  }
}
#endif

// TODO(crbug.com/1216273): DCHECKs are disabled during automated testing on
// CrOS and this test failed when tested on an experimental builder with
// DCHECKs. Revert https://crrev.com/c/2959990 to re-enable it.
// See go/chrome-dcheck-on-cros or http://crbug.com/1113456 for more details.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_P(GetStreamParametersForSystem,
       BehaviorWithCrOSEnforceSystemAecNsAgcAndDisallowedSystemAec) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kCrOSEnforceSystemAecNsAgc);
  feature_list.InitAndDisableFeature(features::kCrOSSystemAEC);

  GetTestParameters();
  AudioParameters params = AudioManagerChromeOS::GetStreamParametersForSystem(
      user_buffer_size_, system_apm_info_);

  EXPECT_TRUE(ExperimentalAecActive(params));
  EXPECT_TRUE(AecActive(params));
  if (system_apm_info_.aec_supported) {
    EXPECT_FALSE(NsActive(params));
    EXPECT_FALSE(AgcActive(params));
  } else {
    EXPECT_TRUE(NsActive(params));
    EXPECT_TRUE(AgcActive(params));
  }
}
#endif

TEST_P(GetStreamParametersForSystem, BehaviorWithCrOSEnforceSystemAecNs) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kCrOSEnforceSystemAecNs);

  GetTestParameters();
  AudioParameters params = AudioManagerChromeOS::GetStreamParametersForSystem(
      user_buffer_size_, system_apm_info_);

  EXPECT_TRUE(ExperimentalAecActive(params));
  EXPECT_TRUE(AecActive(params));
  if (system_apm_info_.aec_supported) {
    EXPECT_FALSE(NsActive(params));
    EXPECT_FALSE(AgcActive(params));
  } else {
    EXPECT_TRUE(NsActive(params));
    EXPECT_EQ(AgcActive(params), system_apm_info_.agc_supported);
  }
}

TEST_P(GetStreamParametersForSystem, BehaviorWithCrOSEnforceSystemAecAgc) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kCrOSEnforceSystemAecAgc);

  GetTestParameters();
  AudioParameters params = AudioManagerChromeOS::GetStreamParametersForSystem(
      user_buffer_size_, system_apm_info_);

  EXPECT_TRUE(ExperimentalAecActive(params));
  EXPECT_TRUE(AecActive(params));
  if (system_apm_info_.aec_supported) {
    EXPECT_FALSE(NsActive(params));
    EXPECT_FALSE(AgcActive(params));
  } else {
    EXPECT_EQ(NsActive(params), system_apm_info_.ns_supported);
    EXPECT_TRUE(AgcActive(params));
  }
}

TEST_P(GetStreamParametersForSystem, BehaviorWithCrOSEnforceSystemAec) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kCrOSEnforceSystemAec);

  GetTestParameters();
  AudioParameters params = AudioManagerChromeOS::GetStreamParametersForSystem(
      user_buffer_size_, system_apm_info_);

  EXPECT_TRUE(ExperimentalAecActive(params));
  EXPECT_TRUE(AecActive(params));
  if (system_apm_info_.aec_supported) {
    EXPECT_FALSE(NsActive(params));
    EXPECT_FALSE(AgcActive(params));
  } else {
    EXPECT_EQ(NsActive(params), system_apm_info_.ns_supported);
    EXPECT_EQ(AgcActive(params), system_apm_info_.agc_supported);
  }
}

class AllowedDspParameters
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::tuple<bool, bool, bool>> {
 protected:
  // Retrieve test parameter values.
  void GetTestParameters() {
    system_apm_info_.aec_group_id = 0;
    system_apm_info_.aec_supported = false;
    system_apm_info_.ns_supported = false;
    system_apm_info_.agc_supported = false;
    aec_on_dsp_allowed_ = std::get<0>(GetParam());
    ns_on_dsp_allowed_ = std::get<1>(GetParam());
    agc_on_dsp_allowed_ = std::get<2>(GetParam());

    if (aec_on_dsp_allowed_) {
      enabled_features_.emplace_back(features::kCrOSDspBasedAecAllowed);
    } else {
      disabled_features_.emplace_back(features::kCrOSDspBasedAecAllowed);
    }

    if (ns_on_dsp_allowed_) {
      enabled_features_.emplace_back(features::kCrOSDspBasedNsAllowed);
    } else {
      disabled_features_.emplace_back(features::kCrOSDspBasedNsAllowed);
    }

    if (agc_on_dsp_allowed_) {
      enabled_features_.emplace_back(features::kCrOSDspBasedAgcAllowed);
    } else {
      disabled_features_.emplace_back(features::kCrOSDspBasedAgcAllowed);
    }
  }

  AudioManagerChromeOS::SystemAudioProcessingInfo system_apm_info_;
  size_t user_buffer_size_ = kDefaultInputBufferSize;
  std::vector<base::Feature> enabled_features_;
  std::vector<base::Feature> disabled_features_;
  bool aec_on_dsp_allowed_;
  bool ns_on_dsp_allowed_;
  bool agc_on_dsp_allowed_;
};

INSTANTIATE_TEST_SUITE_P(AllAllowedDspInputParameters,
                         AllowedDspParameters,
                         ::testing::Combine(::testing::Values(false, true),
                                            ::testing::Values(false, true),
                                            ::testing::Values(false, true)));

TEST_P(AllowedDspParameters, BehaviorWithoutAnyEnforcedEffects) {
  GetTestParameters();
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(enabled_features_, disabled_features_);

  AudioParameters params = AudioManagerChromeOS::GetStreamParametersForSystem(
      user_buffer_size_, system_apm_info_);

  EXPECT_FALSE(DspAecAllowed(params));
  EXPECT_FALSE(DspNsAllowed(params));
  EXPECT_FALSE(DspAgcAllowed(params));
}

TEST_P(AllowedDspParameters, BehaviorWithCrOSEnforceSystemAec) {
  GetTestParameters();
  base::test::ScopedFeatureList feature_list;
  enabled_features_.emplace_back(features::kCrOSEnforceSystemAec);
  feature_list.InitWithFeatures(enabled_features_, disabled_features_);

  AudioParameters params = AudioManagerChromeOS::GetStreamParametersForSystem(
      user_buffer_size_, system_apm_info_);

  EXPECT_TRUE(DspAecAllowed(params) && aec_on_dsp_allowed_ ||
              !DspAecAllowed(params) && !aec_on_dsp_allowed_);
  EXPECT_FALSE(DspNsAllowed(params));
  EXPECT_FALSE(DspAgcAllowed(params));
}

TEST_P(AllowedDspParameters, BehaviorWithCrOSEnforceSystemAecNs) {
  GetTestParameters();
  base::test::ScopedFeatureList feature_list;
  enabled_features_.emplace_back(features::kCrOSEnforceSystemAecNs);
  feature_list.InitWithFeatures(enabled_features_, disabled_features_);

  AudioParameters params = AudioManagerChromeOS::GetStreamParametersForSystem(
      user_buffer_size_, system_apm_info_);

  EXPECT_TRUE(DspAecAllowed(params) && aec_on_dsp_allowed_ ||
              !DspAecAllowed(params) && !aec_on_dsp_allowed_);
  EXPECT_TRUE(DspNsAllowed(params) && ns_on_dsp_allowed_ ||
              !DspNsAllowed(params) && !ns_on_dsp_allowed_);
  EXPECT_FALSE(DspAgcAllowed(params));
}

TEST_P(AllowedDspParameters, BehaviorWithCrOSEnforceSystemAecAgc) {
  GetTestParameters();
  base::test::ScopedFeatureList feature_list;
  enabled_features_.emplace_back(features::kCrOSEnforceSystemAecAgc);
  feature_list.InitWithFeatures(enabled_features_, disabled_features_);

  AudioParameters params = AudioManagerChromeOS::GetStreamParametersForSystem(
      user_buffer_size_, system_apm_info_);

  EXPECT_TRUE(DspAecAllowed(params) && aec_on_dsp_allowed_ ||
              !DspAecAllowed(params) && !aec_on_dsp_allowed_);
  EXPECT_FALSE(DspNsAllowed(params));
  EXPECT_TRUE(DspAgcAllowed(params) && agc_on_dsp_allowed_ ||
              !DspAgcAllowed(params) && !agc_on_dsp_allowed_);
}

TEST_P(AllowedDspParameters, BehaviorWithCrOSEnforceSystemAecNsAgc) {
  GetTestParameters();
  base::test::ScopedFeatureList feature_list;
  enabled_features_.emplace_back(features::kCrOSEnforceSystemAecNsAgc);
  feature_list.InitWithFeatures(enabled_features_, disabled_features_);

  AudioParameters params = AudioManagerChromeOS::GetStreamParametersForSystem(
      user_buffer_size_, system_apm_info_);

  EXPECT_TRUE(DspAecAllowed(params) && aec_on_dsp_allowed_ ||
              !DspAecAllowed(params) && !aec_on_dsp_allowed_);
  EXPECT_TRUE(DspNsAllowed(params) && ns_on_dsp_allowed_ ||
              !DspNsAllowed(params) && !ns_on_dsp_allowed_);
  EXPECT_TRUE(DspAgcAllowed(params) && agc_on_dsp_allowed_ ||
              !DspAgcAllowed(params) && !agc_on_dsp_allowed_);
}

}  // namespace media
