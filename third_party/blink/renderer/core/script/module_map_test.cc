// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/module_map.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_fetch_request.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_fetcher.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_loader_client.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/core/script/module_record_resolver.h"
#include "third_party/blink/renderer/core/script/module_script.h"
#include "third_party/blink/renderer/core/testing/dummy_modulator.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"

namespace blink {

namespace {

class TestSingleModuleClient final : public SingleModuleClient {
 public:
  TestSingleModuleClient() = default;
  ~TestSingleModuleClient() override {}

  void Trace(Visitor* visitor) override {
    visitor->Trace(module_script_);
    SingleModuleClient::Trace(visitor);
  }

  void NotifyModuleLoadFinished(ModuleScript* module_script) override {
    was_notify_finished_ = true;
    module_script_ = module_script;
  }

  bool WasNotifyFinished() const { return was_notify_finished_; }
  ModuleScript* GetModuleScript() { return module_script_; }

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
    NOTREACHED();
    return nullptr;
  }

  v8::Local<v8::Module> Resolve(const String& specifier,
                                v8::Local<v8::Module> referrer,
                                ExceptionState&) override {
    NOTREACHED();
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

  void Trace(Visitor*) override;

  TestModuleRecordResolver* GetTestModuleRecordResolver() {
    return resolver_.Get();
  }
  void ResolveFetches();

 private:
  // Implements Modulator:
  ModuleRecordResolver* GetModuleRecordResolver() override {
    return resolver_.Get();
  }
  ScriptState* GetScriptState() override { return script_state_; }

  class TestModuleScriptFetcher final
      : public GarbageCollected<TestModuleScriptFetcher>,
        public ModuleScriptFetcher {
    USING_GARBAGE_COLLECTED_MIXIN(TestModuleScriptFetcher);

   public:
    explicit TestModuleScriptFetcher(ModuleMapTestModulator* modulator)
        : modulator_(modulator) {}
    void Fetch(FetchParameters& request,
               ResourceFetcher*,
               const Modulator* modulator_for_built_in_modules,
               ModuleGraphLevel,
               ModuleScriptFetcher::Client* client) override {
      TestRequest* test_request = MakeGarbageCollected<TestRequest>(
          ModuleScriptCreationParams(
              request.Url(),
              ModuleScriptCreationParams::ModuleType::kJavaScriptModule,
              ParkableString(String("").ReleaseImpl()), nullptr,
              request.GetResourceRequest().GetCredentialsMode()),
          client);
      modulator_->test_requests_.push_back(test_request);
    }
    String DebugName() const override { return "TestModuleScriptFetcher"; }
    void Trace(Visitor* visitor) override {
      ModuleScriptFetcher::Trace(visitor);
      visitor->Trace(modulator_);
    }

   private:
    Member<ModuleMapTestModulator> modulator_;
  };

  ModuleScriptFetcher* CreateModuleScriptFetcher(
      ModuleScriptCustomFetchType) override {
    return MakeGarbageCollected<TestModuleScriptFetcher>(this);
  }

  Vector<ModuleRequest> ModuleRequestsFromModuleRecord(
      v8::Local<v8::Module>) override {
    return Vector<ModuleRequest>();
  }

  base::SingleThreadTaskRunner* TaskRunner() override {
    return Thread::Current()->GetTaskRunner().get();
  }

  struct TestRequest final : public GarbageCollected<TestRequest> {
    TestRequest(const ModuleScriptCreationParams& params,
                ModuleScriptFetcher::Client* client)
        : params_(params), client_(client) {}
    void NotifyFetchFinished() {
      client_->NotifyFetchFinished(*params_,
                                   HeapVector<Member<ConsoleMessage>>());
    }
    void Trace(Visitor* visitor) { visitor->Trace(client_); }

   private:
    base::Optional<ModuleScriptCreationParams> params_;
    Member<ModuleScriptFetcher::Client> client_;
  };
  HeapVector<Member<TestRequest>> test_requests_;

  Member<ScriptState> script_state_;
  Member<TestModuleRecordResolver> resolver_;
};

ModuleMapTestModulator::ModuleMapTestModulator(ScriptState* script_state)
    : script_state_(script_state),
      resolver_(MakeGarbageCollected<TestModuleRecordResolver>()) {}

void ModuleMapTestModulator::Trace(Visitor* visitor) {
  visitor->Trace(test_requests_);
  visitor->Trace(script_state_);
  visitor->Trace(resolver_);
  DummyModulator::Trace(visitor);
}

void ModuleMapTestModulator::ResolveFetches() {
  for (const auto& test_request : test_requests_) {
    TaskRunner()->PostTask(FROM_HERE,
                           WTF::Bind(&TestRequest::NotifyFetchFinished,
                                     WrapPersistent(test_request.Get())));
  }
  test_requests_.clear();
}

class ModuleMapTest : public PageTestBase {
 public:
  void SetUp() override;

  ModuleMapTestModulator* Modulator() { return modulator_.Get(); }
  ModuleMap* Map() { return map_; }

 protected:
  Persistent<ModuleMapTestModulator> modulator_;
  Persistent<ModuleMap> map_;
};

void ModuleMapTest::SetUp() {
  PageTestBase::SetUp(IntSize(500, 500));
  NavigateTo(KURL("https://example.com"));
  modulator_ = MakeGarbageCollected<ModuleMapTestModulator>(
      ToScriptStateForMainWorld(&GetFrame()));
  map_ = MakeGarbageCollected<ModuleMap>(modulator_);
}

TEST_F(ModuleMapTest, sequentialRequests) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform;
  platform->AdvanceClockSeconds(1.);  // For non-zero DocumentParserTimings

  KURL url(NullURL(), "https://example.com/foo.js");

  // First request
  TestSingleModuleClient* client =
      MakeGarbageCollected<TestSingleModuleClient>();
  Map()->FetchSingleModuleScript(ModuleScriptFetchRequest::CreateForTest(url),
                                 GetDocument().Fetcher(),
                                 ModuleGraphLevel::kTopLevelModuleFetch,
                                 ModuleScriptCustomFetchType::kNone, client);
  Modulator()->ResolveFetches();
  EXPECT_FALSE(client->WasNotifyFinished())
      << "fetchSingleModuleScript shouldn't complete synchronously";
  platform->RunUntilIdle();

  EXPECT_EQ(Modulator()
                ->GetTestModuleRecordResolver()
                ->RegisterModuleScriptCallCount(),
            1);
  EXPECT_TRUE(client->WasNotifyFinished());
  EXPECT_TRUE(client->GetModuleScript());

  // Secondary request
  TestSingleModuleClient* client2 =
      MakeGarbageCollected<TestSingleModuleClient>();
  Map()->FetchSingleModuleScript(ModuleScriptFetchRequest::CreateForTest(url),
                                 GetDocument().Fetcher(),
                                 ModuleGraphLevel::kTopLevelModuleFetch,
                                 ModuleScriptCustomFetchType::kNone, client2);
  Modulator()->ResolveFetches();
  EXPECT_FALSE(client2->WasNotifyFinished())
      << "fetchSingleModuleScript shouldn't complete synchronously";
  platform->RunUntilIdle();

  EXPECT_EQ(Modulator()
                ->GetTestModuleRecordResolver()
                ->RegisterModuleScriptCallCount(),
            1)
      << "registerModuleScript sholudn't be called in secondary request.";
  EXPECT_TRUE(client2->WasNotifyFinished());
  EXPECT_TRUE(client2->GetModuleScript());
}

TEST_F(ModuleMapTest, concurrentRequestsShouldJoin) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform;
  platform->AdvanceClockSeconds(1.);  // For non-zero DocumentParserTimings

  KURL url(NullURL(), "https://example.com/foo.js");

  // First request
  TestSingleModuleClient* client =
      MakeGarbageCollected<TestSingleModuleClient>();
  Map()->FetchSingleModuleScript(ModuleScriptFetchRequest::CreateForTest(url),
                                 GetDocument().Fetcher(),
                                 ModuleGraphLevel::kTopLevelModuleFetch,
                                 ModuleScriptCustomFetchType::kNone, client);

  // Secondary request (which should join the first request)
  TestSingleModuleClient* client2 =
      MakeGarbageCollected<TestSingleModuleClient>();
  Map()->FetchSingleModuleScript(ModuleScriptFetchRequest::CreateForTest(url),
                                 GetDocument().Fetcher(),
                                 ModuleGraphLevel::kTopLevelModuleFetch,
                                 ModuleScriptCustomFetchType::kNone, client2);

  Modulator()->ResolveFetches();
  EXPECT_FALSE(client->WasNotifyFinished())
      << "fetchSingleModuleScript shouldn't complete synchronously";
  EXPECT_FALSE(client2->WasNotifyFinished())
      << "fetchSingleModuleScript shouldn't complete synchronously";
  platform->RunUntilIdle();

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
