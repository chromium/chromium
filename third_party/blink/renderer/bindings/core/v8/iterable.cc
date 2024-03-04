// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/iterable.h"

#include <array>

#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "v8/include/v8-container.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-primitive.h"

namespace blink {
namespace bindings {

v8::Local<v8::Object> ESCreateIterResultObject(ScriptState* script_state,
                                               bool done,
                                               v8::Local<v8::Value> value) {
  static constexpr std::string_view properties[] = {"done", "value"};

  v8::Isolate* const isolate = script_state->GetIsolate();
  auto* per_isolate_data = V8PerIsolateData::From(isolate);
  v8::MaybeLocal<v8::DictionaryTemplate> maybe_template =
      per_isolate_data->FindV8DictionaryTemplate(properties);
  v8::Local<v8::DictionaryTemplate> just_template;
  if (!maybe_template.IsEmpty()) {
    just_template = maybe_template.ToLocalChecked();
  } else {
    just_template = v8::DictionaryTemplate::New(isolate, properties);
    per_isolate_data->AddV8DictionaryTemplate(properties, just_template);
  }

  v8::MaybeLocal<v8::Value> values[] = {v8::Boolean::New(isolate, done), value};
  return just_template->NewInstance(script_state->GetContext(), values);
}

v8::Local<v8::Object> ESCreateIterResultObject(ScriptState* script_state,
                                               bool done,
                                               v8::Local<v8::Value> item1,
                                               v8::Local<v8::Value> item2) {
  v8::Isolate* isolate = script_state->GetIsolate();
  v8::Local<v8::Value> items[] = {item1, item2};
  v8::Local<v8::Array> value = v8::Array::New(isolate, items, std::size(items));
  return ESCreateIterResultObject(script_state, done, value);
}

}  // namespace bindings

bool V8UnpackIterationResult(ScriptState* script_state,
                             v8::Local<v8::Object> sync_iteration_result,
                             v8::Local<v8::Value>* out_value,
                             bool* out_done) {
  v8::Isolate* isolate = script_state->GetIsolate();
  v8::Local<v8::Context> context = script_state->GetContext();
  v8::TryCatch try_block(isolate);

  if (!sync_iteration_result->Get(context, V8AtomicString(isolate, "value"))
           .ToLocal(out_value)) {
    return false;
  }
  v8::Local<v8::Value> done_value;
  if (!sync_iteration_result->Get(context, V8AtomicString(isolate, "done"))
           .ToLocal(&done_value)) {
    return false;
  }
  *out_done = done_value->BooleanValue(isolate);
  return true;
}

}  // namespace blink
