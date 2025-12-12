// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/cache_storage/global_cache_storage.h"

#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/modules/cache_storage/cache_storage.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

namespace {

template <typename T>
class GlobalCacheStorageImpl final
    : public GarbageCollected<GlobalCacheStorageImpl<T>>,
      public Supplement<T> {
 public:
  static const char kSupplementName[];

  static GlobalCacheStorageImpl& From(T& supplementable) {
    GlobalCacheStorageImpl* supplement =
        Supplement<T>::template From<GlobalCacheStorageImpl>(supplementable);
    if (!supplement) {
      supplement = MakeGarbageCollected<GlobalCacheStorageImpl>(supplementable);
      Supplement<T>::ProvideTo(supplementable, supplement);
    }
    return *supplement;
  }

  GlobalCacheStorageImpl(T& supplementable) : Supplement<T>(supplementable) {}
  ~GlobalCacheStorageImpl() = default;

  CacheStorage* Caches(T& fetching_scope, ExceptionState& exception_state) {
    ExecutionContext* context = fetching_scope.GetExecutionContext();
    if (!GlobalCacheStorage::CanCreateCacheStorage(context, exception_state)) {
      return nullptr;
    }

    if (context->GetSecurityOrigin()->IsLocal()) {
      UseCounter::Count(context, WebFeature::kFileAccessedCache);
    }

    if (!caches_) {
      if (&context->GetBrowserInterfaceBroker() ==
          &GetEmptyBrowserInterfaceBroker()) {
        exception_state.ThrowSecurityError(
            "Cache storage isn't available on detached context. No browser "
            "interface broker.");
        return nullptr;
      }
      caches_ = MakeGarbageCollected<CacheStorage>(
          context, GlobalFetch::ScopedFetcher::From(fetching_scope));
    }
    return caches_.Get();
  }

  void Trace(Visitor* visitor) const override {
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

bool GlobalCacheStorage::CanCreateCacheStorage(
    ExecutionContext* context,
    ExceptionState& exception_state) {
  if (context->GetSecurityOrigin()->CanAccessCacheStorage()) {
    return true;
  }

  if (context->IsSandboxed(network::mojom::blink::WebSandboxFlags::kOrigin)) {
    exception_state.ThrowSecurityError(
        "Cache storage is disabled because the context is sandboxed and "
        "lacks the 'allow-same-origin' flag.");
  } else if (context->Url().ProtocolIs("data")) {
    exception_state.ThrowSecurityError(
        "Cache storage is disabled inside 'data:' URLs.");
  } else {
    exception_state.ThrowSecurityError("Access to cache storage is denied.");
  }
  return false;
}

CacheStorage* GlobalCacheStorage::caches(LocalDOMWindow& window,
                                         ExceptionState& exception_state) {
  return GlobalCacheStorageImpl<LocalDOMWindow>::From(window).Caches(
      window, exception_state);
}

CacheStorage* GlobalCacheStorage::caches(WorkerGlobalScope& worker,
                                         ExceptionState& exception_state) {
  return GlobalCacheStorageImpl<WorkerGlobalScope>::From(worker).Caches(
      worker, exception_state);
}

}  // namespace blink
