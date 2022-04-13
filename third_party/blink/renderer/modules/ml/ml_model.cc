// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/ml_model.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

MLModel::MLModel(ExecutionContext* context, DOMArrayBuffer* buffer) {}

HeapVector<Member<MLTensorInfo>> MLModel::inputs(ScriptState* script_state) {
  return HeapVector<Member<MLTensorInfo>>();
}

HeapVector<Member<MLTensorInfo>> MLModel::outputs(ScriptState* script_state) {
  return HeapVector<Member<MLTensorInfo>>();
}

MLModel::~MLModel() = default;

ScriptPromise MLModel::compute(
    ScriptState* script_state,
    const HeapVector<std::pair<String, Member<MLTensor>>>& inputs,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid script state");
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  resolver->Resolve(HeapVector<std::pair<String, Member<MLTensor>>>());

  return promise;
}

void MLModel::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
