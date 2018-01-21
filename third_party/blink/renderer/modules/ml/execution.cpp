// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/ml/Execution.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "modules/ml/Compilation.h"

namespace blink {

Execution::Execution(Compilation* compilation) {}

Execution::~Execution() {}

void Execution::setInput(uint32_t index, MaybeShared<DOMArrayBufferView> data, ExceptionState& state) {

}

void Execution::setOutput(uint32_t index, MaybeShared<DOMArrayBufferView> data, ExceptionState& state) {

}

ScriptPromise Execution::startCompute(ScriptState* script_state) {
  return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(kNotSupportedError, "Not implemented"));
}

void Execution::Trace(blink::Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
