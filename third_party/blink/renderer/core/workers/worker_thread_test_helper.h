// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_WORKER_THREAD_TEST_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_WORKER_THREAD_TEST_HELPER_H_

#include <memory>

#include "base/macros.h"
#include "base/synchronization/waitable_event.h"
#include "services/network/public/mojom/ip_address_space.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_cache_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/worker_devtools_params.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/parent_execution_context_task_runners.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread_startup_data.h"
#include "third_party/blink/renderer/core/workers/worker_clients.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_reporting_proxy.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/network/content_security_policy_parsers.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8.h"

namespace blink {

class FakeWorkerGlobalScope : public WorkerGlobalScope {
 public:
  FakeWorkerGlobalScope(
      std::unique_ptr<GlobalScopeCreationParams> creation_params,
      WorkerThread* thread)
      : WorkerGlobalScope(std::move(creation_params),
                          thread,
                          base::TimeTicks::Now()) {
    ReadyToRunWorkerScript();
  }

  ~FakeWorkerGlobalScope() override = default;

  // EventTarget
  const AtomicString& InterfaceName() const override {
    return event_target_names::kDedicatedWorkerGlobalScope;
  }

  // WorkerGlobalScope
  void Initialize(const KURL& response_url,
                  network::mojom::ReferrerPolicy response_referrer_policy,
                  network::mojom::IPAddressSpace response_address_space,
                  const Vector<CSPHeaderAndType>& response_csp_headers,
                  const Vector<String>* response_origin_trial_tokens,
                  int64_t appcache_id) override {
    InitializeURL(response_url);
    SetReferrerPolicy(response_referrer_policy);
    SetAddressSpace(response_address_space);

    // These should be called after SetAddressSpace() to correctly override the
    // address space by the "treat-as-public-address" CSP directive.
    InitContentSecurityPolicyFromVector(response_csp_headers);
    BindContentSecurityPolicyToExecutionContext();

    OriginTrialContext::AddTokens(this, response_origin_trial_tokens);

    // This should be called after OriginTrialContext::AddTokens() to install
    // origin trial features in JavaScript's global object.
    ScriptController()->PrepareForEvaluation();
  }
  void FetchAndRunClassicScript(
      const KURL& script_url,
      const FetchClientSettingsObjectSnapshot& outside_settings_object,
      WorkerResourceTimingNotifier& outside_resource_timing_notifier,
      const v8_inspector::V8StackTraceId& stack_id) override {
    NOTREACHED();
  }
  void FetchAndRunModuleScript(
      const KURL& module_url_record,
      const FetchClientSettingsObjectSnapshot& outside_settings_object,
      WorkerResourceTimingNotifier& outside_resource_timing_notifier,
      network::mojom::CredentialsMode) override {
    NOTREACHED();
  }

  void ExceptionThrown(ErrorEvent*) override {}
};

class WorkerThreadForTest : public WorkerThread {
 public:
  explicit WorkerThreadForTest(
      WorkerReportingProxy& mock_worker_reporting_proxy)
      : WorkerThread(mock_worker_reporting_proxy),
        worker_backing_thread_(std::make_unique<WorkerBackingThread>(
            ThreadCreationParams(ThreadType::kTestThread))) {}

  ~WorkerThreadForTest() override = default;

  WorkerBackingThread& GetWorkerBackingThread() override {
    return *worker_backing_thread_;
  }
  void ClearWorkerBackingThread() override { worker_backing_thread_.reset(); }

  void StartWithSourceCode(
      const SecurityOrigin* security_origin,
      const String& source,
      const KURL& script_url = KURL("http://fake.url/"),
      WorkerClients* worker_clients = nullptr) {
    Vector<CSPHeaderAndType> headers{
        {"contentSecurityPolicy", kContentSecurityPolicyHeaderTypeReport}};
    auto creation_params = std::make_unique<GlobalScopeCreationParams>(
        script_url, mojom::ScriptType::kClassic,
        OffMainThreadWorkerScriptFetchOption::kDisabled,
        "fake global scope name", "fake user agent",
        nullptr /* web_worker_fetch_context */, headers,
        network::mojom::ReferrerPolicy::kDefault, security_origin,
        false /* starter_secure_context */,
        CalculateHttpsState(security_origin), worker_clients,
        nullptr /* content_settings_client */,
        network::mojom::IPAddressSpace::kLocal, nullptr,
        base::UnguessableToken::Create(),
        std::make_unique<WorkerSettings>(std::make_unique<Settings>().get()),
        kV8CacheOptionsDefault, nullptr /* worklet_module_responses_map */);

    Start(std::move(creation_params),
          WorkerBackingThreadStartupData::CreateDefault(),
          std::make_unique<WorkerDevToolsParams>());
    EvaluateClassicScript(script_url, source, nullptr /* cached_meta_data */,
                          v8_inspector::V8StackTraceId());
  }

  void WaitForInit() {
    base::WaitableEvent completion_event;
    PostCrossThreadTask(
        *GetWorkerBackingThread().BackingThread().GetTaskRunner(), FROM_HERE,
        CrossThreadBindOnce(&base::WaitableEvent::Signal,
                            CrossThreadUnretained(&completion_event)));
    completion_event.Wait();
  }

 protected:
  WorkerOrWorkletGlobalScope* CreateWorkerGlobalScope(
      std::unique_ptr<GlobalScopeCreationParams> creation_params) override {
    return MakeGarbageCollected<FakeWorkerGlobalScope>(
        std::move(creation_params), this);
  }

 private:
  ThreadType GetThreadType() const override {
    return ThreadType::kUnspecifiedWorkerThread;
  }

  std::unique_ptr<WorkerBackingThread> worker_backing_thread_;
};

class MockWorkerReportingProxy final : public WorkerReportingProxy {
 public:
  MockWorkerReportingProxy() = default;
  ~MockWorkerReportingProxy() override = default;

  MOCK_METHOD1(DidCreateWorkerGlobalScope, void(WorkerOrWorkletGlobalScope*));
  MOCK_METHOD2(WillEvaluateClassicScriptMock,
               void(size_t scriptSize, size_t cachedMetadataSize));
  MOCK_METHOD1(DidEvaluateClassicScript, void(bool success));
  MOCK_METHOD0(DidCloseWorkerGlobalScope, void());
  MOCK_METHOD0(WillDestroyWorkerGlobalScope, void());
  MOCK_METHOD0(DidTerminateWorkerThread, void());

  void WillEvaluateClassicScript(size_t script_size,
                                 size_t cached_metadata_size) override {
    script_evaluation_event_.Signal();
    WillEvaluateClassicScriptMock(script_size, cached_metadata_size);
  }

  void WaitUntilScriptEvaluation() { script_evaluation_event_.Wait(); }

 private:
  base::WaitableEvent script_evaluation_event_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_WORKER_THREAD_TEST_HELPER_H_
