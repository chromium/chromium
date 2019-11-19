// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_COOKIE_STORE_GLOBAL_COOKIE_STORE_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_COOKIE_STORE_GLOBAL_COOKIE_STORE_IMPL_H_

#include <utility>

#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/cookie_store/cookie_store.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

template <typename T>
class GlobalCookieStoreImpl final
    : public GarbageCollected<GlobalCookieStoreImpl<T>>,
      public Supplement<T> {
  USING_GARBAGE_COLLECTED_MIXIN(GlobalCookieStoreImpl);

 public:
  static const char kSupplementName[];

  static GlobalCookieStoreImpl& From(T& supplementable) {
    GlobalCookieStoreImpl* supplement =
        Supplement<T>::template From<GlobalCookieStoreImpl>(supplementable);
    if (!supplement) {
      supplement = MakeGarbageCollected<GlobalCookieStoreImpl>(supplementable);
      Supplement<T>::ProvideTo(supplementable, supplement);
    }
    return *supplement;
  }

  explicit GlobalCookieStoreImpl(T& supplementable)
      : Supplement<T>(supplementable) {}

  CookieStore* GetCookieStore(T& scope) {
    if (!cookie_store_) {
      ExecutionContext* execution_context = scope.GetExecutionContext();

      service_manager::InterfaceProvider* interface_provider =
          execution_context->GetInterfaceProvider();
      if (!interface_provider)
        return nullptr;
      cookie_store_ = BuildCookieStore(execution_context, interface_provider);
    }
    return cookie_store_;
  }

  CookieStore* BuildCookieStore(ExecutionContext*,
                                service_manager::InterfaceProvider*);

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(cookie_store_);
    Supplement<T>::Trace(visitor);
  }

 private:
  Member<CookieStore> cookie_store_;
};

// static
template <typename T>
const char GlobalCookieStoreImpl<T>::kSupplementName[] =
    "GlobalCookieStoreImpl";

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_COOKIE_STORE_GLOBAL_COOKIE_STORE_IMPL_H_
