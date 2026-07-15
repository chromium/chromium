// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shared_storage/shared_storage_set_method.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_shared_storage_set_method_options.h"

namespace blink {

// static
SharedStorageSetMethod* SharedStorageSetMethod::Create(
    ScriptState* script_state,
    const String& key,
    const String& value,
    ExceptionState& exception_state) {
  return MakeGarbageCollected<SharedStorageSetMethod>(
      script_state, key, value, SharedStorageSetMethodOptions::Create(),
      exception_state);
}

// static
SharedStorageSetMethod* SharedStorageSetMethod::Create(
    ScriptState* script_state,
    const String& key,
    const String& value,
    const SharedStorageSetMethodOptions* options,
    ExceptionState& exception_state) {
  return MakeGarbageCollected<SharedStorageSetMethod>(script_state, key, value,
                                                      options, exception_state);
}

SharedStorageSetMethod::SharedStorageSetMethod(
    ScriptState* script_state,
    const String& key,
    const String& value,
    const SharedStorageSetMethodOptions* options,
    ExceptionState& exception_state) {}

void SharedStorageSetMethod::Trace(Visitor* visitor) const {
  SharedStorageModifierMethod::Trace(visitor);
}

}  // namespace blink
