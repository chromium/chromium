// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/lock_screen_data/lock_screen_item_storage.h"

#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/login/login_state/login_state.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/test/test_browser_context.h"
#include "extensions/browser/api/lock_screen_data/data_item.h"
#include "extensions/browser/api/lock_screen_data/lock_screen_value_store_migrator.h"
#include "extensions/browser/api/lock_screen_data/operation_result.h"
#include "extensions/browser/api/storage/local_value_store_cache.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/test_extensions_browser_client.h"
#include "extensions/browser/value_store/test_value_store_factory.h"
#include "extensions/common/api/lock_screen_data.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace lock_screen_data {

namespace {

constexpr char kTestUserIdHash[] = "user_id_hash";
constexpr char kTestSymmetricKey[] = "fake_symmetric_key";

constexpr char kTestExtensionId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
constexpr char kSecondTestExtensionId[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";

void RecordCreateResult(OperationResult* result_out,
                        const DataItem** item_out,
                        OperationResult result,
                        const DataItem* item) {
  *result_out = result;
  *item_out = item;
}

void RecordGetAllItemsResult(std::vector<std::string>* items_out,
                             const std::vector<const DataItem*>& items) {
  items_out->clear();
  for (const DataItem* item : items)
    items_out->push_back(item->id());
}

void RecordWriteResult(OperationResult* result_out, OperationResult result) {
  *result_out = result;
}

void RecordReadResult(OperationResult* result_out,
                      std::unique_ptr<std::vector<char>>* content_out,
                      OperationResult result,
                      std::unique_ptr<std::vector<char>> content) {
  *result_out = result;
  *content_out = std::move(content);
}

class TestEventRouter : public extensions::EventRouter {
 public:
  explicit TestEventRouter(content::BrowserContext* context)
      : extensions::EventRouter(context, nullptr) {}
  ~TestEventRouter() override = default;

  bool ExtensionHasEventListener(const std::string& extension_id,
                                 const std::string& event_name) const override {
    return event_name ==
           extensions::api::lock_screen_data::OnDataItemsAvailable::kEventName;
  }

  void BroadcastEvent(std::unique_ptr<extensions::Event> event) override {}

  void DispatchEventToExtension(
      const std::string& extension_id,
      std::unique_ptr<extensions::Event> event) override {
    if (event->event_name !=
        extensions::api::lock_screen_data::OnDataItemsAvailable::kEventName) {
      return;
    }
    ASSERT_TRUE(event->event_args);
    const base::Value* arg_value = nullptr;
    ASSERT_TRUE(event->event_args->Get(0, &arg_value));
    ASSERT_TRUE(arg_value);

    std::unique_ptr<extensions::api::lock_screen_data::DataItemsAvailableEvent>
        event_args = extensions::api::lock_screen_data::
            DataItemsAvailableEvent::FromValue(*arg_value);
    ASSERT_TRUE(event_args);
    was_locked_values_.push_back(event_args->was_locked);
  }

  const std::vector<bool>& was_locked_values() const {
    return was_locked_values_;
  }

  void ClearWasLockedValues() { was_locked_values_.clear(); }

 private:
  std::vector<bool> was_locked_values_;

  DISALLOW_COPY_AND_ASSIGN(TestEventRouter);
};

std::unique_ptr<KeyedService> TestEventRouterFactoryFunction(
    content::BrowserContext* context) {
  return std::make_unique<TestEventRouter>(context);
}

// Keeps track of all fake data items registered during a test.
class ItemRegistry {
 public:
  explicit ItemRegistry(const std::string& extension_id)
      : extension_id_(extension_id) {}
  ~ItemRegistry() = default;

  // Adds a new item to set of registered items.
  bool Add(const std::string& item_id) {
    EXPECT_FALSE(items_.count(item_id));

    if (!allow_new_)
      return false;
    items_.insert(item_id);
    return true;
  }

  // Removes an item from the set of registered items.
  void Remove(const std::string& item_id) {
    ASSERT_TRUE(items_.count(item_id));
    items_.erase(item_id);
  }

  void RemoveAll() { items_.clear(); }

  // Gets the set of registered data items.
  void HandleGetRequest(const DataItem::RegisteredValuesCallback& callback) {
    if (!throttle_get_) {
      RunCallback(callback);
      return;
    }

    ASSERT_TRUE(pending_callback_.is_null());
    pending_callback_ = callback;
  }

  // Completes a pending |HandleGetRequest| request.
  void RunPendingCallback() {
    ASSERT_FALSE(pending_callback_.is_null());
    DataItem::RegisteredValuesCallback callback = pending_callback_;
    pending_callback_.Reset();
    RunCallback(callback);
  }

  bool HasPendingCallback() const { return !pending_callback_.is_null(); }

  void set_allow_new(bool allow_new) { allow_new_ = allow_new; }
  void set_fail(bool fail) { fail_ = fail; }
  void set_throttle_get(bool throttle_get) { throttle_get_ = throttle_get; }

 private:
  void RunCallback(const DataItem::RegisteredValuesCallback& callback) {
    callback.Run(fail_ ? OperationResult::kFailed : OperationResult::kSuccess,
                 ItemsToValue());
  }

  std::unique_ptr<base::DictionaryValue> ItemsToValue() {
    if (fail_)
      return nullptr;

    std::unique_ptr<base::DictionaryValue> result =
        std::make_unique<base::DictionaryValue>();

    for (const std::string& item_id : items_)
      result->Set(item_id, std::make_unique<base::DictionaryValue>());

    return result;
  }

  const std::string extension_id_;
  // Whether data item registration should succeed.
  bool allow_new_ = true;
  // Whether data item retrievals should fail.
  bool fail_ = false;
  // Whether the data item retrivals should be throttled. If set,
  // |HandleGetRequest| callback will be saved to |pending_callback_| without
  // returning. Test will have to invoke |RunPendingCallback| in order to
  // complete the request.
  bool throttle_get_ = false;

  DataItem::RegisteredValuesCallback pending_callback_;
  // Set of registered item ids.
  std::set<std::string> items_;

  DISALLOW_COPY_AND_ASSIGN(ItemRegistry);
};

// Keeps track of all operations requested from the test data item.
// The operations will remain in pending state until completed by calling
// CompleteNextOperation.
// This is owned by the test class, but data items created during the test have
// a reference to the object. More than one data item can have a reference to
// this - data items with the same ID will get the same operation queue.
class OperationQueue {
 public:
  enum class OperationType { kWrite, kRead, kDelete };

  struct PendingOperation {
    explicit PendingOperation(OperationType type) : type(type) {}

    OperationType type;
    // Set only for write - data to be written.
    std::vector<char> data;

    // Callback for write operation.
    DataItem::WriteCallback write_callback;

    // Callback for read operation.
    DataItem::ReadCallback read_callback;

    // Callback for delete operation.
    DataItem::WriteCallback delete_callback;
  };

  OperationQueue(const std::string& id, ItemRegistry* item_registry)
      : id_(id), item_registry_(item_registry) {}

  ~OperationQueue() = default;

  void Register(const DataItem::WriteCallback& callback) {
    bool registered = item_registry_->Add(id_);
    callback.Run(registered ? OperationResult::kSuccess
                            : OperationResult::kFailed);
  }

  void AddWrite(const std::vector<char>& data,
                const DataItem::WriteCallback& callback) {
    PendingOperation operation(OperationType::kWrite);
    operation.data = data;
    operation.write_callback = callback;

    pending_operations_.emplace(std::move(operation));
  }

  void AddRead(const DataItem::ReadCallback& callback) {
    PendingOperation operation(OperationType::kRead);
    operation.read_callback = callback;

    pending_operations_.emplace(std::move(operation));
  }

  void AddDelete(const DataItem::WriteCallback& callback) {
    PendingOperation operation(OperationType::kDelete);
    operation.delete_callback = callback;

    pending_operations_.emplace(std::move(operation));
  }

  // Completes the next pendig operation.
  // |expected_type| - the expected type of the next operation - this will fail
  //     if the operation does not match.
  // |result| - the intended operation result.
  void CompleteNextOperation(OperationType expected_type,
                             OperationResult result) {
    ASSERT_FALSE(pending_operations_.empty());
    ASSERT_FALSE(deleted_);

    const PendingOperation& operation = pending_operations_.front();

    ASSERT_EQ(expected_type, operation.type);

    switch (expected_type) {
      case OperationType::kWrite: {
        if (result == OperationResult::kSuccess)
          content_ = operation.data;
        DataItem::WriteCallback callback = operation.write_callback;
        pending_operations_.pop();
        callback.Run(result);
        break;
      }
      case OperationType::kDelete: {
        if (result == OperationResult::kSuccess) {
          deleted_ = true;
          item_registry_->Remove(id_);
          content_ = std::vector<char>();
        }

        DataItem::WriteCallback callback = operation.delete_callback;
        pending_operations_.pop();
        callback.Run(result);
        break;
      }
      case OperationType::kRead: {
        std::unique_ptr<std::vector<char>> result_data;
        if (result == OperationResult::kSuccess) {
          result_data = std::make_unique<std::vector<char>>(content_.begin(),
                                                            content_.end());
        }

        DataItem::ReadCallback callback = operation.read_callback;
        pending_operations_.pop();
        callback.Run(result, std::move(result_data));
        break;
      }
      default:
        ADD_FAILURE() << "Unexpected operation";
        return;
    }
  }

  bool HasPendingOperations() const { return !pending_operations_.empty(); }

  bool deleted() const { return deleted_; }

  const std::vector<char>& content() const { return content_; }

  void set_content(const std::vector<char>& content) { content_ = content; }

 private:
  std::string id_;
  ItemRegistry* item_registry_;
  base::queue<PendingOperation> pending_operations_;
  std::vector<char> content_;
  bool deleted_ = false;

  DISALLOW_COPY_AND_ASSIGN(OperationQueue);
};

// Test data item - routes all requests to the OperationQueue provided through
// the ctor - the owning test is responsible for completing the started
// operations.
class TestDataItem : public DataItem {
 public:
  // |operations| - Operation queue used by this data item - not owned by this,
  // and expected to outlive this object.
  TestDataItem(const std::string& id,
               const std::string& extension_id,
               const std::string& crypto_key,
               OperationQueue* operations)
      : DataItem(id, extension_id, nullptr, nullptr, nullptr, crypto_key),
        operations_(operations) {}

  ~TestDataItem() override = default;

  void Register(const WriteCallback& callback) override {
    operations_->Register(callback);
  }

  void Write(const std::vector<char>& data,
             const WriteCallback& callback) override {
    operations_->AddWrite(data, callback);
  }

  void Read(const ReadCallback& callback) override {
    operations_->AddRead(callback);
  }

  void Delete(const WriteCallback& callback) override {
    operations_->AddDelete(callback);
  }

 private:
  OperationQueue* operations_;

  DISALLOW_COPY_AND_ASSIGN(TestDataItem);
};

class TestLockScreenValueStoreMigrator : public LockScreenValueStoreMigrator {
 public:
  TestLockScreenValueStoreMigrator() = default;
  ~TestLockScreenValueStoreMigrator() override = default;

  void Run(const std::set<ExtensionId>& extensions_to_migrate,
           const ExtensionMigratedCallback& callback) override {
    ASSERT_TRUE(migration_callback_.is_null());
    ASSERT_TRUE(extensions_to_migrate_.empty());

    migration_callback_ = callback;
    extensions_to_migrate_ = extensions_to_migrate;
  }

  bool IsMigratingExtensionData(
      const ExtensionId& extension_id) const override {
    return extensions_to_migrate_.count(extension_id) > 0;
  }

  void ClearDataForExtension(const ExtensionId& extension_id,
                             const base::Closure& callback) override {
    EXPECT_EQ(clear_data_callbacks_.count(extension_id), 0u);
    EXPECT_GT(extensions_to_migrate_.count(extension_id), 0u);

    extensions_to_migrate_.erase(extension_id);
    clear_data_callbacks_[extension_id] = callback;
  }

  bool ClearingDataForExtension(const ExtensionId& extension_id) const {
    return clear_data_callbacks_.count(extension_id) > 0;
  }

  bool FinishMigration(const ExtensionId& extension_id) {
    if (!IsMigratingExtensionData(extension_id))
      return false;
    migration_callback_.Run(extension_id);
    return true;
  }

  bool FinishClearData(const ExtensionId& extension_id) {
    if (clear_data_callbacks_.count(extension_id) == 0)
      return false;

    base::Closure callback = clear_data_callbacks_[extension_id];
    clear_data_callbacks_.erase(extension_id);
    callback.Run();
    return true;
  }

  const std::set<ExtensionId>& extensions_to_migrate() const {
    return extensions_to_migrate_;
  }

 private:
  ExtensionMigratedCallback migration_callback_;
  std::set<ExtensionId> extensions_to_migrate_;
  std::map<ExtensionId, base::Closure> clear_data_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(TestLockScreenValueStoreMigrator);
};

class LockScreenItemStorageTest : public ExtensionsTest {
 public:
  LockScreenItemStorageTest() = default;
  ~LockScreenItemStorageTest() override = default;

  void SetUp() override {
    ExtensionsTest::SetUp();

    ASSERT_TRUE(test_dir_.CreateUniqueTempDir());
    LockScreenItemStorage::RegisterLocalState(local_state_.registry());
    user_prefs::UserPrefs::Set(browser_context(), &testing_pref_service_);
    extensions_browser_client()->set_lock_screen_context(&lock_screen_context_);

    chromeos::LoginState::Initialize();
    chromeos::LoginState::Get()->SetLoggedInStateAndPrimaryUser(
        chromeos::LoginState::LOGGED_IN_ACTIVE,
        chromeos::LoginState::LOGGED_IN_USER_REGULAR, kTestUserIdHash);

    extension_ = CreateTestExtension(kTestExtensionId);
    item_registry_ = std::make_unique<ItemRegistry>(extension()->id());

    // Inject custom data item factory to be used with LockScreenItemStorage
    // instances.
    value_store_cache_factory_ =
        base::Bind(&LockScreenItemStorageTest::CreateValueStoreCache,
                   base::Unretained(this));
    LockScreenItemStorage::SetValueStoreCacheFactoryForTesting(
        &value_store_cache_factory_);

    migrator_factory_ = base::Bind(&LockScreenItemStorageTest::CreateMigrator,
                                   base::Unretained(this));
    LockScreenItemStorage::SetValueStoreMigratorFactoryForTesting(
        &migrator_factory_);

    item_factory_ = base::Bind(&LockScreenItemStorageTest::CreateItem,
                               base::Unretained(this));
    registered_items_getter_ = base::Bind(
        &LockScreenItemStorageTest::GetRegisteredItems, base::Unretained(this));
    item_store_deleter_ = base::Bind(&LockScreenItemStorageTest::RemoveAllItems,
                                     base::Unretained(this));
    LockScreenItemStorage::SetItemProvidersForTesting(
        &registered_items_getter_, &item_factory_, &item_store_deleter_);

    ResetLockScreenItemStorage();
  }

  void TearDown() override {
    lock_screen_item_storage_.reset();
    operations_.clear();
    item_registry_.reset();
    LockScreenItemStorage::SetItemProvidersForTesting(nullptr, nullptr,
                                                      nullptr);
    chromeos::LoginState::Shutdown();
    ExtensionsTest::TearDown();
  }

  OperationQueue* GetOperations(const std::string& id) {
    return operations_[id].get();
  }

  void UnsetLockScreenItemStorage() { lock_screen_item_storage_.reset(); }

  void ResetLockScreenItemStorage() {
    lock_screen_item_storage_.reset();
    value_store_migrator_ = nullptr;
    lock_screen_item_storage_ = std::make_unique<LockScreenItemStorage>(
        browser_context(), &local_state_, kTestSymmetricKey,
        test_dir_.GetPath().AppendASCII("deprecated_value_store"),
        test_dir_.GetPath().AppendASCII("value_store"));
  }

  // Utility method for setting test item content.
  bool SetItemContent(const std::string& id, const std::vector<char>& content) {
    OperationQueue* item_operations = GetOperations(id);
    if (!item_operations) {
      ADD_FAILURE() << "No item operations";
      return false;
    }
    OperationResult write_result = OperationResult::kFailed;
    lock_screen_item_storage()->SetItemContent(
        extension()->id(), id, content,
        base::Bind(&RecordWriteResult, &write_result));
    if (!item_operations->HasPendingOperations()) {
      ADD_FAILURE() << "Write not registered";
      return false;
    }
    item_operations->CompleteNextOperation(
        OperationQueue::OperationType::kWrite, OperationResult::kSuccess);
    EXPECT_EQ(OperationResult::kSuccess, write_result);
    return write_result == OperationResult::kSuccess;
  }

  const DataItem* CreateNewItem() {
    OperationResult create_result = OperationResult::kFailed;
    const DataItem* item = nullptr;
    lock_screen_item_storage()->CreateItem(
        extension()->id(),
        base::Bind(&RecordCreateResult, &create_result, &item));
    EXPECT_EQ(OperationResult::kSuccess, create_result);

    return item;
  }

  // Utility method for creating a new testing data item, and setting its
  // content.
  const DataItem* CreateItemWithContent(const std::vector<char>& content) {
    const DataItem* item = CreateNewItem();
    if (!item) {
      ADD_FAILURE() << "Item creation failed";
      return nullptr;
    }

    if (!SetItemContent(item->id(), content))
      return nullptr;

    return item;
  }

  void GetAllItems(std::vector<std::string>* all_items) {
    lock_screen_item_storage()->GetAllForExtension(
        extension()->id(), base::Bind(&RecordGetAllItemsResult, all_items));
  }

  // Finds an item with the ID |id| in list of items |items|.
  const DataItem* FindItem(const std::string& id,
                           const std::vector<const DataItem*> items) {
    for (const auto* item : items) {
      if (item && item->id() == id)
        return item;
    }
    return nullptr;
  }

  struct ExtensionPersistedState {
    ExtensionId extension_id;
    int storage_version;
    int item_count;
  };

  void InitExtensionLocalState(
      const std::vector<ExtensionPersistedState>& states) {
    for (const auto& state : states) {
      ASSERT_TRUE(state.storage_version == 1 || state.storage_version == 2)
          << "Failed to init local state " << state.extension_id;

      DictionaryPrefUpdate update(&local_state_, "lockScreenDataItems");
      if (state.storage_version == 1) {
        update->SetPath({kTestUserIdHash, state.extension_id},
                        base::Value(state.item_count));
      } else {
        base::Value info(base::Value::Type::DICTIONARY);
        info.SetKey("item_count", base::Value(state.item_count));
        info.SetKey("storage_version", base::Value(2));
        update->SetPath({kTestUserIdHash, state.extension_id}, std::move(info));
      }
    }
  }

  struct MigratedItem {
    std::string id;
    std::vector<char> content;
  };

  bool FinishMigration(const ExtensionId& extension_id,
                       const std::vector<MigratedItem>& items) {
    if (extension_id != extension_->id())
      return false;

    if (!value_store_migrator())
      return false;

    for (const auto& item : items) {
      item_registry_->Add(item.id);
      GetOrCreateOperations(item.id)->set_content(item.content);
    }
    return value_store_migrator()->FinishMigration(extension_id);
  }

  LockScreenItemStorage* lock_screen_item_storage() {
    return lock_screen_item_storage_.get();
  }

  content::BrowserContext* lock_screen_context() {
    return &lock_screen_context_;
  }

  const Extension* extension() const { return extension_.get(); }

  const base::FilePath& test_dir() const { return test_dir_.GetPath(); }

  PrefService* local_state() { return &local_state_; }

  ItemRegistry* item_registry() { return item_registry_.get(); }

  TestLockScreenValueStoreMigrator* value_store_migrator() {
    return value_store_migrator_;
  }

  scoped_refptr<const Extension> CreateTestExtension(
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
    ExtensionRegistry::Get(browser_context())->AddEnabled(extension);
    return extension;
  }

  void set_can_create_deprecated_value_store(bool value) {
    can_create_deprecated_value_store_ = value;
  }

 private:
  OperationQueue* GetOrCreateOperations(const std::string& id) {
    OperationQueue* operation_queue = GetOperations(id);
    if (!operation_queue) {
      operations_[id] =
          std::make_unique<OperationQueue>(id, item_registry_.get());
      operation_queue = operations_[id].get();
    }
    return operation_queue;
  }

  // Callback for creating value store cache - this is the callback passed to
  // LockScreenItemStorage via SetValueStoreCacheFactoryForTesting.
  std::unique_ptr<LocalValueStoreCache> CreateValueStoreCache(
      const base::FilePath& root) {
    std::vector<base::FilePath> allowed_paths = {
        test_dir_.GetPath()
            .AppendASCII("value_store")
            .AppendASCII(kTestUserIdHash)};
    if (can_create_deprecated_value_store_) {
      allowed_paths.push_back(
          test_dir_.GetPath().AppendASCII("deprecated_value_store"));
    }
    EXPECT_TRUE(base::Contains(allowed_paths, root))
        << "Unexpected value store path " << root.value();

    return std::make_unique<LocalValueStoreCache>(
        base::MakeRefCounted<TestValueStoreFactory>());
  }

  // Callback for creating value store migrator - this is the callback passed to
  // LockScreenItemStorage via SetValueStoreMigratorFactoryForTesting.
  std::unique_ptr<LockScreenValueStoreMigrator> CreateMigrator() {
    auto result = std::make_unique<TestLockScreenValueStoreMigrator>();
    value_store_migrator_ = result.get();
    return result;
  }

  // Callback for creating test data items - this is the callback passed to
  // LockScreenItemStorage via SetItemFactoryForTesting.
  std::unique_ptr<DataItem> CreateItem(const std::string& id,
                                       const std::string& extension_id,
                                       const std::string& crypto_key) {
    EXPECT_EQ(extension()->id(), extension_id);
    EXPECT_EQ(kTestSymmetricKey, crypto_key);

    OperationQueue* operation_queue = GetOrCreateOperations(id);
    // If there is an operation queue for the item id, reuse it in order to
    // retain state on LockScreenItemStorage restart.
    return std::make_unique<TestDataItem>(id, extension_id, crypto_key,
                                          operation_queue);
  }

  void GetRegisteredItems(const std::string& extension_id,
                          const DataItem::RegisteredValuesCallback& callback) {
    if (extension()->id() != extension_id) {
      callback.Run(OperationResult::kUnknownExtension, nullptr);
      return;
    }
    item_registry_->HandleGetRequest(callback);
  }

  void RemoveAllItems(const std::string& extension_id,
                      const base::Closure& callback) {
    ASSERT_EQ(extension()->id(), extension_id);
    item_registry_->RemoveAll();
    callback.Run();
  }

  std::unique_ptr<LockScreenItemStorage> lock_screen_item_storage_;

  content::TestBrowserContext lock_screen_context_;
  TestingPrefServiceSimple local_state_;

  base::ScopedTempDir test_dir_;

  sync_preferences::TestingPrefServiceSyncable testing_pref_service_;

  LockScreenItemStorage::ValueStoreCacheFactoryCallback
      value_store_cache_factory_;
  LockScreenItemStorage::ValueStoreMigratorFactoryCallback migrator_factory_;
  LockScreenItemStorage::ItemFactoryCallback item_factory_;
  LockScreenItemStorage::RegisteredItemsGetter registered_items_getter_;
  LockScreenItemStorage::ItemStoreDeleter item_store_deleter_;

  scoped_refptr<const Extension> extension_;

  std::unique_ptr<ItemRegistry> item_registry_;
  std::map<std::string, std::unique_ptr<OperationQueue>> operations_;

  // Whether the test is expected to create deprecated value store version.
  bool can_create_deprecated_value_store_ = false;

  TestLockScreenValueStoreMigrator* value_store_migrator_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(LockScreenItemStorageTest);
};

}  // namespace

TEST_F(LockScreenItemStorageTest, GetDependingOnSessionState) {
  // Session state not initialized.
  EXPECT_FALSE(LockScreenItemStorage::GetIfAllowed(browser_context()));
  EXPECT_FALSE(LockScreenItemStorage::GetIfAllowed(lock_screen_context()));

  // Locked session.
  lock_screen_item_storage()->SetSessionLocked(true);
  EXPECT_FALSE(LockScreenItemStorage::GetIfAllowed(browser_context()));
  EXPECT_EQ(lock_screen_item_storage(),
            LockScreenItemStorage::GetIfAllowed(lock_screen_context()));

  lock_screen_item_storage()->SetSessionLocked(false);

  EXPECT_EQ(lock_screen_item_storage(),
            LockScreenItemStorage::GetIfAllowed(browser_context()));
  EXPECT_FALSE(LockScreenItemStorage::GetIfAllowed(lock_screen_context()));
}

TEST_F(LockScreenItemStorageTest, SetAndGetContent) {
  lock_screen_item_storage()->SetSessionLocked(true);
  EXPECT_FALSE(value_store_migrator());

  const DataItem* item = CreateNewItem();
  ASSERT_TRUE(item);

  std::vector<std::string> all_items;
  GetAllItems(&all_items);
  ASSERT_EQ(1u, all_items.size());
  EXPECT_EQ(item->id(), all_items[0]);

  OperationQueue* item_operations = GetOperations(item->id());
  ASSERT_TRUE(item_operations);
  EXPECT_FALSE(item_operations->HasPendingOperations());

  std::vector<char> content = {'f', 'i', 'l', 'e'};
  OperationResult write_result = OperationResult::kFailed;
  lock_screen_item_storage()->SetItemContent(
      extension()->id(), item->id(), content,
      base::Bind(&RecordWriteResult, &write_result));

  item_operations->CompleteNextOperation(OperationQueue::OperationType::kWrite,
                                         OperationResult::kSuccess);

  EXPECT_EQ(OperationResult::kSuccess, write_result);
  EXPECT_EQ(content, item_operations->content());

  OperationResult read_result = OperationResult::kFailed;
  std::unique_ptr<std::vector<char>> read_content;

  lock_screen_item_storage()->GetItemContent(
      extension()->id(), item->id(),
      base::Bind(&RecordReadResult, &read_result, &read_content));

  item_operations->CompleteNextOperation(OperationQueue::OperationType::kRead,
                                         OperationResult::kSuccess);
  EXPECT_EQ(OperationResult::kSuccess, read_result);
  EXPECT_EQ(content, *read_content);

  OperationResult delete_result = OperationResult::kFailed;
  lock_screen_item_storage()->DeleteItem(
      extension()->id(), item->id(),
      base::Bind(&RecordWriteResult, &delete_result));

  item_operations->CompleteNextOperation(OperationQueue::OperationType::kDelete,
                                         OperationResult::kSuccess);
  EXPECT_EQ(OperationResult::kSuccess, delete_result);
  EXPECT_TRUE(item_operations->deleted());
}

TEST_F(LockScreenItemStorageTest, FailToInitializeData) {
  lock_screen_item_storage()->SetSessionLocked(true);

  const DataItem* item = CreateNewItem();
  ASSERT_TRUE(item);
  const std::string item_id = item->id();

  ResetLockScreenItemStorage();
  item_registry()->set_fail(true);

  OperationResult write_result = OperationResult::kFailed;
  lock_screen_item_storage()->SetItemContent(
      extension()->id(), item_id, {'x'},
      base::Bind(&RecordWriteResult, &write_result));
  EXPECT_EQ(OperationResult::kNotFound, write_result);

  OperationResult read_result = OperationResult::kFailed;
  std::unique_ptr<std::vector<char>> read_content;
  lock_screen_item_storage()->GetItemContent(
      extension()->id(), item_id,
      base::Bind(&RecordReadResult, &read_result, &read_content));
  EXPECT_EQ(OperationResult::kNotFound, read_result);

  OperationResult delete_result = OperationResult::kFailed;
  lock_screen_item_storage()->DeleteItem(
      extension()->id(), "non_existen",
      base::Bind(&RecordWriteResult, &delete_result));
  EXPECT_EQ(OperationResult::kNotFound, delete_result);

  OperationQueue* operations = GetOperations(item_id);
  ASSERT_TRUE(operations);
  EXPECT_FALSE(operations->HasPendingOperations());

  item_registry()->set_fail(false);

  const DataItem* new_item = CreateNewItem();
  ASSERT_TRUE(new_item);

  write_result = OperationResult::kFailed;
  lock_screen_item_storage()->SetItemContent(
      extension()->id(), new_item->id(), {'y'},
      base::Bind(&RecordWriteResult, &write_result));

  OperationQueue* new_item_operations = GetOperations(new_item->id());
  ASSERT_TRUE(new_item_operations);
  new_item_operations->CompleteNextOperation(
      OperationQueue::OperationType::kWrite, OperationResult::kSuccess);
  EXPECT_EQ(OperationResult::kSuccess, write_result);

  std::vector<std::string> items;
  GetAllItems(&items);
  ASSERT_EQ(1u, items.size());
  EXPECT_EQ(new_item->id(), items[0]);
}

TEST_F(LockScreenItemStorageTest, RequestsDuringInitialLoad) {
  lock_screen_item_storage()->SetSessionLocked(true);

  const DataItem* item = CreateNewItem();
  ASSERT_TRUE(item);
  const std::string item_id = item->id();

  item_registry()->set_throttle_get(true);
  ResetLockScreenItemStorage();

  EXPECT_FALSE(item_registry()->HasPendingCallback());

  OperationResult write_result = OperationResult::kFailed;
  lock_screen_item_storage()->SetItemContent(
      extension()->id(), item_id, {'x'},
      base::Bind(&RecordWriteResult, &write_result));

  OperationResult read_result = OperationResult::kFailed;
  std::unique_ptr<std::vector<char>> read_content;
  lock_screen_item_storage()->GetItemContent(
      extension()->id(), item_id,
      base::Bind(&RecordReadResult, &read_result, &read_content));

  std::vector<std::string> items;
  lock_screen_item_storage()->GetAllForExtension(
      extension()->id(), base::Bind(&RecordGetAllItemsResult, &items));
  EXPECT_TRUE(items.empty());

  OperationResult delete_result = OperationResult::kFailed;
  lock_screen_item_storage()->DeleteItem(
      extension()->id(), item_id,
      base::Bind(&RecordWriteResult, &delete_result));

  OperationQueue* operations = GetOperations(item_id);
  ASSERT_TRUE(operations);
  EXPECT_FALSE(operations->HasPendingOperations());

  OperationResult create_result = OperationResult::kFailed;
  const DataItem* new_item = nullptr;
  lock_screen_item_storage()->CreateItem(
      extension()->id(),
      base::Bind(&RecordCreateResult, &create_result, &new_item));
  EXPECT_FALSE(new_item);

  EXPECT_TRUE(item_registry()->HasPendingCallback());
  item_registry()->RunPendingCallback();

  EXPECT_TRUE(operations->HasPendingOperations());

  operations->CompleteNextOperation(OperationQueue::OperationType::kWrite,
                                    OperationResult::kSuccess);
  operations->CompleteNextOperation(OperationQueue::OperationType::kRead,
                                    OperationResult::kSuccess);
  operations->CompleteNextOperation(OperationQueue::OperationType::kDelete,
                                    OperationResult::kSuccess);

  EXPECT_EQ(OperationResult::kSuccess, write_result);
  EXPECT_EQ(OperationResult::kSuccess, read_result);
  ASSERT_TRUE(read_content);
  EXPECT_EQ(std::vector<char>({'x'}), *read_content);
  EXPECT_EQ(OperationResult::kSuccess, delete_result);
  EXPECT_EQ(OperationResult::kSuccess, create_result);

  EXPECT_TRUE(new_item);

  ASSERT_EQ(1u, items.size());
  EXPECT_EQ(item_id, items[0]);

  GetAllItems(&items);
  ASSERT_EQ(1u, items.size());
  EXPECT_EQ(new_item->id(), items[0]);
}

TEST_F(LockScreenItemStorageTest, HandleNonExistent) {
  lock_screen_item_storage()->SetSessionLocked(true);

  const DataItem* item = CreateNewItem();
  ASSERT_TRUE(item);

  std::vector<char> content = {'x'};

  OperationResult write_result = OperationResult::kFailed;
  lock_screen_item_storage()->SetItemContent(
      extension()->id(), "non_existent", content,
      base::Bind(&RecordWriteResult, &write_result));
  EXPECT_EQ(OperationResult::kNotFound, write_result);

  write_result = OperationResult::kFailed;
  lock_screen_item_storage()->SetItemContent(
      "non_existent", item->id(), content,
      base::Bind(&RecordWriteResult, &write_result));
  EXPECT_EQ(OperationResult::kNotFound, write_result);

  OperationResult read_result = OperationResult::kFailed;
  std::unique_ptr<std::vector<char>> read_content;
  lock_screen_item_storage()->GetItemContent(
      extension()->id(), "non_existent",
      base::Bind(&RecordReadResult, &read_result, &read_content));
  EXPECT_EQ(OperationResult::kNotFound, read_result);
  read_result = OperationResult::kFailed;

  lock_screen_item_storage()->GetItemContent(
      "non_existent", item->id(),
      base::Bind(&RecordReadResult, &read_result, &read_content));
  EXPECT_EQ(OperationResult::kNotFound, read_result);

  OperationResult delete_result = OperationResult::kFailed;
  lock_screen_item_storage()->DeleteItem(
      extension()->id(), "non_existen",
      base::Bind(&RecordWriteResult, &delete_result));
  EXPECT_EQ(OperationResult::kNotFound, delete_result);

  delete_result = OperationResult::kFailed;
  lock_screen_item_storage()->DeleteItem(
      "non_existent", item->id(),
      base::Bind(&RecordWriteResult, &delete_result));
  EXPECT_EQ(OperationResult::kNotFound, delete_result);
}

TEST_F(LockScreenItemStorageTest, HandleFailure) {
  lock_screen_item_storage()->SetSessionLocked(true);

  const DataItem* item = CreateItemWithContent({'x'});
  ASSERT_TRUE(item);
  OperationQueue* operations = GetOperations(item->id());
  ASSERT_TRUE(operations);

  OperationResult write_result = OperationResult::kFailed;
  lock_screen_item_storage()->SetItemContent(
      extension()->id(), item->id(), {'x'},
      base::Bind(&RecordWriteResult, &write_result));
  operations->CompleteNextOperation(OperationQueue::OperationType::kWrite,
                                    OperationResult::kInvalidKey);
  EXPECT_EQ(OperationResult::kInvalidKey, write_result);

  OperationResult read_result = OperationResult::kFailed;
  std::unique_ptr<std::vector<char>> read_content;
  lock_screen_item_storage()->GetItemContent(
      extension()->id(), item->id(),
      base::Bind(&RecordReadResult, &read_result, &read_content));
  operations->CompleteNextOperation(OperationQueue::OperationType::kRead,
                                    OperationResult::kWrongKey);
  EXPECT_EQ(OperationResult::kWrongKey, read_result);
  EXPECT_FALSE(read_content);

  EXPECT_FALSE(operations->HasPendingOperations());
}

TEST_F(LockScreenItemStorageTest, DataItemsAvailableEventOnUnlock) {
  TestEventRouter* event_router = static_cast<TestEventRouter*>(
      extensions::EventRouterFactory::GetInstance()->SetTestingFactoryAndUse(
          browser_context(),
          base::BindRepeating(&TestEventRouterFactoryFunction)));
  ASSERT_TRUE(event_router);

  EXPECT_TRUE(event_router->was_locked_values().empty());

  lock_screen_item_storage()->SetSessionLocked(true);
  EXPECT_TRUE(event_router->was_locked_values().empty());

  // No event since no data items associated with the app exist.
  lock_screen_item_storage()->SetSessionLocked(false);
  EXPECT_TRUE(event_router->was_locked_values().empty());

  lock_screen_item_storage()->SetSessionLocked(true);
  const DataItem* item = CreateItemWithContent({'f', 'i', 'l', 'e', '1'});
  const std::string item_id = item->id();
  EXPECT_TRUE(event_router->was_locked_values().empty());

  // There's an available data item, so unlock should trigger the event.
  lock_screen_item_storage()->SetSessionLocked(false);
  EXPECT_EQ(std::vector<bool>({true}), event_router->was_locked_values());
  event_router->ClearWasLockedValues();

  // Update the item content while the session is unlocked.
  EXPECT_TRUE(SetItemContent(item_id, {'f', 'i', 'l', 'e', '2'}));

  lock_screen_item_storage()->SetSessionLocked(true);

  // Data item is still around - notify the app it's available.
  lock_screen_item_storage()->SetSessionLocked(false);
  EXPECT_EQ(std::vector<bool>({true}), event_router->was_locked_values());
  event_router->ClearWasLockedValues();

  lock_screen_item_storage()->SetSessionLocked(true);

  EXPECT_TRUE(SetItemContent(item_id, {'f', 'i', 'l', 'e', '3'}));

  lock_screen_item_storage()->SetSessionLocked(false);
  EXPECT_EQ(std::vector<bool>({true}), event_router->was_locked_values());
  event_router->ClearWasLockedValues();

  // When the item is deleted, the data item avilable event should stop firing.
  OperationResult delete_result;
  lock_screen_item_storage()->DeleteItem(
      extension()->id(), item_id,
      base::Bind(&RecordWriteResult, &delete_result));
  OperationQueue* operations = GetOperations(item_id);
  ASSERT_TRUE(operations);
  operations->CompleteNextOperation(OperationQueue::OperationType::kDelete,
                                    OperationResult::kSuccess);
  lock_screen_item_storage()->SetSessionLocked(false);
  lock_screen_item_storage()->SetSessionLocked(true);

  EXPECT_TRUE(event_router->was_locked_values().empty());
}

TEST_F(LockScreenItemStorageTest,
       NoDataItemsAvailableEventAfterFailedCreation) {
  TestEventRouter* event_router = static_cast<TestEventRouter*>(
      extensions::EventRouterFactory::GetInstance()->SetTestingFactoryAndUse(
          browser_context(),
          base::BindRepeating(&TestEventRouterFactoryFunction)));
  ASSERT_TRUE(event_router);

  lock_screen_item_storage()->SetSessionLocked(true);

  item_registry()->set_allow_new(false);

  OperationResult create_result = OperationResult::kFailed;
  const DataItem* item = nullptr;
  lock_screen_item_storage()->CreateItem(
      extension()->id(),
      base::Bind(&RecordCreateResult, &create_result, &item));
  EXPECT_EQ(OperationResult::kFailed, create_result);

  lock_screen_item_storage()->SetSessionLocked(false);
  lock_screen_item_storage()->SetSessionLocked(true);
  EXPECT_TRUE(event_router->was_locked_values().empty());
}

TEST_F(LockScreenItemStorageTest, DataItemsAvailableEventOnRestart) {
  TestEventRouter* event_router = static_cast<TestEventRouter*>(
      extensions::EventRouterFactory::GetInstance()->SetTestingFactoryAndUse(
          browser_context(),
          base::BindRepeating(&TestEventRouterFactoryFunction)));
  ASSERT_TRUE(event_router);

  EXPECT_TRUE(event_router->was_locked_values().empty());

  lock_screen_item_storage()->SetSessionLocked(true);
  EXPECT_TRUE(event_router->was_locked_values().empty());

  const DataItem* item = CreateItemWithContent({'f', 'i', 'l', 'e', '1'});
  EXPECT_TRUE(event_router->was_locked_values().empty());
  const std::string item_id = item->id();

  ResetLockScreenItemStorage();

  EXPECT_TRUE(event_router->was_locked_values().empty());
  lock_screen_item_storage()->SetSessionLocked(false);

  EXPECT_EQ(std::vector<bool>({false}), event_router->was_locked_values());
  event_router->ClearWasLockedValues();

  // The event should be dispatched on next unlock event, as long as a valid
  // item exists.
  ResetLockScreenItemStorage();
  lock_screen_item_storage()->SetSessionLocked(false);

  EXPECT_EQ(std::vector<bool>({false}), event_router->was_locked_values());
  event_router->ClearWasLockedValues();

  ResetLockScreenItemStorage();

  OperationResult delete_result = OperationResult::kFailed;
  lock_screen_item_storage()->DeleteItem(
      extension()->id(), item_id,
      base::Bind(&RecordWriteResult, &delete_result));
  OperationQueue* operations = GetOperations(item_id);
  ASSERT_TRUE(operations);
  operations->CompleteNextOperation(OperationQueue::OperationType::kDelete,
                                    OperationResult::kSuccess);

  lock_screen_item_storage()->SetSessionLocked(false);
  EXPECT_TRUE(event_router->was_locked_values().empty());
}

TEST_F(LockScreenItemStorageTest, ClearDataOnUninstall) {
  const DataItem* item = CreateItemWithContent({'x'});
  ASSERT_TRUE(item);

  ExtensionRegistry::Get(browser_context())->RemoveEnabled(extension()->id());
  ExtensionRegistry::Get(browser_context())
      ->TriggerOnUninstalled(extension(), UNINSTALL_REASON_FOR_TESTING);
  ExtensionRegistry::Get(browser_context())->AddEnabled(extension());

  std::vector<std::string> items;
  GetAllItems(&items);
  EXPECT_TRUE(items.empty());
}

TEST_F(LockScreenItemStorageTest,
       ClearOnUninstallWhileLockScreenItemStorageNotSet) {
  TestEventRouter* event_router = static_cast<TestEventRouter*>(
      extensions::EventRouterFactory::GetInstance()->SetTestingFactoryAndUse(
          browser_context(),
          base::BindRepeating(&TestEventRouterFactoryFunction)));
  ASSERT_TRUE(event_router);

  const DataItem* item = CreateItemWithContent({'x'});
  ASSERT_TRUE(item);

  UnsetLockScreenItemStorage();

  ExtensionRegistry::Get(browser_context())->RemoveEnabled(extension()->id());
  ExtensionRegistry::Get(browser_context())
      ->TriggerOnUninstalled(extension(), UNINSTALL_REASON_FOR_TESTING);

  ResetLockScreenItemStorage();
  ExtensionRegistry::Get(browser_context())->AddEnabled(extension());
  lock_screen_item_storage()->SetSessionLocked(false);

  std::vector<std::string> items;
  GetAllItems(&items);
  EXPECT_TRUE(items.empty());

  EXPECT_TRUE(event_router->was_locked_values().empty());
}

TEST_F(LockScreenItemStorageTest, OperationsBlockedOnMigration) {
  EXPECT_FALSE(value_store_migrator());

  lock_screen_item_storage()->SetSessionLocked(true);

  // Create an item in the extension's value store.
  const DataItem* initial_item = CreateNewItem();
  ASSERT_TRUE(initial_item);
  const std::string initial_item_id = initial_item->id();

  // Update the local state so it seems that there are extensions that have
  // entries in the deprecated value store, and recreate the item storage to
  // pick up the local state changes.
  scoped_refptr<const Extension> second_extension =
      CreateTestExtension(kSecondTestExtensionId);
  InitExtensionLocalState(
      {{extension()->id(), 1 /*storage_version*/, 2 /*item_count*/},
       {second_extension->id(), 1 /*storage_version*/, 2 /*item_count*/}});

  set_can_create_deprecated_value_store(true);
  ResetLockScreenItemStorage();

  // This time, the lock screen item storage should have created the store
  // migrator.
  ASSERT_TRUE(value_store_migrator());
  EXPECT_EQ(std::set<ExtensionId>({extension()->id(), second_extension->id()}),
            value_store_migrator()->extensions_to_migrate());

  const std::string migrated_item_id = "migrated";
  const std::vector<char> migrated_item_content = {'a', 'b', 'c'};

  // Verify that requests for the migrating extension are throttled while
  // migration is in progress.
  OperationResult write_result = OperationResult::kFailed;
  lock_screen_item_storage()->SetItemContent(
      extension()->id(), initial_item_id, {'x'},
      base::Bind(&RecordWriteResult, &write_result));

  OperationResult read_result = OperationResult::kFailed;
  std::unique_ptr<std::vector<char>> read_content;
  lock_screen_item_storage()->GetItemContent(
      extension()->id(), migrated_item_id,
      base::Bind(&RecordReadResult, &read_result, &read_content));

  std::vector<std::string> items;
  lock_screen_item_storage()->GetAllForExtension(
      extension()->id(), base::Bind(&RecordGetAllItemsResult, &items));
  EXPECT_TRUE(items.empty());

  OperationResult delete_result = OperationResult::kFailed;
  lock_screen_item_storage()->DeleteItem(
      extension()->id(), initial_item_id,
      base::Bind(&RecordWriteResult, &delete_result));

  OperationQueue* initial_item_operations = GetOperations(initial_item_id);
  ASSERT_TRUE(initial_item_operations);
  EXPECT_FALSE(initial_item_operations->HasPendingOperations());

  OperationQueue* migrated_item_operations = GetOperations(migrated_item_id);
  EXPECT_FALSE(migrated_item_operations &&
               migrated_item_operations->HasPendingOperations());

  // Extension's data operations should not be unblocked by another extension
  // migration finishing.
  FinishMigration(second_extension->id(), std::vector<MigratedItem>());
  EXPECT_FALSE(initial_item_operations->HasPendingOperations());
  EXPECT_FALSE(migrated_item_operations &&
               migrated_item_operations->HasPendingOperations());

  OperationResult create_result = OperationResult::kFailed;
  const DataItem* new_item = nullptr;
  lock_screen_item_storage()->CreateItem(
      extension()->id(),
      base::Bind(&RecordCreateResult, &create_result, &new_item));
  EXPECT_FALSE(new_item);

  // Finish item migration - all queued operations should be now run.
  ASSERT_TRUE(FinishMigration(extension()->id(),
                              {{migrated_item_id, migrated_item_content}}));

  migrated_item_operations = GetOperations(migrated_item_id);
  ASSERT_TRUE(migrated_item_operations);
  EXPECT_TRUE(migrated_item_operations->HasPendingOperations());

  EXPECT_TRUE(initial_item_operations->HasPendingOperations());

  initial_item_operations->CompleteNextOperation(
      OperationQueue::OperationType::kWrite, OperationResult::kSuccess);
  migrated_item_operations->CompleteNextOperation(
      OperationQueue::OperationType::kRead, OperationResult::kSuccess);
  initial_item_operations->CompleteNextOperation(
      OperationQueue::OperationType::kDelete, OperationResult::kSuccess);

  EXPECT_EQ(OperationResult::kSuccess, write_result);
  EXPECT_EQ(OperationResult::kSuccess, read_result);
  ASSERT_TRUE(read_content);
  EXPECT_EQ(migrated_item_content, *read_content);
  EXPECT_EQ(OperationResult::kSuccess, delete_result);
  EXPECT_EQ(OperationResult::kSuccess, create_result);

  EXPECT_TRUE(new_item);

  EXPECT_EQ(2u, items.size());
  EXPECT_TRUE(base::Contains(items, migrated_item_id));
  EXPECT_TRUE(base::Contains(items, initial_item_id));

  GetAllItems(&items);
  ASSERT_EQ(2u, items.size());
  EXPECT_TRUE(base::Contains(items, migrated_item_id));
  EXPECT_TRUE(base::Contains(items, new_item->id()));
}

TEST_F(LockScreenItemStorageTest,
       OperationsBlockedOnAnotherExtensionMigration) {
  EXPECT_FALSE(value_store_migrator());

  // Update the local state so it seems that there are extensions that have
  // entries in the deprecated value store, and recreate the item storage to
  // pick up the local state changes.
  scoped_refptr<const Extension> second_extension =
      CreateTestExtension(kSecondTestExtensionId);
  InitExtensionLocalState(
      {{extension()->id(), 2 /*storage_version*/, 1 /*item_count*/},
       {second_extension->id(), 1 /*storage_version*/, 1 /*item_count*/}});

  set_can_create_deprecated_value_store(true);
  ResetLockScreenItemStorage();

  // The lock screen item storage should have created the store migrator for
  // |second_extension| only.
  ASSERT_TRUE(value_store_migrator());
  EXPECT_EQ(std::set<ExtensionId>({second_extension->id()}),
            value_store_migrator()->extensions_to_migrate());

  // Operations on the |extension()| data items should proceed normally, given
  // that its data is not being migrated.
  const DataItem* item = CreateItemWithContent({'f', 'i', 'l', 'e', '1'});
  EXPECT_TRUE(item);

  std::vector<std::string> items;
  GetAllItems(&items);
  EXPECT_EQ(std::vector<std::string>{item->id()}, items);
}

TEST_F(LockScreenItemStorageTest, MigrationNotReAttemptedAfterSuccess) {
  EXPECT_FALSE(value_store_migrator());

  // Update the local state so it seems that there are extensions that have
  // entries in the deprecated value store, and recreate the item storage to
  // pick up the local state changes.
  InitExtensionLocalState(
      {{extension()->id(), 1 /*storage_version*/, 1 /*item_count*/}});

  set_can_create_deprecated_value_store(true);
  ResetLockScreenItemStorage();

  ASSERT_TRUE(value_store_migrator());
  EXPECT_EQ(std::set<ExtensionId>({extension()->id()}),
            value_store_migrator()->extensions_to_migrate());

  const std::string migrated_item_id = "migrated";
  const std::vector<char> migrated_item_content = {'a', 'b', 'c'};
  ASSERT_TRUE(FinishMigration(extension()->id(),
                              {{migrated_item_id, migrated_item_content}}));

  // Reset the storage, and verify that storage migrator is not recreated, as
  // the data migration for the extension has been completed.
  set_can_create_deprecated_value_store(false);
  ResetLockScreenItemStorage();

  EXPECT_FALSE(value_store_migrator());

  std::vector<std::string> items;
  GetAllItems(&items);
  EXPECT_EQ(std::vector<std::string>{"migrated"}, items);
}

TEST_F(LockScreenItemStorageTest,
       MigrationNotReAttemptedAfterSuccess_NoItemsMigrated) {
  TestEventRouter* event_router = static_cast<TestEventRouter*>(
      extensions::EventRouterFactory::GetInstance()->SetTestingFactoryAndUse(
          browser_context(),
          base::BindRepeating(&TestEventRouterFactoryFunction)));
  ASSERT_TRUE(event_router);

  EXPECT_FALSE(value_store_migrator());

  // Create an item in the extension's value store. The test will verify it's
  // counted in the post-migration local state even if no additional items are
  // migrated.
  const DataItem* initial_item = CreateNewItem();
  ASSERT_TRUE(initial_item);
  const std::string initial_item_id = initial_item->id();

  // Update the local state so it seems that there are extensions that have
  // entries in the deprecated value store, and recreate the item storage to
  // pick up the local state changes.
  InitExtensionLocalState(
      {{extension()->id(), 1 /*storage_version*/, 1 /*item_count*/}});

  set_can_create_deprecated_value_store(true);
  ResetLockScreenItemStorage();
  lock_screen_item_storage()->SetSessionLocked(true);
  ASSERT_TRUE(value_store_migrator());
  EXPECT_EQ(std::set<ExtensionId>({extension()->id()}),
            value_store_migrator()->extensions_to_migrate());

  ASSERT_TRUE(FinishMigration(extension()->id(), std::vector<MigratedItem>()));

  // Make sure that extension is notified about available items after
  // migration if no items got migrated, but an item existed in the target
  // storage before migration started - e.g. if it's a left-over from previous
  // migration attempt.
  lock_screen_item_storage()->SetSessionLocked(false);
  EXPECT_EQ(std::vector<bool>({true}), event_router->was_locked_values());

  // Reset the storage, and verify that storage migrator is not recreated.
  set_can_create_deprecated_value_store(false);
  ResetLockScreenItemStorage();
  EXPECT_FALSE(value_store_migrator());

  std::vector<std::string> items;
  GetAllItems(&items);
  EXPECT_EQ(std::vector<std::string>{initial_item_id}, items);
}

TEST_F(LockScreenItemStorageTest,
       ReAttemptMigrationIfStorageIsResetBeforeRegisteredItemsAreReFetched) {
  EXPECT_FALSE(value_store_migrator());

  // Update the local state so it seems that there are extensions that have
  // entries in the deprecated value store, and recreate the item storage to
  // pick up the local state changes.
  InitExtensionLocalState(
      {{extension()->id(), 1 /*storage_version*/, 1 /*item_count*/}});

  set_can_create_deprecated_value_store(true);
  ResetLockScreenItemStorage();
  ASSERT_TRUE(value_store_migrator());
  EXPECT_EQ(std::set<ExtensionId>({extension()->id()}),
            value_store_migrator()->extensions_to_migrate());

  // Throttle registered item fetch, to delay update to the local state prefs
  // after data migration.
  item_registry()->set_throttle_get(true);

  const std::string migrated_item_id = "migrated";
  const std::vector<char> migrated_item_content = {'a', 'b', 'c'};
  ASSERT_TRUE(FinishMigration(extension()->id(),
                              {{migrated_item_id, migrated_item_content}}));

  // Reset the storage, and verify that storage migrator is created again, as
  // item storage got reset before it refetched info about registered items.
  ResetLockScreenItemStorage();

  EXPECT_TRUE(value_store_migrator());
}

TEST_F(LockScreenItemStorageTest,
       ItemAvailableEventNotSentIfItemsLostDuringMigration) {
  TestEventRouter* event_router = static_cast<TestEventRouter*>(
      extensions::EventRouterFactory::GetInstance()->SetTestingFactoryAndUse(
          browser_context(),
          base::BindRepeating(&TestEventRouterFactoryFunction)));
  ASSERT_TRUE(event_router);

  EXPECT_FALSE(value_store_migrator());

  // Update the local state so it seems that there are extensions that have
  // entries in the deprecated value store, and recreate the item storage to
  // pick up the local state changes.
  InitExtensionLocalState(
      {{extension()->id(), 1 /*storage_version*/, 1 /*item_count*/}});

  set_can_create_deprecated_value_store(true);
  ResetLockScreenItemStorage();
  ASSERT_TRUE(value_store_migrator());
  EXPECT_EQ(std::set<ExtensionId>({extension()->id()}),
            value_store_migrator()->extensions_to_migrate());
  lock_screen_item_storage()->SetSessionLocked(true);

  ASSERT_TRUE(FinishMigration(extension()->id(), std::vector<MigratedItem>()));

  // Make sure that extension is notified about available items after
  // migration if no items got migrated, but an item exists in the storage.
  lock_screen_item_storage()->SetSessionLocked(false);
  EXPECT_TRUE(event_router->was_locked_values().empty());

  // Reset the storage, and verify that storage migrator is not recreated.
  set_can_create_deprecated_value_store(false);
  ResetLockScreenItemStorage();
  EXPECT_FALSE(value_store_migrator());
}

TEST_F(LockScreenItemStorageTest, AttemptMigrationEvenWhenNoDataRecorded) {
  EXPECT_FALSE(value_store_migrator());

  // Update the local state so it seems that lock screen notes have previously
  // been used by the extension, but currently, no items are recorded to exist
  // for the extension.
  InitExtensionLocalState(
      {{extension()->id(), 1 /*storage_version*/, 0 /*item_count*/}});

  // Verify that the migrator gets created regardles of the persisted item
  // count - to handle an edge case where item storage is shut down after an
  // item is created but before the item creation was recorded in the local
  // state.
  set_can_create_deprecated_value_store(true);
  ResetLockScreenItemStorage();
  EXPECT_TRUE(value_store_migrator());
}

TEST_F(LockScreenItemStorageTest, ExtensionUninstalledDuringMigration) {
  // Update the local state so it seems that there are extensions that have
  // entries in the deprecated value store, and recreate the item storage to
  // pick up the local state changes.
  InitExtensionLocalState(
      {{extension()->id(), 1 /*storage_version*/, 2 /*item_count*/}});

  set_can_create_deprecated_value_store(true);
  ResetLockScreenItemStorage();
  ASSERT_TRUE(value_store_migrator());

  ExtensionRegistry::Get(browser_context())->RemoveEnabled(extension()->id());
  ExtensionRegistry::Get(browser_context())
      ->TriggerOnUninstalled(extension(), UNINSTALL_REASON_FOR_TESTING);

  ASSERT_TRUE(value_store_migrator()->FinishClearData(extension()->id()));

  // There should be no migrator, as the extension data should have been
  // cleared.
  set_can_create_deprecated_value_store(false);
  ResetLockScreenItemStorage();
  EXPECT_FALSE(value_store_migrator());
}

TEST_F(LockScreenItemStorageTest,
       ExtensionUninstalledDuringMigration_StorageResetBeforeDataCleared) {
  // Update the local state so it seems that there are extensions that have
  // entries in the deprecated value store, and recreate the item storage to
  // pick up the local state changes.
  InitExtensionLocalState(
      {{extension()->id(), 1 /*storage_version*/, 2 /*item_count*/}});

  set_can_create_deprecated_value_store(true);
  ResetLockScreenItemStorage();
  ASSERT_TRUE(value_store_migrator());

  ExtensionRegistry::Get(browser_context())->RemoveEnabled(extension()->id());
  ExtensionRegistry::Get(browser_context())
      ->TriggerOnUninstalled(extension(), UNINSTALL_REASON_FOR_TESTING);

  EXPECT_TRUE(
      value_store_migrator()->ClearingDataForExtension(extension()->id()));

  // Reset lock screen storage before data cleanup completes.
  // When the storage is recreated it should create migrator and start data
  // cleanup right away.
  ResetLockScreenItemStorage();

  ASSERT_TRUE(value_store_migrator());
  EXPECT_TRUE(
      value_store_migrator()->ClearingDataForExtension(extension()->id()));
  ASSERT_TRUE(value_store_migrator()->FinishClearData(extension()->id()));

  // There should be no migrator, as the extension data should have been
  // cleared.
  set_can_create_deprecated_value_store(false);
  ResetLockScreenItemStorage();
  EXPECT_FALSE(value_store_migrator());
}

}  // namespace lock_screen_data
}  // namespace extensions
