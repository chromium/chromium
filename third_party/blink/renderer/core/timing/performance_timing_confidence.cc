// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_timing_confidence.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"

namespace blink {

PerformanceTimingConfidence::PerformanceTimingConfidence(
    double randomizedTriggerRate,
    V8PerformanceTimingConfidenceValue value)
    : randomizedTriggerRate_(randomizedTriggerRate), value_(value) {}

ScriptValue PerformanceTimingConfidence::toJSON(
    ScriptState* script_state) const {
  V8ObjectBuilder builder(script_state);

  builder.AddNumber("randomizedTriggerRate", randomizedTriggerRate());
  builder.AddStringOrNull("value", value_.AsString());
  return builder.GetScriptValue();
}

}  // namespace blink
