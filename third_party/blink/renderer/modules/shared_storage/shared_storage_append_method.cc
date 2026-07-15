// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shared_storage/shared_storage_append_method.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_shared_storage_modifier_method_options.h"

namespace blink {

// static
SharedStorageAppendMethod* SharedStorageAppendMethod::Create(
    ScriptState* script_state,
    const String& key,
    const String& value,
    ExceptionState& exception_state) {
  return MakeGarbageCollected<SharedStorageAppendMethod>(
      script_state, key, value, SharedStorageModifierMethodOptions::Create(),
      exception_state);
}

// static
SharedStorageAppendMethod* SharedStorageAppendMethod::Create(
    ScriptState* script_state,
    const String& key,
    const String& value,
    const SharedStorageModifierMethodOptions* options,
    ExceptionState& exception_state) {
  return MakeGarbageCollected<SharedStorageAppendMethod>(
      script_state, key, value, options, exception_state);
}

SharedStorageAppendMethod::SharedStorageAppendMethod(
    ScriptState* script_state,
    const String& key,
    const String& value,
    const SharedStorageModifierMethodOptions* options,
    ExceptionState& exception_state) {}

void SharedStorageAppendMethod::Trace(Visitor* visitor) const {
  SharedStorageModifierMethod::Trace(visitor);
}

}  // namespace blink
