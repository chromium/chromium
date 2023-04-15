// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shared_storage/util.h"

#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

bool CheckBrowsingContextIsValid(ScriptState& script_state,
                                 ExceptionState& exception_state) {
  if (!script_state.ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "context is not valid");
    return false;
  }

  ExecutionContext* execution_context = ExecutionContext::From(&script_state);
  if (execution_context->IsContextDestroyed()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "context has been destroyed");
    return false;
  }

  return true;
}

bool CheckSharedStoragePermissionsPolicy(ScriptState& script_state,
                                         ExecutionContext& execution_context,
                                         ScriptPromiseResolver& resolver) {
  // The worklet scope has to be created from the Window scope, thus the
  // shared-storage permissions policy feature must have been enabled. Besides,
  // the `SharedStorageWorkletGlobalScope` is currently given a null
  // `PermissionsPolicy`, so we shouldn't attempt to check the permissions
  // policy.
  //
  // TODO(crbug.com/1414951): When the `PermissionsPolicy` is properly set for
  // `SharedStorageWorkletGlobalScope`, we can remove this.
  if (execution_context.IsSharedStorageWorkletGlobalScope()) {
    return true;
  }

  if (!execution_context.IsFeatureEnabled(
          mojom::blink::PermissionsPolicyFeature::kSharedStorage)) {
    resolver.Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state.GetIsolate(), DOMExceptionCode::kInvalidAccessError,
        "The \"shared-storage\" Permissions Policy denied the method on "
        "window.sharedStorage."));

    return false;
  }

  return true;
}

}  // namespace blink
