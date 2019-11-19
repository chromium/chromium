// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/worker_internals.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/testing/origin_trials_test.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

WorkerInternals::~WorkerInternals() = default;

WorkerInternals::WorkerInternals() = default;

OriginTrialsTest* WorkerInternals::originTrialsTest() const {
  return MakeGarbageCollected<OriginTrialsTest>();
}

void WorkerInternals::countFeature(ScriptState* script_state,
                                   uint32_t feature,
                                   ExceptionState& exception_state) {
  if (static_cast<int32_t>(WebFeature::kNumberOfFeatures) <= feature) {
    exception_state.ThrowTypeError(
        "The given feature does not exist in WebFeature.");
    return;
  }
  UseCounter::Count(ExecutionContext::From(script_state),
                    static_cast<WebFeature>(feature));
}

void WorkerInternals::countDeprecation(ScriptState* script_state,
                                       uint32_t feature,
                                       ExceptionState& exception_state) {
  if (static_cast<int32_t>(WebFeature::kNumberOfFeatures) <= feature) {
    exception_state.ThrowTypeError(
        "The given feature does not exist in WebFeature.");
    return;
  }
  Deprecation::CountDeprecation(ExecutionContext::From(script_state),
                                static_cast<WebFeature>(feature));
}

void WorkerInternals::collectGarbage(ScriptState* script_state) {
  script_state->GetIsolate()->RequestGarbageCollectionForTesting(
      v8::Isolate::kFullGarbageCollection);
}

}  // namespace blink
