// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/public/cpp/features.h"

namespace on_device_model::features {

BASE_FEATURE(kUseFakeChromeML, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kOnDeviceModelForceCpuBackend, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kOnDeviceModelCpuBackend, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kOnDeviceModelConversationBackend,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kOnDeviceModelGpuProgramCache, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kOnDeviceModelGpuWeightCache, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kOnDeviceModelSpeculativeDecoding,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kOnDeviceModelAsrDecoderPrefill,
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kOnDeviceModelAsrDecoderPrefillBackoff{
    &kOnDeviceModelAsrDecoderPrefill, "decoder_prefill_backoff", -1};

BASE_FEATURE(kOnDeviceModelAsrTokenStability,
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kOnDeviceModelAsrInterimTokenHoldback{
    &kOnDeviceModelAsrTokenStability, "interim_token_holdback", 0};

const base::FeatureParam<int> kOnDeviceModelAsrInterimReleaseSilenceMs{
    &kOnDeviceModelAsrTokenStability, "interim_release_silence_ms", 0};

const base::FeatureParam<int> kOnDeviceModelAsrInterimStabilitySteps{
    &kOnDeviceModelAsrTokenStability, "interim_stability_steps", 0};

BASE_FEATURE(kOnDeviceModelAsrSpeculativeDecoding,
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<bool> kOnDeviceModelAsrEnableDraftVerification{
    &kOnDeviceModelAsrSpeculativeDecoding, "enable_draft_verification", true};

const base::FeatureParam<int> kOnDeviceModelAsrInterimDecoderPrefillBackoff{
    &kOnDeviceModelAsrSpeculativeDecoding, "decoder_prefill_backoff", -1};

const base::FeatureParam<int> kOnDeviceModelAsrFinalDecoderPrefillBackoff{
    &kOnDeviceModelAsrSpeculativeDecoding, "final_decoder_prefill_backoff", -2};

}  // namespace on_device_model::features
