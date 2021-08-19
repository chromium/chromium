// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/shared_storage_worklet/shared_storage_worklet_global_scope.h"

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "content/services/shared_storage_worklet/worklet_v8_helper.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace shared_storage_worklet {

namespace {

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
  std::string key;
  std::string value;
  bool ignore_if_present;
};

class TestClient
    : public shared_storage_worklet::mojom::SharedStorageWorkletServiceClient {
 public:
  explicit TestClient(scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : task_runner_(task_runner) {}

  void SetFromWorkletScope(const std::string& key,
                           const std::string& value,
                           bool ignore_if_present,
                           SetFromWorkletScopeCallback callback) override {
    observed_set_params_.push_back({key, value, ignore_if_present});

    task_runner_->PostTask(
        FROM_HERE,
        base::BindLambdaForTesting([callback = std::move(callback)]() mutable {
          std::move(callback).Run(/*success=*/true, /*error_message=*/{});
        }));
  }

  void AppendFromWorkletScope(
      const std::string& key,
      const std::string& value,
      AppendFromWorkletScopeCallback callback) override {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindLambdaForTesting([callback = std::move(callback)]() mutable {
          std::move(callback).Run(
              /*success=*/false,
              /*error_message=*/"testing error message for append");
        }));
  }

  void DeleteFromWorkletScope(
      const std::string& key,
      DeleteFromWorkletScopeCallback callback) override {}

  void ClearFromWorkletScope(ClearFromWorkletScopeCallback callback) override {}

  void GetFromWorkletScope(const std::string& key,
                           GetFromWorkletScopeCallback callback) override {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindLambdaForTesting([callback = std::move(callback)]() mutable {
          std::move(callback).Run(
              /*success=*/true,
              /*error_message=*/{},
              /*value=*/"test-value");
        }));
  }

  void KeyFromWorkletScope(uint32_t pos,
                           KeyFromWorkletScopeCallback callback) override {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindLambdaForTesting([callback = std::move(callback)]() mutable {
          std::move(callback).Run(
              /*success=*/true,
              /*error_message=*/{},
              /*key=*/"test-key");
        }));
  }

  void LengthFromWorkletScope(
      LengthFromWorkletScopeCallback callback) override {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindLambdaForTesting([callback = std::move(callback)]() mutable {
          std::move(callback).Run(
              /*success=*/true,
              /*error_message=*/{},
              /*length=*/1);
        }));
  }

  void ConsoleLogFromWorkletScope(const std::string& message) override {
    observed_console_log_messages_.push_back(message);
  }

  const std::vector<SetParams>& observed_set_params() const {
    return observed_set_params_;
  }

  const std::vector<std::string>& observed_console_log_messages() const {
    return observed_console_log_messages_;
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

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
      test_client_.get(), GURL("https://example.test"),
      base::DoNothing::Once<bool, const std::string&>(),
      /*response_body=*/std::make_unique<std::string>(),
      /*error_message=*/{});

  EXPECT_TRUE(IsolateInitialized());

  EXPECT_EQ(GetTypeOf("console"), "object");
  EXPECT_EQ(GetTypeOf("console.log"), "function");
  EXPECT_EQ(GetTypeOf("registerURLSelectionOperation"), "function");
  EXPECT_EQ(GetTypeOf("registerOperation"), "function");
  EXPECT_EQ(GetTypeOf("sharedStorage"), "object");
  EXPECT_EQ(GetTypeOf("sharedStorage"), "object");
  EXPECT_EQ(GetTypeOf("sharedStorage.set"), "function");
  EXPECT_EQ(GetTypeOf("sharedStorage.append"), "function");
  EXPECT_EQ(GetTypeOf("sharedStorage.delete"), "function");
  EXPECT_EQ(GetTypeOf("sharedStorage.clear"), "function");
  EXPECT_EQ(GetTypeOf("sharedStorage.get"), "function");
  EXPECT_EQ(GetTypeOf("sharedStorage.key"), "function");
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
      const std::vector<std::string>& urls,
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

  SimulateRunURLSelectionOperation("test-operation", {"url0", "url1"},
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

  SimulateRunURLSelectionOperation("test-operation", {"url0", "url1"},
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

  SimulateRunURLSelectionOperation("test-operation", {"url0", "url1"},
                                   /*serialized_data=*/{});

  EXPECT_FALSE(url_selection_operation_finished());

  task_environment_.RunUntilIdle();

  EXPECT_TRUE(url_selection_operation_finished());
  EXPECT_TRUE(url_selection_operation_success());
  EXPECT_TRUE(url_selection_operation_error_message().empty());
  EXPECT_EQ(url_selection_operation_index(), 1u);
}

TEST_F(SharedStorageRunOperationTest,
       URLSelectionOperation_ResultPromiseRejectedAsynchronously) {
  SimulateAddModule(R"(
      class TestClass {
        async run(urls) {
          return sharedStorage.get('test-key');
        }
      }

      registerURLSelectionOperation("test-operation", TestClass);
    )");

  SimulateRunURLSelectionOperation("test-operation", {"url0", "url1"},
                                   /*serialized_data=*/{});

  EXPECT_FALSE(url_selection_operation_finished());

  task_environment_.RunUntilIdle();

  EXPECT_TRUE(url_selection_operation_finished());
  EXPECT_FALSE(url_selection_operation_success());
  EXPECT_EQ(url_selection_operation_error_message(),
            "Promise did not resolve to an uint32 number.");
  EXPECT_EQ(url_selection_operation_index(), 0u);
}

class SharedStorageObjectMethodTest : public SharedStorageRunOperationTest {
 public:
  SharedStorageObjectMethodTest() {
    // Run AddModule so that sharedStorage is exposed.
    SimulateAddModule(R"()");
  }

  void ExecuteScriptAndWatchPromiseResolvedResult(
      const std::string& script_body) {
    WorkletV8Helper::HandleScope scope(Isolate());
    v8::Local<v8::Context> context = LocalContext();
    v8::Context::Scope context_scope(context);

    v8::Local<v8::Value> v8_result = EvalJs(script_body);
    ASSERT_TRUE(!v8_result.IsEmpty());
    ASSERT_TRUE(v8_result->IsPromise());

    v8::Local<v8::Promise> v8_result_promise = v8_result.As<v8::Promise>();
    if (v8_result_promise->State() == v8::Promise::PromiseState::kPending) {
      task_environment_.RunUntilIdle();
    } else {
      finished_synchronously_ = true;
    }

    ASSERT_TRUE(v8_result_promise->State() !=
                v8::Promise::PromiseState::kPending);

    fulfilled_ =
        (v8_result_promise->State() == v8::Promise::PromiseState::kFulfilled);

    v8_resolved_value_ =
        v8::Global<v8::Value>(Isolate(), v8_result_promise->Result());
  }

  bool finished_synchronously() const { return finished_synchronously_; }

  bool fulfilled() const { return fulfilled_; }

  v8::Local<v8::Value> v8_resolved_value() {
    return v8_resolved_value_.Get(Isolate());
  }

 private:
  bool finished_synchronously_ = false;
  bool fulfilled_ = false;
  v8::Global<v8::Value> v8_resolved_value_;
};

TEST_F(SharedStorageObjectMethodTest, SetOperation_MissingKey) {
  ExecuteScriptAndWatchPromiseResolvedResult("sharedStorage.set()");

  EXPECT_TRUE(finished_synchronously());
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
  ExecuteScriptAndWatchPromiseResolvedResult("sharedStorage.set('key')");

  EXPECT_TRUE(finished_synchronously());
  EXPECT_FALSE(fulfilled());

  {
    WorkletV8Helper::HandleScope scope(Isolate());
    EXPECT_TRUE(v8_resolved_value()->IsString());
    EXPECT_EQ(gin::V8ToString(Isolate(), v8_resolved_value()),
              "Missing or invalid \"value\" argument in sharedStorage.set()");
  }

  EXPECT_TRUE(test_client()->observed_set_params().empty());
}

TEST_F(SharedStorageObjectMethodTest, SetOperation_InvalidValue) {
  ExecuteScriptAndWatchPromiseResolvedResult("sharedStorage.set('key', 123)");

  EXPECT_TRUE(finished_synchronously());
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
  ExecuteScriptAndWatchPromiseResolvedResult(
      "sharedStorage.set('key', 'value', true)");

  EXPECT_TRUE(finished_synchronously());
  EXPECT_FALSE(fulfilled());

  {
    WorkletV8Helper::HandleScope scope(Isolate());
    EXPECT_TRUE(v8_resolved_value()->IsString());
    EXPECT_EQ(gin::V8ToString(Isolate(), v8_resolved_value()),
              "Invalid \"options\" argument in sharedStorage.set()");
  }
}

TEST_F(SharedStorageObjectMethodTest, SetOperation_FulfilledAsynchronously) {
  ExecuteScriptAndWatchPromiseResolvedResult(
      "sharedStorage.set('key', 'value')");

  EXPECT_FALSE(finished_synchronously());
  EXPECT_TRUE(fulfilled());

  {
    WorkletV8Helper::HandleScope scope(Isolate());
    EXPECT_TRUE(v8_resolved_value()->IsUndefined());
  }

  EXPECT_EQ(test_client()->observed_set_params().size(), 1u);
  EXPECT_EQ(test_client()->observed_set_params()[0].key, "key");
  EXPECT_EQ(test_client()->observed_set_params()[0].value, "value");
  EXPECT_FALSE(test_client()->observed_set_params()[0].ignore_if_present);
}

TEST_F(SharedStorageObjectMethodTest, SetOperation_IgnoreIfPresent) {
  ExecuteScriptAndWatchPromiseResolvedResult(
      "sharedStorage.set('key', 'value', {ignoreIfPresent: true})");

  EXPECT_FALSE(finished_synchronously());
  EXPECT_TRUE(fulfilled());

  {
    WorkletV8Helper::HandleScope scope(Isolate());
    EXPECT_TRUE(v8_resolved_value()->IsUndefined());
  }

  EXPECT_EQ(test_client()->observed_set_params().size(), 1u);
  EXPECT_EQ(test_client()->observed_set_params()[0].key, "key");
  EXPECT_EQ(test_client()->observed_set_params()[0].value, "value");
  EXPECT_TRUE(test_client()->observed_set_params()[0].ignore_if_present);
}

TEST_F(SharedStorageObjectMethodTest, AppendOperation_MissingKey) {
  ExecuteScriptAndWatchPromiseResolvedResult("sharedStorage.append()");

  EXPECT_TRUE(finished_synchronously());
  EXPECT_FALSE(fulfilled());

  WorkletV8Helper::HandleScope scope(Isolate());
  EXPECT_TRUE(v8_resolved_value()->IsString());
  EXPECT_EQ(gin::V8ToString(Isolate(), v8_resolved_value()),
            "Missing or invalid \"key\" argument in sharedStorage.append()");
}

TEST_F(SharedStorageObjectMethodTest, AppendOperation_MissingValue) {
  ExecuteScriptAndWatchPromiseResolvedResult("sharedStorage.append('key')");

  EXPECT_TRUE(finished_synchronously());
  EXPECT_FALSE(fulfilled());

  WorkletV8Helper::HandleScope scope(Isolate());
  EXPECT_TRUE(v8_resolved_value()->IsString());
  EXPECT_EQ(gin::V8ToString(Isolate(), v8_resolved_value()),
            "Missing or invalid \"value\" argument in sharedStorage.append()");
}

TEST_F(SharedStorageObjectMethodTest, AppendOperation_RejectedAsynchronously) {
  ExecuteScriptAndWatchPromiseResolvedResult(
      "sharedStorage.append('key', 'value')");

  EXPECT_FALSE(finished_synchronously());
  EXPECT_FALSE(fulfilled());

  WorkletV8Helper::HandleScope scope(Isolate());
  EXPECT_TRUE(v8_resolved_value()->IsString());
  EXPECT_EQ(gin::V8ToString(Isolate(), v8_resolved_value()),
            "testing error message for append");
}

TEST_F(SharedStorageObjectMethodTest, DeleteOperation_MissingKey) {
  ExecuteScriptAndWatchPromiseResolvedResult("sharedStorage.delete()");

  EXPECT_TRUE(finished_synchronously());
  EXPECT_FALSE(fulfilled());

  WorkletV8Helper::HandleScope scope(Isolate());
  EXPECT_TRUE(v8_resolved_value()->IsString());
  EXPECT_EQ(gin::V8ToString(Isolate(), v8_resolved_value()),
            "Missing or invalid \"key\" argument in sharedStorage.delete()");
}

TEST_F(SharedStorageObjectMethodTest, GetOperation_MissingKey) {
  ExecuteScriptAndWatchPromiseResolvedResult("sharedStorage.get()");

  EXPECT_TRUE(finished_synchronously());
  EXPECT_FALSE(fulfilled());

  WorkletV8Helper::HandleScope scope(Isolate());
  EXPECT_TRUE(v8_resolved_value()->IsString());
  EXPECT_EQ(gin::V8ToString(Isolate(), v8_resolved_value()),
            "Missing or invalid \"key\" argument in sharedStorage.get()");
}

TEST_F(SharedStorageObjectMethodTest, GetOperation_FulfilledAsynchronously) {
  ExecuteScriptAndWatchPromiseResolvedResult("sharedStorage.get('key')");

  EXPECT_FALSE(finished_synchronously());
  EXPECT_TRUE(fulfilled());

  WorkletV8Helper::HandleScope scope(Isolate());
  EXPECT_TRUE(v8_resolved_value()->IsString());
  EXPECT_EQ(gin::V8ToString(Isolate(), v8_resolved_value()), "test-value");
}

TEST_F(SharedStorageObjectMethodTest, KeyOperation_MissingPos) {
  ExecuteScriptAndWatchPromiseResolvedResult("sharedStorage.key()");

  EXPECT_TRUE(finished_synchronously());
  EXPECT_FALSE(fulfilled());

  WorkletV8Helper::HandleScope scope(Isolate());
  EXPECT_TRUE(v8_resolved_value()->IsString());
  EXPECT_EQ(gin::V8ToString(Isolate(), v8_resolved_value()),
            "Missing or invalid \"pos\" argument in sharedStorage.key()");
}

TEST_F(SharedStorageObjectMethodTest, KeyOperation_FulfilledAsynchronously) {
  ExecuteScriptAndWatchPromiseResolvedResult("sharedStorage.key(123)");

  EXPECT_FALSE(finished_synchronously());
  EXPECT_TRUE(fulfilled());

  WorkletV8Helper::HandleScope scope(Isolate());
  EXPECT_TRUE(v8_resolved_value()->IsString());
  EXPECT_EQ(gin::V8ToString(Isolate(), v8_resolved_value()), "test-key");
}

TEST_F(SharedStorageObjectMethodTest, LengthOperation_FulfilledAsynchronously) {
  ExecuteScriptAndWatchPromiseResolvedResult("sharedStorage.length()");

  EXPECT_FALSE(finished_synchronously());
  EXPECT_TRUE(fulfilled());

  WorkletV8Helper::HandleScope scope(Isolate());
  EXPECT_TRUE(v8_resolved_value()->IsNumber());

  uint32_t n = 0;
  gin::Converter<uint32_t>::FromV8(Isolate(), v8_resolved_value(), &n);
  EXPECT_EQ(n, 1u);
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
