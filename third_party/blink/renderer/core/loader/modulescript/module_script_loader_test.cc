// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/modulescript/module_script_loader.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/loader/modulescript/document_module_script_fetcher.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_fetch_request.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_loader_client.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_loader_registry.h"
#include "third_party/blink/renderer/core/loader/modulescript/worklet_module_script_fetcher.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/core/script/module_script.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/core/testing/dummy_modulator.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/main_thread_worklet_reporting_proxy.h"
#include "third_party/blink/renderer/core/workers/worklet_global_scope.h"
#include "third_party/blink/renderer/core/workers/worklet_module_responses_map.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/testing/fetch_testing_platform_support.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_fetch_context.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

namespace {

class TestModuleScriptLoaderClient final
    : public GarbageCollectedFinalized<TestModuleScriptLoaderClient>,
      public ModuleScriptLoaderClient {
  USING_GARBAGE_COLLECTED_MIXIN(TestModuleScriptLoaderClient);

 public:
  TestModuleScriptLoaderClient() = default;
  ~TestModuleScriptLoaderClient() override = default;

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(module_script_);
  }

  void NotifyNewSingleModuleFinished(ModuleScript* module_script) override {
    was_notify_finished_ = true;
    module_script_ = module_script;
  }

  bool WasNotifyFinished() const { return was_notify_finished_; }
  ModuleScript* GetModuleScript() { return module_script_; }

 private:
  bool was_notify_finished_ = false;
  Member<ModuleScript> module_script_;
};

class ModuleScriptLoaderTestModulator final : public DummyModulator {
 public:
  ModuleScriptLoaderTestModulator(
      ScriptState* script_state,
      scoped_refptr<const SecurityOrigin> security_origin,
      ResourceFetcher* fetcher)
      : script_state_(script_state),
        security_origin_(std::move(security_origin)),
        fetcher_(fetcher) {}

  ~ModuleScriptLoaderTestModulator() override = default;

  KURL ResolveModuleSpecifier(const String& module_request,
                              const KURL& base_url,
                              String* failure_reason) final {
    return KURL(base_url, module_request);
  }

  ScriptState* GetScriptState() override { return script_state_; }

  void SetModuleRequests(const Vector<String>& requests) {
    requests_.clear();
    for (const String& request : requests) {
      requests_.emplace_back(request, TextPosition::MinimumPosition());
    }
  }
  Vector<ModuleRequest> ModuleRequestsFromScriptModule(ScriptModule) override {
    return requests_;
  }

  ModuleScriptFetcher* CreateModuleScriptFetcher(
      ModuleScriptCustomFetchType custom_fetch_type) override {
    auto* execution_context = ExecutionContext::From(script_state_);
    if (auto* scope = DynamicTo<WorkletGlobalScope>(execution_context)) {
      EXPECT_EQ(ModuleScriptCustomFetchType::kWorkletAddModule,
                custom_fetch_type);
      return new WorkletModuleScriptFetcher(Fetcher(),
                                            scope->GetModuleResponsesMap());
    }
    EXPECT_EQ(ModuleScriptCustomFetchType::kNone, custom_fetch_type);
    return new DocumentModuleScriptFetcher(Fetcher());
  }

  ResourceFetcher* Fetcher() const { return fetcher_.Get(); }

  void Trace(blink::Visitor*) override;

 private:
  Member<ScriptState> script_state_;
  scoped_refptr<const SecurityOrigin> security_origin_;
  Member<ResourceFetcher> fetcher_;
  Vector<ModuleRequest> requests_;
};

void ModuleScriptLoaderTestModulator::Trace(blink::Visitor* visitor) {
  visitor->Trace(fetcher_);
  visitor->Trace(script_state_);
  DummyModulator::Trace(visitor);
}

}  // namespace

class ModuleScriptLoaderTest : public PageTestBase {
  DISALLOW_COPY_AND_ASSIGN(ModuleScriptLoaderTest);

 public:
  ModuleScriptLoaderTest() = default;
  void SetUp() override;

  void InitializeForDocument();
  void InitializeForWorklet();

  void TestFetchDataURL(ModuleScriptCustomFetchType,
                        TestModuleScriptLoaderClient*);
  void TestInvalidSpecifier(ModuleScriptCustomFetchType,
                            TestModuleScriptLoaderClient*);
  void TestFetchInvalidURL(ModuleScriptCustomFetchType,
                           TestModuleScriptLoaderClient*);
  void TestFetchURL(ModuleScriptCustomFetchType, TestModuleScriptLoaderClient*);

  ModuleScriptLoaderTestModulator* GetModulator() { return modulator_.Get(); }

  void RunUntilIdle() {
    base::SingleThreadTaskRunner* runner =
        GetModulator()->Fetcher()->Context().GetLoadingTaskRunner().get();
    static_cast<scheduler::FakeTaskRunner*>(runner)->RunUntilIdle();
  }

 protected:
  ScopedTestingPlatformSupport<FetchTestingPlatformSupport> platform_;
  std::unique_ptr<MainThreadWorkletReportingProxy> reporting_proxy_;
  Persistent<ModuleScriptLoaderTestModulator> modulator_;
  Persistent<WorkletGlobalScope> global_scope_;
};

void ModuleScriptLoaderTest::SetUp() {
  platform_->AdvanceClockSeconds(1.);  // For non-zero DocumentParserTimings
  PageTestBase::SetUp(IntSize(500, 500));
  GetDocument().SetURL(KURL("https://example.test"));
  GetDocument().SetSecurityOrigin(SecurityOrigin::Create(GetDocument().Url()));
}

void ModuleScriptLoaderTest::InitializeForDocument() {
  auto* fetch_context =
      MockFetchContext::Create(MockFetchContext::kShouldLoadNewResource);
  auto* fetcher = ResourceFetcher::Create(fetch_context);
  modulator_ = new ModuleScriptLoaderTestModulator(
      ToScriptStateForMainWorld(&GetFrame()), GetDocument().GetSecurityOrigin(),
      fetcher);
}

void ModuleScriptLoaderTest::InitializeForWorklet() {
  auto* fetch_context =
      MockFetchContext::Create(MockFetchContext::kShouldLoadNewResource);
  auto* fetcher = ResourceFetcher::Create(fetch_context);
  reporting_proxy_ =
      std::make_unique<MainThreadWorkletReportingProxy>(&GetDocument());
  auto creation_params = std::make_unique<GlobalScopeCreationParams>(
      GetDocument().Url(), ScriptType::kModule, GetDocument().UserAgent(),
      Vector<CSPHeaderAndType>(), GetDocument().GetReferrerPolicy(),
      GetDocument().GetSecurityOrigin(), GetDocument().IsSecureContext(),
      GetDocument().GetHttpsState(), nullptr /* worker_clients */,
      GetDocument().AddressSpace(),
      OriginTrialContext::GetTokens(&GetDocument()).get(),
      base::UnguessableToken::Create(), nullptr /* worker_settings */,
      kV8CacheOptionsDefault, new WorkletModuleResponsesMap);
  global_scope_ = new WorkletGlobalScope(std::move(creation_params),
                                         *reporting_proxy_, &GetFrame());
  global_scope_->ScriptController()->InitializeContextIfNeeded("Dummy Context",
                                                               NullURL());
  modulator_ = new ModuleScriptLoaderTestModulator(
      global_scope_->ScriptController()->GetScriptState(),
      GetDocument().GetSecurityOrigin(), fetcher);
}

void ModuleScriptLoaderTest::TestFetchDataURL(
    ModuleScriptCustomFetchType custom_fetch_type,
    TestModuleScriptLoaderClient* client) {
  ModuleScriptLoaderRegistry* registry = ModuleScriptLoaderRegistry::Create();
  KURL url("data:text/javascript,export default 'grapes';");
  auto* fetch_client_settings_object =
      GetDocument().CreateFetchClientSettingsObjectSnapshot();
  ModuleScriptLoader::Fetch(
      ModuleScriptFetchRequest::CreateForTest(url),
      fetch_client_settings_object, ModuleGraphLevel::kTopLevelModuleFetch,
      GetModulator(), custom_fetch_type, registry, client);
}

TEST_F(ModuleScriptLoaderTest, FetchDataURL) {
  InitializeForDocument();
  TestModuleScriptLoaderClient* client = new TestModuleScriptLoaderClient;
  TestFetchDataURL(ModuleScriptCustomFetchType::kNone, client);

  EXPECT_TRUE(client->WasNotifyFinished())
      << "ModuleScriptLoader should finish synchronously.";
  ASSERT_TRUE(client->GetModuleScript());
  EXPECT_FALSE(client->GetModuleScript()->HasEmptyRecord());
  EXPECT_FALSE(client->GetModuleScript()->HasParseError());
}

TEST_F(ModuleScriptLoaderTest, FetchDataURL_OnWorklet) {
  InitializeForWorklet();
  TestModuleScriptLoaderClient* client1 = new TestModuleScriptLoaderClient;
  TestFetchDataURL(ModuleScriptCustomFetchType::kWorkletAddModule, client1);

  EXPECT_FALSE(client1->WasNotifyFinished())
      << "ModuleScriptLoader should finish asynchronously.";
  RunUntilIdle();

  EXPECT_TRUE(client1->WasNotifyFinished());
  ASSERT_TRUE(client1->GetModuleScript());
  EXPECT_FALSE(client1->GetModuleScript()->HasEmptyRecord());
  EXPECT_FALSE(client1->GetModuleScript()->HasParseError());

  // Try to fetch the same URL again in order to verify the case where
  // WorkletModuleResponsesMap serves a cache.
  TestModuleScriptLoaderClient* client2 = new TestModuleScriptLoaderClient;
  TestFetchDataURL(ModuleScriptCustomFetchType::kWorkletAddModule, client2);

  EXPECT_FALSE(client2->WasNotifyFinished())
      << "ModuleScriptLoader should finish asynchronously.";
  RunUntilIdle();

  EXPECT_TRUE(client2->WasNotifyFinished());
  ASSERT_TRUE(client2->GetModuleScript());
  EXPECT_FALSE(client2->GetModuleScript()->HasEmptyRecord());
  EXPECT_FALSE(client2->GetModuleScript()->HasParseError());
}

void ModuleScriptLoaderTest::TestInvalidSpecifier(
    ModuleScriptCustomFetchType custom_fetch_type,
    TestModuleScriptLoaderClient* client) {
  ModuleScriptLoaderRegistry* registry = ModuleScriptLoaderRegistry::Create();
  KURL url("data:text/javascript,import 'invalid';export default 'grapes';");
  auto* fetch_client_settings_object =
      GetDocument().CreateFetchClientSettingsObjectSnapshot();
  GetModulator()->SetModuleRequests({"invalid"});
  ModuleScriptLoader::Fetch(
      ModuleScriptFetchRequest::CreateForTest(url),
      fetch_client_settings_object, ModuleGraphLevel::kTopLevelModuleFetch,
      GetModulator(), custom_fetch_type, registry, client);
}

TEST_F(ModuleScriptLoaderTest, InvalidSpecifier) {
  InitializeForDocument();
  TestModuleScriptLoaderClient* client = new TestModuleScriptLoaderClient;
  TestInvalidSpecifier(ModuleScriptCustomFetchType::kNone, client);

  EXPECT_TRUE(client->WasNotifyFinished())
      << "ModuleScriptLoader should finish synchronously.";
  ASSERT_TRUE(client->GetModuleScript());
  EXPECT_TRUE(client->GetModuleScript()->HasEmptyRecord());
  EXPECT_TRUE(client->GetModuleScript()->HasParseError());
}

TEST_F(ModuleScriptLoaderTest, InvalidSpecifier_OnWorklet) {
  InitializeForWorklet();
  TestModuleScriptLoaderClient* client = new TestModuleScriptLoaderClient;
  TestInvalidSpecifier(ModuleScriptCustomFetchType::kWorkletAddModule, client);

  EXPECT_FALSE(client->WasNotifyFinished())
      << "ModuleScriptLoader should finish asynchronously.";
  RunUntilIdle();

  EXPECT_TRUE(client->WasNotifyFinished());
  ASSERT_TRUE(client->GetModuleScript());
  EXPECT_TRUE(client->GetModuleScript()->HasEmptyRecord());
  EXPECT_TRUE(client->GetModuleScript()->HasParseError());
}

void ModuleScriptLoaderTest::TestFetchInvalidURL(
    ModuleScriptCustomFetchType custom_fetch_type,
    TestModuleScriptLoaderClient* client) {
  ModuleScriptLoaderRegistry* registry = ModuleScriptLoaderRegistry::Create();
  KURL url;
  EXPECT_FALSE(url.IsValid());
  auto* fetch_client_settings_object =
      GetDocument().CreateFetchClientSettingsObjectSnapshot();
  ModuleScriptLoader::Fetch(
      ModuleScriptFetchRequest::CreateForTest(url),
      fetch_client_settings_object, ModuleGraphLevel::kTopLevelModuleFetch,
      GetModulator(), custom_fetch_type, registry, client);
}

TEST_F(ModuleScriptLoaderTest, FetchInvalidURL) {
  InitializeForDocument();
  TestModuleScriptLoaderClient* client = new TestModuleScriptLoaderClient;
  TestFetchInvalidURL(ModuleScriptCustomFetchType::kNone, client);

  EXPECT_TRUE(client->WasNotifyFinished())
      << "ModuleScriptLoader should finish synchronously.";
  EXPECT_FALSE(client->GetModuleScript());
}

TEST_F(ModuleScriptLoaderTest, FetchInvalidURL_OnWorklet) {
  InitializeForWorklet();
  TestModuleScriptLoaderClient* client = new TestModuleScriptLoaderClient;
  TestFetchInvalidURL(ModuleScriptCustomFetchType::kWorkletAddModule, client);

  EXPECT_FALSE(client->WasNotifyFinished())
      << "ModuleScriptLoader should finish asynchronously.";
  RunUntilIdle();

  EXPECT_TRUE(client->WasNotifyFinished());
  EXPECT_FALSE(client->GetModuleScript());
}

void ModuleScriptLoaderTest::TestFetchURL(
    ModuleScriptCustomFetchType custom_fetch_type,
    TestModuleScriptLoaderClient* client) {
  KURL url("https://example.test/module.js");
  url_test_helpers::RegisterMockedURLLoad(
      url, test::CoreTestDataPath("module.js"), "text/javascript");
  auto* fetch_client_settings_object =
      GetDocument().CreateFetchClientSettingsObjectSnapshot();

  ModuleScriptLoaderRegistry* registry = ModuleScriptLoaderRegistry::Create();
  ModuleScriptLoader::Fetch(
      ModuleScriptFetchRequest::CreateForTest(url),
      fetch_client_settings_object, ModuleGraphLevel::kTopLevelModuleFetch,
      GetModulator(), custom_fetch_type, registry, client);
}

TEST_F(ModuleScriptLoaderTest, FetchURL) {
  InitializeForDocument();
  TestModuleScriptLoaderClient* client = new TestModuleScriptLoaderClient;
  TestFetchURL(ModuleScriptCustomFetchType::kNone, client);

  EXPECT_FALSE(client->WasNotifyFinished())
      << "ModuleScriptLoader unexpectedly finished synchronously.";
  platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();

  EXPECT_TRUE(client->WasNotifyFinished());
  EXPECT_TRUE(client->GetModuleScript());
}

TEST_F(ModuleScriptLoaderTest, FetchURL_OnWorklet) {
  InitializeForWorklet();
  TestModuleScriptLoaderClient* client = new TestModuleScriptLoaderClient;
  TestFetchURL(ModuleScriptCustomFetchType::kWorkletAddModule, client);

  EXPECT_FALSE(client->WasNotifyFinished())
      << "ModuleScriptLoader unexpectedly finished synchronously.";
  platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  RunUntilIdle();

  EXPECT_TRUE(client->WasNotifyFinished());
  EXPECT_TRUE(client->GetModuleScript());
}

}  // namespace blink
