// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/cache_storage/global_cache_storage.h"

#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/window_or_worker_global_scope.h"
#include "third_party/blink/renderer/modules/cache_storage/cache_storage.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

GlobalCacheStorage& GlobalCacheStorage::From(
    WindowOrWorkerGlobalScope& supplementable) {
  GlobalCacheStorage* supplement = supplementable.GetGlobalCacheStorage();
  if (!supplement) {
    supplement = MakeGarbageCollected<GlobalCacheStorage>();
    supplementable.SetGlobalCacheStorage(supplement);
  }
  return *supplement;
}

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

CacheStorage* GlobalCacheStorage::Caches(
    WindowOrWorkerGlobalScope& window_or_worker,
    ExceptionState& exception_state) {
  ExecutionContext* context = window_or_worker.GetExecutionContext();
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
        context, GlobalFetch::ScopedFetcher::From(window_or_worker));
  }
  return caches_.Get();
}

void GlobalCacheStorage::Trace(Visitor* visitor) const {
  visitor->Trace(caches_);
}

CacheStorage* GlobalCacheStorage::caches(
    WindowOrWorkerGlobalScope& window_or_worker,
    ExceptionState& exception_state) {
  return GlobalCacheStorage::From(window_or_worker)
      .Caches(window_or_worker, exception_state);
}

}  // namespace blink
