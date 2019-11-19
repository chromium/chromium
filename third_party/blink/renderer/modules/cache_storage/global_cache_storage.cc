// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/cache_storage/global_cache_storage.h"

#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/modules/cache_storage/cache_storage.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

namespace {

template <typename T>
class GlobalCacheStorageImpl final
    : public GarbageCollected<GlobalCacheStorageImpl<T>>,
      public Supplement<T> {
  USING_GARBAGE_COLLECTED_MIXIN(GlobalCacheStorageImpl);

 public:
  static const char kSupplementName[];

  static GlobalCacheStorageImpl& From(T& supplementable,
                                      ExecutionContext* execution_context) {
    GlobalCacheStorageImpl* supplement =
        Supplement<T>::template From<GlobalCacheStorageImpl>(supplementable);
    if (!supplement) {
      supplement = MakeGarbageCollected<GlobalCacheStorageImpl>();
      Supplement<T>::ProvideTo(supplementable, supplement);
    }
    return *supplement;
  }

  GlobalCacheStorageImpl() = default;
  ~GlobalCacheStorageImpl() {}

  CacheStorage* Caches(T& fetching_scope, ExceptionState& exception_state) {
    ExecutionContext* context = fetching_scope.GetExecutionContext();
    if (!context->GetSecurityOrigin()->CanAccessCacheStorage()) {
      if (context->GetSecurityContext().IsSandboxed(WebSandboxFlags::kOrigin)) {
        exception_state.ThrowSecurityError(
            "Cache storage is disabled because the context is sandboxed and "
            "lacks the 'allow-same-origin' flag.");
      } else if (context->Url().ProtocolIs("data")) {
        exception_state.ThrowSecurityError(
            "Cache storage is disabled inside 'data:' URLs.");
      } else {
        exception_state.ThrowSecurityError(
            "Access to cache storage is denied.");
      }
      return nullptr;
    }

    if (context->GetSecurityOrigin()->IsLocal()) {
      UseCounter::Count(context, WebFeature::kFileAccessedCache);
    }

    if (!caches_) {
      if (!context->GetInterfaceProvider()) {
        exception_state.ThrowSecurityError(
            "Cache storage isn't available on detached context. No interface "
            "provider.");
        return nullptr;
      }
      caches_ = MakeGarbageCollected<CacheStorage>(
          context, GlobalFetch::ScopedFetcher::From(fetching_scope));
    }
    return caches_;
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(caches_);
    Supplement<T>::Trace(visitor);
  }

 private:
  Member<CacheStorage> caches_;
};

// static
template <typename T>
const char GlobalCacheStorageImpl<T>::kSupplementName[] =
    "GlobalCacheStorageImpl";

}  // namespace

CacheStorage* GlobalCacheStorage::caches(LocalDOMWindow& window,
                                         ExceptionState& exception_state) {
  return GlobalCacheStorageImpl<LocalDOMWindow>::From(
             window, window.GetExecutionContext())
      .Caches(window, exception_state);
}

CacheStorage* GlobalCacheStorage::caches(WorkerGlobalScope& worker,
                                         ExceptionState& exception_state) {
  return GlobalCacheStorageImpl<WorkerGlobalScope>::From(
             worker, worker.GetExecutionContext())
      .Caches(worker, exception_state);
}

}  // namespace blink
