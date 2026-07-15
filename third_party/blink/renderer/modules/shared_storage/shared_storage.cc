// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shared_storage/shared_storage.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_shared_storage_modifier_method_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_shared_storage_run_operation_method_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_shared_storage_set_method_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_shared_storage_url_with_metadata.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_shared_storage_worklet_options.h"
#include "third_party/blink/renderer/modules/shared_storage/shared_storage_worklet.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class SharedStorage::IterationSource final
    : public PairAsyncIterable<SharedStorage>::IterationSource {
 public:
  IterationSource(ScriptState* script_state, Kind kind)
      : PairAsyncIterable<SharedStorage>::IterationSource(script_state, kind) {}

  void GetNextIterationResult() override {
    TakePendingPromiseResolver()->RejectWithDOMException(
        DOMExceptionCode::kOperationError, "Shared Storage is disabled.");
  }
};

SharedStorage::SharedStorage() = default;
SharedStorage::~SharedStorage() = default;

void SharedStorage::Trace(Visitor* visitor) const {
  visitor->Trace(shared_storage_worklet_);
  ScriptWrappable::Trace(visitor);
}

ScriptPromise<IDLAny> SharedStorage::set(ScriptState* script_state,
                                         const String& key,
                                         const String& value,
                                         ExceptionState& exception_state) {
  return set(script_state, key, value, SharedStorageSetMethodOptions::Create(),
             exception_state);
}

ScriptPromise<IDLAny> SharedStorage::set(
    ScriptState* script_state,
    const String& key,
    const String& value,
    const SharedStorageSetMethodOptions* options,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLAny>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  resolver->RejectWithDOMException(DOMExceptionCode::kOperationError,
                                   "Shared Storage is disabled.");
  return promise;
}

ScriptPromise<IDLAny> SharedStorage::append(ScriptState* script_state,
                                            const String& key,
                                            const String& value,
                                            ExceptionState& exception_state) {
  return append(script_state, key, value,
                SharedStorageModifierMethodOptions::Create(), exception_state);
}

ScriptPromise<IDLAny> SharedStorage::append(
    ScriptState* script_state,
    const String& key,
    const String& value,
    const SharedStorageModifierMethodOptions* options,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLAny>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  resolver->RejectWithDOMException(DOMExceptionCode::kOperationError,
                                   "Shared Storage is disabled.");
  return promise;
}

ScriptPromise<IDLAny> SharedStorage::Delete(ScriptState* script_state,
                                            const String& key,
                                            ExceptionState& exception_state) {
  return Delete(script_state, key, SharedStorageModifierMethodOptions::Create(),
                exception_state);
}

ScriptPromise<IDLAny> SharedStorage::Delete(
    ScriptState* script_state,
    const String& key,
    const SharedStorageModifierMethodOptions* options,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLAny>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  resolver->RejectWithDOMException(DOMExceptionCode::kOperationError,
                                   "Shared Storage is disabled.");
  return promise;
}

ScriptPromise<IDLAny> SharedStorage::clear(ScriptState* script_state,
                                           ExceptionState& exception_state) {
  return clear(script_state, SharedStorageModifierMethodOptions::Create(),
               exception_state);
}

ScriptPromise<IDLAny> SharedStorage::clear(
    ScriptState* script_state,
    const SharedStorageModifierMethodOptions* options,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLAny>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  resolver->RejectWithDOMException(DOMExceptionCode::kOperationError,
                                   "Shared Storage is disabled.");
  return promise;
}

ScriptPromise<IDLAny> SharedStorage::batchUpdate(
    ScriptState* script_state,
    const HeapVector<Member<SharedStorageModifierMethod>>& methods,
    ExceptionState& exception_state) {
  return batchUpdate(script_state, methods,
                     SharedStorageModifierMethodOptions::Create(),
                     exception_state);
}

ScriptPromise<IDLAny> SharedStorage::batchUpdate(
    ScriptState* script_state,
    const HeapVector<Member<SharedStorageModifierMethod>>& methods,
    const SharedStorageModifierMethodOptions* options,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLAny>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  resolver->RejectWithDOMException(DOMExceptionCode::kOperationError,
                                   "Shared Storage is disabled.");
  return promise;
}

ScriptPromise<IDLString> SharedStorage::get(ScriptState* script_state,
                                            const String& key,
                                            ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLString>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  resolver->RejectWithDOMException(DOMExceptionCode::kOperationError,
                                   "Shared Storage is disabled.");
  return promise;
}

ScriptPromise<IDLUnsignedLong> SharedStorage::length(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUnsignedLong>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  resolver->RejectWithDOMException(DOMExceptionCode::kOperationError,
                                   "Shared Storage is disabled.");
  return promise;
}

ScriptPromise<IDLDouble> SharedStorage::remainingBudget(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLDouble>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  resolver->RejectWithDOMException(DOMExceptionCode::kOperationError,
                                   "Shared Storage is disabled.");
  return promise;
}

ScriptValue SharedStorage::context(ScriptState* script_state,
                                   ExceptionState& exception_state) const {
  return ScriptValue();
}

ScriptPromise<V8SharedStorageResponse> SharedStorage::selectURL(
    ScriptState* script_state,
    const String& name,
    HeapVector<Member<SharedStorageUrlWithMetadata>> urls,
    ExceptionState& exception_state) {
  return selectURL(script_state, name, urls,
                   SharedStorageRunOperationMethodOptions::Create(),
                   exception_state);
}

ScriptPromise<V8SharedStorageResponse> SharedStorage::selectURL(
    ScriptState* script_state,
    const String& name,
    HeapVector<Member<SharedStorageUrlWithMetadata>> urls,
    const SharedStorageRunOperationMethodOptions* options,
    ExceptionState& exception_state) {
  SharedStorageWorklet* shared_storage_worklet =
      worklet(script_state, exception_state);
  CHECK(shared_storage_worklet);
  return shared_storage_worklet->selectURL(script_state, name, urls, options,
                                           exception_state);
}

ScriptPromise<IDLAny> SharedStorage::run(ScriptState* script_state,
                                         const String& name,
                                         ExceptionState& exception_state) {
  return run(script_state, name,
             SharedStorageRunOperationMethodOptions::Create(), exception_state);
}

ScriptPromise<IDLAny> SharedStorage::run(
    ScriptState* script_state,
    const String& name,
    const SharedStorageRunOperationMethodOptions* options,
    ExceptionState& exception_state) {
  SharedStorageWorklet* shared_storage_worklet =
      worklet(script_state, exception_state);
  CHECK(shared_storage_worklet);
  return shared_storage_worklet->run(script_state, name, options,
                                     exception_state);
}

ScriptPromise<SharedStorageWorklet> SharedStorage::createWorklet(
    ScriptState* script_state,
    const String& module_url,
    const SharedStorageWorkletOptions* options,
    ExceptionState& exception_state) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<SharedStorageWorklet>>(
          script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  resolver->RejectWithDOMException(DOMExceptionCode::kOperationError,
                                   "Shared Storage is disabled.");
  return promise;
}

SharedStorageWorklet* SharedStorage::worklet(ScriptState* script_state,
                                             ExceptionState& exception_state) {
  if (!shared_storage_worklet_) {
    shared_storage_worklet_ = SharedStorageWorklet::Create(script_state);
  }
  return shared_storage_worklet_.Get();
}

PairAsyncIterable<SharedStorage>::IterationSource*
SharedStorage::CreateIterationSource(
    ScriptState* script_state,
    typename PairAsyncIterable<SharedStorage>::IterationSource::Kind kind,
    ExceptionState& exception_state) {
  return MakeGarbageCollected<IterationSource>(script_state, kind);
}

}  // namespace blink
