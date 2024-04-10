// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/script_function.h"

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"

namespace blink {

namespace {

void InstallCallableHolderTemplate(v8::Isolate*,
                                   const DOMWrapperWorld&,
                                   v8::Local<v8::Template> interface_template) {
  v8::Local<v8::ObjectTemplate> instance_template =
      interface_template.As<v8::FunctionTemplate>()->InstanceTemplate();
  instance_template->SetInternalFieldCount(kV8DefaultWrapperInternalFieldCount);
}

const WrapperTypeInfo callable_holder_info = {
    gin::kEmbedderBlink,
    InstallCallableHolderTemplate,
    nullptr,
    "ScriptFunctionCallableHolder",
    nullptr,
    WrapperTypeInfo::kWrapperTypeNoPrototype,
    WrapperTypeInfo::kCustomWrappableId,
    WrapperTypeInfo::kNotInheritFromActiveScriptWrappable,
    WrapperTypeInfo::kCustomWrappableKind,
};

}  // namespace

class CORE_EXPORT CallableHolder final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static v8::Local<v8::Function> Create(ScriptState* script_state,
                                        ScriptFunction::Callable* callable) {
    CHECK(callable);
    CallableHolder* holder = MakeGarbageCollected<CallableHolder>(callable);
    // The wrapper is held alive by the CallHandlerInfo internally in V8 as long
    // as the function is alive.
    return v8::Function::New(script_state->GetContext(), CallCallback,
                             holder->Wrap(script_state), callable->Length(),
                             v8::ConstructorBehavior::kThrow)
        .ToLocalChecked();
  }

  static void CallCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
    RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(args.GetIsolate(),
                                                 "Blink_CallCallback");
    v8::Local<v8::Object> data = v8::Local<v8::Object>::Cast(args.Data());
    auto* holder = static_cast<CallableHolder*>(
        ToScriptWrappable(args.GetIsolate(), data));
    ScriptState* script_state =
        ScriptState::From(args.GetIsolate()->GetCurrentContext());
    holder->callable_->CallRaw(script_state, args);
  }

  explicit CallableHolder(ScriptFunction::Callable* callable)
      : callable_(callable) {}
  const char* NameInHeapSnapshot() const final {
    return "ScriptFunction::Callable";
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(callable_);
    ScriptWrappable::Trace(visitor);
  }

 private:
  const Member<ScriptFunction::Callable> callable_;
};

// The generated bindings normally take care of initializing
// `wrappable_type_info_`, but CallableHolder doesn't have generated bindings,
// so this has to be done manually.
const WrapperTypeInfo& CallableHolder::wrapper_type_info_ =
    callable_holder_info;

ScriptValue ScriptFunction::Callable::Call(ScriptState*, ScriptValue) {
  NOTREACHED();
  return ScriptValue();
}

void ScriptFunction::Callable::CallRaw(
    ScriptState* script_state,
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  ScriptValue result =
      Call(script_state, ScriptValue(script_state->GetIsolate(), args[0]));
  V8SetReturnValue(args, result.V8Value());
}

ScriptFunction::ScriptFunction(ScriptState* script_state, Callable* callable)
    : script_state_(script_state),
      function_(script_state->GetIsolate(),
                CallableHolder::Create(script_state, callable)) {}

}  // namespace blink
