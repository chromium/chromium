// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/warning_service.h"

#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/warning_service_factory.h"
#include "extensions/common/extension_set.h"

using content::BrowserThread;

namespace extensions {

WarningService::WarningService(content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (browser_context_) {
    extension_registry_observer_.Add(ExtensionRegistry::Get(
        ExtensionsBrowserClient::Get()->GetOriginalContext(browser_context_)));
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

std::set<Warning::WarningType> WarningService::
    GetWarningTypesAffectingExtension(const std::string& extension_id) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::set<Warning::WarningType> result;
  for (auto i = warnings_.cbegin(); i != warnings_.cend(); ++i) {
    if (i->extension_id() == extension_id)
      result.insert(i->warning_type());
  }
  return result;
}

std::vector<std::string> WarningService::GetWarningMessagesForExtension(
    const std::string& extension_id) const {
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
void WarningService::NotifyWarningsOnUI(
    void* profile_id,
    const WarningSet& warnings) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::BrowserContext* browser_context =
      reinterpret_cast<content::BrowserContext*>(profile_id);

  if (!browser_context ||
      !ExtensionsBrowserClient::Get() ||
      !ExtensionsBrowserClient::Get()->IsValidContext(browser_context)) {
    return;
  }

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
