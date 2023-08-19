// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/v8_compile_hints_hashing.h"
#include "base/hash/hash.h"

namespace blink::v8_compile_hints {

uint32_t ScriptNameHash(v8::Local<v8::Value> name_value,
                        v8::Local<v8::Context> context,
                        v8::Isolate* isolate) {
  v8::Local<v8::String> name_string;
  if (!name_value->ToString(context).ToLocal(&name_string)) {
    return 0;
  }
  int name_length = name_string->Utf8Length(isolate);
  if (name_length == 0) {
    return 0;
  }

  std::string name_std_string(name_length + 1, '\0');
  name_string->WriteUtf8(isolate, &name_std_string[0]);

  // We need the hash function to be stable across computers, thus using
  // PersistentHash.
  return base::PersistentHash(name_std_string.c_str(), name_length);
}

uint32_t CombineHash(uint32_t script_name_hash, int position) {
  uint32_t function_position_data[2];
  function_position_data[0] = script_name_hash;
  function_position_data[1] = position;
  return base::PersistentHash(function_position_data, 2 * sizeof(int32_t));
}

}  // namespace blink::v8_compile_hints
