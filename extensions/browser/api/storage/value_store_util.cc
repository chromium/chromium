// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/storage/value_store_util.h"

#include "base/files/file_path.h"
#include "base/notreached.h"
#include "components/value_store/value_store_factory.h"
#include "extensions/browser/api/storage/settings_namespace.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_id.h"

namespace extensions {

namespace value_store_util {

base::FilePath GetValueStoreDir(
    settings_namespace::Namespace settings_namespace,
    ModelType model_type,
    const ExtensionId& id) {
  base::FilePath dir;
  switch (settings_namespace) {
    case settings_namespace::LOCAL:
      dir = model_type == ModelType::APP
                ? base::FilePath(kLocalAppSettingsDirectoryName)
                : base::FilePath(kLocalExtensionSettingsDirectoryName);
      break;
    case settings_namespace::SYNC:
      dir = model_type == ModelType::APP
                ? base::FilePath(kSyncAppSettingsDirectoryName)
                : base::FilePath(kSyncExtensionSettingsDirectoryName);
      break;
    case settings_namespace::MANAGED:
      // Currently no such thing as a managed app - only an extension.
      dir = base::FilePath(kManagedSettingsDirectoryName);
      break;
    case settings_namespace::INVALID:
      NOTREACHED_IN_MIGRATION();
  }
  return dir.AppendASCII(id);
}

std::unique_ptr<value_store::ValueStore> CreateSettingsStore(
    settings_namespace::Namespace settings_namespace,
    ModelType model_type,
    const ExtensionId& id,
    scoped_refptr<value_store::ValueStoreFactory> factory) {
  base::FilePath directory =
      GetValueStoreDir(settings_namespace, model_type, id);
  return factory->CreateValueStore(directory, kSettingsDatabaseUMAClientName);
}

void DeleteValueStore(settings_namespace::Namespace settings_namespace,
                      ModelType model_type,
                      const ExtensionId& id,
                      scoped_refptr<value_store::ValueStoreFactory> factory) {
  base::FilePath directory =
      GetValueStoreDir(settings_namespace, model_type, id);
  factory->DeleteValueStore(directory);
}

bool HasValueStore(settings_namespace::Namespace settings_namespace,
                   ModelType model_type,
                   const ExtensionId& id,
                   scoped_refptr<value_store::ValueStoreFactory> factory) {
  base::FilePath directory =
      GetValueStoreDir(settings_namespace, model_type, id);
  return factory->HasValueStore(directory);
}

}  // namespace value_store_util
}  // namespace extensions
