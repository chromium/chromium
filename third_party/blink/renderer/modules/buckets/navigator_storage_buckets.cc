// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "third_party/blink/renderer/modules/buckets/navigator_storage_buckets.h"

#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/workers/worker_navigator.h"
#include "third_party/blink/renderer/modules/buckets/bucket_manager.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

namespace {

template <typename T>
class NavigatorStorageBucketsImpl final
    : public GarbageCollected<NavigatorStorageBucketsImpl<T>>,
      public Supplement<T>,
      public NameClient {
 public:
  static const char kSupplementName[];

  static NavigatorStorageBucketsImpl& From(T& navigator) {
    NavigatorStorageBucketsImpl* supplement =
        static_cast<NavigatorStorageBucketsImpl*>(
            Supplement<T>::template From<NavigatorStorageBucketsImpl>(
                navigator));
    if (!supplement) {
      supplement = MakeGarbageCollected<NavigatorStorageBucketsImpl>(navigator);
      Supplement<T>::ProvideTo(navigator, supplement);
    }
    return *supplement;
  }

  explicit NavigatorStorageBucketsImpl(T& navigator)
      : Supplement<T>(navigator) {}

  BucketManager* GetBucketManager() const {
    if (!buckets_) {
      buckets_ = MakeGarbageCollected<BucketManager>();
    }
    return buckets_.Get();
  }

  void Trace(blink::Visitor* visitor) const override {
    visitor->Trace(buckets_);
    Supplement<T>::Trace(visitor);
  }

  const char* NameInHeapSnapshot() const override {
    return "NavigatorStorageBucketsImpl";
  }

 private:
  mutable Member<BucketManager> buckets_;
};

// static
template <typename T>
const char NavigatorStorageBucketsImpl<T>::kSupplementName[] =
    "NavigatorStorageBucketsImpl";

}  // namespace

BucketManager* NavigatorStorageBuckets::storageBuckets(
    ScriptState* script_state,
    Navigator& navigator,
    ExceptionState& exception_state) {
  DCHECK(ExecutionContext::From(script_state)->IsContextThread());
  return NavigatorStorageBucketsImpl<Navigator>::From(navigator)
      .GetBucketManager();
}

BucketManager* NavigatorStorageBuckets::storageBuckets(
    ScriptState* script_state,
    WorkerNavigator& navigator,
    ExceptionState& exception_state) {
  DCHECK(ExecutionContext::From(script_state)->IsContextThread());
  return NavigatorStorageBucketsImpl<WorkerNavigator>::From(navigator)
      .GetBucketManager();
}

}  // namespace blink
