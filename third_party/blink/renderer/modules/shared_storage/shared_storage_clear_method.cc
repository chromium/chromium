// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shared_storage/shared_storage_clear_method.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_shared_storage_modifier_method_options.h"

namespace blink {

// static
SharedStorageClearMethod* SharedStorageClearMethod::Create(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  return MakeGarbageCollected<SharedStorageClearMethod>(
      script_state, SharedStorageModifierMethodOptions::Create(),
      exception_state);
}

// static
SharedStorageClearMethod* SharedStorageClearMethod::Create(
    ScriptState* script_state,
    const SharedStorageModifierMethodOptions* options,
    ExceptionState& exception_state) {
  return MakeGarbageCollected<SharedStorageClearMethod>(script_state, options,
                                                        exception_state);
}

SharedStorageClearMethod::SharedStorageClearMethod(
    ScriptState* script_state,
    const SharedStorageModifierMethodOptions* options,
    ExceptionState& exception_state) {}

void SharedStorageClearMethod::Trace(Visitor* visitor) const {
  SharedStorageModifierMethod::Trace(visitor);
}

}  // namespace blink
