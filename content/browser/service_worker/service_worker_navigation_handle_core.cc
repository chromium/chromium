// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_navigation_handle_core.h"

#include <utility>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_navigation_handle.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/child_process_host.h"

namespace content {

ServiceWorkerNavigationHandleCore::ServiceWorkerNavigationHandleCore(
    base::WeakPtr<ServiceWorkerNavigationHandle> ui_handle,
    ServiceWorkerContextWrapper* context_wrapper)
    : context_wrapper_(context_wrapper), ui_handle_(ui_handle) {
  // The ServiceWorkerNavigationHandleCore is created on the UI thread but
  // should only be accessed from the IO thread afterwards.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

ServiceWorkerNavigationHandleCore::~ServiceWorkerNavigationHandleCore() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (provider_id_ == kInvalidServiceWorkerProviderId)
    return;
  ServiceWorkerContextCore* context = context_wrapper_->context();
  if (!context)
    return;
  ServiceWorkerProviderHost* host = context->GetProviderHost(
      ChildProcessHost::kInvalidUniqueID, provider_id_);
  if (!host)
    return;
  // Remove the provider host if it was never completed (navigation failed).
  // TODO(falken): ServiceWorkerNavigationHandleCore should just own a Mojo
  // pointer tied to the lifetime of ServiceWorkerProviderHost, and send the
  // Mojo pointer to the renderer on navigation commit. If the handle core dies
  // before that, the provider host would be destroyed by Mojo connection error.
  if (!host->is_execution_ready())
    context->RemoveProviderHost(ChildProcessHost::kInvalidUniqueID,
                                provider_id_);
}

void ServiceWorkerNavigationHandleCore::DidPreCreateProviderHost(
    int provider_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(ServiceWorkerUtils::IsBrowserAssignedProviderId(provider_id));

  provider_id_ = provider_id;
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          &ServiceWorkerNavigationHandle::DidCreateServiceWorkerProviderHost,
          ui_handle_, provider_id_));
}

}  // namespace content
