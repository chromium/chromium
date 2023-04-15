// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/buckets/storage_bucket.h"

#include "base/time/time.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable_creation_key.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_storage_estimate.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_storage_usage_details.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/core/fetch/global_fetch.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/cache_storage/cache_storage.h"
#include "third_party/blink/renderer/modules/cache_storage/global_cache_storage.h"
#include "third_party/blink/renderer/modules/file_system_access/storage_manager_file_system_access.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_factory.h"
#include "third_party/blink/renderer/modules/locks/lock_manager.h"

namespace blink {

StorageBucket::StorageBucket(
    NavigatorBase* navigator,
    const String& name,
    mojo::PendingRemote<mojom::blink::BucketHost> remote)
    : ExecutionContextClient(navigator->GetExecutionContext()),
      name_(name),
      remote_(GetExecutionContext()),
      navigator_base_(navigator) {
  remote_.Bind(std::move(remote), GetExecutionContext()->GetTaskRunner(
                                      TaskType::kInternalDefault));
}

const String& StorageBucket::name() {
  return name_;
}

ScriptPromise StorageBucket::persist(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  // The context may be destroyed and the mojo connection unbound. However the
  // object may live on, reject any requests after the context is destroyed.
  if (!remote_.is_bound()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError));
    return promise;
  }

  remote_->Persist(WTF::BindOnce(&StorageBucket::DidRequestPersist,
                                 WrapPersistent(this),
                                 WrapPersistent(resolver)));
  return promise;
}

ScriptPromise StorageBucket::persisted(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  // The context may be destroyed and the mojo connection unbound. However the
  // object may live on, reject any requests after the context is destroyed.
  if (!remote_.is_bound()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError));
    return promise;
  }

  remote_->Persisted(WTF::BindOnce(&StorageBucket::DidGetPersisted,
                                   WrapPersistent(this),
                                   WrapPersistent(resolver)));
  return promise;
}

ScriptPromise StorageBucket::estimate(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  // The context may be destroyed and the mojo connection unbound. However the
  // object may live on, reject any requests after the context is destroyed.
  if (!remote_.is_bound()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError));
    return promise;
  }

  remote_->Estimate(WTF::BindOnce(&StorageBucket::DidGetEstimate,
                                  WrapPersistent(this),
                                  WrapPersistent(resolver)));
  return promise;
}

ScriptPromise StorageBucket::durability(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  // The context may be destroyed and the mojo connection unbound. However the
  // object may live on, reject any requests after the context is destroyed.
  if (!remote_.is_bound()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError));
    return promise;
  }

  remote_->Durability(WTF::BindOnce(&StorageBucket::DidGetDurability,
                                    WrapPersistent(this),
                                    WrapPersistent(resolver)));
  return promise;
}

ScriptPromise StorageBucket::setExpires(ScriptState* script_state,
                                        const DOMHighResTimeStamp& expires) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  // The context may be destroyed and the mojo connection unbound. However the
  // object may live on, reject any requests after the context is destroyed.
  if (!remote_.is_bound()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError));
    return promise;
  }

  remote_->SetExpires(
      base::Time::FromJsTime(expires),
      WTF::BindOnce(&StorageBucket::DidSetExpires, WrapPersistent(this),
                    WrapPersistent(resolver)));
  return promise;
}

ScriptPromise StorageBucket::expires(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  // The context may be destroyed and the mojo connection unbound. However the
  // object may live on, reject any requests after the context is destroyed.
  if (!remote_.is_bound()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError));
    return promise;
  }

  remote_->Expires(WTF::BindOnce(&StorageBucket::DidGetExpires,
                                 WrapPersistent(this),
                                 WrapPersistent(resolver)));
  return promise;
}

IDBFactory* StorageBucket::indexedDB() {
  if (!idb_factory_) {
    idb_factory_ = MakeGarbageCollected<IDBFactory>(GetExecutionContext());
    mojo::PendingRemote<mojom::blink::IDBFactory> factory;
    remote_->GetIdbFactory(factory.InitWithNewPipeAndPassReceiver());
    idb_factory_->SetFactory(std::move(factory), GetExecutionContext());
  }
  return idb_factory_;
}

LockManager* StorageBucket::locks() {
  if (!lock_manager_) {
    mojo::PendingRemote<mojom::blink::LockManager> lock_manager;
    remote_->GetLockManager(lock_manager.InitWithNewPipeAndPassReceiver());
    lock_manager_ = MakeGarbageCollected<LockManager>(*navigator_base_);
    lock_manager_->SetManager(std::move(lock_manager), GetExecutionContext());
  }
  return lock_manager_;
}

CacheStorage* StorageBucket::caches(ExceptionState& exception_state) {
  if (!caches_ && GlobalCacheStorage::CanCreateCacheStorage(
                      GetExecutionContext(), exception_state)) {
    mojo::PendingRemote<mojom::blink::CacheStorage> cache_storage;
    remote_->GetCaches(cache_storage.InitWithNewPipeAndPassReceiver());
    caches_ = MakeGarbageCollected<CacheStorage>(
        GetExecutionContext(),
        GlobalFetch::ScopedFetcher::From(*navigator_base_),
        std::move(cache_storage));
  }

  return caches_;
}

ScriptPromise StorageBucket::getDirectory(ScriptState* script_state,
                                          ExceptionState& exception_state) {
  return StorageManagerFileSystemAccess::CheckGetDirectoryIsAllowed(
      script_state, exception_state,
      WTF::BindOnce(&StorageBucket::GetSandboxedFileSystem,
                    weak_factory_.GetWeakPtr()));
}

void StorageBucket::Trace(Visitor* visitor) const {
  visitor->Trace(remote_);
  visitor->Trace(idb_factory_);
  visitor->Trace(lock_manager_);
  visitor->Trace(navigator_base_);
  visitor->Trace(caches_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

void StorageBucket::DidRequestPersist(ScriptPromiseResolver* resolver,
                                      bool persisted,
                                      bool success) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;

  if (!success) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError,
        "Unknown error occured while requesting persist."));
    return;
  }

  ScriptState::Scope scope(script_state);
  resolver->Resolve(persisted);
}

void StorageBucket::DidGetPersisted(ScriptPromiseResolver* resolver,
                                    bool persisted,
                                    bool success) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;

  if (!success) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError,
        "Unknown error occured while getting persisted."));
    return;
  }

  ScriptState::Scope scope(script_state);
  resolver->Resolve(persisted);
}

void StorageBucket::DidGetEstimate(ScriptPromiseResolver* resolver,
                                   int64_t current_usage,
                                   int64_t current_quota,
                                   bool success) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;
  ScriptState::Scope scope(script_state);

  if (!success) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError,
        "Unknown error occured while getting estimate."));
    return;
  }

  StorageEstimate* estimate = StorageEstimate::Create();
  estimate->setUsage(current_usage);
  estimate->setQuota(current_quota);
  StorageUsageDetails* details = StorageUsageDetails::Create();
  estimate->setUsageDetails(details);
  resolver->Resolve(estimate);
}

void StorageBucket::DidGetDurability(ScriptPromiseResolver* resolver,
                                     mojom::blink::BucketDurability durability,
                                     bool success) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;

  if (!success) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError,
        "Unknown error occured while getting durability."));
    return;
  }

  ScriptState::Scope scope(script_state);

  if (durability == mojom::blink::BucketDurability::kRelaxed)
    resolver->Resolve("relaxed");
  resolver->Resolve("strict");
}

void StorageBucket::DidSetExpires(ScriptPromiseResolver* resolver,
                                  bool success) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;
  ScriptState::Scope scope(script_state);

  if (success) {
    resolver->Resolve();
  } else {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError,
        "Unknown error occured while setting expires."));
  }
}

void StorageBucket::DidGetExpires(ScriptPromiseResolver* resolver,
                                  const absl::optional<base::Time> expires,
                                  bool success) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;
  ScriptState::Scope scope(script_state);

  if (!success) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError,
        "Unknown error occured while getting expires."));
  } else if (expires.has_value()) {
    resolver->Resolve(base::Time::kMillisecondsPerSecond *
                      expires.value().ToDoubleT());
  } else {
    resolver->Resolve(v8::Null(script_state->GetIsolate()));
  }
}

void StorageBucket::GetSandboxedFileSystem(ScriptPromiseResolver* resolver) {
  // The context may be destroyed and the mojo connection unbound. However the
  // object may live on, reject any requests after the context is destroyed.
  if (!remote_.is_bound()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError));
    return;
  }

  remote_->GetDirectory(
      WTF::BindOnce(&StorageManagerFileSystemAccess::DidGetSandboxedFileSystem,
                    WrapPersistent(resolver)));
}
}  // namespace blink
