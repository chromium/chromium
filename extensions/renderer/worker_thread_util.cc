// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/worker_thread_util.h"

#include "base/lazy_instance.h"
#include "base/threading/thread_local.h"
#include "content/public/renderer/worker_thread.h"
#include "extensions/common/constants.h"
#include "third_party/blink/public/web/modules/service_worker/web_service_worker_context_proxy.h"

namespace extensions {
namespace worker_thread_util {

namespace {
base::LazyInstance<
    base::ThreadLocalPointer<blink::WebServiceWorkerContextProxy>>::Leaky
    g_worker_context_proxy_tls = LAZY_INSTANCE_INITIALIZER;
}

bool IsWorkerThread() {
  return content::WorkerThread::GetCurrentId() != kMainThreadId;
}

void SetWorkerContextProxy(blink::WebServiceWorkerContextProxy* context_proxy) {
  g_worker_context_proxy_tls.Pointer()->Set(context_proxy);
}

bool HasWorkerContextProxyInteraction() {
  DCHECK(IsWorkerThread());
  blink::WebServiceWorkerContextProxy* proxy =
      g_worker_context_proxy_tls.Pointer()->Get();
  return proxy && proxy->IsWindowInteractionAllowed();
}

}  // namespace worker_thread_util
}  // namespace extensions
