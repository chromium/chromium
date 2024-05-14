// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/storage/settings_test_util.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/values.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/storage/storage_frontend.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"

namespace extensions {

namespace settings_test_util {

// Creates a kilobyte of data.
base::Value CreateKilobyte() {
  std::string kilobyte_string(1024u, 'a');
  return base::Value(std::move(kilobyte_string));
}

// Creates a megabyte of data.
base::Value CreateMegabyte() {
  base::Value::List megabyte;
  for (int i = 0; i < 1000; ++i) {
    megabyte.Append(CreateKilobyte());
  }
  return base::Value(std::move(megabyte));
}

// Intended as a StorageCallback from GetStorage.
static void AssignStorage(value_store::ValueStore** dst,
                          value_store::ValueStore* src) {
  *dst = src;
}

value_store::ValueStore* GetStorage(
    scoped_refptr<const Extension> extension,
    settings_namespace::Namespace settings_namespace,
    StorageFrontend* frontend) {
  value_store::ValueStore* storage = nullptr;
  frontend->RunWithStorage(extension, settings_namespace,
                           base::BindOnce(&AssignStorage, &storage));
  content::RunAllTasksUntilIdle();
  return storage;
}

value_store::ValueStore* GetStorage(scoped_refptr<const Extension> extension,
                                    StorageFrontend* frontend) {
  return GetStorage(extension, settings_namespace::SYNC, frontend);
}

scoped_refptr<const Extension> AddExtensionWithId(
    content::BrowserContext* context,
    const std::string& id,
    Manifest::Type type) {
  return AddExtensionWithIdAndPermissions(
      context, id, type, std::set<std::string>());
}

scoped_refptr<const Extension> AddExtensionWithIdAndPermissions(
    content::BrowserContext* context,
    const std::string& id,
    Manifest::Type type,
    const std::set<std::string>& permissions_set) {
  auto manifest =
      base::Value::Dict().Set("name", std::string("Test extension ") + id);
  manifest.Set("version", "1.0");
  manifest.Set("manifest_version", 2);

  base::Value::List permissions;
  for (const auto& perm : permissions_set)
    permissions.Append(perm);
  manifest.Set("permissions", std::move(permissions));

  switch (type) {
    case Manifest::TYPE_EXTENSION:
      break;

    case Manifest::TYPE_LEGACY_PACKAGED_APP: {
      base::Value::Dict app;
      base::Value::Dict app_launch;
      app_launch.Set("local_path", "fake.html");
      app.Set("launch", std::move(app_launch));
      manifest.Set("app", std::move(app));
      break;
    }

    default:
      NOTREACHED_IN_MIGRATION();
  }

  std::string error;
  scoped_refptr<const Extension> extension(
      Extension::Create(base::FilePath(), mojom::ManifestLocation::kInternal,
                        manifest, Extension::NO_FLAGS, id, &error));
  DCHECK(extension.get());
  DCHECK(error.empty());

  // Ensure lookups via ExtensionRegistry (and ExtensionService) work even if
  // the test discards the referenced to the returned extension.
  ExtensionRegistry::Get(context)->AddEnabled(extension);

  for (auto it = permissions_set.cbegin(); it != permissions_set.cend(); ++it) {
    DCHECK(extension->permissions_data()->HasAPIPermission(*it));
  }

  return extension;
}

}  // namespace settings_test_util

}  // namespace extensions
