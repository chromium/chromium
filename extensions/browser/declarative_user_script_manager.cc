// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/declarative_user_script_manager.h"

#include "content/public/browser/browser_context.h"
#include "extensions/browser/declarative_user_script_manager_factory.h"
#include "extensions/browser/declarative_user_script_master.h"

namespace extensions {

DeclarativeUserScriptManager::DeclarativeUserScriptManager(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  extension_registry_observer_.Add(ExtensionRegistry::Get(browser_context));
}

DeclarativeUserScriptManager::~DeclarativeUserScriptManager() {
}

// static
DeclarativeUserScriptManager* DeclarativeUserScriptManager::Get(
    content::BrowserContext* browser_context) {
  return DeclarativeUserScriptManagerFactory::GetForBrowserContext(
      browser_context);
}

DeclarativeUserScriptMaster*
DeclarativeUserScriptManager::GetDeclarativeUserScriptMasterByID(
    const HostID& host_id) {
  auto it = declarative_user_script_masters_.find(host_id);

  if (it != declarative_user_script_masters_.end())
    return it->second.get();

  return CreateDeclarativeUserScriptMaster(host_id);
}

DeclarativeUserScriptMaster*
DeclarativeUserScriptManager::CreateDeclarativeUserScriptMaster(
    const HostID& host_id) {
  // Inserts a new DeclarativeUserScriptManager and returns a ptr to it.
  return declarative_user_script_masters_
      .insert(
          std::make_pair(host_id, std::make_unique<DeclarativeUserScriptMaster>(
                                      browser_context_, host_id)))
      .first->second.get();
}

void DeclarativeUserScriptManager::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  for (const auto& val : declarative_user_script_masters_) {
    DeclarativeUserScriptMaster* master = val.second.get();
    if (master->host_id().id() == extension->id())
      master->ClearScripts();
  }
}

}  // namespace extensions
