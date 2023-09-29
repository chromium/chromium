// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/service_worker/service_worker_host.h"

#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_external_request_result.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/bad_message.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_map.h"
#include "extensions/browser/service_worker_task_queue.h"
#include "extensions/common/constants.h"
#include "extensions/common/mojom/frame.mojom.h"

namespace extensions {

namespace {
const void* const kUserDataKey = &kUserDataKey;
}  // namespace

ServiceWorkerHost::ServiceWorkerHost(
    content::RenderProcessHost* render_process_host)
    : render_process_host_(render_process_host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  dispatcher_ =
      std::make_unique<ExtensionFunctionDispatcher>(GetBrowserContext());
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
    int worker_thread_id,
    mojo::PendingAssociatedRemote<mojom::EventDispatcher> event_dispatcher) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::BrowserContext* browser_context = GetBrowserContext();
  if (!browser_context) {
    return;
  }

  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context);
  DCHECK(registry);
  if (!registry->enabled_extensions().GetByID(extension_id)) {
    // This can happen if the extension is unloaded at this point. Just
    // checking the extension process (as below) is insufficient because
    // tearing down processes is async and happens after extension unload.
    return;
  }

  int render_process_id = render_process_host_->GetID();
  auto* process_map = ProcessMap::Get(browser_context);
  if (!process_map || !process_map->Contains(extension_id, render_process_id)) {
    // We check the process in addition to the registry to guard against
    // situations in which an extension may still be enabled, but no longer
    // running in a given process.
    return;
  }

  ServiceWorkerTaskQueue::Get(browser_context)
      ->DidInitializeServiceWorkerContext(render_process_id, extension_id,
                                          service_worker_version_id,
                                          worker_thread_id);
  EventRouter::Get(browser_context)
      ->BindServiceWorkerEventDispatcher(render_process_id, worker_thread_id,
                                         std::move(event_dispatcher));
}

void ServiceWorkerHost::DidStartServiceWorkerContext(
    const ExtensionId& extension_id,
    const base::UnguessableToken& activation_token,
    const GURL& service_worker_scope,
    int64_t service_worker_version_id,
    int worker_thread_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::BrowserContext* browser_context = GetBrowserContext();
  if (!browser_context) {
    return;
  }

  DCHECK_NE(kMainThreadId, worker_thread_id);
  int render_process_id = render_process_host_->GetID();
  auto* process_map = ProcessMap::Get(browser_context);
  if (!process_map || !process_map->Contains(extension_id, render_process_id)) {
    // We can legitimately get here if the extension was already unloaded.
    return;
  }
  CHECK(service_worker_scope.SchemeIs(kExtensionScheme) &&
        extension_id == service_worker_scope.host_piece());

  ServiceWorkerTaskQueue::Get(browser_context)
      ->DidStartServiceWorkerContext(
          render_process_id, extension_id, activation_token,
          service_worker_scope, service_worker_version_id, worker_thread_id);
}

void ServiceWorkerHost::DidStopServiceWorkerContext(
    const ExtensionId& extension_id,
    const base::UnguessableToken& activation_token,
    const GURL& service_worker_scope,
    int64_t service_worker_version_id,
    int worker_thread_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::BrowserContext* browser_context = GetBrowserContext();
  if (!browser_context) {
    return;
  }

  DCHECK_NE(kMainThreadId, worker_thread_id);
  int render_process_id = render_process_host_->GetID();
  auto* process_map = ProcessMap::Get(browser_context);
  if (!process_map || !process_map->Contains(extension_id, render_process_id)) {
    // We can legitimately get here if the extension was already unloaded.
    return;
  }
  CHECK(service_worker_scope.SchemeIs(kExtensionScheme) &&
        extension_id == service_worker_scope.host_piece());

  ServiceWorkerTaskQueue::Get(browser_context)
      ->DidStopServiceWorkerContext(
          render_process_id, extension_id, activation_token,
          service_worker_scope, service_worker_version_id, worker_thread_id);
}

void ServiceWorkerHost::RequestWorker(mojom::RequestParamsPtr params) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!GetBrowserContext()) {
    return;
  }

  dispatcher_->DispatchForServiceWorker(std::move(params),
                                        render_process_host_->GetID());
}

void ServiceWorkerHost::WorkerResponseAck(const base::Uuid& request_uuid) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!GetBrowserContext()) {
    return;
  }

  dispatcher_->ProcessResponseAck(request_uuid);
}

content::BrowserContext* ServiceWorkerHost::GetBrowserContext() {
  return render_process_host_->GetBrowserContext();
}

}  // namespace extensions
