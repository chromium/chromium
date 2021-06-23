// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/user_script_manager.h"

#include "base/containers/contains.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/user_script_loader.h"
#include "extensions/common/manifest_handlers/content_scripts_handler.h"
#include "extensions/common/mojom/host_id.mojom.h"

namespace extensions {

UserScriptManager::UserScriptManager(content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  extension_registry_observation_.Observe(
      ExtensionRegistry::Get(browser_context_));
}

UserScriptManager::~UserScriptManager() = default;

UserScriptLoader* UserScriptManager::GetUserScriptLoaderByID(
    const mojom::HostID& host_id) {
  switch (host_id.type) {
    case mojom::HostID::HostType::kExtensions:
      return GetUserScriptLoaderForExtension(host_id.id);
    case mojom::HostID::HostType::kWebUi:
      return GetUserScriptLoaderForWebUI(GURL(host_id.id));
  }
}

ExtensionUserScriptLoader* UserScriptManager::GetUserScriptLoaderForExtension(
    const ExtensionId& extension_id) {
  const Extension* extension = ExtensionRegistry::Get(browser_context_)
                                   ->enabled_extensions()
                                   .GetByID(extension_id);
  DCHECK(extension);

  auto it = extension_script_loaders_.find(extension->id());
  return (it == extension_script_loaders_.end())
             ? CreateExtensionUserScriptLoader(extension)
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
  ExtensionUserScriptLoader* loader =
      GetUserScriptLoaderForExtension(extension->id());

  std::unique_ptr<UserScriptList> scripts =
      GetManifestScriptsMetadata(extension);

  // Don't bother adding scripts if this extension has none because adding an
  // empty set of scripts will not trigger a load. This also prevents redundant
  // calls to OnInitialExtensionLoadComplete.
  if (!scripts->empty()) {
    pending_manifest_load_count_++;
    loader->AddScripts(
        std::move(scripts),
        base::BindOnce(&UserScriptManager::OnInitialExtensionLoadComplete,
                       weak_factory_.GetWeakPtr()));
  }
}

void UserScriptManager::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  // The renderer will clean up its scripts from an IPC message which is sent
  // when the extension is unloaded. All we need to do here is to remove the
  // unloaded extension's loader.
  extension_script_loaders_.erase(extension->id());
}

void UserScriptManager::OnInitialExtensionLoadComplete(
    UserScriptLoader* loader,
    const absl::optional<std::string>& error) {
  --pending_manifest_load_count_;
  DCHECK_GE(pending_manifest_load_count_, 0);

  // If there are no more pending manifest script loads, then notify the
  // UserScriptListener.
  if (pending_manifest_load_count_ == 0) {
    DCHECK(ExtensionsBrowserClient::Get());
    ExtensionsBrowserClient::Get()->SignalContentScriptsLoaded(
        browser_context_);
  }
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
    const Extension* extension) {
  DCHECK(!base::Contains(extension_script_loaders_, extension->id()));
  // Inserts a new ExtensionUserScriptLoader and returns a ptr to it.
  ExtensionUserScriptLoader* loader =
      extension_script_loaders_
          .emplace(extension->id(),
                   std::make_unique<ExtensionUserScriptLoader>(
                       browser_context_, *extension,
                       /*listen_for_extension_system_loaded=*/true))
          .first->second.get();

  return loader;
}

WebUIUserScriptLoader* UserScriptManager::CreateWebUIUserScriptLoader(
    const GURL& url) {
  DCHECK(!base::Contains(webui_script_loaders_, url));
  // Inserts a new WebUIUserScriptLoader and returns a ptr to it.
  WebUIUserScriptLoader* loader =
      webui_script_loaders_
          .emplace(url, std::make_unique<WebUIUserScriptLoader>(
                            browser_context_, url))
          .first->second.get();

  return loader;
}

}  // namespace extensions
