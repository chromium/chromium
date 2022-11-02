// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/trustedtypes/trusted_script.h"

#include "third_party/blink/renderer/core/trustedtypes/trusted_types_util.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

TrustedScript::TrustedScript(String script) : script_(std::move(script)) {}

const String& TrustedScript::toString() const {
  return script_;
}

TrustedScript* TrustedScript::fromLiteral(ScriptState* script_state,
                                          const ScriptValue& templateLiteral,
                                          ExceptionState& exception_state) {
  String literal = GetTrustedTypesLiteral(templateLiteral, script_state);
  if (literal.IsNull()) {
    exception_state.ThrowTypeError("Can't fromLiteral a non-literal.");
    return nullptr;
  }
  return MakeGarbageCollected<TrustedScript>(literal);
}

}  // namespace blink
