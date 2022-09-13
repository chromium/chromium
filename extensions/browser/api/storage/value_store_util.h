// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_STORAGE_VALUE_STORE_UTIL_H_
#define EXTENSIONS_BROWSER_API_STORAGE_VALUE_STORE_UTIL_H_

#include "components/value_store/value_store.h"
#include "components/value_store/value_store_factory.h"
#include "extensions/browser/api/storage/settings_namespace.h"
#include "extensions/common/extension_id.h"

namespace base {
class FilePath;
}

namespace extensions {

// Generalises extensions-specific code for use with the non-extensions-specific
// ValueStore.
namespace value_store_util {

enum class ModelType { APP, EXTENSION };

// Gets the directory for ValueStore based on the specified
// `settings_namespace`, `model_type` and `id`.
base::FilePath GetValueStoreDir(
    settings_namespace::Namespace settings_namespace,
    ModelType model_type,
    const ExtensionId& id);

// Creates a `ValueStore` to contain settings data for a specific extension
// namespace and model type.
std::unique_ptr<value_store::ValueStore> CreateSettingsStore(
    settings_namespace::Namespace settings_namespace,
    ModelType model_type,
    const ExtensionId& id,
    scoped_refptr<value_store::ValueStoreFactory> factory);

// Deletes all settings for the given extension in the specified
// `settings_namespace` and `model_type`.
void DeleteValueStore(settings_namespace::Namespace settings_namespace,
                      ModelType model_type,
                      const ExtensionId& id,
                      scoped_refptr<value_store::ValueStoreFactory> factory);

// Returns whether there is any settings stored in the specified
// `settings_namespace` and `model_type` for the given extension.
bool HasValueStore(settings_namespace::Namespace settings_namespace,
                   ModelType model_type,
                   const ExtensionId& id,
                   scoped_refptr<value_store::ValueStoreFactory> factory);

}  // namespace value_store_util
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_STORAGE_VALUE_STORE_UTIL_H_
