// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "gin/array_buffer.h"
#include "gin/dictionary.h"
#include "gin/public/isolate_holder.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage_worklet_service.mojom.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/shared_storage/shared_storage_worklet_messaging_proxy.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "v8/include/v8-isolate.h"

namespace blink {

namespace {

constexpr char kModuleScriptSource[] = "https://foo.com/module_script.js";

struct VoidOperationResult {
  bool success = true;
  std::string error_message;
};

using AddModuleResult = VoidOperationResult;
using RunResult = VoidOperationResult;
using SetResult = VoidOperationResult;
using AppendResult = VoidOperationResult;
using DeleteResult = VoidOperationResult;
using ClearResult = VoidOperationResult;

struct SelectURLResult {
  bool success = true;
  std::string error_message;
  uint32_t index = 0;
};

struct GetResult {
  blink::mojom::SharedStorageGetStatus status =
      blink::mojom::SharedStorageGetStatus::kSuccess;
  std::string error_message;
  std::u16string value;
};

struct LengthResult {
  bool success = true;
  std::string error_message;
  uint32_t length = 0;
};

struct RemainingBudgetResult {
  bool success = true;
  std::string error_message;
  double bits = 0;
};

struct SetParams {
  std::u16string key;
  std::u16string value;
  bool ignore_if_present = false;
};

struct AppendParams {
  std::u16string key;
  std::u16string value;
};

std::vector<uint8_t> CreateSerializedDict(
    const std::map<std::string, std::string>& dict) {
#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
  gin::V8Initializer::LoadV8Snapshot();
#endif

  gin::IsolateHolder::Initialize(gin::IsolateHolder::kNonStrictMode,
                                 gin::ArrayBufferAllocator::SharedInstance());

  std::unique_ptr<gin::IsolateHolder> isolate_holder =
      std::make_unique<gin::IsolateHolder>(
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          gin::IsolateHolder::kSingleThread,
          gin::IsolateHolder::IsolateType::kBlinkMainThread);

  v8::Isolate* isolate = isolate_holder->isolate();

  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);

  v8::Global<v8::Context> global_context =
      v8::Global<v8::Context>(isolate, v8::Context::New(isolate));
  v8::Local<v8::Context> context = global_context.Get(isolate);
  v8::Context::Scope context_scope(context);

  v8::Local<v8::Object> v8_value = v8::Object::New(isolate);
  gin::Dictionary gin_dict(isolate, v8_value);
  for (auto const& [key, val] : dict) {
    gin_dict.Set<std::string>(key, val);
  }

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

class TestClient : public blink::mojom::SharedStorageWorkletServiceClient {
 public:
  explicit TestClient(mojo::PendingAssociatedReceiver<
                      blink::mojom::SharedStorageWorkletServiceClient> receiver)
      : receiver_(this, std::move(receiver)) {}

  void SharedStorageSet(const std::u16string& key,
                        const std::u16string& value,
                        bool ignore_if_present,
                        SharedStorageSetCallback callback) override {
    observed_set_params_.push_back({key, value, ignore_if_present});
    std::move(callback).Run(set_result_.success, set_result_.error_message);
  }

  void SharedStorageAppend(const std::u16string& key,
                           const std::u16string& value,
                           SharedStorageAppendCallback callback) override {
    observed_append_params_.push_back({key, value});
    std::move(callback).Run(append_result_.success,
                            append_result_.error_message);
  }

  void SharedStorageDelete(const std::u16string& key,
                           SharedStorageDeleteCallback callback) override {
    observed_delete_params_.push_back(key);
    std::move(callback).Run(delete_result_.success,
                            delete_result_.error_message);
  }

  void SharedStorageClear(SharedStorageClearCallback callback) override {
    observed_clear_count_++;
    std::move(callback).Run(clear_result_.success, clear_result_.error_message);
  }

  void SharedStorageGet(const std::u16string& key,
                        SharedStorageGetCallback callback) override {
    observed_get_params_.push_back(key);
    std::move(callback).Run(get_result_.status, get_result_.error_message,
                            get_result_.value);
  }

  void SharedStorageKeys(
      mojo::PendingRemote<blink::mojom::SharedStorageEntriesListener>
          pending_listener) override {
    NOTREACHED();
  }

  void SharedStorageEntries(
      mojo::PendingRemote<blink::mojom::SharedStorageEntriesListener>
          pending_listener) override {
    NOTREACHED();
  }

  void SharedStorageLength(SharedStorageLengthCallback callback) override {
    observed_length_count_++;
    std::move(callback).Run(length_result_.success,
                            length_result_.error_message,
                            length_result_.length);
  }

  void SharedStorageRemainingBudget(
      SharedStorageRemainingBudgetCallback callback) override {
    observed_remaining_budget_count_++;
    std::move(callback).Run(remaining_budget_result_.success,
                            remaining_budget_result_.error_message,
                            remaining_budget_result_.bits);
  }

  void ConsoleLog(const std::string& message) override {
    observed_console_log_messages_.push_back(message);
  }

  void RecordUseCounters(
      const std::vector<blink::mojom::WebFeature>& features) override {
    NOTREACHED();
  }

  std::vector<SetParams> observed_set_params_;
  std::vector<AppendParams> observed_append_params_;
  std::vector<std::u16string> observed_delete_params_;
  size_t observed_clear_count_ = 0;
  std::vector<std::u16string> observed_get_params_;
  size_t observed_length_count_ = 0;
  size_t observed_remaining_budget_count_ = 0;
  std::vector<std::string> observed_console_log_messages_;

  // Default results to be returned for corresponding operations. They can be
  // overridden.
  SetResult set_result_;
  AppendResult append_result_;
  DeleteResult delete_result_;
  ClearResult clear_result_;
  GetResult get_result_;
  LengthResult length_result_;
  RemainingBudgetResult remaining_budget_result_;

 private:
  mojo::AssociatedReceiver<blink::mojom::SharedStorageWorkletServiceClient>
      receiver_{this};
};

}  // namespace

class SharedStorageWorkletTest : public testing::Test {
 public:
  SharedStorageWorkletTest() {
    WebRuntimeFeatures::EnableSharedStorageAPI(true);
  }

  AddModuleResult AddModule(const std::string& script_content,
                            std::string mime_type = "application/javascript") {
    InitializeWorkletServiceOnce();

    mojo::Remote<network::mojom::URLLoaderFactory> factory;

    network::TestURLLoaderFactory proxied_url_loader_factory;

    auto head = network::mojom::URLResponseHead::New();
    head->mime_type = mime_type;
    head->charset = "us-ascii";

    proxied_url_loader_factory.AddResponse(
        GURL(kModuleScriptSource), std::move(head),
        /*content=*/script_content, network::URLLoaderCompletionStatus());

    proxied_url_loader_factory.Clone(factory.BindNewPipeAndPassReceiver());

    base::test::TestFuture<bool, const std::string&> future;
    shared_storage_worklet_service_->AddModule(
        factory.Unbind(), GURL(kModuleScriptSource), future.GetCallback());

    return {future.Get<0>(), future.Get<1>()};
  }

  SelectURLResult SelectURL(const std::string& name,
                            const std::vector<GURL>& urls,
                            const std::vector<uint8_t>& serialized_data) {
    InitializeWorkletServiceOnce();

    base::test::TestFuture<bool, const std::string&, uint32_t> future;
    shared_storage_worklet_service_->RunURLSelectionOperation(
        name, urls, serialized_data, future.GetCallback());

    return {future.Get<0>(), future.Get<1>(), future.Get<2>()};
  }

  RunResult Run(const std::string& name,
                const std::vector<uint8_t>& serialized_data) {
    InitializeWorkletServiceOnce();

    base::test::TestFuture<bool, const std::string&> future;
    shared_storage_worklet_service_->RunOperation(name, serialized_data,
                                                  future.GetCallback());

    return {future.Get<0>(), future.Get<1>()};
  }

 protected:
  mojo::Remote<mojom::SharedStorageWorkletService>
      shared_storage_worklet_service_;

  Persistent<SharedStorageWorkletMessagingProxy> messaging_proxy_;

  absl::optional<std::u16string> embedder_context_;

  base::test::TestFuture<void> worklet_terminated_future_;

  std::unique_ptr<TestClient> test_client_;

  base::HistogramTester histogram_tester_;

  bool worklet_service_initialized_ = false;

 private:
  void InitializeWorkletServiceOnce() {
    if (worklet_service_initialized_) {
      return;
    }

    mojo::PendingReceiver<mojom::SharedStorageWorkletService> receiver =
        shared_storage_worklet_service_.BindNewPipeAndPassReceiver();

    messaging_proxy_ = MakeGarbageCollected<SharedStorageWorkletMessagingProxy>(
        base::SingleThreadTaskRunner::GetCurrentDefault(), std::move(receiver),
        worklet_terminated_future_.GetCallback());

    mojo::PendingAssociatedRemote<mojom::SharedStorageWorkletServiceClient>
        pending_remote;
    mojo::PendingAssociatedReceiver<mojom::SharedStorageWorkletServiceClient>
        pending_receiver = pending_remote.InitWithNewEndpointAndPassReceiver();

    test_client_ = std::make_unique<TestClient>(std::move(pending_receiver));

    shared_storage_worklet_service_->Initialize(
        std::move(pending_remote),
        /*private_aggregation_permissions_policy_allowed=*/true,
        mojo::PendingRemote<mojom::PrivateAggregationHost>(),
        embedder_context_);

    worklet_service_initialized_ = true;
  }
};

TEST_F(SharedStorageWorkletTest, AddModule_EmptyScriptSuccess) {
  AddModuleResult result = AddModule(/*script_content=*/"");
  EXPECT_TRUE(result.success);
  EXPECT_TRUE(result.error_message.empty());
}

TEST_F(SharedStorageWorkletTest, AddModule_SimpleScriptSuccess) {
  AddModuleResult result = AddModule(/*script_content=*/"let a = 1;");
  EXPECT_TRUE(result.success);
  EXPECT_TRUE(result.error_message.empty());
}

TEST_F(SharedStorageWorkletTest, AddModule_SimpleScriptError) {
  AddModuleResult result = AddModule(/*script_content=*/"a;");
  EXPECT_FALSE(result.success);
  EXPECT_THAT(result.error_message,
              testing::HasSubstr("ReferenceError: a is not defined"));
}

TEST_F(SharedStorageWorkletTest, AddModule_ScriptDownloadError) {
  AddModuleResult result = AddModule(/*script_content=*/"",
                                     /*mime_type=*/"unsupported_mime_type");
  EXPECT_FALSE(result.success);
  EXPECT_EQ(result.error_message,
            "Rejecting load of https://foo.com/module_script.js due to "
            "unexpected MIME type.");
}

TEST_F(SharedStorageWorkletTest, WorkletTerminationDueToDisconnect) {
  AddModuleResult result = AddModule(/*script_content=*/"");

  // Trigger the disconnect handler.
  shared_storage_worklet_service_.reset();

  // Callback called means the worklet has terminated successfully.
  EXPECT_TRUE(worklet_terminated_future_.Wait());
}

TEST_F(SharedStorageWorkletTest, ConsoleLog_DuringAddModule) {
  AddModuleResult result = AddModule(/*script_content=*/R"(
    console.log(123, "abc");
  )");

  EXPECT_TRUE(result.success);
  EXPECT_TRUE(result.error_message.empty());

  EXPECT_EQ(test_client_->observed_console_log_messages_.size(), 1u);
  EXPECT_EQ(test_client_->observed_console_log_messages_[0], "123 abc");
}

TEST_F(SharedStorageWorkletTest,
       GlobalScopeObjectsAndFunctions_DuringAddModule) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
    var expectedObjects = [
      "console"
    ];

    var expectedFunctions = [
      "register",
      "console.log",
    ];

    // `privateAggregation` is not implemented yet.
    var expectedUndefinedVariables = [
      "privateAggregation"
    ];

    for (let expectedObject of expectedObjects) {
      if (eval("typeof " + expectedObject) !== "object") {
        throw Error(expectedObject + " is not object type.")
      }
    }

    for (let expectedFunction of expectedFunctions) {
      if (eval("typeof " + expectedFunction) !== "function") {
        throw Error(expectedFunction + " is not function type.")
      }
    }

    for (let expectedUndefined of expectedUndefinedVariables) {
      if (eval("typeof " + expectedUndefined) !== "undefined") {
        throw Error(expectedUndefined + " is not undefined.")
      }
    }

    // Verify that trying to access `sharedStorage` would throw a custom error.
    try {
      sharedStorage;
    } catch (e) {
      console.log("Expected error:", e.message);
    }
  )");

  EXPECT_TRUE(add_module_result.success);
  EXPECT_EQ(add_module_result.error_message, "");

  EXPECT_EQ(test_client_->observed_console_log_messages_.size(), 1u);
  EXPECT_EQ(test_client_->observed_console_log_messages_[0],
            "Expected error: Failed to read the 'sharedStorage' property from "
            "'SharedStorageWorkletGlobalScope': sharedStorage cannot be "
            "accessed during addModule().");
}

TEST_F(SharedStorageWorkletTest,
       RegisterOperation_MissingOperationNameArgument) {
  AddModuleResult result = AddModule(/*script_content=*/R"(
      register();
  )");

  EXPECT_FALSE(result.success);
  EXPECT_THAT(result.error_message,
              testing::HasSubstr("2 arguments required, but only 0 present"));
}

TEST_F(SharedStorageWorkletTest, RegisterOperation_MissingClassArgument) {
  AddModuleResult result = AddModule(/*script_content=*/R"(
      register("test-operation");
  )");

  EXPECT_FALSE(result.success);
  EXPECT_THAT(result.error_message,
              testing::HasSubstr("2 arguments required, but only 1 present"));
}

TEST_F(SharedStorageWorkletTest, RegisterOperation_EmptyOperationName) {
  AddModuleResult result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {}
      }

      register("", TestClass);
  )");

  EXPECT_FALSE(result.success);
  EXPECT_THAT(result.error_message,
              testing::HasSubstr("Operation name cannot be empty"));
}

TEST_F(SharedStorageWorkletTest, RegisterOperation_ClassArgumentNotAFunction) {
  AddModuleResult result = AddModule(/*script_content=*/R"(
      register("test-operation", {});
  )");

  EXPECT_FALSE(result.success);
  EXPECT_THAT(result.error_message,
              testing::HasSubstr("parameter 2 is not of type 'Function'"));
}

TEST_F(SharedStorageWorkletTest, RegisterOperation_MissingRunFunction) {
  AddModuleResult result = AddModule(/*script_content=*/R"(
      class TestClass {
        constructor() {
          this.run = 1;
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_FALSE(result.success);
  EXPECT_THAT(result.error_message,
              testing::HasSubstr("Property \"run\" doesn't exist"));
}

TEST_F(SharedStorageWorkletTest,
       RegisterOperation_ClassArgumentPrototypeNotAnObject) {
  AddModuleResult result = AddModule(/*script_content=*/R"(
      function test() {};
      test.prototype = 123;

      register("test-operation", test);
  )");

  EXPECT_FALSE(result.success);
  EXPECT_THAT(result.error_message,
              testing::HasSubstr("constructor prototype is not an object"));
}

TEST_F(SharedStorageWorkletTest, RegisterOperation_Success) {
  AddModuleResult result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {}
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(result.success);
  EXPECT_TRUE(result.error_message.empty());
}

TEST_F(SharedStorageWorkletTest, RegisterOperation_AlreadyRegistered) {
  AddModuleResult result = AddModule(/*script_content=*/R"(
    class TestClass1 {
      async run() {}
    }

    class TestClass2 {
      async run() {}
    }

    register("test-operation", TestClass1);
    register("test-operation", TestClass2);
  )");

  EXPECT_FALSE(result.success);
  EXPECT_THAT(result.error_message,
              testing::HasSubstr("Operation name already registered"));
}

TEST_F(SharedStorageWorkletTest, SelectURL_BeforeAddModuleFinish) {
  SelectURLResult select_url_result =
      SelectURL("test-operation", /*urls=*/{}, /*serialized_data=*/{});

  EXPECT_FALSE(select_url_result.success);
  EXPECT_THAT(select_url_result.error_message,
              testing::HasSubstr("The module script hasn't been loaded"));
  EXPECT_EQ(select_url_result.index, 0u);
}

TEST_F(SharedStorageWorkletTest, SelectURL_OperationNameNotRegistered) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run(urls) {}
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  SelectURLResult select_url_result =
      SelectURL("unregistered-operation", /*urls=*/{}, /*serialized_data=*/{});

  EXPECT_FALSE(select_url_result.success);
  EXPECT_THAT(select_url_result.error_message,
              testing::HasSubstr("Cannot find operation name"));
  EXPECT_EQ(select_url_result.index, 0u);
}

TEST_F(SharedStorageWorkletTest, SelectURL_FunctionError) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run(urls) {
          a;
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  SelectURLResult select_url_result =
      SelectURL("test-operation", /*urls=*/{}, /*serialized_data=*/{});

  EXPECT_FALSE(select_url_result.success);
  EXPECT_THAT(select_url_result.error_message,
              testing::HasSubstr("ReferenceError: a is not defined"));
  EXPECT_EQ(select_url_result.index, 0u);
}

TEST_F(SharedStorageWorkletTest, SelectURL_FulfilledSynchronously) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run(urls) {
          return 1;
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  SelectURLResult select_url_result =
      SelectURL("test-operation",
                /*urls=*/{GURL("https://foo0.com"), GURL("https://foo1.com")},
                /*serialized_data=*/{});

  EXPECT_TRUE(select_url_result.success);
  EXPECT_TRUE(select_url_result.error_message.empty());
  EXPECT_EQ(select_url_result.index, 1u);
}

TEST_F(SharedStorageWorkletTest, SelectURL_RejectedAsynchronously) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run(urls) {
          return sharedStorage.length();
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  test_client_->length_result_ =
      LengthResult{.success = false, .error_message = "error 123", .length = 0};

  SelectURLResult select_url_result =
      SelectURL("test-operation",
                /*urls=*/{GURL("https://foo0.com"), GURL("https://foo1.com")},
                /*serialized_data=*/{});

  EXPECT_FALSE(select_url_result.success);
  EXPECT_THAT(select_url_result.error_message, testing::HasSubstr("error 123"));
  EXPECT_EQ(select_url_result.index, 0u);
}

TEST_F(SharedStorageWorkletTest, SelectURL_FulfilledAsynchronously) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run(urls) {
          return sharedStorage.length();
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  test_client_->length_result_ = LengthResult{
      .success = true, .error_message = std::string(), .length = 1};

  SelectURLResult select_url_result =
      SelectURL("test-operation",
                /*urls=*/{GURL("https://foo0.com"), GURL("https://foo1.com")},
                /*serialized_data=*/{});

  EXPECT_TRUE(select_url_result.success);
  EXPECT_TRUE(select_url_result.error_message.empty());
  EXPECT_EQ(select_url_result.index, 1u);
}

TEST_F(SharedStorageWorkletTest, SelectURL_StringConvertedToUint32) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run(urls) {
          return "1";
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  SelectURLResult select_url_result =
      SelectURL("test-operation",
                /*urls=*/{GURL("https://foo0.com"), GURL("https://foo1.com")},
                /*serialized_data=*/{});

  EXPECT_TRUE(select_url_result.success);
  EXPECT_TRUE(select_url_result.error_message.empty());
  EXPECT_EQ(select_url_result.index, 1u);
}

TEST_F(SharedStorageWorkletTest, SelectURL_NumberOverflow) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run(urls) {
          return -4294967295;
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  SelectURLResult select_url_result =
      SelectURL("test-operation",
                /*urls=*/{GURL("https://foo0.com"), GURL("https://foo1.com")},
                /*serialized_data=*/{});

  EXPECT_TRUE(select_url_result.success);
  EXPECT_TRUE(select_url_result.error_message.empty());
  EXPECT_EQ(select_url_result.index, 1u);
}

TEST_F(SharedStorageWorkletTest, SelectURL_NonNumericStringConvertedTo0) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run(urls) {
          return "abc";
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  SelectURLResult select_url_result =
      SelectURL("test-operation",
                /*urls=*/{GURL("https://foo0.com"), GURL("https://foo1.com")},
                /*serialized_data=*/{});

  EXPECT_TRUE(select_url_result.success);
  EXPECT_TRUE(select_url_result.error_message.empty());
  EXPECT_EQ(select_url_result.index, 0u);
}

TEST_F(SharedStorageWorkletTest, SelectURL_DefaultUndefinedResultConvertedTo0) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run(urls) {}
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  SelectURLResult select_url_result =
      SelectURL("test-operation",
                /*urls=*/{GURL("https://foo0.com"), GURL("https://foo1.com")},
                /*serialized_data=*/{});

  EXPECT_TRUE(select_url_result.success);
  EXPECT_TRUE(select_url_result.error_message.empty());
  EXPECT_EQ(select_url_result.index, 0u);
}

// For a run() member function that is not marked "async", it will still be
// treated as async.
TEST_F(SharedStorageWorkletTest, SelectURL_NoExplicitAsync) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        run(urls) {
          return 1;
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  SelectURLResult select_url_result =
      SelectURL("test-operation",
                /*urls=*/{GURL("https://foo0.com"), GURL("https://foo1.com")},
                /*serialized_data=*/{});

  EXPECT_TRUE(select_url_result.success);
  EXPECT_TRUE(select_url_result.error_message.empty());
  EXPECT_EQ(select_url_result.index, 1u);
}

TEST_F(SharedStorageWorkletTest, SelectURL_ReturnValueOutOfRange) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run(urls) {
          return 2;
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  SelectURLResult select_url_result =
      SelectURL("test-operation",
                /*urls=*/{GURL("https://foo0.com"), GURL("https://foo1.com")},
                /*serialized_data=*/{});

  EXPECT_FALSE(select_url_result.success);
  EXPECT_THAT(
      select_url_result.error_message,
      testing::HasSubstr(
          "Promise resolved to a number outside the length of the input urls"));
  EXPECT_EQ(select_url_result.index, 0u);
}

TEST_F(SharedStorageWorkletTest, SelectURL_ReturnValueToUint32Error) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
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

  EXPECT_TRUE(add_module_result.success);

  SelectURLResult select_url_result =
      SelectURL("test-operation",
                /*urls=*/{GURL("https://foo0.com"), GURL("https://foo1.com")},
                /*serialized_data=*/{});

  EXPECT_FALSE(select_url_result.success);
  EXPECT_THAT(
      select_url_result.error_message,
      testing::HasSubstr("Promise did not resolve to an uint32 number"));
  EXPECT_EQ(select_url_result.index, 0u);
}

TEST_F(SharedStorageWorkletTest,
       SelectURL_ValidateUrlsAndDataParamViaConsoleLog) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run(urls, data) {
          console.log(JSON.stringify(urls, Object.keys(urls).sort()));
          console.log(JSON.stringify(data, Object.keys(data).sort()));

          return 1;
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  SelectURLResult select_url_result =
      SelectURL("test-operation",
                /*urls=*/{GURL("https://foo0.com"), GURL("https://foo1.com")},
                /*serialized_data=*/
                CreateSerializedDict({{"customField", "customValue"}}));

  EXPECT_TRUE(select_url_result.success);
  EXPECT_TRUE(select_url_result.error_message.empty());
  EXPECT_EQ(select_url_result.index, 1u);

  EXPECT_EQ(test_client_->observed_console_log_messages_.size(), 2u);
  EXPECT_EQ(test_client_->observed_console_log_messages_[0],
            "[\"https://foo0.com/\",\"https://foo1.com/\"]");
  EXPECT_EQ(test_client_->observed_console_log_messages_[1],
            "{\"customField\":\"customValue\"}");
}

TEST_F(SharedStorageWorkletTest, Run_BeforeAddModuleFinish) {
  RunResult run_result = Run("test-operation", /*serialized_data=*/{});

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(run_result.error_message,
              testing::HasSubstr("The module script hasn't been loaded"));
}

TEST_F(SharedStorageWorkletTest, Run_OperationNameNotRegistered) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {}
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("unregistered-operation", /*serialized_data=*/{});

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(run_result.error_message,
              testing::HasSubstr("Cannot find operation name"));
}

TEST_F(SharedStorageWorkletTest, Run_FunctionError) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          a;
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", /*serialized_data=*/{});

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(run_result.error_message,
              testing::HasSubstr("ReferenceError: a is not defined"));
}

TEST_F(SharedStorageWorkletTest, Run_FulfilledSynchronously) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {}
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", /*serialized_data=*/{});

  EXPECT_TRUE(run_result.success);
  EXPECT_TRUE(run_result.error_message.empty());
}

TEST_F(SharedStorageWorkletTest, Run_RejectedAsynchronously) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          return sharedStorage.clear();
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  test_client_->clear_result_ =
      ClearResult{.success = false, .error_message = "error 123"};

  RunResult run_result = Run("test-operation", /*serialized_data=*/{});

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(run_result.error_message, testing::HasSubstr("error 123"));
}

TEST_F(SharedStorageWorkletTest, Run_FulfilledAsynchronously) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          return sharedStorage.clear();
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", /*serialized_data=*/{});

  EXPECT_TRUE(run_result.success);
  EXPECT_TRUE(run_result.error_message.empty());
}

TEST_F(SharedStorageWorkletTest, Run_Microtask) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await Promise.resolve(0);
          return 0;
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", /*serialized_data=*/{});

  EXPECT_TRUE(run_result.success);
  EXPECT_TRUE(run_result.error_message.empty());
}

TEST_F(SharedStorageWorkletTest, Run_ValidateDataParamViaConsoleLog) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run(data) {
          console.log(JSON.stringify(data, Object.keys(data).sort()));

          return 1;
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result =
      Run("test-operation",
          /*serialized_data=*/
          CreateSerializedDict({{"customField", "customValue"}}));

  EXPECT_TRUE(run_result.success);
  EXPECT_TRUE(run_result.error_message.empty());

  EXPECT_EQ(test_client_->observed_console_log_messages_.size(), 1u);
  EXPECT_EQ(test_client_->observed_console_log_messages_[0],
            "{\"customField\":\"customValue\"}");
}

TEST_F(SharedStorageWorkletTest, SelectURLAndRunOnSameRegisteredOperation) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          return 1;
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  SelectURLResult select_url_result =
      SelectURL("test-operation",
                /*urls=*/{GURL("https://foo0.com"), GURL("https://foo1.com")},
                /*serialized_data=*/{});

  EXPECT_TRUE(select_url_result.success);
  EXPECT_TRUE(select_url_result.error_message.empty());
  EXPECT_EQ(select_url_result.index, 1u);

  RunResult run_result = Run("test-operation",
                             /*serialized_data=*/{});

  EXPECT_TRUE(run_result.success);
  EXPECT_TRUE(run_result.error_message.empty());
}

TEST_F(SharedStorageWorkletTest,
       GlobalScopeObjectsAndFunctions_AfterAddModuleSuccess) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          var expectedObjects = [
            "sharedStorage"
          ];

          var expectedFunctions = [
            "register",
            "sharedStorage.set",
            "sharedStorage.append",
            "sharedStorage.delete",
            "sharedStorage.clear",
            "sharedStorage.get",
            "sharedStorage.length",
            "sharedStorage.remainingBudget"
          ];

          // Those are either not implemented yet, or should stay undefined.
          var expectedUndefinedVariables = [
            "sharedStorage.selectURL",
            "sharedStorage.run",
            "sharedStorage.worklet",
            "sharedStorage.context",
            "sharedStorage.keys",
            "sharedStorage.entries",
            "privateAggregation"
          ];

          for (let expectedObject of expectedObjects) {
            if (eval("typeof " + expectedObject) !== "object") {
              throw Error(expectedObject + " is not object type.")
            }
          }

          for (let expectedFunction of expectedFunctions) {
            if (eval("typeof " + expectedFunction) !== "function") {
              throw Error(expectedFunction + " is not function type.")
            }
          }

          for (let expectedUndefined of expectedUndefinedVariables) {
            if (eval("typeof " + expectedUndefined) !== "undefined") {
              throw Error(expectedUndefined + " is not undefined.")
            }
          }
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", /*serialized_data=*/{});

  EXPECT_TRUE(run_result.success);
  EXPECT_EQ(run_result.error_message, "");
}

TEST_F(SharedStorageWorkletTest,
       GlobalScopeObjectsAndFunctions_AfterAddModuleFailure) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          var expectedObjects = [
            "sharedStorage"
          ];

          var expectedFunctions = [
            "register",
            "sharedStorage.set",
            "sharedStorage.append",
            "sharedStorage.delete",
            "sharedStorage.clear",
            "sharedStorage.get",
            "sharedStorage.length",
            "sharedStorage.remainingBudget"
          ];

          // Those are either not implemented yet, or should stay undefined.
          var expectedUndefinedVariables = [
            "sharedStorage.selectURL",
            "sharedStorage.run",
            "sharedStorage.worklet",
            "sharedStorage.context",
            "sharedStorage.keys",
            "sharedStorage.entries",
            "privateAggregation"
          ];

          for (let expectedObject of expectedObjects) {
            if (eval("typeof " + expectedObject) !== "object") {
              throw Error(expectedObject + " is not object type.")
            }
          }

          for (let expectedFunction of expectedFunctions) {
            if (eval("typeof " + expectedFunction) !== "function") {
              throw Error(expectedFunction + " is not function type.")
            }
          }

          for (let expectedUndefined of expectedUndefinedVariables) {
            if (eval("typeof " + expectedUndefined) !== "undefined") {
              throw Error(expectedUndefined + " is not undefined.")
            }
          }
        }
      }

      register("test-operation", TestClass);

      // This should fail the addModule()
      a;
  )");

  EXPECT_FALSE(add_module_result.success);
  EXPECT_THAT(add_module_result.error_message,
              testing::HasSubstr("ReferenceError: a is not defined"));

  RunResult run_result = Run("test-operation", /*serialized_data=*/{});

  EXPECT_TRUE(run_result.success);
  EXPECT_EQ(run_result.error_message, "");
}

TEST_F(SharedStorageWorkletTest, Set_MissingKey) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.set();
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", /*serialized_data=*/{});

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(run_result.error_message,
              testing::HasSubstr("2 arguments required, but only 0 present"));

  EXPECT_EQ(test_client_->observed_set_params_.size(), 0u);
}

TEST_F(SharedStorageWorkletTest, Set_InvalidKey_Empty) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.set("", "value");
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", /*serialized_data=*/{});

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(
      run_result.error_message,
      testing::HasSubstr("Length of the \"key\" parameter is not valid"));

  EXPECT_EQ(test_client_->observed_set_params_.size(), 0u);
}

TEST_F(SharedStorageWorkletTest, Set_InvalidKey_TooLong) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.set("a".repeat(1025), "value");
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", /*serialized_data=*/{});

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(
      run_result.error_message,
      testing::HasSubstr("Length of the \"key\" parameter is not valid"));

  EXPECT_EQ(test_client_->observed_set_params_.size(), 0u);
}

TEST_F(SharedStorageWorkletTest, Set_MissingValue) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.set("key");
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", /*serialized_data=*/{});

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(run_result.error_message,
              testing::HasSubstr("2 arguments required, but only 1 present"));

  EXPECT_EQ(test_client_->observed_set_params_.size(), 0u);
}

TEST_F(SharedStorageWorkletTest, Set_InvalidValue_TooLong) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.set("key", "a".repeat(1025));
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", /*serialized_data=*/{});

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(
      run_result.error_message,
      testing::HasSubstr("Length of the \"value\" parameter is not valid"));

  EXPECT_EQ(test_client_->observed_set_params_.size(), 0u);
}

TEST_F(SharedStorageWorkletTest, Set_InvalidOptions) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.set("key", "value", true);
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", /*serialized_data=*/{});

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(
      run_result.error_message,
      testing::HasSubstr(
          "The provided value is not of type 'SharedStorageSetMethodOptions'"));

  EXPECT_EQ(test_client_->observed_set_params_.size(), 0u);
}

TEST_F(SharedStorageWorkletTest, Set_ClientError) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.set("key0", "value0");
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  test_client_->set_result_ =
      SetResult{.success = false, .error_message = "error 123"};

  RunResult run_result = Run("test-operation", /*serialized_data=*/{});

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(run_result.error_message, testing::HasSubstr("error 123"));

  EXPECT_EQ(test_client_->observed_set_params_.size(), 1u);
  EXPECT_EQ(test_client_->observed_set_params_[0].key, u"key0");
  EXPECT_EQ(test_client_->observed_set_params_[0].value, u"value0");
}

TEST_F(SharedStorageWorkletTest, Set_Success) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.set("key0", "value0");
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", /*serialized_data=*/{});

  EXPECT_TRUE(run_result.success);
  EXPECT_TRUE(run_result.error_message.empty());

  EXPECT_EQ(test_client_->observed_set_params_.size(), 1u);
  EXPECT_EQ(test_client_->observed_set_params_[0].key, u"key0");
  EXPECT_EQ(test_client_->observed_set_params_[0].value, u"value0");
}

TEST_F(SharedStorageWorkletTest, Set_IgnoreIfPresent_True) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.set("key", "value", {ignoreIfPresent: true});

          // A non-empty string will evaluate to true.
          await sharedStorage.set("key", "value", {ignoreIfPresent: "false"});

          // A dictionary object will evaluate to true.
          await sharedStorage.set("key", "value", {ignoreIfPresent: {}});
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", /*serialized_data=*/{});

  EXPECT_TRUE(run_result.success);
  EXPECT_TRUE(run_result.error_message.empty());

  EXPECT_EQ(test_client_->observed_set_params_.size(), 3u);
  EXPECT_TRUE(test_client_->observed_set_params_[0].ignore_if_present);
  EXPECT_TRUE(test_client_->observed_set_params_[1].ignore_if_present);
  EXPECT_TRUE(test_client_->observed_set_params_[2].ignore_if_present);
}

TEST_F(SharedStorageWorkletTest, Set_IgnoreIfPresent_False) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.set("key", "value");
          await sharedStorage.set("key", "value", {});
          await sharedStorage.set("key", "value", {ignoreIfPresent: false});
          await sharedStorage.set("key", "value", {ignoreIfPresent: ""});
          await sharedStorage.set("key", "value", {ignoreIfPresent: null});
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", /*serialized_data=*/{});

  EXPECT_TRUE(run_result.success);
  EXPECT_TRUE(run_result.error_message.empty());

  EXPECT_EQ(test_client_->observed_set_params_.size(), 5u);
  EXPECT_FALSE(test_client_->observed_set_params_[0].ignore_if_present);
  EXPECT_FALSE(test_client_->observed_set_params_[1].ignore_if_present);
  EXPECT_FALSE(test_client_->observed_set_params_[2].ignore_if_present);
  EXPECT_FALSE(test_client_->observed_set_params_[3].ignore_if_present);
  EXPECT_FALSE(test_client_->observed_set_params_[4].ignore_if_present);
}

TEST_F(SharedStorageWorkletTest, Set_KeyAndValueConvertedToString) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.set(123, 456);
          await sharedStorage.set(null, null);
          await sharedStorage.set(undefined, undefined);
          await sharedStorage.set({dictKey1: 'dictValue1'}, {dictKey2: 'dictValue2'});
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", /*serialized_data=*/{});

  EXPECT_TRUE(run_result.success);
  EXPECT_TRUE(run_result.error_message.empty());

  EXPECT_EQ(test_client_->observed_set_params_.size(), 4u);
  EXPECT_EQ(test_client_->observed_set_params_[0].key, u"123");
  EXPECT_EQ(test_client_->observed_set_params_[0].value, u"456");
  EXPECT_EQ(test_client_->observed_set_params_[1].key, u"null");
  EXPECT_EQ(test_client_->observed_set_params_[1].value, u"null");
  EXPECT_EQ(test_client_->observed_set_params_[2].key, u"undefined");
  EXPECT_EQ(test_client_->observed_set_params_[2].value, u"undefined");
  EXPECT_EQ(test_client_->observed_set_params_[3].key, u"[object Object]");
  EXPECT_EQ(test_client_->observed_set_params_[3].value, u"[object Object]");
}

TEST_F(SharedStorageWorkletTest, Set_ParamConvertedToStringError) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          class CustomClass {
            toString() { throw Error("error 123"); }
          };

          await sharedStorage.set(new CustomClass(), "value");
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", /*serialized_data=*/{});

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(run_result.error_message, testing::HasSubstr("error 123"));

  EXPECT_EQ(test_client_->observed_set_params_.size(), 0u);
}

TEST_F(SharedStorageWorkletTest, Append_MissingKey) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.append();
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", /*serialized_data=*/{});

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(run_result.error_message,
              testing::HasSubstr("2 arguments required, but only 0 present"));

  EXPECT_EQ(test_client_->observed_append_params_.size(), 0u);
}

TEST_F(SharedStorageWorkletTest, Append_InvalidKey_Empty) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.append("", "value");
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", /*serialized_data=*/{});

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(
      run_result.error_message,
      testing::HasSubstr("Length of the \"key\" parameter is not valid"));

  EXPECT_EQ(test_client_->observed_append_params_.size(), 0u);
}

TEST_F(SharedStorageWorkletTest, Append_InvalidKey_TooLong) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.append("a".repeat(1025), "value");
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", /*serialized_data=*/{});

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(
      run_result.error_message,
      testing::HasSubstr("Length of the \"key\" parameter is not valid"));

  EXPECT_EQ(test_client_->observed_append_params_.size(), 0u);
}

TEST_F(SharedStorageWorkletTest, Append_MissingValue) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.append("key");
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", /*serialized_data=*/{});

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(run_result.error_message,
              testing::HasSubstr("2 arguments required, but only 1 present"));

  EXPECT_EQ(test_client_->observed_append_params_.size(), 0u);
}

TEST_F(SharedStorageWorkletTest, Append_InvalidValue_TooLong) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.append("key", "a".repeat(1025));
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", /*serialized_data=*/{});

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(
      run_result.error_message,
      testing::HasSubstr("Length of the \"value\" parameter is not valid"));

  EXPECT_EQ(test_client_->observed_append_params_.size(), 0u);
}

TEST_F(SharedStorageWorkletTest, Append_ClientError) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.append("key0", "value0");
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  test_client_->append_result_ =
      AppendResult{.success = false, .error_message = "error 123"};

  RunResult run_result = Run("test-operation", /*serialized_data=*/{});

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(run_result.error_message, testing::HasSubstr("error 123"));

  EXPECT_EQ(test_client_->observed_append_params_.size(), 1u);
  EXPECT_EQ(test_client_->observed_append_params_[0].key, u"key0");
  EXPECT_EQ(test_client_->observed_append_params_[0].value, u"value0");
}

TEST_F(SharedStorageWorkletTest, Append_Success) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.append("key0", "value0");
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", /*serialized_data=*/{});

  EXPECT_TRUE(run_result.success);
  EXPECT_TRUE(run_result.error_message.empty());

  EXPECT_EQ(test_client_->observed_append_params_.size(), 1u);
  EXPECT_EQ(test_client_->observed_append_params_[0].key, u"key0");
  EXPECT_EQ(test_client_->observed_append_params_[0].value, u"value0");
}

TEST_F(SharedStorageWorkletTest, Delete_MissingKey) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.delete();
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", /*serialized_data=*/{});

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(run_result.error_message,
              testing::HasSubstr("1 argument required, but only 0 present"));

  EXPECT_EQ(test_client_->observed_delete_params_.size(), 0u);
}

TEST_F(SharedStorageWorkletTest, Delete_InvalidKey_Empty) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.delete("");
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", /*serialized_data=*/{});

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(
      run_result.error_message,
      testing::HasSubstr("Length of the \"key\" parameter is not valid"));

  EXPECT_EQ(test_client_->observed_delete_params_.size(), 0u);
}

TEST_F(SharedStorageWorkletTest, Delete_InvalidKey_TooLong) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.delete("a".repeat(1025), "value");
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", /*serialized_data=*/{});

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(
      run_result.error_message,
      testing::HasSubstr("Length of the \"key\" parameter is not valid"));

  EXPECT_EQ(test_client_->observed_delete_params_.size(), 0u);
}

TEST_F(SharedStorageWorkletTest, Delete_ClientError) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.delete("key0");
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  test_client_->delete_result_ =
      DeleteResult{.success = false, .error_message = "error 123"};

  RunResult run_result = Run("test-operation", /*serialized_data=*/{});

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(run_result.error_message, testing::HasSubstr("error 123"));

  EXPECT_EQ(test_client_->observed_delete_params_.size(), 1u);
  EXPECT_EQ(test_client_->observed_delete_params_[0], u"key0");
}

TEST_F(SharedStorageWorkletTest, Delete_Success) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.delete("key0");
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", /*serialized_data=*/{});

  EXPECT_TRUE(run_result.success);
  EXPECT_TRUE(run_result.error_message.empty());

  EXPECT_EQ(test_client_->observed_delete_params_.size(), 1u);
  EXPECT_EQ(test_client_->observed_delete_params_[0], u"key0");
}

TEST_F(SharedStorageWorkletTest, Clear_ClientError) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.clear();
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  test_client_->clear_result_ =
      ClearResult{.success = false, .error_message = "error 123"};

  RunResult run_result = Run("test-operation", /*serialized_data=*/{});

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(run_result.error_message, testing::HasSubstr("error 123"));

  EXPECT_EQ(test_client_->observed_clear_count_, 1u);
}

TEST_F(SharedStorageWorkletTest, Clear_Success) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.clear();
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", /*serialized_data=*/{});

  EXPECT_TRUE(run_result.success);
  EXPECT_TRUE(run_result.error_message.empty());

  EXPECT_EQ(test_client_->observed_clear_count_, 1u);
}

TEST_F(SharedStorageWorkletTest, Get_MissingKey) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.get();
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", /*serialized_data=*/{});

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(run_result.error_message,
              testing::HasSubstr("1 argument required, but only 0 present"));

  EXPECT_EQ(test_client_->observed_get_params_.size(), 0u);
}

TEST_F(SharedStorageWorkletTest, Get_InvalidKey_Empty) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.get("");
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", /*serialized_data=*/{});

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(
      run_result.error_message,
      testing::HasSubstr("Length of the \"key\" parameter is not valid"));

  EXPECT_EQ(test_client_->observed_get_params_.size(), 0u);
}

TEST_F(SharedStorageWorkletTest, Get_InvalidKey_TooLong) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.get("a".repeat(1025), "value");
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", /*serialized_data=*/{});

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(
      run_result.error_message,
      testing::HasSubstr("Length of the \"key\" parameter is not valid"));

  EXPECT_EQ(test_client_->observed_get_params_.size(), 0u);
}

TEST_F(SharedStorageWorkletTest, Get_ClientError) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          let a = await sharedStorage.get("key0");
          console.log(a);
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  test_client_->get_result_ =
      GetResult{.status = blink::mojom::SharedStorageGetStatus::kError,
                .error_message = "error 123",
                .value = std::u16string()};

  RunResult run_result = Run("test-operation", /*serialized_data=*/{});

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(run_result.error_message, testing::HasSubstr("error 123"));

  EXPECT_EQ(test_client_->observed_get_params_.size(), 1u);
  EXPECT_EQ(test_client_->observed_get_params_[0], u"key0");

  EXPECT_EQ(test_client_->observed_console_log_messages_.size(), 0u);
}

TEST_F(SharedStorageWorkletTest, Get_NotFound) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          let a = await sharedStorage.get("key0");
          console.log(a);
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  test_client_->get_result_ =
      GetResult{.status = blink::mojom::SharedStorageGetStatus::kNotFound,
                .error_message = std::string(),
                .value = std::u16string()};

  RunResult run_result = Run("test-operation", /*serialized_data=*/{});

  EXPECT_TRUE(run_result.success);
  EXPECT_TRUE(run_result.error_message.empty());

  EXPECT_EQ(test_client_->observed_get_params_.size(), 1u);
  EXPECT_EQ(test_client_->observed_get_params_[0], u"key0");

  EXPECT_EQ(test_client_->observed_console_log_messages_.size(), 1u);
  EXPECT_EQ(test_client_->observed_console_log_messages_[0], "undefined");
}

TEST_F(SharedStorageWorkletTest, Get_Success) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          let a = await sharedStorage.get("key0");
          console.log(a);
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  test_client_->get_result_ =
      GetResult{.status = blink::mojom::SharedStorageGetStatus::kSuccess,
                .error_message = std::string(),
                .value = u"value0"};

  RunResult run_result = Run("test-operation", /*serialized_data=*/{});

  EXPECT_TRUE(run_result.success);
  EXPECT_TRUE(run_result.error_message.empty());

  EXPECT_EQ(test_client_->observed_get_params_.size(), 1u);
  EXPECT_EQ(test_client_->observed_get_params_[0], u"key0");

  EXPECT_EQ(test_client_->observed_console_log_messages_.size(), 1u);
  EXPECT_EQ(test_client_->observed_console_log_messages_[0], "value0");
}

TEST_F(SharedStorageWorkletTest, Length_ClientError) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          let a = await sharedStorage.length();
          console.log(a);
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  test_client_->length_result_ =
      LengthResult{.success = false, .error_message = "error 123", .length = 0};

  RunResult run_result = Run("test-operation", /*serialized_data=*/{});

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(run_result.error_message, testing::HasSubstr("error 123"));

  EXPECT_EQ(test_client_->observed_length_count_, 1u);

  EXPECT_EQ(test_client_->observed_console_log_messages_.size(), 0u);
}

TEST_F(SharedStorageWorkletTest, Length_Success) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          let a = await sharedStorage.length();
          console.log(a);
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  test_client_->length_result_ = LengthResult{
      .success = true, .error_message = std::string(), .length = 123};

  RunResult run_result = Run("test-operation", /*serialized_data=*/{});

  EXPECT_TRUE(run_result.success);
  EXPECT_TRUE(run_result.error_message.empty());

  EXPECT_EQ(test_client_->observed_length_count_, 1u);

  EXPECT_EQ(test_client_->observed_console_log_messages_.size(), 1u);
  EXPECT_EQ(test_client_->observed_console_log_messages_[0], "123");
}

TEST_F(SharedStorageWorkletTest, RemainingBudget_ClientError) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          let a = await sharedStorage.remainingBudget();
          console.log(a);
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  test_client_->remaining_budget_result_ = RemainingBudgetResult{
      .success = false, .error_message = "error 123", .bits = 0};

  RunResult run_result = Run("test-operation", /*serialized_data=*/{});

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(run_result.error_message, testing::HasSubstr("error 123"));

  EXPECT_EQ(test_client_->observed_remaining_budget_count_, 1u);

  EXPECT_EQ(test_client_->observed_console_log_messages_.size(), 0u);
}

TEST_F(SharedStorageWorkletTest, RemainingBudget_Success) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          let a = await sharedStorage.remainingBudget();
          console.log(a);
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  test_client_->remaining_budget_result_ = RemainingBudgetResult{
      .success = true, .error_message = std::string(), .bits = 2.0};

  RunResult run_result = Run("test-operation", /*serialized_data=*/{});

  EXPECT_TRUE(run_result.success);
  EXPECT_TRUE(run_result.error_message.empty());

  EXPECT_EQ(test_client_->observed_remaining_budget_count_, 1u);

  EXPECT_EQ(test_client_->observed_console_log_messages_.size(), 1u);
  EXPECT_EQ(test_client_->observed_console_log_messages_[0], "2");
}

TEST_F(SharedStorageWorkletTest, ContextAttribute_Undefined) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          console.log(sharedStorage.context);
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", /*serialized_data=*/{});

  EXPECT_TRUE(run_result.success);
  EXPECT_TRUE(run_result.error_message.empty());

  EXPECT_EQ(test_client_->observed_console_log_messages_.size(), 1u);
  EXPECT_EQ(test_client_->observed_console_log_messages_[0], "undefined");

  histogram_tester_.ExpectUniqueSample(
      "Storage.SharedStorage.Worklet.Context.IsDefined", /*sample=*/false,
      /*expected_bucket_count=*/1);
}

TEST_F(SharedStorageWorkletTest, ContextAttribute_String) {
  embedder_context_ = u"some embedder context";

  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          console.log(sharedStorage.context);
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", /*serialized_data=*/{});

  EXPECT_TRUE(run_result.success);
  EXPECT_TRUE(run_result.error_message.empty());

  EXPECT_EQ(test_client_->observed_console_log_messages_.size(), 1u);
  EXPECT_EQ(test_client_->observed_console_log_messages_[0],
            "some embedder context");

  histogram_tester_.ExpectUniqueSample(
      "Storage.SharedStorage.Worklet.Context.IsDefined", /*sample=*/true,
      /*expected_bucket_count=*/1);
}

// Test that methods on sharedStorage are resolved asynchronously, e.g. param
// validation failures won't affect the result of run().
TEST_F(SharedStorageWorkletTest,
       AsyncFailuresDuringOperation_OperationSucceed) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          sharedStorage.set();
          sharedStorage.append();
          sharedStorage.delete();
          sharedStorage.get();
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", /*serialized_data=*/{});

  EXPECT_TRUE(run_result.success);
  EXPECT_TRUE(run_result.error_message.empty());
}

}  // namespace blink
