// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/warning_service.h"

#include "base/observer_list.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/warning_service_factory.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_set.h"

using content::BrowserThread;

namespace extensions {

WarningService::WarningService(content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (browser_context_) {
    extension_registry_observation_.Observe(ExtensionRegistry::Get(
        ExtensionsBrowserClient::Get()->GetContextRedirectedToOriginal(
            browser_context_, /*force_guest_profile=*/true)));
  }
}

WarningService::~WarningService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

// static
WarningService* WarningService::Get(content::BrowserContext* browser_context) {
  return WarningServiceFactory::GetForBrowserContext(browser_context);
}

void WarningService::ClearWarnings(
    const std::set<Warning::WarningType>& types) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ExtensionIdSet affected_extensions;
  for (auto i = warnings_.begin(); i != warnings_.end();) {
    if (types.find(i->warning_type()) != types.end()) {
      affected_extensions.insert(i->extension_id());
      warnings_.erase(i++);
    } else {
      ++i;
    }
  }

  if (!affected_extensions.empty())
    NotifyWarningsChanged(affected_extensions);
}

std::set<Warning::WarningType>
WarningService::GetWarningTypesAffectingExtension(
    const ExtensionId& extension_id) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::set<Warning::WarningType> result;
  for (auto i = warnings_.cbegin(); i != warnings_.cend(); ++i) {
    if (i->extension_id() == extension_id)
      result.insert(i->warning_type());
  }
  return result;
}

std::vector<std::string> WarningService::GetWarningMessagesForExtension(
    const ExtensionId& extension_id) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::vector<std::string> result;

  const ExtensionSet& extension_set =
      ExtensionRegistry::Get(browser_context_)->enabled_extensions();

  for (auto i = warnings_.cbegin(); i != warnings_.cend(); ++i) {
    if (i->extension_id() == extension_id)
      result.push_back(i->GetLocalizedMessage(&extension_set));
  }
  return result;
}

void WarningService::AddWarnings(const WarningSet& warnings) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ExtensionIdSet affected_extensions;
  for (const Warning& warning : warnings) {
    if (warnings_.insert(warning).second)
      affected_extensions.insert(warning.extension_id());
  }
  if (!affected_extensions.empty())
    NotifyWarningsChanged(affected_extensions);
}

// static
void WarningService::NotifyWarningsOnUI(void* browser_context_id,
                                        const WarningSet& warnings) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!browser_context_id || !ExtensionsBrowserClient::Get() ||
      !ExtensionsBrowserClient::Get()->IsValidContext(browser_context_id)) {
    return;
  }

  content::BrowserContext* browser_context =
      reinterpret_cast<content::BrowserContext*>(browser_context_id);

  WarningService* warning_service = WarningService::Get(browser_context);

  warning_service->AddWarnings(warnings);
}

void WarningService::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void WarningService::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void WarningService::NotifyWarningsChanged(
    const ExtensionIdSet& affected_extensions) {
  for (auto& observer : observer_list_)
    observer.ExtensionWarningsChanged(affected_extensions);
}

void WarningService::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  // Unloading one extension might have solved the problems of others.
  // Therefore, we clear warnings of this type for all extensions.
  std::set<Warning::WarningType> warning_types =
      GetWarningTypesAffectingExtension(extension->id());
  ClearWarnings(warning_types);
}

}  // namespace extensions
