// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_FEATURES_H_
#define SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_FEATURES_H_

#include <string>

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace on_device_model::features {

// Whether the fake implementation is used in the OnDeviceModelService.
COMPONENT_EXPORT(ON_DEVICE_MODEL_CPP)
BASE_DECLARE_FEATURE(kUseFakeChromeML);

// Whether the on-device model should be limited to running only on the CPU.
COMPONENT_EXPORT(ON_DEVICE_MODEL_CPP)
BASE_DECLARE_FEATURE(kOnDeviceModelForceCpuBackend);

// Whether the CPU backend for the on-device model is enabled.
COMPONENT_EXPORT(ON_DEVICE_MODEL_CPP)
BASE_DECLARE_FEATURE(kOnDeviceModelCpuBackend);

// Whether the on-device model should use the conversation API for LiteRT-LM.
// Enabling this feature implies usage of the LiteRT-LM backend framework.
COMPONENT_EXPORT(ON_DEVICE_MODEL_CPP)
BASE_DECLARE_FEATURE(kOnDeviceModelConversationBackend);

// Whether the GPU program cache is enabled for the on-device model.
COMPONENT_EXPORT(ON_DEVICE_MODEL_CPP)
BASE_DECLARE_FEATURE(kOnDeviceModelGpuProgramCache);

// Whether the GPU weight cache is enabled for the on-device model.
COMPONENT_EXPORT(ON_DEVICE_MODEL_CPP)
BASE_DECLARE_FEATURE(kOnDeviceModelGpuWeightCache);

// Whether speculative decoding / MTP is enabled for the on-device model.
COMPONENT_EXPORT(ON_DEVICE_MODEL_CPP)
BASE_DECLARE_FEATURE(kOnDeviceModelSpeculativeDecoding);

// Controls the decoder prefill setting for on-device ASR stream.
COMPONENT_EXPORT(ON_DEVICE_MODEL_CPP)
BASE_DECLARE_FEATURE(kOnDeviceModelAsrDecoderPrefill);

COMPONENT_EXPORT(ON_DEVICE_MODEL_CPP)
extern const base::FeatureParam<int> kOnDeviceModelAsrDecoderPrefillBackoff;

// Controls the token stability settings for on-device ASR stream.
COMPONENT_EXPORT(ON_DEVICE_MODEL_CPP)
BASE_DECLARE_FEATURE(kOnDeviceModelAsrTokenStability);

COMPONENT_EXPORT(ON_DEVICE_MODEL_CPP)
extern const base::FeatureParam<int> kOnDeviceModelAsrInterimTokenHoldback;

COMPONENT_EXPORT(ON_DEVICE_MODEL_CPP)
extern const base::FeatureParam<int> kOnDeviceModelAsrInterimReleaseSilenceMs;

COMPONENT_EXPORT(ON_DEVICE_MODEL_CPP)
extern const base::FeatureParam<int> kOnDeviceModelAsrInterimStabilitySteps;

// Controls speculative decoding and decoder prefill settings for on-device ASR
// stream.
COMPONENT_EXPORT(ON_DEVICE_MODEL_CPP)
BASE_DECLARE_FEATURE(kOnDeviceModelAsrSpeculativeDecoding);

COMPONENT_EXPORT(ON_DEVICE_MODEL_CPP)
extern const base::FeatureParam<bool> kOnDeviceModelAsrEnableDraftVerification;

COMPONENT_EXPORT(ON_DEVICE_MODEL_CPP)
extern const base::FeatureParam<int>
    kOnDeviceModelAsrInterimDecoderPrefillBackoff;

// End-of-session / final decoder prefill backoff setting:
// -2 = unset (uses the same setting as `decoder_prefill_backoff`),
// -1 = disable final prefill (`final_prefill_disabled`),
// >= 0 = `final_decoder_prefill_backoff` token count.
COMPONENT_EXPORT(ON_DEVICE_MODEL_CPP)
extern const base::FeatureParam<int>
    kOnDeviceModelAsrFinalDecoderPrefillBackoff;

// Whether to scan the full text when running the language detection in the text
// safety classifier.
COMPONENT_EXPORT(ON_DEVICE_MODEL_CPP)
BASE_DECLARE_FEATURE(kTextSafetyScanLanguageDetection);

// Allows setting feature params for model download configuration, such as
// minimum performance class for download.
COMPONENT_EXPORT(ON_DEVICE_MODEL_CPP)
BASE_DECLARE_FEATURE(kOnDeviceModelPerformanceParams);

// Comma-separated list of performance classes (e.g. "3,4,5") that should
// download the base model. Use "*" if there is no performance class
// requirement.
COMPONENT_EXPORT(ON_DEVICE_MODEL_CPP)
extern const base::FeatureParam<std::string>
    kPerformanceClassListForOnDeviceModel;

// Comma-separated list of performance classes that should use a smaller model
// if available. This should be a subset of
// kPerformanceClassListForOnDeviceModel.
COMPONENT_EXPORT(ON_DEVICE_MODEL_CPP)
extern const base::FeatureParam<std::string>
    kLowTierPerformanceClassListForOnDeviceModel;

// Comma-separated list of performance classes that have image input enabled.
COMPONENT_EXPORT(ON_DEVICE_MODEL_CPP)
extern const base::FeatureParam<std::string> kPerformanceClassListForImageInput;

// Settings to control output sampling.
inline constexpr int GetOnDeviceModelDefaultTopK() {
  return 64;
}
inline constexpr int GetOnDeviceModelMaxTopK() {
  return 128;
}
inline constexpr double GetOnDeviceModelDefaultTemperature() {
  return 1.0;
}

}  // namespace on_device_model::features

#endif  // SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_FEATURES_H_
