// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/model_execution/exception_helpers.h"

#include "third_party/blink/renderer/platform/bindings/exception_code.h"

namespace blink {

void ThrowInvalidContextException(ExceptionState& exception_state) {
  exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                    "The execution context is not valid.");
}

void RejectPromiseWithInternalError(ScriptPromiseResolverBase* resolver) {
  resolver->Reject(DOMException::Create(
      "Model execution service is not available.",
      DOMException::GetErrorName(DOMExceptionCode::kOperationError)));
}

}  // namespace blink
