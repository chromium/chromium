// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/storage/storage_area_namespace.h"

#include "base/notreached.h"

namespace extensions {

namespace {
constexpr char kLocalString[] = "local";
constexpr char kSyncString[] = "sync";
constexpr char kManagedString[] = "managed";
constexpr char kSessionString[] = "session";
}  // namespace

const char* StorageAreaToString(StorageAreaNamespace storage_area) {
  switch (storage_area) {
    case StorageAreaNamespace::kLocal:
      return kLocalString;
    case StorageAreaNamespace::kSync:
      return kSyncString;
    case StorageAreaNamespace::kManaged:
      return kManagedString;
    case StorageAreaNamespace::kSession:
      return kSessionString;
    case StorageAreaNamespace::kInvalid:
      NOTREACHED_IN_MIGRATION();
      return "";
  }
}

settings_namespace::Namespace StorageAreaToSettingsNamespace(
    StorageAreaNamespace storage_area) {
  switch (storage_area) {
    case StorageAreaNamespace::kLocal:
      return settings_namespace::LOCAL;
    case StorageAreaNamespace::kSync:
      return settings_namespace::SYNC;
    case StorageAreaNamespace::kManaged:
      return settings_namespace::MANAGED;
    case StorageAreaNamespace::kSession:
      return settings_namespace::INVALID;
    case StorageAreaNamespace::kInvalid:
      NOTREACHED_IN_MIGRATION();
      return settings_namespace::INVALID;
  }
}

StorageAreaNamespace StorageAreaFromString(
    const std::string& storage_area_string) {
  if (storage_area_string == kLocalString)
    return StorageAreaNamespace::kLocal;
  if (storage_area_string == kSyncString)
    return StorageAreaNamespace::kSync;
  if (storage_area_string == kManagedString)
    return StorageAreaNamespace::kManaged;
  if (storage_area_string == kSessionString)
    return StorageAreaNamespace::kSession;
  return StorageAreaNamespace::kInvalid;
}

}  // namespace extensions
