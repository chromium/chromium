// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/dedicated_worker_test.h"

#include <bitset>
#include <memory>
#include "base/single_thread_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/v8_cache_options.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/sanitize_script_errors.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/inspector/console_message_storage.h"
#include "third_party/blink/renderer/core/inspector/thread_debugger.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/workers/dedicated_worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/dedicated_worker_messaging_proxy.h"
#include "third_party/blink/renderer/core/workers/dedicated_worker_object_proxy.h"
#include "third_party/blink/renderer/core/workers/dedicated_worker_thread.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread_startup_data.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/core/workers/worker_thread_test_helper.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

class DedicatedWorkerThreadForTest final : public DedicatedWorkerThread {
 public:
  DedicatedWorkerThreadForTest(ExecutionContext* parent_execution_context,
                               DedicatedWorkerObjectProxy& worker_object_proxy)
      : DedicatedWorkerThread(parent_execution_context, worker_object_proxy) {
    worker_backing_thread_ = std::make_unique<WorkerBackingThread>(
        ThreadCreationParams(ThreadType::kTestThread));
  }

  WorkerOrWorkletGlobalScope* CreateWorkerGlobalScope(
      std::unique_ptr<GlobalScopeCreationParams> creation_params) override {
    auto* global_scope = DedicatedWorkerGlobalScope::Create(
        std::move(creation_params), this, time_origin_, ukm::kInvalidSourceId);
    // Initializing a global scope with a dummy creation params may emit warning
    // messages (e.g., invalid CSP directives). Clear them here for tests that
    // check console messages (i.e., UseCounter tests).
    GetConsoleMessageStorage()->Clear();
    return global_scope;
  }

  // Emulates API use on DedicatedWorkerGlobalScope.
  void CountFeature(WebFeature feature) {
    EXPECT_TRUE(IsCurrentThread());
    GlobalScope()->CountUse(feature);
    PostCrossThreadTask(*GetParentTaskRunnerForTesting(), FROM_HERE,
                        CrossThreadBindOnce(&test::ExitRunLoop));
  }

  // Emulates deprecated API use on DedicatedWorkerGlobalScope.
  void CountDeprecation(WebFeature feature) {
    EXPECT_TRUE(IsCurrentThread());
    Deprecation::CountDeprecation(GlobalScope(), feature);

    // CountDeprecation() should add a warning message.
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
};

class DedicatedWorkerObjectProxyForTest final
    : public DedicatedWorkerObjectProxy {
 public:
  DedicatedWorkerObjectProxyForTest(
      DedicatedWorkerMessagingProxy* messaging_proxy,
      ParentExecutionContextTaskRunners* parent_execution_context_task_runners)
      : DedicatedWorkerObjectProxy(messaging_proxy,
                                   parent_execution_context_task_runners,
                                   DedicatedWorkerToken()) {}

  void CountFeature(WebFeature feature) override {
    // Any feature should be reported only one time.
    EXPECT_FALSE(reported_features_[static_cast<size_t>(feature)]);
    reported_features_.set(static_cast<size_t>(feature));
    DedicatedWorkerObjectProxy::CountFeature(feature);
  }

 private:
  std::bitset<static_cast<size_t>(WebFeature::kNumberOfFeatures)>
      reported_features_;
};

class DedicatedWorkerMessagingProxyForTest
    : public DedicatedWorkerMessagingProxy {
 public:
  DedicatedWorkerMessagingProxyForTest(ExecutionContext* execution_context)
      : DedicatedWorkerMessagingProxy(execution_context,
                                      nullptr /* worker_object */) {
    // The |worker_object_proxy_| should not have been set in the
    // DedicatedWorkerMessagingProxy constructor as |worker_object| is nullptr.
    DCHECK(!worker_object_proxy_);
    worker_object_proxy_ = std::make_unique<DedicatedWorkerObjectProxyForTest>(
        this, GetParentExecutionContextTaskRunners());
  }

  ~DedicatedWorkerMessagingProxyForTest() override = default;

  void StartWithSourceCode(const String& source) {
    KURL script_url("http://fake.url/");
    security_origin_ = SecurityOrigin::Create(script_url);
    Vector<CSPHeaderAndType> headers{
        {"contentSecurityPolicy",
         network::mojom::ContentSecurityPolicyType::kReport}};
    auto worker_settings = std::make_unique<WorkerSettings>(
        To<LocalDOMWindow>(GetExecutionContext())->GetFrame()->GetSettings());
    auto params = std::make_unique<GlobalScopeCreationParams>(
        script_url, mojom::ScriptType::kClassic, "fake global scope name",
        "fake user agent", UserAgentMetadata(),
        nullptr /* web_worker_fetch_context */, headers,
        network::mojom::ReferrerPolicy::kDefault, security_origin_.get(),
        false /* starter_secure_context */,
        CalculateHttpsState(security_origin_.get()),
        nullptr /* worker_clients */, nullptr /* content_settings_client */,
        network::mojom::IPAddressSpace::kLocal,
        nullptr /* origin_trial_tokens */, base::UnguessableToken::Create(),
        std::move(worker_settings), mojom::blink::V8CacheOptions::kDefault,
        nullptr /* worklet_module_responses_map */);
    params->parent_context_token =
        GetExecutionContext()->GetExecutionContextToken();
    InitializeWorkerThread(
        std::move(params),
        WorkerBackingThreadStartupData(
            WorkerBackingThreadStartupData::HeapLimitMode::kDefault,
            WorkerBackingThreadStartupData::AtomicsWaitMode::kAllow));
    GetWorkerThread()->EvaluateClassicScript(script_url, source,
                                             nullptr /* cached_meta_data */,
                                             v8_inspector::V8StackTraceId());
  }

  DedicatedWorkerThreadForTest* GetDedicatedWorkerThread() {
    return static_cast<DedicatedWorkerThreadForTest*>(GetWorkerThread());
  }

  void Trace(Visitor* visitor) const override {
    DedicatedWorkerMessagingProxy::Trace(visitor);
  }

 private:
  std::unique_ptr<WorkerThread> CreateWorkerThread() override {
    return std::make_unique<DedicatedWorkerThreadForTest>(GetExecutionContext(),
                                                          WorkerObjectProxy());
  }

  scoped_refptr<const SecurityOrigin> security_origin_;
};

void DedicatedWorkerTest::SetUp() {
  PageTestBase::SetUp(IntSize());
  worker_messaging_proxy_ =
      MakeGarbageCollected<DedicatedWorkerMessagingProxyForTest>(
          GetFrame().DomWindow());
}

void DedicatedWorkerTest::TearDown() {
  GetWorkerThread()->TerminateForTesting();
  GetWorkerThread()->WaitForShutdownForTesting();
}

void DedicatedWorkerTest::DispatchMessageEvent() {
  BlinkTransferableMessage message;
  WorkerMessagingProxy()->PostMessageToWorkerGlobalScope(std::move(message));
}

DedicatedWorkerMessagingProxyForTest*
DedicatedWorkerTest::WorkerMessagingProxy() {
  return worker_messaging_proxy_.Get();
}

DedicatedWorkerThreadForTest* DedicatedWorkerTest::GetWorkerThread() {
  return worker_messaging_proxy_->GetDedicatedWorkerThread();
}

void DedicatedWorkerTest::StartWorker(const String& source_code) {
  WorkerMessagingProxy()->StartWithSourceCode(source_code);
}

namespace {

void PostExitRunLoopTaskOnParent(WorkerThread* worker_thread) {
  PostCrossThreadTask(*worker_thread->GetParentTaskRunnerForTesting(),
                      FROM_HERE, CrossThreadBindOnce(&test::ExitRunLoop));
}

}  // anonymous namespace

void DedicatedWorkerTest::WaitUntilWorkerIsRunning() {
  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBindOnce(&PostExitRunLoopTaskOnParent,
                          CrossThreadUnretained(GetWorkerThread())));

  test::EnterRunLoop();
}

TEST_F(DedicatedWorkerTest, PendingActivity_NoActivityAfterContextDestroyed) {
  const String source_code = "// Do nothing";
  StartWorker(source_code);

  EXPECT_TRUE(WorkerMessagingProxy()->HasPendingActivity());

  // Destroying the context should result in no pending activities.
  WorkerMessagingProxy()->TerminateGlobalScope();
  EXPECT_FALSE(WorkerMessagingProxy()->HasPendingActivity());
}

TEST_F(DedicatedWorkerTest, UseCounter) {
  Page::InsertOrdinaryPageForTesting(&GetPage());
  const String source_code = "// Do nothing";
  StartWorker(source_code);

  // This feature is randomly selected.
  const WebFeature kFeature1 = WebFeature::kRequestFileSystem;

  // API use on the DedicatedWorkerGlobalScope should be recorded in UseCounter
  // on the Document.
  EXPECT_FALSE(GetDocument().IsUseCounted(kFeature1));
  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBindOnce(&DedicatedWorkerThreadForTest::CountFeature,
                          CrossThreadUnretained(GetWorkerThread()), kFeature1));
  test::EnterRunLoop();
  EXPECT_TRUE(GetDocument().IsUseCounted(kFeature1));

  // API use should be reported to the Document only one time. See comments in
  // DedicatedWorkerObjectProxyForTest::CountFeature.
  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBindOnce(&DedicatedWorkerThreadForTest::CountFeature,
                          CrossThreadUnretained(GetWorkerThread()), kFeature1));
  test::EnterRunLoop();

  // This feature is randomly selected from Deprecation::deprecationMessage().
  const WebFeature kFeature2 = WebFeature::kPrefixedStorageInfo;

  // Deprecated API use on the DedicatedWorkerGlobalScope should be recorded in
  // UseCounter on the Document.
  EXPECT_FALSE(GetDocument().IsUseCounted(kFeature2));
  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBindOnce(&DedicatedWorkerThreadForTest::CountDeprecation,
                          CrossThreadUnretained(GetWorkerThread()), kFeature2));
  test::EnterRunLoop();
  EXPECT_TRUE(GetDocument().IsUseCounted(kFeature2));

  // API use should be reported to the Document only one time. See comments in
  // DedicatedWorkerObjectProxyForTest::CountDeprecation.
  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBindOnce(&DedicatedWorkerThreadForTest::CountDeprecation,
                          CrossThreadUnretained(GetWorkerThread()), kFeature2));
  test::EnterRunLoop();
}

TEST_F(DedicatedWorkerTest, TaskRunner) {
  const String source_code = "// Do nothing";
  StartWorker(source_code);

  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBindOnce(&DedicatedWorkerThreadForTest::TestTaskRunner,
                          CrossThreadUnretained(GetWorkerThread())));
  test::EnterRunLoop();
}

}  // namespace blink
