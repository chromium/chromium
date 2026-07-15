// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shared_storage/shared_storage_worklet.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_shared_storage_run_operation_method_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_shared_storage_url_with_metadata.h"
#include "third_party/blink/renderer/core/html/fenced_frame/fenced_frame_config.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

// static
SharedStorageWorklet* SharedStorageWorklet::Create(ScriptState* script_state) {
  return MakeGarbageCollected<SharedStorageWorklet>();
}

SharedStorageWorklet::SharedStorageWorklet() = default;

void SharedStorageWorklet::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

ScriptPromise<IDLUndefined> SharedStorageWorklet::addModule(
    ScriptState* script_state,
    const String& module_url,
    const WorkletOptions* options,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  resolver->RejectWithDOMException(DOMExceptionCode::kOperationError,
                                   "Shared Storage is disabled.");
  return promise;
}

ScriptPromise<V8SharedStorageResponse> SharedStorageWorklet::selectURL(
    ScriptState* script_state,
    const String& name,
    HeapVector<Member<SharedStorageUrlWithMetadata>> urls,
    ExceptionState& exception_state) {
  return selectURL(script_state, name, urls,
                   SharedStorageRunOperationMethodOptions::Create(),
                   exception_state);
}

ScriptPromise<V8SharedStorageResponse> SharedStorageWorklet::selectURL(
    ScriptState* script_state,
    const String& name,
    HeapVector<Member<SharedStorageUrlWithMetadata>> urls,
    const SharedStorageRunOperationMethodOptions* options,
    ExceptionState& exception_state) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<V8SharedStorageResponse>>(
          script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  resolver->RejectWithDOMException(DOMExceptionCode::kOperationError,
                                   "Shared Storage is disabled.");
  return promise;
}

ScriptPromise<IDLAny> SharedStorageWorklet::run(
    ScriptState* script_state,
    const String& name,
    ExceptionState& exception_state) {
  return run(script_state, name,
             SharedStorageRunOperationMethodOptions::Create(), exception_state);
}

ScriptPromise<IDLAny> SharedStorageWorklet::run(
    ScriptState* script_state,
    const String& name,
    const SharedStorageRunOperationMethodOptions* options,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLAny>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  resolver->RejectWithDOMException(DOMExceptionCode::kOperationError,
                                   "Shared Storage is disabled.");
  return promise;
}

}  // namespace blink
