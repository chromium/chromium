// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/modulescript/module_script_loader.h"

#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
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
#include "third_party/blink/renderer/core/workers/worker_thread_test_helper.h"
#include "third_party/blink/renderer/core/workers/worklet_global_scope.h"
#include "third_party/blink/renderer/core/workers/worklet_global_scope_test_helper.h"
#include "third_party/blink/renderer/core/workers/worklet_module_responses_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/testing/fetch_testing_platform_support.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_fetch_context.h"
#include "third_party/blink/renderer/platform/loader/testing/test_loader_factory.h"
#include "third_party/blink/renderer/platform/loader/testing/test_resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/testing/mock_context_lifecycle_notifier.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_loader_mock_factory.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

namespace {

class TestModuleScriptLoaderClient final
    : public GarbageCollected<TestModuleScriptLoaderClient>,
      public ModuleScriptLoaderClient {
 public:
  TestModuleScriptLoaderClient() = default;
  ~TestModuleScriptLoaderClient() override = default;

  void Trace(Visitor* visitor) const override {
    visitor->Trace(module_script_);
  }

  void NotifyNewSingleModuleFinished(ModuleScript* module_script) override {
    was_notify_finished_ = true;
    module_script_ = module_script;
  }

  bool WasNotifyFinished() const { return was_notify_finished_; }
  ModuleScript* GetModuleScript() { return module_script_.Get(); }

 private:
  bool was_notify_finished_ = false;
  Member<ModuleScript> module_script_;
};

class ModuleScriptLoaderTestModulator final : public DummyModulator {
 public:
  explicit ModuleScriptLoaderTestModulator(ScriptState* script_state)
      : script_state_(script_state) {}

  ~ModuleScriptLoaderTestModulator() override = default;

  KURL ResolveModuleSpecifier(const String& module_request,
                              const KURL& base_url,
                              String* failure_reason) final {
    return KURL(base_url, module_request);
  }

  ScriptState* GetScriptState() override { return script_state_.Get(); }

  ModuleScriptFetcher* CreateModuleScriptFetcher(
      ModuleScriptCustomFetchType custom_fetch_type,
      base::PassKey<ModuleScriptLoader> pass_key) override {
    auto* execution_context = ExecutionContext::From(script_state_);
    if (auto* scope = DynamicTo<WorkletGlobalScope>(execution_context)) {
      EXPECT_EQ(ModuleScriptCustomFetchType::kWorkletAddModule,
                custom_fetch_type);
      return MakeGarbageCollected<WorkletModuleScriptFetcher>(scope, pass_key);
    }
    EXPECT_EQ(ModuleScriptCustomFetchType::kNone, custom_fetch_type);
    return MakeGarbageCollected<DocumentModuleScriptFetcher>(execution_context,
                                                             pass_key);
  }

  void Trace(Visitor*) const override;

 private:
  Member<ScriptState> script_state_;
};

void ModuleScriptLoaderTestModulator::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
  DummyModulator::Trace(visitor);
}

}  // namespace

class ModuleScriptLoaderTest : public PageTestBase {
 public:
  ModuleScriptLoaderTest();
  ModuleScriptLoaderTest(const ModuleScriptLoaderTest&) = delete;
  ModuleScriptLoaderTest& operator=(const ModuleScriptLoaderTest&) = delete;
  void SetUp() override;
  void TearDown() override;

  void InitializeForDocument();
  void InitializeForWorklet();

  void TestFetchDataURL(ModuleScriptCustomFetchType,
                        TestModuleScriptLoaderClient*);
  void TestInvalidSpecifier(ModuleScriptCustomFetchType,
                            TestModuleScriptLoaderClient*);
  void TestFetchInvalidURL(ModuleScriptCustomFetchType,
                           TestModuleScriptLoaderClient*);
  void TestFetchURL(ModuleScriptCustomFetchType, TestModuleScriptLoaderClient*);
  void TestFetchDataURLJSONModule(ModuleScriptCustomFetchType custom_fetch_type,
                                  TestModuleScriptLoaderClient* client);
  void TestFetchDataURLInvalidJSONModule(
      ModuleScriptCustomFetchType custom_fetch_type,
      TestModuleScriptLoaderClient* client);

  ModuleScriptLoaderTestModulator* GetModulator() { return modulator_.Get(); }

  void RunUntilIdle() {
    static_cast<scheduler::FakeTaskRunner*>(fetcher_->GetTaskRunner().get())
        ->RunUntilIdle();
  }

  const base::TickClock* GetTickClock() override {
    return PageTestBase::GetTickClock();
  }

 protected:
  const KURL url_;
  const scoped_refptr<const SecurityOrigin> security_origin_;

  Persistent<ResourceFetcher> fetcher_;

  ScopedTestingPlatformSupport<FetchTestingPlatformSupport> platform_;
  std::unique_ptr<MockWorkerReportingProxy> reporting_proxy_;
  Persistent<ModuleScriptLoaderTestModulator> modulator_;
  Persistent<WorkletGlobalScope> global_scope_;
};

void ModuleScriptLoaderTest::SetUp() {
  PageTestBase::SetUp(gfx::Size(500, 500));
}

void ModuleScriptLoaderTest::TearDown() {
  if (global_scope_) {
    global_scope_->Dispose();
    global_scope_->NotifyContextDestroyed();
  }
}

ModuleScriptLoaderTest::ModuleScriptLoaderTest()
    : PageTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
      url_("https://example.test"),
      security_origin_(SecurityOrigin::Create(url_)) {
}

void ModuleScriptLoaderTest::InitializeForDocument() {
  auto* fetch_context = MakeGarbageCollected<MockFetchContext>();
  auto* properties =
      MakeGarbageCollected<TestResourceFetcherProperties>(security_origin_);
  fetcher_ = MakeGarbageCollected<ResourceFetcher>(
      ResourceFetcherInit(properties->MakeDetachable(), fetch_context,
                          base::MakeRefCounted<scheduler::FakeTaskRunner>(),
                          base::MakeRefCounted<scheduler::FakeTaskRunner>(),
                          MakeGarbageCollected<TestLoaderFactory>(
                              platform_->GetURLLoaderMockFactory()),
                          MakeGarbageCollected<MockContextLifecycleNotifier>(),
                          nullptr /* back_forward_cache_loader_helper */));
  modulator_ = MakeGarbageCollected<ModuleScriptLoaderTestModulator>(
      ToScriptStateForMainWorld(&GetFrame()));
}

void ModuleScriptLoaderTest::InitializeForWorklet() {
  auto* fetch_context = MakeGarbageCollected<MockFetchContext>();
  auto* properties =
      MakeGarbageCollected<TestResourceFetcherProperties>(security_origin_);
  fetcher_ = MakeGarbageCollected<ResourceFetcher>(
      ResourceFetcherInit(properties->MakeDetachable(), fetch_context,
                          base::MakeRefCounted<scheduler::FakeTaskRunner>(),
                          base::MakeRefCounted<scheduler::FakeTaskRunner>(),
                          MakeGarbageCollected<TestLoaderFactory>(
                              platform_->GetURLLoaderMockFactory()),
                          MakeGarbageCollected<MockContextLifecycleNotifier>(),
                          nullptr /* back_forward_cache_loader_helper */));
  reporting_proxy_ = std::make_unique<MockWorkerReportingProxy>();
  auto creation_params = std::make_unique<GlobalScopeCreationParams>(
      url_, mojom::blink::ScriptType::kModule, "GlobalScopeName", "UserAgent",
      UserAgentMetadata(), nullptr /* web_worker_fetch_context */,
      Vector<network::mojom::blink::ContentSecurityPolicyPtr>(),
      Vector<network::mojom::blink::ContentSecurityPolicyPtr>(),
      network::mojom::ReferrerPolicy::kDefault, security_origin_.get(),
      true /* is_secure_context */, HttpsState::kModern,
      nullptr /* worker_clients */, nullptr /* content_settings_client */,
      nullptr /* inherited_trial_features */, base::UnguessableToken::Create(),
      nullptr /* worker_settings */, mojom::blink::V8CacheOptions::kDefault,
      MakeGarbageCollected<WorkletModuleResponsesMap>(),
      mojo::NullRemote() /* browser_interface_broker */,
      mojo::NullRemote() /* code_cache_host_interface */,
      mojo::NullRemote() /* blob_url_store */, BeginFrameProviderParams(),
      nullptr /* parent_permissions_policy */,
      base::UnguessableToken::Create() /* agent_cluster_id */);
  creation_params->parent_context_token = GetFrame().GetLocalFrameToken();
  global_scope_ = MakeGarbageCollected<FakeWorkletGlobalScope>(
      std::move(creation_params), *reporting_proxy_, &GetFrame());
  global_scope_->ScriptController()->Initialize(NullURL());
  modulator_ = MakeGarbageCollected<ModuleScriptLoaderTestModulator>(
      global_scope_->ScriptController()->GetScriptState());
}
// TODO(nhiroki): Add tests for workers.

void ModuleScriptLoaderTest::TestFetchDataURL(
    ModuleScriptCustomFetchType custom_fetch_type,
    TestModuleScriptLoaderClient* client) {
  auto* registry = MakeGarbageCollected<ModuleScriptLoaderRegistry>();
  KURL url("data:text/javascript,export default 'grapes';");
  ModuleScriptLoader::Fetch(
      ModuleScriptFetchRequest::CreateForTest(url, ModuleType::kJavaScript),
      fetcher_, ModuleGraphLevel::kTopLevelModuleFetch, GetModulator(),
      custom_fetch_type, registry, client);
}

void ModuleScriptLoaderTest::TestFetchDataURLJSONModule(
    ModuleScriptCustomFetchType custom_fetch_type,
    TestModuleScriptLoaderClient* client) {
  auto* registry = MakeGarbageCollected<ModuleScriptLoaderRegistry>();
  KURL url(
      "data:application/"
      "json,{\"1\":{\"name\":\"MIKE\",\"surname\":\"TAYLOR\"},\"2\":{\"name\":"
      "\"TOM\",\"surname\":\"JERRY\"}}");
  ModuleScriptLoader::Fetch(
      ModuleScriptFetchRequest::CreateForTest(url, ModuleType::kJSON), fetcher_,
      ModuleGraphLevel::kTopLevelModuleFetch, GetModulator(), custom_fetch_type,
      registry, client);
}

void ModuleScriptLoaderTest::TestFetchDataURLInvalidJSONModule(
    ModuleScriptCustomFetchType custom_fetch_type,
    TestModuleScriptLoaderClient* client) {
  auto* registry = MakeGarbageCollected<ModuleScriptLoaderRegistry>();
  KURL url(
      "data:application/"
      "json,{{{");
  ModuleScriptLoader::Fetch(
      ModuleScriptFetchRequest::CreateForTest(url, ModuleType::kJSON), fetcher_,
      ModuleGraphLevel::kTopLevelModuleFetch, GetModulator(), custom_fetch_type,
      registry, client);
}

TEST_F(ModuleScriptLoaderTest, FetchDataURL) {
  InitializeForDocument();
  TestModuleScriptLoaderClient* client =
      MakeGarbageCollected<TestModuleScriptLoaderClient>();
  TestFetchDataURL(ModuleScriptCustomFetchType::kNone, client);

  // TODO(leszeks): This should finish synchronously, but currently due
  // to the script resource/script streamer interaction, it does not.
  RunUntilIdle();
  EXPECT_TRUE(client->WasNotifyFinished());
  ASSERT_TRUE(client->GetModuleScript());
  EXPECT_FALSE(client->GetModuleScript()->HasEmptyRecord());
  EXPECT_FALSE(client->GetModuleScript()->HasParseError());
}

TEST_F(ModuleScriptLoaderTest, FetchDataURLJSONModule) {
  InitializeForDocument();
  TestModuleScriptLoaderClient* client =
      MakeGarbageCollected<TestModuleScriptLoaderClient>();
  TestFetchDataURLJSONModule(ModuleScriptCustomFetchType::kNone, client);

  // TODO(leszeks): This should finish synchronously, but currently due
  // to the script resource/script streamer interaction, it does not.
  RunUntilIdle();
  EXPECT_TRUE(client->WasNotifyFinished());
  ASSERT_TRUE(client->GetModuleScript());
  EXPECT_FALSE(client->GetModuleScript()->HasEmptyRecord());
  EXPECT_FALSE(client->GetModuleScript()->HasParseError());
}

TEST_F(ModuleScriptLoaderTest, FetchDataURLInvalidJSONModule) {
  InitializeForDocument();
  TestModuleScriptLoaderClient* client =
      MakeGarbageCollected<TestModuleScriptLoaderClient>();
  TestFetchDataURLInvalidJSONModule(ModuleScriptCustomFetchType::kNone, client);

  // TODO(leszeks): This should finish synchronously, but currently due
  // to the script resource/script streamer interaction, it does not.
  RunUntilIdle();
  EXPECT_TRUE(client->WasNotifyFinished());
  ASSERT_TRUE(client->GetModuleScript());
  EXPECT_TRUE(client->GetModuleScript()->HasEmptyRecord());
  EXPECT_TRUE(client->GetModuleScript()->HasParseError());
}

TEST_F(ModuleScriptLoaderTest, FetchDataURL_OnWorklet) {
  InitializeForWorklet();
  TestModuleScriptLoaderClient* client1 =
      MakeGarbageCollected<TestModuleScriptLoaderClient>();
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
  TestModuleScriptLoaderClient* client2 =
      MakeGarbageCollected<TestModuleScriptLoaderClient>();
  TestFetchDataURL(ModuleScriptCustomFetchType::kWorkletAddModule, client2);

  EXPECT_FALSE(client2->WasNotifyFinished())
      << "ModuleScriptLoader should finish asynchronously.";
  RunUntilIdle();

  EXPECT_TRUE(client2->WasNotifyFinished());
  ASSERT_TRUE(client2->GetModuleScript());
  EXPECT_FALSE(client2->GetModuleScript()->HasEmptyRecord());
  EXPECT_FALSE(client2->GetModuleScript()->HasParseError());
}

TEST_F(ModuleScriptLoaderTest, FetchDataURLJSONModule_OnWorklet) {
  InitializeForWorklet();
  TestModuleScriptLoaderClient* client1 =
      MakeGarbageCollected<TestModuleScriptLoaderClient>();
  TestFetchDataURLJSONModule(ModuleScriptCustomFetchType::kWorkletAddModule,
                             client1);

  EXPECT_FALSE(client1->WasNotifyFinished())
      << "ModuleScriptLoader should finish asynchronously.";
  RunUntilIdle();

  EXPECT_TRUE(client1->WasNotifyFinished());
  ASSERT_TRUE(client1->GetModuleScript());
  EXPECT_FALSE(client1->GetModuleScript()->HasEmptyRecord());
  EXPECT_FALSE(client1->GetModuleScript()->HasParseError());

  // Try to fetch the same URL again in order to verify the case where
  // WorkletModuleResponsesMap serves a cache.
  TestModuleScriptLoaderClient* client2 =
      MakeGarbageCollected<TestModuleScriptLoaderClient>();
  TestFetchDataURLJSONModule(ModuleScriptCustomFetchType::kWorkletAddModule,
                             client2);

  EXPECT_FALSE(client2->WasNotifyFinished())
      << "ModuleScriptLoader should finish asynchronously.";
  RunUntilIdle();

  EXPECT_TRUE(client2->WasNotifyFinished());
  ASSERT_TRUE(client2->GetModuleScript());
  EXPECT_FALSE(client2->GetModuleScript()->HasEmptyRecord());
  EXPECT_FALSE(client2->GetModuleScript()->HasParseError());
}

TEST_F(ModuleScriptLoaderTest, FetchDataURLInvalidJSONModule_OnWorklet) {
  InitializeForWorklet();
  TestModuleScriptLoaderClient* client1 =
      MakeGarbageCollected<TestModuleScriptLoaderClient>();
  TestFetchDataURLInvalidJSONModule(
      ModuleScriptCustomFetchType::kWorkletAddModule, client1);

  EXPECT_FALSE(client1->WasNotifyFinished())
      << "ModuleScriptLoader should finish asynchronously.";
  RunUntilIdle();

  EXPECT_TRUE(client1->WasNotifyFinished());
  ASSERT_TRUE(client1->GetModuleScript());
  EXPECT_TRUE(client1->GetModuleScript()->HasEmptyRecord());
  EXPECT_TRUE(client1->GetModuleScript()->HasParseError());

  // Try to fetch the same URL again in order to verify the case where
  // WorkletModuleResponsesMap serves a cache.
  TestModuleScriptLoaderClient* client2 =
      MakeGarbageCollected<TestModuleScriptLoaderClient>();
  TestFetchDataURLInvalidJSONModule(
      ModuleScriptCustomFetchType::kWorkletAddModule, client2);

  EXPECT_FALSE(client2->WasNotifyFinished())
      << "ModuleScriptLoader should finish asynchronously.";
  RunUntilIdle();

  EXPECT_TRUE(client2->WasNotifyFinished());
  ASSERT_TRUE(client2->GetModuleScript());
  EXPECT_TRUE(client2->GetModuleScript()->HasEmptyRecord());
  EXPECT_TRUE(client2->GetModuleScript()->HasParseError());
}

void ModuleScriptLoaderTest::TestInvalidSpecifier(
    ModuleScriptCustomFetchType custom_fetch_type,
    TestModuleScriptLoaderClient* client) {
  auto* registry = MakeGarbageCollected<ModuleScriptLoaderRegistry>();
  KURL url("data:text/javascript,import 'invalid';export default 'grapes';");
  ModuleScriptLoader::Fetch(
      ModuleScriptFetchRequest::CreateForTest(url, ModuleType::kJavaScript),
      fetcher_, ModuleGraphLevel::kTopLevelModuleFetch, GetModulator(),
      custom_fetch_type, registry, client);
}

TEST_F(ModuleScriptLoaderTest, InvalidSpecifier) {
  InitializeForDocument();
  TestModuleScriptLoaderClient* client =
      MakeGarbageCollected<TestModuleScriptLoaderClient>();
  TestInvalidSpecifier(ModuleScriptCustomFetchType::kNone, client);

  // TODO(leszeks): This should finish synchronously, but currently due
  // to the script resource/script streamer interaction, it does not.
  RunUntilIdle();
  EXPECT_TRUE(client->WasNotifyFinished());

  ASSERT_TRUE(client->GetModuleScript());
  EXPECT_TRUE(client->GetModuleScript()->HasEmptyRecord());
  EXPECT_TRUE(client->GetModuleScript()->HasParseError());
}

TEST_F(ModuleScriptLoaderTest, InvalidSpecifier_OnWorklet) {
  InitializeForWorklet();
  TestModuleScriptLoaderClient* client =
      MakeGarbageCollected<TestModuleScriptLoaderClient>();
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
  auto* registry = MakeGarbageCollected<ModuleScriptLoaderRegistry>();
  KURL url;
  EXPECT_FALSE(url.IsValid());
  ModuleScriptLoader::Fetch(
      ModuleScriptFetchRequest::CreateForTest(url, ModuleType::kJavaScript),
      fetcher_, ModuleGraphLevel::kTopLevelModuleFetch, GetModulator(),
      custom_fetch_type, registry, client);
}

TEST_F(ModuleScriptLoaderTest, FetchInvalidURL) {
  InitializeForDocument();
  TestModuleScriptLoaderClient* client =
      MakeGarbageCollected<TestModuleScriptLoaderClient>();
  TestFetchInvalidURL(ModuleScriptCustomFetchType::kNone, client);

  // TODO(leszeks): This should finish synchronously, but currently due
  // to the script resource/script streamer interaction, it does not.
  RunUntilIdle();
  EXPECT_TRUE(client->WasNotifyFinished());
  EXPECT_FALSE(client->GetModuleScript());
}

TEST_F(ModuleScriptLoaderTest, FetchInvalidURL_OnWorklet) {
  InitializeForWorklet();
  TestModuleScriptLoaderClient* client =
      MakeGarbageCollected<TestModuleScriptLoaderClient>();
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
      url, test::CoreTestDataPath("module.js"), "text/javascript",
      platform_->GetURLLoaderMockFactory());

  auto* registry = MakeGarbageCollected<ModuleScriptLoaderRegistry>();
  ModuleScriptLoader::Fetch(
      ModuleScriptFetchRequest::CreateForTest(url, ModuleType::kJavaScript),
      fetcher_, ModuleGraphLevel::kTopLevelModuleFetch, GetModulator(),
      custom_fetch_type, registry, client);
}

TEST_F(ModuleScriptLoaderTest, FetchURL) {
  InitializeForDocument();
  TestModuleScriptLoaderClient* client =
      MakeGarbageCollected<TestModuleScriptLoaderClient>();
  TestFetchURL(ModuleScriptCustomFetchType::kNone, client);

  EXPECT_FALSE(client->WasNotifyFinished())
      << "ModuleScriptLoader unexpectedly finished synchronously.";
  platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  // TODO(leszeks): This should finish synchronously, but currently due
  // to the script resource/script streamer interaction, it does not.
  RunUntilIdle();

  EXPECT_TRUE(client->WasNotifyFinished());
  EXPECT_TRUE(client->GetModuleScript());
}

TEST_F(ModuleScriptLoaderTest, FetchURL_OnWorklet) {
  InitializeForWorklet();
  TestModuleScriptLoaderClient* client =
      MakeGarbageCollected<TestModuleScriptLoaderClient>();
  TestFetchURL(ModuleScriptCustomFetchType::kWorkletAddModule, client);

  EXPECT_FALSE(client->WasNotifyFinished())
      << "ModuleScriptLoader unexpectedly finished synchronously.";
  platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  RunUntilIdle();

  EXPECT_TRUE(client->WasNotifyFinished());
  EXPECT_TRUE(client->GetModuleScript());
}

}  // namespace blink
