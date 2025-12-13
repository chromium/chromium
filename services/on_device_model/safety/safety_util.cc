// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/safety/safety_util.h"

#include "base/strings/utf_string_conversions.h"
#include "components/optimization_guide/core/optimization_guide_features.h"

namespace on_device_model {

language_detection::Prediction PredictLanguage(
    language_detection::LanguageDetectionModel& tflite_model,
    std::string_view text) {
  if (base::FeatureList::IsEnabled(
          optimization_guide::features::kTextSafetyScanLanguageDetection)) {
    return language_detection::TopPrediction(
        tflite_model.PredictWithScan(base::UTF8ToUTF16(text)));
  } else {
    return tflite_model.PredictTopLanguageWithSamples(base::UTF8ToUTF16(text));
  }
}

}  // namespace on_device_model
