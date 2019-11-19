// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/script_function.h"

#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

void ScriptFunction::Trace(blink::Visitor* visitor) {
  visitor->Trace(script_state_);
  CustomWrappableAdapter::Trace(visitor);
}

v8::Local<v8::Function> ScriptFunction::BindToV8Function() {
#if DCHECK_IS_ON()
  DCHECK(!bind_to_v8_function_already_called_);
  bind_to_v8_function_already_called_ = true;
#endif

  v8::Local<v8::Object> wrapper = CreateAndInitializeWrapper(script_state_);
  // The wrapper is held alive by the CallHandlerInfo internally in V8 as long
  // as the function is alive.
  return v8::Function::New(script_state_->GetContext(), CallCallback, wrapper,
                           0, v8::ConstructorBehavior::kThrow)
      .ToLocalChecked();
}

ScriptValue ScriptFunction::Call(ScriptValue) {
  NOTREACHED();
  return ScriptValue();
}

void ScriptFunction::CallRaw(const v8::FunctionCallbackInfo<v8::Value>& args) {
  ScriptValue result =
      Call(ScriptValue(GetScriptState()->GetIsolate(), args[0]));
  V8SetReturnValue(args, result.V8Value());
}

void ScriptFunction::CallCallback(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(args.GetIsolate(),
                                               "Blink_CallCallback");
  ScriptFunction* script_function = static_cast<ScriptFunction*>(
      ToCustomWrappable(v8::Local<v8::Object>::Cast(args.Data())));
  script_function->CallRaw(args);
}

}  // namespace blink
