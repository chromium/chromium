// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shared_storage/shared_storage.h"

#include <memory>
#include <utility>

#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_binding_for_modules.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_shared_storage_run_operation_method_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_shared_storage_set_method_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/probe/async_task_id.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/modules/shared_storage/shared_storage_worklet.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

SharedStorage::SharedStorage() = default;
SharedStorage::~SharedStorage() = default;

void SharedStorage::Trace(Visitor* visitor) const {
  visitor->Trace(shared_storage_worklet_);
  ScriptWrappable::Trace(visitor);
}

ScriptPromise SharedStorage::set(ScriptState* script_state,
                                 const String& key,
                                 const String& value,
                                 ExceptionState& exception_state) {
  return set(script_state, key, value, SharedStorageSetMethodOptions::Create(),
             exception_state);
}

ScriptPromise SharedStorage::set(ScriptState* script_state,
                                 const String& key,
                                 const String& value,
                                 const SharedStorageSetMethodOptions* options,
                                 ExceptionState& exception_state) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  CHECK(execution_context->IsWindow());

  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  // TODO: handle the operation
  resolver->Resolve();
  return promise;
}

ScriptPromise SharedStorage::append(ScriptState* script_state,
                                    const String& key,
                                    const String& value,
                                    ExceptionState& exception_state) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  CHECK(execution_context->IsWindow());

  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  // TODO: handle the operation
  resolver->Resolve();
  return promise;
}

ScriptPromise SharedStorage::Delete(ScriptState* script_state,
                                    const String& key,
                                    ExceptionState& exception_state) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  CHECK(execution_context->IsWindow());

  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  // TODO: handle the operation
  resolver->Resolve();
  return promise;
}

ScriptPromise SharedStorage::clear(ScriptState* script_state,
                                   ExceptionState& exception_state) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  CHECK(execution_context->IsWindow());

  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  // TODO: handle the operation
  resolver->Resolve();
  return promise;
}

ScriptPromise SharedStorage::runURLSelectionOperation(
    ScriptState* script_state,
    const String& name,
    const Vector<String>& urls,
    ExceptionState& exception_state) {
  return runURLSelectionOperation(
      script_state, name, urls,
      SharedStorageRunOperationMethodOptions::Create(), exception_state);
}

ScriptPromise SharedStorage::runURLSelectionOperation(
    ScriptState* script_state,
    const String& name,
    const Vector<String>& urls,
    const SharedStorageRunOperationMethodOptions* options,
    ExceptionState& exception_state) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  CHECK(execution_context->IsWindow());

  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  if (urls.size() >
      static_cast<unsigned int>(
          features::kSharedStorageURLSelectionOperationInputURLSizeLimit
              .Get())) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kDataError,
        "Length of the \"urls\" parameter exceeds the size limit."));
    return promise;
  }

  Vector<KURL> converted_urls;
  converted_urls.ReserveInitialCapacity(urls.size());

  for (const String& url : urls) {
    KURL converted_url = execution_context->CompleteURL(url);
    if (!converted_url.IsValid()) {
      resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
          script_state->GetIsolate(), DOMExceptionCode::kDataError,
          "The url \"" + url + "\" is invalid."));
      return promise;
    }

    converted_urls.push_back(converted_url);
  }

  // TODO: handle the operation
  return promise;
}

ScriptPromise SharedStorage::runOperation(ScriptState* script_state,
                                          const String& name,
                                          ExceptionState& exception_state) {
  return runOperation(script_state, name,
                      SharedStorageRunOperationMethodOptions::Create(),
                      exception_state);
}

ScriptPromise SharedStorage::runOperation(
    ScriptState* script_state,
    const String& name,
    const SharedStorageRunOperationMethodOptions* options,
    ExceptionState& exception_state) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  CHECK(execution_context->IsWindow());

  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  resolver->Resolve();

  // TODO: handle the operation
  return promise;
}

SharedStorageWorklet* SharedStorage::worklet(ScriptState* script_state,
                                             ExceptionState& exception_state) {
  if (shared_storage_worklet_)
    return shared_storage_worklet_.Get();

  shared_storage_worklet_ = MakeGarbageCollected<SharedStorageWorklet>(this);

  return shared_storage_worklet_.Get();
}

mojom::blink::SharedStorageDocumentService*
SharedStorage::GetSharedStorageDocumentService(
    ExecutionContext* execution_context) {
  CHECK(execution_context->IsWindow());
  if (!shared_storage_document_service_.is_bound()) {
    execution_context->GetBrowserInterfaceBroker().GetInterface(
        shared_storage_document_service_.BindNewPipeAndPassReceiver(
            execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  }
  return shared_storage_document_service_.get();
}

}  // namespace blink
