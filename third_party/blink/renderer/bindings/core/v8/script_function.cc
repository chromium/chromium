// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/script_function.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_set_return_value_for_core.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

namespace {

void InstallFunctionHolderTemplate(v8::Isolate*,
                                   const DOMWrapperWorld&,
                                   v8::Local<v8::Template> interface_template) {
}

const WrapperTypeInfo function_holder_info = {
    {gin::kEmbedderBlink},
    InstallFunctionHolderTemplate,
    nullptr,
    "ScriptFunctionHolder",
    nullptr,
    static_cast<v8::CppHeapPointerTag>(
        ScriptWrappableArrayTag::kScriptFunctionHolderTag),
    static_cast<v8::CppHeapPointerTag>(
        ScriptWrappableArrayTag::kScriptFunctionHolderTag),
    WrapperTypeInfo::kWrapperTypeNoPrototype,
    WrapperTypeInfo::kCustomWrappableId,
    WrapperTypeInfo::kIdlOtherType,
};

}  // namespace

class CORE_EXPORT FunctionHolder final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static v8::Local<v8::Function> Create(ScriptState* script_state,
                                        ScriptFunction* function) {
    CHECK(function);
    FunctionHolder* holder = MakeGarbageCollected<FunctionHolder>(function);
    // The wrapper is held alive by the CallHandlerInfo internally in V8 as long
    // as the function is alive.
    return v8::Function::New(script_state->GetContext(), CallCallback,
                             holder->Wrap(script_state), function->Length(),
                             v8::ConstructorBehavior::kThrow)
        .ToLocalChecked();
  }

  static void CallCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
    RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(args.GetIsolate(),
                                                 "Blink_CallCallback");
    v8::Local<v8::Object> data = v8::Local<v8::Object>::Cast(args.Data());
    v8::Isolate* isolate = args.GetIsolate();
    auto* holder = ToScriptWrappable<FunctionHolder>(isolate, data);
    ScriptState* script_state = ScriptState::ForCurrentRealm(isolate);
    holder->function_->CallRaw(script_state, args);
  }

  explicit FunctionHolder(ScriptFunction* function) : function_(function) {}

  const char* GetHumanReadableName() const final { return "ScriptFunction"; }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(function_);
    ScriptWrappable::Trace(visitor);
  }

 private:
  const Member<ScriptFunction> function_;
};

// The generated bindings normally take care of initializing
// `wrappable_type_info_`, but FunctionHolder doesn't have generated bindings,
// so this has to be done manually.
const WrapperTypeInfo& FunctionHolder::wrapper_type_info_ =
    function_holder_info;

ScriptValue ScriptFunction::Call(ScriptState*, ScriptValue) {
  NOTREACHED();
}

void ScriptFunction::CallRaw(ScriptState* script_state,
                             const v8::FunctionCallbackInfo<v8::Value>& args) {
  ScriptValue result =
      Call(script_state, ScriptValue(script_state->GetIsolate(), args[0]));
  bindings::V8SetReturnValue(args, result);
}

v8::Local<v8::Function> ScriptFunction::ToV8Function(
    ScriptState* script_state) {
  v8::Isolate* isolate = script_state->GetIsolate();
  if (function_.IsEmpty()) {
    function_.Reset(isolate, FunctionHolder::Create(script_state, this));
  }
  return function_.Get(isolate);
}

}  // namespace blink
