// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shared_storage/shared_storage_delete_method.h"

#include "services/network/public/cpp/shared_storage_utils.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_shared_storage_modifier_method_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/shared_storage/util.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

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
    ExceptionState& exception_state) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  CHECK(execution_context->IsWindow() ||
        execution_context->IsSharedStorageWorkletGlobalScope());

  if (!CheckBrowsingContextIsValid(*script_state, exception_state)) {
    return;
  }

  if (execution_context->IsWindow() &&
      execution_context->GetSecurityOrigin()->IsOpaque()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      kOpaqueContextOriginCheckErrorMessage);
    return;
  }

  if (!CheckSharedStoragePermissionsPolicy(*execution_context,
                                           exception_state)) {
    return;
  }

  if (!network::IsValidSharedStorageKeyStringLength(key.length())) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      kInvalidKeyParameterLengthErrorMessage);
    return;
  }

  String with_lock = options->getWithLockOr(/*fallback_value=*/String());
  if (IsReservedLockName(with_lock)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      network::kReservedLockNameErrorMessage);
    return;
  }

  auto method =
      network::mojom::blink::SharedStorageModifierMethod::NewDeleteMethod(
          network::mojom::blink::SharedStorageDeleteMethod::New(key));

  std::optional<String> optional_with_lock =
      with_lock ? std::optional(with_lock) : std::nullopt;

  method_with_options_ =
      network::mojom::blink::SharedStorageModifierMethodWithOptions::New(
          std::move(method), optional_with_lock);
}

void SharedStorageDeleteMethod::Trace(Visitor* visitor) const {
  SharedStorageModifierMethod::Trace(visitor);
}

}  // namespace blink
