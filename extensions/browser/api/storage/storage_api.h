// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_STORAGE_STORAGE_API_H_
#define EXTENSIONS_BROWSER_API_STORAGE_STORAGE_API_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "extensions/browser/api/storage/settings_namespace.h"
#include "extensions/browser/api/storage/settings_observer.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/value_store/value_store.h"

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
  virtual ResponseValue RunWithStorage(ValueStore* storage) = 0;

  // Convert the |result| of a read function to the appropriate response value.
  // - If the |result| succeeded this will return a response object argument.
  // - If the |result| failed will return an error object.
  ResponseValue UseReadResult(ValueStore::ReadResult result);

  // Handles the |result| of a write function.
  // - If the |result| succeeded this will send out change notification(s), if
  //   appropriate, and return no arguments.
  // - If the |result| failed will return an error object.
  ResponseValue UseWriteResult(ValueStore::WriteResult result);

 private:
  // Called via PostTask from Run. Calls RunWithStorage and then
  // SendResponse with its success value.
  void AsyncRunWithStorage(ValueStore* storage);

  // The settings namespace the call was for.  For example, SYNC if the API
  // call was chrome.settings.experimental.sync..., LOCAL if .local, etc.
  settings_namespace::Namespace settings_namespace_;

  // Observers, cached so that it's only grabbed from the UI thread.
  scoped_refptr<SettingsObserverList> observers_;
};

class StorageStorageAreaGetFunction : public SettingsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("storage.get", STORAGE_GET)

 protected:
  ~StorageStorageAreaGetFunction() override {}

  // SettingsFunction:
  ResponseValue RunWithStorage(ValueStore* storage) override;
};

class StorageStorageAreaSetFunction : public SettingsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("storage.set", STORAGE_SET)

 protected:
  ~StorageStorageAreaSetFunction() override {}

  // SettingsFunction:
  ResponseValue RunWithStorage(ValueStore* storage) override;

  // ExtensionFunction:
  void GetQuotaLimitHeuristics(QuotaLimitHeuristics* heuristics) const override;
};

class StorageStorageAreaRemoveFunction : public SettingsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("storage.remove", STORAGE_REMOVE)

 protected:
  ~StorageStorageAreaRemoveFunction() override {}

  // SettingsFunction:
  ResponseValue RunWithStorage(ValueStore* storage) override;

  // ExtensionFunction:
  void GetQuotaLimitHeuristics(QuotaLimitHeuristics* heuristics) const override;
};

class StorageStorageAreaClearFunction : public SettingsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("storage.clear", STORAGE_CLEAR)

 protected:
  ~StorageStorageAreaClearFunction() override {}

  // SettingsFunction:
  ResponseValue RunWithStorage(ValueStore* storage) override;

  // ExtensionFunction:
  void GetQuotaLimitHeuristics(QuotaLimitHeuristics* heuristics) const override;
};

class StorageStorageAreaGetBytesInUseFunction : public SettingsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("storage.getBytesInUse", STORAGE_GETBYTESINUSE)

 protected:
  ~StorageStorageAreaGetBytesInUseFunction() override {}

  // SettingsFunction:
  ResponseValue RunWithStorage(ValueStore* storage) override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_STORAGE_STORAGE_API_H_
