// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/ml/Compilation.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "modules/ml/Model.h"

namespace blink {

Compilation::Compilation(Model* model) {}

Compilation::~Compilation() {}

void Compilation::setPreference(uint32_t preference, ExceptionState& state) {

}

ScriptPromise Compilation::finish(ScriptState* script_state) {
  return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(kNotSupportedError, "Not implemented"));
}

void Compilation::Trace(blink::Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
