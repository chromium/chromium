// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/origin_trials_test.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

OriginTrialsTest* OriginTrialsTest::Create() {
  return MakeGarbageCollected<OriginTrialsTest>();
}

bool OriginTrialsTest::throwingAttribute(ScriptState* script_state,
                                         ExceptionState& exception_state) {
  String error_message;
  if (!RuntimeEnabledFeatures::OriginTrialsSampleAPIEnabled(
          ExecutionContext::From(script_state))) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "The Origin Trials Sample API has not been enabled in this context");
    return false;
  }
  return unconditionalAttribute();
}

}  // namespace blink
