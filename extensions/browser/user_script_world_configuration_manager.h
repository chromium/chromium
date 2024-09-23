// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_USER_SCRIPT_WORLD_CONFIGURATION_MANAGER_H_
#define EXTENSIONS_BROWSER_USER_SCRIPT_WORLD_CONFIGURATION_MANAGER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/renderer.mojom.h"

class BrowserContextKeyedServiceFactory;

namespace content {
class BrowserContext;
}  // content

namespace extensions {
class Extension;
class ExtensionPrefs;
class RendererStartupHelper;

// A class to manage the configurations for extensions' isolated worlds. The
// configurations themselves are stored in the associated ExtensionPrefs.
// Note: Like the ExtensionPrefs themselves, this class is shared between
// on- and off-the-record contexts.
class UserScriptWorldConfigurationManager
    : public KeyedService,
      public ExtensionRegistryObserver {
 public:
  explicit UserScriptWorldConfigurationManager(
      content::BrowserContext* browser_context);
  UserScriptWorldConfigurationManager(
      const UserScriptWorldConfigurationManager&) = delete;
  UserScriptWorldConfigurationManager& operator=(
      const UserScriptWorldConfigurationManager&) = delete;
  ~UserScriptWorldConfigurationManager() override;

  // Set a configuration for a user script world indicated by `world_id`. If
  // `world_id` is omitted, sets the configuration for the default user script
  // world.
  void SetUserScriptWorldInfo(const Extension& extension,
                              const std::optional<std::string>& world_id,
                              std::optional<std::string> csp,
                              bool enable_messaging);

  // Clears any stored configuration for a user script world indicated by
  // `world_id`. If `world_id` is omitted, removes the configuration for the
  // default user script world.
  void ClearUserScriptWorldInfo(const Extension& extension,
                                const std::optional<std::string>& world_id);

  // Retrieve a configuration for a user script world indicated by `world_id`.
  // If `world_id` is omitted, retrieves the configuration for the default user
  // script world. If there is no registration for the given user script world,
  // returns the default configuration for user script worlds.
  mojom::UserScriptWorldInfoPtr GetUserScriptWorldInfo(
      const ExtensionId& extension_id,
      const std::optional<std::string>& world_id);

  // Retrieves all registered configurations of user script worlds for the
  // given `extension_id`.
  std::vector<mojom::UserScriptWorldInfoPtr> GetAllUserScriptWorlds(
      const ExtensionId& extension_id);

  // KeyedService related bits:
  static BrowserContextKeyedServiceFactory& GetFactory();
  static UserScriptWorldConfigurationManager* Get(
      content::BrowserContext* browser_context);

 private:
  // ExtensionRegistryObserver:
  void OnExtensionWillBeInstalled(
      content::BrowserContext* browser_context,
      const Extension* extension,
      bool is_update,
      const std::string& old_name) override;

  // The ExtensionPrefs to use when setting or retrieving isolated world
  // configurations. Safe to use because this KeyedService depends on
  // ExtensionPrefs.
  // Always safe to use because this depends on it as a KeyedService.
  raw_ptr<ExtensionPrefs> extension_prefs_;

  // The RendererStartupHelper to notify when when world configurations have
  // changed.
  // Note: The RendererStartupHelper is also a shared instance between
  // on- and off-the-record profiles. This is important, because it means this
  // is always the accurate object to send updates through.
  // Always safe to use because this depends on it as a KeyedService.
  raw_ptr<RendererStartupHelper> renderer_helper_;

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      registry_observation_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_USER_SCRIPT_WORLD_CONFIGURATION_MANAGER_H_
