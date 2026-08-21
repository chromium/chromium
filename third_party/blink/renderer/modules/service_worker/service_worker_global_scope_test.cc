// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/metrics/histogram_tester.h"
#include "net/base/net_errors.h"
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
       PostRespondWithRaceFetchNetErrorHistogram_RaceLoaderUsed) {
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
            const base::UnguessableToken token =
                base::UnguessableToken::Create();
            request_data->SetServiceWorkerRaceNetworkRequestToken(token);

            mojo::PendingRemote<network::mojom::blink::URLLoaderFactory>
                pending_remote;
            std::ignore = pending_remote.InitWithNewPipeAndPassReceiver();
            global_scope->InsertNewItemToRaceNetworkRequestsForTesting(
                /*fetch_event_id=*/1, token, std::move(pending_remote),
                KURL("https://example.com/"));
            auto result =
                global_scope->FindRaceNetworkRequestURLLoaderFactory(token);
            EXPECT_TRUE(result.has_value());

            global_scope->MaybeRecordFetchError(-105, request_data);

            run_loop_ptr->Quit();
          },
          CrossThreadUnretained(worker_thread_.get()),
          CrossThreadUnretained(&run_loop)));

  run_loop.Run();

  histogram_tester.ExpectUniqueSample(
      "ServiceWorker.FetchInFetchHandler.PostRespondWithRaceFetchNetError", 105,
      1);
  histogram_tester.ExpectUniqueSample(
      "ServiceWorker.FetchInFetchHandler.PostRespondWithRaceURLLoaderNetError",
      105, 1);
  histogram_tester.ExpectUniqueSample(
      "ServiceWorker.FetchInFetchHandler.RaceFetchNetError", 105, 1);
  histogram_tester.ExpectUniqueSample(
      "ServiceWorker.FetchInFetchHandler.RaceURLLoaderNetError", 105, 1);
  histogram_tester.ExpectUniqueSample(
      "ServiceWorker.FetchInFetchHandler.RaceNetworkRequestLoaderState",
      ServiceWorkerRaceNetworkRequestLoaderState::kRaceLoaderUsed, 1);
  histogram_tester.ExpectUniqueSample(
      "ServiceWorker.FetchInFetchHandler.RaceNetworkRequestLoaderState.Failure",
      ServiceWorkerRaceNetworkRequestLoaderState::kRaceLoaderUsed, 1);
  histogram_tester.ExpectTotalCount(
      "ServiceWorker.FetchInFetchHandler.RaceNetworkRequestLoaderState.Success",
      0);
}

TEST_F(ServiceWorkerGlobalScopeTest,
       PostRespondWithRaceFetchNetErrorHistogram_Fallback) {
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
            const base::UnguessableToken token =
                base::UnguessableToken::Create();
            request_data->SetServiceWorkerRaceNetworkRequestToken(token);

            mojo::PendingRemote<network::mojom::blink::URLLoaderFactory>
                pending_remote;
            std::ignore = pending_remote.InitWithNewPipeAndPassReceiver();
            global_scope->InsertNewItemToRaceNetworkRequestsForTesting(
                /*fetch_event_id=*/1, token, std::move(pending_remote),
                KURL("https://example.com/"));
            global_scope->OnRaceNetworkRequestDisconnectedForTesting(token);
            auto result =
                global_scope->FindRaceNetworkRequestURLLoaderFactory(token);
            EXPECT_FALSE(result.has_value());

            global_scope->MaybeRecordFetchError(-105, request_data);

            run_loop_ptr->Quit();
          },
          CrossThreadUnretained(worker_thread_.get()),
          CrossThreadUnretained(&run_loop)));

  run_loop.Run();

  histogram_tester.ExpectUniqueSample(
      "ServiceWorker.FetchInFetchHandler.PostRespondWithRaceFetchNetError", 105,
      1);
  histogram_tester.ExpectTotalCount(
      "ServiceWorker.FetchInFetchHandler.PostRespondWithRaceURLLoaderNetError",
      0);
  histogram_tester.ExpectUniqueSample(
      "ServiceWorker.FetchInFetchHandler.RaceFetchNetError", 105, 1);
  histogram_tester.ExpectTotalCount(
      "ServiceWorker.FetchInFetchHandler.RaceURLLoaderNetError", 0);
  histogram_tester.ExpectUniqueSample(
      "ServiceWorker.FetchInFetchHandler.RaceNetworkRequestLoaderState",
      ServiceWorkerRaceNetworkRequestLoaderState::kFallbackDueToDisconnect, 1);
  histogram_tester.ExpectUniqueSample(
      "ServiceWorker.FetchInFetchHandler.RaceNetworkRequestLoaderState.Failure",
      ServiceWorkerRaceNetworkRequestLoaderState::kFallbackDueToDisconnect, 1);
  histogram_tester.ExpectTotalCount(
      "ServiceWorker.FetchInFetchHandler.RaceNetworkRequestLoaderState.Success",
      0);
}

TEST_F(ServiceWorkerGlobalScopeTest, RaceNetworkRequestLoaderStateTransitions) {
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
            const base::UnguessableToken token =
                base::UnguessableToken::Create();

            // 0. Invalid / empty token -> kInvalidToken.
            EXPECT_EQ(ServiceWorkerRaceNetworkRequestLoaderState::kInvalidToken,
                      global_scope->GetRaceNetworkRequestLoaderState(
                          base::UnguessableToken::Null()));

            // 1. Initial state for unregistered token -> kNotFound.
            EXPECT_EQ(ServiceWorkerRaceNetworkRequestLoaderState::kNotFound,
                      global_scope->GetRaceNetworkRequestLoaderState(token));

            // 2. Find for unregistered token -> kFallbackDueToNoFactory.
            auto result =
                global_scope->FindRaceNetworkRequestURLLoaderFactory(token);
            EXPECT_FALSE(result.has_value());
            EXPECT_EQ(ServiceWorkerRaceNetworkRequestLoaderState::
                          kFallbackDueToNoFactory,
                      global_scope->GetRaceNetworkRequestLoaderState(token));

            // 3. Register a new token -> kNotInitiated.
            const base::UnguessableToken token2 =
                base::UnguessableToken::Create();
            mojo::PendingRemote<network::mojom::blink::URLLoaderFactory>
                pending_remote;
            std::ignore = pending_remote.InitWithNewPipeAndPassReceiver();
            global_scope->InsertNewItemToRaceNetworkRequestsForTesting(
                /*fetch_event_id=*/1, token2, std::move(pending_remote),
                KURL("https://example.com/"));
            EXPECT_EQ(ServiceWorkerRaceNetworkRequestLoaderState::kNotInitiated,
                      global_scope->GetRaceNetworkRequestLoaderState(token2));

            // 4. Find valid factory -> kRaceLoaderUsed.
            auto result2 =
                global_scope->FindRaceNetworkRequestURLLoaderFactory(token2);
            EXPECT_TRUE(result2.has_value());
            EXPECT_EQ(
                ServiceWorkerRaceNetworkRequestLoaderState::kRaceLoaderUsed,
                global_scope->GetRaceNetworkRequestLoaderState(token2));

            // 5. Second Find for consumed token ->
            // kFallbackDueToAlreadyConsumed.
            auto result3 =
                global_scope->FindRaceNetworkRequestURLLoaderFactory(token2);
            EXPECT_FALSE(result3.has_value());
            EXPECT_EQ(ServiceWorkerRaceNetworkRequestLoaderState::
                          kFallbackDueToAlreadyConsumed,
                      global_scope->GetRaceNetworkRequestLoaderState(token2));

            // 6. MaybeRecordFetchError cleans up the loader state entry ->
            // kNotFound.
            auto* request_data =
                MakeGarbageCollected<FetchRequestData>(global_scope);
            request_data->SetServiceWorkerRaceNetworkRequestToken(token2);
            global_scope->MaybeRecordFetchError(-105, request_data);
            EXPECT_EQ(ServiceWorkerRaceNetworkRequestLoaderState::kNotFound,
                      global_scope->GetRaceNetworkRequestLoaderState(token2));

            // 7. RemoveItemFromRaceNetworkRequests cleans up kNotInitiated
            // token -> kNotFound.
            const base::UnguessableToken token3 =
                base::UnguessableToken::Create();
            mojo::PendingRemote<network::mojom::blink::URLLoaderFactory>
                pending_remote3;
            std::ignore = pending_remote3.InitWithNewPipeAndPassReceiver();
            global_scope->InsertNewItemToRaceNetworkRequestsForTesting(
                /*fetch_event_id=*/2, token3, std::move(pending_remote3),
                KURL("https://example.com/"));
            EXPECT_EQ(ServiceWorkerRaceNetworkRequestLoaderState::kNotInitiated,
                      global_scope->GetRaceNetworkRequestLoaderState(token3));
            global_scope->RemoveItemFromRaceNetworkRequestsForTesting(
                /*fetch_event_id=*/2);
            EXPECT_EQ(ServiceWorkerRaceNetworkRequestLoaderState::kNotFound,
                      global_scope->GetRaceNetworkRequestLoaderState(token3));

            // 8. MaybeRecordFetchError(net::OK, request_data) on success cleans
            // up the loader state entry -> kNotFound.
            const base::UnguessableToken token4 =
                base::UnguessableToken::Create();
            mojo::PendingRemote<network::mojom::blink::URLLoaderFactory>
                pending_remote4;
            std::ignore = pending_remote4.InitWithNewPipeAndPassReceiver();
            global_scope->InsertNewItemToRaceNetworkRequestsForTesting(
                /*fetch_event_id=*/3, token4, std::move(pending_remote4),
                KURL("https://example.com/"));
            auto result4 =
                global_scope->FindRaceNetworkRequestURLLoaderFactory(token4);
            EXPECT_TRUE(result4.has_value());
            EXPECT_EQ(
                ServiceWorkerRaceNetworkRequestLoaderState::kRaceLoaderUsed,
                global_scope->GetRaceNetworkRequestLoaderState(token4));

            auto* request_data4 =
                MakeGarbageCollected<FetchRequestData>(global_scope);
            request_data4->SetServiceWorkerRaceNetworkRequestToken(token4);
            global_scope->MaybeRecordFetchError(net::OK, request_data4);
            EXPECT_EQ(ServiceWorkerRaceNetworkRequestLoaderState::kNotFound,
                      global_scope->GetRaceNetworkRequestLoaderState(token4));

            // 9. Disconnect -> Find sets kFallbackDueToDisconnect ->
            // MaybeRecordFetchError cleans up -> kNotFound.
            const base::UnguessableToken token5 =
                base::UnguessableToken::Create();
            mojo::PendingRemote<network::mojom::blink::URLLoaderFactory>
                pending_remote5;
            std::ignore = pending_remote5.InitWithNewPipeAndPassReceiver();
            global_scope->InsertNewItemToRaceNetworkRequestsForTesting(
                /*fetch_event_id=*/4, token5, std::move(pending_remote5),
                KURL("https://example.com/"));
            global_scope->OnRaceNetworkRequestDisconnectedForTesting(token5);
            auto result5 =
                global_scope->FindRaceNetworkRequestURLLoaderFactory(token5);
            EXPECT_FALSE(result5.has_value());
            EXPECT_EQ(ServiceWorkerRaceNetworkRequestLoaderState::
                          kFallbackDueToDisconnect,
                      global_scope->GetRaceNetworkRequestLoaderState(token5));

            auto* request_data5 =
                MakeGarbageCollected<FetchRequestData>(global_scope);
            request_data5->SetServiceWorkerRaceNetworkRequestToken(token5);
            global_scope->MaybeRecordFetchError(-105, request_data5);
            EXPECT_EQ(ServiceWorkerRaceNetworkRequestLoaderState::kNotFound,
                      global_scope->GetRaceNetworkRequestLoaderState(token5));

            // 10. Disconnect -> NO fetch called ->
            // RemoveItemFromRaceNetworkRequests cleans up -> kNotFound.
            const base::UnguessableToken token6 =
                base::UnguessableToken::Create();
            mojo::PendingRemote<network::mojom::blink::URLLoaderFactory>
                pending_remote6;
            std::ignore = pending_remote6.InitWithNewPipeAndPassReceiver();
            global_scope->InsertNewItemToRaceNetworkRequestsForTesting(
                /*fetch_event_id=*/5, token6, std::move(pending_remote6),
                KURL("https://example.com/"));
            global_scope->OnRaceNetworkRequestDisconnectedForTesting(token6);
            global_scope->RemoveItemFromRaceNetworkRequestsForTesting(
                /*fetch_event_id=*/5);
            EXPECT_EQ(ServiceWorkerRaceNetworkRequestLoaderState::kNotFound,
                      global_scope->GetRaceNetworkRequestLoaderState(token6));

            run_loop_ptr->Quit();
          },
          CrossThreadUnretained(worker_thread_.get()),
          CrossThreadUnretained(&run_loop)));

  run_loop.Run();

  histogram_tester.ExpectBucketCount(
      "ServiceWorker.FetchInFetchHandler.RaceNetworkRequestLoaderState",
      ServiceWorkerRaceNetworkRequestLoaderState::kFallbackDueToNoFactory, 1);
  histogram_tester.ExpectBucketCount(
      "ServiceWorker.FetchInFetchHandler.RaceNetworkRequestLoaderState",
      ServiceWorkerRaceNetworkRequestLoaderState::kRaceLoaderUsed, 2);
  histogram_tester.ExpectBucketCount(
      "ServiceWorker.FetchInFetchHandler.RaceNetworkRequestLoaderState",
      ServiceWorkerRaceNetworkRequestLoaderState::kFallbackDueToAlreadyConsumed,
      1);
  histogram_tester.ExpectBucketCount(
      "ServiceWorker.FetchInFetchHandler.RaceNetworkRequestLoaderState",
      ServiceWorkerRaceNetworkRequestLoaderState::kFallbackDueToDisconnect, 1);

  histogram_tester.ExpectUniqueSample(
      "ServiceWorker.FetchInFetchHandler.RaceNetworkRequestLoaderState.Success",
      ServiceWorkerRaceNetworkRequestLoaderState::kRaceLoaderUsed, 1);

  histogram_tester.ExpectBucketCount(
      "ServiceWorker.FetchInFetchHandler.RaceNetworkRequestLoaderState.Failure",
      ServiceWorkerRaceNetworkRequestLoaderState::kFallbackDueToAlreadyConsumed,
      1);
  histogram_tester.ExpectBucketCount(
      "ServiceWorker.FetchInFetchHandler.RaceNetworkRequestLoaderState.Failure",
      ServiceWorkerRaceNetworkRequestLoaderState::kFallbackDueToDisconnect, 1);

  histogram_tester.ExpectBucketCount(
      "ServiceWorker.FetchInFetchHandler.RaceFetchNetError", 0, 1);
  histogram_tester.ExpectBucketCount(
      "ServiceWorker.FetchInFetchHandler.RaceFetchNetError", 105, 2);
  histogram_tester.ExpectUniqueSample(
      "ServiceWorker.FetchInFetchHandler.RaceURLLoaderNetError", 0, 1);
}

}  // namespace blink
