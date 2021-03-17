// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_VALUE_STORE_TEST_VALUE_STORE_FACTORY_H_
#define EXTENSIONS_BROWSER_VALUE_STORE_TEST_VALUE_STORE_FACTORY_H_

#include <map>
#include <memory>
#include <set>

#include "base/files/file_path.h"
#include "extensions/browser/value_store/value_store_factory.h"
#include "extensions/common/extension_id.h"

class ValueStore;

namespace extensions {

// Used for tests when a new test ValueStore is required. Will either open a
// database on disk (if path provided) returning a |LeveldbValueStore|.
// Otherwise a new |TestingValueStore| instance will be returned.
class TestValueStoreFactory : public ValueStoreFactory {
 public:
  TestValueStoreFactory();
  explicit TestValueStoreFactory(const base::FilePath& db_path);

  // ValueStoreFactory
  std::unique_ptr<ValueStore> CreateRulesStore() override;
  std::unique_ptr<ValueStore> CreateStateStore() override;
  std::unique_ptr<ValueStore> CreateSettingsStore(
      settings_namespace::Namespace settings_namespace,
      ModelType model_type,
      const ExtensionId& extension_id) override;
  void DeleteSettings(settings_namespace::Namespace settings_namespace,
                      ModelType model_type,
                      const ExtensionId& extension_id) override;
  bool HasSettings(settings_namespace::Namespace settings_namespace,
                   ModelType model_type,
                   const ExtensionId& extension_id) override;
  std::set<ExtensionId> GetKnownExtensionIDs(
      settings_namespace::Namespace settings_namespace,
      ModelType model_type) const override;

  // Return the last created |ValueStore|. Use with caution as this may return
  // a dangling pointer since the creator now owns the ValueStore which can be
  // deleted at any time.
  ValueStore* LastCreatedStore() const;
  // Return a previously created |ValueStore| for an extension.
  ValueStore* GetExisting(const ExtensionId& extension_id) const;
  // Reset this class (as if just created).
  void Reset();

 private:
  // Manages a collection of |ValueStore|'s created for an app/extension.
  // One of these exists for each setting type.
  class StorageHelper {
   public:
    StorageHelper();
    ~StorageHelper();
    std::set<ExtensionId> GetKnownExtensionIDs(ModelType model_type) const;
    ValueStore* AddValueStore(const ExtensionId& extension_id,
                              ValueStore* value_store,
                              ModelType model_type);
    void DeleteSettings(const ExtensionId& extension_id, ModelType model_type);
    bool HasSettings(const ExtensionId& extension_id,
                     ModelType model_type) const;
    void Reset();
    ValueStore* GetExisting(const ExtensionId& extension_id) const;

   private:
    std::map<ExtensionId, ValueStore*> app_stores_;
    std::map<ExtensionId, ValueStore*> extension_stores_;

    DISALLOW_COPY_AND_ASSIGN(StorageHelper);
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

  DISALLOW_COPY_AND_ASSIGN(TestValueStoreFactory);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_VALUE_STORE_TEST_VALUE_STORE_FACTORY_H_
