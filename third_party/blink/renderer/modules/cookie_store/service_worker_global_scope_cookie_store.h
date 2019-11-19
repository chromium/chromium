// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_COOKIE_STORE_SERVICE_WORKER_GLOBAL_SCOPE_COOKIE_STORE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_COOKIE_STORE_SERVICE_WORKER_GLOBAL_SCOPE_COOKIE_STORE_H_

#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class CookieStore;
class ServiceWorkerGlobalScope;

// Exposes a CookieStore as the "cookieStore" attribute on the SW global scope.
class ServiceWorkerGlobalScopeCookieStore {
  STATIC_ONLY(ServiceWorkerGlobalScopeCookieStore);

 public:
  static CookieStore* cookieStore(ServiceWorkerGlobalScope&);

  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(cookiechange, kCookiechange)
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_COOKIE_STORE_SERVICE_WORKER_GLOBAL_SCOPE_COOKIE_STORE_H_
