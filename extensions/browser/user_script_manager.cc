// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/user_script_manager.h"

#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/user_script_loader.h"
#include "extensions/common/manifest_handlers/content_scripts_handler.h"

namespace extensions {

UserScriptManager::UserScriptManager(content::BrowserContext* browser_context)
    : manifest_script_loader_(browser_context,
                              ExtensionId(),
                              true /* listen_for_extension_system_loaded */),
      browser_context_(browser_context) {
  extension_registry_observer_.Add(ExtensionRegistry::Get(browser_context_));
}

UserScriptManager::~UserScriptManager() = default;

UserScriptLoader* UserScriptManager::GetUserScriptLoaderByID(
    const HostID& host_id) {
  switch (host_id.type()) {
    case HostID::EXTENSIONS:
      return GetUserScriptLoaderForExtension(host_id.id());
    case HostID::WEBUI:
      return GetUserScriptLoaderForWebUI(GURL(host_id.id()));
  }
}

ExtensionUserScriptLoader* UserScriptManager::GetUserScriptLoaderForExtension(
    const ExtensionId& extension_id) {
  // TODO(crbug.com/1168627): This should be created when the extension loads.
  auto it = extension_script_loaders_.find(extension_id);
  return (it == extension_script_loaders_.end())
             ? CreateExtensionUserScriptLoader(extension_id)
             : it->second.get();
}

WebUIUserScriptLoader* UserScriptManager::GetUserScriptLoaderForWebUI(
    const GURL& url) {
  auto it = webui_script_loaders_.find(url);
  return (it == webui_script_loaders_.end()) ? CreateWebUIUserScriptLoader(url)
                                             : it->second.get();
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

  auto it = extension_script_loaders_.find(extension->id());
  if (it != extension_script_loaders_.end())
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

ExtensionUserScriptLoader* UserScriptManager::CreateExtensionUserScriptLoader(
    const ExtensionId& extension_id) {
  // Inserts a new ExtensionUserScriptLoader and returns a ptr to it.
  return extension_script_loaders_
      .emplace(extension_id,
               std::make_unique<ExtensionUserScriptLoader>(
                   browser_context_, extension_id,
                   false /* listen_for_extension_system_loaded */))
      .first->second.get();
}

WebUIUserScriptLoader* UserScriptManager::CreateWebUIUserScriptLoader(
    const GURL& url) {
  // Inserts a new WebUIUserScriptLoader and returns a ptr to it.
  return webui_script_loaders_
      .emplace(url,
               std::make_unique<WebUIUserScriptLoader>(browser_context_, url))
      .first->second.get();
}

}  // namespace extensions
