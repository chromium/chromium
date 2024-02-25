// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/worker_thread_util.h"

#include "content/public/renderer/worker_thread.h"
#include "extensions/common/constants.h"
#include "extensions/renderer/service_worker_data.h"
#include "extensions/renderer/worker_thread_dispatcher.h"
#include "third_party/blink/public/web/modules/service_worker/web_service_worker_context_proxy.h"

namespace extensions {
namespace worker_thread_util {

bool IsWorkerThread() {
  return content::WorkerThread::GetCurrentId() != kMainThreadId;
}

bool HasWorkerContextProxyInteraction() {
  DCHECK(IsWorkerThread());
  ServiceWorkerData* data = WorkerThreadDispatcher::GetServiceWorkerData();
  return data && data->worker_context_proxy()->IsWindowInteractionAllowed();
}

}  // namespace worker_thread_util
}  // namespace extensions
