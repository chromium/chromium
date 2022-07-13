// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shared_storage/shared_storage_worklet.h"

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
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  CHECK(execution_context->IsWindow());

  if (!CheckBrowsingContextIsValid(*script_state, exception_state))
    return ScriptPromise();

  KURL script_source_url = execution_context->CompleteURL(module_url);

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  if (!CheckSharedStoragePermissionsPolicy(*script_state, *execution_context,
                                           *resolver)) {
    return promise;
  }

  if (!script_source_url.IsValid()) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kDataError,
        "The module script url is invalid."));
    return promise;
  }

  scoped_refptr<SecurityOrigin> script_security_origin =
      SecurityOrigin::Create(script_source_url);

  if (!execution_context->GetSecurityOrigin()->IsSameOriginWith(
          script_security_origin.get())) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kDataError,
        "Only same origin module script is allowed."));
    return promise;
  }

  shared_storage_->GetSharedStorageDocumentService(execution_context)
      ->AddModuleOnWorklet(
          script_source_url,
          WTF::Bind(
              [](ScriptPromiseResolver* resolver,
                 SharedStorageWorklet* shared_storage_worklet, bool success,
                 const String& error_message) {
                DCHECK(resolver);
                ScriptState* script_state = resolver->GetScriptState();

                if (!success) {
                  ScriptState::Scope scope(script_state);
                  resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
                      script_state->GetIsolate(),
                      DOMExceptionCode::kOperationError, error_message));
                  return;
                }

                resolver->Resolve();
              },
              WrapPersistent(resolver), WrapPersistent(this)));

  return promise;
}

}  // namespace blink
