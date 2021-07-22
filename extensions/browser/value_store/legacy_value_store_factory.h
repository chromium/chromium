// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_VALUE_STORE_LEGACY_VALUE_STORE_FACTORY_H_
#define EXTENSIONS_BROWSER_VALUE_STORE_LEGACY_VALUE_STORE_FACTORY_H_

#include <memory>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "extensions/browser/value_store/value_store.h"
#include "extensions/browser/value_store/value_store_client_id.h"
#include "extensions/browser/value_store/value_store_factory.h"

namespace extensions {

// A factory to create legacy ValueStore instances for storing extension
// state/rules/settings. "legacy" refers to the initial storage implementation
// which created a settings database per extension. This factory was created as
// part of a refactoring for crbug.com/453946 which was never finished. Because
// it was never finished, this is the only implementation of ValueStoreFactory.
// TODO(crbug.com/453946): Finish this refactoring or merge
// legacy_value_store_factory with value_store_factory_impl.
class LegacyValueStoreFactory : public ValueStoreFactory {
 public:
  explicit LegacyValueStoreFactory(const base::FilePath& profile_path);
  LegacyValueStoreFactory(const LegacyValueStoreFactory&) = delete;
  LegacyValueStoreFactory& operator=(const LegacyValueStoreFactory&) = delete;

  bool RulesDBExists() const;
  bool StateDBExists() const;

  // ValueStoreFactory:
  std::unique_ptr<ValueStore> CreateRulesStore() override;
  std::unique_ptr<ValueStore> CreateStateStore() override;
  std::unique_ptr<ValueStore> CreateSettingsStore(
      settings_namespace::Namespace settings_namespace,
      ModelType model_type,
      const ValueStoreClientId& id) override;

  void DeleteSettings(settings_namespace::Namespace settings_namespace,
                      ModelType model_type,
                      const ValueStoreClientId& id) override;
  bool HasSettings(settings_namespace::Namespace settings_namespace,
                   ModelType model_type,
                   const ValueStoreClientId& id) override;
  std::set<ValueStoreClientId> GetKnownExtensionIDs(
      settings_namespace::Namespace settings_namespace,
      ModelType model_type) const override;

 private:
  friend class base::RefCounted<LegacyValueStoreFactory>;

  // Manages a collection of legacy settings databases all within a common
  // directory.
  class ModelSettings {
   public:
    explicit ModelSettings(const base::FilePath& data_path);
    ModelSettings(const ModelSettings&) = delete;
    ModelSettings& operator=(const ModelSettings&) = delete;

    base::FilePath GetDBPath(const ValueStoreClientId& id) const;
    bool DeleteData(const ValueStoreClientId& id);
    bool DataExists(const ValueStoreClientId& id) const;
    std::set<ValueStoreClientId> GetKnownExtensionIDs() const;

   private:
    // The path containing all settings databases under this root.
    const base::FilePath data_path_;
  };

  // Manages two collections of legacy settings databases (apps & extensions)
  // within a common base directory.
  class SettingsRoot {
   public:
    // If either |extension_dirname| or |app_dirname| are empty then that
    // ModelSetting will *not* be created.
    SettingsRoot(const base::FilePath& base_path,
                 const std::string& extension_dirname,
                 const std::string& app_dirname);
    ~SettingsRoot();
    SettingsRoot(const SettingsRoot&) = delete;
    SettingsRoot& operator=(const SettingsRoot&) = delete;

    std::set<ValueStoreClientId> GetKnownExtensionIDs(
        ModelType model_type) const;
    const ModelSettings* GetModel(ModelType model_type) const;
    ModelSettings* GetModel(ModelType model_type);

   private:
    // TODO(crbug.com/1226956): Remove references to extensions and Chrome Apps.
    std::unique_ptr<ModelSettings> extensions_;
    std::unique_ptr<ModelSettings> apps_;
  };

  ~LegacyValueStoreFactory() override;

  const SettingsRoot& GetSettingsRoot(
      settings_namespace::Namespace settings_namespace) const;
  SettingsRoot& GetSettingsRoot(
      settings_namespace::Namespace settings_namespace);

  base::FilePath GetRulesDBPath() const;
  base::FilePath GetStateDBPath() const;

  const base::FilePath profile_path_;
  SettingsRoot local_settings_;
  SettingsRoot sync_settings_;
  SettingsRoot managed_settings_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_VALUE_STORE_LEGACY_VALUE_STORE_FACTORY_H_
