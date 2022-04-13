// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/ml_model_loader.h"

#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/ml_model.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

MLModelLoader::MLModelLoader(ExecutionContext* execution_context,
                             MLContext* ml_context)
    : ml_context_(ml_context) {}

// static
MLModelLoader* MLModelLoader::Create(ScriptState* script_state,
                                     MLContext* ml_context,
                                     ExceptionState& exception_state) {
  if (!script_state->ContextIsValid())
    return nullptr;

  // ml_context is a required input and can not be null.

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  return MakeGarbageCollected<MLModelLoader>(execution_context, ml_context);
}

MLModelLoader::~MLModelLoader() = default;

ScriptPromise MLModelLoader::load(ScriptState* script_state,
                                  DOMArrayBuffer* buffer,
                                  ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid script state");
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  if (ml_context_->GetML() == nullptr) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, "Internal error."));
  } else if (buffer == nullptr) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kConstraintError, "Invalid input arguments."));
  } else {
    NOTREACHED() << "Not implemented yet";
  }

  return promise;
}

void MLModelLoader::Trace(Visitor* visitor) const {
  visitor->Trace(ml_context_);

  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
