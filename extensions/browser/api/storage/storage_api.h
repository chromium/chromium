// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_STORAGE_STORAGE_API_H_
#define EXTENSIONS_BROWSER_API_STORAGE_STORAGE_API_H_

#include "base/compiler_specific.h"
#include "components/value_store/value_store.h"
#include "extensions/browser/api/storage/session_storage_manager.h"
#include "extensions/browser/api/storage/settings_namespace.h"
#include "extensions/browser/api/storage/settings_observer.h"
#include "extensions/browser/api/storage/storage_area_namespace.h"
#include "extensions/browser/api/storage/storage_frontend.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

// Superclass of all settings functions.
class SettingsFunction : public ExtensionFunction {
 protected:
  SettingsFunction();
  ~SettingsFunction() override;

  // ExtensionFunction:
  bool ShouldSkipQuotaLimiting() const override;
  bool PreRunValidation(std::string* error) override;

  // Returns whether the caller's context has access to the storage or not.
  bool IsAccessToStorageAllowed();

  StorageAreaNamespace storage_area() const { return storage_area_; }

  void OnWriteOperationFinished(StorageFrontend::ResultStatus status);

 private:
  // The Storage Area the call was for. For example: kLocal if the API call was
  // chrome.storage.local, kSync if the API call was chrome.storage.sync, etc.
  StorageAreaNamespace storage_area_ = StorageAreaNamespace::kInvalid;

  // The settings namespace the call was for. Only includes
  // StorageAreaNamespace's that use ValueStore.
  settings_namespace::Namespace settings_namespace_ =
      settings_namespace::INVALID;
};

class StorageStorageAreaGetFunction : public SettingsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("storage.get", STORAGE_GET)

 protected:
  ~StorageStorageAreaGetFunction() override {}

  // SettingsFunction:
  ResponseAction Run() override;

  // Called after getting data from storage. If `defaults` is provided, merges
  // the data from `result` into the dictionary. This allows developers to
  // provide a fallback for data not present in storage.
  void OnGetOperationFinished(std::optional<base::Value::Dict> defaults,
                              StorageFrontend::GetResult result);
};

class StorageStorageAreaGetKeysFunction : public SettingsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("storage.getKeys", STORAGE_GETKEYS)

 protected:
  ~StorageStorageAreaGetKeysFunction() override = default;

  // SettingsFunction:
  ResponseAction Run() override;

  // Called after getting keys from storage.
  void OnGetKeysOperationFinished(StorageFrontend::GetKeysResult result);
};

class StorageStorageAreaSetFunction : public SettingsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("storage.set", STORAGE_SET)

 protected:
  ~StorageStorageAreaSetFunction() override {}

  // SettingsFunction:
  ResponseAction Run() override;

  // ExtensionFunction:
  void GetQuotaLimitHeuristics(QuotaLimitHeuristics* heuristics) const override;
};

class StorageStorageAreaRemoveFunction : public SettingsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("storage.remove", STORAGE_REMOVE)

 protected:
  ~StorageStorageAreaRemoveFunction() override {}

  // SettingsFunction:
  ResponseAction Run() override;

  // ExtensionFunction:
  void GetQuotaLimitHeuristics(QuotaLimitHeuristics* heuristics) const override;
};

class StorageStorageAreaClearFunction : public SettingsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("storage.clear", STORAGE_CLEAR)

 protected:
  ~StorageStorageAreaClearFunction() override {}

  // SettingsFunction:
  ResponseAction Run() override;

  // ExtensionFunction:
  void GetQuotaLimitHeuristics(QuotaLimitHeuristics* heuristics) const override;
};

class StorageStorageAreaGetBytesInUseFunction : public SettingsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("storage.getBytesInUse", STORAGE_GETBYTESINUSE)

  FRIEND_TEST_ALL_PREFIXES(StorageApiUnittest, GetBytesInUseIntOverflow);

 protected:
  ~StorageStorageAreaGetBytesInUseFunction() override {}

  // SettingsFunction:
  ResponseAction Run() override;

  // Called after retrieving bytes from storage.
  void OnGetBytesInUseOperationFinished(size_t);
};

class StorageStorageAreaSetAccessLevelFunction : public SettingsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("storage.setAccessLevel", STORAGE_SETACCESSLEVEL)
  StorageStorageAreaSetAccessLevelFunction() = default;
  StorageStorageAreaSetAccessLevelFunction(
      const StorageStorageAreaSetAccessLevelFunction&) = delete;
  StorageStorageAreaSetAccessLevelFunction& operator=(
      const StorageStorageAreaSetAccessLevelFunction&) = delete;

 protected:
  ~StorageStorageAreaSetAccessLevelFunction() override = default;

  // SettingsFunction:
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_STORAGE_STORAGE_API_H_
