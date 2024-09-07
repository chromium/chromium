// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <bitset>

#include "base/gtest_prod_util.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/v8_cache_options.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/thread_debugger_common_impl.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/threaded_worklet_messaging_proxy.h"
#include "third_party/blink/renderer/core/workers/threaded_worklet_object_proxy.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/core/workers/worker_thread_test_helper.h"
#include "third_party/blink/renderer/core/workers/worklet_global_scope.h"
#include "third_party/blink/renderer/core/workers/worklet_global_scope_test_helper.h"
#include "third_party/blink/renderer/core/workers/worklet_module_responses_map.h"
#include "third_party/blink/renderer/core/workers/worklet_thread_holder.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

class ThreadedWorkletObjectProxyForTest final
    : public ThreadedWorkletObjectProxy {
 public:
  ThreadedWorkletObjectProxyForTest(
      ThreadedWorkletMessagingProxy* messaging_proxy,
      ParentExecutionContextTaskRunners* parent_execution_context_task_runners)
      : ThreadedWorkletObjectProxy(messaging_proxy,
                                   parent_execution_context_task_runners,
                                   /*parent_agent_group_task_runner=*/nullptr) {
  }

 protected:
  void CountFeature(WebFeature feature) override {
    // Any feature should be reported only one time.
    EXPECT_FALSE(reported_features_[static_cast<size_t>(feature)]);
    reported_features_.set(static_cast<size_t>(feature));
    ThreadedWorkletObjectProxy::CountFeature(feature);
  }

 private:
  std::bitset<static_cast<size_t>(WebFeature::kMaxValue) + 1>
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

  void TestSecurityOrigin(WTF::CrossThreadOnceClosure quit_closure) {
    WorkletGlobalScope* global_scope = To<WorkletGlobalScope>(GlobalScope());
    // The SecurityOrigin for a worklet should be a unique opaque origin, while
    // the owner Document's SecurityOrigin shouldn't.
    EXPECT_TRUE(global_scope->GetSecurityOrigin()->IsOpaque());
    EXPECT_FALSE(global_scope->DocumentSecurityOrigin()->IsOpaque());
    PostCrossThreadTask(*GetParentTaskRunnerForTesting(), FROM_HERE,
                        CrossThreadBindOnce(std::move(quit_closure)));
  }

  void TestAgentCluster(base::UnguessableToken owner_agent_cluster_id,
                        WTF::CrossThreadOnceClosure quit_closure) {
    ASSERT_TRUE(owner_agent_cluster_id);
    EXPECT_EQ(GlobalScope()->GetAgentClusterID(), owner_agent_cluster_id);
    PostCrossThreadTask(*GetParentTaskRunnerForTesting(), FROM_HERE,
                        CrossThreadBindOnce(std::move(quit_closure)));
  }

  void TestContentSecurityPolicy(WTF::CrossThreadOnceClosure quit_closure) {
    EXPECT_TRUE(IsCurrentThread());
    ContentSecurityPolicy* csp = GlobalScope()->GetContentSecurityPolicy();
    KURL main_document_url = KURL("https://example.com/script.js");

    // The "script-src 'self'" directive allows |main_document_url| since it is
    // same-origin with the main document.
    EXPECT_TRUE(csp->AllowScriptFromSource(
        main_document_url, String(), IntegrityMetadataSet(), kParserInserted,
        main_document_url, RedirectStatus::kNoRedirect));

    // The "script-src https://allowed.example.com" should allow this.
    EXPECT_TRUE(csp->AllowScriptFromSource(
        KURL("https://allowed.example.com"), String(), IntegrityMetadataSet(),
        kParserInserted, KURL("https://allowed.example.com"),
        RedirectStatus::kNoRedirect));

    EXPECT_FALSE(csp->AllowScriptFromSource(
        KURL("https://disallowed.example.com"), String(),
        IntegrityMetadataSet(), kParserInserted,
        KURL("https://disallowed.example.com"), RedirectStatus::kNoRedirect));

    PostCrossThreadTask(*GetParentTaskRunnerForTesting(), FROM_HERE,
                        CrossThreadBindOnce(std::move(quit_closure)));
  }

  // Test that having an invalid CSP does not result in an exception.
  // See bugs: 844383,844317
  void TestInvalidContentSecurityPolicy(
      WTF::CrossThreadOnceClosure quit_closure) {
    EXPECT_TRUE(IsCurrentThread());

    // At this point check that the CSP that was set is indeed invalid.
    const Vector<network::mojom::blink::ContentSecurityPolicyPtr>& csp =
        GlobalScope()->GetContentSecurityPolicy()->GetParsedPolicies();
    EXPECT_EQ(1ul, csp.size());
    EXPECT_EQ("invalid-csp", csp[0]->header->header_value);
    EXPECT_EQ(network::mojom::ContentSecurityPolicyType::kEnforce,
              csp[0]->header->type);

    PostCrossThreadTask(*GetParentTaskRunnerForTesting(), FROM_HERE,
                        CrossThreadBindOnce(std::move(quit_closure)));
  }

  // Emulates API use on threaded WorkletGlobalScope.
  void CountFeature(WebFeature feature,
                    WTF::CrossThreadOnceClosure quit_closure) {
    EXPECT_TRUE(IsCurrentThread());
    GlobalScope()->CountUse(feature);
    PostCrossThreadTask(*GetParentTaskRunnerForTesting(), FROM_HERE,
                        CrossThreadBindOnce(std::move(quit_closure)));
  }

  // Emulates deprecated API use on threaded WorkletGlobalScope.
  void CountDeprecation(WebFeature feature,
                        WTF::CrossThreadOnceClosure quit_closure) {
    EXPECT_TRUE(IsCurrentThread());
    Deprecation::CountDeprecation(GlobalScope(), feature);
    PostCrossThreadTask(*GetParentTaskRunnerForTesting(), FROM_HERE,
                        CrossThreadBindOnce(std::move(quit_closure)));
  }

  void TestTaskRunner(WTF::CrossThreadOnceClosure quit_closure) {
    EXPECT_TRUE(IsCurrentThread());
    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        GlobalScope()->GetTaskRunner(TaskType::kInternalTest);
    EXPECT_TRUE(task_runner->RunsTasksInCurrentSequence());
    PostCrossThreadTask(*GetParentTaskRunnerForTesting(), FROM_HERE,
                        CrossThreadBindOnce(std::move(quit_closure)));
  }

 private:
  WorkerOrWorkletGlobalScope* CreateWorkerGlobalScope(
      std::unique_ptr<GlobalScopeCreationParams> creation_params) final {
    auto* global_scope = MakeGarbageCollected<FakeWorkletGlobalScope>(
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
    std::unique_ptr<Vector<char>> cached_meta_data;
    WorkerClients* worker_clients = nullptr;
    std::unique_ptr<WorkerSettings> worker_settings;
    LocalFrame* frame = To<LocalDOMWindow>(GetExecutionContext())->GetFrame();
    InitializeWorkerThread(
        std::make_unique<GlobalScopeCreationParams>(
            GetExecutionContext()->Url(), mojom::blink::ScriptType::kModule,
            "threaded_worklet", GetExecutionContext()->UserAgent(),
            frame->Loader().UserAgentMetadata(),
            nullptr /* web_worker_fetch_context */,
            mojo::Clone(GetExecutionContext()
                            ->GetContentSecurityPolicy()
                            ->GetParsedPolicies()),
            Vector<network::mojom::blink::ContentSecurityPolicyPtr>(),
            GetExecutionContext()->GetReferrerPolicy(),
            GetExecutionContext()->GetSecurityOrigin(),
            GetExecutionContext()->IsSecureContext(),
            GetExecutionContext()->GetHttpsState(), worker_clients,
            nullptr /* content_settings_client */,
            OriginTrialContext::GetInheritedTrialFeatures(GetExecutionContext())
                .get(),
            base::UnguessableToken::Create(), std::move(worker_settings),
            mojom::blink::V8CacheOptions::kDefault,
            MakeGarbageCollected<WorkletModuleResponsesMap>(),
            mojo::NullRemote() /* browser_interface_broker */,
            frame->Loader().CreateWorkerCodeCacheHost(),
            frame->GetBlobUrlStorePendingRemote(), BeginFrameProviderParams(),
            nullptr /* parent_permissions_policy */,
            GetExecutionContext()->GetAgentClusterID(), ukm::kInvalidSourceId,
            GetExecutionContext()->GetExecutionContextToken()),
        std::nullopt, std::nullopt);
  }

 private:
  friend class ThreadedWorkletTest;
  FRIEND_TEST_ALL_PREFIXES(ThreadedWorkletTest, NestedRunLoopTermination);

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
        WebNavigationParams::CreateWithEmptyHTMLForTesting(url),
        nullptr /* extra_data */);
    blink::test::RunPendingTasks();
    ASSERT_EQ(url.GetString(), GetDocument().Url().GetString());

    messaging_proxy_ =
        MakeGarbageCollected<ThreadedWorkletMessagingProxyForTest>(
            page_->GetFrame().DomWindow());
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

  ExecutionContext* GetExecutionContext() {
    return page_->GetFrame().DomWindow();
  }
  Document& GetDocument() { return page_->GetDocument(); }

  void WaitForReady(WorkerThread* worker_thread) {
    base::WaitableEvent child_waitable;
    PostCrossThreadTask(
        *worker_thread->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
        CrossThreadBindOnce(&base::WaitableEvent::Signal,
                            CrossThreadUnretained(&child_waitable)));

    child_waitable.Wait();
  }

 private:
  test::TaskEnvironment task_environment_;
  std::unique_ptr<DummyPageHolder> page_;
  Persistent<ThreadedWorkletMessagingProxyForTest> messaging_proxy_;
};

TEST_F(ThreadedWorkletTest, SecurityOrigin) {
  base::RunLoop loop;
  MessagingProxy()->Start();

  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBindOnce(&ThreadedWorkletThreadForTest::TestSecurityOrigin,
                          CrossThreadUnretained(GetWorkerThread()),
                          CrossThreadBindOnce(loop.QuitClosure())));
  loop.Run();
}

TEST_F(ThreadedWorkletTest, AgentCluster) {
  base::RunLoop loop;
  MessagingProxy()->Start();

  // The worklet should be in the owner window's agent cluster.
  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBindOnce(&ThreadedWorkletThreadForTest::TestAgentCluster,
                          CrossThreadUnretained(GetWorkerThread()),
                          GetExecutionContext()->GetAgentClusterID(),
                          CrossThreadBindOnce(loop.QuitClosure())));
  loop.Run();
}

TEST_F(ThreadedWorkletTest, ContentSecurityPolicy) {
  base::RunLoop loop;
  // Set up the CSP for Document before starting ThreadedWorklet because
  // ThreadedWorklet inherits the owner Document's CSP.
  auto* csp = MakeGarbageCollected<ContentSecurityPolicy>();
  csp->AddPolicies(ParseContentSecurityPolicies(
      "script-src 'self' https://allowed.example.com",
      network::mojom::ContentSecurityPolicyType::kEnforce,
      network::mojom::ContentSecurityPolicySource::kHTTP,
      *(GetExecutionContext()->GetSecurityOrigin())));
  GetExecutionContext()->SetContentSecurityPolicy(csp);

  MessagingProxy()->Start();

  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBindOnce(
          &ThreadedWorkletThreadForTest::TestContentSecurityPolicy,
          CrossThreadUnretained(GetWorkerThread()),
          CrossThreadBindOnce(loop.QuitClosure())));
  loop.Run();
}

TEST_F(ThreadedWorkletTest, InvalidContentSecurityPolicy) {
  base::RunLoop loop;
  auto* csp = MakeGarbageCollected<ContentSecurityPolicy>();
  csp->AddPolicies(ParseContentSecurityPolicies(
      "invalid-csp", network::mojom::ContentSecurityPolicyType::kEnforce,
      network::mojom::ContentSecurityPolicySource::kHTTP,
      *(GetExecutionContext()->GetSecurityOrigin())));
  GetExecutionContext()->SetContentSecurityPolicy(csp);

  MessagingProxy()->Start();

  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBindOnce(
          &ThreadedWorkletThreadForTest::TestInvalidContentSecurityPolicy,
          CrossThreadUnretained(GetWorkerThread()),
          CrossThreadBindOnce(loop.QuitClosure())));
  loop.Run();
}

TEST_F(ThreadedWorkletTest, UseCounter) {
  Page::InsertOrdinaryPageForTesting(GetDocument().GetPage());
  MessagingProxy()->Start();

  // This feature is randomly selected.
  const WebFeature kFeature1 = WebFeature::kRequestFileSystem;

  // API use on the threaded WorkletGlobalScope should be recorded in UseCounter
  // on the Document.
  EXPECT_FALSE(GetDocument().IsUseCounted(kFeature1));
  {
    base::RunLoop loop;
    PostCrossThreadTask(
        *GetWorkerThread()->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
        CrossThreadBindOnce(&ThreadedWorkletThreadForTest::CountFeature,
                            CrossThreadUnretained(GetWorkerThread()), kFeature1,
                            CrossThreadBindOnce(loop.QuitClosure())));
    loop.Run();
  }
  EXPECT_TRUE(GetDocument().IsUseCounted(kFeature1));

  // API use should be reported to the Document only one time. See comments in
  // ThreadedWorkletObjectProxyForTest::CountFeature.
  {
    base::RunLoop loop;
    PostCrossThreadTask(
        *GetWorkerThread()->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
        CrossThreadBindOnce(&ThreadedWorkletThreadForTest::CountFeature,
                            CrossThreadUnretained(GetWorkerThread()), kFeature1,
                            CrossThreadBindOnce(loop.QuitClosure())));
    loop.Run();
  }

  // This feature is randomly selected from Deprecation::deprecationMessage().
  const WebFeature kFeature2 = WebFeature::kPaymentInstruments;

  // Deprecated API use on the threaded WorkletGlobalScope should be recorded in
  // UseCounter on the Document.
  EXPECT_FALSE(GetDocument().IsUseCounted(kFeature2));
  {
    base::RunLoop loop;
    PostCrossThreadTask(
        *GetWorkerThread()->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
        CrossThreadBindOnce(&ThreadedWorkletThreadForTest::CountDeprecation,
                            CrossThreadUnretained(GetWorkerThread()), kFeature2,
                            CrossThreadBindOnce(loop.QuitClosure())));
    loop.Run();
  }
  EXPECT_TRUE(GetDocument().IsUseCounted(kFeature2));

  // API use should be reported to the Document only one time. See comments in
  // ThreadedWorkletObjectProxyForTest::CountDeprecation.
  {
    base::RunLoop loop;
    PostCrossThreadTask(
        *GetWorkerThread()->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
        CrossThreadBindOnce(&ThreadedWorkletThreadForTest::CountDeprecation,
                            CrossThreadUnretained(GetWorkerThread()), kFeature2,
                            CrossThreadBindOnce(loop.QuitClosure())));
    loop.Run();
  }
}

TEST_F(ThreadedWorkletTest, TaskRunner) {
  MessagingProxy()->Start();

  base::RunLoop loop;
  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBindOnce(&ThreadedWorkletThreadForTest::TestTaskRunner,
                          CrossThreadUnretained(GetWorkerThread()),
                          CrossThreadBindOnce(loop.QuitClosure())));
  loop.Run();
}

TEST_F(ThreadedWorkletTest, NestedRunLoopTermination) {
  base::RunLoop loop;
  MessagingProxy()->Start();

  ThreadedWorkletMessagingProxyForTest* second_messaging_proxy =
      MakeGarbageCollected<ThreadedWorkletMessagingProxyForTest>(
          GetExecutionContext());

  // Get a nested event loop where the first one is on the stack
  // and the second is still alive.
  second_messaging_proxy->Start();

  // Wait until the workers are setup and ready to accept work before we
  // pause them.
  WaitForReady(GetWorkerThread());
  WaitForReady(second_messaging_proxy->GetWorkerThread());

  // Pause the second worker, then the first.
  second_messaging_proxy->GetWorkerThread()->Pause();
  GetWorkerThread()->Pause();

  // Resume then terminate the second worker.
  second_messaging_proxy->GetWorkerThread()->Resume();
  second_messaging_proxy->GetWorkerThread()->Terminate();
  second_messaging_proxy = nullptr;

  // Now resume the first worker.
  GetWorkerThread()->Resume();

  // Make sure execution still works without crashing.
  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBindOnce(&ThreadedWorkletThreadForTest::TestTaskRunner,
                          CrossThreadUnretained(GetWorkerThread()),
                          CrossThreadBindOnce(loop.QuitClosure())));
  loop.Run();
}

}  // namespace blink
