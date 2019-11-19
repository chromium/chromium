// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/service_worker_manager.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"

namespace extensions {

ServiceWorkerManager::ServiceWorkerManager(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  registry_observer_.Add(ExtensionRegistry::Get(browser_context_));
}

ServiceWorkerManager::~ServiceWorkerManager() {}

void ServiceWorkerManager::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  content::BrowserContext::GetStoragePartitionForSite(browser_context_,
                                                      extension->url())
      ->GetServiceWorkerContext()
      ->StopAllServiceWorkersForOrigin(extension->url());
}

void ServiceWorkerManager::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UninstallReason reason) {
  // TODO(devlin): Technically, this can fail. We should ideally:
  // a) Keep track of extensions with registered service workers.
  // b) Add a callback to the (Un)SuspendServiceWorkersOnOrigin() method.
  // c) Check for any orphaned workers.
  content::BrowserContext::GetStoragePartitionForSite(browser_context_,
                                                      extension->url())
      ->GetServiceWorkerContext()
      ->DeleteForOrigin(extension->url(), base::DoNothing());
}

}  // namespace extensions
