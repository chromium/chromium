// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/storage/storage_api.h"

#include <stdint.h>

#include <limits>
#include <memory>
#include <set>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "components/crx_file/id_util.h"
#include "components/value_store/leveldb_value_store.h"
#include "components/value_store/value_store.h"
#include "components/value_store/value_store_factory_impl.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/storage/settings_storage_quota_enforcer.h"
#include "extensions/browser/api/storage/settings_test_util.h"
#include "extensions/browser/api/storage/storage_frontend.h"
#include "extensions/browser/api/storage/storage_utils.h"
#include "extensions/browser/api/storage/value_store_cache.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/api_unittest.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/test_event_router_observer.h"
#include "extensions/browser/test_extensions_browser_client.h"
#include "extensions/common/api/storage.h"
#include "extensions/common/manifest.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace extensions {

namespace {

// Caller owns the returned object.
std::unique_ptr<KeyedService> CreateStorageFrontendForTesting(
    content::BrowserContext* context) {
  scoped_refptr<value_store::ValueStoreFactory> factory =
      new value_store::ValueStoreFactoryImpl(context->GetPath());
  return StorageFrontend::CreateForTesting(factory, context);
}

std::unique_ptr<KeyedService> BuildEventRouter(
    content::BrowserContext* context) {
  return std::make_unique<extensions::EventRouter>(context, nullptr);
}

// A fake ValueStore that can be used to mock a ValueStore for testing.
class FakeValueStore : public value_store::ValueStore {
 public:
  FakeValueStore() = default;
  // Constructor for GetBytesInUseIntOverflow test.
  explicit FakeValueStore(size_t bytes_in_use) : bytes_in_use_(bytes_in_use) {}
  // Constructor for GetOperationExceedsSizeLimit test.
  explicit FakeValueStore(base::Value::Dict large_value)
      : large_value_(std::move(large_value)) {}

  // value_store::ValueStore:
  size_t GetBytesInUse(const std::string& key) override {
    return bytes_in_use_;
  }
  size_t GetBytesInUse(const std::vector<std::string>& keys) override {
    return bytes_in_use_;
  }
  size_t GetBytesInUse() override { return bytes_in_use_; }
  ReadResult Get(const std::vector<std::string>& keys) override {
    return ReadResult(large_value_.Clone(), Status());
  }

  // The following methods are not used in tests and are not implemented.
  ReadResult GetKeys() override { NOTREACHED(); }
  ReadResult Get(const std::string& key) override { NOTREACHED(); }
  ReadResult Get() override { NOTREACHED(); }
  WriteResult Set(WriteOptions options,
                  const std::string& key,
                  const base::Value& value) override {
    NOTREACHED();
  }
  WriteResult Set(WriteOptions options,
                  const base::Value::Dict& values) override {
    NOTREACHED();
  }
  WriteResult Remove(const std::string& key) override { NOTREACHED(); }
  WriteResult Remove(const std::vector<std::string>& keys) override {
    NOTREACHED();
  }
  WriteResult Clear() override { NOTREACHED(); }

 private:
  size_t bytes_in_use_ = 0;
  base::Value::Dict large_value_;
};

// A fake ValueStoreCache that we can assign to a storage area in the
// StorageFrontend. This allows us to call StorageFrontend using an extension
// API and access our mock ValueStore.
class FakeValueStoreCache : public ValueStoreCache {
 public:
  explicit FakeValueStoreCache(FakeValueStore&& store)
      : store_(std::move(store)) {}

  // ValueStoreCache:
  void ShutdownOnUI() override {}
  void RunWithValueStoreForExtension(
      StorageCallback callback,
      scoped_refptr<const Extension> extension) override {
    std::move(callback).Run(&store_);
  }
  void DeleteStorageSoon(const ExtensionId& extension_id) override {}

 private:
  FakeValueStore store_;
};

}  // namespace

class StorageApiUnittest : public ApiUnitTest {
 public:
  StorageApiUnittest() = default;
  ~StorageApiUnittest() override = default;

 protected:
  void SetUp() override {
    ApiUnitTest::SetUp();

    EventRouterFactory::GetInstance()->SetTestingFactory(
        browser_context(), base::BindRepeating(&BuildEventRouter));

    // Ensure a StorageFrontend can be created on demand. The StorageFrontend
    // will be owned by the KeyedService system.
    StorageFrontend::GetFactoryInstance()->SetTestingFactory(
        browser_context(),
        base::BindRepeating(&CreateStorageFrontendForTesting));

    render_process_host_ =
        std::make_unique<content::MockRenderProcessHost>(browser_context());
  }

  void TearDown() override {
    render_process_host_.reset();
    ApiUnitTest::TearDown();
  }

  content::RenderProcessHost* render_process_host() const {
    return render_process_host_.get();
  }

  // Runs the storage.set() API function with local storage.
  void RunSetFunction(const std::string& key, const std::string& value) {
    scoped_refptr<StorageStorageAreaSetFunction> function =
        base::MakeRefCounted<StorageStorageAreaSetFunction>();
    function->set_source_context_type(mojom::ContextType::kPrivilegedExtension);
    RunFunction(function.get(),
                base::StringPrintf("[\"local\", {\"%s\": \"%s\"}]", key.c_str(),
                                   value.c_str()));
  }

  testing::AssertionResult RunSetFunctionWithContextAndExpectSuccess(
      const std::string& storage_area_name,
      const std::string& key,
      const std::string& value,
      mojom::ContextType context_type) {
    scoped_refptr<StorageStorageAreaSetFunction> function =
        base::MakeRefCounted<StorageStorageAreaSetFunction>();
    function->set_source_context_type(context_type);
    function->set_extension(extension());
    std::string args = base::StringPrintf("[\"%s\", {\"%s\": \"%s\"}]",
                                          storage_area_name.c_str(),
                                          key.c_str(), value.c_str());

    api_test_utils::RunFunction(function.get(), args, browser_context());
    std::string error = function->GetError();

    if (error.empty()) {
      return testing::AssertionSuccess();
    }
    return testing::AssertionFailure()
           << "Function failed unexpectedly for " << storage_area_name
           << " with context " << static_cast<int>(context_type)
           << ". Error: " << error;
  }

  testing::AssertionResult RunSetFunctionWithContextAndExpectError(
      const std::string& storage_area_name,
      const std::string& key,
      const std::string& value,
      mojom::ContextType context_type,
      const std::string& expected_error_substring) {
    scoped_refptr<StorageStorageAreaSetFunction> function =
        base::MakeRefCounted<StorageStorageAreaSetFunction>();
    function->set_source_context_type(context_type);
    function->set_extension(extension());
    std::string args = base::StringPrintf("[\"%s\", {\"%s\": \"%s\"}]",
                                          storage_area_name.c_str(),
                                          key.c_str(), value.c_str());

    std::string error = RunFunctionAndReturnError(function.get(), args);
    if (error.empty()) {
      return testing::AssertionFailure()
             << "Expected error containing '" << expected_error_substring
             << "' but function succeeded for " << storage_area_name
             << " with context " << static_cast<int>(context_type);
    }
    if (error.find(expected_error_substring) != std::string::npos) {
      return testing::AssertionSuccess();
    }
    return testing::AssertionFailure()
           << "Expected error containing '" << expected_error_substring
           << "' for " << storage_area_name << " with context "
           << static_cast<int>(context_type) << " but got error: '" << error
           << "'.";
  }

  // Runs the storage.get() API function with the local storage, and populates
  // |out_value| with the string result.
  testing::AssertionResult RunGetFunction(const std::string& key,
                                          std::string* out_value) {
    scoped_refptr<StorageStorageAreaGetFunction> function =
        base::MakeRefCounted<StorageStorageAreaGetFunction>();
    function->set_source_context_type(mojom::ContextType::kPrivilegedExtension);
    std::optional<base::Value> result = RunFunctionAndReturnValue(
        function.get(), base::StringPrintf("[\"local\", \"%s\"]", key.c_str()));
    if (!result) {
      return testing::AssertionFailure() << "No result";
    }

    const base::Value::Dict* dict = result->GetIfDict();
    if (!dict) {
      return testing::AssertionFailure() << *result << " was not a dictionary.";
    }

    const std::string* dict_value = dict->FindString(key);
    if (!dict_value) {
      return testing::AssertionFailure() << " could not retrieve a string from"
          << dict << " at " << key;
    }
    *out_value = *dict_value;

    return testing::AssertionSuccess();
  }

  ExtensionsAPIClient extensions_api_client_;
  std::unique_ptr<content::RenderProcessHost> render_process_host_;
};

TEST_F(StorageApiUnittest,
       StorageAreaAccessControlByContextTypeAndAccessLevel) {
  const std::string kAccessErrorMsg =
      "Access to storage is not allowed from this context.";

  const struct {
    const char* area_name;
    StorageAreaNamespace area_namespace;
  } areas_to_test[] = {
      // `local` and `sync` have similar access control logic and both have
      // untrusted access level by default. Since sync isn't available in this
      // instance of Chrome, we won't test it here.
      {"local", StorageAreaNamespace::kLocal},
      {"session", StorageAreaNamespace::kSession},
  };

  for (const auto& area_info : areas_to_test) {
    // Test Case 1: AccessLevel = TRUSTED_CONTEXTS
    storage_utils::SetAccessLevelForArea(
        extension()->id(), *browser_context(), area_info.area_namespace,
        api::storage::AccessLevel::kTrustedContexts);

    // Privileged context should succeed.
    EXPECT_TRUE(RunSetFunctionWithContextAndExpectSuccess(
        area_info.area_name, "key_priv_ok", "value",
        mojom::ContextType::kPrivilegedExtension));

    // Unprivileged context should fail and get the expected access control
    // error.
    EXPECT_TRUE(RunSetFunctionWithContextAndExpectError(
        area_info.area_name, "key_unpriv_fail", "value",
        mojom::ContextType::kUnprivilegedExtension, kAccessErrorMsg));

    // Test Case 2: AccessLevel = TRUSTED_AND_UNTRUSTED_CONTEXTS
    storage_utils::SetAccessLevelForArea(
        extension()->id(), *browser_context(), area_info.area_namespace,
        api::storage::AccessLevel::kTrustedAndUntrustedContexts);

    // Privileged context should succeed.
    EXPECT_TRUE(RunSetFunctionWithContextAndExpectSuccess(
        area_info.area_name, "key_priv_ok_all_access", "value",
        mojom::ContextType::kPrivilegedExtension));

    // Unprivileged context should also succeed.
    EXPECT_TRUE(RunSetFunctionWithContextAndExpectSuccess(
        area_info.area_name, "key_unpriv_ok_all_access", "value",
        mojom::ContextType::kUnprivilegedExtension));
  }
}

TEST_F(StorageApiUnittest, RestoreCorruptedStorage) {
  const char kKey[] = "key";
  const char kValue[] = "value";
  std::string result;

  // Do a simple set/get combo to make sure the API works.
  RunSetFunction(kKey, kValue);
  EXPECT_TRUE(RunGetFunction(kKey, &result));
  EXPECT_EQ(kValue, result);

  // Corrupt the store. This is not as pretty as ideal, because we use knowledge
  // of the underlying structure, but there's no real good way to corrupt a
  // store other than directly modifying the files.
  value_store::ValueStore* store =
      settings_test_util::GetStorage(extension_ref(), settings_namespace::LOCAL,
                                     StorageFrontend::Get(browser_context()));
  ASSERT_TRUE(store);
  // TODO(cmumford): Modify test as this requires that the factory always
  //                 creates instances of LeveldbValueStore.
  SettingsStorageQuotaEnforcer* quota_store =
      static_cast<SettingsStorageQuotaEnforcer*>(store);
  value_store::LeveldbValueStore* leveldb_store =
      static_cast<value_store::LeveldbValueStore*>(
          quota_store->get_delegate_for_test());
  leveldb::WriteBatch batch;
  batch.Put(kKey, "[{(.*+\"\'\\");
  EXPECT_TRUE(leveldb_store->WriteToDbForTest(&batch));
  EXPECT_TRUE(leveldb_store->Get(kKey).status().IsCorrupted());

  // Running another set should end up working (even though it will restore the
  // store behind the scenes).
  RunSetFunction(kKey, kValue);
  EXPECT_TRUE(RunGetFunction(kKey, &result));
  EXPECT_EQ(kValue, result);
}

TEST_F(StorageApiUnittest, StorageAreaOnChanged) {
  TestEventRouterObserver event_observer(EventRouter::Get(browser_context()));

  EventRouter* event_router = EventRouter::Get(browser_context());
  event_router->AddEventListener(api::storage::OnChanged::kEventName,
                                 render_process_host(), extension()->id());
  event_router->AddEventListener("storage.local.onChanged",
                                 render_process_host(), extension()->id());

  RunSetFunction("key", "value");
  EXPECT_EQ(2u, event_observer.events().size());

  EXPECT_TRUE(base::Contains(event_observer.events(),
                             api::storage::OnChanged::kEventName));
  EXPECT_TRUE(
      base::Contains(event_observer.events(), "storage.local.onChanged"));
}

// Test that no event is dispatched if no listener is added.
TEST_F(StorageApiUnittest, StorageAreaOnChangedNoListener) {
  TestEventRouterObserver event_observer(EventRouter::Get(browser_context()));

  RunSetFunction("key", "value");
  EXPECT_EQ(0u, event_observer.events().size());
}

// Test that no event is dispatched if a listener for a different extension is
// added.
TEST_F(StorageApiUnittest, StorageAreaOnChangedOtherListener) {
  TestEventRouterObserver event_observer(EventRouter::Get(browser_context()));

  EventRouter* event_router = EventRouter::Get(browser_context());
  std::string other_listener_id =
      crx_file::id_util::GenerateId("other-listener");
  event_router->AddEventListener(api::storage::OnChanged::kEventName,
                                 render_process_host(), other_listener_id);
  event_router->AddEventListener("storage.local.onChanged",
                                 render_process_host(), other_listener_id);

  RunSetFunction("key", "value");
  EXPECT_EQ(0u, event_observer.events().size());
}

TEST_F(StorageApiUnittest, StorageAreaOnChangedOnlyOneListener) {
  TestEventRouterObserver event_observer(EventRouter::Get(browser_context()));

  EventRouter* event_router = EventRouter::Get(browser_context());
  event_router->AddEventListener(api::storage::OnChanged::kEventName,
                                 render_process_host(), extension()->id());

  RunSetFunction("key", "value");
  EXPECT_EQ(1u, event_observer.events().size());

  EXPECT_TRUE(base::Contains(event_observer.events(),
                             api::storage::OnChanged::kEventName));
}

// This is a regression test for crbug.com/1483828.
TEST_F(StorageApiUnittest, GetBytesInUseIntOverflow) {
  static constexpr struct TestCase {
    size_t bytes_in_use;
    double result;
  } test_cases[] = {
      {1, 1.0},
      {std::numeric_limits<int>::max(), std::numeric_limits<int>::max()},
      // Test the overflow case from the bug. It's enough to have a value
      // that exceeds the max value that an int can represent.
      {static_cast<size_t>(std::numeric_limits<int>::max()) + 1,
       static_cast<size_t>(std::numeric_limits<int>::max()) + 1}};

  StorageFrontend* frontend = StorageFrontend::Get(browser_context());

  for (const auto& test_case : test_cases) {
    FakeValueStore value_store(test_case.bytes_in_use);
    frontend->SetCacheForTesting(
        settings_namespace::Namespace::LOCAL,
        std::make_unique<FakeValueStoreCache>(std::move(value_store)));

    auto function =
        base::MakeRefCounted<StorageStorageAreaGetBytesInUseFunction>();

    function->set_extension(extension());
    function->set_source_context_type(mojom::ContextType::kPrivilegedExtension);

    std::optional<base::Value> result =
        api_test_utils::RunFunctionAndReturnSingleResult(
            function.get(),
            base::Value::List().Append("local").Append(base::Value()),
            browser_context());
    ASSERT_TRUE(result);
    ASSERT_TRUE(result->is_double());
    EXPECT_EQ(test_case.result, result->GetDouble());
    frontend->DisableStorageForTesting(settings_namespace::Namespace::LOCAL);
  }
}

TEST_F(StorageApiUnittest, GetOperationExceedsSizeLimit) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitFromCommandLine("EnforceStorageGetSizeLimit", "");

  constexpr size_t kMaxSingleGetSizeBytes = 512 * 1024 * 1024;
  base::Value::Dict dict;
  dict.Set("kKeyWithLargeValue", std::string(kMaxSingleGetSizeBytes, 'a'));

  StorageFrontend* frontend = StorageFrontend::Get(browser_context());
  frontend->SetCacheForTesting(
      settings_namespace::Namespace::LOCAL,
      std::make_unique<FakeValueStoreCache>(FakeValueStore(std::move(dict))));

  auto function = base::MakeRefCounted<StorageStorageAreaGetFunction>();
  function->set_extension(extension());
  function->set_source_context_type(mojom::ContextType::kPrivilegedExtension);

  std::string error = api_test_utils::RunFunctionAndReturnError(
      function.get(), "[\"local\", \"kKeyWithLargeValue\"]", browser_context());

  const std::string expected_error_substring = "exceeds the maximum limit";
  EXPECT_TRUE(base::Contains(error, expected_error_substring));

  frontend->DisableStorageForTesting(settings_namespace::Namespace::LOCAL);
}

}  // namespace extensions
