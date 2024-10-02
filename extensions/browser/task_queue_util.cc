// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/task_queue_util.h"

#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/lazy_background_task_queue.h"
#include "extensions/browser/lazy_context_id.h"
#include "extensions/browser/service_worker/service_worker_task_queue.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/incognito_info.h"

namespace extensions {

namespace {

#if DCHECK_IS_ON()
// Whether the task queue is allowed to be created for OTR profile.
bool IsOffTheRecordContextAllowed(content::BrowserContext* browser_context) {
#if BUILDFLAG(IS_CHROMEOS)
  // In Guest mode on Chrome OS we want to create a task queue for OTR profile.
  if (ExtensionsBrowserClient::Get()->IsGuestSession(browser_context)) {
    return true;
  }
#endif

  // In other cases don't create a task queue for OTR profile.
  return false;
}
#endif  // DCHECK_IS_ON()

// Get the ServiceWorkerTaskQueue instance for the BrowserContext.
//
ServiceWorkerTaskQueue* GetServiceWorkerTaskQueueForBrowserContext(
    content::BrowserContext* browser_context,
    bool is_split_mode) {
  content::BrowserContext* context_to_use = browser_context;
  // Incognito extensions in split mode use their own task queue, while those
  // in spanning mode use the task queue of the original BrowserContext.
  if (browser_context->IsOffTheRecord() && !is_split_mode) {
    context_to_use =
        ExtensionsBrowserClient::Get()->GetOriginalContext(browser_context);
  }
  return ServiceWorkerTaskQueue::Get(context_to_use);
}

// Get the ServiceWorkerTaskQueue instance for the extension.
//
// Only call this for a SW-based extension.
ServiceWorkerTaskQueue* GetServiceWorkerTaskQueueForExtension(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  DCHECK(BackgroundInfo::IsServiceWorkerBased(extension));
  return GetServiceWorkerTaskQueueForBrowserContext(
      browser_context, IncognitoInfo::IsSplitMode(extension));
}

// Get the ServiceWorkerTaskQueue instance for the extension ID.
//
// Only call this for a SW-based extension.
ServiceWorkerTaskQueue* GetServiceWorkerTaskQueueForExtensionId(
    content::BrowserContext* browser_context,
    const ExtensionId& extension_id) {
  // Incognito extensions in split mode use their own task queue, while those
  // in spanning mode use the task queue of the original BrowserContext.
  // This is an optimization to avoid looking up an Extension instance,
  // since we only need it for the off-the-record case.
  if (!browser_context->IsOffTheRecord()) {
    return ServiceWorkerTaskQueue::Get(browser_context);
  }

  const Extension* extension = ExtensionRegistry::Get(browser_context)
                                   ->enabled_extensions()
                                   .GetByID(extension_id);
  DCHECK(extension);
  return GetServiceWorkerTaskQueueForExtension(browser_context, extension);
}

// Use a pointer-to-member function so we can use the same logic for the
// activation and deactivation paths.
using TaskQueueFunction = void (ServiceWorkerTaskQueue::*)(const Extension*);

void DoTaskQueueFunction(content::BrowserContext* browser_context,
                         const Extension* extension,
                         TaskQueueFunction function) {
#if DCHECK_IS_ON()
  DCHECK(IsOffTheRecordContextAllowed(browser_context) ||
         !browser_context->IsOffTheRecord());
#endif  // DCHECK_IS_ON()

  // This is only necessary for service worker-based extensions.
  if (!BackgroundInfo::IsServiceWorkerBased(extension)) {
    return;
  }

  ServiceWorkerTaskQueue* const queue =
      ServiceWorkerTaskQueue::Get(browser_context);
  (queue->*function)(extension);

  // There is a separate task queue for the off-the-record context
  // for any extension running in split mode.
  if (!ExtensionsBrowserClient::Get()->HasOffTheRecordContext(
          browser_context) ||
      !IncognitoInfo::IsSplitMode(extension) ||
      !ExtensionsBrowserClient::Get()->IsExtensionIncognitoEnabled(
          extension->id(), browser_context)) {
    return;
  }

  content::BrowserContext* off_the_record_context =
      ExtensionsBrowserClient::Get()->GetOffTheRecordContext(browser_context);
  DCHECK(off_the_record_context);
  ServiceWorkerTaskQueue* const off_the_record_queue =
      ServiceWorkerTaskQueue::Get(off_the_record_context);
  (off_the_record_queue->*function)(extension);
}

}  // anonymous namespace

LazyContextTaskQueue* GetTaskQueueForLazyContextId(
    const LazyContextId& context_id) {
  if (context_id.IsForBackgroundPage()) {
    return LazyBackgroundTaskQueue::Get(context_id.browser_context());
  }

  if (context_id.IsForServiceWorker()) {
    return GetServiceWorkerTaskQueueForExtensionId(context_id.browser_context(),
                                                   context_id.extension_id());
  }

  return nullptr;
}

void ActivateTaskQueueForExtension(content::BrowserContext* browser_context,
                                   const Extension* extension) {
  DoTaskQueueFunction(browser_context, extension,
                      &ServiceWorkerTaskQueue::ActivateExtension);
}

void DeactivateTaskQueueForExtension(content::BrowserContext* browser_context,
                                     const Extension* extension) {
  DoTaskQueueFunction(browser_context, extension,
                      &ServiceWorkerTaskQueue::DeactivateExtension);
}

}  // namespace extensions
