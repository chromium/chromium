// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/lock_screen_data/lock_screen_value_store_migrator_impl.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "crypto/symmetric_key.h"
#include "extensions/browser/api/lock_screen_data/data_item.h"
#include "extensions/browser/api/lock_screen_data/operation_result.h"
#include "extensions/browser/api/storage/backend_task_runner.h"
#include "extensions/browser/api/storage/local_value_store_cache.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/test_extensions_browser_client.h"
#include "extensions/browser/value_store/test_value_store_factory.h"
#include "extensions/browser/value_store/testing_value_store.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace lock_screen_data {

namespace {

constexpr char kFirstExtensionId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
constexpr char kSecondExtensionId[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
constexpr char kThirdExtensionId[] = "cccccccccccccccccccccccccccccccc";

void ExpectNotRun(const std::string& message) {
  ADD_FAILURE() << "Unexpectedly run: " << message;
}

void WriteCallback(const base::Closure& callback,
                   OperationResult* result_out,
                   OperationResult result) {
  *result_out = result;
  callback.Run();
}

void ReadCallback(const base::Closure& callback,
                  OperationResult* result_out,
                  std::unique_ptr<std::vector<char>>* content_out,
                  OperationResult result,
                  std::unique_ptr<std::vector<char>> content) {
  *result_out = result;
  *content_out = std::move(content);
  callback.Run();
}

void GetRegisteredItemsCallback(
    const base::Closure& callback,
    OperationResult* result_out,
    std::unique_ptr<base::DictionaryValue>* value_out,
    OperationResult result,
    std::unique_ptr<base::DictionaryValue> value) {
  *result_out = result;
  *value_out = std::move(value);
  callback.Run();
}

}  // namespace

class LockScreenValueStoreMigratorImplTest : public testing::Test {
 public:
  LockScreenValueStoreMigratorImplTest() = default;
  ~LockScreenValueStoreMigratorImplTest() override = default;

  void SetUp() override {
    task_runner_ = GetBackendTaskRunner();

    context_ = std::make_unique<content::TestBrowserContext>();

    extensions_browser_client_ =
        std::make_unique<TestExtensionsBrowserClient>(context_.get());
    BrowserContextDependencyManager::GetInstance()->MarkBrowserContextLive(
        context_.get());

    ExtensionsBrowserClient::Set(extensions_browser_client_.get());

    source_value_store_factory_ = base::MakeRefCounted<TestValueStoreFactory>();
    source_value_store_cache_ =
        std::make_unique<LocalValueStoreCache>(source_value_store_factory_);

    target_value_store_factory_ = base::MakeRefCounted<TestValueStoreFactory>();
    target_value_store_cache_ =
        std::make_unique<LocalValueStoreCache>(target_value_store_factory_);

    migrator_crypto_key_ = GenerateKey("password");

    migrator_ = std::make_unique<LockScreenValueStoreMigratorImpl>(
        context_.get(), source_value_store_cache_.get(),
        target_value_store_cache_.get(), task_runner_.get(),
        migrator_crypto_key_);
  }

  void TearDown() override {
    TearDownValueStoreCache();

    BrowserContextDependencyManager::GetInstance()
        ->DestroyBrowserContextServices(context_.get());
    ExtensionsBrowserClient::Set(nullptr);
    extensions_browser_client_.reset();
    context_.reset();
  }

 protected:
  struct TestItemData {
    std::string id;
    ExtensionId extension_id;
    std::string crypto_key;
    std::vector<char> content;
  };

  enum class StorageType { SOURCE, TARGET };

  void RunMigrator(const std::set<ExtensionId>& extensions_to_migrate) {
    migrator_->Run(
        extensions_to_migrate,
        base::Bind(&LockScreenValueStoreMigratorImplTest::OnExtensionMigrated,
                   base::Unretained(this)));
  }

  std::string GenerateKey(const std::string& password) {
    std::unique_ptr<crypto::SymmetricKey> key =
        crypto::SymmetricKey::DeriveKeyFromPasswordUsingPbkdf2(
            crypto::SymmetricKey::AES, password, "salt", 1000, 256);
    if (!key) {
      ADD_FAILURE() << "Failed to create symmetric key";
      return std::string();
    }

    return key->key();
  }

  bool InitializeStorage(StorageType storage_type,
                         const std::vector<TestItemData>& items) {
    ValueStoreCache* storage = storage_type == StorageType::SOURCE
                                   ? source_value_store_cache_.get()
                                   : target_value_store_cache_.get();
    for (const auto& item : items) {
      auto data_item = std::make_unique<DataItem>(
          item.id, item.extension_id, context_.get(), storage,
          task_runner_.get(), item.crypto_key);

      if (RegisterDataItem(data_item.get()) != OperationResult::kSuccess) {
        ADD_FAILURE() << "Register " << item.id;
        return false;
      }

      if (WriteToDataItem(data_item.get(), item.content) !=
          OperationResult::kSuccess) {
        ADD_FAILURE() << "Write " << item.id;
        return false;
      }
    }
    return true;
  }

  std::vector<std::vector<char>> GetItemContent(
      StorageType storage_type,
      std::vector<std::string> item_ids,
      const ExtensionId& extension_id,
      const std::string& crypto_key) {
    ValueStoreCache* storage = storage_type == StorageType::SOURCE
                                   ? source_value_store_cache_.get()
                                   : target_value_store_cache_.get();

    std::vector<std::vector<char>> items;
    for (const auto& item_id : item_ids) {
      auto data_item =
          std::make_unique<DataItem>(item_id, extension_id, context_.get(),
                                     storage, task_runner_.get(), crypto_key);
      std::unique_ptr<std::vector<char>> data;
      OperationResult read_result = ReadDataItem(data_item.get(), &data);
      if (read_result == OperationResult::kSuccess) {
        items.emplace_back();
        items.back().swap(*data);
      } else {
        ADD_FAILURE() << "Reading " << item_id << " failed "
                      << static_cast<int>(read_result);
      }
    }

    return items;
  }

  OperationResult RegisterDataItem(DataItem* item) {
    base::RunLoop run_loop;
    OperationResult result = OperationResult::kFailed;
    item->Register(base::Bind(&WriteCallback, run_loop.QuitClosure(), &result));
    run_loop.Run();
    return result;
  }

  OperationResult WriteToDataItem(DataItem* item,
                                  const std::vector<char>& content) {
    base::RunLoop run_loop;
    OperationResult result = OperationResult::kFailed;
    item->Write(content,
                base::Bind(&WriteCallback, run_loop.QuitClosure(), &result));
    run_loop.Run();
    return result;
  }

  OperationResult ReadDataItem(DataItem* item,
                               std::unique_ptr<std::vector<char>>* data) {
    OperationResult result = OperationResult::kFailed;
    std::unique_ptr<std::vector<char>> read_content;
    base::RunLoop run_loop;
    item->Read(base::Bind(&ReadCallback, run_loop.QuitClosure(), &result,
                          &read_content));
    run_loop.Run();
    if (data)
      *data = std::move(read_content);
    return result;
  }

  std::set<std::string> GetRegisteredItemIds(StorageType storage_type,
                                             const ExtensionId& extension_id) {
    ValueStoreCache* storage = storage_type == StorageType::SOURCE
                                   ? source_value_store_cache_.get()
                                   : target_value_store_cache_.get();

    OperationResult result = OperationResult::kFailed;
    std::unique_ptr<base::DictionaryValue> items_value;

    base::RunLoop run_loop;
    DataItem::GetRegisteredValuesForExtension(
        context_.get(), storage, task_runner_.get(), extension_id,
        base::Bind(&GetRegisteredItemsCallback, run_loop.QuitClosure(), &result,
                   &items_value));
    run_loop.Run();

    if (result != OperationResult::kSuccess) {
      ADD_FAILURE() << "Getting registered items failed";
      return std::set<std::string>();
    }

    std::set<std::string> items;
    for (base::DictionaryValue::Iterator iter(*items_value); !iter.IsAtEnd();
         iter.Advance()) {
      items.insert(iter.key());
    }
    return items;
  }

  scoped_refptr<const Extension> AddTestExtension(
      const ExtensionId& extension_id) {
    DictionaryBuilder app_builder;
    app_builder.Set("background",
                    DictionaryBuilder()
                        .Set("scripts", ListBuilder().Append("script").Build())
                        .Build());
    ListBuilder app_handlers_builder;
    app_handlers_builder.Append(DictionaryBuilder()
                                    .Set("action", "new_note")
                                    .Set("enabled_on_lock_screen", true)
                                    .Build());
    scoped_refptr<const Extension> extension =
        ExtensionBuilder()
            .SetID(extension_id)
            .SetManifest(
                DictionaryBuilder()
                    .Set("name", "Test app")
                    .Set("version", "1.0")
                    .Set("manifest_version", 2)
                    .Set("app", app_builder.Build())
                    .Set("action_handlers", app_handlers_builder.Build())
                    .Set("permissions",
                         ListBuilder().Append("lockScreen").Build())
                    .Build())
            .Build();
    ExtensionRegistry::Get(context_.get())->AddEnabled(extension);
    return extension;
  }

  const std::string& migrator_crypto_key() const {
    return migrator_crypto_key_;
  }

  LockScreenValueStoreMigratorImpl* migrator() { return migrator_.get(); }

  const std::vector<ExtensionId>& migrated_extension_ids() const {
    return migrated_extension_ids_;
  }

  void DeleteMigrator() { migrator_.reset(); }

  void RunTaskRunnerTasks() {
    base::RunLoop run_loop;
    task_runner_->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                   run_loop.QuitClosure());
    run_loop.Run();
  }

  void WaitMigrationDone(const ExtensionId& extension_id) {
    extension_waiters_[extension_id].Run();
  }

  void SetReturnCodeForValueStoreOperations(StorageType storage_type,
                                            const ExtensionId& extension_id,
                                            ValueStore::StatusCode code) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &LockScreenValueStoreMigratorImplTest::SetValueStoreReturnCodeImpl,
            base::Unretained(this), storage_type, extension_id, code));
  }

 private:
  void TearDownValueStoreCache() {
    base::RunLoop run_loop;
    task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::Bind(
            &LockScreenValueStoreMigratorImplTest::ReleaseValueStoreCaches,
            base::Unretained(this)),
        run_loop.QuitClosure());
    run_loop.Run();
  }

  void ReleaseValueStoreCaches() {
    source_value_store_cache_.reset();
    target_value_store_cache_.reset();
  }

  void SetValueStoreReturnCodeImpl(StorageType storage_type,
                                   const ExtensionId& extension_id,
                                   ValueStore::StatusCode code) {
    TestValueStoreFactory* factory = storage_type == StorageType::SOURCE
                                         ? source_value_store_factory_.get()
                                         : target_value_store_factory_.get();
    TestingValueStore* store =
        static_cast<TestingValueStore*>(factory->GetExisting(extension_id));
    ASSERT_TRUE(store);

    store->set_status_code(code);
  }

  void OnExtensionMigrated(const ExtensionId& extension_id) {
    migrated_extension_ids_.push_back(extension_id);

    extension_waiters_[extension_id].Quit();
  }

  content::BrowserTaskEnvironment task_environment_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  std::unique_ptr<content::TestBrowserContext> context_;

  std::unique_ptr<TestExtensionsBrowserClient> extensions_browser_client_;

  scoped_refptr<TestValueStoreFactory> source_value_store_factory_;
  std::unique_ptr<ValueStoreCache> source_value_store_cache_;

  scoped_refptr<TestValueStoreFactory> target_value_store_factory_;
  std::unique_ptr<ValueStoreCache> target_value_store_cache_;

  std::string migrator_crypto_key_;

  std::vector<ExtensionId> migrated_extension_ids_;

  std::unique_ptr<LockScreenValueStoreMigratorImpl> migrator_;

  std::map<ExtensionId, base::RunLoop> extension_waiters_;

  DISALLOW_COPY_AND_ASSIGN(LockScreenValueStoreMigratorImplTest);
};

TEST_F(LockScreenValueStoreMigratorImplTest, Basic) {
  auto app_1 = AddTestExtension(kFirstExtensionId);
  auto app_2 = AddTestExtension(kSecondExtensionId);

  std::string other_key = GenerateKey("other");
  ASSERT_TRUE(InitializeStorage(
      StorageType::SOURCE,
      {{"item_1", app_1->id(), migrator_crypto_key(), {'a', 'b', 'c'}},
       {"item_2", app_1->id(), migrator_crypto_key(), {'c', 'd'}},
       {"item_3", app_2->id(), migrator_crypto_key(), {'e', 'f', 'g'}},
       {"item_4", app_1->id(), other_key, {'h', 'i'}}}));

  RunMigrator({app_1->id()});
  WaitMigrationDone(app_1->id());

  // Verify items beloning to app_1 and encrypted with the same key have been
  // moved to the new storage.
  EXPECT_EQ(std::set<std::string>({"item_1", "item_2"}),
            GetRegisteredItemIds(StorageType::TARGET, app_1->id()));
  EXPECT_EQ(std::vector<std::vector<char>>({{'a', 'b', 'c'}, {'c', 'd'}}),
            GetItemContent(StorageType::TARGET, {"item_1", "item_2"},
                           app_1->id(), migrator_crypto_key()));

  // Items belonging to app_2 should not be moved.
  EXPECT_TRUE(GetRegisteredItemIds(StorageType::TARGET, app_2->id()).empty());

  // Original storage should not contain migrated items anymore.
  EXPECT_EQ(std::set<std::string>({"item_4"}),
            GetRegisteredItemIds(StorageType::SOURCE, app_1->id()));
  EXPECT_EQ(
      std::vector<std::vector<char>>({{'h', 'i'}}),
      GetItemContent(StorageType::SOURCE, {"item_4"}, app_1->id(), other_key));

  // Original storage should still contain items belonging to app_2.
  EXPECT_EQ(std::set<std::string>({"item_3"}),
            GetRegisteredItemIds(StorageType::SOURCE, app_2->id()));
  EXPECT_EQ(std::vector<std::vector<char>>({{'e', 'f', 'g'}}),
            GetItemContent(StorageType::SOURCE, {"item_3"}, app_2->id(),
                           migrator_crypto_key()));
}

TEST_F(LockScreenValueStoreMigratorImplTest, MigratingMultipleApps) {
  auto app_1 = AddTestExtension(kFirstExtensionId);
  auto app_2 = AddTestExtension(kSecondExtensionId);
  auto app_3 = AddTestExtension(kThirdExtensionId);

  std::string other_key = GenerateKey("other");
  ASSERT_TRUE(InitializeStorage(
      StorageType::SOURCE,
      {{"item_1", app_1->id(), migrator_crypto_key(), {'a', 'b', 'c'}},
       {"item_2", app_1->id(), migrator_crypto_key(), {'c', 'd'}},
       {"item_3", app_1->id(), other_key, {'e', 'f'}},
       {"item_4", app_2->id(), migrator_crypto_key(), {'g', 'h', 'i'}},
       {"item_5", app_3->id(), migrator_crypto_key(), {'j'}},
       {"item_6", app_3->id(), other_key, {'k', 'l', 'm'}}}));

  RunMigrator({app_1->id(), app_2->id()});
  WaitMigrationDone(app_1->id());
  WaitMigrationDone(app_2->id());

  // Verify items beloning to app_1 and encrypted with the same key have been
  // moved to the new storage.
  EXPECT_EQ(std::set<std::string>({"item_1", "item_2"}),
            GetRegisteredItemIds(StorageType::TARGET, app_1->id()));
  EXPECT_EQ(std::vector<std::vector<char>>({{'a', 'b', 'c'}, {'c', 'd'}}),
            GetItemContent(StorageType::TARGET, {"item_1", "item_2"},
                           app_1->id(), migrator_crypto_key()));

  // Verify items beloning to app_2 and encrypted with the same key have been
  // moved to the new storage.
  EXPECT_EQ(std::set<std::string>({"item_4"}),
            GetRegisteredItemIds(StorageType::TARGET, app_2->id()));
  EXPECT_EQ(std::vector<std::vector<char>>({{'g', 'h', 'i'}}),
            GetItemContent(StorageType::TARGET, {"item_4"}, app_2->id(),
                           migrator_crypto_key()));

  // Items belonging to app_3 should not be moved.
  EXPECT_TRUE(GetRegisteredItemIds(StorageType::TARGET, app_3->id()).empty());

  // Original storage should not contain migrated items anymore.
  EXPECT_EQ(std::set<std::string>({"item_3"}),
            GetRegisteredItemIds(StorageType::SOURCE, app_1->id()));
  EXPECT_EQ(
      std::vector<std::vector<char>>({{'e', 'f'}}),
      GetItemContent(StorageType::SOURCE, {"item_3"}, app_1->id(), other_key));
  EXPECT_TRUE(GetRegisteredItemIds(StorageType::SOURCE, app_2->id()).empty());

  // Original storage should still contain all items belonging to app_3.
  EXPECT_EQ(std::set<std::string>({"item_5", "item_6"}),
            GetRegisteredItemIds(StorageType::SOURCE, app_3->id()));
  EXPECT_EQ(std::vector<std::vector<char>>({{'j'}}),
            GetItemContent(StorageType::SOURCE, {"item_5"}, app_3->id(),
                           migrator_crypto_key()));
  EXPECT_EQ(
      std::vector<std::vector<char>>({{'k', 'l', 'm'}}),
      GetItemContent(StorageType::SOURCE, {"item_6"}, app_3->id(), other_key));
}

// Tests that any item in the target storage is overriden with the contents of
// the item with the same id in the source storage (this situation might arrise
// if previous migration attempt was cancelled before the item was written to
// target, but after its registration in target storage).
TEST_F(LockScreenValueStoreMigratorImplTest, ItemIdExistsInTheTarget) {
  auto app = AddTestExtension(kFirstExtensionId);

  ASSERT_TRUE(InitializeStorage(
      StorageType::SOURCE,
      {{"item_1", app->id(), migrator_crypto_key(), {'a', 'b', 'c'}},
       {"item_2", app->id(), migrator_crypto_key(), {'c', 'd'}}}));

  ASSERT_TRUE(
      InitializeStorage(StorageType::TARGET,
                        {{"item_1", app->id(), migrator_crypto_key(), {0}}}));

  RunMigrator({app->id()});
  WaitMigrationDone(app->id());

  EXPECT_EQ(std::set<std::string>({"item_1", "item_2"}),
            GetRegisteredItemIds(StorageType::TARGET, app->id()));
  EXPECT_EQ(std::vector<std::vector<char>>({{'a', 'b', 'c'}, {'c', 'd'}}),
            GetItemContent(StorageType::TARGET, {"item_1", "item_2"}, app->id(),
                           migrator_crypto_key()));
  EXPECT_TRUE(GetRegisteredItemIds(StorageType::SOURCE, app->id()).empty());
}

// Tests storage state at the moment the item is registered in the target.
TEST_F(LockScreenValueStoreMigratorImplTest,
       StepByStepMigrationRegisterInTarget) {
  auto app_1 = AddTestExtension(kFirstExtensionId);
  auto app_2 = AddTestExtension(kSecondExtensionId);

  ASSERT_TRUE(InitializeStorage(
      StorageType::SOURCE,
      {{"item_1", app_1->id(), migrator_crypto_key(), {'a', 'b', 'c'}},
       {"item_3", app_2->id(), migrator_crypto_key(), {'e'}}}));

  RunMigrator({app_1->id()});
  EXPECT_TRUE(migrator()->IsMigratingExtensionData(app_1->id()));
  EXPECT_FALSE(migrator()->IsMigratingExtensionData(app_2->id()));
  EXPECT_TRUE(migrated_extension_ids().empty());

  // Run task runner - first set of operation should retrieve set of
  // registered items in the source, with no changs to the storage.
  RunTaskRunnerTasks();
  EXPECT_TRUE(migrator()->IsMigratingExtensionData(app_1->id()));
  EXPECT_FALSE(migrator()->IsMigratingExtensionData(app_2->id()));
  EXPECT_TRUE(migrated_extension_ids().empty());

  // Run task runner - second set of operation should retrieve the first
  // item source, there should be no visible changes to the storage,
  RunTaskRunnerTasks();
  EXPECT_TRUE(migrator()->IsMigratingExtensionData(app_1->id()));
  EXPECT_FALSE(migrator()->IsMigratingExtensionData(app_2->id()));
  EXPECT_TRUE(migrated_extension_ids().empty());

  // Reset the migrator instance to stop migration (so the running migration
  // tasks do not interfere with the torage state) - note that at this will not
  // cancel in progress tasks on the storage.
  DeleteMigrator();

  // Run task runner - third set of operation should register the item in the
  // target storage.
  RunTaskRunnerTasks();
  EXPECT_TRUE(migrated_extension_ids().empty());

  EXPECT_EQ(std::set<std::string>({"item_1"}),
            GetRegisteredItemIds(StorageType::SOURCE, app_1->id()));
  EXPECT_EQ(std::vector<std::vector<char>>({{'a', 'b', 'c'}}),
            GetItemContent(StorageType::SOURCE, {"item_1"}, app_1->id(),
                           migrator_crypto_key()));

  EXPECT_EQ(std::set<std::string>({"item_1"}),
            GetRegisteredItemIds(StorageType::TARGET, app_1->id()));
  EXPECT_EQ(std::vector<std::vector<char>>({{}}),
            GetItemContent(StorageType::TARGET, {"item_1"}, app_1->id(),
                           migrator_crypto_key()));
}

// Tests storage state at the moment the target item content is written.
TEST_F(LockScreenValueStoreMigratorImplTest,
       StepByStepMigrationWriteTargetContents) {
  auto app_1 = AddTestExtension(kFirstExtensionId);
  auto app_2 = AddTestExtension(kSecondExtensionId);

  ASSERT_TRUE(InitializeStorage(
      StorageType::SOURCE,
      {{"item_1", app_1->id(), migrator_crypto_key(), {'a', 'b', 'c'}},
       {"item_3", app_2->id(), migrator_crypto_key(), {'e'}}}));

  RunMigrator({app_1->id()});
  EXPECT_TRUE(migrator()->IsMigratingExtensionData(app_1->id()));
  EXPECT_FALSE(migrator()->IsMigratingExtensionData(app_2->id()));
  EXPECT_TRUE(migrated_extension_ids().empty());

  // Get registered items in source.
  RunTaskRunnerTasks();
  // Read migrated item contents in source.
  RunTaskRunnerTasks();
  // Register migrated item in the target.
  RunTaskRunnerTasks();

  EXPECT_TRUE(migrator()->IsMigratingExtensionData(app_1->id()));
  EXPECT_FALSE(migrator()->IsMigratingExtensionData(app_2->id()));
  EXPECT_TRUE(migrated_extension_ids().empty());

  // Reset the migrator instance to stop migration (so the running migration
  // tasks do not interfere with the torage state) - note that at this will not
  // cancel in progress tasks on the storage.
  DeleteMigrator();

  // Run task runner - fourth set of operations should copy the item content
  // from the source to the target storage.
  RunTaskRunnerTasks();
  EXPECT_TRUE(migrated_extension_ids().empty());

  EXPECT_EQ(std::set<std::string>({"item_1"}),
            GetRegisteredItemIds(StorageType::SOURCE, app_1->id()));
  EXPECT_EQ(std::vector<std::vector<char>>({{'a', 'b', 'c'}}),
            GetItemContent(StorageType::SOURCE, {"item_1"}, app_1->id(),
                           migrator_crypto_key()));

  EXPECT_EQ(std::set<std::string>({"item_1"}),
            GetRegisteredItemIds(StorageType::TARGET, app_1->id()));
  EXPECT_EQ(std::vector<std::vector<char>>({{'a', 'b', 'c'}}),
            GetItemContent(StorageType::TARGET, {"item_1"}, app_1->id(),
                           migrator_crypto_key()));
}

// Tests storage state after the final step of an item migration - deletion
// from source storage.
TEST_F(LockScreenValueStoreMigratorImplTest,
       StepByStepMigrationDeleteSourceItem) {
  auto app_1 = AddTestExtension(kFirstExtensionId);
  auto app_2 = AddTestExtension(kSecondExtensionId);

  ASSERT_TRUE(InitializeStorage(
      StorageType::SOURCE,
      {{"item_1", app_1->id(), migrator_crypto_key(), {'a', 'b', 'c'}},
       {"item_3", app_2->id(), migrator_crypto_key(), {'e'}}}));

  RunMigrator({app_1->id()});
  EXPECT_TRUE(migrator()->IsMigratingExtensionData(app_1->id()));
  EXPECT_FALSE(migrator()->IsMigratingExtensionData(app_2->id()));
  EXPECT_TRUE(migrated_extension_ids().empty());

  // Get registered items in source.
  RunTaskRunnerTasks();
  // Read migrated item contents in source.
  RunTaskRunnerTasks();
  // Register migrated item in the target.
  RunTaskRunnerTasks();
  // Copy item data to target.
  RunTaskRunnerTasks();

  EXPECT_TRUE(migrator()->IsMigratingExtensionData(app_1->id()));
  EXPECT_FALSE(migrator()->IsMigratingExtensionData(app_2->id()));
  EXPECT_TRUE(migrated_extension_ids().empty());

  // Run task runner - fifth set of operations should delete the item from the
  // source storage. Given that there are no items to migrate left, migrator
  // should report extension data as migrated.
  RunTaskRunnerTasks();
  EXPECT_FALSE(migrator()->IsMigratingExtensionData(app_1->id()));
  EXPECT_FALSE(migrator()->IsMigratingExtensionData(app_2->id()));
  EXPECT_EQ(std::vector<ExtensionId>({app_1->id()}), migrated_extension_ids());

  EXPECT_TRUE(GetRegisteredItemIds(StorageType::SOURCE, app_1->id()).empty());
  EXPECT_EQ(std::set<std::string>({"item_1"}),
            GetRegisteredItemIds(StorageType::TARGET, app_1->id()));
  EXPECT_EQ(std::vector<std::vector<char>>({{'a', 'b', 'c'}}),
            GetItemContent(StorageType::TARGET, {"item_1"}, app_1->id(),
                           migrator_crypto_key()));
}

TEST_F(LockScreenValueStoreMigratorImplTest,
       MigratorDeletedWhenGettingItemsFromSource) {
  auto app = AddTestExtension(kFirstExtensionId);

  ASSERT_TRUE(InitializeStorage(
      StorageType::SOURCE,
      {{"item_1", app->id(), migrator_crypto_key(), {'a', 'b', 'c'}}}));

  RunMigrator({app->id()});

  DeleteMigrator();

  // Get registered items in source.
  RunTaskRunnerTasks();
  EXPECT_TRUE(migrated_extension_ids().empty());

  EXPECT_EQ(std::set<std::string>({"item_1"}),
            GetRegisteredItemIds(StorageType::SOURCE, app->id()));
  EXPECT_EQ(std::vector<std::vector<char>>({{'a', 'b', 'c'}}),
            GetItemContent(StorageType::SOURCE, {"item_1"}, app->id(),
                           migrator_crypto_key()));

  EXPECT_TRUE(GetRegisteredItemIds(StorageType::TARGET, app->id()).empty());
}

TEST_F(LockScreenValueStoreMigratorImplTest,
       MigratorDeletedWhenReadingItemsFromSource) {
  auto app = AddTestExtension(kFirstExtensionId);

  ASSERT_TRUE(InitializeStorage(
      StorageType::SOURCE,
      {{"item_1", app->id(), migrator_crypto_key(), {'a', 'b', 'c'}}}));

  RunMigrator({app->id()});

  // Get registered items in source.
  RunTaskRunnerTasks();
  DeleteMigrator();

  // Get registered items in source.
  RunTaskRunnerTasks();
  EXPECT_TRUE(migrated_extension_ids().empty());

  EXPECT_EQ(std::set<std::string>({"item_1"}),
            GetRegisteredItemIds(StorageType::SOURCE, app->id()));
  EXPECT_EQ(std::vector<std::vector<char>>({{'a', 'b', 'c'}}),
            GetItemContent(StorageType::SOURCE, {"item_1"}, app->id(),
                           migrator_crypto_key()));

  EXPECT_TRUE(GetRegisteredItemIds(StorageType::TARGET, app->id()).empty());
}

TEST_F(LockScreenValueStoreMigratorImplTest,
       MigratorDeletedWhenDeletingItemsFromSource) {
  auto app = AddTestExtension(kFirstExtensionId);

  ASSERT_TRUE(InitializeStorage(
      StorageType::SOURCE,
      {{"item_1", app->id(), migrator_crypto_key(), {'a', 'b', 'c'}}}));

  RunMigrator({app->id()});

  // Get registered items in source.
  RunTaskRunnerTasks();
  // Read migrated item contents in source.
  RunTaskRunnerTasks();
  // Register migrated item in the target.
  RunTaskRunnerTasks();
  // Copy item data to target.
  RunTaskRunnerTasks();
  DeleteMigrator();

  // Delete items from source.
  RunTaskRunnerTasks();
  // While the extension data has been moved at this point, the migrator was
  // deleted before it could send out notification about migration completion
  // to observers.
  EXPECT_TRUE(migrated_extension_ids().empty());

  EXPECT_TRUE(GetRegisteredItemIds(StorageType::SOURCE, app->id()).empty());
  EXPECT_EQ(std::set<std::string>({"item_1"}),
            GetRegisteredItemIds(StorageType::TARGET, app->id()));
  EXPECT_EQ(std::vector<std::vector<char>>({{'a', 'b', 'c'}}),
            GetItemContent(StorageType::TARGET, {"item_1"}, app->id(),
                           migrator_crypto_key()));
}

TEST_F(LockScreenValueStoreMigratorImplTest,
       ClearExtensionDataDuringGetRegisteredItemsFromSource) {
  auto app_1 = AddTestExtension(kFirstExtensionId);
  auto app_2 = AddTestExtension(kSecondExtensionId);

  ASSERT_TRUE(InitializeStorage(
      StorageType::SOURCE,
      {{"item_1", app_1->id(), migrator_crypto_key(), {'a', 'b', 'c'}},
       {"item_2", app_1->id(), migrator_crypto_key(), {'d', 'e'}},
       {"item_3", app_2->id(), migrator_crypto_key(), {'f', 'g'}},
       {"item_4", app_2->id(), migrator_crypto_key(), {'h'}}}));

  ASSERT_TRUE(InitializeStorage(
      StorageType::TARGET,
      {{"item_0", app_1->id(), migrator_crypto_key(), {'a', 'b', 'c', 'd'}}}));

  RunMigrator({app_1->id(), app_2->id()});

  // Clear data for app 1.
  base::RunLoop run_loop;
  migrator()->ClearDataForExtension(app_1->id(), run_loop.QuitClosure());
  EXPECT_FALSE(migrator()->IsMigratingExtensionData(app_1->id()));
  EXPECT_TRUE(migrator()->IsMigratingExtensionData(app_2->id()));

  run_loop.Run();

  // Verify there are no registered items left.
  EXPECT_TRUE(GetRegisteredItemIds(StorageType::SOURCE, app_1->id()).empty());
  EXPECT_TRUE(GetRegisteredItemIds(StorageType::TARGET, app_1->id()).empty());

  // Finish migration for app 2, and verify that it's not affected by clearing
  // of app 1 data.
  WaitMigrationDone(app_2->id());

  EXPECT_EQ(std::vector<ExtensionId>({app_2->id()}), migrated_extension_ids());

  EXPECT_TRUE(GetRegisteredItemIds(StorageType::SOURCE, app_2->id()).empty());
  EXPECT_EQ(std::set<std::string>({"item_3", "item_4"}),
            GetRegisteredItemIds(StorageType::TARGET, app_2->id()));
  EXPECT_EQ(std::vector<std::vector<char>>({{'f', 'g'}, {'h'}}),
            GetItemContent(StorageType::TARGET, {"item_3", "item_4"},
                           app_2->id(), migrator_crypto_key()));
}

TEST_F(LockScreenValueStoreMigratorImplTest,
       ClearExtensionDataDuringReadItemFromSource) {
  auto app_1 = AddTestExtension(kFirstExtensionId);
  auto app_2 = AddTestExtension(kSecondExtensionId);

  ASSERT_TRUE(InitializeStorage(
      StorageType::SOURCE,
      {{"item_1", app_1->id(), migrator_crypto_key(), {'a', 'b', 'c'}},
       {"item_2", app_1->id(), migrator_crypto_key(), {'d', 'e'}},
       {"item_3", app_2->id(), migrator_crypto_key(), {'f', 'g'}},
       {"item_4", app_2->id(), migrator_crypto_key(), {'h'}}}));

  ASSERT_TRUE(InitializeStorage(
      StorageType::TARGET,
      {{"item_0", app_1->id(), migrator_crypto_key(), {'a', 'b', 'c', 'd'}}}));

  RunMigrator({app_1->id(), app_2->id()});

  // Get registered items in source.
  RunTaskRunnerTasks();

  // Clear data for app 1.
  base::RunLoop run_loop;
  migrator()->ClearDataForExtension(app_1->id(), run_loop.QuitClosure());
  EXPECT_FALSE(migrator()->IsMigratingExtensionData(app_1->id()));
  EXPECT_TRUE(migrator()->IsMigratingExtensionData(app_2->id()));

  run_loop.Run();

  // Verify there are no registered items left.
  EXPECT_TRUE(GetRegisteredItemIds(StorageType::SOURCE, app_1->id()).empty());
  EXPECT_TRUE(GetRegisteredItemIds(StorageType::TARGET, app_1->id()).empty());

  // Finish migration for app 2, and verify that it's not affected by clearing
  // of app 1 data.
  WaitMigrationDone(app_2->id());

  EXPECT_EQ(std::vector<ExtensionId>({app_2->id()}), migrated_extension_ids());

  EXPECT_TRUE(GetRegisteredItemIds(StorageType::SOURCE, app_2->id()).empty());
  EXPECT_EQ(std::set<std::string>({"item_3", "item_4"}),
            GetRegisteredItemIds(StorageType::TARGET, app_2->id()));
  EXPECT_EQ(std::vector<std::vector<char>>({{'f', 'g'}, {'h'}}),
            GetItemContent(StorageType::TARGET, {"item_3", "item_4"},
                           app_2->id(), migrator_crypto_key()));
}

TEST_F(LockScreenValueStoreMigratorImplTest,
       ClearExtensionDataDuringRegisterItemInTarget) {
  auto app_1 = AddTestExtension(kFirstExtensionId);
  auto app_2 = AddTestExtension(kSecondExtensionId);

  ASSERT_TRUE(InitializeStorage(
      StorageType::SOURCE,
      {{"item_1", app_1->id(), migrator_crypto_key(), {'a', 'b', 'c'}},
       {"item_2", app_1->id(), migrator_crypto_key(), {'d', 'e'}},
       {"item_3", app_2->id(), migrator_crypto_key(), {'f', 'g'}},
       {"item_4", app_2->id(), migrator_crypto_key(), {'h'}}}));

  ASSERT_TRUE(InitializeStorage(
      StorageType::TARGET,
      {{"item_0", app_1->id(), migrator_crypto_key(), {'a', 'b', 'c', 'd'}}}));

  RunMigrator({app_1->id(), app_2->id()});

  // Get registered items in source.
  RunTaskRunnerTasks();
  // Read migrated item contents in source.
  RunTaskRunnerTasks();

  // Clear data for app 1.
  base::RunLoop run_loop;
  migrator()->ClearDataForExtension(app_1->id(), run_loop.QuitClosure());
  EXPECT_FALSE(migrator()->IsMigratingExtensionData(app_1->id()));
  EXPECT_TRUE(migrator()->IsMigratingExtensionData(app_2->id()));
  run_loop.Run();

  // Verify there are no registered items left.
  EXPECT_TRUE(GetRegisteredItemIds(StorageType::SOURCE, app_1->id()).empty());
  EXPECT_TRUE(GetRegisteredItemIds(StorageType::TARGET, app_1->id()).empty());

  // Finish migration for app 2, and verify that it's not affected by clearing
  // of app 1 data.
  WaitMigrationDone(app_2->id());

  EXPECT_EQ(std::vector<ExtensionId>({app_2->id()}), migrated_extension_ids());

  EXPECT_TRUE(GetRegisteredItemIds(StorageType::SOURCE, app_2->id()).empty());
  EXPECT_EQ(std::set<std::string>({"item_3", "item_4"}),
            GetRegisteredItemIds(StorageType::TARGET, app_2->id()));
  EXPECT_EQ(std::vector<std::vector<char>>({{'f', 'g'}, {'h'}}),
            GetItemContent(StorageType::TARGET, {"item_3", "item_4"},
                           app_2->id(), migrator_crypto_key()));
}

TEST_F(LockScreenValueStoreMigratorImplTest,
       ClearExtensionDataDuringWriteItemInTarget) {
  auto app_1 = AddTestExtension(kFirstExtensionId);
  auto app_2 = AddTestExtension(kSecondExtensionId);

  ASSERT_TRUE(InitializeStorage(
      StorageType::SOURCE,
      {{"item_1", app_1->id(), migrator_crypto_key(), {'a', 'b', 'c'}},
       {"item_2", app_1->id(), migrator_crypto_key(), {'d', 'e'}},
       {"item_3", app_2->id(), migrator_crypto_key(), {'f', 'g'}},
       {"item_4", app_2->id(), migrator_crypto_key(), {'h'}}}));

  ASSERT_TRUE(InitializeStorage(
      StorageType::TARGET,
      {{"item_0", app_1->id(), migrator_crypto_key(), {'a', 'b', 'c', 'd'}}}));

  RunMigrator({app_1->id(), app_2->id()});

  // Get registered items in source.
  RunTaskRunnerTasks();
  // Read migrated item contents in source.
  RunTaskRunnerTasks();
  // Register migrated item in the target.
  RunTaskRunnerTasks();

  // Clear data for app 1.
  base::RunLoop run_loop;
  migrator()->ClearDataForExtension(app_1->id(), run_loop.QuitClosure());
  EXPECT_FALSE(migrator()->IsMigratingExtensionData(app_1->id()));
  EXPECT_TRUE(migrator()->IsMigratingExtensionData(app_2->id()));
  run_loop.Run();

  // Verify there are no registered items left.
  EXPECT_TRUE(GetRegisteredItemIds(StorageType::SOURCE, app_1->id()).empty());
  EXPECT_TRUE(GetRegisteredItemIds(StorageType::TARGET, app_1->id()).empty());

  // Finish migration for app 2, and verify that it's not affected by clearing
  // of app 1 data.
  WaitMigrationDone(app_2->id());

  EXPECT_EQ(std::vector<ExtensionId>({app_2->id()}), migrated_extension_ids());

  EXPECT_TRUE(GetRegisteredItemIds(StorageType::SOURCE, app_2->id()).empty());
  EXPECT_EQ(std::set<std::string>({"item_3", "item_4"}),
            GetRegisteredItemIds(StorageType::TARGET, app_2->id()));
  EXPECT_EQ(std::vector<std::vector<char>>({{'f', 'g'}, {'h'}}),
            GetItemContent(StorageType::TARGET, {"item_3", "item_4"},
                           app_2->id(), migrator_crypto_key()));
}

TEST_F(LockScreenValueStoreMigratorImplTest,
       ClearExtensionDataDuringDeleteItemFromSource) {
  auto app_1 = AddTestExtension(kFirstExtensionId);
  auto app_2 = AddTestExtension(kSecondExtensionId);

  ASSERT_TRUE(InitializeStorage(
      StorageType::SOURCE,
      {{"item_1", app_1->id(), migrator_crypto_key(), {'a', 'b', 'c'}},
       {"item_2", app_1->id(), migrator_crypto_key(), {'d', 'e'}},
       {"item_3", app_2->id(), migrator_crypto_key(), {'f', 'g'}},
       {"item_4", app_2->id(), migrator_crypto_key(), {'h'}}}));

  ASSERT_TRUE(InitializeStorage(
      StorageType::TARGET,
      {{"item_0", app_1->id(), migrator_crypto_key(), {'a', 'b', 'c', 'd'}}}));

  RunMigrator({app_1->id(), app_2->id()});

  // Get registered items in source.
  RunTaskRunnerTasks();
  // Read migrated item contents in source.
  RunTaskRunnerTasks();
  // Register migrated item in the target.
  RunTaskRunnerTasks();
  // Copy item data to target.
  RunTaskRunnerTasks();

  // Clear data for app 1.
  base::RunLoop run_loop;
  migrator()->ClearDataForExtension(app_1->id(), run_loop.QuitClosure());
  EXPECT_FALSE(migrator()->IsMigratingExtensionData(app_1->id()));
  EXPECT_TRUE(migrator()->IsMigratingExtensionData(app_2->id()));
  run_loop.Run();

  // Verify there are no registered items left.
  EXPECT_TRUE(GetRegisteredItemIds(StorageType::SOURCE, app_1->id()).empty());
  EXPECT_TRUE(GetRegisteredItemIds(StorageType::TARGET, app_1->id()).empty());

  // Finish migration for app 2, and verify that it's not affected by clearing
  // of app 1 data.
  WaitMigrationDone(app_2->id());

  EXPECT_EQ(std::vector<ExtensionId>({app_2->id()}), migrated_extension_ids());

  EXPECT_TRUE(GetRegisteredItemIds(StorageType::SOURCE, app_2->id()).empty());
  EXPECT_EQ(std::set<std::string>({"item_3", "item_4"}),
            GetRegisteredItemIds(StorageType::TARGET, app_2->id()));
  EXPECT_EQ(std::vector<std::vector<char>>({{'f', 'g'}, {'h'}}),
            GetItemContent(StorageType::TARGET, {"item_3", "item_4"},
                           app_2->id(), migrator_crypto_key()));
}

TEST_F(LockScreenValueStoreMigratorImplTest,
       MigratorDeletedWhileClearingDataFromTargetStorage) {
  auto app = AddTestExtension(kFirstExtensionId);

  ASSERT_TRUE(InitializeStorage(
      StorageType::SOURCE,
      {{"item_1", app->id(), migrator_crypto_key(), {'a', 'b', 'c'}},
       {"item_2", app->id(), migrator_crypto_key(), {'d', 'e'}}}));

  ASSERT_TRUE(InitializeStorage(
      StorageType::TARGET,
      {{"item_0", app->id(), migrator_crypto_key(), {'a', 'b', 'c', 'd'}}}));

  RunMigrator({app->id()});

  // Get registered items in source.
  RunTaskRunnerTasks();
  // Read migrated item contents in source.
  RunTaskRunnerTasks();
  // Register migrated item in the target.
  RunTaskRunnerTasks();

  // Clear data for app 1.
  migrator()->ClearDataForExtension(
      app->id(), base::Bind(&ExpectNotRun, "clear data callback"));
  EXPECT_FALSE(migrator()->IsMigratingExtensionData(app->id()));

  DeleteMigrator();

  // Run any tasks left over on the task runner.
  RunTaskRunnerTasks();

  EXPECT_TRUE(migrated_extension_ids().empty());
  EXPECT_TRUE(GetRegisteredItemIds(StorageType::TARGET, app->id()).empty());
}

TEST_F(LockScreenValueStoreMigratorImplTest,
       MigratorDeletedWhileClearingDataFromSourceStorage) {
  auto app = AddTestExtension(kFirstExtensionId);

  ASSERT_TRUE(InitializeStorage(
      StorageType::SOURCE,
      {{"item_1", app->id(), migrator_crypto_key(), {'a', 'b', 'c'}},
       {"item_2", app->id(), migrator_crypto_key(), {'d', 'e'}}}));

  ASSERT_TRUE(InitializeStorage(
      StorageType::TARGET,
      {{"item_0", app->id(), migrator_crypto_key(), {'a', 'b', 'c', 'd'}}}));

  RunMigrator({app->id()});

  // Get registered items in source.
  RunTaskRunnerTasks();
  // Read migrated item contents in source.
  RunTaskRunnerTasks();
  // Register migrated item in the target.
  RunTaskRunnerTasks();

  // Clear data for app 1.
  migrator()->ClearDataForExtension(
      app->id(), base::Bind(&ExpectNotRun, "clear data callback"));
  EXPECT_FALSE(migrator()->IsMigratingExtensionData(app->id()));

  // This should clear the target storage.
  RunTaskRunnerTasks();
  DeleteMigrator();

  // Run any tasks left over on the task runner.
  RunTaskRunnerTasks();

  EXPECT_TRUE(migrated_extension_ids().empty());

  EXPECT_TRUE(GetRegisteredItemIds(StorageType::SOURCE, app->id()).empty());
  EXPECT_TRUE(GetRegisteredItemIds(StorageType::TARGET, app->id()).empty());
}

TEST_F(LockScreenValueStoreMigratorImplTest, FailToGetItemsFromSource) {
  auto app_1 = AddTestExtension(kFirstExtensionId);
  auto app_2 = AddTestExtension(kSecondExtensionId);

  ASSERT_TRUE(InitializeStorage(
      StorageType::SOURCE,
      {{"item_1", app_1->id(), migrator_crypto_key(), {'a', 'b', 'c'}},
       {"item_2", app_1->id(), migrator_crypto_key(), {'d', 'e'}},
       {"item_3", app_2->id(), migrator_crypto_key(), {'f', 'g'}},
       {"item_4", app_2->id(), migrator_crypto_key(), {'h'}}}));

  ASSERT_TRUE(InitializeStorage(
      StorageType::TARGET,
      {{"item_0", app_1->id(), migrator_crypto_key(), {'a', 'b', 'c', 'd'}}}));

  SetReturnCodeForValueStoreOperations(StorageType::SOURCE, app_1->id(),
                                       ValueStore::OTHER_ERROR);

  RunMigrator({app_1->id(), app_2->id()});

  WaitMigrationDone(app_1->id());
  WaitMigrationDone(app_2->id());

  EXPECT_EQ(std::set<std::string>({"item_0"}),
            GetRegisteredItemIds(StorageType::TARGET, app_1->id()));
  EXPECT_EQ(std::vector<std::vector<char>>({{'a', 'b', 'c', 'd'}}),
            GetItemContent(StorageType::TARGET, {"item_0"}, app_1->id(),
                           migrator_crypto_key()));

  EXPECT_TRUE(GetRegisteredItemIds(StorageType::SOURCE, app_2->id()).empty());
  EXPECT_EQ(std::set<std::string>({"item_3", "item_4"}),
            GetRegisteredItemIds(StorageType::TARGET, app_2->id()));
  EXPECT_EQ(std::vector<std::vector<char>>({{'f', 'g'}, {'h'}}),
            GetItemContent(StorageType::TARGET, {"item_3", "item_4"},
                           app_2->id(), migrator_crypto_key()));
}

TEST_F(LockScreenValueStoreMigratorImplTest, FailToReadItemFromSource) {
  auto app_1 = AddTestExtension(kFirstExtensionId);
  auto app_2 = AddTestExtension(kSecondExtensionId);

  ASSERT_TRUE(InitializeStorage(
      StorageType::SOURCE,
      {{"item_1", app_1->id(), migrator_crypto_key(), {'a', 'b', 'c'}},
       {"item_2", app_1->id(), migrator_crypto_key(), {'d', 'e'}},
       {"item_3", app_2->id(), migrator_crypto_key(), {'f', 'g'}},
       {"item_4", app_2->id(), migrator_crypto_key(), {'h'}}}));

  ASSERT_TRUE(InitializeStorage(
      StorageType::TARGET,
      {{"item_0", app_1->id(), migrator_crypto_key(), {'a', 'b', 'c', 'd'}}}));

  RunMigrator({app_1->id(), app_2->id()});

  // Make read for the first app_1 item fail.
  SetReturnCodeForValueStoreOperations(StorageType::SOURCE, app_1->id(),
                                       ValueStore::OTHER_ERROR);
  // Run tasks that get registered items.
  RunTaskRunnerTasks();
  // Make read for the rest of app_1 items succeed.
  SetReturnCodeForValueStoreOperations(StorageType::SOURCE, app_1->id(),
                                       ValueStore::OK);

  // Run tasks that read item contents.
  RunTaskRunnerTasks();

  WaitMigrationDone(app_1->id());
  WaitMigrationDone(app_2->id());

  EXPECT_TRUE(GetRegisteredItemIds(StorageType::SOURCE, app_1->id()).empty());
  std::set<std::string> items =
      GetRegisteredItemIds(StorageType::TARGET, app_1->id());
  EXPECT_EQ(2u, items.size());
  if (items.count("item_1")) {
    EXPECT_EQ(
        std::vector<std::vector<char>>({{'a', 'b', 'c'}, {'a', 'b', 'c', 'd'}}),
        GetItemContent(StorageType::TARGET, {"item_1", "item_0"}, app_1->id(),
                       migrator_crypto_key()));
  } else if (items.count("item_2")) {
    EXPECT_EQ(
        std::vector<std::vector<char>>({{'d', 'e'}, {'a', 'b', 'c', 'd'}}),
        GetItemContent(StorageType::TARGET, {"item_2", "item_0"}, app_1->id(),
                       migrator_crypto_key()));
  } else {
    ADD_FAILURE() << "Neither of items migrated";
  }

  EXPECT_TRUE(GetRegisteredItemIds(StorageType::SOURCE, app_2->id()).empty());
  EXPECT_EQ(std::set<std::string>({"item_3", "item_4"}),
            GetRegisteredItemIds(StorageType::TARGET, app_2->id()));
  EXPECT_EQ(std::vector<std::vector<char>>({{'f', 'g'}, {'h'}}),
            GetItemContent(StorageType::TARGET, {"item_3", "item_4"},
                           app_2->id(), migrator_crypto_key()));
}

TEST_F(LockScreenValueStoreMigratorImplTest, FailToRegisterItemInTarget) {
  auto app_1 = AddTestExtension(kFirstExtensionId);
  auto app_2 = AddTestExtension(kSecondExtensionId);

  ASSERT_TRUE(InitializeStorage(
      StorageType::SOURCE,
      {{"item_1", app_1->id(), migrator_crypto_key(), {'a', 'b', 'c'}},
       {"item_2", app_1->id(), migrator_crypto_key(), {'d', 'e'}},
       {"item_3", app_2->id(), migrator_crypto_key(), {'f', 'g'}},
       {"item_4", app_2->id(), migrator_crypto_key(), {'h'}}}));

  ASSERT_TRUE(InitializeStorage(
      StorageType::TARGET,
      {{"item_0", app_1->id(), migrator_crypto_key(), {'a', 'b', 'c', 'd'}}}));

  RunMigrator({app_1->id(), app_2->id()});

  // Run tasks that get registered items.
  RunTaskRunnerTasks();

  // Make registration for the first app_1 item fail.
  SetReturnCodeForValueStoreOperations(StorageType::TARGET, app_1->id(),
                                       ValueStore::OTHER_ERROR);
  // Run tasks that read registered items.
  RunTaskRunnerTasks();
  // Make read for the rest of app_1 items succeed.
  SetReturnCodeForValueStoreOperations(StorageType::TARGET, app_1->id(),
                                       ValueStore::OK);

  WaitMigrationDone(app_1->id());
  WaitMigrationDone(app_2->id());

  EXPECT_TRUE(GetRegisteredItemIds(StorageType::SOURCE, app_1->id()).empty());
  std::set<std::string> items =
      GetRegisteredItemIds(StorageType::TARGET, app_1->id());
  EXPECT_EQ(2u, items.size());
  if (items.count("item_1")) {
    EXPECT_EQ(
        std::vector<std::vector<char>>({{'a', 'b', 'c'}, {'a', 'b', 'c', 'd'}}),
        GetItemContent(StorageType::TARGET, {"item_1", "item_0"}, app_1->id(),
                       migrator_crypto_key()));
  } else if (items.count("item_2")) {
    EXPECT_EQ(
        std::vector<std::vector<char>>({{'d', 'e'}, {'a', 'b', 'c', 'd'}}),
        GetItemContent(StorageType::TARGET, {"item_2", "item_0"}, app_1->id(),
                       migrator_crypto_key()));
  } else {
    ADD_FAILURE() << "Neither of items migrated";
  }

  EXPECT_TRUE(GetRegisteredItemIds(StorageType::SOURCE, app_2->id()).empty());
  EXPECT_EQ(std::set<std::string>({"item_3", "item_4"}),
            GetRegisteredItemIds(StorageType::TARGET, app_2->id()));
  EXPECT_EQ(std::vector<std::vector<char>>({{'f', 'g'}, {'h'}}),
            GetItemContent(StorageType::TARGET, {"item_3", "item_4"},
                           app_2->id(), migrator_crypto_key()));
}

TEST_F(LockScreenValueStoreMigratorImplTest, FailToWriteItemContentsToTarget) {
  auto app_1 = AddTestExtension(kFirstExtensionId);
  auto app_2 = AddTestExtension(kSecondExtensionId);

  ASSERT_TRUE(InitializeStorage(
      StorageType::SOURCE,
      {{"item_1", app_1->id(), migrator_crypto_key(), {'a', 'b', 'c'}},
       {"item_2", app_1->id(), migrator_crypto_key(), {'d', 'e'}},
       {"item_3", app_2->id(), migrator_crypto_key(), {'f', 'g'}},
       {"item_4", app_2->id(), migrator_crypto_key(), {'h'}}}));

  ASSERT_TRUE(InitializeStorage(
      StorageType::TARGET,
      {{"item_0", app_1->id(), migrator_crypto_key(), {'a', 'b', 'c', 'd'}}}));

  RunMigrator({app_1->id(), app_2->id()});

  // Run tasks that get registered items.
  RunTaskRunnerTasks();
  // Run tasks that read registered items.
  RunTaskRunnerTasks();

  // Make write for the first app_1 item fail.
  SetReturnCodeForValueStoreOperations(StorageType::TARGET, app_1->id(),
                                       ValueStore::OTHER_ERROR);
  // Run tasks that register new items in target.
  RunTaskRunnerTasks();
  // Make read for the rest of app_1 items succeed.
  SetReturnCodeForValueStoreOperations(StorageType::TARGET, app_1->id(),
                                       ValueStore::OK);

  WaitMigrationDone(app_1->id());
  WaitMigrationDone(app_2->id());

  EXPECT_TRUE(GetRegisteredItemIds(StorageType::SOURCE, app_1->id()).empty());
  std::set<std::string> items =
      GetRegisteredItemIds(StorageType::TARGET, app_1->id());
  EXPECT_EQ(2u, items.size());
  if (items.count("item_1")) {
    EXPECT_EQ(
        std::vector<std::vector<char>>({{'a', 'b', 'c'}, {'a', 'b', 'c', 'd'}}),
        GetItemContent(StorageType::TARGET, {"item_1", "item_0"}, app_1->id(),
                       migrator_crypto_key()));
  } else if (items.count("item_2")) {
    EXPECT_EQ(
        std::vector<std::vector<char>>({{'d', 'e'}, {'a', 'b', 'c', 'd'}}),
        GetItemContent(StorageType::TARGET, {"item_2", "item_0"}, app_1->id(),
                       migrator_crypto_key()));
  } else {
    ADD_FAILURE() << "Neither of items migrated";
  }

  EXPECT_TRUE(GetRegisteredItemIds(StorageType::SOURCE, app_2->id()).empty());
  EXPECT_EQ(std::set<std::string>({"item_3", "item_4"}),
            GetRegisteredItemIds(StorageType::TARGET, app_2->id()));
  EXPECT_EQ(std::vector<std::vector<char>>({{'f', 'g'}, {'h'}}),
            GetItemContent(StorageType::TARGET, {"item_3", "item_4"},
                           app_2->id(), migrator_crypto_key()));
}

}  // namespace lock_screen_data
}  // namespace extensions
