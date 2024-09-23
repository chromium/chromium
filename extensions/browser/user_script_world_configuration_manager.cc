// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/user_script_world_configuration_manager.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/renderer_startup_helper.h"

namespace extensions {

namespace {

constexpr char kDefaultUserScriptWorldKey[] = "_default";
constexpr char kUserScriptWorldMessagingKey[] = "messaging";
constexpr char kUserScriptWorldCspKey[] = "csp";

class UserScriptWorldConfigurationManagerFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  UserScriptWorldConfigurationManagerFactory()
      : BrowserContextKeyedServiceFactory(
            "UserScriptWorldConfigurationManager",
            BrowserContextDependencyManager::GetInstance()) {
    DependsOn(ExtensionPrefsFactory::GetInstance());
    DependsOn(ExtensionRegistryFactory::GetInstance());
    DependsOn(RendererStartupHelperFactory::GetInstance());
  }

  UserScriptWorldConfigurationManagerFactory(
      const UserScriptWorldConfigurationManagerFactory&) = delete;
  UserScriptWorldConfigurationManagerFactory& operator=(
      const UserScriptWorldConfigurationManagerFactory&) = delete;
  ~UserScriptWorldConfigurationManagerFactory() override = default;

  UserScriptWorldConfigurationManager* GetForBrowserContext(
      content::BrowserContext* context) {
    return static_cast<UserScriptWorldConfigurationManager*>(
        GetServiceForBrowserContext(context, /*create=*/true));
  }

 private:
  // BrowserContextKeyedServiceFactory:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override {
    // TODO(devlin): I wonder if it would make sense for this to have its own
    // instance in incognito. That would allow split-mode extensions to have
    // incognito-only world specifications and have them cleaned up when the
    // profile is destroyed.
    return ExtensionsBrowserClient::Get()->GetContextRedirectedToOriginal(
        context, /*force_guest_profile=*/true);
  }
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override {
    return new UserScriptWorldConfigurationManager(context);
  }
};

// Returns the key entry in the user script world dictionary to use for the
// provided `world_id`.
std::string_view GetUserScriptWorldKeyForWorldId(
    const std::optional<std::string>& world_id) {
  return world_id ? world_id->c_str() : kDefaultUserScriptWorldKey;
}

// Attempts to parse the given `dict` into a mojom::UserScriptWorldInfo. If
// `dict` is null, returns a default specification for a
// mojom::UserScriptWorldInfo.
mojom::UserScriptWorldInfoPtr ParseUserScriptWorldInfo(
    const ExtensionId& extension_id,
    const std::optional<std::string>& world_id,
    const base::Value::Dict* dict) {
  bool enable_messaging = false;
  std::optional<std::string> csp;
  if (dict) {
    enable_messaging =
        dict->FindBool(kUserScriptWorldMessagingKey).value_or(false);
    const std::string* csp_pref = dict->FindString(kUserScriptWorldCspKey);
    csp = csp_pref ? std::make_optional(*csp_pref) : std::nullopt;
  }

  return mojom::UserScriptWorldInfo::New(extension_id, world_id, csp,
                                         enable_messaging);
}

}  // namespace

UserScriptWorldConfigurationManager::UserScriptWorldConfigurationManager(
    content::BrowserContext* browser_context)
    : extension_prefs_(ExtensionPrefs::Get(browser_context)),
      renderer_helper_(
          RendererStartupHelperFactory::GetForBrowserContext(browser_context)) {
  registry_observation_.Observe(ExtensionRegistry::Get(browser_context));
}

UserScriptWorldConfigurationManager::~UserScriptWorldConfigurationManager() =
    default;

void UserScriptWorldConfigurationManager::SetUserScriptWorldInfo(
    const Extension& extension,
    const std::optional<std::string>& world_id,
    std::optional<std::string> csp,
    bool enable_messaging) {
  // Persist world configuratation in ExtensionPrefs.
  ExtensionPrefs::ScopedDictionaryUpdate update(
      extension_prefs_, extension.id(), kUserScriptsWorldsConfiguration.name);
  std::unique_ptr<prefs::DictionaryValueUpdate> update_dict = update.Get();
  if (!update_dict) {
    update_dict = update.Create();
  }

  base::Value::Dict world_info;
  world_info.Set(kUserScriptWorldMessagingKey, enable_messaging);
  if (csp.has_value()) {
    world_info.Set(kUserScriptWorldCspKey, *csp);
  }

  update_dict->SetKey(GetUserScriptWorldKeyForWorldId(world_id),
                      base::Value(std::move(world_info)));

  renderer_helper_->SetUserScriptWorldProperties(extension, world_id, csp,
                                                 enable_messaging);
}

void UserScriptWorldConfigurationManager::ClearUserScriptWorldInfo(
    const Extension& extension,
    const std::optional<std::string>& world_id) {
  ExtensionPrefs::ScopedDictionaryUpdate update(
      extension_prefs_, extension.id(), kUserScriptsWorldsConfiguration.name);
  std::unique_ptr<prefs::DictionaryValueUpdate> update_dict = update.Get();

  if (!update_dict) {
    return;  // No configs.
  }

  std::string_view world_key = GetUserScriptWorldKeyForWorldId(world_id);
  if (!update_dict->HasKey(world_key)) {
    return;  // No config for this world ID.
  }

  update_dict->Remove(world_key);
  renderer_helper_->ClearUserScriptWorldProperties(extension, world_id);
}

mojom::UserScriptWorldInfoPtr
UserScriptWorldConfigurationManager::GetUserScriptWorldInfo(
    const ExtensionId& extension_id,
    const std::optional<std::string>& world_id) {
  const base::Value::Dict* worlds_configuration =
      extension_prefs_->ReadPrefAsDictionary(extension_id,
                                             kUserScriptsWorldsConfiguration);
  const base::Value::Dict* world_info =
      worlds_configuration ? worlds_configuration->FindDict(
                                 GetUserScriptWorldKeyForWorldId(world_id))
                           : nullptr;

  return ParseUserScriptWorldInfo(extension_id, world_id, world_info);
}

std::vector<mojom::UserScriptWorldInfoPtr>
UserScriptWorldConfigurationManager::GetAllUserScriptWorlds(
    const ExtensionId& extension_id) {
  const base::Value::Dict* worlds_configuration =
      extension_prefs_->ReadPrefAsDictionary(extension_id,
                                             kUserScriptsWorldsConfiguration);
  if (!worlds_configuration) {
    return {};
  }

  std::vector<mojom::UserScriptWorldInfoPtr> result;
  for (auto [world_id_key, world_value] : *worlds_configuration) {
    if (world_id_key.length() < 1) {
      continue;  // Invalid key. Ignore.
    }

    if (!world_value.is_dict()) {
      continue;  // Invalid value. Ignore.
    }

    std::optional<std::string> world_id;
    if (world_id_key[0] == '_') {
      if (world_id_key != kDefaultUserScriptWorldKey) {
        continue;  // Invalid key. Ignore.
      }
    } else {
      // Otherwise, the world ID is the key in the dictionary.
      world_id = world_id_key;
    }

    mojom::UserScriptWorldInfoPtr parsed_world = ParseUserScriptWorldInfo(
        extension_id, world_id, &world_value.GetDict());
    if (!parsed_world) {
      continue;  // Failed to parse. Ignore.
    }

    result.push_back(std::move(parsed_world));
  }

  return result;
}

// static
BrowserContextKeyedServiceFactory&
UserScriptWorldConfigurationManager::GetFactory() {
  static base::NoDestructor<UserScriptWorldConfigurationManagerFactory>
      g_factory;
  return *g_factory;
}

// static
UserScriptWorldConfigurationManager* UserScriptWorldConfigurationManager::Get(
    content::BrowserContext* browser_context) {
  auto& factory =
      static_cast<UserScriptWorldConfigurationManagerFactory&>(GetFactory());
  return factory.GetForBrowserContext(browser_context);
}

void UserScriptWorldConfigurationManager::OnExtensionWillBeInstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    bool is_update,
    const std::string& old_name) {
  if (!is_update) {
    return;
  }

  extension_prefs_->UpdateExtensionPref(
      extension->id(), kUserScriptsWorldsConfiguration.name, std::nullopt);
}

}  // namespace extensions
