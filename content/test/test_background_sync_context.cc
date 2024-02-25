// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_background_sync_context.h"

#include <memory>
#include <utility>

#include "content/browser/devtools/devtools_background_services_context_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_thread.h"
#include "content/test/test_background_sync_manager.h"

namespace content {

void TestBackgroundSyncContext::CreateBackgroundSyncManager(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    DevToolsBackgroundServicesContextImpl& devtools_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!background_sync_manager());

  set_background_sync_manager_for_testing(
      std::make_unique<TestBackgroundSyncManager>(
          std::move(service_worker_context), devtools_context));
}

}  // namespace content
