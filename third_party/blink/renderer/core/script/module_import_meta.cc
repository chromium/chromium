// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/module_import_meta.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

const v8::Local<v8::Function> ModuleImportMeta::MakeResolveV8Function(
    Modulator* modulator) const {
  ScriptFunction* fn = MakeGarbageCollected<ScriptFunction>(
      modulator->GetScriptState(),
      MakeGarbageCollected<Resolve>(modulator, url_));
  return fn->V8Function();
}

ScriptValue ModuleImportMeta::Resolve::Call(ScriptState* script_state,
                                            ScriptValue value) {
  ExceptionState exception_state(script_state->GetIsolate(),
                                 v8::ExceptionContext::kOperation,
                                 "import.meta", "resolve");

  const String specifier = NativeValueTraits<IDLString>::NativeValue(
      script_state->GetIsolate(), value.V8Value(), exception_state);
  if (exception_state.HadException()) {
    return ScriptValue();
  }

  String failure_reason = "Unknown failure";
  const KURL result = modulator_->ResolveModuleSpecifier(specifier, KURL(url_),
                                                         &failure_reason);

  if (!result.IsValid()) {
    exception_state.ThrowTypeError("Failed to resolve module specifier " +
                                   specifier + ": " + failure_reason);
  }

  return ScriptValue(
      script_state->GetIsolate(),
      ToV8Traits<IDLString>::ToV8(script_state, result.GetString()));
}

void ModuleImportMeta::Resolve::Trace(Visitor* visitor) const {
  visitor->Trace(modulator_);
  ScriptFunction::Callable::Trace(visitor);
}

}  // namespace blink
