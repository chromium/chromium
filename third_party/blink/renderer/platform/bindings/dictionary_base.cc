// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/dictionary_base.h"

#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-object.h"

namespace blink {

namespace bindings {

v8::MaybeLocal<v8::Value> DictionaryBase::ToV8Value(
    ScriptState* script_state) const {
  v8::Local<v8::Object> v8_dictionary;
  {
    v8::Context::Scope context_scope(script_state->GetContext());
    v8_dictionary = v8::Object::New(script_state->GetIsolate());
  }

  if (!FillV8ObjectWithMembers(script_state, v8_dictionary))
    return v8::MaybeLocal<v8::Value>();

  return v8_dictionary;
}

}  // namespace bindings

}  // namespace blink
