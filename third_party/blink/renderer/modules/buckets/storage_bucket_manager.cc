// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/buckets/storage_bucket_manager.h"

#include <cstdint>

#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_storage_bucket_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/navigator_base.h"
#include "third_party/blink/renderer/modules/buckets/storage_bucket.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"

namespace blink {

namespace {

bool IsValidName(const String& name) {
  if (!name.IsLowerASCII()) {
    return false;
  }

  if (!name.ContainsOnlyASCIIOrEmpty()) {
    return false;
  }

  if (name.empty() || name.length() >= 64) {
    return false;
  }

  // | name | must only contain lowercase latin letters, digits 0-9, or special
  // characters '-' & '_' in the middle of the name, but not at the beginning.
  for (wtf_size_t i = 0; i < name.length(); i++) {
    if (!IsASCIIAlphanumeric(name[i]) &&
        (i == 0 || (name[i] != '_' && name[i] != '-'))) {
      return false;
    }
  }
  return true;
}

mojom::blink::BucketPoliciesPtr ToMojoBucketPolicies(
    const StorageBucketOptions* options) {
  auto policies = mojom::blink::BucketPolicies::New();
  if (options->hasPersisted()) {
    policies->persisted = options->persisted();
    policies->has_persisted = true;
  }

  if (options->hasQuota()) {
    DCHECK_LE(options->quota(), uint64_t{std::numeric_limits<int64_t>::max()});
    policies->quota = options->quota();
    policies->has_quota = true;
  }

  if (options->hasDurability()) {
    policies->durability = options->durability() == "strict"
                               ? mojom::blink::BucketDurability::kStrict
                               : mojom::blink::BucketDurability::kRelaxed;
    policies->has_durability = true;
  }

  if (options->hasExpires()) {
    policies->expires =
        base::Time::FromMillisecondsSinceUnixEpoch(options->expires());
  }

  return policies;
}

}  // namespace

const char StorageBucketManager::kSupplementName[] = "StorageBucketManager";

StorageBucketManager::StorageBucketManager(NavigatorBase& navigator)
    : Supplement<NavigatorBase>(navigator),
      ExecutionContextClient(navigator.GetExecutionContext()),
      manager_remote_(navigator.GetExecutionContext()),
      navigator_base_(navigator) {}

StorageBucketManager* StorageBucketManager::storageBuckets(
    NavigatorBase& navigator) {
  auto* supplement =
      Supplement<NavigatorBase>::From<StorageBucketManager>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<StorageBucketManager>(navigator);
    Supplement<NavigatorBase>::ProvideTo(navigator, supplement);
  }
  return supplement;
}

ScriptPromise<StorageBucket> StorageBucketManager::open(
    ScriptState* script_state,
    const String& name,
    const StorageBucketOptions* options,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<StorageBucket>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  ExecutionContext* context = ExecutionContext::From(script_state);

  if (context->IsContextDestroyed()) {
    exception_state.ThrowTypeError("The window/worker has been destroyed.");
    return promise;
  }

  if (!context->GetSecurityOrigin()->CanAccessStorageBuckets()) {
    exception_state.ThrowSecurityError(
        "Access to Storage Buckets API is denied in this context.");
    return promise;
  }

  if (!IsValidName(name)) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(),
        "The bucket name '" + name + "' is not a valid name."));
    return promise;
  }

  if (options->hasQuota() && options->quota() == 0) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(), "The bucket's quota cannot equal zero."));
    return promise;
  }

  mojom::blink::BucketPoliciesPtr bucket_policies =
      ToMojoBucketPolicies(options);
  GetBucketManager(script_state)
      ->OpenBucket(
          name, std::move(bucket_policies),
          WTF::BindOnce(&StorageBucketManager::DidOpen, WrapPersistent(this),
                        WrapPersistent(resolver), name));
  return promise;
}

ScriptPromise<IDLSequence<IDLString>> StorageBucketManager::keys(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLSequence<IDLString>>>(
          script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  ExecutionContext* context = ExecutionContext::From(script_state);
  if (context->IsContextDestroyed()) {
    exception_state.ThrowTypeError("The window/worker has been destroyed.");
    return promise;
  }

  if (!context->GetSecurityOrigin()->CanAccessStorageBuckets()) {
    exception_state.ThrowSecurityError(
        "Access to Storage Buckets API is denied in this context.");
    return promise;
  }

  GetBucketManager(script_state)
      ->Keys(WTF::BindOnce(&StorageBucketManager::DidGetKeys,
                           WrapPersistent(this), WrapPersistent(resolver)));
  return promise;
}

ScriptPromise<IDLUndefined> StorageBucketManager::Delete(
    ScriptState* script_state,
    const String& name,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  ExecutionContext* context = ExecutionContext::From(script_state);
  if (context->IsContextDestroyed()) {
    exception_state.ThrowTypeError("The window/worker has been destroyed.");
    return promise;
  }

  if (!context->GetSecurityOrigin()->CanAccessStorageBuckets()) {
    exception_state.ThrowSecurityError(
        "Access to Storage Buckets API is denied in this context.");
    return promise;
  }

  if (!IsValidName(name)) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(),
        "The bucket name " + name + " is not a valid name."));
    return promise;
  }

  GetBucketManager(script_state)
      ->DeleteBucket(
          name, WTF::BindOnce(&StorageBucketManager::DidDelete,
                              WrapPersistent(this), WrapPersistent(resolver)));
  return promise;
}

mojom::blink::BucketManagerHost* StorageBucketManager::GetBucketManager(
    ScriptState* script_state) {
  if (!manager_remote_.is_bound()) {
    ExecutionContext* context = ExecutionContext::From(script_state);
    mojo::PendingReceiver<mojom::blink::BucketManagerHost> receiver =
        manager_remote_.BindNewPipeAndPassReceiver(
            context->GetTaskRunner(blink::TaskType::kMiscPlatformAPI));
    context->GetBrowserInterfaceBroker().GetInterface(std::move(receiver));
  }
  DCHECK(manager_remote_.is_bound());
  return manager_remote_.get();
}

void StorageBucketManager::DidOpen(
    ScriptPromiseResolver<StorageBucket>* resolver,
    const String& name,
    mojo::PendingRemote<mojom::blink::BucketHost> bucket_remote,
    mojom::blink::BucketError error) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid()) {
    return;
  }
  ScriptState::Scope scope(script_state);

  if (!bucket_remote) {
    switch (error) {
      case mojom::blink::BucketError::kUnknown:
        resolver->Reject(MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kUnknownError,
            "Unknown error occured while creating a bucket."));
        return;
      case mojom::blink::BucketError::kQuotaExceeded:
        resolver->Reject(MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kQuotaExceededError,
            "Too many buckets created."));
        return;
      case mojom::blink::BucketError::kInvalidExpiration:
        resolver->Reject(V8ThrowException::CreateTypeError(
            script_state->GetIsolate(), "The bucket expiration is invalid."));
        return;
    }
  }

  resolver->Resolve(MakeGarbageCollected<StorageBucket>(
      navigator_base_, name, std::move(bucket_remote)));
}

void StorageBucketManager::DidGetKeys(
    ScriptPromiseResolver<IDLSequence<IDLString>>* resolver,
    const Vector<String>& keys,
    bool success) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid()) {
    return;
  }
  ScriptState::Scope scope(script_state);

  if (!success) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError,
        "Unknown error occured while retrieving bucket names."));
    return;
  }
  resolver->Resolve(keys);
}

void StorageBucketManager::DidDelete(
    ScriptPromiseResolver<IDLUndefined>* resolver,
    bool success) {
  if (!success) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError,
        "Unknown error occured while deleting a bucket."));
    return;
  }
  resolver->Resolve();
}

StorageBucket* StorageBucketManager::GetBucketForDevtools(
    ScriptState* script_state,
    const String& name) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  if (!context->GetSecurityOrigin()->CanAccessStorageBuckets()) {
    return nullptr;
  }

  mojo::PendingRemote<mojom::blink::BucketHost> bucket_remote;

  GetBucketManager(script_state)
      ->GetBucketForDevtools(name,
                             bucket_remote.InitWithNewPipeAndPassReceiver());

  return MakeGarbageCollected<StorageBucket>(navigator_base_, name,
                                             std::move(bucket_remote));
}

void StorageBucketManager::Trace(Visitor* visitor) const {
  visitor->Trace(manager_remote_);
  visitor->Trace(navigator_base_);
  ScriptWrappable::Trace(visitor);
  Supplement<NavigatorBase>::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
