// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/buckets/storage_bucket.h"

#include "base/time/time.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable_creation_key.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_storage_bucket_durability.h"
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
#include "third_party/blink/renderer/platform/heap/persistent.h"

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

ScriptPromiseTyped<IDLBoolean> StorageBucket::persist(
    ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolverTyped<IDLBoolean>>(
      script_state);
  auto promise = resolver->Promise();

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

ScriptPromiseTyped<IDLBoolean> StorageBucket::persisted(
    ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolverTyped<IDLBoolean>>(
      script_state);
  auto promise = resolver->Promise();

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

ScriptPromiseTyped<StorageEstimate> StorageBucket::estimate(
    ScriptState* script_state) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolverTyped<StorageEstimate>>(
          script_state);
  auto promise = resolver->Promise();

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

ScriptPromiseTyped<V8StorageBucketDurability> StorageBucket::durability(
    ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolverTyped<V8StorageBucketDurability>>(script_state);
  auto promise = resolver->Promise();

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

ScriptPromiseTyped<IDLUndefined> StorageBucket::setExpires(
    ScriptState* script_state,
    const DOMHighResTimeStamp& expires) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolverTyped<IDLUndefined>>(
          script_state);
  auto promise = resolver->Promise();

  // The context may be destroyed and the mojo connection unbound. However the
  // object may live on, reject any requests after the context is destroyed.
  if (!remote_.is_bound()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError));
    return promise;
  }

  remote_->SetExpires(
      base::Time::FromMillisecondsSinceUnixEpoch(expires),
      WTF::BindOnce(&StorageBucket::DidSetExpires, WrapPersistent(this),
                    WrapPersistent(resolver)));
  return promise;
}

ScriptPromiseTyped<IDLNullable<IDLDOMHighResTimeStamp>> StorageBucket::expires(
    ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolverTyped<IDLNullable<IDLDOMHighResTimeStamp>>>(
      script_state);
  auto promise = resolver->Promise();

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
    mojo::PendingRemote<mojom::blink::IDBFactory> remote_factory;
    remote_->GetIdbFactory(remote_factory.InitWithNewPipeAndPassReceiver());
    idb_factory_->SetRemote(std::move(remote_factory));
  }
  return idb_factory_.Get();
}

LockManager* StorageBucket::locks() {
  if (!lock_manager_) {
    mojo::PendingRemote<mojom::blink::LockManager> lock_manager;
    remote_->GetLockManager(lock_manager.InitWithNewPipeAndPassReceiver());
    lock_manager_ = MakeGarbageCollected<LockManager>(*navigator_base_);
    lock_manager_->SetManager(std::move(lock_manager), GetExecutionContext());
  }
  return lock_manager_.Get();
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

  return caches_.Get();
}

ScriptPromiseTyped<FileSystemDirectoryHandle> StorageBucket::getDirectory(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  return StorageManagerFileSystemAccess::CheckGetDirectoryIsAllowed(
      script_state, exception_state,
      WTF::BindOnce(&StorageBucket::GetSandboxedFileSystem,
                    WrapWeakPersistent(this)));
}

void StorageBucket::GetDirectoryForDevTools(
    ExecutionContext* context,
    base::OnceCallback<void(mojom::blink::FileSystemAccessErrorPtr,
                            FileSystemDirectoryHandle*)> callback) {
  StorageManagerFileSystemAccess::CheckGetDirectoryIsAllowed(
      context, WTF::BindOnce(&StorageBucket::GetSandboxedFileSystemForDevtools,
                             WrapWeakPersistent(this),
                             WrapWeakPersistent(context), std::move(callback)));
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

void StorageBucket::DidRequestPersist(
    ScriptPromiseResolverTyped<IDLBoolean>* resolver,
    bool persisted,
    bool success) {
  if (!success) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError,
        "Unknown error occurred while requesting persist."));
    return;
  }

  resolver->Resolve(persisted);
}

void StorageBucket::DidGetPersisted(
    ScriptPromiseResolverTyped<IDLBoolean>* resolver,
    bool persisted,
    bool success) {
  if (!success) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError,
        "Unknown error occurred while getting persisted."));
    return;
  }

  resolver->Resolve(persisted);
}

void StorageBucket::DidGetEstimate(
    ScriptPromiseResolverTyped<StorageEstimate>* resolver,
    int64_t current_usage,
    int64_t current_quota,
    bool success) {
  if (!success) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError,
        "Unknown error occurred while getting estimate."));
    return;
  }

  StorageEstimate* estimate = StorageEstimate::Create();
  estimate->setUsage(current_usage);
  estimate->setQuota(current_quota);
  StorageUsageDetails* details = StorageUsageDetails::Create();
  estimate->setUsageDetails(details);
  resolver->Resolve(estimate);
}

void StorageBucket::DidGetDurability(
    ScriptPromiseResolverTyped<V8StorageBucketDurability>* resolver,
    mojom::blink::BucketDurability durability,
    bool success) {
  if (!success) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError,
        "Unknown error occurred while getting durability."));
    return;
  }

  if (durability == mojom::blink::BucketDurability::kRelaxed) {
    resolver->Resolve(
        V8StorageBucketDurability(V8StorageBucketDurability::Enum::kRelaxed));
  } else {
    resolver->Resolve(
        V8StorageBucketDurability(V8StorageBucketDurability::Enum::kStrict));
  }
}

void StorageBucket::DidSetExpires(
    ScriptPromiseResolverTyped<IDLUndefined>* resolver,
    bool success) {
  if (success) {
    resolver->Resolve();
  } else {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError,
        "Unknown error occurred while setting expires."));
  }
}

void StorageBucket::DidGetExpires(
    ScriptPromiseResolverTyped<IDLNullable<IDLDOMHighResTimeStamp>>* resolver,
    const std::optional<base::Time> expires,
    bool success) {
  if (!success) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError,
        "Unknown error occurred while getting expires."));
  } else {
    resolver->Resolve(expires);
  }
}

void StorageBucket::GetSandboxedFileSystem(
    ScriptPromiseResolverTyped<FileSystemDirectoryHandle>* resolver) {
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

void StorageBucket::GetSandboxedFileSystemForDevtools(
    ExecutionContext* context,
    base::OnceCallback<void(mojom::blink::FileSystemAccessErrorPtr,
                            FileSystemDirectoryHandle*)> callback,
    mojom::blink::FileSystemAccessErrorPtr result) {
  if (result->status != mojom::blink::FileSystemAccessStatus::kOk) {
    std::move(callback).Run(std::move(result), nullptr);
    return;
  }

  if (!remote_.is_bound()) {
    std::move(callback).Run(
        mojom::blink::FileSystemAccessError::New(
            mojom::blink::FileSystemAccessStatus::kInvalidState,
            base::File::Error::FILE_ERROR_FAILED, "Invalid state Error."), nullptr);
    return;
  }

  remote_->GetDirectory(WTF::BindOnce(
      &StorageManagerFileSystemAccess::DidGetSandboxedFileSystemForDevtools,
      WrapWeakPersistent(context), std::move(callback)));
}
}  // namespace blink
