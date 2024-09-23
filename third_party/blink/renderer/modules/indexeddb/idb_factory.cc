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

#include "base/task/bind_post_task.h"
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
#include "third_party/blink/renderer/modules/indexeddb/idb_factory_client.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key.h"
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

IDBFactory::IDBFactory(ExecutionContext* context)
    : ExecutionContextLifecycleObserver(context),
      remote_(context),
      feature_observer_(context) {}
IDBFactory::~IDBFactory() = default;

static bool IsContextValid(ExecutionContext* context) {
  if (!context || context->IsContextDestroyed()) {
    return false;
  }
  if (auto* window = DynamicTo<LocalDOMWindow>(context)) {
    return window->GetFrame();
  }
  DCHECK(context->IsWorkerGlobalScope());
  return true;
}

void IDBFactory::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  visitor->Trace(remote_);
  visitor->Trace(feature_observer_);
  visitor->Trace(weak_factory_);
}

void IDBFactory::SetRemote(
    mojo::PendingRemote<mojom::blink::IDBFactory> remote) {
  DCHECK(!remote_);
  remote_.Bind(std::move(remote), GetTaskRunner());
}

ExecutionContext* IDBFactory::GetValidContext(ScriptState* script_state) {
  ExecutionContext* context = GetExecutionContext();
  ExecutionContext* script_context = ExecutionContext::From(script_state);
  CHECK(script_context);
  if (context) {
    CHECK_EQ(context, script_context);
  } else if (!context) {
    CHECK(script_context->IsContextDestroyed());
  }
  if (IsContextValid(context)) {
    return context;
  }
  return nullptr;
}

HeapMojoRemote<mojom::blink::IDBFactory>& IDBFactory::GetRemote() {
  if (!remote_) {
    mojo::PendingRemote<mojom::blink::IDBFactory> remote;
    GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
        remote.InitWithNewPipeAndPassReceiver());
    SetRemote(std::move(remote));
  }
  return remote_;
}

scoped_refptr<base::SingleThreadTaskRunner> IDBFactory::GetTaskRunner() {
  CHECK(GetExecutionContext() && !GetExecutionContext()->IsContextDestroyed());
  return GetExecutionContext()->GetTaskRunner(TaskType::kDatabaseAccess);
}

ScriptPromise<IDLSequence<IDBDatabaseInfo>> IDBFactory::GetDatabaseInfo(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  ExecutionContext* context = GetValidContext(script_state);
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLSequence<IDBDatabaseInfo>>>(
          script_state, exception_state.GetContext());
  if (!context) {
    resolver->Reject();
    return resolver->Promise();
  }

  // The BlinkIDL definition for GetDatabaseInfo() already has a [Measure]
  // attribute, so the kIndexedDBRead use counter must be explicitly updated.
  UseCounter::Count(context, WebFeature::kIndexedDBRead);
  CHECK(context->IsContextThread());

  if (!context->GetSecurityOrigin()->CanAccessDatabase()) {
    exception_state.ThrowSecurityError(
        "Access to the IndexedDB API is denied in this context.");
    resolver->Reject();
    return resolver->Promise();
  }

  AllowIndexedDB(WTF::BindOnce(&IDBFactory::GetDatabaseInfoImpl,
                               WrapPersistent(weak_factory_.GetWeakCell()),
                               WrapPersistent(resolver)));
  return resolver->Promise();
}

void IDBFactory::GetDatabaseInfoImpl(
    ScriptPromiseResolver<IDLSequence<IDBDatabaseInfo>>* resolver) {
  if (!allowed_.value()) {
    ScriptState* script_state = resolver->GetScriptState();
    ScriptState::Scope scope(script_state);
    resolver->Reject(V8ThrowDOMException::CreateOrDie(
        script_state->GetIsolate(), DOMExceptionCode::kUnknownError,
        kPermissionDeniedErrorMessage));
    return;
  }

  GetRemote()->GetDatabaseInfo(WTF::BindOnce(
      &IDBFactory::DidGetDatabaseInfo,
      WrapPersistent(weak_factory_.GetWeakCell()), WrapPersistent(resolver)));
}

void IDBFactory::DidGetDatabaseInfo(
    ScriptPromiseResolver<IDLSequence<IDBDatabaseInfo>>* resolver,
    Vector<mojom::blink::IDBNameAndVersionPtr> names_and_versions,
    mojom::blink::IDBErrorPtr error) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid()) {
    return;
  }

  if (error->error_code != mojom::blink::IDBException::kNoError) {
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
    mojom::blink::IDBFactory::GetDatabaseInfoCallback callback) {
  ExecutionContext* context = GetExecutionContext();

  // TODO(jsbell): Used only by inspector; remove unneeded checks/exceptions?
  if (!IsContextValid(context) ||
      !context->GetSecurityOrigin()->CanAccessDatabase()) {
    std::move(callback).Run(
        {}, mojom::blink::IDBError::New(
                mojom::blink::IDBException::kAbortError,
                "Access to the IndexedDB API is denied in this context."));
    return;
  }

  DCHECK(context->IsContextThread());

  AllowIndexedDB(WTF::BindOnce(&IDBFactory::GetDatabaseInfoForDevToolsHelper,
                               WrapPersistent(weak_factory_.GetWeakCell()),
                               std::move(callback)));
}

void IDBFactory::ContextDestroyed() {
  weak_factory_.Invalidate();
}

void IDBFactory::GetDatabaseInfoForDevToolsHelper(
    mojom::blink::IDBFactory::GetDatabaseInfoCallback callback) {
  if (!allowed_.value()) {
    std::move(callback).Run({}, mojom::blink::IDBError::New(
                                    mojom::blink::IDBException::kUnknownError,
                                    kPermissionDeniedErrorMessage));
    return;
  }

  GetRemote()->GetDatabaseInfo(std::move(callback));
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
  IDBRequest::AsyncTraceState metrics(IDBRequest::TypeForMetrics::kFactoryOpen);
  DCHECK(version >= 1 || version == IDBDatabaseMetadata::kNoVersion);

  ExecutionContext* context = GetValidContext(script_state);
  if (!context) {
    // TODO(crbug.com/1473972): throw exception?
    return nullptr;
  }
  DCHECK(context->IsContextThread());
  if (!context->GetSecurityOrigin()->CanAccessDatabase()) {
    exception_state.ThrowSecurityError(
        "access to the Indexed Database API is denied in this context.");
    return nullptr;
  }

  if (context->GetSecurityOrigin()->IsLocal()) {
    UseCounter::Count(context, WebFeature::kFileAccessedDatabase);
  }

  int64_t transaction_id = IDBDatabase::NextTransactionId();

  IDBTransaction::TransactionMojoRemote transaction_remote(context);
  mojo::PendingAssociatedReceiver<mojom::blink::IDBTransaction>
      transaction_receiver =
          transaction_remote.BindNewEndpointAndPassReceiver(GetTaskRunner());

  mojo::PendingAssociatedRemote<mojom::blink::IDBDatabaseCallbacks>
      callbacks_remote;

  auto* request = MakeGarbageCollected<IDBOpenDBRequest>(
      script_state, callbacks_remote.InitWithNewEndpointAndPassReceiver(),
      std::move(transaction_remote), transaction_id, version,
      std::move(metrics), CreatePendingRemoteFeatureObserver());

  auto do_open = WTF::BindOnce(
      &IDBFactory::OpenInternalImpl,
      WrapPersistent(weak_factory_.GetWeakCell()), WrapPersistent(request),
      std::move(callbacks_remote), std::move(transaction_receiver), name,
      version, transaction_id);
  if (allowed_.has_value() && !*allowed_) {
    // When the permission state is cached, `AllowIndexedDB` will invoke its
    // callback synchronously, and thus we'd dispatch the error event
    // synchronously. As per IDB spec, firing the event at the request has to be
    // asynchronous.
    do_open = base::BindPostTask(GetTaskRunner(), std::move(do_open));
  }
  AllowIndexedDB(std::move(do_open));
  return request;
}

void IDBFactory::OpenInternalImpl(
    IDBOpenDBRequest* request,
    mojo::PendingAssociatedRemote<mojom::blink::IDBDatabaseCallbacks>
        callbacks_remote,
    mojo::PendingAssociatedReceiver<mojom::blink::IDBTransaction>
        transaction_receiver,
    const String& name,
    int64_t version,
    int64_t transaction_id) {
  DCHECK(IsContextValid(GetExecutionContext()));

  if (!allowed_.value()) {
    request->OnDBFactoryError(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError, kPermissionDeniedErrorMessage));
    return;
  }

  // Getting the scheduling priority as a one-off is somewhat awkward.
  int scheduling_priority = -1;
  std::unique_ptr<FrameOrWorkerScheduler::LifecycleObserverHandle> lifecycle =
      GetExecutionContext()->GetScheduler()->AddLifecycleObserver(
          FrameOrWorkerScheduler::ObserverType::kWorkerScheduler,
          WTF::BindRepeating(
              [](int* priority,
                 scheduler::SchedulingLifecycleState lifecycle_state) {
                *priority = IDBDatabase::GetSchedulingPriority(lifecycle_state);
              },
              WTF::Unretained(&scheduling_priority)));
  DCHECK_GE(scheduling_priority, 0);
  request->set_connection_priority(scheduling_priority);

  GetRemote()->Open(CreatePendingRemote(request->CreateFactoryClient()),
                    std::move(callbacks_remote), name, version,
                    std::move(transaction_receiver), transaction_id,
                    scheduling_priority);
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
  IDBRequest::AsyncTraceState metrics(
      IDBRequest::TypeForMetrics::kFactoryDeleteDatabase);

  ExecutionContext* context = GetValidContext(script_state);
  if (!context) {
    // TODO(crbug.com/1473972): throw exception?
    return nullptr;
  }
  DCHECK(context->IsContextThread());
  if (!context->GetSecurityOrigin()->CanAccessDatabase()) {
    exception_state.ThrowSecurityError(
        "access to the Indexed Database API is denied in this context.");
    return nullptr;
  }
  if (context->GetSecurityOrigin()->IsLocal()) {
    UseCounter::Count(context, WebFeature::kFileAccessedDatabase);
  }

  auto* request = MakeGarbageCollected<IDBOpenDBRequest>(
      script_state,
      /*callbacks_receiver=*/mojo::NullAssociatedReceiver(),
      IDBTransaction::TransactionMojoRemote(context), 0,
      IDBDatabaseMetadata::kDefaultVersion, std::move(metrics),
      CreatePendingRemoteFeatureObserver());

  auto do_delete = WTF::BindOnce(&IDBFactory::DeleteDatabaseInternalImpl,
                                 WrapPersistent(weak_factory_.GetWeakCell()),
                                 WrapPersistent(request), name, force_close);
  if (allowed_.has_value() && !*allowed_) {
    // When the permission state is cached, `AllowIndexedDB` will invoke its
    // callback synchronously, and thus we'd dispatch the error event
    // synchronously. As per IDB spec, firing the event at the request has to be
    // asynchronous.
    do_delete = base::BindPostTask(GetTaskRunner(), std::move(do_delete));
  }
  AllowIndexedDB(std::move(do_delete));
  return request;
}

void IDBFactory::DeleteDatabaseInternalImpl(
    IDBOpenDBRequest* request,
    const String& name,
    bool force_close) {
  DCHECK(GetExecutionContext());

  if (!allowed_.value()) {
    request->OnDBFactoryError(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError, kPermissionDeniedErrorMessage));
    return;
  }

  GetRemote()->DeleteDatabase(
      CreatePendingRemote(request->CreateFactoryClient()), name, force_close);
}

int16_t IDBFactory::cmp(ScriptState* script_state,
                        const ScriptValue& first_value,
                        const ScriptValue& second_value,
                        ExceptionState& exception_state) {
  const std::unique_ptr<IDBKey> first = CreateIDBKeyFromValue(
      script_state->GetIsolate(), first_value.V8Value(), exception_state);
  if (exception_state.HadException())
    return 0;
  DCHECK(first);
  if (!first->IsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      IDBDatabase::kNotValidKeyErrorMessage);
    return 0;
  }

  const std::unique_ptr<IDBKey> second = CreateIDBKeyFromValue(
      script_state->GetIsolate(), second_value.V8Value(), exception_state);
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

void IDBFactory::AllowIndexedDB(base::OnceCallback<void()> callback) {
  ExecutionContext* context = GetExecutionContext();
  DCHECK(context->IsContextThread());
  SECURITY_DCHECK(context->IsWindow() || context->IsWorkerGlobalScope());

  if (allowed_.has_value()) {
    std::move(callback).Run();
    return;
  }
  callbacks_waiting_on_permission_decision_.push_back(std::move(callback));
  if (callbacks_waiting_on_permission_decision_.size() > 1) {
    return;
  }

  if (auto* window = DynamicTo<LocalDOMWindow>(context)) {
    LocalFrame* frame = window->GetFrame();
    if (!frame) {
      DidAllowIndexedDB(false);
      return;
    }
    frame->AllowStorageAccessAndNotify(
        WebContentSettingsClient::StorageType::kIndexedDB,
        WTF::BindOnce(&IDBFactory::DidAllowIndexedDB,
                      WrapPersistent(weak_factory_.GetWeakCell())));
    return;
  }

  WebContentSettingsClient* settings_client =
      To<WorkerGlobalScope>(context)->ContentSettingsClient();
  if (!settings_client) {
    DidAllowIndexedDB(true);
    return;
  }
  settings_client->AllowStorageAccess(
      WebContentSettingsClient::StorageType::kIndexedDB,
      WTF::BindOnce(&IDBFactory::DidAllowIndexedDB,
                    WrapPersistent(weak_factory_.GetWeakCell())));
}

void IDBFactory::DidAllowIndexedDB(bool allow_access) {
  DCHECK(!allowed_.has_value());
  allowed_ = allow_access;

  for (auto& callback : callbacks_waiting_on_permission_decision_) {
    std::move(callback).Run();
  }
  callbacks_waiting_on_permission_decision_.clear();
}

mojo::PendingAssociatedRemote<mojom::blink::IDBFactoryClient>
IDBFactory::CreatePendingRemote(
    std::unique_ptr<IDBFactoryClient> factory_client) {
  mojo::PendingAssociatedRemote<mojom::blink::IDBFactoryClient>
      pending_factory_client;
  mojo::MakeSelfOwnedAssociatedReceiver(
      std::move(factory_client),
      pending_factory_client.InitWithNewEndpointAndPassReceiver(),
      GetTaskRunner());
  return pending_factory_client;
}

mojo::PendingRemote<mojom::blink::ObservedFeature>
IDBFactory::CreatePendingRemoteFeatureObserver() {
  if (!feature_observer_) {
    mojo::PendingRemote<mojom::blink::FeatureObserver> feature_observer;
    GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
        feature_observer.InitWithNewPipeAndPassReceiver());
    feature_observer_.Bind(std::move(feature_observer), GetTaskRunner());
  }

  mojo::PendingRemote<mojom::blink::ObservedFeature> feature;
  feature_observer_->Register(
      feature.InitWithNewPipeAndPassReceiver(),
      mojom::blink::ObservedFeatureType::kIndexedDBConnection);
  return feature;
}

}  // namespace blink
