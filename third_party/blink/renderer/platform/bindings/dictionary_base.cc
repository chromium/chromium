// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/dictionary_base.h"

#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "v8/include/v8-object.h"

namespace blink {

namespace bindings {

v8::Local<v8::Value> DictionaryBase::ToV8(ScriptState* script_state) const {
  const void* const key = TemplateKey();
  auto* per_isolate_data = V8PerIsolateData::From(script_state->GetIsolate());
  v8::MaybeLocal<v8::DictionaryTemplate> maybe_template =
      per_isolate_data->FindV8DictionaryTemplate(key);
  v8::Local<v8::DictionaryTemplate> just_template;
  if (!maybe_template.IsEmpty()) {
    just_template = maybe_template.ToLocalChecked();
  } else {
    WTF::Vector<std::string_view> properties;
    FillTemplateProperties(properties);
    just_template = v8::DictionaryTemplate::New(
        script_state->GetIsolate(), v8::MemorySpan<const std::string_view>(
                                        properties.data(), properties.size()));
    per_isolate_data->AddV8DictionaryTemplate(key, just_template);
  }
  return FillValues(script_state, just_template).As<v8::Value>();
}

}  // namespace bindings

}  // namespace blink
