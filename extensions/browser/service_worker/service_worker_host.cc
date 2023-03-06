// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/service_worker/service_worker_host.h"

#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_map.h"
#include "extensions/browser/service_worker_task_queue.h"

namespace extensions {

namespace {
const void* const kUserDataKey = &kUserDataKey;
}  // namespace

ServiceWorkerHost::ServiceWorkerHost(
    content::RenderProcessHost* render_process_host)
    : render_process_host_(render_process_host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

ServiceWorkerHost::~ServiceWorkerHost() = default;

// static
void ServiceWorkerHost::BindReceiver(
    int render_process_id,
    mojo::PendingAssociatedReceiver<mojom::ServiceWorkerHost> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* render_process_host =
      content::RenderProcessHost::FromID(render_process_id);
  if (!render_process_host) {
    return;
  }

  auto* service_worker_host = static_cast<ServiceWorkerHost*>(
      render_process_host->GetUserData(kUserDataKey));
  if (!service_worker_host) {
    auto new_host = std::make_unique<ServiceWorkerHost>(render_process_host);
    service_worker_host = new_host.get();
    render_process_host->SetUserData(kUserDataKey, std::move(new_host));
  }

  service_worker_host->receiver_.Bind(std::move(receiver));
  service_worker_host->receiver_.reset_on_disconnect();
}

void ServiceWorkerHost::DidInitializeServiceWorkerContext(
    const ExtensionId& extension_id,
    int64_t service_worker_version_id,
    int worker_thread_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::BrowserContext* browser_context = GetBrowserContext();
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context);
  DCHECK(registry);
  if (!registry->enabled_extensions().GetByID(extension_id)) {
    // This can happen if the extension is unloaded at this point. Just
    // checking the extension process (as below) is insufficient because
    // tearing down processes is async and happens after extension unload.
    return;
  }

  int render_process_id = render_process_host_->GetID();
  if (!ProcessMap::Get(browser_context)
           ->Contains(extension_id, render_process_id)) {
    // We check the process in addition to the registry to guard against
    // situations in which an extension may still be enabled, but no longer
    // running in a given process.
    return;
  }

  ServiceWorkerTaskQueue::Get(browser_context)
      ->DidInitializeServiceWorkerContext(render_process_id, extension_id,
                                          service_worker_version_id,
                                          worker_thread_id);
}

content::BrowserContext* ServiceWorkerHost::GetBrowserContext() {
  return render_process_host_->GetBrowserContext();
}

}  // namespace extensions
