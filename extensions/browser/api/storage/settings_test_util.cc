// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/storage/settings_test_util.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
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
std::unique_ptr<base::Value> CreateKilobyte() {
  std::string kilobyte_string(1024u, 'a');
  return std::unique_ptr<base::Value>(
      new base::Value(std::move(kilobyte_string)));
}

// Creates a megabyte of data.
std::unique_ptr<base::Value> CreateMegabyte() {
  base::ListValue* megabyte = new base::ListValue();
  for (int i = 0; i < 1000; ++i) {
    megabyte->Append(CreateKilobyte());
  }
  return std::unique_ptr<base::Value>(megabyte);
}

// Intended as a StorageCallback from GetStorage.
static void AssignStorage(ValueStore** dst, ValueStore* src) {
  *dst = src;
}

ValueStore* GetStorage(scoped_refptr<const Extension> extension,
                       settings_namespace::Namespace settings_namespace,
                       StorageFrontend* frontend) {
  ValueStore* storage = NULL;
  frontend->RunWithStorage(
      extension, settings_namespace, base::Bind(&AssignStorage, &storage));
  content::RunAllTasksUntilIdle();
  return storage;
}

ValueStore* GetStorage(scoped_refptr<const Extension> extension,
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
  base::DictionaryValue manifest;
  manifest.SetString("name", std::string("Test extension ") + id);
  manifest.SetString("version", "1.0");
  manifest.SetInteger("manifest_version", 2);

  std::unique_ptr<base::ListValue> permissions(new base::ListValue());
  for (auto it = permissions_set.cbegin(); it != permissions_set.cend(); ++it) {
    permissions->AppendString(*it);
  }
  manifest.Set("permissions", std::move(permissions));

  switch (type) {
    case Manifest::TYPE_EXTENSION:
      break;

    case Manifest::TYPE_LEGACY_PACKAGED_APP: {
      auto app = std::make_unique<base::DictionaryValue>();
      auto app_launch = std::make_unique<base::DictionaryValue>();
      app_launch->SetString("local_path", "fake.html");
      app->Set("launch", std::move(app_launch));
      manifest.Set("app", std::move(app));
      break;
    }

    default:
      NOTREACHED();
  }

  std::string error;
  scoped_refptr<const Extension> extension(
      Extension::Create(base::FilePath(),
                        Manifest::INTERNAL,
                        manifest,
                        Extension::NO_FLAGS,
                        id,
                        &error));
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
