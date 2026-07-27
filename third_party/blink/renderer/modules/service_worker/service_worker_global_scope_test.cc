// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/renderer/core/fetch/fetch_request_data.h"
#include "third_party/blink/renderer/core/inspector/worker_devtools_params.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread_startup_data.h"
#include "third_party/blink/renderer/core/workers/worker_reporting_proxy.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_thread.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

class ServiceWorkerGlobalScopeTest : public testing::Test {
 public:
  ServiceWorkerGlobalScopeTest() = default;

  void SetUp() override {
    reporting_proxy_ = std::make_unique<WorkerReportingProxy>();
    security_origin_ = SecurityOrigin::Create(KURL("https://example.com/"));
    worker_thread_ = base::WrapUnique(new ServiceWorkerThread(
        *reporting_proxy_,
        /*installed_scripts_manager=*/nullptr,
        /*cache_storage_remote=*/mojo::NullRemote(),
        /*parent_thread_default_task_runner=*/
        ThreadScheduler::Current()->CleanupTaskRunner(), ServiceWorkerToken()));
  }

  void TearDown() override {
    if (worker_thread_) {
      worker_thread_->Terminate();
      worker_thread_->WaitForShutdownForTesting();
    }
  }

 protected:
  test::TaskEnvironment task_environment_;
  scoped_refptr<const SecurityOrigin> security_origin_;
  std::unique_ptr<WorkerReportingProxy> reporting_proxy_;
  std::unique_ptr<ServiceWorkerThread> worker_thread_;
};

TEST_F(ServiceWorkerGlobalScopeTest,
       PostRespondWithRaceFetchNetErrorHistogram) {
  const KURL script_url("http://fake.url/");
  worker_thread_->Start(GlobalScopeCreationParams::CreateForWorkerForTesting(
                            security_origin_.get(), script_url),
                        WorkerBackingThreadStartupData::CreateDefault(),
                        std::make_unique<WorkerDevToolsParams>());
  worker_thread_->EvaluateClassicScript(script_url,
                                        "//fake service worker script", nullptr,
                                        v8_inspector::V8StackTraceId());

  base::WaitableEvent completion_event;
  PostCrossThreadTask(
      *worker_thread_->GetWorkerBackingThread().BackingThread().GetTaskRunner(),
      FROM_HERE,
      CrossThreadBindOnce(&base::WaitableEvent::Signal,
                          CrossThreadUnretained(&completion_event)));
  completion_event.Wait();

  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;

  PostCrossThreadTask(
      *worker_thread_->GetWorkerBackingThread().BackingThread().GetTaskRunner(),
      FROM_HERE,
      CrossThreadBindOnce(
          [](WorkerThread* thread, base::RunLoop* run_loop_ptr) {
            auto* global_scope =
                To<ServiceWorkerGlobalScope>(thread->GlobalScope());
            auto* request_data =
                MakeGarbageCollected<FetchRequestData>(global_scope);
            request_data->SetServiceWorkerRaceNetworkRequestToken(
                base::UnguessableToken::Create());

            global_scope->MaybeRecordFetchError(-105, request_data);

            run_loop_ptr->Quit();
          },
          CrossThreadUnretained(worker_thread_.get()),
          CrossThreadUnretained(&run_loop)));

  run_loop.Run();

  histogram_tester.ExpectUniqueSample(
      "ServiceWorker.FetchInFetchHandler.PostRespondWithRaceFetchNetError", 105,
      1);
}

}  // namespace blink
