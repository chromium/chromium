// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_response.mojom-blink.h"
#include "third_party/blink/public/mojom/service_worker/dispatch_fetch_event_params.mojom-blink.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_fetch_response_callback.mojom-blink.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_stream_handle.mojom-blink.h"
#include "third_party/blink/renderer/core/fetch/fetch_request_data.h"
#include "third_party/blink/renderer/core/inspector/worker_devtools_params.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread_startup_data.h"
#include "third_party/blink/renderer/core/workers/worker_reporting_proxy.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_thread.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "v8/include/cppgc/testing.h"

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

class ServiceWorkerGlobalScopeAbortedErrorMessageTest
    : public ServiceWorkerGlobalScopeTest,
      public testing::WithParamInterface<bool> {
 public:
  ServiceWorkerGlobalScopeAbortedErrorMessageTest() {
    feature_list_.InitWithFeatureState(
        features::kServiceWorkerRaceNetworkRequestAbortedErrorMessage,
        IsCustomMessageEnabled());
  }

  bool IsCustomMessageEnabled() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Verifies that `GetCustomFetchErrorMessage()` returns the custom aborted-race
// TypeError message only when `kRaceLoaderUsed` + `net::ERR_ABORTED` + feature
// flag enabled (before `MaybeRecordFetchError()` erases the loader state), and
// returns `std::nullopt` for fallback loaders, non-ERR_ABORTED errors, or
// requests without a race token.
TEST_P(ServiceWorkerGlobalScopeAbortedErrorMessageTest,
       GetCustomFetchErrorMessage) {
  const KURL script_url("http://fake.url/");
  worker_thread_->Start(GlobalScopeCreationParams::CreateForWorkerForTesting(
                            security_origin_.get(), script_url),
                        WorkerBackingThreadStartupData::CreateDefault(),
                        std::make_unique<WorkerDevToolsParams>());

  base::WaitableEvent completion_event;
  PostCrossThreadTask(
      *worker_thread_->GetWorkerBackingThread().BackingThread().GetTaskRunner(),
      FROM_HERE,
      CrossThreadBindOnce(&base::WaitableEvent::Signal,
                          CrossThreadUnretained(&completion_event)));
  completion_event.Wait();

  const bool expect_custom = IsCustomMessageEnabled();
  base::RunLoop run_loop;

  PostCrossThreadTask(
      *worker_thread_->GetWorkerBackingThread().BackingThread().GetTaskRunner(),
      FROM_HERE,
      CrossThreadBindOnce(
          [](WorkerThread* thread, bool expect_custom,
             base::RunLoop* run_loop_ptr) {
            auto* global_scope =
                To<ServiceWorkerGlobalScope>(thread->GlobalScope());

            auto register_and_consume = [&](int fetch_event_id) {
              const base::UnguessableToken token =
                  base::UnguessableToken::Create();
              mojo::PendingRemote<network::mojom::blink::URLLoaderFactory>
                  pending_remote;
              std::ignore = pending_remote.InitWithNewPipeAndPassReceiver();
              global_scope->InsertNewItemToRaceNetworkRequestsForTesting(
                  fetch_event_id, token, std::move(pending_remote),
                  KURL("https://example.com/"));
              EXPECT_TRUE(
                  global_scope->FindRaceNetworkRequestURLLoaderFactory(token)
                      .has_value());
              auto* request_data =
                  MakeGarbageCollected<FetchRequestData>(global_scope);
              request_data->SetServiceWorkerRaceNetworkRequestToken(token);
              return std::make_pair(token, request_data);
            };

            // 1. kRaceLoaderUsed + ERR_ABORTED before MaybeRecordFetchError()
            // erases the token's loader state.
            auto [token1, req1] = register_and_consume(/*fetch_event_id=*/1);
            std::optional<String> msg1 =
                global_scope->GetCustomFetchErrorMessage(net::ERR_ABORTED,
                                                         req1);
            if (expect_custom) {
              ASSERT_TRUE(msg1.has_value());
              EXPECT_EQ(
                  "The service worker race-network-and-fetch-handler request "
                  "was cancelled before 'fetch()' settled.",
                  *msg1);
            } else {
              EXPECT_FALSE(msg1.has_value());
            }
            global_scope->MaybeRecordFetchError(net::ERR_ABORTED, req1);
            EXPECT_EQ(ServiceWorkerRaceNetworkRequestLoaderState::kNotFound,
                      global_scope->GetRaceNetworkRequestLoaderState(token1));
            EXPECT_FALSE(
                global_scope->GetCustomFetchErrorMessage(net::ERR_ABORTED, req1)
                    .has_value());

            // 2. kRaceLoaderUsed + non-ERR_ABORTED error.
            auto [token2, req2] = register_and_consume(/*fetch_event_id=*/2);
            EXPECT_FALSE(global_scope
                             ->GetCustomFetchErrorMessage(
                                 net::ERR_NAME_NOT_RESOLVED, req2)
                             .has_value());
            global_scope->MaybeRecordFetchError(net::ERR_NAME_NOT_RESOLVED,
                                                req2);

            // 3. kFallbackDueToAlreadyConsumed (second fetch(event.request)) +
            // ERR_ABORTED.
            auto [token3, req3] = register_and_consume(/*fetch_event_id=*/3);
            EXPECT_FALSE(
                global_scope->FindRaceNetworkRequestURLLoaderFactory(token3)
                    .has_value());
            EXPECT_FALSE(
                global_scope->GetCustomFetchErrorMessage(net::ERR_ABORTED, req3)
                    .has_value());
            global_scope->MaybeRecordFetchError(net::ERR_ABORTED, req3);

            // 4. Request without a race network request token + ERR_ABORTED.
            auto* untokenized_req =
                MakeGarbageCollected<FetchRequestData>(global_scope);
            EXPECT_FALSE(global_scope
                             ->GetCustomFetchErrorMessage(net::ERR_ABORTED,
                                                          untokenized_req)
                             .has_value());

            run_loop_ptr->Quit();
          },
          CrossThreadUnretained(worker_thread_.get()), expect_custom,
          CrossThreadUnretained(&run_loop)));

  run_loop.Run();
}

INSTANTIATE_TEST_SUITE_P(All,
                         ServiceWorkerGlobalScopeAbortedErrorMessageTest,
                         testing::Bool());

namespace {

// Mojo response receiver that records fallback invocations and signals
// completion when either `OnFallback()` arrives or the remote pipe is reset
// prematurely during Oilpan garbage collection.
class TestFetchResponseCallback
    : public mojom::blink::ServiceWorkerFetchResponseCallback {
 public:
  explicit TestFetchResponseCallback(base::RepeatingClosure on_done_closure)
      : on_done_closure_(std::move(on_done_closure)) {}

  mojo::PendingRemote<mojom::blink::ServiceWorkerFetchResponseCallback>
  BindNewPipeAndPassRemote() {
    auto remote = receiver_.BindNewPipeAndPassRemote();
    // Reset the receiver and quit the run loop if the remote disconnects
    // without sending a response (which happens when the HeapMojoRemote wrapper
    // is collected by GC before OnFallback() is invoked).
    receiver_.set_disconnect_handler(base::BindOnce(
        &TestFetchResponseCallback::OnDisconnect, base::Unretained(this)));
    return remote;
  }

  int fallback_count() const { return fallback_count_; }
  int request_body_count() const { return request_body_count_; }

  // mojom::blink::ServiceWorkerFetchResponseCallback:
  void OnResponse(
      mojom::blink::FetchAPIResponsePtr response,
      mojom::blink::ServiceWorkerFetchEventTimingPtr timing,
      mojom::blink::ServiceWorkerFetchHandlerErrorsPtr errors) override {}
  void OnResponseStream(
      mojom::blink::FetchAPIResponsePtr response,
      mojom::blink::ServiceWorkerStreamHandlePtr body_as_stream,
      mojom::blink::ServiceWorkerFetchEventTimingPtr timing,
      mojom::blink::ServiceWorkerFetchHandlerErrorsPtr errors) override {}
  void OnFallback(
      std::optional<network::DataElementChunkedDataPipe> request_body,
      mojom::blink::ServiceWorkerFetchEventTimingPtr timing,
      mojom::blink::ServiceWorkerFetchHandlerErrorsPtr errors) override {
    ++fallback_count_;
    if (request_body.has_value()) {
      ++request_body_count_;
    }
    receiver_.reset();
    if (on_done_closure_) {
      on_done_closure_.Run();
    }
  }

 private:
  void OnDisconnect() {
    receiver_.reset();
    if (on_done_closure_) {
      on_done_closure_.Run();
    }
  }

  mojo::Receiver<mojom::blink::ServiceWorkerFetchResponseCallback> receiver_{
      this};
  base::RepeatingClosure on_done_closure_;
  int fallback_count_ = 0;
  int request_body_count_ = 0;
};

class ServiceWorkerGlobalScopeGCTest : public ServiceWorkerGlobalScopeTest {
 public:
  void SetUp() override {
    // Configure `--cppgc-random-gc-interval` before `ServiceWorkerThread`
    // construction so the worker backing thread's `CppHeap` initializes
    // `allocation_timeout_` on thread attachment.
    v8::V8::SetFlagsFromString(
        "--fuzzer-random-seed=42 --cppgc-random-gc-interval=50");
    ServiceWorkerGlobalScopeTest::SetUp();
  }

  void TearDown() override {
    ServiceWorkerGlobalScopeTest::TearDown();
    v8::V8::SetFlagsFromString("--cppgc-random-gc-interval=0");
  }
};

}  // namespace

// Verifies that `RespondToFetchEventWithNoResponse` retains the
// `HeapMojoRemote` response callback wrapper in `fetch_response_callbacks_`
// across the Oilpan hash table allocation in
// `pending_streaming_upload_fetch_events_.insert(...)` when a garbage
// collection is triggered on allocation (crbug.com/565681452).
// TODO(crbug.com/586910800): Re-enable once TSan and Fuchsia timeouts are
// fixed.
#if defined(THREAD_SANITIZER) || BUILDFLAG(IS_FUCHSIA)
#define MAYBE_RespondToFetchEventWithNoResponse_StreamingUploadGC \
  DISABLED_RespondToFetchEventWithNoResponse_StreamingUploadGC
#else
#define MAYBE_RespondToFetchEventWithNoResponse_StreamingUploadGC \
  RespondToFetchEventWithNoResponse_StreamingUploadGC
#endif
TEST_F(ServiceWorkerGlobalScopeGCTest,
       MAYBE_RespondToFetchEventWithNoResponse_StreamingUploadGC) {
  const KURL script_url("http://fake.url/");
  auto creation_params = GlobalScopeCreationParams::CreateForWorkerForTesting(
      security_origin_.get(), script_url);
  std::ignore = creation_params->browser_interface_broker
                    .InitWithNewPipeAndPassReceiver();
  worker_thread_->Start(std::move(creation_params),
                        WorkerBackingThreadStartupData::CreateDefault(),
                        std::make_unique<WorkerDevToolsParams>());

  base::WaitableEvent completion_event;
  PostCrossThreadTask(
      *worker_thread_->GetWorkerBackingThread().BackingThread().GetTaskRunner(),
      FROM_HERE,
      CrossThreadBindOnce(&base::WaitableEvent::Signal,
                          CrossThreadUnretained(&completion_event)));
  completion_event.Wait();

  base::RunLoop run_loop;
  TestFetchResponseCallback response_callback(run_loop.QuitClosure());
  mojo::PendingRemote<mojom::blink::ServiceWorkerFetchResponseCallback>
      response_callback_remote = response_callback.BindNewPipeAndPassRemote();

  PostCrossThreadTask(
      *worker_thread_->GetWorkerBackingThread().BackingThread().GetTaskRunner(),
      FROM_HERE,
      CrossThreadBindOnce(
          [](WorkerThread* thread,
             mojo::PendingRemote<
                 mojom::blink::ServiceWorkerFetchResponseCallback>
                 response_callback_remote) {
            auto* global_scope =
                To<ServiceWorkerGlobalScope>(thread->GlobalScope());
            global_scope->DeferPrepareForEvaluation();
            global_scope->RunDeferredPrepareForEvaluation();
            auto* service_worker =
                static_cast<mojom::blink::ServiceWorker*>(global_scope);
            service_worker->ExecuteScriptForTest(
                "self.onfetch = e => e.respondWith(new Promise(() => {}));",
                /*wants_result=*/false, base::DoNothing());
            global_scope->DidEvaluateScript();

            const KURL request_url("https://example.com/upload");
            auto dispatch_fetch_event =
                [&](mojo::PendingRemote<
                    mojom::blink::ServiceWorkerFetchResponseCallback>
                        callback) {
                  auto request = mojom::blink::FetchAPIRequest::New();
                  request->url = request_url;
                  request->method = "POST";
                  request->is_main_resource_load = true;

                  auto params = mojom::blink::DispatchFetchEventParams::New();
                  params->request = std::move(request);
                  params->client_id = "client_id";

                  service_worker->DispatchFetchEventForMainResource(
                      std::move(params), std::move(callback),
                      base::DoNothing());
                };

            // Start two fetch events (IDs 1 and 2). Both stay unanswered, so
            // both response callbacks remain registered once event dispatch
            // has returned. Only event 1 is responded to below. Event 2 keeps
            // the callback map non-empty: removing the last entry may shrink
            // the table, i.e. allocate, inside `Take()` while the taken
            // callback is referenced only from the stack, which the GC forced
            // below does not scan.
            dispatch_fetch_event(std::move(response_callback_remote));
            mojo::PendingRemote<
                mojom::blink::ServiceWorkerFetchResponseCallback>
                unanswered_callback;
            std::ignore = unanswered_callback.InitWithNewPipeAndPassReceiver();
            dispatch_fetch_event(std::move(unanswered_callback));

            // Make every subsequent Oilpan allocation on this thread trigger a
            // GC. The countdown drawn with the fixture's initial interval (50)
            // must run out before the new interval takes effect, hence the 50
            // throwaway allocations.
            v8::V8::SetFlagsFromString("--cppgc-random-gc-interval=1");
            for (int i = 0; i < 50; ++i) {
              MakeGarbageCollected<FetchRequestData>(global_scope);
            }

            mojo::PendingRemote<network::mojom::ChunkedDataPipeGetter>
                pipe_getter;
            std::ignore = pipe_getter.InitWithNewPipeAndPassReceiver();
            std::optional<network::DataElementChunkedDataPipe> request_body(
                std::in_place, std::move(pipe_getter),
                network::DataElementChunkedDataPipe::ReadOnlyOnce(true));

            // Respond from the top of this task, where no event dispatch is on
            // the stack, and have the GCs triggered inside the call ignore the
            // stack. A response callback that is no longer reachable from the
            // heap when the event is recorded as a pending streaming upload is
            // then collected before it is invoked.
            cppgc::testing::OverrideEmbedderStackStateScope stack_state_scope(
                ThreadState::Current()->heap_handle(),
                cppgc::EmbedderStackState::kNoHeapPointers);
            global_scope->RespondToFetchEventWithNoResponse(
                /*fetch_event_id=*/1, /*fetch_event=*/nullptr, request_url,
                /*range_request=*/false, std::move(request_body),
                base::TimeTicks::Now(), base::TimeTicks::Now(),
                /*errors=*/nullptr);
          },
          CrossThreadUnretained(worker_thread_.get()),
          std::move(response_callback_remote)));
  run_loop.Run();

  EXPECT_EQ(1, response_callback.fallback_count());
  EXPECT_EQ(1, response_callback.request_body_count());
}
}  // namespace blink
