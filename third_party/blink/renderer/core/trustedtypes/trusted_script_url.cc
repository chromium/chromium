// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/trustedtypes/trusted_script_url.h"

#include "third_party/blink/renderer/core/trustedtypes/trusted_types_util.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

TrustedScriptURL::TrustedScriptURL(String url) : url_(std::move(url)) {}

const String& TrustedScriptURL::toString() const {
  return url_;
}

TrustedScriptURL* TrustedScriptURL::fromLiteral(
    ScriptState* script_state,
    const ScriptValue& templateLiteral,
    ExceptionState& exception_state) {
  String literal = GetTrustedTypesLiteral(templateLiteral, script_state);
  if (literal.IsNull()) {
    exception_state.ThrowTypeError("Can't fromLiteral a non-literal.");
    return nullptr;
  }
  return MakeGarbageCollected<TrustedScriptURL>(literal);
}

}  // namespace blink
