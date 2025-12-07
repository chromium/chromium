// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/service_worker_manager.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/service_worker_context.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace extensions {

namespace {

// Returns the incognito browser context if it exists and the given `extension`
// is enabled in incognito and is in split mode. Otherwise, returns nullptr.
content::BrowserContext* GetIncognitoContextForSplitMode(
    content::BrowserContext* context,
    const Extension* extension) {
  if (context->IsOffTheRecord() ||
      !ExtensionsBrowserClient::Get()->HasOffTheRecordContext(context) ||
      !IncognitoInfo::IsSplitMode(extension) ||
      !ExtensionsBrowserClient::Get()->IsExtensionIncognitoEnabled(
          extension->id(), context)) {
    return nullptr;
  }
  return ExtensionsBrowserClient::Get()->GetOffTheRecordContext(context);
}

}  // namespace

ServiceWorkerManager::ServiceWorkerManager(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  registry_observation_.Observe(ExtensionRegistry::Get(browser_context_));
}

ServiceWorkerManager::~ServiceWorkerManager() = default;

void ServiceWorkerManager::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  util::GetServiceWorkerContextForExtensionId(extension->id(), browser_context_)
      ->StopAllServiceWorkersForStorageKey(
          blink::StorageKey::CreateFirstParty(extension->origin()));

  // If the extension is in split mode, we need to also stop the service
  // workers in the incognito context.
  if (content::BrowserContext* incognito_context =
          GetIncognitoContextForSplitMode(browser_context_, extension)) {
    util::GetServiceWorkerContextForExtensionId(extension->id(),
                                                incognito_context)
        ->StopAllServiceWorkersForStorageKey(
            blink::StorageKey::CreateFirstParty(extension->origin()));
  }
}

void ServiceWorkerManager::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UninstallReason reason) {
  // TODO(devlin): Technically, this can fail. We should ideally:
  // a) Keep track of extensions with registered service workers.
  // b) Add a callback to the (Un)SuspendServiceWorkersOnOrigin() method.
  // c) Check for any orphaned workers.
  util::GetServiceWorkerContextForExtensionId(extension->id(), browser_context_)
      ->DeleteForStorageKey(
          blink::StorageKey::CreateFirstParty(extension->origin()),
          base::DoNothing());

  if (content::BrowserContext* incognito_context =
          GetIncognitoContextForSplitMode(browser_context_, extension)) {
    util::GetServiceWorkerContextForExtensionId(extension->id(),
                                                incognito_context)
        ->DeleteForStorageKey(
            blink::StorageKey::CreateFirstParty(extension->origin()),
            base::DoNothing());
  }
}

}  // namespace extensions
