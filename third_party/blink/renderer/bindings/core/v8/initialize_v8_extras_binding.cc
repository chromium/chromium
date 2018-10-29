// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/initialize_v8_extras_binding.h"

#include <algorithm>
#include <iterator>

#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/bindings/to_v8.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

// This macro avoids duplicating the name and hence prevents typos.
#define WEB_FEATURE_ID_NAME_LOOKUP_ENTRY(id) \
  { #id, WebFeature::k##id }

struct WebFeatureIdNameLookupEntry {
  const char* const name;
  const WebFeature id;
};

// TODO(ricea): Replace with a more efficient data structure if the
// number of entries increases.
constexpr WebFeatureIdNameLookupEntry web_feature_id_name_lookup_table[] = {
    WEB_FEATURE_ID_NAME_LOOKUP_ENTRY(ReadableStreamConstructor),
    WEB_FEATURE_ID_NAME_LOOKUP_ENTRY(WritableStreamConstructor),
    WEB_FEATURE_ID_NAME_LOOKUP_ENTRY(TransformStreamConstructor),
};

#undef WEB_FEATURE_ID_NAME_LOOKUP_ENTRY

class CountUseForBindings : public ScriptFunction {
 public:
  static v8::Local<v8::Function> CreateFunction(ScriptState* script_state) {
    auto* self = new CountUseForBindings(script_state);
    return self->BindToV8Function();
  }

 private:
  explicit CountUseForBindings(ScriptState* script_state)
      : ScriptFunction(script_state) {}

  ScriptValue Call(ScriptValue value) override {
    String string_id;
    if (!value.ToString(string_id)) {
      V8ThrowException::ThrowTypeError(value.GetIsolate(),
                                       "countUse requires a string argument");
      return ScriptValue();
    }

    auto* const it =
        std::find_if(std::begin(web_feature_id_name_lookup_table),
                     std::end(web_feature_id_name_lookup_table),
                     [&string_id](const WebFeatureIdNameLookupEntry& entry) {
                       return string_id == entry.name;
                     });

    if (it == std::end(web_feature_id_name_lookup_table)) {
      V8ThrowException::ThrowTypeError(value.GetIsolate(),
                                       "unknown use counter");
      return ScriptValue();
    }

    UseCounter::Count(ExecutionContext::From(GetScriptState()), it->id);

    return ScriptValue::From(GetScriptState(), ToV8UndefinedGenerator());
  }
};

void AddCountUse(ScriptState* script_state, v8::Local<v8::Object> binding) {
  v8::Local<v8::Function> fn =
      CountUseForBindings::CreateFunction(script_state);
  v8::Local<v8::String> name =
      V8AtomicString(script_state->GetIsolate(), "countUse");
  binding->Set(script_state->GetContext(), name, fn).ToChecked();
}

void AddOriginals(ScriptState* script_state, v8::Local<v8::Object> binding) {
  v8::Local<v8::Object> global = script_state->GetContext()->Global();
  v8::Local<v8::Context> context = script_state->GetContext();
  v8::Isolate* isolate = script_state->GetIsolate();

  const auto ObjectGet = [&context, &isolate](v8::Local<v8::Value> object,
                                              const char* property) {
    DCHECK(object->IsObject());
    return object.As<v8::Object>()
        ->Get(context, V8AtomicString(isolate, property))
        .ToLocalChecked();
  };

  const auto GetPrototype = [&ObjectGet](v8::Local<v8::Value> object) {
    return ObjectGet(object, "prototype");
  };

  const auto GetOwnPD = [&context, &isolate](v8::Local<v8::Value> object,
                                             const char* property) {
    DCHECK(object->IsObject());
    return object.As<v8::Object>()
        ->GetOwnPropertyDescriptor(context, V8AtomicString(isolate, property))
        .ToLocalChecked();
  };

  const auto GetOwnPDGet = [&ObjectGet, &GetOwnPD](v8::Local<v8::Value> object,
                                                   const char* property) {
    return ObjectGet(GetOwnPD(object, property), "get");
  };

  const auto Bind = [&context, &isolate, &binding](const char* name,
                                                   v8::Local<v8::Value> value) {
    bool result =
        binding
            ->CreateDataProperty(context, V8AtomicString(isolate, name), value)
            .ToChecked();
    DCHECK(result);
  };

  v8::Local<v8::Value> message_channel = ObjectGet(global, "MessageChannel");

  // Some Worklets don't have MessageChannel. In this case, serialization will
  // be disabled.
  if (message_channel->IsUndefined())
    return;

  Bind("MessageChannel", message_channel);

  v8::Local<v8::Value> message_channel_prototype =
      GetPrototype(message_channel);
  Bind("MessageChannel_port1_get",
       GetOwnPDGet(message_channel_prototype, "port1"));
  Bind("MessageChannel_port2_get",
       GetOwnPDGet(message_channel_prototype, "port2"));

  v8::Local<v8::Value> event_target_prototype =
      GetPrototype(ObjectGet(global, "EventTarget"));
  Bind("EventTarget_addEventListener",
       ObjectGet(event_target_prototype, "addEventListener"));

  v8::Local<v8::Value> message_port_prototype =
      GetPrototype(ObjectGet(global, "MessagePort"));
  Bind("MessagePort_postMessage",
       ObjectGet(message_port_prototype, "postMessage"));
  Bind("MessagePort_close", ObjectGet(message_port_prototype, "close"));
  Bind("MessagePort_start", ObjectGet(message_port_prototype, "start"));

  v8::Local<v8::Value> message_event_prototype =
      GetPrototype(ObjectGet(global, "MessageEvent"));

  Bind("MessageEvent_data_get", GetOwnPDGet(message_event_prototype, "data"));

  v8::Local<v8::Value> dom_exception = ObjectGet(global, "DOMException");
  Bind("DOMException", dom_exception);

  v8::Local<v8::Value> dom_exception_prototype = GetPrototype(dom_exception);
  Bind("DOMException_message_get",
       GetOwnPDGet(dom_exception_prototype, "message"));
  Bind("DOMException_name_get", GetOwnPDGet(dom_exception_prototype, "name"));
}

}  // namespace

void InitializeV8ExtrasBinding(ScriptState* script_state) {
  v8::Local<v8::Object> binding =
      script_state->GetContext()->GetExtrasBindingObject();
  AddCountUse(script_state, binding);
  AddOriginals(script_state, binding);
}

}  // namespace blink
