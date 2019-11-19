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
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-blink.h"
#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_binding_for_modules.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/probe/async_task_id.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/modules/indexed_db_names.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_database.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_database_callbacks.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_database_info.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_name_and_version.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_tracing.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_callbacks.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_database_callbacks.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_factory.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_factory_impl.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_transaction_impl.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

namespace {

class WebIDBGetDBNamesCallbacksImpl : public WebIDBCallbacks {
 public:
  WebIDBGetDBNamesCallbacksImpl(ScriptPromiseResolver* promise_resolver)
      : promise_resolver_(promise_resolver) {
    probe::AsyncTaskScheduled(
        ExecutionContext::From(promise_resolver_->GetScriptState()),
        indexed_db_names::kIndexedDB, &async_task_id_);
  }

  ~WebIDBGetDBNamesCallbacksImpl() override {
    if (promise_resolver_) {
      probe::AsyncTaskCanceled(
          ExecutionContext::From(promise_resolver_->GetScriptState()),
          &async_task_id_);
      promise_resolver_->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kUnknownError,
          "An unexpected shutdown occured before the "
          "databases() promise could be resolved"));
    }
  }

  void SetState(base::WeakPtr<WebIDBCursorImpl> cursor,
                int64_t transaction_id) override {}

  void Error(mojom::blink::IDBException code, const String& message) override {
    if (!promise_resolver_)
      return;

    probe::AsyncTask async_task(
        ExecutionContext::From(promise_resolver_->GetScriptState()),
        &async_task_id_, "error");
    promise_resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError,
        "The databases() promise was rejected."));
    promise_resolver_.Clear();
  }

  void SuccessNamesAndVersionsList(
      Vector<mojom::blink::IDBNameAndVersionPtr> names_and_versions) override {
    if (!promise_resolver_)
      return;

    HeapVector<Member<IDBDatabaseInfo>> name_and_version_list;
    name_and_version_list.ReserveInitialCapacity(name_and_version_list.size());
    for (const mojom::blink::IDBNameAndVersionPtr& name_version :
         names_and_versions) {
      const IDBNameAndVersion idb_name_and_version(name_version->name,
                                                   name_version->version);
      IDBDatabaseInfo* idb_info = IDBDatabaseInfo::Create();
      idb_info->setName(name_version->name);
      idb_info->setVersion(name_version->version);
      name_and_version_list.push_back(idb_info);
    }

    probe::AsyncTask async_task(
        ExecutionContext::From(promise_resolver_->GetScriptState()),
        &async_task_id_, "success");
    promise_resolver_->Resolve(name_and_version_list);
    promise_resolver_.Clear();
  }

  void SuccessStringList(const Vector<String>&) override { NOTREACHED(); }

  void SuccessCursor(
      mojo::PendingAssociatedRemote<mojom::blink::IDBCursor> cursor_info,
      std::unique_ptr<IDBKey> key,
      std::unique_ptr<IDBKey> primary_key,
      base::Optional<std::unique_ptr<IDBValue>> optional_value) override {
    NOTREACHED();
  }

  void SuccessCursorPrefetch(
      Vector<std::unique_ptr<IDBKey>> keys,
      Vector<std::unique_ptr<IDBKey>> primary_keys,
      Vector<std::unique_ptr<IDBValue>> values) override {
    NOTREACHED();
  }

  void SuccessDatabase(
      mojo::PendingAssociatedRemote<mojom::blink::IDBDatabase> pending_backend,
      const IDBDatabaseMetadata& metadata) override {
    NOTREACHED();
  }

  void SuccessKey(std::unique_ptr<IDBKey> key) override { NOTREACHED(); }

  void SuccessValue(mojom::blink::IDBReturnValuePtr return_value) override {
    NOTREACHED();
  }

  void SuccessArray(Vector<mojom::blink::IDBReturnValuePtr> values) override {
    NOTREACHED();
  }

  void SuccessInteger(int64_t value) override { NOTREACHED(); }

  void Success() override { NOTREACHED(); }

  void SuccessCursorContinue(
      std::unique_ptr<IDBKey> key,
      std::unique_ptr<IDBKey> primary_key,
      base::Optional<std::unique_ptr<IDBValue>> value) override {
    NOTREACHED();
  }

  void Blocked(int64_t old_version) override { NOTREACHED(); }

  void UpgradeNeeded(
      mojo::PendingAssociatedRemote<mojom::blink::IDBDatabase> pending_database,
      int64_t old_version,
      mojom::IDBDataLoss data_loss,
      const String& data_loss_message,
      const IDBDatabaseMetadata& metadata) override {
    NOTREACHED();
  }

  void DetachRequestFromCallback() override { NOTREACHED(); }

 private:
  probe::AsyncTaskId async_task_id_;
  Persistent<ScriptPromiseResolver> promise_resolver_;
};

}  // namespace

static const char kPermissionDeniedErrorMessage[] =
    "The user denied permission to access the database.";

IDBFactory::IDBFactory() = default;

IDBFactory::IDBFactory(std::unique_ptr<WebIDBFactory> web_idb_factory)
    : web_idb_factory_(std::move(web_idb_factory)) {}

static bool IsContextValid(ExecutionContext* context) {
  DCHECK(IsA<Document>(context) || context->IsWorkerGlobalScope());
  if (auto* document = DynamicTo<Document>(context))
    return document->GetFrame() && document->GetPage();
  return true;
}

WebIDBFactory* IDBFactory::GetFactory(ExecutionContext* execution_context) {
  if (!web_idb_factory_) {
    mojo::PendingRemote<mojom::blink::IDBFactory> web_idb_factory_host_remote;
    execution_context->GetBrowserInterfaceBroker().GetInterface(
        web_idb_factory_host_remote.InitWithNewPipeAndPassReceiver());
    web_idb_factory_ = std::make_unique<WebIDBFactoryImpl>(
        std::move(web_idb_factory_host_remote),
        execution_context->GetTaskRunner(TaskType::kDatabaseAccess));
  }
  return web_idb_factory_.get();
}

ScriptPromise IDBFactory::GetDatabaseInfo(ScriptState* script_state,
                                          ExceptionState& exception_state) {
  // The BlinkIDL definition for GetDatabaseInfo() already has a [Measure]
  // attribute, so the kIndexedDBRead use counter must be explicitly updated.
  UseCounter::Count(ExecutionContext::From(script_state),
                    WebFeature::kIndexedDBRead);

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);

  if (!IsContextValid(ExecutionContext::From(script_state))) {
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

  if (!CachedAllowIndexedDB(script_state)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kUnknownError,
                                      kPermissionDeniedErrorMessage);
    resolver->Reject();
    return resolver->Promise();
  }

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  WebIDBFactory* factory = GetFactory(execution_context);
  if (!factory) {
    exception_state.ThrowSecurityError("An internal error occurred.");
    resolver->Reject();
    return resolver->Promise();
  }
  factory->GetDatabaseInfo(
      std::make_unique<WebIDBGetDBNamesCallbacksImpl>(resolver));
  ScriptPromise promise = resolver->Promise();
  return promise;
}

IDBRequest* IDBFactory::GetDatabaseNames(ScriptState* script_state,
                                         ExceptionState& exception_state) {
  IDB_TRACE("IDBFactory::getDatabaseNamesRequestSetup");
  IDBRequest::AsyncTraceState metrics("IDBFactory::getDatabaseNames");
  IDBRequest* request = IDBRequest::Create(script_state, IDBRequest::Source(),
                                           nullptr, std::move(metrics));
  // TODO(jsbell): Used only by inspector; remove unneeded checks/exceptions?
  if (!IsContextValid(ExecutionContext::From(script_state)))
    return nullptr;
  if (!ExecutionContext::From(script_state)
           ->GetSecurityOrigin()
           ->CanAccessDatabase()) {
    exception_state.ThrowSecurityError(
        "access to the Indexed Database API is denied in this context.");
    return nullptr;
  }

  if (ExecutionContext::From(script_state)->GetSecurityOrigin()->IsLocal()) {
    UseCounter::Count(ExecutionContext::From(script_state),
                      WebFeature::kFileAccessedDatabase);
  }

  if (!CachedAllowIndexedDB(script_state)) {
    request->HandleResponse(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError, kPermissionDeniedErrorMessage));
    return request;
  }

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  WebIDBFactory* factory = GetFactory(execution_context);
  if (!factory) {
    exception_state.ThrowSecurityError("An internal error occurred.");
    return nullptr;
  }
  factory->GetDatabaseNames(request->CreateWebCallbacks());
  return request;
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
  IDB_TRACE1("IDBFactory::open", "name", name.Utf8());
  IDBRequest::AsyncTraceState metrics("IDBFactory::open");
  DCHECK(version >= 1 || version == IDBDatabaseMetadata::kNoVersion);
  if (!IsContextValid(ExecutionContext::From(script_state)))
    return nullptr;
  if (!ExecutionContext::From(script_state)
           ->GetSecurityOrigin()
           ->CanAccessDatabase()) {
    exception_state.ThrowSecurityError(
        "access to the Indexed Database API is denied in this context.");
    return nullptr;
  }

  if (ExecutionContext::From(script_state)->GetSecurityOrigin()->IsLocal()) {
    UseCounter::Count(ExecutionContext::From(script_state),
                      WebFeature::kFileAccessedDatabase);
  }

  auto* database_callbacks = MakeGarbageCollected<IDBDatabaseCallbacks>();
  int64_t transaction_id = IDBDatabase::NextTransactionId();

  auto transaction_backend = std::make_unique<WebIDBTransactionImpl>(
      ExecutionContext::From(script_state)
          ->GetTaskRunner(TaskType::kDatabaseAccess),
      transaction_id);
  mojo::PendingAssociatedReceiver<mojom::blink::IDBTransaction>
      transaction_receiver = transaction_backend->CreateReceiver();
  auto* request = MakeGarbageCollected<IDBOpenDBRequest>(
      script_state, database_callbacks, std::move(transaction_backend),
      transaction_id, version, std::move(metrics));

  if (!CachedAllowIndexedDB(script_state)) {
    request->HandleResponse(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError, kPermissionDeniedErrorMessage));
    return request;
  }

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  WebIDBFactory* factory = GetFactory(execution_context);
  if (!factory) {
    exception_state.ThrowSecurityError("An internal error occurred.");
    return nullptr;
  }
  factory->Open(name, version, std::move(transaction_receiver), transaction_id,
                request->CreateWebCallbacks(),
                database_callbacks->CreateWebCallbacks());
  return request;
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
  IDB_TRACE1("IDBFactory::deleteDatabase", "name", name.Utf8());
  IDBRequest::AsyncTraceState metrics("IDBFactory::deleteDatabase");
  if (!IsContextValid(ExecutionContext::From(script_state)))
    return nullptr;
  if (!ExecutionContext::From(script_state)
           ->GetSecurityOrigin()
           ->CanAccessDatabase()) {
    exception_state.ThrowSecurityError(
        "access to the Indexed Database API is denied in this context.");
    return nullptr;
  }

  if (ExecutionContext::From(script_state)->GetSecurityOrigin()->IsLocal()) {
    UseCounter::Count(ExecutionContext::From(script_state),
                      WebFeature::kFileAccessedDatabase);
  }

  auto* request = MakeGarbageCollected<IDBOpenDBRequest>(
      script_state, nullptr, /*IDBTransactionAssociatedPtr=*/nullptr, 0,
      IDBDatabaseMetadata::kDefaultVersion, std::move(metrics));

  if (!CachedAllowIndexedDB(script_state)) {
    request->HandleResponse(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError, kPermissionDeniedErrorMessage));
    return request;
  }

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  WebIDBFactory* factory = GetFactory(execution_context);
  if (!factory) {
    exception_state.ThrowSecurityError("An internal error occurred.");
    return nullptr;
  }
  factory->DeleteDatabase(name, request->CreateWebCallbacks(), force_close);
  return request;
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

bool IDBFactory::AllowIndexedDB(ScriptState* script_state) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  DCHECK(execution_context->IsContextThread());
  SECURITY_DCHECK(execution_context->IsDocument() ||
                  execution_context->IsWorkerGlobalScope());
  if (auto* document = DynamicTo<Document>(execution_context)) {
    LocalFrame* frame = document->GetFrame();
    if (!frame)
      return false;
    if (auto* settings_client = frame->GetContentSettingsClient()) {
      // This triggers a sync IPC.
      return settings_client->AllowIndexedDB();
    }
    return true;
  }

  WebContentSettingsClient* content_settings_client =
      To<WorkerGlobalScope>(execution_context)->ContentSettingsClient();
  if (!content_settings_client)
    return true;
  // This triggers a sync IPC.
  return content_settings_client->AllowIndexedDB();
}

bool IDBFactory::CachedAllowIndexedDB(ScriptState* script_state) {
  if (!cached_allowed_.has_value()) {
    // Cache the AllowIndexedDB() call because it triggers a sync IPC.
    cached_allowed_.emplace(AllowIndexedDB(script_state));
  }
  return cached_allowed_.value();
}

}  // namespace blink
