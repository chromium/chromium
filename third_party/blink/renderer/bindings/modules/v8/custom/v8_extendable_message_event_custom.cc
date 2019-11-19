// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/modules/v8/v8_extendable_message_event.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_extendable_message_event_init.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"

namespace blink {

void V8ExtendableMessageEvent::ConstructorCustom(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ExceptionState exception_state(isolate, ExceptionState::kConstructionContext,
                                 "ExtendableMessageEvent");
  if (UNLIKELY(info.Length() < 1)) {
    exception_state.ThrowTypeError(
        ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  V8StringResource<> type = info[0];
  if (!type.Prepare())
    return;

  ExtendableMessageEventInit* event_init_dict =
      ExtendableMessageEventInit::Create();
  if (!IsUndefinedOrNull(info[1])) {
    if (!info[1]->IsObject()) {
      exception_state.ThrowTypeError(
          "parameter 2 ('eventInitDict') is not an object.");
      return;
    }
    V8ExtendableMessageEventInit::ToImpl(isolate, info[1], event_init_dict,
                                         exception_state);
    if (exception_state.HadException())
      return;
  }

  ExtendableMessageEvent* impl =
      ExtendableMessageEvent::Create(type, event_init_dict);
  v8::Local<v8::Object> wrapper = info.Holder();
  wrapper = impl->AssociateWithWrapper(
      isolate, V8ExtendableMessageEvent::GetWrapperTypeInfo(), wrapper);

  // TODO(bashi): Workaround for http://crbug.com/529941. We need to store
  // |data| as a private value to avoid cyclic references.
  if (event_init_dict->hasData()) {
    v8::Local<v8::Value> v8_data = event_init_dict->data().V8Value();
    V8PrivateProperty::GetSymbol(isolate,
                                 kPrivatePropertyMessageEventCachedData)
        .Set(wrapper, v8_data);
    if (DOMWrapperWorld::Current(isolate).IsIsolatedWorld()) {
      impl->SetSerializedData(
          SerializedScriptValue::SerializeAndSwallowExceptions(isolate,
                                                               v8_data));
    }
  }
  V8SetReturnValue(info, wrapper);
}

void V8ExtendableMessageEvent::DataAttributeGetterCustom(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExtendableMessageEvent* event =
      V8ExtendableMessageEvent::ToImpl(info.Holder());
  v8::Isolate* isolate = info.GetIsolate();
  auto private_cached_data = V8PrivateProperty::GetSymbol(
      isolate, kPrivatePropertyMessageEventCachedData);
  v8::Local<v8::Value> result;
  if (private_cached_data.GetOrUndefined(info.Holder()).ToLocal(&result) &&
      !result->IsUndefined()) {
    V8SetReturnValue(info, result);
    return;
  }

  v8::Local<v8::Value> data;
  if (SerializedScriptValue* serialized_value = event->SerializedData()) {
    MessagePortArray ports = event->ports();
    SerializedScriptValue::DeserializeOptions options;
    options.message_ports = &ports;
    data = serialized_value->Deserialize(isolate, options);
  } else if (DOMWrapperWorld::Current(isolate).IsIsolatedWorld()) {
    v8::Local<v8::Value> main_world_data;
    if (private_cached_data.GetFromMainWorld(event).ToLocal(&main_world_data) &&
        !main_world_data->IsUndefined()) {
      // TODO(bashi): Enter the main world's ScriptState::Scope while
      // serializing the main world's value.
      event->SetSerializedData(
          SerializedScriptValue::SerializeAndSwallowExceptions(
              info.GetIsolate(), main_world_data));
      data = event->SerializedData()->Deserialize(isolate);
    }
  }
  if (data.IsEmpty())
    data = v8::Null(isolate);
  private_cached_data.Set(info.Holder(), data);
  V8SetReturnValue(info, data);
}

}  // namespace blink
