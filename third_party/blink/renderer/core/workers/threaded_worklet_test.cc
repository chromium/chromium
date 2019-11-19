// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <bitset>
#include "base/single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_cache_options.h"
#include "third_party/blink/renderer/core/inspector/console_message_storage.h"
#include "third_party/blink/renderer/core/inspector/thread_debugger.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/threaded_worklet_messaging_proxy.h"
#include "third_party/blink/renderer/core/workers/threaded_worklet_object_proxy.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/core/workers/worker_thread_test_helper.h"
#include "third_party/blink/renderer/core/workers/worklet_global_scope.h"
#include "third_party/blink/renderer/core/workers/worklet_module_responses_map.h"
#include "third_party/blink/renderer/core/workers/worklet_thread_holder.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

class ThreadedWorkletObjectProxyForTest final
    : public ThreadedWorkletObjectProxy {
 public:
  ThreadedWorkletObjectProxyForTest(
      ThreadedWorkletMessagingProxy* messaging_proxy,
      ParentExecutionContextTaskRunners* parent_execution_context_task_runners)
      : ThreadedWorkletObjectProxy(messaging_proxy,
                                   parent_execution_context_task_runners) {}

 protected:
  void CountFeature(WebFeature feature) override {
    // Any feature should be reported only one time.
    EXPECT_FALSE(reported_features_[static_cast<size_t>(feature)]);
    reported_features_.set(static_cast<size_t>(feature));
    ThreadedWorkletObjectProxy::CountFeature(feature);
  }

  void CountDeprecation(WebFeature feature) final {
    // Any feature should be reported only one time.
    EXPECT_FALSE(reported_features_[static_cast<size_t>(feature)]);
    reported_features_.set(static_cast<size_t>(feature));
    ThreadedWorkletObjectProxy::CountDeprecation(feature);
  }

 private:
  std::bitset<static_cast<size_t>(WebFeature::kNumberOfFeatures)>
      reported_features_;
};

class ThreadedWorkletThreadForTest : public WorkerThread {
 public:
  explicit ThreadedWorkletThreadForTest(
      WorkerReportingProxy& worker_reporting_proxy)
      : WorkerThread(worker_reporting_proxy) {}
  ~ThreadedWorkletThreadForTest() override = default;

  WorkerBackingThread& GetWorkerBackingThread() override {
    auto* worklet_thread_holder =
        WorkletThreadHolder<ThreadedWorkletThreadForTest>::GetInstance();
    DCHECK(worklet_thread_holder);
    return *worklet_thread_holder->GetThread();
  }

  void ClearWorkerBackingThread() override {}

  static void EnsureSharedBackingThread() {
    DCHECK(IsMainThread());
    WorkletThreadHolder<ThreadedWorkletThreadForTest>::EnsureInstance(
        ThreadCreationParams(ThreadType::kTestThread)
            .SetThreadNameForTest("ThreadedWorkletThreadForTest"));
  }

  static void ClearSharedBackingThread() {
    DCHECK(IsMainThread());
    WorkletThreadHolder<ThreadedWorkletThreadForTest>::ClearInstance();
  }

  void TestSecurityOrigin() {
    WorkletGlobalScope* global_scope = To<WorkletGlobalScope>(GlobalScope());
    // The SecurityOrigin for a worklet should be a unique opaque origin, while
    // the owner Document's SecurityOrigin shouldn't.
    EXPECT_TRUE(global_scope->GetSecurityOrigin()->IsOpaque());
    EXPECT_FALSE(global_scope->DocumentSecurityOrigin()->IsOpaque());
    PostCrossThreadTask(*GetParentTaskRunnerForTesting(), FROM_HERE,
                        CrossThreadBindOnce(&test::ExitRunLoop));
  }

  void TestContentSecurityPolicy() {
    EXPECT_TRUE(IsCurrentThread());
    ContentSecurityPolicy* csp = GlobalScope()->GetContentSecurityPolicy();

    // The "script-src 'self'" directive allows this.
    EXPECT_TRUE(csp->AllowScriptFromSource(GlobalScope()->Url(), String(),
                                           IntegrityMetadataSet(),
                                           kParserInserted));

    // The "script-src https://allowed.example.com" should allow this.
    EXPECT_TRUE(csp->AllowScriptFromSource(KURL("https://allowed.example.com"),
                                           String(), IntegrityMetadataSet(),
                                           kParserInserted));

    EXPECT_FALSE(csp->AllowScriptFromSource(
        KURL("https://disallowed.example.com"), String(),
        IntegrityMetadataSet(), kParserInserted));

    PostCrossThreadTask(*GetParentTaskRunnerForTesting(), FROM_HERE,
                        CrossThreadBindOnce(&test::ExitRunLoop));
  }

  // Test that having an invalid CSP does not result in an exception.
  // See bugs: 844383,844317
  void TestInvalidContentSecurityPolicy() {
    EXPECT_TRUE(IsCurrentThread());

    // At this point check that the CSP that was set is indeed invalid.
    ContentSecurityPolicy* csp = GlobalScope()->GetContentSecurityPolicy();
    EXPECT_EQ(1ul, csp->Headers().size());
    EXPECT_EQ("invalid-csp", csp->Headers().at(0).first);
    EXPECT_EQ(kContentSecurityPolicyHeaderTypeEnforce,
              csp->Headers().at(0).second);

    PostCrossThreadTask(*GetParentTaskRunnerForTesting(), FROM_HERE,
                        CrossThreadBindOnce(&test::ExitRunLoop));
  }

  // Emulates API use on threaded WorkletGlobalScope.
  void CountFeature(WebFeature feature) {
    EXPECT_TRUE(IsCurrentThread());
    GlobalScope()->CountFeature(feature);
    PostCrossThreadTask(*GetParentTaskRunnerForTesting(), FROM_HERE,
                        CrossThreadBindOnce(&test::ExitRunLoop));
  }

  // Emulates deprecated API use on threaded WorkletGlobalScope.
  void CountDeprecation(WebFeature feature) {
    EXPECT_TRUE(IsCurrentThread());
    GlobalScope()->CountDeprecation(feature);

    // countDeprecation() should add a warning message.
    EXPECT_EQ(1u, GetConsoleMessageStorage()->size());
    String console_message = GetConsoleMessageStorage()->at(0)->Message();
    EXPECT_TRUE(console_message.Contains("deprecated"));

    PostCrossThreadTask(*GetParentTaskRunnerForTesting(), FROM_HERE,
                        CrossThreadBindOnce(&test::ExitRunLoop));
  }

  void TestTaskRunner() {
    EXPECT_TRUE(IsCurrentThread());
    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        GlobalScope()->GetTaskRunner(TaskType::kInternalTest);
    EXPECT_TRUE(task_runner->RunsTasksInCurrentSequence());
    PostCrossThreadTask(*GetParentTaskRunnerForTesting(), FROM_HERE,
                        CrossThreadBindOnce(&test::ExitRunLoop));
  }

 private:
  WorkerOrWorkletGlobalScope* CreateWorkerGlobalScope(
      std::unique_ptr<GlobalScopeCreationParams> creation_params) final {
    auto* global_scope = MakeGarbageCollected<WorkletGlobalScope>(
        std::move(creation_params), GetWorkerReportingProxy(), this);
    EXPECT_FALSE(global_scope->IsMainThreadWorkletGlobalScope());
    EXPECT_TRUE(global_scope->IsThreadedWorkletGlobalScope());
    return global_scope;
  }

  bool IsOwningBackingThread() const final { return false; }

  ThreadType GetThreadType() const override {
    return ThreadType::kUnspecifiedWorkerThread;
  }
};

class ThreadedWorkletMessagingProxyForTest
    : public ThreadedWorkletMessagingProxy {
 public:
  explicit ThreadedWorkletMessagingProxyForTest(
      ExecutionContext* execution_context)
      : ThreadedWorkletMessagingProxy(execution_context) {
    worklet_object_proxy_ = std::make_unique<ThreadedWorkletObjectProxyForTest>(
        this, GetParentExecutionContextTaskRunners());
  }

  ~ThreadedWorkletMessagingProxyForTest() override = default;

  void Start() {
    Document* document = To<Document>(GetExecutionContext());
    std::unique_ptr<Vector<char>> cached_meta_data = nullptr;
    WorkerClients* worker_clients = nullptr;
    std::unique_ptr<WorkerSettings> worker_settings = nullptr;
    InitializeWorkerThread(
        std::make_unique<GlobalScopeCreationParams>(
            document->Url(), mojom::ScriptType::kModule,
            OffMainThreadWorkerScriptFetchOption::kEnabled, "threaded_worklet",
            document->UserAgent(), nullptr /* web_worker_fetch_context */,
            document->GetContentSecurityPolicy()->Headers(),
            document->GetReferrerPolicy(), document->GetSecurityOrigin(),
            document->IsSecureContext(), document->GetHttpsState(),
            worker_clients, nullptr /* content_settings_client */,
            document->AddressSpace(),
            OriginTrialContext::GetTokens(document).get(),
            base::UnguessableToken::Create(), std::move(worker_settings),
            kV8CacheOptionsDefault,
            MakeGarbageCollected<WorkletModuleResponsesMap>()),
        base::nullopt);
  }

 private:
  friend class ThreadedWorkletTest;

  std::unique_ptr<WorkerThread> CreateWorkerThread() final {
    return std::make_unique<ThreadedWorkletThreadForTest>(WorkletObjectProxy());
  }
};

class ThreadedWorkletTest : public testing::Test {
 public:
  void SetUp() override {
    page_ = std::make_unique<DummyPageHolder>();
    KURL url("https://example.com/");
    page_->GetFrame().Loader().CommitNavigation(
        WebNavigationParams::CreateWithHTMLBuffer(SharedBuffer::Create(), url),
        nullptr /* extra_data */);
    blink::test::RunPendingTasks();
    ASSERT_EQ(url.GetString(), GetDocument().Url().GetString());

    messaging_proxy_ =
        MakeGarbageCollected<ThreadedWorkletMessagingProxyForTest>(
            &page_->GetDocument());
    ThreadedWorkletThreadForTest::EnsureSharedBackingThread();
  }

  void TearDown() override {
    GetWorkerThread()->Terminate();
    GetWorkerThread()->WaitForShutdownForTesting();
    test::RunPendingTasks();
    ThreadedWorkletThreadForTest::ClearSharedBackingThread();
    messaging_proxy_ = nullptr;
  }

  ThreadedWorkletMessagingProxyForTest* MessagingProxy() {
    return messaging_proxy_.Get();
  }

  ThreadedWorkletThreadForTest* GetWorkerThread() {
    return static_cast<ThreadedWorkletThreadForTest*>(
        messaging_proxy_->GetWorkerThread());
  }

  Document& GetDocument() { return page_->GetDocument(); }

 private:
  std::unique_ptr<DummyPageHolder> page_;
  Persistent<ThreadedWorkletMessagingProxyForTest> messaging_proxy_;
};

TEST_F(ThreadedWorkletTest, SecurityOrigin) {
  MessagingProxy()->Start();

  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBindOnce(&ThreadedWorkletThreadForTest::TestSecurityOrigin,
                          CrossThreadUnretained(GetWorkerThread())));
  test::EnterRunLoop();
}

TEST_F(ThreadedWorkletTest, ContentSecurityPolicy) {
  // Set up the CSP for Document before starting ThreadedWorklet because
  // ThreadedWorklet inherits the owner Document's CSP.
  auto* csp = MakeGarbageCollected<ContentSecurityPolicy>();
  csp->DidReceiveHeader("script-src 'self' https://allowed.example.com",
                        kContentSecurityPolicyHeaderTypeEnforce,
                        kContentSecurityPolicyHeaderSourceHTTP);
  GetDocument().InitContentSecurityPolicy(csp);

  MessagingProxy()->Start();

  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBindOnce(
          &ThreadedWorkletThreadForTest::TestContentSecurityPolicy,
          CrossThreadUnretained(GetWorkerThread())));
  test::EnterRunLoop();
}

TEST_F(ThreadedWorkletTest, InvalidContentSecurityPolicy) {
  auto* csp = MakeGarbageCollected<ContentSecurityPolicy>();
  csp->DidReceiveHeader("invalid-csp", kContentSecurityPolicyHeaderTypeEnforce,
                        kContentSecurityPolicyHeaderSourceHTTP);
  GetDocument().InitContentSecurityPolicy(csp);

  MessagingProxy()->Start();

  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBindOnce(
          &ThreadedWorkletThreadForTest::TestInvalidContentSecurityPolicy,
          CrossThreadUnretained(GetWorkerThread())));
  test::EnterRunLoop();
}

TEST_F(ThreadedWorkletTest, UseCounter) {
  Page::InsertOrdinaryPageForTesting(GetDocument().GetPage());
  MessagingProxy()->Start();

  // This feature is randomly selected.
  const WebFeature kFeature1 = WebFeature::kRequestFileSystem;

  // API use on the threaded WorkletGlobalScope should be recorded in UseCounter
  // on the Document.
  EXPECT_FALSE(GetDocument().IsUseCounted(kFeature1));
  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBindOnce(&ThreadedWorkletThreadForTest::CountFeature,
                          CrossThreadUnretained(GetWorkerThread()), kFeature1));
  test::EnterRunLoop();
  EXPECT_TRUE(GetDocument().IsUseCounted(kFeature1));

  // API use should be reported to the Document only one time. See comments in
  // ThreadedWorkletObjectProxyForTest::CountFeature.
  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBindOnce(&ThreadedWorkletThreadForTest::CountFeature,
                          CrossThreadUnretained(GetWorkerThread()), kFeature1));
  test::EnterRunLoop();

  // This feature is randomly selected from Deprecation::deprecationMessage().
  const WebFeature kFeature2 = WebFeature::kPrefixedStorageInfo;

  // Deprecated API use on the threaded WorkletGlobalScope should be recorded in
  // UseCounter on the Document.
  EXPECT_FALSE(GetDocument().IsUseCounted(kFeature2));
  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBindOnce(&ThreadedWorkletThreadForTest::CountDeprecation,
                          CrossThreadUnretained(GetWorkerThread()), kFeature2));
  test::EnterRunLoop();
  EXPECT_TRUE(GetDocument().IsUseCounted(kFeature2));

  // API use should be reported to the Document only one time. See comments in
  // ThreadedWorkletObjectProxyForTest::CountDeprecation.
  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBindOnce(&ThreadedWorkletThreadForTest::CountDeprecation,
                          CrossThreadUnretained(GetWorkerThread()), kFeature2));
  test::EnterRunLoop();
}

TEST_F(ThreadedWorkletTest, TaskRunner) {
  MessagingProxy()->Start();

  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBindOnce(&ThreadedWorkletThreadForTest::TestTaskRunner,
                          CrossThreadUnretained(GetWorkerThread())));
  test::EnterRunLoop();
}

}  // namespace blink
