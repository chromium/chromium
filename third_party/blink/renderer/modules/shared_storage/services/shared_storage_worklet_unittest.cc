// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shared_storage/services/shared_storage_worklet_messaging_proxy.h"

#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "gin/array_buffer.h"
#include "gin/dictionary.h"
#include "gin/public/isolate_holder.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage_worklet_service.mojom.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "v8/include/v8-isolate.h"

namespace blink {

namespace {

constexpr char kModuleScriptSource[] = "https://foo.com/module_script.js";

struct AddModuleResult {
  bool success = false;
  std::string error_message;
};

struct SelectURLResult {
  bool success = false;
  std::string error_message;
  uint32_t index = 0;
};

struct RunResult {
  bool success = false;
  std::string error_message;
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
    NOTREACHED();
  }

  void SharedStorageAppend(const std::u16string& key,
                           const std::u16string& value,
                           SharedStorageAppendCallback callback) override {
    NOTREACHED();
  }

  void SharedStorageDelete(const std::u16string& key,
                           SharedStorageDeleteCallback callback) override {
    NOTREACHED();
  }

  void SharedStorageClear(SharedStorageClearCallback callback) override {
    NOTREACHED();
  }

  void SharedStorageGet(const std::u16string& key,
                        SharedStorageGetCallback callback) override {
    NOTREACHED();
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
    NOTREACHED();
  }

  void SharedStorageRemainingBudget(
      SharedStorageRemainingBudgetCallback callback) override {
    NOTREACHED();
  }

  void ConsoleLog(const std::string& message) override {
    observed_console_log_messages_.push_back(message);
  }

  void RecordUseCounters(
      const std::vector<blink::mojom::WebFeature>& features) override {
    NOTREACHED();
  }

  const std::vector<std::string>& observed_console_log_messages() const {
    return observed_console_log_messages_;
  }

 private:
  mojo::AssociatedReceiver<blink::mojom::SharedStorageWorkletServiceClient>
      receiver_{this};

  std::vector<std::string> observed_console_log_messages_;
};

}  // namespace

class SharedStorageWorkletTest : public testing::Test {
 public:
  SharedStorageWorkletTest() = default;

  void SetUp() override {
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

    absl::optional<std::u16string> embedder_context;

    shared_storage_worklet_service_->Initialize(
        std::move(pending_remote),
        /*private_aggregation_permissions_policy_allowed=*/true,
        mojo::PendingRemote<mojom::PrivateAggregationHost>(), embedder_context);
  }

  AddModuleResult AddModule(const std::string& script_content,
                            std::string mime_type = "application/javascript") {
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
    base::test::TestFuture<bool, const std::string&, uint32_t> future;
    shared_storage_worklet_service_->RunURLSelectionOperation(
        name, urls, serialized_data, future.GetCallback());

    return {future.Get<0>(), future.Get<1>(), future.Get<2>()};
  }

  RunResult Run(const std::string& name,
                const std::vector<uint8_t>& serialized_data) {
    base::test::TestFuture<bool, const std::string&> future;
    shared_storage_worklet_service_->RunOperation(name, serialized_data,
                                                  future.GetCallback());

    return {future.Get<0>(), future.Get<1>()};
  }

 protected:
  mojo::Remote<mojom::SharedStorageWorkletService>
      shared_storage_worklet_service_;

  Persistent<SharedStorageWorkletMessagingProxy> messaging_proxy_;

  base::test::TestFuture<void> worklet_terminated_future_;

  std::unique_ptr<TestClient> test_client_;
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

  EXPECT_EQ(test_client_->observed_console_log_messages().size(), 1u);
  EXPECT_EQ(test_client_->observed_console_log_messages()[0], "123 abc");
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

  EXPECT_EQ(test_client_->observed_console_log_messages().size(), 2u);
  EXPECT_EQ(test_client_->observed_console_log_messages()[0],
            "[\"https://foo0.com/\",\"https://foo1.com/\"]");
  EXPECT_EQ(test_client_->observed_console_log_messages()[1],
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

  EXPECT_EQ(test_client_->observed_console_log_messages().size(), 1u);
  EXPECT_EQ(test_client_->observed_console_log_messages()[0],
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

// TODO(yaoxia): Add test scenarios where the operation finishes asynchronously.
// This will be doable when some async operations are supported in the worklet
// (e.g. sharedStorage methods).

}  // namespace blink
