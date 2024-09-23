// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/module_map.h"

#include "base/task/single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_location_type.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_fetch_request.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_fetcher.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_loader_client.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/core/script/module_record_resolver.h"
#include "third_party/blink/renderer/core/script/module_script.h"
#include "third_party/blink/renderer/core/testing/dummy_modulator.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/module_test_base.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

namespace {

class TestSingleModuleClient final : public SingleModuleClient {
 public:
  TestSingleModuleClient() = default;
  ~TestSingleModuleClient() override {}

  void Trace(Visitor* visitor) const override {
    visitor->Trace(module_script_);
    SingleModuleClient::Trace(visitor);
  }

  void NotifyModuleLoadFinished(ModuleScript* module_script) override {
    was_notify_finished_ = true;
    module_script_ = module_script;
  }

  bool WasNotifyFinished() const { return was_notify_finished_; }
  ModuleScript* GetModuleScript() { return module_script_.Get(); }

 private:
  bool was_notify_finished_ = false;
  Member<ModuleScript> module_script_;
};

class TestModuleRecordResolver final : public ModuleRecordResolver {
 public:
  TestModuleRecordResolver() {}

  int RegisterModuleScriptCallCount() const {
    return register_module_script_call_count_;
  }

  void RegisterModuleScript(const ModuleScript*) override {
    register_module_script_call_count_++;
  }

  void UnregisterModuleScript(const ModuleScript*) override {
    FAIL() << "UnregisterModuleScript shouldn't be called in ModuleMapTest";
  }

  const ModuleScript* GetModuleScriptFromModuleRecord(
      v8::Local<v8::Module>) const override {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }

  v8::Local<v8::Module> Resolve(const ModuleRequest& module_request,
                                v8::Local<v8::Module> referrer,
                                ExceptionState&) override {
    NOTREACHED_IN_MIGRATION();
    return v8::Local<v8::Module>();
  }

 private:
  int register_module_script_call_count_ = 0;
};

}  // namespace

class ModuleMapTestModulator final : public DummyModulator {
 public:
  explicit ModuleMapTestModulator(ScriptState*);
  ~ModuleMapTestModulator() override {}

  void Trace(Visitor*) const override;

  TestModuleRecordResolver* GetTestModuleRecordResolver() {
    return resolver_.Get();
  }
  void ResolveFetches();

 private:
  // Implements Modulator:
  ModuleRecordResolver* GetModuleRecordResolver() override {
    return resolver_.Get();
  }
  ScriptState* GetScriptState() override { return script_state_.Get(); }

  class TestModuleScriptFetcher final
      : public GarbageCollected<TestModuleScriptFetcher>,
        public ModuleScriptFetcher {
   public:
    TestModuleScriptFetcher(ModuleMapTestModulator* modulator,
                            base::PassKey<ModuleScriptLoader> pass_key)
        : ModuleScriptFetcher(pass_key), modulator_(modulator) {}
    void Fetch(FetchParameters& request,
               ModuleType module_type,
               ResourceFetcher*,
               ModuleGraphLevel,
               ModuleScriptFetcher::Client* client) override {
      CHECK_EQ(request.GetScriptType(), mojom::blink::ScriptType::kModule);
      TestRequest* test_request =
          MakeGarbageCollected<TestRequest>(request.Url(), client);
      modulator_->test_requests_.push_back(test_request);
    }
    String DebugName() const override { return "TestModuleScriptFetcher"; }
    void Trace(Visitor* visitor) const override {
      ModuleScriptFetcher::Trace(visitor);
      visitor->Trace(modulator_);
    }

   private:
    Member<ModuleMapTestModulator> modulator_;
  };

  ModuleScriptFetcher* CreateModuleScriptFetcher(
      ModuleScriptCustomFetchType,
      base::PassKey<ModuleScriptLoader> pass_key) override {
    return MakeGarbageCollected<TestModuleScriptFetcher>(this, pass_key);
  }

  base::SingleThreadTaskRunner* TaskRunner() override {
    return task_runner_.get();
  }

  struct TestRequest final : public GarbageCollected<TestRequest> {
    TestRequest(const KURL& url, ModuleScriptFetcher::Client* client)
        : url_(url), client_(client) {}
    void NotifyFetchFinished() {
      client_->NotifyFetchFinishedSuccess(ModuleScriptCreationParams(
          url_, url_, ScriptSourceLocationType::kExternalFile,
          ModuleType::kJavaScript, ParkableString(String("").ReleaseImpl()),
          nullptr, network::mojom::ReferrerPolicy::kDefault));
    }
    void Trace(Visitor* visitor) const { visitor->Trace(client_); }

   private:
    const KURL url_;
    Member<ModuleScriptFetcher::Client> client_;
  };
  HeapVector<Member<TestRequest>> test_requests_;

  Member<ScriptState> script_state_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  Member<TestModuleRecordResolver> resolver_;
};

ModuleMapTestModulator::ModuleMapTestModulator(ScriptState* script_state)
    : script_state_(script_state),
      task_runner_(ExecutionContext::From(script_state_)
                       ->GetTaskRunner(TaskType::kNetworking)),
      resolver_(MakeGarbageCollected<TestModuleRecordResolver>()) {}

void ModuleMapTestModulator::Trace(Visitor* visitor) const {
  visitor->Trace(test_requests_);
  visitor->Trace(script_state_);
  visitor->Trace(resolver_);
  DummyModulator::Trace(visitor);
}

void ModuleMapTestModulator::ResolveFetches() {
  for (const auto& test_request : test_requests_) {
    TaskRunner()->PostTask(FROM_HERE,
                           WTF::BindOnce(&TestRequest::NotifyFetchFinished,
                                         WrapPersistent(test_request.Get())));
  }
  test_requests_.clear();
}

class ModuleMapTest : public PageTestBase, public ModuleTestBase {
 public:
  void SetUp() override;
  void TearDown() override;

  ModuleMapTestModulator* Modulator() { return modulator_.Get(); }
  ModuleMap* Map() { return map_; }

 protected:
  Persistent<ModuleMapTestModulator> modulator_;
  Persistent<ModuleMap> map_;
};

void ModuleMapTest::SetUp() {
  ModuleTestBase::SetUp();
  PageTestBase::SetUp(gfx::Size(500, 500));
  NavigateTo(KURL("https://example.com"));
  modulator_ = MakeGarbageCollected<ModuleMapTestModulator>(
      ToScriptStateForMainWorld(&GetFrame()));
  map_ = MakeGarbageCollected<ModuleMap>(modulator_);
}

void ModuleMapTest::TearDown() {
  ModuleTestBase::TearDown();
  PageTestBase::TearDown();
}

TEST_F(ModuleMapTest, sequentialRequests) {
  KURL url(NullURL(), "https://example.com/foo.js");

  // First request
  TestSingleModuleClient* client =
      MakeGarbageCollected<TestSingleModuleClient>();
  Map()->FetchSingleModuleScript(
      ModuleScriptFetchRequest::CreateForTest(url, ModuleType::kJavaScript),
      GetDocument().Fetcher(), ModuleGraphLevel::kTopLevelModuleFetch,
      ModuleScriptCustomFetchType::kNone, client);
  Modulator()->ResolveFetches();
  EXPECT_FALSE(client->WasNotifyFinished())
      << "fetchSingleModuleScript shouldn't complete synchronously";
  test::RunPendingTasks();

  EXPECT_EQ(Modulator()
                ->GetTestModuleRecordResolver()
                ->RegisterModuleScriptCallCount(),
            1);
  EXPECT_TRUE(client->WasNotifyFinished());
  EXPECT_TRUE(client->GetModuleScript());

  // Secondary request
  TestSingleModuleClient* client2 =
      MakeGarbageCollected<TestSingleModuleClient>();
  Map()->FetchSingleModuleScript(
      ModuleScriptFetchRequest::CreateForTest(url, ModuleType::kJavaScript),
      GetDocument().Fetcher(), ModuleGraphLevel::kTopLevelModuleFetch,
      ModuleScriptCustomFetchType::kNone, client2);
  Modulator()->ResolveFetches();
  EXPECT_FALSE(client2->WasNotifyFinished())
      << "fetchSingleModuleScript shouldn't complete synchronously";
  test::RunPendingTasks();

  EXPECT_EQ(Modulator()
                ->GetTestModuleRecordResolver()
                ->RegisterModuleScriptCallCount(),
            1)
      << "registerModuleScript sholudn't be called in secondary request.";
  EXPECT_TRUE(client2->WasNotifyFinished());
  EXPECT_TRUE(client2->GetModuleScript());
}

TEST_F(ModuleMapTest, concurrentRequestsShouldJoin) {
  KURL url(NullURL(), "https://example.com/foo.js");

  // First request
  TestSingleModuleClient* client =
      MakeGarbageCollected<TestSingleModuleClient>();
  Map()->FetchSingleModuleScript(
      ModuleScriptFetchRequest::CreateForTest(url, ModuleType::kJavaScript),
      GetDocument().Fetcher(), ModuleGraphLevel::kTopLevelModuleFetch,
      ModuleScriptCustomFetchType::kNone, client);

  // Secondary request (which should join the first request)
  TestSingleModuleClient* client2 =
      MakeGarbageCollected<TestSingleModuleClient>();
  Map()->FetchSingleModuleScript(
      ModuleScriptFetchRequest::CreateForTest(url, ModuleType::kJavaScript),
      GetDocument().Fetcher(), ModuleGraphLevel::kTopLevelModuleFetch,
      ModuleScriptCustomFetchType::kNone, client2);

  Modulator()->ResolveFetches();
  EXPECT_FALSE(client->WasNotifyFinished())
      << "fetchSingleModuleScript shouldn't complete synchronously";
  EXPECT_FALSE(client2->WasNotifyFinished())
      << "fetchSingleModuleScript shouldn't complete synchronously";
  test::RunPendingTasks();

  EXPECT_EQ(Modulator()
                ->GetTestModuleRecordResolver()
                ->RegisterModuleScriptCallCount(),
            1);

  EXPECT_TRUE(client->WasNotifyFinished());
  EXPECT_TRUE(client->GetModuleScript());
  EXPECT_TRUE(client2->WasNotifyFinished());
  EXPECT_TRUE(client2->GetModuleScript());
}

}  // namespace blink
