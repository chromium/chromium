// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_STORAGE_STORAGE_AREA_NAMESPACE_H_
#define EXTENSIONS_BROWSER_API_STORAGE_STORAGE_AREA_NAMESPACE_H_

#include <string>

#include "extensions/browser/api/storage/settings_namespace.h"

namespace extensions {

// Enumerates all the namespaces of the storage areas.
enum class StorageAreaNamespace {
  kLocal,    // "local"    i.e. chrome.storage.local
  kSync,     // "sync"     i.e. chrome.storage.sync
  kManaged,  // "managed"  i.e. chrome.storage.managed
  kSession,  // "session"  i.e. chrome.storage.session
  kInvalid,
};

// Returns the string representation of `storage_area`, or an empty string if
// it doesn't map to one.
const char* StorageAreaToString(StorageAreaNamespace storage_area);

// Returns the settings namespace of `storage_area`, or `Namespace::INVALID` if
// the StorageArea doesn't map to one.
settings_namespace::Namespace StorageAreaToSettingsNamespace(
    StorageAreaNamespace storage_area);

// Returns the StorageArea of `storage_area_string`, or
// `StorageAreaNamespace::kInvalid` if the string doesn't map to one.
StorageAreaNamespace StorageAreaFromString(
    const std::string& storage_area_string);

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_STORAGE_STORAGE_AREA_NAMESPACE_H_
