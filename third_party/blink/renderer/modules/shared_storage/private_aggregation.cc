// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shared_storage/private_aggregation.h"

namespace blink {

PrivateAggregation::PrivateAggregation() = default;
PrivateAggregation::~PrivateAggregation() = default;

void PrivateAggregation::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

void PrivateAggregation::contributeToHistogram(
    ScriptState* script_state,
    const PrivateAggregationHistogramContribution* contribution,
    ExceptionState& exception_state) {}

void PrivateAggregation::enableDebugMode(ScriptState* script_state,
                                         ExceptionState& exception_state) {}

void PrivateAggregation::enableDebugMode(
    ScriptState* script_state,
    const PrivateAggregationDebugModeOptions* options,
    ExceptionState& exception_state) {}

}  // namespace blink
