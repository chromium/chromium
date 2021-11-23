// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shared_storage/shared_storage.h"

#include <memory>
#include <utility>

#include "base/ignore_result.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
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
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

namespace {

// Use the native v8::ValueSerializer here as opposed to using
// blink::V8ScriptValueSerializer. It's capable of serializing objects of
// primitive types. It's TBD whether we want to support any other non-primitive
// types supported by blink::V8ScriptValueSerializer.
bool Serialize(ScriptState* script_state,
               const SharedStorageRunOperationMethodOptions* options,
               ExceptionState& exception_state,
               Vector<uint8_t>& output) {
  DCHECK(output.IsEmpty());

  if (!options->hasData())
    return true;

  v8::Isolate* isolate = script_state->GetIsolate();
  v8::ValueSerializer serializer(isolate);

  v8::TryCatch try_catch(isolate);

  bool wrote_value;
  if (!serializer
           .WriteValue(script_state->GetContext(), options->data().V8Value())
           .To(&wrote_value)) {
    DCHECK(try_catch.HasCaught());
    exception_state.RethrowV8Exception(try_catch.Exception());
    return false;
  }

  DCHECK(wrote_value);

  std::pair<uint8_t*, size_t> buffer = serializer.Release();

  output.ReserveInitialCapacity(SafeCast<wtf_size_t>(buffer.second));
  output.Append(buffer.first, static_cast<wtf_size_t>(buffer.second));
  DCHECK_EQ(output.size(), buffer.second);

  free(buffer.first);

  return true;
}

}  // namespace

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

  Vector<uint8_t> serialized_data;
  if (!Serialize(script_state, options, exception_state, serialized_data))
    return ScriptPromise();

  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  resolver->Resolve();

  GetSharedStorageDocumentService(execution_context)
      ->RunOperationOnWorklet(name, std::move(serialized_data));

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
    LocalDOMWindow* window = To<LocalDOMWindow>(execution_context);
    if (!window->GetFrame())
      return GetEmptySharedStorageDocumentService();

    window->GetFrame()->GetRemoteNavigationAssociatedInterfaces()->GetInterface(
        shared_storage_document_service_.BindNewEndpointAndPassReceiver(
            execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  }
  return shared_storage_document_service_.get();
}

mojom::blink::SharedStorageDocumentService*
SharedStorage::GetEmptySharedStorageDocumentService() {
  static base::SequenceLocalStorageSlot<
      mojo::Remote<mojom::blink::SharedStorageDocumentService>>
      slot;

  if (!slot.GetValuePointer()) {
    auto& remote = slot.GetOrCreateValue();
    mojo::PendingRemote<mojom::blink::SharedStorageDocumentService>
        pending_remote;
    ignore_result(pending_remote.InitWithNewPipeAndPassReceiver());
    remote.Bind(std::move(pending_remote), base::ThreadTaskRunnerHandle::Get());
  }
  return slot.GetOrCreateValue().get();
}

}  // namespace blink
