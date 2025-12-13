// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_SAFETY_SAFETY_UTIL_H_
#define SERVICES_ON_DEVICE_MODEL_SAFETY_SAFETY_UTIL_H_

#include "components/translate/core/language_detection/language_detection_model.h"

namespace on_device_model {

// Make a language prediction given a `LanguageDetectionModel` and the string
// to be analyzed.
language_detection::Prediction PredictLanguage(
    language_detection::LanguageDetectionModel& tflite_model,
    std::string_view text);

}  // namespace on_device_model

#endif  // SERVICES_ON_DEVICE_MODEL_SAFETY_SAFETY_UTIL_H_
