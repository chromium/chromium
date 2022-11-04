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
#include "extensions/browser/extension_function.h"

namespace extensions {

// Superclass of all settings functions.
class SettingsFunction : public ExtensionFunction {
 protected:
  SettingsFunction();
  ~SettingsFunction() override;

  // ExtensionFunction:
  bool ShouldSkipQuotaLimiting() const override;
  ResponseAction Run() override;

  // Extension settings function implementations should do their work here.
  // The StorageFrontend makes sure this is posted to the appropriate thread.
  virtual ResponseValue RunWithStorage(value_store::ValueStore* storage) = 0;

  // Extension settings function implementations in `session` namespace should
  // do their work here.
  virtual ResponseValue RunInSession() = 0;

  // Convert the |result| of a read function to the appropriate response value.
  // - If the |result| succeeded this will return a response object argument.
  // - If the |result| failed will return an error object.
  ResponseValue UseReadResult(value_store::ValueStore::ReadResult result);

  // Handles the |result| of a write function.
  // - If the |result| succeeded this will send out change notification(s), if
  //   appropriate, and return no arguments.
  // - If the |result| failed will return an error object.
  ResponseValue UseWriteResult(value_store::ValueStore::WriteResult result);

  // Notifies the given `changes`, if non empty, to the observer.
  void OnSessionSettingsChanged(
      std::vector<SessionStorageManager::ValueChange> changes);

  // Returns whether the caller's context has access to the storage or not.
  bool IsAccessToStorageAllowed();

 private:
  // Called via PostTask from Run. Calls RunWithStorage and then
  // SendResponse with its success value.
  void AsyncRunWithStorage(value_store::ValueStore* storage);

  // The Storage Area the call was for. For example: kLocal if the API call was
  // chrome.storage.local, kSync if the API call was chrome.storage.sync, etc.
  StorageAreaNamespace storage_area_ = StorageAreaNamespace::kInvalid;

  // The settings namespace the call was for. Only includes
  // StorageAreaNamespace's that use ValueStore.
  settings_namespace::Namespace settings_namespace_ =
      settings_namespace::INVALID;

  // Observers, cached so that it's only grabbed from the UI thread.
  SequenceBoundSettingsChangedCallback observer_;
};

class StorageStorageAreaGetFunction : public SettingsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("storage.get", STORAGE_GET)

 protected:
  ~StorageStorageAreaGetFunction() override {}

  // SettingsFunction:
  ResponseValue RunWithStorage(value_store::ValueStore* storage) override;
  ResponseValue RunInSession() override;
};

class StorageStorageAreaSetFunction : public SettingsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("storage.set", STORAGE_SET)

 protected:
  ~StorageStorageAreaSetFunction() override {}

  // SettingsFunction:
  ResponseValue RunWithStorage(value_store::ValueStore* storage) override;
  ResponseValue RunInSession() override;

  // ExtensionFunction:
  void GetQuotaLimitHeuristics(QuotaLimitHeuristics* heuristics) const override;
};

class StorageStorageAreaRemoveFunction : public SettingsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("storage.remove", STORAGE_REMOVE)

 protected:
  ~StorageStorageAreaRemoveFunction() override {}

  // SettingsFunction:
  ResponseValue RunWithStorage(value_store::ValueStore* storage) override;
  ResponseValue RunInSession() override;

  // ExtensionFunction:
  void GetQuotaLimitHeuristics(QuotaLimitHeuristics* heuristics) const override;
};

class StorageStorageAreaClearFunction : public SettingsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("storage.clear", STORAGE_CLEAR)

 protected:
  ~StorageStorageAreaClearFunction() override {}

  // SettingsFunction:
  ResponseValue RunWithStorage(value_store::ValueStore* storage) override;
  ResponseValue RunInSession() override;

  // ExtensionFunction:
  void GetQuotaLimitHeuristics(QuotaLimitHeuristics* heuristics) const override;
};

class StorageStorageAreaGetBytesInUseFunction : public SettingsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("storage.getBytesInUse", STORAGE_GETBYTESINUSE)

 protected:
  ~StorageStorageAreaGetBytesInUseFunction() override {}

  // SettingsFunction:
  ResponseValue RunWithStorage(value_store::ValueStore* storage) override;
  ResponseValue RunInSession() override;
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
  ResponseValue RunWithStorage(value_store::ValueStore* storage) override;
  ResponseValue RunInSession() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_STORAGE_STORAGE_API_H_
