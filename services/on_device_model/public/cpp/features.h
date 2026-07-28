// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_FEATURES_H_
#define SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

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

// Whether the on-device model should use the LiteRT-LM backend.
COMPONENT_EXPORT(ON_DEVICE_MODEL_CPP)
BASE_DECLARE_FEATURE(kOnDeviceModelLitertLmBackend);

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

}  // namespace on_device_model::features

#endif  // SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_FEATURES_H_
