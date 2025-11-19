// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/cookie_store/global_cookie_store.h"

#include <utility>

#include "services/network/public/mojom/restricted_cookie_manager.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/modules/cookie_store/cookie_store.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

template <typename T>
class GlobalCookieStoreImpl final
    : public GarbageCollected<GlobalCookieStoreImpl<T>>,
      public GarbageCollectedMixin {
 public:
  static GlobalCookieStoreImpl& From(T& supplementable) {
    GlobalCookieStoreImpl* supplement =
        supplementable.GetGlobalCookieStoreImpl();
    if (!supplement) {
      supplement = MakeGarbageCollected<GlobalCookieStoreImpl>();
      supplementable.SetGlobalCookieStoreImpl(supplement);
    }
    return *supplement;
  }

  GlobalCookieStoreImpl() = default;

  CookieStore* GetCookieStore(T& scope) {
    if (!cookie_store_) {
      ExecutionContext* execution_context = scope.GetExecutionContext();
      if (&execution_context->GetBrowserInterfaceBroker() ==
          &GetEmptyBrowserInterfaceBroker()) {
        return nullptr;
      }

      HeapMojoRemote<network::mojom::blink::RestrictedCookieManager> backend(
          execution_context);
      execution_context->GetBrowserInterfaceBroker().GetInterface(
          backend.BindNewPipeAndPassReceiver(
              execution_context->GetTaskRunner(TaskType::kDOMManipulation)));
      cookie_store_ = MakeGarbageCollected<CookieStore>(execution_context,
                                                        std::move(backend));
    }
    return cookie_store_.Get();
  }

  void Trace(Visitor* visitor) const override { visitor->Trace(cookie_store_); }

 private:
  Member<CookieStore> cookie_store_;
};

// static
CookieStore* GlobalCookieStore::cookieStore(LocalDOMWindow& window) {
  return GlobalCookieStoreImpl<LocalDOMWindow>::From(window).GetCookieStore(
      window);
}

// static
CookieStore* GlobalCookieStore::cookieStore(ServiceWorkerGlobalScope& worker) {
  return GlobalCookieStoreImpl<WorkerGlobalScope>::From(worker).GetCookieStore(
      worker);
}

}  // namespace blink
