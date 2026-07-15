// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shared_storage/shared_storage_worklet_global_scope.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

SharedStorageWorkletGlobalScope::SharedStorageWorkletGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    WorkerThread* thread)
    : WorkletGlobalScope(std::move(creation_params),
                         thread->GetWorkerReportingProxy(),
                         thread) {}

SharedStorageWorkletGlobalScope::~SharedStorageWorkletGlobalScope() = default;

void SharedStorageWorkletGlobalScope::Register(
    const String& name,
    V8NoArgumentConstructor* operation_ctor,
    ExceptionState& exception_state) {}

void SharedStorageWorkletGlobalScope::Trace(Visitor* visitor) const {
  Supplementable<SharedStorageWorkletGlobalScope>::Trace(visitor);
  WorkletGlobalScope::Trace(visitor);
}

SharedStorage* SharedStorageWorkletGlobalScope::sharedStorage(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  return nullptr;
}

PrivateAggregation* SharedStorageWorkletGlobalScope::privateAggregation(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  return nullptr;
}

Crypto* SharedStorageWorkletGlobalScope::crypto(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  return nullptr;
}

ScriptPromise<IDLSequence<StorageInterestGroup>>
SharedStorageWorkletGlobalScope::interestGroups(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolver<IDLSequence<StorageInterestGroup>>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  resolver->RejectWithDOMException(DOMExceptionCode::kOperationError,
                                   "Shared Storage is disabled.");
  return promise;
}

SharedStorageWorkletNavigator* SharedStorageWorkletGlobalScope::Navigator(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  return nullptr;
}

}  // namespace blink
