// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/buckets/storage_bucket.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_storage_estimate.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_storage_usage_details.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/dom_time_stamp.h"

namespace blink {

StorageBucket::StorageBucket(
    ExecutionContext* context,
    mojo::PendingRemote<mojom::blink::BucketHost> remote)
    : ExecutionContextLifecycleObserver(context) {
  remote_.Bind(std::move(remote), GetExecutionContext()->GetTaskRunner(
                                      TaskType::kInternalDefault));
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

  remote_->Persist(WTF::Bind(&StorageBucket::DidRequestPersist,
                             WrapPersistent(this), WrapPersistent(resolver)));
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

  remote_->Persisted(WTF::Bind(&StorageBucket::DidGetPersisted,
                               WrapPersistent(this), WrapPersistent(resolver)));
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

  remote_->Estimate(WTF::Bind(&StorageBucket::DidGetEstimate,
                              WrapPersistent(this), WrapPersistent(resolver)));
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

  remote_->Durability(WTF::Bind(&StorageBucket::DidGetDurability,
                                WrapPersistent(this),
                                WrapPersistent(resolver)));
  return promise;
}

ScriptPromise StorageBucket::setExpires(ScriptState* script_state,
                                        const DOMTimeStamp& expires) {
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
      base::Time::FromJavaTime(expires),
      WTF::Bind(&StorageBucket::DidSetExpires, WrapPersistent(this),
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

  remote_->Expires(WTF::Bind(&StorageBucket::DidGetExpires,
                             WrapPersistent(this), WrapPersistent(resolver)));
  return promise;
}

bool StorageBucket::HasPendingActivity() const {
  return GetExecutionContext();
}

void StorageBucket::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
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

  // TODO(ayui): Pass correct values once connected to quota.
  StorageEstimate* estimate = StorageEstimate::Create();
  estimate->setUsage(0);
  estimate->setQuota(0);
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
    resolver->Resolve(
        ConvertSecondsToDOMTimeStamp(expires.value().ToDoubleT()));
  } else {
    resolver->Resolve(v8::Null(script_state->GetIsolate()));
  }
}

void StorageBucket::ContextDestroyed() {
  remote_.reset();
}

}  // namespace blink
