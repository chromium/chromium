// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/lock_screen_data/data_item.h"

#include <memory>
#include <set>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/value_store/test_value_store_factory.h"
#include "components/value_store/testing_value_store.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "crypto/symmetric_key.h"
#include "extensions/browser/api/lock_screen_data/operation_result.h"
#include "extensions/browser/api/storage/backend_task_runner.h"
#include "extensions/browser/api/storage/local_value_store_cache.h"
#include "extensions/browser/api/storage/value_store_util.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/test_extensions_browser_client.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace lock_screen_data {

namespace {

const char kPrimaryExtensionId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char kSecondaryExtensionId[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";

void WriteCallbackNotCalled(const std::string& message,
                            OperationResult result) {
  ADD_FAILURE() << "Unexpected callback " << message;
}

void ReadCallbackNotCalled(const std::string& message,
                           OperationResult result,
                           std::unique_ptr<std::vector<char>> data) {
  ADD_FAILURE() << "Unexpected callback " << message;
}

void WriteCallback(base::OnceClosure callback,
                   OperationResult* result_out,
                   OperationResult result) {
  *result_out = result;
  std::move(callback).Run();
}

void ReadCallback(base::OnceClosure callback,
                  OperationResult* result_out,
                  std::unique_ptr<std::vector<char>>* content_out,
                  OperationResult result,
                  std::unique_ptr<std::vector<char>> content) {
  *result_out = result;
  *content_out = std::move(content);
  std::move(callback).Run();
}

void GetRegisteredItemsCallback(base::OnceClosure callback,
                                OperationResult* result_out,
                                base::Value::Dict* dict_out,
                                OperationResult result,
                                base::Value::Dict dict) {
  *result_out = result;
  *dict_out = std::move(dict);
  std::move(callback).Run();
}

}  // namespace

class DataItemTest : public testing::Test {
 public:
  DataItemTest() {}

  DataItemTest(const DataItemTest&) = delete;
  DataItemTest& operator=(const DataItemTest&) = delete;

  ~DataItemTest() override = default;

  void SetUp() override {
    task_runner_ = GetBackendTaskRunner();
    ASSERT_TRUE(test_dir_.CreateUniqueTempDir());

    context_ = std::make_unique<content::TestBrowserContext>();
    extensions_browser_client_ =
        std::make_unique<TestExtensionsBrowserClient>(context_.get());
    BrowserContextDependencyManager::GetInstance()->MarkBrowserContextLive(
        context_.get());

    ExtensionsBrowserClient::Set(extensions_browser_client_.get());

    value_store_factory_ =
        base::MakeRefCounted<value_store::TestValueStoreFactory>();
    value_store_cache_ =
        std::make_unique<LocalValueStoreCache>(value_store_factory_);

    extension_ = CreateTestExtension(kPrimaryExtensionId);
  }

  void TearDown() override {
    TearDownValueStoreCache();

    BrowserContextDependencyManager::GetInstance()
        ->DestroyBrowserContextServices(context_.get());
    ExtensionsBrowserClient::Set(nullptr);
    extensions_browser_client_.reset();
    context_.reset();
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

  std::unique_ptr<DataItem> CreateDataItem(const std::string& item_id,
                                           const ExtensionId& extension_id,
                                           const std::string& crypto_key) {
    return std::make_unique<DataItem>(item_id, extension_id, context_.get(),
                                      value_store_cache_.get(),
                                      task_runner_.get(), crypto_key);
  }

  std::unique_ptr<DataItem> CreateAndRegisterDataItem(
      const std::string& item_id,
      const ExtensionId& extension_id,
      const std::string& crypto_key) {
    std::unique_ptr<DataItem> item =
        CreateDataItem(item_id, extension_id, crypto_key);

    OperationResult result = OperationResult::kFailed;
    base::RunLoop run_loop;
    item->Register(
        base::BindOnce(&WriteCallback, run_loop.QuitClosure(), &result));
    run_loop.Run();

    EXPECT_EQ(OperationResult::kSuccess, result);
    return item;
  }

  void DrainTaskRunner() {
    base::RunLoop run_loop;
    task_runner()->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                    run_loop.QuitClosure());
    run_loop.Run();
  }

  const base::FilePath& test_dir() const { return test_dir_.GetPath(); }

  scoped_refptr<base::SequencedTaskRunner> task_runner() {
    return task_runner_;
  }

  scoped_refptr<const Extension> CreateTestExtension(
      const ExtensionId& extension_id) {
    base::Value::Dict app_builder;
    app_builder.Set("background",
                    base::Value::Dict().Set(
                        "scripts", base::Value::List().Append("script")));
    base::Value::List app_handlers_builder;
    app_handlers_builder.Append(base::Value::Dict()
                                    .Set("action", "new_note")
                                    .Set("enabled_on_lock_screen", true));
    scoped_refptr<const Extension> extension =
        ExtensionBuilder()
            .SetID(extension_id)
            .SetManifest(
                base::Value::Dict()
                    .Set("name", "Test app")
                    .Set("version", "1.0")
                    .Set("manifest_version", 2)
                    .Set("app", std::move(app_builder))
                    .Set("action_handlers", std::move(app_handlers_builder))
                    .Set("permissions",
                         base::Value::List().Append("lockScreen")))
            .Build();
    ExtensionRegistry::Get(context_.get())->AddEnabled(extension);
    return extension;
  }

  OperationResult WriteItemAndWaitForResult(DataItem* item,
                                            const std::vector<char>& data) {
    OperationResult result = OperationResult::kFailed;
    base::RunLoop run_loop;
    item->Write(
        data, base::BindOnce(&WriteCallback, run_loop.QuitClosure(), &result));
    run_loop.Run();
    return result;
  }

  OperationResult ReadItemAndWaitForResult(
      DataItem* item,
      std::unique_ptr<std::vector<char>>* data) {
    OperationResult result = OperationResult::kFailed;
    std::unique_ptr<std::vector<char>> read_content;
    base::RunLoop run_loop;
    item->Read(base::BindOnce(&ReadCallback, run_loop.QuitClosure(), &result,
                              &read_content));
    run_loop.Run();
    if (data) {
      *data = std::move(read_content);
    }
    return result;
  }

  OperationResult DeleteItemAndWaitForResult(DataItem* item) {
    OperationResult result = OperationResult::kFailed;
    base::RunLoop run_loop;
    item->Delete(
        base::BindOnce(&WriteCallback, run_loop.QuitClosure(), &result));
    run_loop.Run();
    return result;
  }

  OperationResult RegisterItemAndWaitForResult(DataItem* item) {
    OperationResult result = OperationResult::kFailed;
    base::RunLoop run_loop;
    item->Register(
        base::BindOnce(&WriteCallback, run_loop.QuitClosure(), &result));
    run_loop.Run();
    return result;
  }

  void SetReturnCodeForValueStoreOperations(
      const ExtensionId& extension_id,
      value_store::ValueStore::StatusCode code) {
    base::FilePath value_store_dir = value_store_util::GetValueStoreDir(
        settings_namespace::LOCAL, value_store_util::ModelType::APP,
        extension_id);
    value_store::TestingValueStore* store =
        static_cast<value_store::TestingValueStore*>(
            value_store_factory_->GetExisting(value_store_dir));
    ASSERT_TRUE(store);
    store->set_status_code(code);
  }

  OperationResult GetRegisteredItemIds(const ExtensionId& extension_id,
                                       std::set<std::string>* items) {
    OperationResult result = OperationResult::kFailed;
    base::Value::Dict items_dict;

    base::RunLoop run_loop;
    DataItem::GetRegisteredValuesForExtension(
        context_.get(), value_store_cache_.get(), task_runner_.get(),
        extension_id,
        base::BindOnce(&GetRegisteredItemsCallback, run_loop.QuitClosure(),
                       &result, &items_dict));
    run_loop.Run();

    if (result != OperationResult::kSuccess) {
      return result;
    }

    items->clear();
    for (const auto item : items_dict) {
      EXPECT_EQ(0u, items->count(item.first));
      items->insert(item.first);
    }
    return OperationResult::kSuccess;
  }

  void DeleteAllItems(const ExtensionId& extension_id) {
    base::RunLoop run_loop;
    DataItem::DeleteAllItemsForExtension(
        context_.get(), value_store_cache_.get(), task_runner_.get(),
        extension_id, run_loop.QuitClosure());
    run_loop.Run();
  }

  const Extension* extension() const { return extension_.get(); }

 private:
  void TearDownValueStoreCache() {
    base::RunLoop run_loop;
    task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&DataItemTest::ReleaseValueStoreCache,
                       base::Unretained(this)),
        run_loop.QuitClosure());
    run_loop.Run();
  }

  void ReleaseValueStoreCache() { value_store_cache_.reset(); }

  base::ScopedTempDir test_dir_;

  std::unique_ptr<content::TestBrowserContext> context_;

  content::BrowserTaskEnvironment task_environment_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  std::unique_ptr<TestExtensionsBrowserClient> extensions_browser_client_;

  scoped_refptr<value_store::TestValueStoreFactory> value_store_factory_;
  std::unique_ptr<ValueStoreCache> value_store_cache_;

  scoped_refptr<const Extension> extension_;
};

TEST_F(DataItemTest, OperationsOnUnregisteredItem) {
  std::unique_ptr<DataItem> item =
      CreateDataItem("data_id", extension()->id(), GenerateKey("key_1"));

  std::vector<char> content = {'f', 'i', 'l', 'e', '_', '1'};
  EXPECT_EQ(OperationResult::kNotFound,
            WriteItemAndWaitForResult(item.get(), content));

  EXPECT_EQ(OperationResult::kNotFound,
            ReadItemAndWaitForResult(item.get(), nullptr));

  EXPECT_EQ(OperationResult::kNotFound, DeleteItemAndWaitForResult(item.get()));

  EXPECT_EQ(OperationResult::kSuccess,
            RegisterItemAndWaitForResult(item.get()));

  EXPECT_EQ(OperationResult::kSuccess,
            WriteItemAndWaitForResult(item.get(), content));
}

TEST_F(DataItemTest, OperationsWithUnknownExtension) {
  std::unique_ptr<DataItem> item =
      CreateDataItem("data_id", "unknown", GenerateKey("key_1"));

  std::vector<char> content = {'f', 'i', 'l', 'e', '_', '1'};
  EXPECT_EQ(OperationResult::kUnknownExtension,
            WriteItemAndWaitForResult(item.get(), content));

  EXPECT_EQ(OperationResult::kUnknownExtension,
            ReadItemAndWaitForResult(item.get(), nullptr));

  EXPECT_EQ(OperationResult::kUnknownExtension,
            DeleteItemAndWaitForResult(item.get()));

  EXPECT_EQ(OperationResult::kUnknownExtension,
            RegisterItemAndWaitForResult(item.get()));

  std::set<std::string> item_ids;
  EXPECT_EQ(OperationResult::kUnknownExtension,
            GetRegisteredItemIds("unknown", &item_ids));
}

TEST_F(DataItemTest, ValueStoreErrors) {
  std::unique_ptr<DataItem> item = CreateAndRegisterDataItem(
      "data_id", extension()->id(), GenerateKey("key_1"));
  std::vector<char> content = {'f', 'i', 'l', 'e', '_', '1'};
  EXPECT_EQ(OperationResult::kSuccess,
            WriteItemAndWaitForResult(item.get(), content));

  SetReturnCodeForValueStoreOperations(extension()->id(),
                                       value_store::ValueStore::OTHER_ERROR);

  EXPECT_EQ(OperationResult::kNotFound,
            ReadItemAndWaitForResult(item.get(), nullptr));
  EXPECT_EQ(OperationResult::kNotFound,
            WriteItemAndWaitForResult(item.get(), {'x'}));
  EXPECT_EQ(OperationResult::kFailed, DeleteItemAndWaitForResult(item.get()));

  std::unique_ptr<DataItem> unregistered =
      CreateDataItem("data_id_1", extension()->id(), GenerateKey("key_1"));
  EXPECT_EQ(OperationResult::kFailed,
            RegisterItemAndWaitForResult(unregistered.get()));

  std::set<std::string> item_ids;
  EXPECT_EQ(OperationResult::kFailed,
            GetRegisteredItemIds(extension()->id(), &item_ids));
}

TEST_F(DataItemTest, GetRegisteredItems) {
  std::set<std::string> item_ids;
  EXPECT_EQ(OperationResult::kSuccess,
            GetRegisteredItemIds(extension()->id(), &item_ids));
  EXPECT_TRUE(item_ids.empty());

  std::unique_ptr<DataItem> item_1 =
      CreateDataItem("data_id_1", extension()->id(), GenerateKey("key_1"));

  EXPECT_EQ(OperationResult::kSuccess,
            GetRegisteredItemIds(extension()->id(), &item_ids));
  EXPECT_TRUE(item_ids.empty());

  EXPECT_EQ(OperationResult::kSuccess,
            RegisterItemAndWaitForResult(item_1.get()));

  EXPECT_EQ(OperationResult::kSuccess,
            GetRegisteredItemIds(extension()->id(), &item_ids));
  EXPECT_EQ(std::set<std::string>({"data_id_1"}), item_ids);

  std::unique_ptr<DataItem> item_2 = CreateAndRegisterDataItem(
      "data_id_2", extension()->id(), GenerateKey("key_1"));

  std::unique_ptr<DataItem> unregistered =
      CreateDataItem("unregistered", extension()->id(), GenerateKey("key_1"));

  EXPECT_EQ(OperationResult::kSuccess,
            GetRegisteredItemIds(extension()->id(), &item_ids));
  EXPECT_EQ(std::set<std::string>({"data_id_1", "data_id_2"}), item_ids);

  scoped_refptr<const Extension> secondary_extension =
      CreateTestExtension(kSecondaryExtensionId);

  std::unique_ptr<DataItem> secondary_extension_item =
      CreateAndRegisterDataItem("data_id_2", secondary_extension->id(),
                                GenerateKey("key_1"));

  EXPECT_EQ(OperationResult::kSuccess,
            GetRegisteredItemIds(extension()->id(), &item_ids));
  EXPECT_EQ(std::set<std::string>({"data_id_1", "data_id_2"}), item_ids);

  EXPECT_EQ(OperationResult::kSuccess,
            GetRegisteredItemIds(secondary_extension->id(), &item_ids));
  EXPECT_EQ(std::set<std::string>({"data_id_2"}), item_ids);

  EXPECT_EQ(OperationResult::kSuccess,
            DeleteItemAndWaitForResult(item_2.get()));

  EXPECT_EQ(OperationResult::kSuccess,
            GetRegisteredItemIds(extension()->id(), &item_ids));
  EXPECT_EQ(std::set<std::string>({"data_id_1"}), item_ids);

  EXPECT_EQ(OperationResult::kSuccess,
            DeleteItemAndWaitForResult(item_1.get()));

  EXPECT_EQ(OperationResult::kSuccess,
            GetRegisteredItemIds(extension()->id(), &item_ids));
  EXPECT_TRUE(item_ids.empty());

  EXPECT_EQ(OperationResult::kSuccess,
            GetRegisteredItemIds(secondary_extension->id(), &item_ids));
  EXPECT_EQ(std::set<std::string>({"data_id_2"}), item_ids);
}

TEST_F(DataItemTest, DoubleRegistration) {
  std::unique_ptr<DataItem> item =
      CreateDataItem("data_id", extension()->id(), GenerateKey("key_1"));

  EXPECT_EQ(OperationResult::kSuccess,
            RegisterItemAndWaitForResult(item.get()));

  std::unique_ptr<DataItem> duplicate =
      CreateDataItem("data_id", extension()->id(), GenerateKey("key_1"));

  EXPECT_EQ(OperationResult::kAlreadyRegistered,
            RegisterItemAndWaitForResult(duplicate.get()));

  EXPECT_EQ(OperationResult::kSuccess, DeleteItemAndWaitForResult(item.get()));

  EXPECT_EQ(OperationResult::kSuccess,
            RegisterItemAndWaitForResult(duplicate.get()));
}

TEST_F(DataItemTest, ReadWrite) {
  std::unique_ptr<DataItem> item = CreateAndRegisterDataItem(
      "data_id", extension()->id(), GenerateKey("key_1"));

  std::vector<char> content = {'f', 'i', 'l', 'e', '_', '1'};
  EXPECT_EQ(OperationResult::kSuccess,
            WriteItemAndWaitForResult(item.get(), content));

  std::unique_ptr<std::vector<char>> read_content;
  ASSERT_EQ(OperationResult::kSuccess,
            ReadItemAndWaitForResult(item.get(), &read_content));
  ASSERT_TRUE(read_content);
  EXPECT_EQ(content, *read_content);

  read_content.reset();
  std::unique_ptr<DataItem> item_copy =
      CreateDataItem("data_id", extension()->id(), GenerateKey("key_1"));
  ASSERT_EQ(OperationResult::kSuccess,
            ReadItemAndWaitForResult(item_copy.get(), &read_content));
  ASSERT_TRUE(read_content);
  EXPECT_EQ(content, *read_content);

  std::unique_ptr<DataItem> different_key =
      CreateDataItem("data_id", extension()->id(), GenerateKey("key_2"));
  EXPECT_EQ(OperationResult::kWrongKey,
            ReadItemAndWaitForResult(different_key.get(), nullptr));
}

TEST_F(DataItemTest, ExtensionsWithConflictingDataItemIds) {
  std::unique_ptr<DataItem> first = CreateAndRegisterDataItem(
      "data_id", extension()->id(), GenerateKey("key_1"));

  scoped_refptr<const Extension> second_extension =
      CreateTestExtension(kSecondaryExtensionId);
  ASSERT_NE(extension()->id(), second_extension->id());
  std::unique_ptr<DataItem> second = CreateAndRegisterDataItem(
      "data_id", second_extension->id(), GenerateKey("key_1"));

  std::vector<char> first_content = {'f', 'i', 'l', 'e', '_', '1'};
  EXPECT_EQ(OperationResult::kSuccess,
            WriteItemAndWaitForResult(first.get(), first_content));

  std::vector<char> second_content = {'f', 'i', 'l', 'e', '_', '2'};
  EXPECT_EQ(OperationResult::kSuccess,
            WriteItemAndWaitForResult(second.get(), second_content));

  std::unique_ptr<std::vector<char>> first_read;
  ASSERT_EQ(OperationResult::kSuccess,
            ReadItemAndWaitForResult(first.get(), &first_read));
  ASSERT_TRUE(first_read);
  EXPECT_EQ(first_content, *first_read);

  std::unique_ptr<std::vector<char>> second_read;
  ASSERT_EQ(OperationResult::kSuccess,
            ReadItemAndWaitForResult(second.get(), &second_read));
  ASSERT_TRUE(second_read);
  EXPECT_EQ(second_content, *second_read);

  EXPECT_EQ(OperationResult::kSuccess, DeleteItemAndWaitForResult(first.get()));

  // The second extension item is still writable after the first extension's one
  // went away.
  EXPECT_EQ(OperationResult::kSuccess,
            WriteItemAndWaitForResult(second.get(), {'x'}));
}

TEST_F(DataItemTest, ReadNonRegisteredItem) {
  std::unique_ptr<DataItem> item =
      CreateDataItem("data_id", extension()->id(), GenerateKey("key_1"));

  EXPECT_EQ(OperationResult::kNotFound,
            ReadItemAndWaitForResult(item.get(), nullptr));
}

TEST_F(DataItemTest, ReadOldFile) {
  std::unique_ptr<DataItem> writer = CreateAndRegisterDataItem(
      "data_id", extension()->id(), GenerateKey("key_1"));

  std::vector<char> content = {'f', 'i', 'l', 'e', '_', '1'};
  EXPECT_EQ(OperationResult::kSuccess,
            WriteItemAndWaitForResult(writer.get(), content));
  writer.reset();

  std::unique_ptr<DataItem> reader =
      CreateDataItem("data_id", extension()->id(), GenerateKey("key_1"));
  std::unique_ptr<std::vector<char>> read_content;
  ASSERT_EQ(OperationResult::kSuccess,
            ReadItemAndWaitForResult(reader.get(), &read_content));
  ASSERT_TRUE(read_content);
  EXPECT_EQ(content, *read_content);
}

TEST_F(DataItemTest, RepeatedWrite) {
  std::unique_ptr<DataItem> writer = CreateAndRegisterDataItem(
      "data_id", extension()->id(), GenerateKey("key_1"));

  OperationResult write_result = OperationResult::kFailed;
  std::vector<char> first_write = {'f', 'i', 'l', 'e', '_', '1'};
  std::vector<char> second_write = {'f', 'i', 'l', 'e', '_', '2'};

  writer->Write(first_write, base::BindOnce(&WriteCallback, base::DoNothing(),
                                            &write_result));
  EXPECT_EQ(OperationResult::kSuccess,
            WriteItemAndWaitForResult(writer.get(), second_write));

  std::unique_ptr<DataItem> reader =
      CreateDataItem("data_id", extension()->id(), GenerateKey("key_1"));
  std::unique_ptr<std::vector<char>> read_content;
  ASSERT_EQ(OperationResult::kSuccess,
            ReadItemAndWaitForResult(reader.get(), &read_content));
  ASSERT_TRUE(read_content);
  EXPECT_EQ(second_write, *read_content);
}

TEST_F(DataItemTest, ReadDeletedAndReregisteredItem) {
  std::unique_ptr<DataItem> item = CreateAndRegisterDataItem(
      "data_id", extension()->id(), GenerateKey("key_1"));

  std::vector<char> content = {'f', 'i', 'l', 'e', '_', '1'};
  EXPECT_EQ(OperationResult::kSuccess,
            WriteItemAndWaitForResult(item.get(), content));

  EXPECT_EQ(OperationResult::kSuccess, DeleteItemAndWaitForResult(item.get()));

  std::unique_ptr<DataItem> duplicate = CreateAndRegisterDataItem(
      "data_id", extension()->id(), GenerateKey("key_1"));

  std::unique_ptr<std::vector<char>> read;
  ASSERT_EQ(OperationResult::kSuccess,
            ReadItemAndWaitForResult(duplicate.get(), &read));
  ASSERT_TRUE(read);
  EXPECT_TRUE(read->empty());
}

TEST_F(DataItemTest, ReadEmpty) {
  std::unique_ptr<DataItem> item = CreateAndRegisterDataItem(
      "data_id", extension()->id(), GenerateKey("key_1"));

  std::unique_ptr<std::vector<char>> read_content;
  ASSERT_EQ(OperationResult::kSuccess,
            ReadItemAndWaitForResult(item.get(), &read_content));
  ASSERT_TRUE(read_content);
  EXPECT_TRUE(read_content->empty());

  ASSERT_EQ(OperationResult::kSuccess, DeleteItemAndWaitForResult(item.get()));

  EXPECT_EQ(OperationResult::kNotFound,
            ReadItemAndWaitForResult(item.get(), nullptr));
}

TEST_F(DataItemTest, ReadDeletedItem) {
  std::unique_ptr<DataItem> item = CreateAndRegisterDataItem(
      "data_id", extension()->id(), GenerateKey("key_1"));

  std::vector<char> content = {'f', 'i', 'l', 'e', '_', '1'};
  EXPECT_EQ(OperationResult::kSuccess,
            WriteItemAndWaitForResult(item.get(), content));

  ASSERT_EQ(OperationResult::kSuccess, DeleteItemAndWaitForResult(item.get()));

  EXPECT_EQ(OperationResult::kNotFound,
            ReadItemAndWaitForResult(item.get(), nullptr));
}

TEST_F(DataItemTest, WriteDeletedItem) {
  std::unique_ptr<DataItem> item = CreateAndRegisterDataItem(
      "data_id", extension()->id(), GenerateKey("key_1"));

  std::vector<char> content = {'f', 'i', 'l', 'e', '_', '1'};
  EXPECT_EQ(OperationResult::kSuccess,
            WriteItemAndWaitForResult(item.get(), content));

  ASSERT_EQ(OperationResult::kSuccess, DeleteItemAndWaitForResult(item.get()));

  EXPECT_EQ(OperationResult::kNotFound,
            WriteItemAndWaitForResult(item.get(), content));
}

TEST_F(DataItemTest, WriteWithInvalidKey) {
  std::unique_ptr<DataItem> item =
      CreateAndRegisterDataItem("data_id", extension()->id(), "invalid");

  std::vector<char> content = {'f', 'i', 'l', 'e', '_', '1'};
  EXPECT_EQ(OperationResult::kInvalidKey,
            WriteItemAndWaitForResult(item.get(), content));
}

TEST_F(DataItemTest, ReadWithInvalidKey) {
  std::unique_ptr<DataItem> item = CreateAndRegisterDataItem(
      "data_id", extension()->id(), GenerateKey("key_1"));

  std::vector<char> content = {'f', 'i', 'l', 'e', '_', '1'};
  EXPECT_EQ(OperationResult::kSuccess,
            WriteItemAndWaitForResult(item.get(), content));

  std::unique_ptr<DataItem> reader =
      CreateDataItem("data_id", extension()->id(), "invalid");
  EXPECT_EQ(OperationResult::kInvalidKey,
            ReadItemAndWaitForResult(reader.get(), nullptr));
}

TEST_F(DataItemTest, ReadWithWrongKey) {
  std::unique_ptr<DataItem> item = CreateAndRegisterDataItem(
      "data_id", extension()->id(), GenerateKey("key_1"));

  std::vector<char> content = {'f', 'i', 'l', 'e', '_', '1'};
  EXPECT_EQ(OperationResult::kSuccess,
            WriteItemAndWaitForResult(item.get(), content));

  std::unique_ptr<DataItem> reader =
      CreateDataItem("data_id", extension()->id(), GenerateKey("key_2"));
  EXPECT_EQ(OperationResult::kWrongKey,
            ReadItemAndWaitForResult(reader.get(), nullptr));
}

TEST_F(DataItemTest, ResetBeforeCallback) {
  std::unique_ptr<DataItem> writer = CreateAndRegisterDataItem(
      "data_id", extension()->id(), GenerateKey("key_1"));

  std::vector<char> content = {'f', 'i', 'l', 'e', '_', '1'};
  writer->Write(content,
                base::BindOnce(&WriteCallbackNotCalled, "Reset writer"));
  writer.reset();

  std::unique_ptr<DataItem> reader =
      CreateDataItem("data_id", extension()->id(), GenerateKey("key_1"));
  std::unique_ptr<std::vector<char>> read_content;
  ASSERT_EQ(OperationResult::kSuccess,
            ReadItemAndWaitForResult(reader.get(), &read_content));
  ASSERT_TRUE(read_content);
  EXPECT_EQ(content, *read_content);

  reader->Read(base::BindOnce(&ReadCallbackNotCalled, "Reset read"));
  reader.reset();

  std::unique_ptr<DataItem> deleter =
      CreateDataItem("data_id", extension()->id(), GenerateKey("key_1"));
  deleter->Delete(base::BindOnce(&WriteCallbackNotCalled, "Reset deleter"));
  deleter.reset();

  DrainTaskRunner();

  // Verify item write fails now the item's been deleted.
  std::unique_ptr<DataItem> second_writer =
      CreateDataItem("data_id", extension()->id(), GenerateKey("key_1"));
  EXPECT_EQ(OperationResult::kNotFound,
            WriteItemAndWaitForResult(second_writer.get(), content));
}

TEST_F(DataItemTest, DeleteAllForExtension) {
  std::unique_ptr<DataItem> item_1 = CreateAndRegisterDataItem(
      "data_id_1", extension()->id(), GenerateKey("key_1"));
  ASSERT_TRUE(item_1);

  std::unique_ptr<DataItem> item_2 = CreateAndRegisterDataItem(
      "data_id_2", extension()->id(), GenerateKey("key_1"));
  ASSERT_TRUE(item_2);

  DeleteAllItems(extension()->id());

  std::set<std::string> item_ids;
  ASSERT_EQ(OperationResult::kSuccess,
            GetRegisteredItemIds(extension()->id(), &item_ids));
  EXPECT_TRUE(item_ids.empty());

  std::unique_ptr<DataItem> new_item = CreateAndRegisterDataItem(
      "data_id_1", extension()->id(), GenerateKey("key_1"));
  ASSERT_TRUE(item_2);

  ASSERT_EQ(OperationResult::kSuccess,
            GetRegisteredItemIds(extension()->id(), &item_ids));
  EXPECT_EQ(std::set<std::string>({"data_id_1"}), item_ids);
}

}  // namespace lock_screen_data
}  // namespace extensions
