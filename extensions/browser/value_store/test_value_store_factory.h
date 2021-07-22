// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_VALUE_STORE_TEST_VALUE_STORE_FACTORY_H_
#define EXTENSIONS_BROWSER_VALUE_STORE_TEST_VALUE_STORE_FACTORY_H_

#include <map>
#include <memory>
#include <set>

#include "base/files/file_path.h"
#include "extensions/browser/value_store/value_store_client_id.h"
#include "extensions/browser/value_store/value_store_factory.h"

class ValueStore;

namespace extensions {

// Used for tests when a new test ValueStore is required. Will either open a
// database on disk (if path provided) returning a |LeveldbValueStore|.
// Otherwise a new |TestingValueStore| instance will be returned.
class TestValueStoreFactory : public ValueStoreFactory {
 public:
  TestValueStoreFactory();
  explicit TestValueStoreFactory(const base::FilePath& db_path);
  TestValueStoreFactory(const TestValueStoreFactory&) = delete;
  TestValueStoreFactory& operator=(const TestValueStoreFactory&) = delete;

  // ValueStoreFactory
  std::unique_ptr<ValueStore> CreateRulesStore() override;
  std::unique_ptr<ValueStore> CreateStateStore() override;
  std::unique_ptr<ValueStore> CreateSettingsStore(
      settings_namespace::Namespace settings_namespace,
      ModelType model_type,
      const ValueStoreClientId& id) override;
  void DeleteSettings(settings_namespace::Namespace settings_namespace,
                      ModelType model_type,
                      const ValueStoreClientId& id) override;
  bool HasSettings(settings_namespace::Namespace settings_namespace,
                   ModelType model_type,
                   const ValueStoreClientId& id) override;
  std::set<ValueStoreClientId> GetKnownExtensionIDs(
      settings_namespace::Namespace settings_namespace,
      ModelType model_type) const override;

  // Return the last created |ValueStore|. Use with caution as this may return
  // a dangling pointer since the creator now owns the ValueStore which can be
  // deleted at any time.
  ValueStore* LastCreatedStore() const;
  // Return a previously created |ValueStore| for an extension.
  ValueStore* GetExisting(const ValueStoreClientId& id) const;
  // Reset this class (as if just created).
  void Reset();

 private:
  // Manages a collection of |ValueStore|'s created for an app/extension.
  // One of these exists for each setting type.
  class StorageHelper {
   public:
    StorageHelper();
    ~StorageHelper();
    StorageHelper(const StorageHelper&) = delete;
    StorageHelper& operator=(const StorageHelper&) = delete;

    std::set<ValueStoreClientId> GetKnownExtensionIDs(
        ModelType model_type) const;
    ValueStore* AddValueStore(const ValueStoreClientId& id,
                              ValueStore* value_store,
                              ModelType model_type);
    void DeleteSettings(const ValueStoreClientId& id, ModelType model_type);
    bool HasSettings(const ValueStoreClientId& id, ModelType model_type) const;
    void Reset();
    ValueStore* GetExisting(const ValueStoreClientId& id) const;

   private:
    std::map<ValueStoreClientId, ValueStore*> app_stores_;
    std::map<ValueStoreClientId, ValueStore*> extension_stores_;
  };

  StorageHelper& GetStorageHelper(
      settings_namespace::Namespace settings_namespace);

  ~TestValueStoreFactory() override;
  base::FilePath db_path_;
  ValueStore* last_created_store_ = nullptr;

  // None of these value stores are owned by this factory, so care must be
  // taken when calling GetExisting.
  StorageHelper local_helper_;
  StorageHelper sync_helper_;
  StorageHelper managed_helper_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_VALUE_STORE_TEST_VALUE_STORE_FACTORY_H_
