// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/shared_storage_worklet/shared_storage_worklet_global_scope.h"

#include "base/check_op.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "content/services/shared_storage_worklet/worklet_v8_helper.h"
#include "gin/arguments.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "gin/function_template.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/blink/public/mojom/private_aggregation/aggregatable_report.mojom.h"
#include "third_party/blink/public/mojom/private_aggregation/private_aggregation_host.mojom.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage_worklet_service.mojom.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-value-serializer.h"

namespace shared_storage_worklet {

namespace {

constexpr char WorkletContextDefinedHistogram[] =
    "Storage.SharedStorage.Worklet.Context.IsDefined";

std::vector<blink::mojom::SharedStorageKeyAndOrValuePtr> CreateBatchResult(
    std::vector<std::pair<std::u16string, std::u16string>> input) {
  std::vector<blink::mojom::SharedStorageKeyAndOrValuePtr> result;
  for (const auto& p : input) {
    blink::mojom::SharedStorageKeyAndOrValuePtr e =
        blink::mojom::SharedStorageKeyAndOrValue::New(p.first, p.second);
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

struct AppendParams {
  std::u16string key;
  std::u16string value;
};

class TestClient : public blink::mojom::SharedStorageWorkletServiceClient {
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
    observed_append_params_.push_back({key, value});

    task_runner_->PostTask(
        FROM_HERE,
        base::BindLambdaForTesting([callback = std::move(callback)]() mutable {
          std::move(callback).Run(
              /*success=*/false,
              /*error_message=*/"testing error message for append");
        }));
  }

  void SharedStorageDelete(const std::u16string& key,
                           SharedStorageDeleteCallback callback) override {
    observed_delete_params_.push_back(key);
  }

  void SharedStorageClear(SharedStorageClearCallback callback) override {}

  void SharedStorageGet(const std::u16string& key,
                        SharedStorageGetCallback callback) override {
    observed_get_params_.push_back(key);

    task_runner_->PostTask(
        FROM_HERE,
        base::BindLambdaForTesting([callback = std::move(callback)]() mutable {
          std::move(callback).Run(
              blink::mojom::SharedStorageGetStatus::kSuccess,
              /*error_message=*/{},
              /*value=*/u"test-value");
        }));
  }

  void SharedStorageKeys(
      mojo::PendingRemote<blink::mojom::SharedStorageEntriesListener>
          pending_listener) override {
    pending_keys_listeners_.push_back(std::move(pending_listener));
  }

  void SharedStorageEntries(
      mojo::PendingRemote<blink::mojom::SharedStorageEntriesListener>
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

  void SharedStorageRemainingBudget(
      SharedStorageRemainingBudgetCallback callback) override {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindLambdaForTesting([callback = std::move(callback)]() mutable {
          std::move(callback).Run(
              /*success=*/true,
              /*error_message=*/{},
              /*bits=*/2.5);
        }));
  }

  void ConsoleLog(const std::string& message) override {
    observed_console_log_messages_.push_back(message);
  }

  void RecordUseCounters(
      const std::vector<blink::mojom::WebFeature>& features) override {
    ASSERT_THAT(
        features,
        testing::UnorderedElementsAre(
            blink::mojom::WebFeature::kPrivateAggregationApiAll,
            blink::mojom::WebFeature::kPrivateAggregationApiSharedStorage));
    observed_record_use_counter_call_ = true;
  }

  const std::vector<SetParams>& observed_set_params() const {
    return observed_set_params_;
  }

  const std::vector<AppendParams>& observed_append_params() const {
    return observed_append_params_;
  }

  const std::vector<std::u16string>& observed_delete_params() const {
    return observed_delete_params_;
  }

  const std::vector<std::u16string>& observed_get_params() const {
    return observed_get_params_;
  }

  bool observed_record_use_counter_call() const {
    return observed_record_use_counter_call_;
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

  mojo::Remote<blink::mojom::SharedStorageEntriesListener>
  OfferKeysListenerAtFront() {
    CHECK(!pending_keys_listeners_.empty());

    auto pending_listener = std::move(pending_keys_listeners_.front());
    pending_keys_listeners_.pop_front();

    return mojo::Remote<blink::mojom::SharedStorageEntriesListener>(
        std::move(pending_listener));
  }

  mojo::Remote<blink::mojom::SharedStorageEntriesListener>
  OfferEntriesListenerAtFront() {
    CHECK(!pending_entries_listeners_.empty());

    auto pending_listener = std::move(pending_entries_listeners_.front());
    pending_entries_listeners_.pop_front();

    return mojo::Remote<blink::mojom::SharedStorageEntriesListener>(
        std::move(pending_listener));
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  std::deque<mojo::PendingRemote<blink::mojom::SharedStorageEntriesListener>>
      pending_keys_listeners_;

  std::deque<mojo::PendingRemote<blink::mojom::SharedStorageEntriesListener>>
      pending_entries_listeners_;

  std::vector<SetParams> observed_set_params_;
  std::vector<AppendParams> observed_append_params_;
  std::vector<std::u16string> observed_delete_params_;
  std::vector<std::u16string> observed_get_params_;
  std::vector<std::string> observed_console_log_messages_;
  bool observed_record_use_counter_call_ = false;
};

class MockMojomPrivateAggregationHost
    : public blink::mojom::PrivateAggregationHost {
 public:
  // blink::mojom::PrivateAggregationHost:
  MOCK_METHOD(
      void,
      SendHistogramReport,
      (std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>,
       blink::mojom::AggregationServiceMode,
       blink::mojom::DebugModeDetailsPtr),
      (override));
};

}  // namespace

class SharedStorageWorkletGlobalScopeTest : public testing::Test {
 public:
  SharedStorageWorkletGlobalScopeTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    test_client_ = std::make_unique<TestClient>(
        task_environment_.GetMainThreadTaskRunner());
    mock_private_aggregation_host_ =
        std::make_unique<MockMojomPrivateAggregationHost>();
    global_scope_ = std::make_unique<SharedStorageWorkletGlobalScope>(
        /*private_aggregation_permissions_policy_allowed=*/true,
        /*embedder_context=*/absl::nullopt);
  }

  ~SharedStorageWorkletGlobalScopeTest() override = default;

  v8::Isolate* Isolate() { return global_scope_->Isolate(); }

  bool IsolateInitialized() { return !!global_scope_->isolate_holder_; }

  v8::Local<v8::Context> LocalContext() {
    return global_scope_->LocalContext();
  }

  void OverrideGlobalScope(
      std::unique_ptr<SharedStorageWorkletGlobalScope> global_scope) {
    global_scope_ = std::move(global_scope);
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

  void RegisterAsyncReturnForTesting() {
    WorkletV8Helper::HandleScope scope(Isolate());

    v8::Local<v8::Context> context = global_scope_->LocalContext();
    v8::Context::Scope context_scope(context);

    v8::Local<v8::Object> global = context->Global();

    global
        ->Set(context, gin::StringToSymbol(Isolate(), "asyncFulfillForTesting"),
              gin::CreateFunctionTemplate(
                  Isolate(),
                  base::BindRepeating(&SharedStorageWorkletGlobalScopeTest::
                                          AsyncFulfillForTesting,
                                      base::Unretained(this)))
                  ->GetFunction(context)
                  .ToLocalChecked())
        .Check();

    global
        ->Set(
            context, gin::StringToSymbol(Isolate(), "asyncRejectForTesting"),
            gin::CreateFunctionTemplate(
                Isolate(),
                base::BindRepeating(
                    &SharedStorageWorkletGlobalScopeTest::AsyncRejectForTesting,
                    base::Unretained(this)))
                ->GetFunction(context)
                .ToLocalChecked())
        .Check();
  }

  v8::Local<v8::Promise> AsyncFulfillForTesting(gin::Arguments* args) {
    std::vector<v8::Local<v8::Value>> v8_args = args->GetAll();

    v8::Local<v8::Value> val =
        !v8_args.empty()
            ? v8_args[0]
            : v8::Local<v8::Value>(v8::Object::New(args->isolate()));

    v8::Local<v8::Promise::Resolver> resolver =
        v8::Promise::Resolver::New(args->GetHolderCreationContext())
            .ToLocalChecked();

    v8::Local<v8::Promise> promise = resolver->GetPromise();

    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindLambdaForTesting(
            [isolate = args->isolate(),
             global_val = v8::Global<v8::Value>(args->isolate(), val),
             global_resolver = v8::Global<v8::Promise::Resolver>(
                 args->isolate(), resolver)]() mutable {
              WorkletV8Helper::HandleScope scope(isolate);
              v8::Local<v8::Value> val = global_val.Get(isolate);
              v8::Local<v8::Promise::Resolver> resolver =
                  global_resolver.Get(isolate);
              v8::Local<v8::Context> context =
                  resolver->GetCreationContextChecked();
              resolver->Resolve(context, val).ToChecked();
            }));

    return promise;
  }

  v8::Local<v8::Promise> AsyncRejectForTesting(gin::Arguments* args) {
    std::vector<v8::Local<v8::Value>> v8_args = args->GetAll();

    v8::Local<v8::Value> val =
        !v8_args.empty()
            ? v8_args[0]
            : v8::Local<v8::Value>(v8::Object::New(args->isolate()));

    v8::Local<v8::Promise::Resolver> resolver =
        v8::Promise::Resolver::New(args->GetHolderCreationContext())
            .ToLocalChecked();

    v8::Local<v8::Promise> promise = resolver->GetPromise();

    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindLambdaForTesting(
            [isolate = args->isolate(),
             global_val = v8::Global<v8::Value>(args->isolate(), val),
             global_resolver = v8::Global<v8::Promise::Resolver>(
                 args->isolate(), resolver)]() mutable {
              WorkletV8Helper::HandleScope scope(isolate);
              v8::Local<v8::Value> val = global_val.Get(isolate);
              v8::Local<v8::Promise::Resolver> resolver =
                  global_resolver.Get(isolate);
              v8::Local<v8::Context> context =
                  resolver->GetCreationContextChecked();
              resolver->Reject(context, val).ToChecked();
            }));

    return promise;
  }

  TestClient* test_client() { return test_client_.get(); }
  MockMojomPrivateAggregationHost* mock_private_aggregation_host() {
    return mock_private_aggregation_host_.get();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;

  std::unique_ptr<TestClient> test_client_;
  std::unique_ptr<MockMojomPrivateAggregationHost>
      mock_private_aggregation_host_;

  std::unique_ptr<SharedStorageWorkletGlobalScope> global_scope_;
};

TEST_F(SharedStorageWorkletGlobalScopeTest, IsolateNotInitializedByDefault) {
  EXPECT_FALSE(IsolateInitialized());
}

TEST_F(SharedStorageWorkletGlobalScopeTest, OnModuleScriptDownloadedSuccess) {
  global_scope_->OnModuleScriptDownloaded(
      test_client_.get(), mock_private_aggregation_host_.get(),
      GURL("https://example.test"), base::DoNothing(),
      /*response_body=*/std::make_unique<std::string>(),
      /*error_message=*/{});

  EXPECT_TRUE(IsolateInitialized());

  EXPECT_EQ(GetTypeOf("console"), "object");
  EXPECT_EQ(GetTypeOf("console.log"), "function");
  EXPECT_EQ(GetTypeOf("register"), "function");
  EXPECT_EQ(GetTypeOf("sharedStorage"), "object");
  EXPECT_EQ(GetTypeOf("sharedStorage.set"), "function");
  EXPECT_EQ(GetTypeOf("sharedStorage.append"), "function");
  EXPECT_EQ(GetTypeOf("sharedStorage.delete"), "function");
  EXPECT_EQ(GetTypeOf("sharedStorage.clear"), "function");
  EXPECT_EQ(GetTypeOf("sharedStorage.get"), "function");
  EXPECT_EQ(GetTypeOf("sharedStorage.keys"), "function");
  EXPECT_EQ(GetTypeOf("sharedStorage.entries"), "function");
  EXPECT_EQ(GetTypeOf("sharedStorage.length"), "function");
  EXPECT_EQ(GetTypeOf("sharedStorage.remainingBudget"), "function");
  EXPECT_EQ(GetTypeOf("sharedStorage.context"), "undefined");
  EXPECT_EQ(GetTypeOf("privateAggregation"), "object");
  EXPECT_EQ(GetTypeOf("privateAggregation.sendHistogramReport"), "function");
}

TEST_F(SharedStorageWorkletGlobalScopeTest, OnModuleScriptDownloadedWithError) {
  bool callback_called = false;
  auto cb = base::BindLambdaForTesting(
      [&](bool success, const std::string& error_message) {
        EXPECT_FALSE(success);
        EXPECT_EQ(error_message, "error1");
        callback_called = true;
      });

  global_scope_->OnModuleScriptDownloaded(
      test_client_.get(), mock_private_aggregation_host_.get(),
      GURL("https://example.test"), std::move(cb), nullptr, "error1");

  EXPECT_FALSE(IsolateInitialized());
  EXPECT_TRUE(callback_called);
}

TEST_F(SharedStorageWorkletGlobalScopeTest,
       OnModuleScriptDownloadedWithoutPrivateAggregationHost) {
  global_scope_->OnModuleScriptDownloaded(
      test_client_.get(), /*private_aggregation_host=*/nullptr,
      GURL("https://example.test"), base::DoNothing(),
      /*response_body=*/std::make_unique<std::string>(),
      /*error_message=*/{});

  EXPECT_TRUE(IsolateInitialized());

  EXPECT_EQ(GetTypeOf("privateAggregation"), "undefined");
}

class SharedStorageAddModuleTest : public SharedStorageWorkletGlobalScopeTest {
 public:
  void SimulateAddModule(const std::string& script_body,
                         bool define_private_aggregation_host = true) {
    bool callback_called = false;

    auto cb = base::BindLambdaForTesting(
        [&](bool success, const std::string& error_message) {
          success_ = success;
          error_message_ = error_message;

          callback_called = true;
        });

    global_scope_->OnModuleScriptDownloaded(
        test_client_.get(),
        define_private_aggregation_host ? mock_private_aggregation_host_.get()
                                        : nullptr,
        GURL("https://example.test"), std::move(cb),
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
        typeof(register) !== 'function' ||
        typeof(sharedStorage) !== 'undefined') {
      throw Error('Unexpected object defined status.');
    }
  )");

  EXPECT_TRUE(success());
  EXPECT_TRUE(error_message().empty());
}

TEST_F(SharedStorageAddModuleTest, RegisterOperation_MissingOperationName) {
  SimulateAddModule(R"(
    register();
  )");

  EXPECT_FALSE(success());
  EXPECT_EQ(error_message(),
            "https://example.test/:2 Uncaught TypeError: Missing "
            "\"name\" argument in operation registration.");
}

TEST_F(SharedStorageAddModuleTest, RegisterOperation_EmptyOperationName) {
  SimulateAddModule(R"(
    register("");
  )");

  EXPECT_FALSE(success());
  EXPECT_EQ(error_message(),
            "https://example.test/:2 Uncaught TypeError: Operation name "
            "cannot be empty.");
}

TEST_F(SharedStorageAddModuleTest,
       RegisterOperation_MissingClassName_MissingArgument) {
  SimulateAddModule(R"(
    register("test-operation");
  )");

  EXPECT_FALSE(success());
  EXPECT_EQ(error_message(),
            "https://example.test/:2 Uncaught TypeError: Missing class "
            "name argument in operation registration.");
}

TEST_F(SharedStorageAddModuleTest,
       RegisterOperation_MissingClassName_NotAnObject) {
  SimulateAddModule(R"(
    register("test-operation", 1);
  )");

  EXPECT_FALSE(success());
  EXPECT_EQ(error_message(),
            "https://example.test/:2 Uncaught TypeError: Missing class "
            "name argument in operation registration.");
}

TEST_F(SharedStorageAddModuleTest, RegisterOperation_ClassNameNotAConstructor) {
  SimulateAddModule(R"(
    register("test-operation", {});
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

    register("test-operation", TestClass);
  )");

  EXPECT_FALSE(success());
  EXPECT_EQ(error_message(),
            "https://example.test/:8 Uncaught TypeError: Missing \"run()\" "
            "function in the class.");
}

TEST_F(SharedStorageAddModuleTest,
       RegisterOperation_ClassPrototypeNotAnObject) {
  SimulateAddModule(R"(
    function test() {};
    test.prototype = 123;

    register("test-operation", test);
  )");

  EXPECT_FALSE(success());
  EXPECT_EQ(error_message(),
            "https://example.test/:5 Uncaught TypeError: Unexpected class "
            "prototype: not an object.");
}

TEST_F(SharedStorageAddModuleTest, RegisterOperation_Success) {
  SimulateAddModule(R"(
    class TestClass {
      async run() {}
    }

    register("test-operation", TestClass);
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

    register("test-operation", TestClass1);
    register("test-operation", TestClass2);
  )");

  EXPECT_FALSE(success());
  EXPECT_EQ(error_message(),
            "https://example.test/:11 Uncaught TypeError: Operation name "
            "already registered.");
}

TEST_F(SharedStorageAddModuleTest,
       RegisterOperationWithPrivateAggregationCall_CallForwarded) {
  // The operation will not be run.
  EXPECT_CALL(*mock_private_aggregation_host(), SendHistogramReport).Times(0);

  SimulateAddModule(R"(
    class TestClass {
      async run() {
        privateAggregation.sendHistogramReport({bucket: 1n, value: 2});
      }
    }

    register("test-operation", TestClass);
  )");

  EXPECT_TRUE(success());
  EXPECT_TRUE(error_message().empty());
}

TEST_F(SharedStorageAddModuleTest,
       RegisterOperationWithPrivateAggregationCall_PAHostNotDefined) {
  EXPECT_CALL(*mock_private_aggregation_host(), SendHistogramReport).Times(0);

  SimulateAddModule(R"(
    class TestClass {
      async run() {
        privateAggregation.sendHistogramReport({bucket: 1n, value: 2});
      }
    }

    register("test-operation", TestClass);
  )",
                    /*define_private_aggregation_host=*/false);

  EXPECT_TRUE(success());
  EXPECT_TRUE(error_message().empty());
  EXPECT_FALSE(test_client()->observed_record_use_counter_call());
}

class SharedStorageRunOperationTest
    : public SharedStorageWorkletGlobalScopeTest {
 public:
  // The caller should provide a valid module script. The purpose of this test
  // suite is to test RunOperation.
  void SimulateAddModule(const std::string& script_body,
                         bool define_private_aggregation_host = true) {
    bool add_module_callback_called = false;

    auto add_module_callback = base::BindLambdaForTesting(
        [&](bool success, const std::string& error_message) {
          DCHECK(success);
          add_module_callback_called = true;
        });

    global_scope_->OnModuleScriptDownloaded(
        test_client_.get(),
        define_private_aggregation_host ? mock_private_aggregation_host_.get()
                                        : nullptr,
        GURL("https://example.test"), std::move(add_module_callback),
        std::make_unique<std::string>(script_body), /*error_message=*/{});

    ASSERT_TRUE(add_module_callback_called);

    RegisterAsyncReturnForTesting();
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

      register("test-operation", TestClass);
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

      register("test-operation", TestClass);
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

      register("test-operation", TestClass);
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

      register("test-operation", TestClass);
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

      register("test-operation", TestClass);
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

      register("test-operation", TestClass);
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

      register("test-operation", TestClass);
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

      register("test-operation", TestClass);
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

      register("test-operation", TestClass);
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

      register("test-operation", TestClass);
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

      register("test-operation", TestClass);
    )");

  SimulateRunURLSelectionOperation(
      "test-operation", {GURL("https://foo.com"), GURL("https://bar.com")},
      /*serialized_data=*/{});

  EXPECT_TRUE(url_selection_operation_finished());
  EXPECT_TRUE(url_selection_operation_success());
  EXPECT_TRUE(url_selection_operation_error_message().empty());
  EXPECT_EQ(url_selection_operation_index(), 1u);
}

TEST_F(
    SharedStorageRunOperationTest,
    URLSelectionOperation_ResultPromiseFulfilledSynchronously_NumberOverflow) {
  SimulateAddModule(R"(
      class TestClass {
        async run(urls) {
          return -4294967295;
        }
      }

      register("test-operation", TestClass);
    )");

  SimulateRunURLSelectionOperation(
      "test-operation", {GURL("https://foo.com"), GURL("https://bar.com")},
      /*serialized_data=*/{});

  EXPECT_TRUE(url_selection_operation_finished());
  EXPECT_TRUE(url_selection_operation_success());
  EXPECT_TRUE(url_selection_operation_error_message().empty());
  EXPECT_EQ(url_selection_operation_index(), 1u);
}

TEST_F(
    SharedStorageRunOperationTest,
    URLSelectionOperation_ResultPromiseFulfilledSynchronously_StringConvertedToUint32) {
  SimulateAddModule(R"(
      class TestClass {
        async run(urls) {
          return '1';
        }
      }

      register("test-operation", TestClass);
    )");

  SimulateRunURLSelectionOperation(
      "test-operation", {GURL("https://foo.com"), GURL("https://bar.com")},
      /*serialized_data=*/{});

  EXPECT_TRUE(url_selection_operation_finished());
  EXPECT_TRUE(url_selection_operation_success());
  EXPECT_TRUE(url_selection_operation_error_message().empty());
  EXPECT_EQ(url_selection_operation_index(), 1u);
}

TEST_F(
    SharedStorageRunOperationTest,
    URLSelectionOperation_ResultPromiseFulfilledSynchronously_RandomStringConvertedTo0) {
  SimulateAddModule(R"(
      class TestClass {
        async run(urls) {
          return 'abc';
        }
      }

      register("test-operation", TestClass);
    )");

  SimulateRunURLSelectionOperation(
      "test-operation", {GURL("https://foo.com"), GURL("https://bar.com")},
      /*serialized_data=*/{});

  EXPECT_TRUE(url_selection_operation_finished());
  EXPECT_TRUE(url_selection_operation_success());
  EXPECT_TRUE(url_selection_operation_error_message().empty());
  EXPECT_EQ(url_selection_operation_index(), 0u);
}

TEST_F(
    SharedStorageRunOperationTest,
    URLSelectionOperation_ResultPromiseFulfilledSynchronously_DefaultUndefinedResultConvertedTo0) {
  SimulateAddModule(R"(
      class TestClass {
        async run(urls) {}
      }

      register("test-operation", TestClass);
    )");

  SimulateRunURLSelectionOperation(
      "test-operation", {GURL("https://foo.com"), GURL("https://bar.com")},
      /*serialized_data=*/{});

  EXPECT_TRUE(url_selection_operation_finished());
  EXPECT_TRUE(url_selection_operation_success());
  EXPECT_TRUE(url_selection_operation_error_message().empty());
  EXPECT_EQ(url_selection_operation_index(), 0u);
}

TEST_F(
    SharedStorageRunOperationTest,
    URLSelectionOperation_ResultPromiseRejectedSynchronously_SynchronousScriptError) {
  SimulateAddModule(R"(
      class TestClass {
        async run(urls) {
          undefined_variable;
        }
      }

      register("test-operation", TestClass);
    )");

  SimulateRunURLSelectionOperation("test-operation", {GURL("https://foo.com")},
                                   /*serialized_data=*/{});

  EXPECT_TRUE(url_selection_operation_finished());
  EXPECT_FALSE(url_selection_operation_success());
  EXPECT_EQ(url_selection_operation_error_message(),
            "ReferenceError: undefined_variable is not defined");
  EXPECT_EQ(url_selection_operation_index(), 0u);
}

TEST_F(
    SharedStorageRunOperationTest,
    URLSelectionOperation_ResultPromiseRejectedSynchronously_ReturnValueOutOfRange) {
  SimulateAddModule(R"(
      class TestClass {
        async run(urls) {
          return 1;
        }
      }

      register("test-operation", TestClass);
    )");

  SimulateRunURLSelectionOperation("test-operation", {GURL("https://foo.com")},
                                   /*serialized_data=*/{});

  EXPECT_TRUE(url_selection_operation_finished());
  EXPECT_FALSE(url_selection_operation_success());
  EXPECT_EQ(
      url_selection_operation_error_message(),
      "Promise resolved to a number outside the length of the input urls.");
  EXPECT_EQ(url_selection_operation_index(), 0u);
}

TEST_F(
    SharedStorageRunOperationTest,
    URLSelectionOperation_ResultPromiseRejectedSynchronously_ReturnValueToInt32Error) {
  SimulateAddModule(R"(
      class TestClass {
        async run(urls) {
          class CustomClass {
            toString() { throw Error('error 123'); }
          }

          return new CustomClass();
        }
      }

      register("test-operation", TestClass);
    )");

  SimulateRunURLSelectionOperation("test-operation", {GURL("https://foo.com")},
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
          return asyncFulfillForTesting(1);
        }
      }

      register("test-operation", TestClass);
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
    URLSelectionOperation_ResultPromiseFulfilledAsynchronously_NumberOverflow) {
  SimulateAddModule(R"(
      class TestClass {
        async run(urls) {
          return asyncFulfillForTesting(-4294967295);
        }
      }

      register("test-operation", TestClass);
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
    URLSelectionOperation_ResultPromiseFulfilledAsynchronously_StringConvertedToUint32) {
  SimulateAddModule(R"(
      class TestClass {
        async run(urls) {
          return asyncFulfillForTesting('1');
        }
      }

      register("test-operation", TestClass);
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
    URLSelectionOperation_ResultPromiseFulfilledAsynchronously_RandomStringConvertedTo0) {
  SimulateAddModule(R"(
      class TestClass {
        async run(urls) {
          return asyncFulfillForTesting('abc');
        }
      }

      register("test-operation", TestClass);
    )");

  SimulateRunURLSelectionOperation(
      "test-operation", {GURL("https://foo.com"), GURL("https://bar.com")},
      /*serialized_data=*/{});

  EXPECT_FALSE(url_selection_operation_finished());

  task_environment_.RunUntilIdle();

  EXPECT_TRUE(url_selection_operation_finished());
  EXPECT_TRUE(url_selection_operation_success());
  EXPECT_TRUE(url_selection_operation_error_message().empty());
  EXPECT_EQ(url_selection_operation_index(), 0u);
}

TEST_F(
    SharedStorageRunOperationTest,
    URLSelectionOperation_ResultPromiseFulfilledAsynchronously_DefaultUndefinedResultConvertedTo0) {
  SimulateAddModule(R"(
      class TestClass {
        async run(urls) {
          return asyncFulfillForTesting();
        }
      }

      register("test-operation", TestClass);
    )");

  SimulateRunURLSelectionOperation(
      "test-operation", {GURL("https://foo.com"), GURL("https://bar.com")},
      /*serialized_data=*/{});

  EXPECT_FALSE(url_selection_operation_finished());

  task_environment_.RunUntilIdle();

  EXPECT_TRUE(url_selection_operation_finished());
  EXPECT_TRUE(url_selection_operation_success());
  EXPECT_TRUE(url_selection_operation_error_message().empty());
  EXPECT_EQ(url_selection_operation_index(), 0u);
}

TEST_F(SharedStorageRunOperationTest,
       URLSelectionOperation_ResultPromiseRejectedAsynchronously) {
  SimulateAddModule(R"(
      class TestClass {
        async run(urls) {
          return asyncRejectForTesting('custom error message 123');
        }
      }

      register("test-operation", TestClass);
    )");

  SimulateRunURLSelectionOperation(
      "test-operation", {GURL("https://foo.com"), GURL("https://bar.com")},
      /*serialized_data=*/{});

  EXPECT_FALSE(url_selection_operation_finished());

  task_environment_.RunUntilIdle();

  EXPECT_TRUE(url_selection_operation_finished());
  EXPECT_FALSE(url_selection_operation_success());
  EXPECT_EQ(url_selection_operation_error_message(),
            "custom error message 123");
  EXPECT_EQ(url_selection_operation_index(), 0u);
}

TEST_F(
    SharedStorageRunOperationTest,
    URLSelectionOperation_ResultPromiseRejectedAsynchronously_ReturnValueOutOfRange) {
  SimulateAddModule(R"(
      class TestClass {
        async run(urls) {
          return asyncFulfillForTesting(1);
        }
      }

      register("test-operation", TestClass);
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

TEST_F(
    SharedStorageRunOperationTest,
    URLSelectionOperation_ResultPromiseRejectedAsynchronously_ReturnValueToInt32Error) {
  SimulateAddModule(R"(
      class TestClass {
        async run(urls) {
          class CustomClass {
            toString() { throw Error('error 123'); }
          }

          return asyncFulfillForTesting(new CustomClass());
        }
      }

      register("test-operation", TestClass);
    )");

  SimulateRunURLSelectionOperation("test-operation", {GURL("https://foo.com")},
                                   /*serialized_data=*/{});

  EXPECT_FALSE(url_selection_operation_finished());

  task_environment_.RunUntilIdle();

  EXPECT_TRUE(url_selection_operation_finished());
  EXPECT_FALSE(url_selection_operation_success());
  EXPECT_EQ(url_selection_operation_error_message(),
            "Promise did not resolve to an uint32 number.");
  EXPECT_EQ(url_selection_operation_index(), 0u);
}

TEST_F(SharedStorageRunOperationTest,
       UnnamedOperationWithPrivateAggregationCall_Success) {
  EXPECT_CALL(*mock_private_aggregation_host(), SendHistogramReport)
      .WillOnce(testing::Invoke(
          [](std::vector<
                 blink::mojom::AggregatableReportHistogramContributionPtr>
                 contributions,
             blink::mojom::AggregationServiceMode aggregation_mode,
             blink::mojom::DebugModeDetailsPtr debug_mode_details) {
            ASSERT_EQ(contributions.size(), 1u);
            EXPECT_EQ(contributions[0]->bucket, 1);
            EXPECT_EQ(contributions[0]->value, 2);
            EXPECT_EQ(aggregation_mode,
                      blink::mojom::AggregationServiceMode::kDefault);
            ASSERT_FALSE(debug_mode_details.is_null());
            EXPECT_EQ(*debug_mode_details, blink::mojom::DebugModeDetails());
          }));

  SimulateAddModule(R"(
      class TestClass {
        async run() {
          privateAggregation.sendHistogramReport({bucket: 1n, value: 2});
        }
      }

      register("test-operation", TestClass);
    )");

  SimulateRunOperation("test-operation", /*serialized_data=*/{});

  EXPECT_TRUE(unnamed_operation_finished());
  EXPECT_TRUE(unnamed_operation_success());
  EXPECT_TRUE(unnamed_operation_error_message().empty());
}

TEST_F(SharedStorageRunOperationTest,
       UnnamedOperationWithPrivateAggregationCall_PAPermissionsPolicyDisabled) {
  OverrideGlobalScope(std::make_unique<SharedStorageWorkletGlobalScope>(
      /*private_aggregation_permissions_policy_allowed=*/false,
      /*embedder_context=*/absl::nullopt));

  SimulateAddModule(R"(
      class TestClass {
        async run() {
          privateAggregation.sendHistogramReport({bucket: 1n, value: 2});
        }
      }

      register("test-operation", TestClass);
    )");

  SimulateRunOperation("test-operation", /*serialized_data=*/{});

  EXPECT_TRUE(unnamed_operation_finished());
  EXPECT_FALSE(unnamed_operation_success());

  EXPECT_EQ(unnamed_operation_error_message(),
            "TypeError: The \"private-aggregation\" Permissions Policy denied "
            "the method on privateAggregation");
  EXPECT_TRUE(test_client()->observed_record_use_counter_call());
}

TEST_F(SharedStorageRunOperationTest,
       UnnamedOperationWithPrivateAggregationCall_PAHostNotDefined) {
  EXPECT_CALL(*mock_private_aggregation_host(), SendHistogramReport).Times(0);

  SimulateAddModule(R"(
      class TestClass {
        async run() {
          privateAggregation.sendHistogramReport({bucket: 1n, value: 2});
        }
      }

      register("test-operation", TestClass);
    )",
                    /*define_private_aggregation_host=*/false);

  SimulateRunOperation("test-operation", /*serialized_data=*/{});

  EXPECT_TRUE(unnamed_operation_finished());
  EXPECT_FALSE(unnamed_operation_success());
  EXPECT_EQ(unnamed_operation_error_message(),
            "ReferenceError: privateAggregation is not defined");
  EXPECT_FALSE(test_client()->observed_record_use_counter_call());
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

TEST_F(SharedStorageObjectMethodTest,
       SetOperation_KeyAndValueConvertedToString) {
  ExecuteScript("sharedStorage.set(123, 456)");
  ExecuteScript("sharedStorage.set(null, null)");
  ExecuteScript("sharedStorage.set(undefined, undefined)");
  ExecuteScript(
      "sharedStorage.set({dictKey1: 'dictValue1'}, {dictKey2: 'dictValue2'})");
  task_environment_.RunUntilIdle();

  EXPECT_EQ(test_client()->observed_set_params().size(), 4u);
  EXPECT_EQ(test_client()->observed_set_params()[0].key, u"123");
  EXPECT_EQ(test_client()->observed_set_params()[0].value, u"456");
  EXPECT_EQ(test_client()->observed_set_params()[1].key, u"null");
  EXPECT_EQ(test_client()->observed_set_params()[1].value, u"null");
  EXPECT_EQ(test_client()->observed_set_params()[2].key, u"undefined");
  EXPECT_EQ(test_client()->observed_set_params()[2].value, u"undefined");
  EXPECT_EQ(test_client()->observed_set_params()[3].key, u"[object Object]");
  EXPECT_EQ(test_client()->observed_set_params()[3].value, u"[object Object]");
}

TEST_F(SharedStorageObjectMethodTest, SetOperation_KeyConvertedToStringError) {
  ExecuteScript(
      "class CustomClass {"
      "  toString() { throw Error('error 123'); }"
      "}"
      "sharedStorage.set(new CustomClass(), new CustomClass())");
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

TEST_F(SharedStorageObjectMethodTest,
       SetOperation_ValueConvertedToStringError) {
  ExecuteScript(
      "class CustomClass {"
      "  toString() { throw Error('error 123'); }"
      "}"
      "sharedStorage.set(123, new CustomClass())");
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

TEST_F(SharedStorageObjectMethodTest, SetOperation_IgnoreIfPresent_False) {
  ExecuteScript("sharedStorage.set('key', 'value')");
  ExecuteScript("sharedStorage.set('key', 'value', {})");
  ExecuteScript("sharedStorage.set('key', 'value', {ignoreIfPresent: false})");
  ExecuteScript("sharedStorage.set('key', 'value', {ignoreIfPresent: ''})");
  ExecuteScript("sharedStorage.set('key', 'value', {ignoreIfPresent: null})");
  ExecuteScript(
      "sharedStorage.set('key', 'value', {ignoreIfPresent: undefined})");

  task_environment_.RunUntilIdle();

  EXPECT_EQ(test_client()->observed_set_params().size(), 6u);
  EXPECT_FALSE(test_client()->observed_set_params()[0].ignore_if_present);
  EXPECT_FALSE(test_client()->observed_set_params()[1].ignore_if_present);
  EXPECT_FALSE(test_client()->observed_set_params()[2].ignore_if_present);
  EXPECT_FALSE(test_client()->observed_set_params()[3].ignore_if_present);
  EXPECT_FALSE(test_client()->observed_set_params()[4].ignore_if_present);
  EXPECT_FALSE(test_client()->observed_set_params()[5].ignore_if_present);
}

TEST_F(SharedStorageObjectMethodTest, SetOperation_IgnoreIfPresent_True) {
  ExecuteScript("sharedStorage.set('key', 'value', {ignoreIfPresent: true})");
  // A non-empty string will evaluate to true.
  ExecuteScript(
      "sharedStorage.set('key', 'value', {ignoreIfPresent: 'false'})");
  // A dictionary object will evaluate to true.
  ExecuteScript("sharedStorage.set('key', 'value', {ignoreIfPresent: {}})");
  task_environment_.RunUntilIdle();

  EXPECT_EQ(test_client()->observed_set_params().size(), 3u);
  EXPECT_TRUE(test_client()->observed_set_params()[0].ignore_if_present);
  EXPECT_TRUE(test_client()->observed_set_params()[1].ignore_if_present);
  EXPECT_TRUE(test_client()->observed_set_params()[2].ignore_if_present);
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

TEST_F(SharedStorageObjectMethodTest,
       AppendOperation_KeyAndValueConvertedToString) {
  ExecuteScript("sharedStorage.append(123, 456)");
  ExecuteScript("sharedStorage.append(null, null)");
  ExecuteScript("sharedStorage.append(undefined, undefined)");
  ExecuteScript(
      "sharedStorage.append({dictKey1: 'dictValue1'}, {dictKey2: "
      "'dictValue2'})");
  task_environment_.RunUntilIdle();

  EXPECT_EQ(test_client()->observed_append_params().size(), 4u);
  EXPECT_EQ(test_client()->observed_append_params()[0].key, u"123");
  EXPECT_EQ(test_client()->observed_append_params()[0].value, u"456");
  EXPECT_EQ(test_client()->observed_append_params()[1].key, u"null");
  EXPECT_EQ(test_client()->observed_append_params()[1].value, u"null");
  EXPECT_EQ(test_client()->observed_append_params()[2].key, u"undefined");
  EXPECT_EQ(test_client()->observed_append_params()[2].value, u"undefined");
  EXPECT_EQ(test_client()->observed_append_params()[3].key, u"[object Object]");
  EXPECT_EQ(test_client()->observed_append_params()[3].value,
            u"[object Object]");
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

TEST_F(SharedStorageObjectMethodTest, DeleteOperation_KeyConvertedToString) {
  ExecuteScript("sharedStorage.delete(123)");
  ExecuteScript("sharedStorage.delete(null)");
  ExecuteScript("sharedStorage.delete(undefined)");
  ExecuteScript("sharedStorage.delete({dictKey1: 'dictValue1'})");
  task_environment_.RunUntilIdle();

  EXPECT_EQ(test_client()->observed_delete_params().size(), 4u);
  EXPECT_EQ(test_client()->observed_delete_params()[0], u"123");
  EXPECT_EQ(test_client()->observed_delete_params()[1], u"null");
  EXPECT_EQ(test_client()->observed_delete_params()[2], u"undefined");
  EXPECT_EQ(test_client()->observed_delete_params()[3], u"[object Object]");
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

TEST_F(SharedStorageObjectMethodTest, GetOperation_KeyConvertedToString) {
  ExecuteScript("sharedStorage.get(123)");
  ExecuteScript("sharedStorage.get(null)");
  ExecuteScript("sharedStorage.get(undefined)");
  ExecuteScript("sharedStorage.get({dictKey1: 'dictValue1'})");
  task_environment_.RunUntilIdle();

  EXPECT_EQ(test_client()->observed_get_params().size(), 4u);
  EXPECT_EQ(test_client()->observed_get_params()[0], u"123");
  EXPECT_EQ(test_client()->observed_get_params()[1], u"null");
  EXPECT_EQ(test_client()->observed_get_params()[2], u"undefined");
  EXPECT_EQ(test_client()->observed_get_params()[3], u"[object Object]");
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
       RemainingBudgetOperation_FulfilledAsynchronously) {
  ExecuteScript("sharedStorage.remainingBudget()");
  EXPECT_FALSE(finished());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(finished());
  EXPECT_TRUE(fulfilled());

  WorkletV8Helper::HandleScope scope(Isolate());
  EXPECT_TRUE(v8_resolved_value()->IsNumber());

  double bits = 0.0;
  gin::Converter<double>::FromV8(Isolate(), v8_resolved_value(), &bits);
  EXPECT_EQ(bits, 2.5);
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
      /*has_more_entries=*/false, /*total_queued_to_send=*/0);
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
      /*has_more_entries=*/true, /*total_queued_to_send=*/0);
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
      /*has_more_entries=*/true, /*total_queued_to_send=*/3);
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(finished());
  EXPECT_EQ(test_client()->observed_console_log_messages().size(), 1u);
  EXPECT_EQ(test_client()->observed_console_log_messages()[0], "key0;value0");

  remote_listener->DidReadEntries(
      /*success=*/true, /*error_message=*/{},
      CreateBatchResult({{u"key1", u"value1"}, {u"key2", u"value2"}}),
      /*has_more_entries=*/false, /*total_queued_to_send=*/3);
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
      /*has_more_entries=*/true, /*total_queued_to_send=*/3);
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(finished());
  EXPECT_EQ(test_client()->observed_console_log_messages().size(), 1u);
  EXPECT_EQ(test_client()->observed_console_log_messages()[0], "key0;value0");

  remote_listener->DidReadEntries(
      /*success=*/false, /*error_message=*/"Internal error 12345",
      CreateBatchResult({}),
      /*has_more_entries=*/true, /*total_queued_to_send=*/3);
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
      /*has_more_entries=*/false, /*total_queued_to_send=*/2);
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
      /*has_more_entries=*/true, /*total_queued_to_send=*/6);
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(finished());
  EXPECT_EQ(test_client()->observed_console_log_messages().size(), 0u);

  remote_listener->DidReadEntries(
      /*success=*/true, /*error_message=*/{},
      CreateBatchResult({{u"key1", /*value=*/{}}, {u"key2", /*value=*/{}}}),
      /*has_more_entries=*/true, /*total_queued_to_send=*/6);
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(finished());
  EXPECT_EQ(test_client()->observed_console_log_messages().size(), 1u);
  EXPECT_EQ(test_client()->observed_console_log_messages()[0],
            "{\"done\":false,\"value\":\"key2\"}");

  remote_listener->DidReadEntries(
      /*success=*/true, /*error_message=*/{},
      CreateBatchResult({{u"key3", /*value=*/{}}}),
      /*has_more_entries=*/false, /*total_queued_to_send=*/6);
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

class SharedStorageObjectPropertyTest : public SharedStorageRunOperationTest {
 public:
  SharedStorageObjectPropertyTest() = default;

  void SetEmbedderContext(absl::optional<std::u16string> embedder_context) {
    OverrideGlobalScope(std::make_unique<SharedStorageWorkletGlobalScope>(
        /*private_aggregation_permissions_policy_allowed=*/true,
        embedder_context));

    // Run AddModule so that sharedStorage is exposed.
    SimulateAddModule(R"()");
  }

  void ExecuteScript(const std::string& script_body) {
    WorkletV8Helper::HandleScope scope(Isolate());
    v8::Local<v8::Context> context = LocalContext();
    v8::Context::Scope context_scope(context);

    v8::Local<v8::Value> v8_result = EvalJs(script_body);

    ASSERT_TRUE(!v8_result.IsEmpty());
    ASSERT_FALSE(v8_result->IsPromise());

    v8_result_value_ =
        v8::Global<v8::Value>(Isolate(), v8_result.As<v8::Value>());
  }

  v8::Local<v8::Value> v8_result_value() {
    v8::Local<v8::Value> v8_result_value = v8_result_value_.Get(Isolate());
    return v8_result_value;
  }

 protected:
  base::HistogramTester histogram_tester_;

 private:
  v8::Global<v8::Value> v8_result_value_;
};

TEST_F(SharedStorageObjectPropertyTest, ContextOperation_String) {
  SetEmbedderContext(absl::make_optional(u"some embedder context"));
  EXPECT_EQ(GetTypeOf("sharedStorage.context"), "string");

  ExecuteScript("sharedStorage.context");

  WorkletV8Helper::HandleScope scope(Isolate());
  EXPECT_TRUE(v8_result_value()->IsString());
  EXPECT_EQ(gin::V8ToString(Isolate(), v8_result_value()),
            "some embedder context");
  histogram_tester_.ExpectUniqueSample(WorkletContextDefinedHistogram, true, 2);
}

TEST_F(SharedStorageObjectPropertyTest, ContextOperation_Undefined) {
  SetEmbedderContext(absl::nullopt);
  EXPECT_EQ(GetTypeOf("sharedStorage.context"), "undefined");

  ExecuteScript("sharedStorage.context");

  WorkletV8Helper::HandleScope scope(Isolate());
  EXPECT_TRUE(v8_result_value()->IsUndefined());
  histogram_tester_.ExpectUniqueSample(WorkletContextDefinedHistogram, false,
                                       2);
}

class SharedStoragePrivateAggregationTest
    : public SharedStorageRunOperationTest {
 public:
  SharedStoragePrivateAggregationTest() {
    // Run AddModule so that `privateAggregation` is exposed.
    SimulateAddModule(R"()");
  }

  void ExecuteScriptExpectNoError(const std::string& script_body) {
    std::string error_message;
    ExecuteScript(script_body, &error_message);
    EXPECT_TRUE(error_message.empty());
  }

  void ExecuteScriptAndValidateContribution(
      const std::string& script_body,
      absl::uint128 expected_bucket,
      int expected_value,
      blink::mojom::DebugModeDetailsPtr expected_debug_mode_details =
          blink::mojom::DebugModeDetails::New()) {
    EXPECT_CALL(*mock_private_aggregation_host(), SendHistogramReport)
        .WillOnce(testing::Invoke(
            [&](std::vector<
                    blink::mojom::AggregatableReportHistogramContributionPtr>
                    contributions,
                blink::mojom::AggregationServiceMode aggregation_mode,
                blink::mojom::DebugModeDetailsPtr debug_mode_details) {
              ASSERT_EQ(contributions.size(), 1u);
              EXPECT_EQ(contributions[0]->bucket, expected_bucket);
              EXPECT_EQ(contributions[0]->value, expected_value);
              EXPECT_EQ(aggregation_mode,
                        blink::mojom::AggregationServiceMode::kDefault);
              EXPECT_TRUE(debug_mode_details == expected_debug_mode_details);
            }));

    ExecuteScriptExpectNoError(script_body);

    EXPECT_TRUE(test_client()->observed_record_use_counter_call());
  }

  std::string ExecuteScriptReturningError(
      const std::string& script_body,
      bool flush_and_reset_private_aggregation = true) {
    EXPECT_CALL(*mock_private_aggregation_host(), SendHistogramReport).Times(0);

    std::string error_message;
    ExecuteScript(script_body, &error_message,
                  flush_and_reset_private_aggregation);
    EXPECT_FALSE(error_message.empty());

    // These tests all invoke sendHistogramReport (albeit incorrectly), so the
    // use counter is expected to be triggered.
    EXPECT_TRUE(test_client()->observed_record_use_counter_call());
    return error_message;
  }

 private:
  void ExecuteScript(const std::string& script_body,
                     std::string* out_error,
                     bool flush_and_reset_private_aggregation = true) {
    WorkletV8Helper::HandleScope scope(Isolate());
    v8::Local<v8::Context> context = LocalContext();
    v8::Context::Scope context_scope(context);

    WorkletV8Helper::CompileAndRunScript(
        LocalContext(), script_body, GURL("https://example.test"), out_error);

    if (flush_and_reset_private_aggregation) {
      // Ensures that Private Aggregation is flushed and reset after.
      SimulateRunOperation("", {});
    }
  }
};

TEST_F(SharedStoragePrivateAggregationTest, BasicTest) {
  ExecuteScriptAndValidateContribution(
      "privateAggregation.sendHistogramReport({bucket: 1n, value: 2});",
      /*expected_bucket=*/1, /*expected_value=*/2);
}

TEST_F(SharedStoragePrivateAggregationTest, ZeroBucket) {
  ExecuteScriptAndValidateContribution(
      "privateAggregation.sendHistogramReport({bucket: 0n, value: 2});",
      /*expected_bucket=*/0, /*expected_value=*/2);
}

TEST_F(SharedStoragePrivateAggregationTest, ZeroValue) {
  ExecuteScriptAndValidateContribution(
      "privateAggregation.sendHistogramReport({bucket: 1n, value: 0});",
      /*expected_bucket=*/1, /*expected_value=*/0);
}

TEST_F(SharedStoragePrivateAggregationTest, LargeBucket) {
  ExecuteScriptAndValidateContribution(
      "privateAggregation.sendHistogramReport({bucket: 18446744073709551616n, "
      "value: 2});",
      /*expected_bucket=*/absl::MakeUint128(/*high=*/1, /*low=*/0),
      /*expected_value=*/2);
}

TEST_F(SharedStoragePrivateAggregationTest, MaxBucket) {
  ExecuteScriptAndValidateContribution(
      "privateAggregation.sendHistogramReport({bucket: "
      "340282366920938463463374607431768211455n, value: 2});",
      /*expected_bucket=*/absl::Uint128Max(), /*expected_value=*/2);
}

TEST_F(SharedStoragePrivateAggregationTest, TooLargeBucket_Rejected) {
  std::string error_str = ExecuteScriptReturningError(
      "privateAggregation.sendHistogramReport({bucket: "
      "340282366920938463463374607431768211456n, value: 2});");

  EXPECT_EQ(error_str,
            "https://example.test/:1 Uncaught TypeError: BigInt is too large.");
}

TEST_F(SharedStoragePrivateAggregationTest, NegativeBucket_Rejected) {
  std::string error_str = ExecuteScriptReturningError(
      "privateAggregation.sendHistogramReport({bucket: "
      "-1n, value: 2});");

  EXPECT_EQ(error_str,
            "https://example.test/:1 Uncaught TypeError: BigInt must be "
            "non-negative.");
}

TEST_F(SharedStoragePrivateAggregationTest, NonBigIntBucket_Rejected) {
  std::string error_str = ExecuteScriptReturningError(
      "privateAggregation.sendHistogramReport({bucket: 1, value: 2});");

  EXPECT_EQ(
      error_str,
      "https://example.test/:1 Uncaught TypeError: bucket must be a BigInt.");
}

TEST_F(SharedStoragePrivateAggregationTest, NonIntegerValue_Rejected) {
  std::string error_str = ExecuteScriptReturningError(
      "privateAggregation.sendHistogramReport({bucket: 1n, value: 2.3});");

  EXPECT_EQ(error_str,
            "https://example.test/:1 Uncaught TypeError: Value must be an "
            "integer Number.");
}

TEST_F(SharedStoragePrivateAggregationTest, NegativeValue_Rejected) {
  std::string error_str = ExecuteScriptReturningError(
      "privateAggregation.sendHistogramReport({bucket: 1n, value: -1});");

  EXPECT_EQ(error_str,
            "https://example.test/:1 Uncaught TypeError: Value must be "
            "non-negative.");
}

TEST_F(SharedStoragePrivateAggregationTest, NoApiUse_UseCounterNotCalled) {
  EXPECT_CALL(*mock_private_aggregation_host(), SendHistogramReport).Times(0);
  ExecuteScriptExpectNoError("const a = 1;");
  EXPECT_FALSE(test_client()->observed_record_use_counter_call());
}

TEST_F(SharedStoragePrivateAggregationTest, MultipleRequests) {
  EXPECT_CALL(*mock_private_aggregation_host(), SendHistogramReport)
      .WillOnce(testing::Invoke(
          [](std::vector<
                 blink::mojom::AggregatableReportHistogramContributionPtr>
                 contributions,
             blink::mojom::AggregationServiceMode aggregation_mode,
             blink::mojom::DebugModeDetailsPtr debug_mode_details) {
            ASSERT_EQ(contributions.size(), 2u);
            EXPECT_EQ(contributions[0]->bucket, 1);
            EXPECT_EQ(contributions[0]->value, 2);
            EXPECT_EQ(contributions[1]->bucket, 3);
            EXPECT_EQ(contributions[1]->value, 4);
            EXPECT_EQ(aggregation_mode,
                      blink::mojom::AggregationServiceMode::kDefault);
            ASSERT_FALSE(debug_mode_details.is_null());
            EXPECT_EQ(*debug_mode_details, blink::mojom::DebugModeDetails());
          }));

  ExecuteScriptExpectNoError(
      R"(
        privateAggregation.sendHistogramReport({bucket: 1n, value: 2});
        privateAggregation.sendHistogramReport({bucket: 3n, value: 4});
      )");
}

TEST_F(SharedStoragePrivateAggregationTest, DebugModeWithNoDebugKey) {
  ExecuteScriptAndValidateContribution(
      R"(
        privateAggregation.enableDebugMode();
        privateAggregation.sendHistogramReport({bucket: 1n, value: 2});
      )",
      /*expected_bucket=*/1,
      /*expected_value=*/2,
      /*expected_debug_mode_details=*/
      blink::mojom::DebugModeDetails::New(/*is_enabled=*/true,
                                          /*debug_key=*/nullptr));
}

TEST_F(SharedStoragePrivateAggregationTest, DebugModeWithDebugKey) {
  ExecuteScriptAndValidateContribution(
      R"(
        privateAggregation.enableDebugMode({debug_key: 1234n});
        privateAggregation.sendHistogramReport({bucket: 1n, value: 2});
      )",
      /*expected_bucket=*/1,
      /*expected_value=*/2,
      /*expected_debug_mode_details=*/
      blink::mojom::DebugModeDetails::New(
          /*is_enabled=*/true,
          /*debug_key=*/blink::mojom::DebugKey::New(1234u)));
}

TEST_F(SharedStoragePrivateAggregationTest, NegativeDebugKey_Rejected) {
  std::string error_str = ExecuteScriptReturningError(
      "privateAggregation.enableDebugMode({debug_key: -1n});");

  EXPECT_EQ(error_str,
            "https://example.test/:1 Uncaught TypeError: BigInt must be "
            "non-negative.");
}

TEST_F(SharedStoragePrivateAggregationTest, TooLargeDebugKey_Rejected) {
  std::string error_str = ExecuteScriptReturningError(
      "privateAggregation.enableDebugMode({debug_key: "
      "18446744073709551616n});");

  EXPECT_EQ(error_str,
            "https://example.test/:1 Uncaught TypeError: BigInt is too large.");
}

TEST_F(SharedStoragePrivateAggregationTest, NonBigIntDebugKey_Rejected) {
  std::string error_str = ExecuteScriptReturningError(
      "privateAggregation.enableDebugMode({debug_key: 1});");

  EXPECT_EQ(error_str,
            "https://example.test/:1 Uncaught TypeError: debug_key must be a "
            "BigInt.");
}

TEST_F(SharedStoragePrivateAggregationTest,
       InvalidEnableDebugModeArgument_Rejected) {
  // The debug key is not wrapped in a dictionary.
  std::string error_str =
      ExecuteScriptReturningError("privateAggregation.enableDebugMode(1234n);");

  EXPECT_EQ(error_str,
            "https://example.test/:1 Uncaught TypeError: Invalid argument in "
            "enableDebugMode.");
}

TEST_F(SharedStoragePrivateAggregationTest,
       EnableDebugModeCalledTwice_SecondCallFails) {
  std::string error_str = ExecuteScriptReturningError(
      R"(
        privateAggregation.enableDebugMode({debug_key: 1234n});
        privateAggregation.enableDebugMode();
      )",
      /*flush_and_reset_private_aggregation=*/false);

  EXPECT_EQ(error_str,
            "https://example.test/:3 Uncaught TypeError: enableDebugMode may "
            "be called at most once.");

  // Note that the first call still applies to future requests.
  ExecuteScriptAndValidateContribution(
      "privateAggregation.sendHistogramReport({bucket: 1n, value: 2});",
      /*expected_bucket=*/1,
      /*expected_value=*/2,
      /*expected_debug_mode_details=*/
      blink::mojom::DebugModeDetails::New(
          /*is_enabled=*/true,
          /*debug_key=*/blink::mojom::DebugKey::New(1234u)));
}

// Note that FLEDGE worklets have different behavior in this case.
TEST_F(SharedStoragePrivateAggregationTest,
       EnableDebugModeCalledAfterRequest_DoesntApply) {
  ExecuteScriptAndValidateContribution(
      "privateAggregation.sendHistogramReport({bucket: 1n, value: 2});",
      /*expected_bucket=*/1,
      /*expected_value=*/2,
      /*expected_debug_mode_details=*/
      blink::mojom::DebugModeDetails::New());

  ExecuteScriptExpectNoError(
      "privateAggregation.enableDebugMode({debug_key: 1234n});");
}

TEST_F(SharedStoragePrivateAggregationTest, MultipleDebugModeRequests) {
  EXPECT_CALL(*mock_private_aggregation_host(), SendHistogramReport)
      .WillOnce(testing::Invoke(
          [](std::vector<
                 blink::mojom::AggregatableReportHistogramContributionPtr>
                 contributions,
             blink::mojom::AggregationServiceMode aggregation_mode,
             blink::mojom::DebugModeDetailsPtr debug_mode_details) {
            ASSERT_EQ(contributions.size(), 2u);
            EXPECT_EQ(contributions[0]->bucket, 1);
            EXPECT_EQ(contributions[0]->value, 2);
            EXPECT_EQ(contributions[1]->bucket, 3);
            EXPECT_EQ(contributions[1]->value, 4);
            EXPECT_EQ(aggregation_mode,
                      blink::mojom::AggregationServiceMode::kDefault);
            EXPECT_EQ(debug_mode_details,
                      blink::mojom::DebugModeDetails::New(
                          /*is_enabled=*/true,
                          /*debug_key=*/blink::mojom::DebugKey::New(1234u)));
          }));

  ExecuteScriptExpectNoError(
      R"(
        privateAggregation.enableDebugMode({debug_key: 1234n});
        privateAggregation.sendHistogramReport({bucket: 1n, value: 2});
        privateAggregation.sendHistogramReport({bucket: 3n, value: 4});
      )");
}

}  // namespace shared_storage_worklet
