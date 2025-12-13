// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/module_import_meta.h"

#include "third_party/blink/renderer/bindings/core/v8/native_value_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"

namespace blink {

const v8::Local<v8::Function> ModuleImportMeta::MakeResolveV8Function(
    Modulator* modulator) const {
  return MakeGarbageCollected<Resolve>(modulator, url_)
      ->ToV8Function(modulator->GetScriptState());
}

ScriptValue ModuleImportMeta::Resolve::Call(ScriptState* script_state,
                                            ScriptValue value) {
  v8::Isolate* isolate = script_state->GetIsolate();
  const String specifier = NativeValueTraits<IDLString>::NativeValue(
      isolate, value.V8Value(), PassThroughException(isolate));
  if (isolate->HasPendingException()) {
    return ScriptValue();
  }

  String failure_reason = "Unknown failure";
  const KURL result = modulator_->ResolveModuleSpecifier(specifier, KURL(url_),
                                                         &failure_reason);

  if (!result.IsValid()) {
    V8ThrowException::ThrowTypeError(
        isolate, StrCat({"Failed to resolve module specifier ", specifier, ": ",
                         failure_reason}));
  }

  return ScriptValue(
      isolate, ToV8Traits<IDLString>::ToV8(script_state, result.GetString()));
}

void ModuleImportMeta::Resolve::Trace(Visitor* visitor) const {
  visitor->Trace(modulator_);
  ScriptFunction::Trace(visitor);
}

}  // namespace blink
