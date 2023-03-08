// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/worker_thread_util.h"

#include "content/public/renderer/worker_thread.h"
#include "extensions/common/constants.h"
#include "third_party/abseil-cpp/absl/base/attributes.h"
#include "third_party/blink/public/web/modules/service_worker/web_service_worker_context_proxy.h"

namespace extensions {
namespace worker_thread_util {

namespace {
ABSL_CONST_INIT thread_local blink::WebServiceWorkerContextProxy*
    worker_context_proxy = nullptr;
}

bool IsWorkerThread() {
  return content::WorkerThread::GetCurrentId() != kMainThreadId;
}

void SetWorkerContextProxy(blink::WebServiceWorkerContextProxy* context_proxy) {
  worker_context_proxy = context_proxy;
}

bool HasWorkerContextProxyInteraction() {
  DCHECK(IsWorkerThread());
  return worker_context_proxy &&
         worker_context_proxy->IsWindowInteractionAllowed();
}

}  // namespace worker_thread_util
}  // namespace extensions
