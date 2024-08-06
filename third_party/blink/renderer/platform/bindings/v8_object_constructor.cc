/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/bindings/v8_object_constructor.h"

#include "components/crash/core/common/crash_key.h"
#include "third_party/blink/renderer/platform/bindings/runtime_call_stats.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_context_data.h"
#include "third_party/blink/renderer/platform/bindings/v8_set_return_value.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

v8::MaybeLocal<v8::Object> V8ObjectConstructor::NewInstance(
    v8::Isolate* isolate,
    v8::Local<v8::Function> function,
    int argc,
    v8::Local<v8::Value> argv[]) {
  DCHECK(!function.IsEmpty());
  TRACE_EVENT0("v8", "v8.newInstance");
  RUNTIME_CALL_TIMER_SCOPE(isolate, RuntimeCallStats::CounterId::kV8);
  ConstructorMode constructor_mode(isolate);
  v8::MicrotasksScope microtasks_scope(
      isolate, isolate->GetCurrentContext()->GetMicrotaskQueue(),
      v8::MicrotasksScope::kDoNotRunMicrotasks);
  // Construct without side effect only in ConstructorMode::kWrapExistingObject
  // cases. Allowed methods can correctly set return values without invoking
  // Blink's internal constructors.
  v8::MaybeLocal<v8::Object> result = function->NewInstanceWithSideEffectType(
      isolate->GetCurrentContext(), argc, argv,
      v8::SideEffectType::kHasNoSideEffect);
  CHECK(!isolate->IsDead());
  return result;
}

void V8ObjectConstructor::IsValidConstructorMode(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(),
                                               "Blink_IsValidConstructorMode");
  if (ConstructorMode::Current(info.GetIsolate()) ==
      ConstructorMode::kCreateNewObject) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), "Illegal constructor");
    return;
  }
  bindings::V8SetReturnValue(info, info.This());
}

v8::Local<v8::Function> V8ObjectConstructor::CreateInterfaceObject(
    const WrapperTypeInfo* type,
    v8::Local<v8::Context> context,
    const DOMWrapperWorld& world,
    v8::Isolate* isolate,
    v8::Local<v8::Function> parent_interface,
    CreationMode creation_mode) {
  v8::Local<v8::FunctionTemplate> interface_template =
      type->GetV8ClassTemplate(isolate, world).As<v8::FunctionTemplate>();
  // Getting the function might fail if we're running out of stack or memory.
  v8::Local<v8::Function> interface_object;
  bool get_interface_object =
      interface_template->GetFunction(context).ToLocal(&interface_object);
  if (!get_interface_object) [[unlikely]] {
    // For investigation of crbug.com/1247628
    static crash_reporter::CrashKeyString<64> crash_key(
        "blink__create_interface_object");
    crash_key.Set(type->interface_name);
    CHECK(get_interface_object);
  }

  if (type->parent_class) {
    DCHECK(!parent_interface.IsEmpty());
    bool set_parent_interface =
        interface_object->SetPrototype(context, parent_interface).ToChecked();
    CHECK(set_parent_interface);
  }

  v8::Local<v8::Object> prototype_object;
  if (type->wrapper_type_prototype ==
      WrapperTypeInfo::kWrapperTypeObjectPrototype) {
    v8::Local<v8::Value> prototype_value;
    bool get_prototype_value =
        interface_object->Get(context, V8AtomicString(isolate, "prototype"))
            .ToLocal(&prototype_value);
    CHECK(get_prototype_value);
    CHECK(prototype_value->IsObject());

    prototype_object = prototype_value.As<v8::Object>();
  }

  if (creation_mode == CreationMode::kInstallConditionalFeatures) {
    type->InstallConditionalFeatures(context, world, v8::Local<v8::Object>(),
                                     prototype_object, interface_object,
                                     interface_template);
  }

  return interface_object;
}

}  // namespace blink
