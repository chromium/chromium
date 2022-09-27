// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shared_storage/shared_storage_worklet.h"

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "third_party/blink/public/common/shared_storage/shared_storage_utils.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/shared_storage/shared_storage.h"
#include "third_party/blink/renderer/modules/shared_storage/util.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

SharedStorageWorklet::SharedStorageWorklet(SharedStorage* shared_storage)
    : shared_storage_(shared_storage) {}

void SharedStorageWorklet::Trace(Visitor* visitor) const {
  visitor->Trace(shared_storage_);
  ScriptWrappable::Trace(visitor);
}

ScriptPromise SharedStorageWorklet::addModule(ScriptState* script_state,
                                              const String& module_url,
                                              ExceptionState& exception_state) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  CHECK(execution_context->IsWindow());

  if (!CheckBrowsingContextIsValid(*script_state, exception_state)) {
    LogSharedStorageWorkletError(
        SharedStorageWorkletErrorType::kAddModuleWebVisible);
    return ScriptPromise();
  }

  KURL script_source_url = execution_context->CompleteURL(module_url);

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  if (!CheckSharedStoragePermissionsPolicy(*script_state, *execution_context,
                                           *resolver)) {
    LogSharedStorageWorkletError(
        SharedStorageWorkletErrorType::kAddModuleWebVisible);
    return promise;
  }

  if (!script_source_url.IsValid()) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kDataError,
        "The module script url is invalid."));
    LogSharedStorageWorkletError(
        SharedStorageWorkletErrorType::kAddModuleWebVisible);
    return promise;
  }

  scoped_refptr<SecurityOrigin> script_security_origin =
      SecurityOrigin::Create(script_source_url);

  if (!execution_context->GetSecurityOrigin()->IsSameOriginWith(
          script_security_origin.get())) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kDataError,
        "Only same origin module script is allowed."));
    LogSharedStorageWorkletError(
        SharedStorageWorkletErrorType::kAddModuleWebVisible);
    return promise;
  }

  shared_storage_->GetSharedStorageDocumentService(execution_context)
      ->AddModuleOnWorklet(
          script_source_url,
          WTF::BindOnce(
              [](ScriptPromiseResolver* resolver,
                 SharedStorageWorklet* shared_storage_worklet,
                 base::TimeTicks start_time, bool success,
                 const String& error_message) {
                DCHECK(resolver);
                ScriptState* script_state = resolver->GetScriptState();

                if (!success) {
                  ScriptState::Scope scope(script_state);
                  resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
                      script_state->GetIsolate(),
                      DOMExceptionCode::kOperationError, error_message));
                  LogSharedStorageWorkletError(
                      SharedStorageWorkletErrorType::kAddModuleWebVisible);
                  return;
                }

                base::UmaHistogramMediumTimes(
                    "Storage.SharedStorage.Document.Timing.AddModule",
                    base::TimeTicks::Now() - start_time);
                resolver->Resolve();
              },
              WrapPersistent(resolver), WrapPersistent(this), start_time));

  return promise;
}

}  // namespace blink
