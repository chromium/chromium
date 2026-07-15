// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shared_storage/shared_storage_delete_method.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_shared_storage_modifier_method_options.h"

namespace blink {

// static
SharedStorageDeleteMethod* SharedStorageDeleteMethod::Create(
    ScriptState* script_state,
    const String& key,
    ExceptionState& exception_state) {
  return MakeGarbageCollected<SharedStorageDeleteMethod>(
      script_state, key, SharedStorageModifierMethodOptions::Create(),
      exception_state);
}

// static
SharedStorageDeleteMethod* SharedStorageDeleteMethod::Create(
    ScriptState* script_state,
    const String& key,
    const SharedStorageModifierMethodOptions* options,
    ExceptionState& exception_state) {
  return MakeGarbageCollected<SharedStorageDeleteMethod>(
      script_state, key, options, exception_state);
}

SharedStorageDeleteMethod::SharedStorageDeleteMethod(
    ScriptState* script_state,
    const String& key,
    const SharedStorageModifierMethodOptions* options,
    ExceptionState& exception_state) {}

void SharedStorageDeleteMethod::Trace(Visitor* visitor) const {
  SharedStorageModifierMethod::Trace(visitor);
}

}  // namespace blink
