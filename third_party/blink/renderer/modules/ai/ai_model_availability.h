// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_MODEL_AVAILABILITY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_MODEL_AVAILABILITY_H_

#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_model_availability.h"

namespace blink {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(AIModelAvailability)
enum class AIModelAvailability {
  kReadily = 0,
  kAfterDownload = 1,
  kNo = 2,

  kMaxValue = kNo,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ai/enums.xml:AIModelAvailability)

V8AIModelAvailability AIModelAvailabilityToV8(AIModelAvailability availability);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_MODEL_AVAILABILITY_H_
