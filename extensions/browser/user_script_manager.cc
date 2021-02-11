// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/user_script_manager.h"

#include "content/public/browser/browser_context.h"
#include "extensions/browser/declarative_user_script_set.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/manifest_handlers/content_scripts_handler.h"

namespace extensions {

UserScriptManager::UserScriptManager(content::BrowserContext* browser_context)
    : manifest_script_loader_(browser_context,
                              HostID(),
                              true /* listen_for_extension_system_loaded */),
      browser_context_(browser_context) {
  extension_registry_observer_.Add(ExtensionRegistry::Get(browser_context_));
}

UserScriptManager::~UserScriptManager() = default;

DeclarativeUserScriptSet* UserScriptManager::GetDeclarativeUserScriptSetByID(
    const HostID& host_id) {
  auto it = declarative_user_script_sets_.find(host_id);

  if (it != declarative_user_script_sets_.end())
    return it->second.get();

  return CreateDeclarativeUserScriptSet(host_id);
}

void UserScriptManager::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  manifest_script_loader_.AddScripts(GetManifestScriptsMetadata(extension));
}

void UserScriptManager::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  const UserScriptList& script_list =
      ContentScriptsInfo::GetContentScripts(extension);
  std::set<UserScriptIDPair> scripts_to_remove;
  for (const std::unique_ptr<UserScript>& script : script_list)
    scripts_to_remove.insert(UserScriptIDPair(script->id(), script->host_id()));
  manifest_script_loader_.RemoveScripts(scripts_to_remove);

  auto it = declarative_user_script_sets_.find(
      HostID(HostID::EXTENSIONS, extension->id()));
  if (it != declarative_user_script_sets_.end())
    it->second->ClearScripts();
}

std::unique_ptr<UserScriptList> UserScriptManager::GetManifestScriptsMetadata(
    const Extension* extension) {
  bool incognito_enabled =
      util::IsIncognitoEnabled(extension->id(), browser_context_);
  const UserScriptList& script_list =
      ContentScriptsInfo::GetContentScripts(extension);
  auto script_vector = std::make_unique<UserScriptList>();
  script_vector->reserve(script_list.size());
  for (const auto& script : script_list) {
    std::unique_ptr<UserScript> script_copy =
        UserScript::CopyMetadataFrom(*script);
    script_copy->set_incognito_enabled(incognito_enabled);
    script_vector->push_back(std::move(script_copy));
  }
  return script_vector;
}

DeclarativeUserScriptSet* UserScriptManager::CreateDeclarativeUserScriptSet(
    const HostID& host_id) {
  // Inserts a new DeclarativeUserScriptSet and returns a ptr to it.
  return declarative_user_script_sets_
      .emplace(host_id, std::make_unique<DeclarativeUserScriptSet>(
                            browser_context_, host_id))
      .first->second.get();
}

}  // namespace extensions
