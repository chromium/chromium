// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <bitset>

#include "base/task/single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/main_thread_worklet_reporting_proxy.h"
#include "third_party/blink/renderer/core/workers/worklet_global_scope.h"
#include "third_party/blink/renderer/core/workers/worklet_global_scope_test_helper.h"
#include "third_party/blink/renderer/core/workers/worklet_module_responses_map.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

class MainThreadWorkletReportingProxyForTest final
    : public MainThreadWorkletReportingProxy {
 public:
  explicit MainThreadWorkletReportingProxyForTest(LocalDOMWindow* window)
      : MainThreadWorkletReportingProxy(window) {}

  void CountFeature(WebFeature feature) override {
    // Any feature should be reported only one time.
    EXPECT_FALSE(reported_features_[static_cast<size_t>(feature)]);
    reported_features_.set(static_cast<size_t>(feature));
    MainThreadWorkletReportingProxy::CountFeature(feature);
  }

 private:
  std::bitset<static_cast<size_t>(WebFeature::kMaxValue) + 1>
      reported_features_;
};

class MainThreadWorkletTest : public PageTestBase {
 public:
  void SetUp() override {
    SetUpScope("script-src 'self' https://allowed.example.com");
  }
  void SetUpScope(const String& csp_header) {
    PageTestBase::SetUp(gfx::Size());
    KURL url = KURL("https://example.com/");
    NavigateTo(url);
    LocalDOMWindow* window = GetFrame().DomWindow();

    // Set up the CSP for Document before starting MainThreadWorklet because
    // MainThreadWorklet inherits the owner Document's CSP.
    auto* csp = MakeGarbageCollected<ContentSecurityPolicy>();
    scoped_refptr<SecurityOrigin> self_origin = SecurityOrigin::Create(url);
    csp->AddPolicies(ParseContentSecurityPolicies(
        csp_header, network::mojom::ContentSecurityPolicyType::kEnforce,
        network::mojom::ContentSecurityPolicySource::kHTTP, *(self_origin)));
    window->SetContentSecurityPolicy(csp);

    reporting_proxy_ =
        std::make_unique<MainThreadWorkletReportingProxyForTest>(window);
    auto creation_params = std::make_unique<GlobalScopeCreationParams>(
        window->Url(), mojom::blink::ScriptType::kModule, "MainThreadWorklet",
        window->UserAgent(), window->GetFrame()->Loader().UserAgentMetadata(),
        nullptr /* web_worker_fetch_context */,
        mojo::Clone(window->GetContentSecurityPolicy()->GetParsedPolicies()),
        Vector<network::mojom::blink::ContentSecurityPolicyPtr>(),
        window->GetReferrerPolicy(), window->GetSecurityOrigin(),
        window->IsSecureContext(), window->GetHttpsState(),
        nullptr /* worker_clients */, nullptr /* content_settings_client */,
        OriginTrialContext::GetInheritedTrialFeatures(window).get(),
        base::UnguessableToken::Create(), nullptr /* worker_settings */,
        mojom::blink::V8CacheOptions::kDefault,
        MakeGarbageCollected<WorkletModuleResponsesMap>(),
        mojo::NullRemote() /* browser_interface_broker */,
        window->GetFrame()->Loader().CreateWorkerCodeCacheHost(),
        mojo::NullRemote() /* blob_url_store */, BeginFrameProviderParams(),
        nullptr /* parent_permissions_policy */, window->GetAgentClusterID(),
        ukm::kInvalidSourceId, window->GetExecutionContextToken());
    global_scope_ = MakeGarbageCollected<FakeWorkletGlobalScope>(
        std::move(creation_params), *reporting_proxy_, &GetFrame());
    EXPECT_TRUE(global_scope_->IsMainThreadWorkletGlobalScope());
    EXPECT_FALSE(global_scope_->IsThreadedWorkletGlobalScope());
  }

  void TearDown() override {
    global_scope_->Dispose();
    global_scope_->NotifyContextDestroyed();
  }

 protected:
  std::unique_ptr<MainThreadWorkletReportingProxyForTest> reporting_proxy_;
  Persistent<WorkletGlobalScope> global_scope_;
};

class MainThreadWorkletInvalidCSPTest : public MainThreadWorkletTest {
 public:
  void SetUp() override { SetUpScope("invalid-csp"); }
};

TEST_F(MainThreadWorkletTest, SecurityOrigin) {
  // The SecurityOrigin for a worklet should be a unique opaque origin, while
  // the owner Document's SecurityOrigin shouldn't.
  EXPECT_TRUE(global_scope_->GetSecurityOrigin()->IsOpaque());
  EXPECT_FALSE(global_scope_->DocumentSecurityOrigin()->IsOpaque());
}

TEST_F(MainThreadWorkletTest, AgentCluster) {
  // The worklet should be in the owner window's agent cluster.
  ASSERT_TRUE(GetFrame().DomWindow()->GetAgentClusterID());
  EXPECT_EQ(global_scope_->GetAgentClusterID(),
            GetFrame().DomWindow()->GetAgentClusterID());
}

TEST_F(MainThreadWorkletTest, ContentSecurityPolicy) {
  ContentSecurityPolicy* csp = global_scope_->GetContentSecurityPolicy();

  // The "script-src 'self'" directive allows this.
  EXPECT_TRUE(csp->AllowScriptFromSource(
      global_scope_->Url(), String(), IntegrityMetadataSet(), kParserInserted,
      global_scope_->Url(), RedirectStatus::kNoRedirect));

  // The "script-src https://allowed.example.com" should allow this.
  EXPECT_TRUE(csp->AllowScriptFromSource(
      KURL("https://allowed.example.com"), String(), IntegrityMetadataSet(),
      kParserInserted, KURL("https://allowed.example.com"),
      RedirectStatus::kNoRedirect));

  EXPECT_FALSE(csp->AllowScriptFromSource(
      KURL("https://disallowed.example.com"), String(), IntegrityMetadataSet(),
      kParserInserted, KURL("https://disallowed.example.com"),
      RedirectStatus::kNoRedirect));
}

TEST_F(MainThreadWorkletTest, UseCounter) {
  Page::InsertOrdinaryPageForTesting(&GetPage());
  // This feature is randomly selected.
  const WebFeature kFeature1 = WebFeature::kRequestFileSystem;

  // API use on WorkletGlobalScope for the main thread should be recorded in
  // UseCounter on the Document.
  EXPECT_FALSE(GetDocument().IsUseCounted(kFeature1));
  UseCounter::Count(global_scope_, kFeature1);
  EXPECT_TRUE(GetDocument().IsUseCounted(kFeature1));

  // API use should be reported to the Document only one time. See comments in
  // MainThreadWorkletReportingProxyForTest::ReportFeature.
  UseCounter::Count(global_scope_, kFeature1);

  // This feature is randomly selected from Deprecation::deprecationMessage().
  const WebFeature kFeature2 = WebFeature::kPaymentInstruments;

  // Deprecated API use on WorkletGlobalScope for the main thread should be
  // recorded in UseCounter on the Document.
  EXPECT_FALSE(GetDocument().IsUseCounted(kFeature2));
  Deprecation::CountDeprecation(global_scope_, kFeature2);
  EXPECT_TRUE(GetDocument().IsUseCounted(kFeature2));

  // API use should be reported to the Document only one time. See comments in
  // MainThreadWorkletReportingProxyForTest::ReportDeprecation.
  Deprecation::CountDeprecation(global_scope_, kFeature2);
}

TEST_F(MainThreadWorkletTest, TaskRunner) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      global_scope_->GetTaskRunner(TaskType::kInternalTest);
  EXPECT_TRUE(task_runner->RunsTasksInCurrentSequence());
}

// Test that having an invalid CSP does not result in an exception.
// See bugs: 844383,844317
TEST_F(MainThreadWorkletInvalidCSPTest, InvalidContentSecurityPolicy) {
  const Vector<network::mojom::blink::ContentSecurityPolicyPtr>& csp =
      global_scope_->GetContentSecurityPolicy()->GetParsedPolicies();

  // At this point check that the CSP that was set is indeed invalid.
  EXPECT_EQ(1ul, csp.size());
  EXPECT_EQ("invalid-csp", csp[0]->header->header_value);
  EXPECT_EQ(network::mojom::ContentSecurityPolicyType::kEnforce,
            csp[0]->header->type);
}

}  // namespace blink
