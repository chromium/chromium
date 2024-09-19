// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/lazy_background_task_queue.h"

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/lazy_background_task_queue_factory.h"
#include "extensions/browser/lazy_context_id.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/mojom/view_type.mojom.h"

namespace extensions {

namespace {

// Attempts to create a background host for a lazy background page. Returns true
// if the background host is created.
bool CreateLazyBackgroundHost(ProcessManager* pm, const Extension* extension) {
  pm->IncrementLazyKeepaliveCount(extension, Activity::LIFECYCLE_MANAGEMENT,
                                  Activity::kCreatePage);
  // Creating the background host may fail, e.g. if the extension isn't enabled
  // in incognito mode.
  return pm->CreateBackgroundHost(extension,
                                  BackgroundInfo::GetBackgroundURL(extension));
}

}  // namespace

LazyBackgroundTaskQueue::LazyBackgroundTaskQueue(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  extension_registry_observation_.Observe(
      ExtensionRegistry::Get(browser_context));
  extension_host_registry_observation_.Observe(
      ExtensionHostRegistry::Get(browser_context));
}

LazyBackgroundTaskQueue::~LazyBackgroundTaskQueue() {
}

// static
LazyBackgroundTaskQueue* LazyBackgroundTaskQueue::Get(
    content::BrowserContext* browser_context) {
  return LazyBackgroundTaskQueueFactory::GetForBrowserContext(browser_context);
}

bool LazyBackgroundTaskQueue::ShouldEnqueueTask(
    content::BrowserContext* browser_context,
    const Extension* extension) const {
  // Note: browser_context may not be the same as browser_context_ for incognito
  // extension tasks.
  DCHECK(extension);
  if (BackgroundInfo::HasBackgroundPage(extension)) {
    ProcessManager* pm = ProcessManager::Get(browser_context);
    ExtensionHost* background_host =
        pm->GetBackgroundHostForExtension(extension->id());
    if (!background_host || !background_host->has_loaded_once()) {
      return true;
    }
    if (pm->IsBackgroundHostClosing(extension->id())) {
      pm->CancelSuspend(extension);
    }
  }

  return false;
}

// TODO(crbug.com/40276609): Refactor into `ShouldEnqueueTask()` since they are
// so similar.
bool LazyBackgroundTaskQueue::IsReadyToRunTasks(
    content::BrowserContext* browser_context,
    const Extension* extension) const {
  // Note: browser_context may not be the same as browser_context_ for incognito
  // extension tasks.
  CHECK(extension);

  if (!BackgroundInfo::HasBackgroundPage(extension)) {
    return false;
  }

  ProcessManager* pm = ProcessManager::Get(browser_context);
  ExtensionHost* background_host =
      pm->GetBackgroundHostForExtension(extension->id());
  return background_host && background_host->has_loaded_once();
}

void LazyBackgroundTaskQueue::AddPendingTask(const LazyContextId& context_id,
                                             PendingTask task) {
  if (ExtensionsBrowserClient::Get()->IsShuttingDown()) {
    std::move(task).Run(nullptr);
    return;
  }
  const ExtensionId& extension_id = context_id.extension_id();
  content::BrowserContext* const browser_context = context_id.browser_context();
  auto it = pending_tasks_.find(context_id);
  if (it != pending_tasks_.end()) {
    it->second.push_back(std::move(task));
  } else {
    const Extension* extension = ExtensionRegistry::Get(browser_context)
                                     ->enabled_extensions()
                                     .GetByID(extension_id);
    if (extension && BackgroundInfo::HasLazyBackgroundPage(extension)) {
      // If this is the first enqueued task, and we're not waiting for the
      // background page to unload, ensure the background page is loaded.
      if (!CreateLazyBackgroundHost(ProcessManager::Get(browser_context),
                                    extension)) {
        std::move(task).Run(nullptr);
        return;
      }
    }
    pending_tasks_[context_id].push_back(std::move(task));
  }
}

void LazyBackgroundTaskQueue::ProcessPendingTasks(
    ExtensionHost* host,
    content::BrowserContext* browser_context,
    const Extension* extension) {
  DCHECK(extension);

  if (!ExtensionsBrowserClient::Get()->IsSameContext(browser_context,
                                                     browser_context_)) {
    return;
  }

  const auto key = LazyContextId::ForExtension(browser_context, extension);
  if (key.IsForServiceWorker()) {
    return;
  }

  auto map_it = pending_tasks_.find(key);
  if (map_it == pending_tasks_.end()) {
    if (BackgroundInfo::HasLazyBackgroundPage(extension)) {
      CHECK(!host);  // lazy page should not load without any pending tasks
    }
    return;
  }

  // Move the pending tasks to a temporary to avoid problems if the pending
  // tasks map is modified during processing, which might invalidate the
  // iterator.
  PendingTasksList tasks = std::move(map_it->second);
  for (auto& task : tasks) {
    std::move(task).Run(host ? std::make_unique<ContextInfo>(host) : nullptr);
  }

  pending_tasks_.erase(key);

  // Balance the keepalive in CreateLazyBackgroundHost. Note we don't do this on
  // a failure to load, because the keepalive count is reset in that case.
  if (host && BackgroundInfo::HasLazyBackgroundPage(extension)) {
    ProcessManager::Get(browser_context)
        ->DecrementLazyKeepaliveCount(extension, Activity::LIFECYCLE_MANAGEMENT,
                                      Activity::kCreatePage);
  }
}

void LazyBackgroundTaskQueue::NotifyTasksExtensionFailedToLoad(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  ProcessPendingTasks(nullptr, browser_context, extension);
  // If this extension is also running in an off-the-record context, notify that
  // task queue as well.
  ExtensionsBrowserClient* browser_client = ExtensionsBrowserClient::Get();
  if (browser_client->HasOffTheRecordContext(browser_context)) {
    ProcessPendingTasks(nullptr,
                        browser_client->GetOffTheRecordContext(browser_context),
                        extension);
  }
}

void LazyBackgroundTaskQueue::OnExtensionHostCompletedFirstLoad(
    content::BrowserContext* browser_context,
    ExtensionHost* host) {
  // If an on-demand background page finished loading, dispatch queued up
  // events for it.
  if (host->extension_host_type() ==
      mojom::ViewType::kExtensionBackgroundPage) {
    CHECK(host->has_loaded_once());
    ProcessPendingTasks(host, host->browser_context(), host->extension());
  }
}

void LazyBackgroundTaskQueue::OnExtensionHostDestroyed(
    content::BrowserContext* browser_context,
    ExtensionHost* host) {
  // Notify consumers about the load failure when the background host dies.
  // This can happen if the extension crashes. This is not strictly
  // necessary, since we also unload the extension in that case (which
  // dispatches the tasks below), but is a good extra precaution.
  if (host->extension() && host->extension_host_type() ==
                               mojom::ViewType::kExtensionBackgroundPage) {
    ProcessPendingTasks(nullptr, browser_context, host->extension());
  }
}

void LazyBackgroundTaskQueue::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  // If there are pending tasks for a lazy background page, and its background
  // host has not been created yet, then create it. This can happen if a pending
  // task was added while the extension is not yet enabled (e.g., component
  // extension crashed and waiting to reload, https://crbug.com/835017).
  if (!BackgroundInfo::HasLazyBackgroundPage(extension)) {
    return;
  }

  CreateLazyBackgroundHostOnExtensionLoaded(browser_context, extension);

  // Also try to create the background host for the off-the-record context.
  ExtensionsBrowserClient* browser_client = ExtensionsBrowserClient::Get();
  if (browser_client->HasOffTheRecordContext(browser_context)) {
    CreateLazyBackgroundHostOnExtensionLoaded(
        browser_client->GetOffTheRecordContext(browser_context), extension);
  }
}

void LazyBackgroundTaskQueue::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  NotifyTasksExtensionFailedToLoad(browser_context, extension);
}

void LazyBackgroundTaskQueue::CreateLazyBackgroundHostOnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  const auto key = LazyContextId::ForExtension(browser_context, extension);
  CHECK(key.IsForBackgroundPage());
  if (!base::Contains(pending_tasks_, key)) {
    return;
  }

  ProcessManager* pm = ProcessManager::Get(browser_context);

  // Background host already created, just wait for it to finish loading.
  if (pm->GetBackgroundHostForExtension(extension->id())) {
    return;
  }

  if (!CreateLazyBackgroundHost(pm, extension)) {
    ProcessPendingTasks(nullptr, browser_context, extension);
  }
}

}  // namespace extensions
