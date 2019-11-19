// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <bitset>
#include "base/single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/main_thread_worklet_reporting_proxy.h"
#include "third_party/blink/renderer/core/workers/worklet_global_scope.h"
#include "third_party/blink/renderer/core/workers/worklet_module_responses_map.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

class MainThreadWorkletReportingProxyForTest final
    : public MainThreadWorkletReportingProxy {
 public:
  explicit MainThreadWorkletReportingProxyForTest(Document* document)
      : MainThreadWorkletReportingProxy(document) {}

  void CountFeature(WebFeature feature) override {
    // Any feature should be reported only one time.
    EXPECT_FALSE(reported_features_[static_cast<size_t>(feature)]);
    reported_features_.set(static_cast<size_t>(feature));
    MainThreadWorkletReportingProxy::CountFeature(feature);
  }

  void CountDeprecation(WebFeature feature) override {
    // Any feature should be reported only one time.
    EXPECT_FALSE(reported_features_[static_cast<size_t>(feature)]);
    reported_features_.set(static_cast<size_t>(feature));
    MainThreadWorkletReportingProxy::CountDeprecation(feature);
  }

 private:
  std::bitset<static_cast<size_t>(WebFeature::kNumberOfFeatures)>
      reported_features_;
};

class MainThreadWorkletTest : public PageTestBase {
 public:
  void SetUp() override {
    SetUpScope("script-src 'self' https://allowed.example.com");
  }
  void SetUpScope(const String& csp_header) {
    PageTestBase::SetUp(IntSize());
    NavigateTo(KURL("https://example.com/"));
    Document* document = &GetDocument();

    // Set up the CSP for Document before starting MainThreadWorklet because
    // MainThreadWorklet inherits the owner Document's CSP.
    auto* csp = MakeGarbageCollected<ContentSecurityPolicy>();
    csp->DidReceiveHeader(csp_header, kContentSecurityPolicyHeaderTypeEnforce,
                          kContentSecurityPolicyHeaderSourceHTTP);
    document->InitContentSecurityPolicy(csp);

    reporting_proxy_ =
        std::make_unique<MainThreadWorkletReportingProxyForTest>(document);
    auto creation_params = std::make_unique<GlobalScopeCreationParams>(
        document->Url(), mojom::ScriptType::kModule,
        OffMainThreadWorkerScriptFetchOption::kEnabled, "MainThreadWorklet",
        document->UserAgent(), nullptr /* web_worker_fetch_context */,
        document->GetContentSecurityPolicy()->Headers(),
        document->GetReferrerPolicy(), document->GetSecurityOrigin(),
        document->IsSecureContext(), document->GetHttpsState(),
        nullptr /* worker_clients */, nullptr /* content_settings_client */,
        document->AddressSpace(), OriginTrialContext::GetTokens(document).get(),
        base::UnguessableToken::Create(), nullptr /* worker_settings */,
        kV8CacheOptionsDefault,
        MakeGarbageCollected<WorkletModuleResponsesMap>());
    global_scope_ = MakeGarbageCollected<WorkletGlobalScope>(
        std::move(creation_params), *reporting_proxy_, &GetFrame());
    EXPECT_TRUE(global_scope_->IsMainThreadWorkletGlobalScope());
    EXPECT_FALSE(global_scope_->IsThreadedWorkletGlobalScope());
  }

  void TearDown() override { global_scope_->Dispose(); }

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

TEST_F(MainThreadWorkletTest, ContentSecurityPolicy) {
  ContentSecurityPolicy* csp = global_scope_->GetContentSecurityPolicy();

  // The "script-src 'self'" directive allows this.
  EXPECT_TRUE(csp->AllowScriptFromSource(
      global_scope_->Url(), String(), IntegrityMetadataSet(), kParserInserted));

  // The "script-src https://allowed.example.com" should allow this.
  EXPECT_TRUE(csp->AllowScriptFromSource(KURL("https://allowed.example.com"),
                                         String(), IntegrityMetadataSet(),
                                         kParserInserted));

  EXPECT_FALSE(csp->AllowScriptFromSource(
      KURL("https://disallowed.example.com"), String(), IntegrityMetadataSet(),
      kParserInserted));
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
  const WebFeature kFeature2 = WebFeature::kPrefixedStorageInfo;

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
  ContentSecurityPolicy* csp = global_scope_->GetContentSecurityPolicy();

  // At this point check that the CSP that was set is indeed invalid.
  EXPECT_EQ(1ul, csp->Headers().size());
  EXPECT_EQ("invalid-csp", csp->Headers().at(0).first);
  EXPECT_EQ(kContentSecurityPolicyHeaderTypeEnforce,
            csp->Headers().at(0).second);
}

}  // namespace blink
