// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/user_script_manager.h"

#include "base/containers/contains.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/scripting_constants.h"
#include "extensions/browser/scripting_utils.h"
#include "extensions/browser/state_store.h"
#include "extensions/browser/user_script_loader.h"
#include "extensions/common/api/content_scripts.h"
#include "extensions/common/features/feature_developer_mode_only.h"
#include "extensions/common/manifest_handlers/content_scripts_handler.h"
#include "extensions/common/mojom/host_id.mojom.h"
#include "extensions/common/mojom/run_location.mojom-shared.h"
#include "extensions/common/utils/content_script_utils.h"

namespace extensions {

UserScriptManager::UserScriptManager(content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  extension_registry_observation_.Observe(
      ExtensionRegistry::Get(browser_context_));

  StateStore* store =
      ExtensionSystem::Get(browser_context_)->dynamic_user_scripts_store();
  if (store) {
    store->RegisterKey(scripting::kRegisteredScriptsStorageKey);
  }
}

UserScriptManager::~UserScriptManager() = default;

UserScriptLoader* UserScriptManager::GetUserScriptLoaderByID(
    const mojom::HostID& host_id) {
  switch (host_id.type) {
    case mojom::HostID::HostType::kExtensions:
      return GetUserScriptLoaderForExtension(host_id.id);
    case mojom::HostID::HostType::kControlledFrameEmbedder:
    case mojom::HostID::HostType::kWebUi:
      return GetUserScriptLoaderForEmbedder(host_id);
  }
}

ExtensionUserScriptLoader* UserScriptManager::GetUserScriptLoaderForExtension(
    const ExtensionId& extension_id) {
  const Extension* extension = ExtensionRegistry::Get(browser_context_)
                                   ->enabled_extensions()
                                   .GetByID(extension_id);
  CHECK(extension);

  auto it = extension_script_loaders_.find(extension->id());
  return (it == extension_script_loaders_.end())
             ? CreateExtensionUserScriptLoader(extension)
             : it->second.get();
}

EmbedderUserScriptLoader* UserScriptManager::GetUserScriptLoaderForEmbedder(
    const mojom::HostID& host_id) {
  auto it = embedder_script_loaders_.find(host_id);
  if (it != embedder_script_loaders_.end()) {
    return it->second.get();
  }

  switch (host_id.type) {
    case mojom::HostID::HostType::kControlledFrameEmbedder:
    case mojom::HostID::HostType::kWebUi:
      return CreateEmbedderUserScriptLoader(host_id);
    case mojom::HostID::HostType::kExtensions:
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

void UserScriptManager::SetUserScriptSourceEnabledForExtensions(
    UserScript::Source source,
    bool enabled) {
  for (auto& map_entry : extension_script_loaders_) {
    map_entry.second->SetSourceEnabled(source, enabled);
  }
}

void UserScriptManager::OnExtensionWillBeInstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    bool is_update,
    const std::string& old_name) {
  scripting::ClearPersistentScriptURLPatterns(browser_context, extension->id());
}

void UserScriptManager::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  ExtensionUserScriptLoader* loader =
      GetUserScriptLoaderForExtension(extension->id());

  if (loader->AddScriptsForExtensionLoad(
          *extension,
          base::BindOnce(&UserScriptManager::OnInitialExtensionLoadComplete,
                         weak_factory_.GetWeakPtr()))) {
    pending_initial_extension_loads_.insert(extension->id());
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
  RemovePendingExtensionLoadAndSignal(extension->id());
}

void UserScriptManager::OnInitialExtensionLoadComplete(
    UserScriptLoader* loader,
    const std::optional<std::string>& error) {
  RemovePendingExtensionLoadAndSignal(loader->host_id().id);
}

void UserScriptManager::RemovePendingExtensionLoadAndSignal(
    const ExtensionId& extension_id) {
  int erased = pending_initial_extension_loads_.erase(extension_id);
  if (!erased || !pending_initial_extension_loads_.empty()) {
    return;  // Not a relevant extension, or still waiting on more.
  }

  // All our extensions are loaded!
  ExtensionsBrowserClient::Get()->SignalContentScriptsLoaded(browser_context_);
}

ExtensionUserScriptLoader* UserScriptManager::CreateExtensionUserScriptLoader(
    const Extension* extension) {
  CHECK(!base::Contains(extension_script_loaders_, extension->id()));
  // Inserts a new ExtensionUserScriptLoader and returns a ptr to it.
  ExtensionUserScriptLoader* loader =
      extension_script_loaders_
          .emplace(extension->id(),
                   std::make_unique<ExtensionUserScriptLoader>(
                       browser_context_, *extension,
                       ExtensionSystem::Get(browser_context_)
                           ->dynamic_user_scripts_store(),
                       /*listen_for_extension_system_loaded=*/true))
          .first->second.get();
  loader->SetSourceEnabled(
      UserScript::Source::kDynamicUserScript,
      GetCurrentDeveloperMode(util::GetBrowserContextId(browser_context_)));

  return loader;
}

EmbedderUserScriptLoader* UserScriptManager::CreateEmbedderUserScriptLoader(
    const mojom::HostID& host_id) {
  CHECK(!base::Contains(embedder_script_loaders_, host_id));
  // Inserts a new EmbedderUserScriptLoader and returns a ptr to it.
  EmbedderUserScriptLoader* loader =
      embedder_script_loaders_
          .emplace(host_id, std::make_unique<EmbedderUserScriptLoader>(
                                browser_context_, host_id))
          .first->second.get();

  return loader;
}

}  // namespace extensions
