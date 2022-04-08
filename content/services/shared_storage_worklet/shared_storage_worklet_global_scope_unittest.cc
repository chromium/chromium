// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/shared_storage_worklet/shared_storage_worklet_global_scope.h"

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/services/storage/shared_storage/public/mojom/shared_storage.mojom.h"
#include "content/services/shared_storage_worklet/worklet_v8_helper.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-value-serializer.h"

namespace shared_storage_worklet {

namespace {

std::vector<shared_storage_worklet::mojom::SharedStorageKeyAndOrValuePtr>
CreateBatchResult(
    std::vector<std::pair<std::u16string, std::u16string>> input) {
  std::vector<shared_storage_worklet::mojom::SharedStorageKeyAndOrValuePtr>
      result;
  for (const auto& p : input) {
    shared_storage_worklet::mojom::SharedStorageKeyAndOrValuePtr e =
        shared_storage_worklet::mojom::SharedStorageKeyAndOrValue::New(
            p.first, p.second);
    result.push_back(std::move(e));
  }
  return result;
}

std::vector<uint8_t> Serialize(v8::Isolate* isolate,
                               v8::Local<v8::Context> context,
                               v8::Local<v8::Value> v8_value) {
  v8::ValueSerializer serializer(isolate);

  bool wrote_value;
  CHECK(serializer.WriteValue(context, v8_value).To(&wrote_value));
  CHECK(wrote_value);

  std::pair<uint8_t*, size_t> buffer = serializer.Release();

  std::vector<uint8_t> serialized_data(buffer.first,
                                       buffer.first + buffer.second);

  DCHECK_EQ(serialized_data.size(), buffer.second);

  free(buffer.first);

  return serialized_data;
}

struct SetParams {
  std::u16string key;
  std::u16string value;
  bool ignore_if_present;
};

class TestClient
    : public shared_storage_worklet::mojom::SharedStorageWorkletServiceClient {
 public:
  explicit TestClient(scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : task_runner_(task_runner) {}

  void SharedStorageSet(const std::u16string& key,
                        const std::u16string& value,
                        bool ignore_if_present,
                        SharedStorageSetCallback callback) override {
    observed_set_params_.push_back({key, value, ignore_if_present});

    task_runner_->PostTask(
        FROM_HERE,
        base::BindLambdaForTesting([callback = std::move(callback)]() mutable {
          std::move(callback).Run(/*success=*/true, /*error_message=*/{});
        }));
  }

  void SharedStorageAppend(const std::u16string& key,
                           const std::u16string& value,
                           SharedStorageAppendCallback callback) override {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindLambdaForTesting([callback = std::move(callback)]() mutable {
          std::move(callback).Run(
              /*success=*/false,
              /*error_message=*/"testing error message for append");
        }));
  }

  void SharedStorageDelete(const std::u16string& key,
                           SharedStorageDeleteCallback callback) override {}

  void SharedStorageClear(SharedStorageClearCallback callback) override {}

  void SharedStorageGet(const std::u16string& key,
                        SharedStorageGetCallback callback) override {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindLambdaForTesting([callback = std::move(callback)]() mutable {
          std::move(callback).Run(
              /*success=*/true,
              /*error_message=*/{},
              /*value=*/u"test-value");
        }));
  }

  void SharedStorageKeys(
      mojo::PendingRemote<
          shared_storage_worklet::mojom::SharedStorageEntriesListener>
          pending_listener) override {
    pending_keys_listeners_.push_back(std::move(pending_listener));
  }

  void SharedStorageEntries(
      mojo::PendingRemote<
          shared_storage_worklet::mojom::SharedStorageEntriesListener>
          pending_listener) override {
    pending_entries_listeners_.push_back(std::move(pending_listener));
  }

  void SharedStorageLength(SharedStorageLengthCallback callback) override {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindLambdaForTesting([callback = std::move(callback)]() mutable {
          std::move(callback).Run(
              /*success=*/true,
              /*error_message=*/{},
              /*length=*/1);
        }));
  }

  void ConsoleLog(const std::string& message) override {
    observed_console_log_messages_.push_back(message);
  }

  const std::vector<SetParams>& observed_set_params() const {
    return observed_set_params_;
  }

  const std::vector<std::string>& observed_console_log_messages() const {
    return observed_console_log_messages_;
  }

  size_t pending_keys_listeners_count() const {
    return pending_keys_listeners_.size();
  }

  size_t pending_entries_listeners_count() const {
    return pending_entries_listeners_.size();
  }

  mojo::Remote<shared_storage_worklet::mojom::SharedStorageEntriesListener>
  OfferKeysListenerAtFront() {
    CHECK(!pending_keys_listeners_.empty());

    auto pending_listener = std::move(pending_keys_listeners_.front());
    pending_keys_listeners_.pop_front();

    return mojo::Remote<
        shared_storage_worklet::mojom::SharedStorageEntriesListener>(
        std::move(pending_listener));
  }

  mojo::Remote<shared_storage_worklet::mojom::SharedStorageEntriesListener>
  OfferEntriesListenerAtFront() {
    CHECK(!pending_entries_listeners_.empty());

    auto pending_listener = std::move(pending_entries_listeners_.front());
    pending_entries_listeners_.pop_front();

    return mojo::Remote<
        shared_storage_worklet::mojom::SharedStorageEntriesListener>(
        std::move(pending_listener));
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  std::deque<mojo::PendingRemote<
      shared_storage_worklet::mojom::SharedStorageEntriesListener>>
      pending_keys_listeners_;

  std::deque<mojo::PendingRemote<
      shared_storage_worklet::mojom::SharedStorageEntriesListener>>
      pending_entries_listeners_;

  std::vector<SetParams> observed_set_params_;
  std::vector<std::string> observed_console_log_messages_;
};

}  // namespace

class SharedStorageWorkletGlobalScopeTest : public testing::Test {
 public:
  SharedStorageWorkletGlobalScopeTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    test_client_ = std::make_unique<TestClient>(
        task_environment_.GetMainThreadTaskRunner());
    global_scope_ = std::make_unique<SharedStorageWorkletGlobalScope>();
  }

  ~SharedStorageWorkletGlobalScopeTest() override = default;

  v8::Isolate* Isolate() { return global_scope_->Isolate(); }

  bool IsolateInitialized() { return !!global_scope_->isolate_holder_; }

  v8::Local<v8::Context> LocalContext() {
    return global_scope_->LocalContext();
  }

  v8::Local<v8::Value> EvalJs(const std::string& src) {
    std::string error_message;
    return WorkletV8Helper::CompileAndRunScript(LocalContext(), src,
                                                GURL("https://example.test"),
                                                &error_message)
        .ToLocalChecked();
  }

  std::string GetTypeOf(std::string operand) {
    WorkletV8Helper::HandleScope scope(Isolate());
    v8::Local<v8::Context> context = LocalContext();
    v8::Context::Scope context_scope(context);

    v8::Local<v8::Value> result = EvalJs("typeof " + operand);
    return gin::V8ToString(Isolate(), result);
  }

  TestClient* test_client() { return test_client_.get(); }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;

  std::unique_ptr<TestClient> test_client_;

  std::unique_ptr<SharedStorageWorkletGlobalScope> global_scope_;
};

TEST_F(SharedStorageWorkletGlobalScopeTest, IsolateNotInitializedByDefault) {
  EXPECT_FALSE(IsolateInitialized());
}

TEST_F(SharedStorageWorkletGlobalScopeTest, OnModuleScriptDownloadedSuccess) {
  global_scope_->OnModuleScriptDownloaded(
      test_client_.get(), GURL("https://example.test"), base::DoNothing(),
      /*response_body=*/std::make_unique<std::string>(),
      /*error_message=*/{});

  EXPECT_TRUE(IsolateInitialized());

  EXPECT_EQ(GetTypeOf("console"), "object");
  EXPECT_EQ(GetTypeOf("console.log"), "function");
  EXPECT_EQ(GetTypeOf("registerURLSelectionOperation"), "function");
  EXPECT_EQ(GetTypeOf("registerOperation"), "function");
  EXPECT_EQ(GetTypeOf("sharedStorage"), "object");
  EXPECT_EQ(GetTypeOf("sharedStorage.set"), "function");
  EXPECT_EQ(GetTypeOf("sharedStorage.append"), "function");
  EXPECT_EQ(GetTypeOf("sharedStorage.delete"), "function");
  EXPECT_EQ(GetTypeOf("sharedStorage.clear"), "function");
  EXPECT_EQ(GetTypeOf("sharedStorage.get"), "function");
  EXPECT_EQ(GetTypeOf("sharedStorage.keys"), "function");
  EXPECT_EQ(GetTypeOf("sharedStorage.entries"), "function");
  EXPECT_EQ(GetTypeOf("sharedStorage.length"), "function");
}

TEST_F(SharedStorageWorkletGlobalScopeTest, OnModuleScriptDownloadedWithError) {
  bool callback_called = false;
  auto cb = base::BindLambdaForTesting(
      [&](bool success, const std::string& error_message) {
        EXPECT_FALSE(success);
        EXPECT_EQ(error_message, "error1");
        callback_called = true;
      });

  global_scope_->OnModuleScriptDownloaded(test_client_.get(),
                                          GURL("https://example.test"),
                                          std::move(cb), nullptr, "error1");

  EXPECT_FALSE(IsolateInitialized());
  EXPECT_TRUE(callback_called);
}

class SharedStorageAddModuleTest : public SharedStorageWorkletGlobalScopeTest {
 public:
  void SimulateAddModule(const std::string& script_body) {
    bool callback_called = false;

    auto cb = base::BindLambdaForTesting(
        [&](bool success, const std::string& error_message) {
          success_ = success;
          error_message_ = error_message;

          callback_called = true;
        });

    global_scope_->OnModuleScriptDownloaded(
        test_client_.get(), GURL("https://example.test"), std::move(cb),
        std::make_unique<std::string>(script_body), /*error_message=*/{});

    ASSERT_TRUE(callback_called);
  }

  bool success() const { return success_; }

  const std::string& error_message() const { return error_message_; }

 private:
  bool success_ = false;
  std::string error_message_;
};

TEST_F(SharedStorageAddModuleTest, VanillaScriptSuccess) {
  SimulateAddModule(R"(
    a = 1;
  )");

  EXPECT_TRUE(success());
  EXPECT_TRUE(error_message().empty());
  EXPECT_EQ(GetTypeOf("a"), "number");
}

TEST_F(SharedStorageAddModuleTest, VanillaScriptError) {
  SimulateAddModule(R"(
    a;
  )");

  EXPECT_FALSE(success());
  EXPECT_EQ(
      error_message(),
      "https://example.test/:2 Uncaught ReferenceError: a is not defined.");
}

TEST_F(SharedStorageAddModuleTest, ObjectDefinedStatusDuringAddModule) {
  SimulateAddModule(R"(
    if (typeof(console) !== 'object' ||
        typeof(registerOperation) !== 'function' ||
        typeof(registerURLSelectionOperation) !== 'function' ||
        typeof(sharedStorage) !== 'undefined') {
      throw Error('Unexpected object defined status.');
    }
  )");

  EXPECT_TRUE(success());
  EXPECT_TRUE(error_message().empty());
}

TEST_F(SharedStorageAddModuleTest, RegisterOperation_MissingOperationName) {
  SimulateAddModule(R"(
    registerOperation();
  )");

  EXPECT_FALSE(success());
  EXPECT_EQ(error_message(),
            "https://example.test/:2 Uncaught TypeError: Missing "
            "\"name\" argument in operation registration.");
}

TEST_F(SharedStorageAddModuleTest, RegisterOperation_EmptyOperationName) {
  SimulateAddModule(R"(
    registerOperation("");
  )");

  EXPECT_FALSE(success());
  EXPECT_EQ(error_message(),
            "https://example.test/:2 Uncaught TypeError: Operation name "
            "cannot be empty.");
}

TEST_F(SharedStorageAddModuleTest,
       RegisterOperation_MissingClassName_MissingArgument) {
  SimulateAddModule(R"(
    registerOperation("test-operation");
  )");

  EXPECT_FALSE(success());
  EXPECT_EQ(error_message(),
            "https://example.test/:2 Uncaught TypeError: Missing class "
            "name argument in operation registration.");
}

TEST_F(SharedStorageAddModuleTest,
       RegisterOperation_MissingClassName_NotAnObject) {
  SimulateAddModule(R"(
    registerOperation("test-operation", 1);
  )");

  EXPECT_FALSE(success());
  EXPECT_EQ(error_message(),
            "https://example.test/:2 Uncaught TypeError: Missing class "
            "name argument in operation registration.");
}

TEST_F(SharedStorageAddModuleTest, RegisterOperation_ClassNameNotAConstructor) {
  SimulateAddModule(R"(
    registerOperation("test-operation", {});
  )");

  EXPECT_FALSE(success());
  EXPECT_EQ(error_message(),
            "https://example.test/:2 Uncaught TypeError: Unexpected class "
            "argument: not a constructor.");
}

TEST_F(SharedStorageAddModuleTest, RegisterOperation_MissingRunFunction) {
  SimulateAddModule(R"(
    class TestClass {
      constructor() {
        this.run = 1;
      }
    }

    registerOperation("test-operation", TestClass);
  )");

  EXPECT_FALSE(success());
  EXPECT_EQ(error_message(),
            "https://example.test/:8 Uncaught TypeError: Missing \"run()\" "
            "function in the class.");
}

TEST_F(SharedStorageAddModuleTest, RegisterOperation_Success) {
  SimulateAddModule(R"(
    class TestClass {
      async run() {}
    }

    registerOperation("test-operation", TestClass);
  )");

  EXPECT_TRUE(success());
  EXPECT_TRUE(error_message().empty());
}

TEST_F(SharedStorageAddModuleTest, RegisterOperation_AlreadyRegistered) {
  SimulateAddModule(R"(
    class TestClass1 {
      async run() {}
    }

    class TestClass2 {
      async run() {}
    }

    registerOperation("test-operation", TestClass1);
    registerOperation("test-operation", TestClass2);
  )");

  EXPECT_FALSE(success());
  EXPECT_EQ(error_message(),
            "https://example.test/:11 Uncaught TypeError: Operation name "
            "already registered.");
}

class SharedStorageRunOperationTest
    : public SharedStorageWorkletGlobalScopeTest {
 public:
  // The caller should provide a valid module script. The purpose of this test
  // suite is to test RunOperation.
  void SimulateAddModule(const std::string& script_body) {
    bool add_module_callback_called = false;

    auto add_module_callback = base::BindLambdaForTesting(
        [&](bool success, const std::string& error_message) {
          DCHECK(success);
          add_module_callback_called = true;
        });

    global_scope_->OnModuleScriptDownloaded(
        test_client_.get(), GURL("https://example.test"),
        std::move(add_module_callback),
        std::make_unique<std::string>(script_body), /*error_message=*/{});

    ASSERT_TRUE(add_module_callback_called);
  }

  void SimulateRunOperation(const std::string& name,
                            const std::vector<uint8_t>& serialized_data) {
    auto run_operation_callback = base::BindLambdaForTesting(
        [&](bool success, const std::string& error_message) {
          unnamed_operation_finished_ = true;
          unnamed_operation_success_ = success;
          unnamed_operation_error_message_ = error_message;
        });

    global_scope_->RunOperation(name, serialized_data,
                                std::move(run_operation_callback));
  }

  void SimulateRunURLSelectionOperation(
      const std::string& name,
      const std::vector<GURL>& urls,
      const std::vector<uint8_t>& serialized_data) {
    auto run_operation_callback = base::BindLambdaForTesting(
        [&](bool success, const std::string& error_message, uint32_t index) {
          url_selection_operation_finished_ = true;
          url_selection_operation_success_ = success;
          url_selection_operation_error_message_ = error_message;
          url_selection_operation_index_ = index;
        });

    global_scope_->RunURLSelectionOperation(name, urls, serialized_data,
                                            std::move(run_operation_callback));
  }

  bool unnamed_operation_finished() const {
    return unnamed_operation_finished_;
  }

  bool unnamed_operation_success() const { return unnamed_operation_success_; }

  const std::string& unnamed_operation_error_message() const {
    return unnamed_operation_error_message_;
  }

  bool url_selection_operation_finished() const {
    return url_selection_operation_finished_;
  }

  bool url_selection_operation_success() const {
    return url_selection_operation_success_;
  }

  const std::string& url_selection_operation_error_message() const {
    return url_selection_operation_error_message_;
  }

  uint32_t url_selection_operation_index() const {
    return url_selection_operation_index_;
  }

 private:
  bool unnamed_operation_finished_ = false;
  bool unnamed_operation_success_ = false;
  std::string unnamed_operation_error_message_;

  bool url_selection_operation_finished_ = false;
  bool url_selection_operation_success_ = false;
  std::string url_selection_operation_error_message_;
  uint32_t url_selection_operation_index_ = -1;
};

TEST_F(SharedStorageRunOperationTest, UnnamedOperation_BeforeAddModuleFinish) {
  SimulateRunOperation("test-operation-1", /*serialized_data=*/{});

  EXPECT_TRUE(unnamed_operation_finished());
  EXPECT_FALSE(unnamed_operation_success());
  EXPECT_EQ(unnamed_operation_error_message(),
            "The module script hasn't been loaded.");
}

TEST_F(SharedStorageRunOperationTest,
       UnnamedOperation_OperationNameNotRegistered) {
  SimulateAddModule(R"(
      class TestClass {
        async run() {}
      }

      registerOperation("test-operation", TestClass);
    )");

  SimulateRunOperation("test-operation-1", /*serialized_data=*/{});

  EXPECT_TRUE(unnamed_operation_finished());
  EXPECT_FALSE(unnamed_operation_success());
  EXPECT_EQ(unnamed_operation_error_message(), "Cannot find operation name.");
}

TEST_F(SharedStorageRunOperationTest, UnnamedOperation_FunctionError) {
  SimulateAddModule(R"(
      class TestClass {
        run() {
          a;
        }
      }

      registerOperation("test-operation", TestClass);
    )");

  SimulateRunOperation("test-operation", /*serialized_data=*/{});

  EXPECT_TRUE(unnamed_operation_finished());
  EXPECT_FALSE(unnamed_operation_success());
  EXPECT_EQ(
      unnamed_operation_error_message(),
      "https://example.test/:4 Uncaught ReferenceError: a is not defined.");
}

TEST_F(SharedStorageRunOperationTest, UnnamedOperation_ReturnValueNotAPromise) {
  SimulateAddModule(R"(
      class TestClass {
        run() {}
      }

      registerOperation("test-operation", TestClass);
    )");

  SimulateRunOperation("test-operation", /*serialized_data=*/{});

  EXPECT_TRUE(unnamed_operation_finished());
  EXPECT_FALSE(unnamed_operation_success());
  EXPECT_EQ(unnamed_operation_error_message(),
            "run() did not return a promise.");
}

TEST_F(SharedStorageRunOperationTest, UnnamedOperation_Microtask) {
  SimulateAddModule(R"(
      class TestClass {
        async run() {
          await Promise.resolve(0);
          return 0;
        }
      }

      registerOperation("test-operation", TestClass);
    )");

  SimulateRunOperation("test-operation", /*serialized_data=*/{});

  EXPECT_TRUE(unnamed_operation_finished());
  EXPECT_TRUE(unnamed_operation_success());
  EXPECT_TRUE(unnamed_operation_error_message().empty());
}

TEST_F(SharedStorageRunOperationTest,
       UnnamedOperation_ResultPromiseFulfilledSynchronously) {
  SimulateAddModule(R"(
      class TestClass {
        async run() {}
      }

      registerOperation("test-operation", TestClass);
    )");

  SimulateRunOperation("test-operation", /*serialized_data=*/{});

  EXPECT_TRUE(unnamed_operation_finished());
  EXPECT_TRUE(unnamed_operation_success());
  EXPECT_TRUE(unnamed_operation_error_message().empty());
}

TEST_F(SharedStorageRunOperationTest,
       UnnamedOperation_ResultPromiseRejectedSynchronously) {
  SimulateAddModule(R"(
      class TestClass {
        async run() {
          a;
        }
      }

      registerOperation("test-operation", TestClass);
    )");

  SimulateRunOperation("test-operation", /*serialized_data=*/{});

  EXPECT_TRUE(unnamed_operation_finished());
  EXPECT_FALSE(unnamed_operation_success());
  EXPECT_EQ(unnamed_operation_error_message(),
            "ReferenceError: a is not defined");
}

TEST_F(SharedStorageRunOperationTest,
       UnnamedOperation_ResultPromiseFulfilledAsynchronously) {
  SimulateAddModule(R"(
      class TestClass {
        async run() {
          return sharedStorage.set('key', 'value');
        }
      }

      registerOperation("test-operation", TestClass);
    )");

  SimulateRunOperation("test-operation", /*serialized_data=*/{});

  EXPECT_FALSE(unnamed_operation_finished());

  task_environment_.RunUntilIdle();

  EXPECT_TRUE(unnamed_operation_finished());
  EXPECT_TRUE(unnamed_operation_success());
  EXPECT_TRUE(unnamed_operation_error_message().empty());
}

TEST_F(SharedStorageRunOperationTest,
       UnnamedOperation_ResultPromiseRejectedAsynchronously) {
  SimulateAddModule(R"(
      class TestClass {
        async run() {
          return sharedStorage.append('key', 'value');
        }
      }

      registerOperation("test-operation", TestClass);
    )");

  SimulateRunOperation("test-operation", /*serialized_data=*/{});

  EXPECT_FALSE(unnamed_operation_finished());

  task_environment_.RunUntilIdle();

  EXPECT_TRUE(unnamed_operation_finished());
  EXPECT_FALSE(unnamed_operation_success());
  EXPECT_EQ(unnamed_operation_error_message(),
            "testing error message for append");
}

TEST_F(SharedStorageRunOperationTest, UnnamedOperation_ExpectedCustomData) {
  SimulateAddModule(R"(
      class TestClass {
        async run(data) {
          if (data.customField != 'customValue') {
            throw 'Unexpected value for customField field';
          }
        }
      }

      registerOperation("test-operation", TestClass);
    )");

  std::vector<uint8_t> serialized_data;
  {
    WorkletV8Helper::HandleScope scope(Isolate());
    v8::Local<v8::Context> context = LocalContext();
    v8::Context::Scope context_scope(context);

    v8::Local<v8::Object> obj = v8::Object::New(Isolate());
    gin::Dictionary dict(Isolate(), obj);
    dict.Set<std::string>("customField", std::string("customValue"));
    serialized_data = Serialize(Isolate(), LocalContext(), obj);
  }

  SimulateRunOperation("test-operation", serialized_data);

  EXPECT_TRUE(unnamed_operation_finished());
  EXPECT_TRUE(unnamed_operation_success());
  EXPECT_TRUE(unnamed_operation_error_message().empty());
}

TEST_F(SharedStorageRunOperationTest, UnnamedOperation_UnexpectedCustomData) {
  SimulateAddModule(R"(
      class TestClass {
        async run(data) {
          if (data.customField != 'customValue') {
            throw 'Unexpected value for customField field';
          }
        }
      }

      registerOperation("test-operation", TestClass);
    )");

  std::vector<uint8_t> serialized_data;
  {
    WorkletV8Helper::HandleScope scope(Isolate());
    v8::Local<v8::Context> context = LocalContext();
    v8::Context::Scope context_scope(context);

    v8::Local<v8::Object> obj = v8::Object::New(Isolate());
    gin::Dictionary dict(Isolate(), obj);
    dict.Set<std::string>("customField", std::string("customValue123"));
    serialized_data = Serialize(Isolate(), LocalContext(), obj);
  }

  SimulateRunOperation("test-operation", serialized_data);

  EXPECT_TRUE(unnamed_operation_finished());
  EXPECT_FALSE(unnamed_operation_success());
  EXPECT_EQ(unnamed_operation_error_message(),
            "Unexpected value for customField field");
}

TEST_F(SharedStorageRunOperationTest,
       URLSelectionOperation_ResultPromiseFulfilledSynchronously) {
  SimulateAddModule(R"(
      class TestClass {
        async run(urls) {
          return 1;
        }
      }

      registerURLSelectionOperation("test-operation", TestClass);
    )");

  SimulateRunURLSelectionOperation(
      "test-operation", {GURL("https://foo.com"), GURL("https://bar.com")},
      /*serialized_data=*/{});

  EXPECT_TRUE(url_selection_operation_finished());
  EXPECT_TRUE(url_selection_operation_success());
  EXPECT_TRUE(url_selection_operation_error_message().empty());
  EXPECT_EQ(url_selection_operation_index(), 1u);
}

TEST_F(SharedStorageRunOperationTest,
       URLSelectionOperation_ResultPromiseRejectedSynchronously) {
  SimulateAddModule(R"(
      class TestClass {
        async run(urls) {
          return '';
        }
      }

      registerURLSelectionOperation("test-operation", TestClass);
    )");

  SimulateRunURLSelectionOperation(
      "test-operation", {GURL("https://foo.com"), GURL("https://bar.com")},
      /*serialized_data=*/{});

  EXPECT_TRUE(url_selection_operation_finished());
  EXPECT_FALSE(url_selection_operation_success());
  EXPECT_EQ(url_selection_operation_error_message(),
            "Promise did not resolve to an uint32 number.");
  EXPECT_EQ(url_selection_operation_index(), 0u);
}

TEST_F(SharedStorageRunOperationTest,
       URLSelectionOperation_ResultPromiseFulfilledAsynchronously) {
  SimulateAddModule(R"(
      class TestClass {
        async run(urls) {
          return sharedStorage.length();
        }
      }

      registerURLSelectionOperation("test-operation", TestClass);
    )");

  SimulateRunURLSelectionOperation(
      "test-operation", {GURL("https://foo.com"), GURL("https://bar.com")},
      /*serialized_data=*/{});

  EXPECT_FALSE(url_selection_operation_finished());

  task_environment_.RunUntilIdle();

  EXPECT_TRUE(url_selection_operation_finished());
  EXPECT_TRUE(url_selection_operation_success());
  EXPECT_TRUE(url_selection_operation_error_message().empty());
  EXPECT_EQ(url_selection_operation_index(), 1u);
}

TEST_F(
    SharedStorageRunOperationTest,
    URLSelectionOperation_ResultPromiseRejectedAsynchronously_ReturnValueNotUint32) {
  SimulateAddModule(R"(
      class TestClass {
        async run(urls) {
          return sharedStorage.get('test-key');
        }
      }

      registerURLSelectionOperation("test-operation", TestClass);
    )");

  SimulateRunURLSelectionOperation(
      "test-operation", {GURL("https://foo.com"), GURL("https://bar.com")},
      /*serialized_data=*/{});

  EXPECT_FALSE(url_selection_operation_finished());

  task_environment_.RunUntilIdle();

  EXPECT_TRUE(url_selection_operation_finished());
  EXPECT_FALSE(url_selection_operation_success());
  EXPECT_EQ(url_selection_operation_error_message(),
            "Promise did not resolve to an uint32 number.");
  EXPECT_EQ(url_selection_operation_index(), 0u);
}

TEST_F(
    SharedStorageRunOperationTest,
    URLSelectionOperation_ResultPromiseRejectedAsynchronously_ReturnValueOutOfRange) {
  SimulateAddModule(R"(
      class TestClass {
        async run(urls) {
          return sharedStorage.length(); // this would return 1 for this test
        }
      }

      registerURLSelectionOperation("test-operation", TestClass);
    )");

  SimulateRunURLSelectionOperation("test-operation", {GURL("https://foo.com")},
                                   /*serialized_data=*/{});

  EXPECT_FALSE(url_selection_operation_finished());

  task_environment_.RunUntilIdle();

  EXPECT_TRUE(url_selection_operation_finished());
  EXPECT_FALSE(url_selection_operation_success());
  EXPECT_EQ(
      url_selection_operation_error_message(),
      "Promise resolved to a number outside the length of the input urls.");
  EXPECT_EQ(url_selection_operation_index(), 0u);
}

class SharedStorageObjectMethodTest : public SharedStorageRunOperationTest {
 public:
  SharedStorageObjectMethodTest() {
    // Run AddModule so that sharedStorage is exposed.
    SimulateAddModule(R"()");
  }

  void ExecuteScript(const std::string& script_body) {
    WorkletV8Helper::HandleScope scope(Isolate());
    v8::Local<v8::Context> context = LocalContext();
    v8::Context::Scope context_scope(context);

    v8::Local<v8::Value> v8_result = EvalJs(script_body);
    ASSERT_TRUE(!v8_result.IsEmpty());
    ASSERT_TRUE(v8_result->IsPromise());

    v8_result_promise_ =
        v8::Global<v8::Promise>(Isolate(), v8_result.As<v8::Promise>());
  }

  bool finished() {
    WorkletV8Helper::HandleScope scope(Isolate());
    v8::Local<v8::Promise> v8_result_promise =
        v8_result_promise_.Get(Isolate());
    return v8_result_promise->State() != v8::Promise::PromiseState::kPending;
  }

  bool fulfilled() {
    WorkletV8Helper::HandleScope scope(Isolate());
    v8::Local<v8::Promise> v8_result_promise =
        v8_result_promise_.Get(Isolate());
    return v8_result_promise->State() == v8::Promise::PromiseState::kFulfilled;
  }

  v8::Local<v8::Value> v8_resolved_value() {
    DCHECK(finished());
    v8::Local<v8::Promise> v8_result_promise =
        v8_result_promise_.Get(Isolate());
    return v8_result_promise->Result();
  }

 private:
  v8::Global<v8::Promise> v8_result_promise_;
};

TEST_F(SharedStorageObjectMethodTest, SetOperation_MissingKey) {
  ExecuteScript("sharedStorage.set()");
  EXPECT_TRUE(finished());
  EXPECT_FALSE(fulfilled());

  {
    WorkletV8Helper::HandleScope scope(Isolate());
    EXPECT_TRUE(v8_resolved_value()->IsString());
    EXPECT_EQ(gin::V8ToString(Isolate(), v8_resolved_value()),
              "Missing or invalid \"key\" argument in sharedStorage.set()");
  }

  EXPECT_TRUE(test_client()->observed_set_params().empty());
}

TEST_F(SharedStorageObjectMethodTest, SetOperation_InvalidKey_Empty) {
  ExecuteScript("sharedStorage.set('', 'value')");
  EXPECT_TRUE(finished());
  EXPECT_FALSE(fulfilled());

  {
    WorkletV8Helper::HandleScope scope(Isolate());
    EXPECT_TRUE(v8_resolved_value()->IsString());
    EXPECT_EQ(gin::V8ToString(Isolate(), v8_resolved_value()),
              "Missing or invalid \"key\" argument in sharedStorage.set()");
  }

  EXPECT_TRUE(test_client()->observed_set_params().empty());
}

TEST_F(SharedStorageObjectMethodTest, SetOperation_InvalidKey_NotAString) {
  ExecuteScript("sharedStorage.set(123, 'value')");
  EXPECT_TRUE(finished());
  EXPECT_FALSE(fulfilled());

  {
    WorkletV8Helper::HandleScope scope(Isolate());
    EXPECT_TRUE(v8_resolved_value()->IsString());
    EXPECT_EQ(gin::V8ToString(Isolate(), v8_resolved_value()),
              "Missing or invalid \"key\" argument in sharedStorage.set()");
  }

  EXPECT_TRUE(test_client()->observed_set_params().empty());
}

TEST_F(SharedStorageObjectMethodTest, SetOperation_InvalidKey_LengthTooBig) {
  ExecuteScript("sharedStorage.set('a'.repeat(1025), 'value')");
  EXPECT_TRUE(finished());
  EXPECT_FALSE(fulfilled());

  {
    WorkletV8Helper::HandleScope scope(Isolate());
    EXPECT_TRUE(v8_resolved_value()->IsString());
    EXPECT_EQ(gin::V8ToString(Isolate(), v8_resolved_value()),
              "Missing or invalid \"key\" argument in sharedStorage.set()");
  }

  EXPECT_TRUE(test_client()->observed_set_params().empty());
}

TEST_F(SharedStorageObjectMethodTest, SetOperation_MissingValue) {
  ExecuteScript("sharedStorage.set('key')");
  EXPECT_TRUE(finished());
  EXPECT_FALSE(fulfilled());

  {
    WorkletV8Helper::HandleScope scope(Isolate());
    EXPECT_TRUE(v8_resolved_value()->IsString());
    EXPECT_EQ(gin::V8ToString(Isolate(), v8_resolved_value()),
              "Missing or invalid \"value\" argument in sharedStorage.set()");
  }

  EXPECT_TRUE(test_client()->observed_set_params().empty());
}

TEST_F(SharedStorageObjectMethodTest, SetOperation_InvalidValue_NotAString) {
  ExecuteScript("sharedStorage.set('key', 123)");
  EXPECT_TRUE(finished());
  EXPECT_FALSE(fulfilled());

  {
    WorkletV8Helper::HandleScope scope(Isolate());
    EXPECT_TRUE(v8_resolved_value()->IsString());
    EXPECT_EQ(gin::V8ToString(Isolate(), v8_resolved_value()),
              "Missing or invalid \"value\" argument in sharedStorage.set()");
  }

  EXPECT_TRUE(test_client()->observed_set_params().empty());
}

TEST_F(SharedStorageObjectMethodTest, SetOperation_InvalidValue_LengthTooBig) {
  ExecuteScript("sharedStorage.set('key', 'a'.repeat(1025))");
  EXPECT_TRUE(finished());
  EXPECT_FALSE(fulfilled());

  {
    WorkletV8Helper::HandleScope scope(Isolate());
    EXPECT_TRUE(v8_resolved_value()->IsString());
    EXPECT_EQ(gin::V8ToString(Isolate(), v8_resolved_value()),
              "Missing or invalid \"value\" argument in sharedStorage.set()");
  }

  EXPECT_TRUE(test_client()->observed_set_params().empty());
}

TEST_F(SharedStorageObjectMethodTest, SetOperation_InvalidOptions) {
  ExecuteScript("sharedStorage.set('key', 'value', true)");
  EXPECT_TRUE(finished());
  EXPECT_FALSE(fulfilled());

  {
    WorkletV8Helper::HandleScope scope(Isolate());
    EXPECT_TRUE(v8_resolved_value()->IsString());
    EXPECT_EQ(gin::V8ToString(Isolate(), v8_resolved_value()),
              "Invalid \"options\" argument in sharedStorage.set()");
  }
}

TEST_F(SharedStorageObjectMethodTest, SetOperation_FulfilledAsynchronously) {
  ExecuteScript("sharedStorage.set('key', 'value')");
  EXPECT_FALSE(finished());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(finished());
  EXPECT_TRUE(fulfilled());

  {
    WorkletV8Helper::HandleScope scope(Isolate());
    EXPECT_TRUE(v8_resolved_value()->IsUndefined());
  }

  EXPECT_EQ(test_client()->observed_set_params().size(), 1u);
  EXPECT_EQ(test_client()->observed_set_params()[0].key, u"key");
  EXPECT_EQ(test_client()->observed_set_params()[0].value, u"value");
  EXPECT_FALSE(test_client()->observed_set_params()[0].ignore_if_present);
}

TEST_F(SharedStorageObjectMethodTest, SetOperation_IgnoreIfPresent) {
  ExecuteScript("sharedStorage.set('key', 'value', {ignoreIfPresent: true})");
  EXPECT_FALSE(finished());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(finished());
  EXPECT_TRUE(fulfilled());

  {
    WorkletV8Helper::HandleScope scope(Isolate());
    EXPECT_TRUE(v8_resolved_value()->IsUndefined());
  }

  EXPECT_EQ(test_client()->observed_set_params().size(), 1u);
  EXPECT_EQ(test_client()->observed_set_params()[0].key, u"key");
  EXPECT_EQ(test_client()->observed_set_params()[0].value, u"value");
  EXPECT_TRUE(test_client()->observed_set_params()[0].ignore_if_present);
}

TEST_F(SharedStorageObjectMethodTest, AppendOperation_MissingKey) {
  ExecuteScript("sharedStorage.append()");
  EXPECT_TRUE(finished());
  EXPECT_FALSE(fulfilled());

  {
    WorkletV8Helper::HandleScope scope(Isolate());
    EXPECT_TRUE(v8_resolved_value()->IsString());
    EXPECT_EQ(gin::V8ToString(Isolate(), v8_resolved_value()),
              "Missing or invalid \"key\" argument in sharedStorage.append()");
  }
}

TEST_F(SharedStorageObjectMethodTest, AppendOperation_InvalidKey_Empty) {
  ExecuteScript("sharedStorage.append('', 'value')");
  EXPECT_TRUE(finished());
  EXPECT_FALSE(fulfilled());

  {
    WorkletV8Helper::HandleScope scope(Isolate());
    EXPECT_TRUE(v8_resolved_value()->IsString());
    EXPECT_EQ(gin::V8ToString(Isolate(), v8_resolved_value()),
              "Missing or invalid \"key\" argument in sharedStorage.append()");
  }
}

TEST_F(SharedStorageObjectMethodTest, AppendOperation_InvalidKey_NotAString) {
  ExecuteScript("sharedStorage.append(123, 'value')");
  EXPECT_TRUE(finished());
  EXPECT_FALSE(fulfilled());

  {
    WorkletV8Helper::HandleScope scope(Isolate());
    EXPECT_TRUE(v8_resolved_value()->IsString());
    EXPECT_EQ(gin::V8ToString(Isolate(), v8_resolved_value()),
              "Missing or invalid \"key\" argument in sharedStorage.append()");
  }
}

TEST_F(SharedStorageObjectMethodTest, AppendOperation_InvalidKey_LengthTooBig) {
  ExecuteScript("sharedStorage.append('a'.repeat(1025), 'value')");
  EXPECT_TRUE(finished());
  EXPECT_FALSE(fulfilled());

  {
    WorkletV8Helper::HandleScope scope(Isolate());
    EXPECT_TRUE(v8_resolved_value()->IsString());
    EXPECT_EQ(gin::V8ToString(Isolate(), v8_resolved_value()),
              "Missing or invalid \"key\" argument in sharedStorage.append()");
  }
}

TEST_F(SharedStorageObjectMethodTest, AppendOperation_MissingValue) {
  ExecuteScript("sharedStorage.append('key')");
  EXPECT_TRUE(finished());
  EXPECT_FALSE(fulfilled());

  {
    WorkletV8Helper::HandleScope scope(Isolate());
    EXPECT_TRUE(v8_resolved_value()->IsString());
    EXPECT_EQ(
        gin::V8ToString(Isolate(), v8_resolved_value()),
        "Missing or invalid \"value\" argument in sharedStorage.append()");
  }
}

TEST_F(SharedStorageObjectMethodTest, AppendOperation_InvalidValue_NotAString) {
  ExecuteScript("sharedStorage.append('key', 123)");
  EXPECT_TRUE(finished());
  EXPECT_FALSE(fulfilled());

  {
    WorkletV8Helper::HandleScope scope(Isolate());
    EXPECT_TRUE(v8_resolved_value()->IsString());
    EXPECT_EQ(
        gin::V8ToString(Isolate(), v8_resolved_value()),
        "Missing or invalid \"value\" argument in sharedStorage.append()");
  }
}

TEST_F(SharedStorageObjectMethodTest,
       AppendOperation_InvalidValue_LengthTooBig) {
  ExecuteScript("sharedStorage.append('key', 'a'.repeat(1025))");
  EXPECT_TRUE(finished());
  EXPECT_FALSE(fulfilled());

  {
    WorkletV8Helper::HandleScope scope(Isolate());
    EXPECT_TRUE(v8_resolved_value()->IsString());
    EXPECT_EQ(
        gin::V8ToString(Isolate(), v8_resolved_value()),
        "Missing or invalid \"value\" argument in sharedStorage.append()");
  }
}

TEST_F(SharedStorageObjectMethodTest, AppendOperation_RejectedAsynchronously) {
  ExecuteScript("sharedStorage.append('key', 'value')");
  EXPECT_FALSE(finished());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(finished());
  EXPECT_FALSE(fulfilled());

  WorkletV8Helper::HandleScope scope(Isolate());
  EXPECT_TRUE(v8_resolved_value()->IsString());
  EXPECT_EQ(gin::V8ToString(Isolate(), v8_resolved_value()),
            "testing error message for append");
}

TEST_F(SharedStorageObjectMethodTest, DeleteOperation_MissingKey) {
  ExecuteScript("sharedStorage.delete()");
  EXPECT_TRUE(finished());
  EXPECT_FALSE(fulfilled());

  WorkletV8Helper::HandleScope scope(Isolate());
  EXPECT_TRUE(v8_resolved_value()->IsString());
  EXPECT_EQ(gin::V8ToString(Isolate(), v8_resolved_value()),
            "Missing or invalid \"key\" argument in sharedStorage.delete()");
}

TEST_F(SharedStorageObjectMethodTest, GetOperation_MissingKey) {
  ExecuteScript("sharedStorage.get()");
  EXPECT_TRUE(finished());
  EXPECT_FALSE(fulfilled());

  WorkletV8Helper::HandleScope scope(Isolate());
  EXPECT_TRUE(v8_resolved_value()->IsString());
  EXPECT_EQ(gin::V8ToString(Isolate(), v8_resolved_value()),
            "Missing or invalid \"key\" argument in sharedStorage.get()");
}

TEST_F(SharedStorageObjectMethodTest, GetOperation_FulfilledAsynchronously) {
  ExecuteScript("sharedStorage.get('key')");
  EXPECT_FALSE(finished());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(finished());
  EXPECT_TRUE(fulfilled());

  WorkletV8Helper::HandleScope scope(Isolate());
  EXPECT_TRUE(v8_resolved_value()->IsString());
  EXPECT_EQ(gin::V8ToString(Isolate(), v8_resolved_value()), "test-value");
}

TEST_F(SharedStorageObjectMethodTest, LengthOperation_FulfilledAsynchronously) {
  ExecuteScript("sharedStorage.length()");
  EXPECT_FALSE(finished());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(finished());
  EXPECT_TRUE(fulfilled());

  WorkletV8Helper::HandleScope scope(Isolate());
  EXPECT_TRUE(v8_resolved_value()->IsNumber());

  uint32_t n = 0;
  gin::Converter<uint32_t>::FromV8(Isolate(), v8_resolved_value(), &n);
  EXPECT_EQ(n, 1u);
}

TEST_F(SharedStorageObjectMethodTest,
       EntriesOperationAsyncIterator_OneEmptyBatch_Success) {
  ExecuteScript(R"(
    (async () => {
      for await (const [key, value] of sharedStorage.entries()) {
        console.log(key + ';' + value);
      }
    })();
  )");
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(finished());
  EXPECT_EQ(test_client()->observed_console_log_messages().size(), 0u);

  EXPECT_EQ(test_client()->pending_entries_listeners_count(), 1u);
  auto remote_listener = test_client()->OfferEntriesListenerAtFront();

  remote_listener->DidReadEntries(
      /*success=*/true, /*error_message=*/{}, CreateBatchResult({}),
      /*has_more_entries=*/false);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(finished());
  EXPECT_TRUE(fulfilled());
  EXPECT_EQ(test_client()->observed_console_log_messages().size(), 0u);
}

TEST_F(SharedStorageObjectMethodTest,
       EntriesOperationAsyncIterator_FirstBatchError_Failure) {
  ExecuteScript(R"(
    (async () => {
      for await (const [key, value] of sharedStorage.entries()) {
        console.log(key + ';' + value);
      }
    })();
  )");
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(finished());
  EXPECT_EQ(test_client()->observed_console_log_messages().size(), 0u);

  EXPECT_EQ(test_client()->pending_entries_listeners_count(), 1u);
  auto remote_listener = test_client()->OfferEntriesListenerAtFront();

  remote_listener->DidReadEntries(
      /*success=*/false, /*error_message=*/"Internal error 12345",
      CreateBatchResult({}),
      /*has_more_entries=*/true);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(finished());
  EXPECT_FALSE(fulfilled());
  EXPECT_EQ(test_client()->observed_console_log_messages().size(), 0u);

  WorkletV8Helper::HandleScope scope(Isolate());
  EXPECT_TRUE(v8_resolved_value()->IsString());
  EXPECT_EQ(gin::V8ToString(Isolate(), v8_resolved_value()),
            "Internal error 12345");
}

TEST_F(SharedStorageObjectMethodTest,
       EntriesOperationAsyncIterator_TwoBatches_Success) {
  ExecuteScript(R"(
    (async () => {
      for await (const [key, value] of sharedStorage.entries()) {
        console.log(key + ';' + value);
      }
    })();
  )");
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(finished());
  EXPECT_EQ(test_client()->observed_console_log_messages().size(), 0u);

  EXPECT_EQ(test_client()->pending_entries_listeners_count(), 1u);
  auto remote_listener = test_client()->OfferEntriesListenerAtFront();

  remote_listener->DidReadEntries(
      /*success=*/true, /*error_message=*/{},
      CreateBatchResult({{u"key0", u"value0"}}),
      /*has_more_entries=*/true);
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(finished());
  EXPECT_EQ(test_client()->observed_console_log_messages().size(), 1u);
  EXPECT_EQ(test_client()->observed_console_log_messages()[0], "key0;value0");

  remote_listener->DidReadEntries(
      /*success=*/true, /*error_message=*/{},
      CreateBatchResult({{u"key1", u"value1"}, {u"key2", u"value2"}}),
      /*has_more_entries=*/false);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(finished());
  EXPECT_TRUE(fulfilled());
  EXPECT_EQ(test_client()->observed_console_log_messages().size(), 3u);
  EXPECT_EQ(test_client()->observed_console_log_messages()[1], "key1;value1");
  EXPECT_EQ(test_client()->observed_console_log_messages()[2], "key2;value2");
}

TEST_F(SharedStorageObjectMethodTest,
       EntriesOperationAsyncIterator_SecondBatchError_Failure) {
  ExecuteScript(R"(
    (async () => {
      for await (const [key, value] of sharedStorage.entries()) {
        console.log(key + ';' + value);
      }
    })();
  )");
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(finished());
  EXPECT_EQ(test_client()->observed_console_log_messages().size(), 0u);

  EXPECT_EQ(test_client()->pending_entries_listeners_count(), 1u);
  auto remote_listener = test_client()->OfferEntriesListenerAtFront();

  remote_listener->DidReadEntries(
      /*success=*/true, /*error_message=*/{},
      CreateBatchResult({{u"key0", u"value0"}}),
      /*has_more_entries=*/true);
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(finished());
  EXPECT_EQ(test_client()->observed_console_log_messages().size(), 1u);
  EXPECT_EQ(test_client()->observed_console_log_messages()[0], "key0;value0");

  remote_listener->DidReadEntries(
      /*success=*/false, /*error_message=*/"Internal error 12345",
      CreateBatchResult({}),
      /*has_more_entries=*/true);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(finished());
  EXPECT_FALSE(fulfilled());
  EXPECT_EQ(test_client()->observed_console_log_messages().size(), 1u);

  WorkletV8Helper::HandleScope scope(Isolate());
  EXPECT_TRUE(v8_resolved_value()->IsString());
  EXPECT_EQ(gin::V8ToString(Isolate(), v8_resolved_value()),
            "Internal error 12345");
}

TEST_F(SharedStorageObjectMethodTest,
       KeysOperationAsyncIterator_OneBatch_Success) {
  ExecuteScript(R"(
    (async () => {
      for await (const key of sharedStorage.keys()) {
        console.log(key);
      }
    })();
  )");
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(finished());
  EXPECT_EQ(test_client()->observed_console_log_messages().size(), 0u);

  EXPECT_EQ(test_client()->pending_keys_listeners_count(), 1u);
  auto remote_listener = test_client()->OfferKeysListenerAtFront();

  // It's harmless to still send the `value` field. They will simply be ignored.
  remote_listener->DidReadEntries(
      /*success=*/true, /*error_message=*/{},
      CreateBatchResult({{u"key0", u"value0"}, {u"key1", u"value1"}}),
      /*has_more_entries=*/false);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(finished());
  EXPECT_TRUE(fulfilled());
  EXPECT_EQ(test_client()->observed_console_log_messages().size(), 2u);
  EXPECT_EQ(test_client()->observed_console_log_messages()[0], "key0");
  EXPECT_EQ(test_client()->observed_console_log_messages()[1], "key1");
}

TEST_F(SharedStorageObjectMethodTest,
       KeysOperationAsyncIterator_ManuallyCallNext) {
  ExecuteScript(R"(
    (async () => {
      const keys_iterator = sharedStorage.keys()[Symbol.asyncIterator]();

      keys_iterator.next(); // result0 skipped
      keys_iterator.next(); // result1 skipped

      const result2 = await keys_iterator.next();
      console.log(JSON.stringify(result2, Object.keys(result2).sort()));

      const result3 = await keys_iterator.next();
      console.log(JSON.stringify(result3, Object.keys(result3).sort()));

      const result4 = await keys_iterator.next();
      console.log(JSON.stringify(result4, Object.keys(result4).sort()));

      const result5 = await keys_iterator.next();
      console.log(JSON.stringify(result5, Object.keys(result5).sort()));
    })();
  )");
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(finished());
  EXPECT_EQ(test_client()->observed_console_log_messages().size(), 0u);

  EXPECT_EQ(test_client()->pending_keys_listeners_count(), 1u);
  auto remote_listener = test_client()->OfferKeysListenerAtFront();

  remote_listener->DidReadEntries(
      /*success=*/true, /*error_message=*/{},
      CreateBatchResult({{u"key0", /*value=*/{}}}),
      /*has_more_entries=*/true);
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(finished());
  EXPECT_EQ(test_client()->observed_console_log_messages().size(), 0u);

  remote_listener->DidReadEntries(
      /*success=*/true, /*error_message=*/{},
      CreateBatchResult({{u"key1", /*value=*/{}}, {u"key2", /*value=*/{}}}),
      /*has_more_entries=*/true);
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(finished());
  EXPECT_EQ(test_client()->observed_console_log_messages().size(), 1u);
  EXPECT_EQ(test_client()->observed_console_log_messages()[0],
            "{\"done\":false,\"value\":\"key2\"}");

  remote_listener->DidReadEntries(
      /*success=*/true, /*error_message=*/{},
      CreateBatchResult({{u"key3", /*value=*/{}}}),
      /*has_more_entries=*/false);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(finished());
  EXPECT_TRUE(fulfilled());
  EXPECT_EQ(test_client()->observed_console_log_messages().size(), 4u);
  EXPECT_EQ(test_client()->observed_console_log_messages()[1],
            "{\"done\":false,\"value\":\"key3\"}");
  EXPECT_EQ(test_client()->observed_console_log_messages()[2],
            "{\"done\":true}");
  EXPECT_EQ(test_client()->observed_console_log_messages()[3],
            "{\"done\":true}");
}

TEST_F(SharedStorageObjectMethodTest, ConsoleLogOperation_NoArgument) {
  {
    WorkletV8Helper::HandleScope scope(Isolate());
    v8::Local<v8::Context> context = LocalContext();
    v8::Context::Scope context_scope(context);

    v8::Local<v8::Value> v8_result = EvalJs("console.log()");
    EXPECT_TRUE(!v8_result.IsEmpty());
    EXPECT_TRUE(v8_result->IsUndefined());
  }

  EXPECT_EQ(test_client()->observed_console_log_messages().size(), 1u);
  EXPECT_EQ(test_client()->observed_console_log_messages()[0], "");
}

TEST_F(SharedStorageObjectMethodTest, ConsoleLogOperation_SingleArgument) {
  {
    WorkletV8Helper::HandleScope scope(Isolate());
    v8::Local<v8::Context> context = LocalContext();
    v8::Context::Scope context_scope(context);

    v8::Local<v8::Value> v8_result = EvalJs("console.log('123')");
    EXPECT_TRUE(!v8_result.IsEmpty());
    EXPECT_TRUE(v8_result->IsUndefined());
  }

  EXPECT_EQ(test_client()->observed_console_log_messages().size(), 1u);
  EXPECT_EQ(test_client()->observed_console_log_messages()[0], "123");
}

TEST_F(SharedStorageObjectMethodTest, ConsoleLogOperation_MultipleArguments) {
  {
    WorkletV8Helper::HandleScope scope(Isolate());
    v8::Local<v8::Context> context = LocalContext();
    v8::Context::Scope context_scope(context);

    v8::Local<v8::Value> v8_result =
        EvalJs("console.log(123, '456', true, undefined, null, {})");
    EXPECT_TRUE(!v8_result.IsEmpty());
    EXPECT_TRUE(v8_result->IsUndefined());
  }

  EXPECT_EQ(test_client()->observed_console_log_messages().size(), 1u);
  EXPECT_EQ(test_client()->observed_console_log_messages()[0],
            "123 456 true undefined null [object Object]");
}

}  // namespace shared_storage_worklet
