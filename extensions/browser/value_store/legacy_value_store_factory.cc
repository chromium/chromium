// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/value_store/legacy_value_store_factory.h"

#include <string>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "extensions/browser/value_store/leveldb_value_store.h"
#include "extensions/common/constants.h"

using base::AutoLock;

namespace {

// Statistics are logged to UMA with these strings as part of histogram name.
// They can all be found under Extensions.Database.Open.<client>. Changing this
// needs to synchronize with histograms.xml, AND will also become incompatible
// with older browsers still reporting the previous values.
const char kSettingsDatabaseUMAClientName[] = "Settings";
const char kRulesDatabaseUMAClientName[] = "Rules";
const char kStateDatabaseUMAClientName[] = "State";

bool ValidDBExists(const base::FilePath& path) {
  // TODO(cmumford): Enhance to detect if dir contains valid database.
  return base::DirectoryExists(path);
}

}  // namespace

namespace extensions {

//
// ModelSettings
//
LegacyValueStoreFactory::ModelSettings::ModelSettings(
    const base::FilePath& data_path)
    : data_path_(data_path) {}

base::FilePath LegacyValueStoreFactory::ModelSettings::GetDBPath(
    const ExtensionId& extension_id) const {
  return data_path_.AppendASCII(extension_id);
}

bool LegacyValueStoreFactory::ModelSettings::DeleteData(
    const ExtensionId& extension_id) {
  return base::DeleteFile(GetDBPath(extension_id), true /* recursive */);
}

bool LegacyValueStoreFactory::ModelSettings::DataExists(
    const ExtensionId& extension_id) const {
  return ValidDBExists(GetDBPath(extension_id));
}

std::set<ExtensionId>
LegacyValueStoreFactory::ModelSettings::GetKnownExtensionIDs() const {
  std::set<ExtensionId> result;

  // Leveldb databases are directories inside |base_path_|.
  base::FileEnumerator extension_dirs(data_path_, false,
                                      base::FileEnumerator::DIRECTORIES);
  while (!extension_dirs.Next().empty()) {
    base::FilePath extension_dir = extension_dirs.GetInfo().GetName();
    DCHECK(!extension_dir.IsAbsolute());
    // Extension ID's are 'a'..'p', so any directory within this folder will
    // either be ASCII, or created by some other application and safe to ignore.
    std::string maybe_as_ascii(extension_dir.MaybeAsASCII());
    if (!maybe_as_ascii.empty()) {
      result.insert(maybe_as_ascii);
    }
  }

  return result;
}

//
// SettingsRoot
//

LegacyValueStoreFactory::SettingsRoot::SettingsRoot(
    const base::FilePath& base_path,
    const std::string& extension_dirname,
    const std::string& app_dirname) {
  if (!extension_dirname.empty())
    extensions_.reset(
        new ModelSettings(base_path.AppendASCII(extension_dirname)));

  if (!app_dirname.empty())
    apps_.reset(new ModelSettings(base_path.AppendASCII(app_dirname)));
}

LegacyValueStoreFactory::SettingsRoot::~SettingsRoot() = default;

const LegacyValueStoreFactory::ModelSettings*
LegacyValueStoreFactory::SettingsRoot::GetModel(ModelType model_type) const {
  switch (model_type) {
    case ValueStoreFactory::ModelType::APP:
      DCHECK(apps_ != nullptr);
      return apps_.get();
    case ValueStoreFactory::ModelType::EXTENSION:
      DCHECK(extensions_ != nullptr);
      return extensions_.get();
  }
  NOTREACHED();
  return nullptr;
}

LegacyValueStoreFactory::ModelSettings*
LegacyValueStoreFactory::SettingsRoot::GetModel(ModelType model_type) {
  switch (model_type) {
    case ValueStoreFactory::ModelType::APP:
      DCHECK(apps_ != nullptr);
      return apps_.get();
    case ValueStoreFactory::ModelType::EXTENSION:
      DCHECK(extensions_ != nullptr);
      return extensions_.get();
  }
  NOTREACHED();
  return nullptr;
}

std::set<ExtensionId>
LegacyValueStoreFactory::SettingsRoot::GetKnownExtensionIDs(
    ModelType model_type) const {
  switch (model_type) {
    case ValueStoreFactory::ModelType::APP:
      DCHECK(apps_ != nullptr);
      return apps_->GetKnownExtensionIDs();
    case ValueStoreFactory::ModelType::EXTENSION:
      DCHECK(extensions_ != nullptr);
      return extensions_->GetKnownExtensionIDs();
  }
  NOTREACHED();
  return std::set<ExtensionId>();
}

//
// LegacyValueStoreFactory
//

LegacyValueStoreFactory::LegacyValueStoreFactory(
    const base::FilePath& profile_path)
    : profile_path_(profile_path),
      local_settings_(profile_path,
                      kLocalExtensionSettingsDirectoryName,
                      kLocalAppSettingsDirectoryName),
      sync_settings_(profile_path,
                     kSyncExtensionSettingsDirectoryName,
                     kSyncAppSettingsDirectoryName),
      // Currently no such thing as a managed app - only an extension.
      managed_settings_(profile_path, kManagedSettingsDirectoryName, "") {}

LegacyValueStoreFactory::~LegacyValueStoreFactory() = default;

bool LegacyValueStoreFactory::RulesDBExists() const {
  return ValidDBExists(GetRulesDBPath());
}

bool LegacyValueStoreFactory::StateDBExists() const {
  return ValidDBExists(GetStateDBPath());
}

std::unique_ptr<ValueStore> LegacyValueStoreFactory::CreateRulesStore() {
  return std::make_unique<LeveldbValueStore>(kRulesDatabaseUMAClientName,
                                             GetRulesDBPath());
}

std::unique_ptr<ValueStore> LegacyValueStoreFactory::CreateStateStore() {
  return std::make_unique<LeveldbValueStore>(kStateDatabaseUMAClientName,
                                             GetStateDBPath());
}

std::unique_ptr<ValueStore> LegacyValueStoreFactory::CreateSettingsStore(
    settings_namespace::Namespace settings_namespace,
    ModelType model_type,
    const ExtensionId& extension_id) {
  const ModelSettings* settings_root =
      GetSettingsRoot(settings_namespace).GetModel(model_type);
  DCHECK(settings_root != nullptr);
  return std::make_unique<LeveldbValueStore>(
      kSettingsDatabaseUMAClientName, settings_root->GetDBPath(extension_id));
}

void LegacyValueStoreFactory::DeleteSettings(
    settings_namespace::Namespace settings_namespace,
    ModelType model_type,
    const ExtensionId& extension_id) {
  ModelSettings* model_settings =
      GetSettingsRoot(settings_namespace).GetModel(model_type);
  if (model_settings == nullptr) {
    NOTREACHED();
    return;
  }
  model_settings->DeleteData(extension_id);
}

bool LegacyValueStoreFactory::HasSettings(
    settings_namespace::Namespace settings_namespace,
    ModelType model_type,
    const ExtensionId& extension_id) {
  const ModelSettings* model_settings =
      GetSettingsRoot(settings_namespace).GetModel(model_type);
  if (model_settings == nullptr)
    return false;
  return model_settings->DataExists(extension_id);
}

std::set<ExtensionId> LegacyValueStoreFactory::GetKnownExtensionIDs(
    settings_namespace::Namespace settings_type,
    ModelType model_type) const {
  return GetSettingsRoot(settings_type).GetKnownExtensionIDs(model_type);
}

const LegacyValueStoreFactory::SettingsRoot&
LegacyValueStoreFactory::GetSettingsRoot(
    settings_namespace::Namespace settings_namespace) const {
  switch (settings_namespace) {
    case settings_namespace::LOCAL:
      return local_settings_;
    case settings_namespace::SYNC:
      return sync_settings_;
    case settings_namespace::MANAGED:
      return managed_settings_;
    case settings_namespace::INVALID:
      break;
  }
  NOTREACHED();
  return local_settings_;
}

LegacyValueStoreFactory::SettingsRoot& LegacyValueStoreFactory::GetSettingsRoot(
    settings_namespace::Namespace settings_namespace) {
  switch (settings_namespace) {
    case settings_namespace::LOCAL:
      return local_settings_;
    case settings_namespace::SYNC:
      return sync_settings_;
    case settings_namespace::MANAGED:
      return managed_settings_;
    case settings_namespace::INVALID:
      break;
  }
  NOTREACHED();
  return local_settings_;
}

base::FilePath LegacyValueStoreFactory::GetRulesDBPath() const {
  return profile_path_.AppendASCII(kRulesStoreName);
}

base::FilePath LegacyValueStoreFactory::GetStateDBPath() const {
  return profile_path_.AppendASCII(kStateStoreName);
}

}  // namespace extensions
