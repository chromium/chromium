// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/buckets/bucket_manager.h"

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_storage_bucket_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/navigator_base.h"
#include "third_party/blink/renderer/modules/buckets/storage_bucket.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

mojom::blink::BucketPoliciesPtr ToMojoBucketPolicies(
    const StorageBucketOptions* options) {
  auto policies = mojom::blink::BucketPolicies::New();
  policies->persisted = options->persisted();
  policies->title = options->hasTitleNonNull() ? options->titleNonNull() : "";
  policies->quota = options->hasQuotaNonNull()
                        ? options->quotaNonNull()
                        : mojom::blink::kNoQuotaPolicyValue;

  if (options->durability() == "strict") {
    policies->durability = mojom::blink::BucketDurability::kStrict;
  } else {
    policies->durability = mojom::blink::BucketDurability::kRelaxed;
  }

  if (options->hasExpiresNonNull())
    policies->expires = base::Time::FromJavaTime(options->expiresNonNull());
  return policies;
}

}  // namespace

const char BucketManager::kSupplementName[] = "BucketManager";

BucketManager::BucketManager(NavigatorBase& navigator,
                             ExecutionContext* context)
    : Supplement<NavigatorBase>(navigator),
      ExecutionContextClient(context),
      manager_remote_(context) {
  context->GetBrowserInterfaceBroker().GetInterface(
      manager_remote_.BindNewPipeAndPassReceiver(
          context->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  DCHECK(manager_remote_.is_bound());
}

BucketManager* BucketManager::storageBuckets(ScriptState* script_state,
                                             NavigatorBase& navigator,
                                             ExceptionState& exception_state) {
  auto* supplement = Supplement<NavigatorBase>::From<BucketManager>(navigator);
  if (!supplement) {
    auto* context = ExecutionContext::From(script_state);
    supplement = MakeGarbageCollected<BucketManager>(navigator, context);
    Supplement<NavigatorBase>::ProvideTo(navigator, supplement);
  }
  return supplement;
}

ScriptPromise BucketManager::open(ScriptState* script_state,
                                  const String& name,
                                  const StorageBucketOptions* options,
                                  ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  mojom::blink::BucketPoliciesPtr bucket_policies =
      ToMojoBucketPolicies(options);

  manager_remote_->OpenBucket(
      name, std::move(bucket_policies),
      WTF::Bind(&BucketManager::DidOpen, WrapPersistent(this),
                WrapPersistent(resolver)));
  return promise;
}

ScriptPromise BucketManager::keys(ScriptState* script_state,
                                  ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  manager_remote_->Keys(WTF::Bind(&BucketManager::DidGetKeys,
                                  WrapPersistent(this),
                                  WrapPersistent(resolver)));
  return promise;
}

ScriptPromise BucketManager::Delete(ScriptState* script_state,
                                    const String& name,
                                    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  manager_remote_->DeleteBucket(
      name, WTF::Bind(&BucketManager::DidDelete, WrapPersistent(this),
                      WrapPersistent(resolver)));
  return promise;
}

void BucketManager::DidOpen(
    ScriptPromiseResolver* resolver,
    mojo::PendingRemote<mojom::blink::BucketHost> bucket_remote) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;
  ScriptState::Scope scope(script_state);

  if (!bucket_remote) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError,
        "Unknown error occured while creating a bucket."));
    return;
  }
  resolver->Resolve(MakeGarbageCollected<StorageBucket>(
      GetExecutionContext(), std::move(bucket_remote)));
}

void BucketManager::DidGetKeys(ScriptPromiseResolver* resolver,
                               const Vector<String>& keys,
                               bool success) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;
  ScriptState::Scope scope(script_state);

  if (!success) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError,
        "Unknown error occured while retrieving bucket names."));
    return;
  }
  resolver->Resolve(keys);
}

void BucketManager::DidDelete(ScriptPromiseResolver* resolver, bool success) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;
  ScriptState::Scope scope(script_state);

  if (!success) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError,
        "Unknown error occured while deleting a bucket."));
    return;
  }
  resolver->Resolve();
}

void BucketManager::Trace(Visitor* visitor) const {
  visitor->Trace(manager_remote_);
  ScriptWrappable::Trace(visitor);
  Supplement<NavigatorBase>::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
