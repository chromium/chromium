/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/indexeddb/idb_factory.h"

#include <memory>
#include <utility>

#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "third_party/blink/public/mojom/feature_observer/feature_observer.mojom-blink.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_binding_for_modules.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_idb_database_info.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/probe/async_task_context.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/modules/indexed_db_names.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_database.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_callbacks.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_callbacks_impl.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_transaction.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/gc_plugin.h"

namespace blink {

static const char kPermissionDeniedErrorMessage[] =
    "The user denied permission to access the database.";

IDBFactory::IDBFactory(ContextLifecycleNotifier* notifier)
    : factory_(notifier), feature_observer_(notifier) {}
IDBFactory::~IDBFactory() = default;

static bool IsContextValid(ExecutionContext* context) {
  if (auto* window = DynamicTo<LocalDOMWindow>(context))
    return window->GetFrame();
  DCHECK(context->IsWorkerGlobalScope());
  return true;
}

void IDBFactory::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(factory_);
  visitor->Trace(feature_observer_);
}

void IDBFactory::SetFactoryForTesting(
    HeapMojoRemote<mojom::blink::IDBFactory> factory) {
  factory_ = std::move(factory);
}

void IDBFactory::SetFactory(
    mojo::PendingRemote<mojom::blink::IDBFactory> factory,
    ExecutionContext* execution_context) {
  DCHECK(!factory_);

  mojo::PendingRemote<mojom::blink::FeatureObserver> feature_observer;
  execution_context->GetBrowserInterfaceBroker().GetInterface(
      feature_observer.InitWithNewPipeAndPassReceiver());

  task_runner_ = execution_context->GetTaskRunner(TaskType::kDatabaseAccess);
  factory_.Bind(std::move(factory), task_runner_);
  feature_observer_.Bind(std::move(feature_observer), task_runner_);
}

HeapMojoRemote<mojom::blink::IDBFactory>& IDBFactory::GetFactory(
    ExecutionContext* execution_context) {
  if (!factory_) {
    mojo::PendingRemote<mojom::blink::IDBFactory> factory;
    execution_context->GetBrowserInterfaceBroker().GetInterface(
        factory.InitWithNewPipeAndPassReceiver());
    SetFactory(std::move(factory), execution_context);
  }
  return factory_;
}

ScriptPromise IDBFactory::GetDatabaseInfo(ScriptState* script_state,
                                          ExceptionState& exception_state) {
  // The BlinkIDL definition for GetDatabaseInfo() already has a [Measure]
  // attribute, so the kIndexedDBRead use counter must be explicitly updated.
  UseCounter::Count(ExecutionContext::From(script_state),
                    WebFeature::kIndexedDBRead);

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

  ExecutionContext* context = ExecutionContext::From(script_state);
  DCHECK(context->IsContextThread());

  if (!IsContextValid(context)) {
    resolver->Reject();
    return resolver->Promise();
  }

  if (!ExecutionContext::From(script_state)
           ->GetSecurityOrigin()
           ->CanAccessDatabase()) {
    exception_state.ThrowSecurityError(
        "Access to the IndexedDB API is denied in this context.");
    resolver->Reject();
    return resolver->Promise();
  }

  AllowIndexedDB(
      context,
      WTF::BindOnce(&IDBFactory::GetDatabaseInfoImpl, WrapWeakPersistent(this),
                    WrapPersistent(context), WrapPersistent(resolver)));
  return resolver->Promise();
}

void IDBFactory::GetDatabaseInfoImpl(ExecutionContext* context,
                                     ScriptPromiseResolver* resolver) {
  ScriptState* script_state = resolver->GetScriptState();

  if (context->IsContextDestroyed()) {
    resolver->Reject();
    return;
  }

  if (!allowed_.value()) {
    ScriptState::Scope scope(script_state);
    resolver->Reject(V8ThrowDOMException::CreateOrDie(
        script_state->GetIsolate(), DOMExceptionCode::kUnknownError,
        kPermissionDeniedErrorMessage));
    return;
  }

  GetFactory(context)->GetDatabaseInfo(
      WTF::BindOnce(&IDBFactory::DidGetDatabaseInfo, WrapWeakPersistent(this),
                    WrapPersistent(resolver)));
}

void IDBFactory::DidGetDatabaseInfo(
    ScriptPromiseResolver* resolver,
    Vector<mojom::blink::IDBNameAndVersionPtr> names_and_versions,
    mojom::blink::IDBErrorPtr error) {
  if (!resolver) {
    return;
  }
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid()) {
    return;
  }

  if (error) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        static_cast<DOMExceptionCode>(error->error_code),
        error->error_message));
    return;
  }

  HeapVector<Member<IDBDatabaseInfo>> name_and_version_list;
  name_and_version_list.ReserveInitialCapacity(name_and_version_list.size());
  for (const mojom::blink::IDBNameAndVersionPtr& name_version :
       names_and_versions) {
    IDBDatabaseInfo* idb_info = IDBDatabaseInfo::Create();
    idb_info->setName(name_version->name);
    idb_info->setVersion(name_version->version);
    name_and_version_list.push_back(idb_info);
  }

  resolver->Resolve(name_and_version_list);
}

void IDBFactory::GetDatabaseInfoForDevTools(
    ScriptState* script_state,
    mojom::blink::IDBFactory::GetDatabaseInfoCallback callback) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  DCHECK(context->IsContextThread());

  // TODO(jsbell): Used only by inspector; remove unneeded checks/exceptions?
  if (!IsContextValid(context)) {
    return;
  }

  if (!context->GetSecurityOrigin()->CanAccessDatabase()) {
    std::move(callback).Run(
        {}, mojom::blink::IDBError::New(
                mojom::blink::IDBException::kAbortError,
                "Access to the IndexedDB API is denied in this context."));
    return;
  }

  AllowIndexedDB(
      context, WTF::BindOnce(&IDBFactory::GetDatabaseInfoForDevToolsHelper,
                             WrapWeakPersistent(this), WrapPersistent(context),
                             std::move(callback)));
  return;
}

void IDBFactory::GetDatabaseInfoForDevToolsHelper(
    ExecutionContext* context,
    mojom::blink::IDBFactory::GetDatabaseInfoCallback callback) {
  if (context->IsContextDestroyed()) {
    std::move(callback).Run(
        {}, mojom::blink::IDBError::New(
                mojom::blink::IDBException::kAbortError,
                "Access to the IndexedDB API is denied in this context."));
    return;
  }

  if (!allowed_.value()) {
    std::move(callback).Run({}, mojom::blink::IDBError::New(
                                    mojom::blink::IDBException::kUnknownError,
                                    kPermissionDeniedErrorMessage));
    return;
  }

  GetFactory(context)->GetDatabaseInfo(std::move(callback));
}

IDBOpenDBRequest* IDBFactory::open(ScriptState* script_state,
                                   const String& name,
                                   uint64_t version,
                                   ExceptionState& exception_state) {
  if (!version) {
    exception_state.ThrowTypeError("The version provided must not be 0.");
    return nullptr;
  }
  return OpenInternal(script_state, name, version, exception_state);
}

IDBOpenDBRequest* IDBFactory::OpenInternal(ScriptState* script_state,
                                           const String& name,
                                           int64_t version,
                                           ExceptionState& exception_state) {
  TRACE_EVENT1("IndexedDB", "IDBFactory::open", "name", name.Utf8());
  IDBRequest::AsyncTraceState metrics("IDBFactory::open");
  DCHECK(version >= 1 || version == IDBDatabaseMetadata::kNoVersion);

  ExecutionContext* context = ExecutionContext::From(script_state);
  DCHECK(context->IsContextThread());

  if (!IsContextValid(context))
    return nullptr;
  if (!context->GetSecurityOrigin()->CanAccessDatabase()) {
    exception_state.ThrowSecurityError(
        "access to the Indexed Database API is denied in this context.");
    return nullptr;
  }

  if (context->GetSecurityOrigin()->IsLocal()) {
    UseCounter::Count(context, WebFeature::kFileAccessedDatabase);
  }

  int64_t transaction_id = IDBDatabase::NextTransactionId();

  auto& factory = GetFactory(context);

  auto transaction_backend = std::make_unique<WebIDBTransaction>(
      context->GetTaskRunner(TaskType::kDatabaseAccess), transaction_id);
  mojo::PendingAssociatedReceiver<mojom::blink::IDBTransaction>
      transaction_receiver = transaction_backend->CreateReceiver();
  mojo::PendingAssociatedRemote<mojom::blink::IDBDatabaseCallbacks>
      callbacks_remote;
  auto* request = MakeGarbageCollected<IDBOpenDBRequest>(
      script_state, callbacks_remote.InitWithNewEndpointAndPassReceiver(),
      std::move(transaction_backend), transaction_id, version,
      std::move(metrics), GetObservedFeature());

  AllowIndexedDB(
      context,
      WTF::BindOnce(&IDBFactory::OpenInternalImpl, WrapWeakPersistent(this),
                    WrapPersistent(request), std::move(callbacks_remote),
                    std::move(transaction_receiver), std::ref(factory), name,
                    version, transaction_id));
  return request;
}

void IDBFactory::OpenInternalImpl(
    IDBOpenDBRequest* request,
    mojo::PendingAssociatedRemote<mojom::blink::IDBDatabaseCallbacks>
        callbacks_remote,
    mojo::PendingAssociatedReceiver<mojom::blink::IDBTransaction>
        transaction_receiver,
    HeapMojoRemote<mojom::blink::IDBFactory>& factory,
    const String& name,
    int64_t version,
    int64_t transaction_id) {
  if (!request->GetExecutionContext() || !allowed_.value()) {
    request->HandleResponse(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError, kPermissionDeniedErrorMessage));
    return;
  }

  auto callbacks = request->CreateWebCallbacks();
  callbacks->SetState(WebIDBCallbacksImpl::kNoTransaction);
  factory->Open(GetCallbacksProxy(std::move(callbacks)),
                std::move(callbacks_remote), name, version,
                std::move(transaction_receiver), transaction_id);
}

IDBOpenDBRequest* IDBFactory::open(ScriptState* script_state,
                                   const String& name,
                                   ExceptionState& exception_state) {
  return OpenInternal(script_state, name, IDBDatabaseMetadata::kNoVersion,
                      exception_state);
}

IDBOpenDBRequest* IDBFactory::deleteDatabase(ScriptState* script_state,
                                             const String& name,
                                             ExceptionState& exception_state) {
  return DeleteDatabaseInternal(script_state, name, exception_state,
                                /*force_close=*/false);
}

IDBOpenDBRequest* IDBFactory::CloseConnectionsAndDeleteDatabase(
    ScriptState* script_state,
    const String& name,
    ExceptionState& exception_state) {
  // TODO(jsbell): Used only by inspector; remove unneeded checks/exceptions?
  return DeleteDatabaseInternal(script_state, name, exception_state,
                                /*force_close=*/true);
}

IDBOpenDBRequest* IDBFactory::DeleteDatabaseInternal(
    ScriptState* script_state,
    const String& name,
    ExceptionState& exception_state,
    bool force_close) {
  TRACE_EVENT1("IndexedDB", "IDBFactory::deleteDatabase", "name", name.Utf8());
  IDBRequest::AsyncTraceState metrics("IDBFactory::deleteDatabase");
  ExecutionContext* context = ExecutionContext::From(script_state);

  DCHECK(context->IsContextThread());
  if (!IsContextValid(context))
    return nullptr;
  if (!context->GetSecurityOrigin()->CanAccessDatabase()) {
    exception_state.ThrowSecurityError(
        "access to the Indexed Database API is denied in this context.");
    return nullptr;
  }
  if (context->GetSecurityOrigin()->IsLocal()) {
    UseCounter::Count(context, WebFeature::kFileAccessedDatabase);
  }

  auto& factory = GetFactory(context);

  auto* request = MakeGarbageCollected<IDBOpenDBRequest>(
      script_state,
      /*callbacks_receiver=*/mojo::NullAssociatedReceiver(),
      /*IDBTransactionAssociatedPtr=*/nullptr, 0,
      IDBDatabaseMetadata::kDefaultVersion, std::move(metrics),
      GetObservedFeature());

  AllowIndexedDB(
      context, WTF::BindOnce(&IDBFactory::DeleteDatabaseInternalImpl,
                             WrapWeakPersistent(this), WrapPersistent(request),
                             std::ref(factory), name, force_close));
  return request;
}

void IDBFactory::DeleteDatabaseInternalImpl(
    IDBOpenDBRequest* request,
    HeapMojoRemote<mojom::blink::IDBFactory>& factory,
    const String& name,
    bool force_close) {
  if (!request->GetExecutionContext()) {
    return;
  }

  if (!allowed_.value()) {
    request->HandleResponse(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError, kPermissionDeniedErrorMessage));
    return;
  }

  auto callbacks = request->CreateWebCallbacks();
  callbacks->SetState(WebIDBCallbacksImpl::kNoTransaction);
  factory->DeleteDatabase(GetCallbacksProxy(std::move(callbacks)), name,
                          force_close);
}

int16_t IDBFactory::cmp(ScriptState* script_state,
                        const ScriptValue& first_value,
                        const ScriptValue& second_value,
                        ExceptionState& exception_state) {
  const std::unique_ptr<IDBKey> first =
      ScriptValue::To<std::unique_ptr<IDBKey>>(script_state->GetIsolate(),
                                               first_value, exception_state);
  if (exception_state.HadException())
    return 0;
  DCHECK(first);
  if (!first->IsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      IDBDatabase::kNotValidKeyErrorMessage);
    return 0;
  }

  const std::unique_ptr<IDBKey> second =
      ScriptValue::To<std::unique_ptr<IDBKey>>(script_state->GetIsolate(),
                                               second_value, exception_state);
  if (exception_state.HadException())
    return 0;
  DCHECK(second);
  if (!second->IsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      IDBDatabase::kNotValidKeyErrorMessage);
    return 0;
  }

  return static_cast<int16_t>(first->Compare(second.get()));
}

void IDBFactory::AllowIndexedDB(ExecutionContext* context,
                                base::OnceCallback<void()> callback) {
  DCHECK(context->IsContextThread());
  SECURITY_DCHECK(context->IsWindow() || context->IsWorkerGlobalScope());
  auto wrapped_callback =
      WTF::BindOnce(&IDBFactory::DidAllowIndexedDB, WrapWeakPersistent(this),
                    std::move(callback));

  if (allowed_.has_value()) {
    std::move(wrapped_callback).Run(allowed_.value());
    return;
  }

  WebContentSettingsClient* settings_client = nullptr;

  if (auto* window = DynamicTo<LocalDOMWindow>(context)) {
    LocalFrame* frame = window->GetFrame();
    if (!frame) {
      std::move(wrapped_callback).Run(false);
      return;
    }
    settings_client = window->GetFrame()->GetContentSettingsClient();
  } else {
    settings_client = To<WorkerGlobalScope>(context)->ContentSettingsClient();
  }

  if (!settings_client) {
    std::move(wrapped_callback).Run(true);
    return;
  }
  settings_client->AllowStorageAccess(
      WebContentSettingsClient::StorageType::kIndexedDB,
      std::move(wrapped_callback));
}

void IDBFactory::DidAllowIndexedDB(base::OnceCallback<void()> callback,
                                   bool allow_access) {
  if (allowed_.has_value()) {
    DCHECK_EQ(allowed_.value(), allow_access);
  } else {
    allowed_ = allow_access;
  }

  std::move(callback).Run();
  return;
}

mojo::PendingAssociatedRemote<mojom::blink::IDBCallbacks>
IDBFactory::GetCallbacksProxy(std::unique_ptr<WebIDBCallbacks> callbacks_impl) {
  mojo::PendingAssociatedRemote<mojom::blink::IDBCallbacks> pending_callbacks;
  mojo::MakeSelfOwnedAssociatedReceiver(
      std::move(callbacks_impl),
      pending_callbacks.InitWithNewEndpointAndPassReceiver(), task_runner_);
  return pending_callbacks;
}

mojo::PendingRemote<mojom::blink::ObservedFeature>
IDBFactory::GetObservedFeature() {
  mojo::PendingRemote<mojom::blink::ObservedFeature> feature;
  feature_observer_->Register(
      feature.InitWithNewPipeAndPassReceiver(),
      mojom::blink::ObservedFeatureType::kIndexedDBConnection);
  return feature;
}

}  // namespace blink
