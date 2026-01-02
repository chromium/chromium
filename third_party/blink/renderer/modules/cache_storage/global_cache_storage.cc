// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/cache_storage/global_cache_storage.h"

#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/cache_storage/cache_storage.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

const char GlobalCacheStorage::kSupplementName[] = "GlobalCacheStorage";

GlobalCacheStorage::GlobalCacheStorage(ExecutionContext& context)
    : Supplement<ExecutionContext>(context) {}

GlobalCacheStorage& GlobalCacheStorage::From(ExecutionContext& context) {
  GlobalCacheStorage* supplement =
      Supplement<ExecutionContext>::From<GlobalCacheStorage>(context);
  if (!supplement) {
    supplement = MakeGarbageCollected<GlobalCacheStorage>(context);
    Supplement<ExecutionContext>::ProvideTo(context, supplement);
  }
  return *supplement;
}

CacheStorage* GlobalCacheStorage::Caches(ExecutionContext* context,
                                         ExceptionState& exception_state) {
  if (!GlobalCacheStorage::CanCreateCacheStorage(context, exception_state)) {
    return nullptr;
  }

  if (context->GetSecurityOrigin()->IsLocal()) {
    UseCounter::Count(context,
                      blink::mojom::blink::WebFeature::kFileAccessedCache);
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
        context, GlobalFetch::ScopedFetcher::From(*context));
  }
  return caches_.Get();
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

CacheStorage* GlobalCacheStorage::caches(ExecutionContext& context,
                                         ExceptionState& exception_state) {
  return GlobalCacheStorage::From(context).Caches(&context, exception_state);
}

void GlobalCacheStorage::Trace(Visitor* visitor) const {
  visitor->Trace(caches_);
  Supplement<ExecutionContext>::Trace(visitor);
}

}  // namespace blink
