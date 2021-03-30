// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_SET_RETURN_VALUE_FOR_CORE_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_SET_RETURN_VALUE_FOR_CORE_H_

#include "third_party/blink/renderer/bindings/core/v8/js_event_handler.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/platform/bindings/v8_set_return_value.h"

namespace blink {

namespace bindings {

class NativeValueTraitsStringAdapter;

// ScriptValue
template <typename CallbackInfo, typename... ExtraArgs>
void V8SetReturnValue(const CallbackInfo& info,
                      const ScriptValue& value,
                      ExtraArgs... extra_args) {
  // Ignore all |extra_args| given as inputs for optimization.
  V8SetReturnValue(info, value.V8Value());
}

// IDLObject type
template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info,
                      const ScriptValue& value,
                      V8ReturnValue::IDLObject) {
  DCHECK(!value.IsEmpty());
  v8::Local<v8::Value> v8_value = value.V8Value();
  // TODO(crbug.com/1185033): Change this if-branch to DCHECK.
  if (!v8_value->IsObject()) {
    V8SetReturnValue(info, v8::Undefined(info.GetIsolate()));
    return;
  }
  V8SetReturnValue(info, v8_value);
}

// String types
template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info,
                      const NativeValueTraitsStringAdapter& value,
                      v8::Isolate* isolate,
                      V8ReturnValue::NonNullable) {
  V8SetReturnValue(info, static_cast<String>(value), isolate,
                   V8ReturnValue::kNonNullable);
}

template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info,
                      const NativeValueTraitsStringAdapter& value,
                      v8::Isolate* isolate,
                      V8ReturnValue::Nullable) {
  V8SetReturnValue(info, static_cast<String>(value), isolate,
                   V8ReturnValue::kNullable);
}

// EventListener
template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info,
                      const EventListener* value,
                      v8::Isolate* isolate,
                      EventTarget* event_target) {
  EventListener* event_listener = const_cast<EventListener*>(value);
  info.GetReturnValue().Set(
      JSEventHandler::AsV8Value(isolate, event_target, event_listener));
}

// IDLDictionaryBase
template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info,
                      const IDLDictionaryBase* dictionary) {
  // TODO(crbug.com/1185018): Change this if-branch to DCHECK(dictionary).
  if (!dictionary)
    return V8SetReturnValue(info, v8::Null(info.GetIsolate()));
  V8SetReturnValue(info,
                   dictionary->ToV8Impl(V8ReturnValue::CreationContext(info),
                                        info.GetIsolate()));
}

}  // namespace bindings

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_SET_RETURN_VALUE_FOR_CORE_H_
