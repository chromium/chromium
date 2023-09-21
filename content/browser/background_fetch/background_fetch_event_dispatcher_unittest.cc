// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_event_dispatcher.h"

#include <stdint.h>
#include <memory>

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "content/browser/background_fetch/background_fetch_registration_id.h"
#include "content/browser/background_fetch/background_fetch_test_base.h"
#include "content/browser/background_fetch/background_fetch_test_service_worker.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/storage_partition_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/background_fetch/background_fetch.mojom.h"

namespace content {
namespace {

const char kExampleDeveloperId[] = "my-id";
const char kExampleDeveloperId2[] = "my-second-id";
const char kExampleUniqueId[] = "7e57ab1e-c0de-a150-ca75-1e75f005ba11";
const char kExampleUniqueId2[] = "bb48a9fb-c21f-4c2d-a9ae-58bd48a9fb53";

class BackgroundFetchEventDispatcherTest : public BackgroundFetchTestBase {
 public:
  BackgroundFetchEventDispatcherTest() = default;

  BackgroundFetchEventDispatcherTest(
      const BackgroundFetchEventDispatcherTest&) = delete;
  BackgroundFetchEventDispatcherTest& operator=(
      const BackgroundFetchEventDispatcherTest&) = delete;

  ~BackgroundFetchEventDispatcherTest() override = default;

  void SetUp() override {
    BackgroundFetchTestBase::SetUp();
    auto* background_fetch_context =
        storage_partition()->GetBackgroundFetchContext();
    event_dispatcher_ = std::make_unique<BackgroundFetchEventDispatcher>(
        background_fetch_context,
        embedded_worker_test_helper()->context_wrapper(), devtools_context());
  }

 protected:
  std::unique_ptr<BackgroundFetchEventDispatcher> event_dispatcher_;
  base::HistogramTester histogram_tester_;
};

TEST_F(BackgroundFetchEventDispatcherTest, DispatchAbortEvent) {
  auto* worker =
      embedded_worker_test_helper()
          ->AddNewPendingServiceWorker<BackgroundFetchTestServiceWorker>(
              embedded_worker_test_helper());
  int64_t service_worker_registration_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId,
            service_worker_registration_id);

  std::vector<blink::mojom::BackgroundFetchSettledFetchPtr> fetches;
  auto fetch = blink::mojom::BackgroundFetchSettledFetch::New();
  fetch->request = blink::mojom::FetchAPIRequest::New();
  fetch->response = blink::mojom::FetchAPIResponse::New();
  fetches.push_back(std::move(fetch));

  BackgroundFetchRegistrationId registration_id(
      service_worker_registration_id, storage_key(), kExampleDeveloperId,
      kExampleUniqueId);

  {
    base::RunLoop run_loop;
    auto registration_data = CreateBackgroundFetchRegistrationData(
        kExampleDeveloperId, blink::mojom::BackgroundFetchResult::FAILURE,
        blink::mojom::BackgroundFetchFailureReason::CANCELLED_FROM_UI);
    event_dispatcher_->DispatchBackgroundFetchCompletionEvent(
        registration_id, std::move(registration_data), run_loop.QuitClosure());

    run_loop.Run();
  }

  ASSERT_TRUE(worker->last_registration());
  EXPECT_EQ(kExampleDeveloperId, worker->last_registration()->developer_id);
  EXPECT_EQ(blink::mojom::BackgroundFetchFailureReason::CANCELLED_FROM_UI,
            worker->last_registration()->failure_reason);

  worker->set_fail_abort_event(true);

  BackgroundFetchRegistrationId second_registration_id(
      service_worker_registration_id, storage_key(), kExampleDeveloperId2,
      kExampleUniqueId2);

  {
    base::RunLoop run_loop;
    auto registration_data = CreateBackgroundFetchRegistrationData(
        kExampleDeveloperId2, blink::mojom::BackgroundFetchResult::FAILURE,
        blink::mojom::BackgroundFetchFailureReason::CANCELLED_BY_DEVELOPER);
    event_dispatcher_->DispatchBackgroundFetchCompletionEvent(
        second_registration_id, std::move(registration_data),
        run_loop.QuitClosure());

    run_loop.Run();
  }

  ASSERT_TRUE(worker->last_registration());
  EXPECT_EQ(kExampleDeveloperId2, worker->last_registration()->developer_id);
}

TEST_F(BackgroundFetchEventDispatcherTest, DispatchClickEvent) {
  auto* worker =
      embedded_worker_test_helper()
          ->AddNewPendingServiceWorker<BackgroundFetchTestServiceWorker>(
              embedded_worker_test_helper());
  int64_t service_worker_registration_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId,
            service_worker_registration_id);

  BackgroundFetchRegistrationId registration_id(
      service_worker_registration_id, storage_key(), kExampleDeveloperId,
      kExampleUniqueId);

  {
    base::RunLoop run_loop;
    auto registration_data = CreateBackgroundFetchRegistrationData(
        kExampleDeveloperId, blink::mojom::BackgroundFetchResult::UNSET,
        blink::mojom::BackgroundFetchFailureReason::NONE);
    event_dispatcher_->DispatchBackgroundFetchClickEvent(
        registration_id, std::move(registration_data), run_loop.QuitClosure());

    run_loop.Run();
  }

  ASSERT_TRUE(worker->last_registration());
  EXPECT_EQ(kExampleDeveloperId, worker->last_registration()->developer_id);
  EXPECT_EQ(blink::mojom::BackgroundFetchResult::UNSET,
            worker->last_registration()->result);

  worker->set_fail_click_event(true);

  BackgroundFetchRegistrationId second_registration_id(
      service_worker_registration_id, storage_key(), kExampleDeveloperId2,
      kExampleUniqueId2);

  {
    base::RunLoop run_loop;
    auto registration_data = CreateBackgroundFetchRegistrationData(
        kExampleDeveloperId2, blink::mojom::BackgroundFetchResult::FAILURE,
        blink::mojom::BackgroundFetchFailureReason::QUOTA_EXCEEDED);
    event_dispatcher_->DispatchBackgroundFetchClickEvent(
        second_registration_id, std::move(registration_data),
        run_loop.QuitClosure());

    run_loop.Run();
  }

  ASSERT_TRUE(worker->last_registration());
  EXPECT_EQ(kExampleDeveloperId2, worker->last_registration()->developer_id);
  EXPECT_EQ(blink::mojom::BackgroundFetchResult::FAILURE,
            worker->last_registration()->result);
}

TEST_F(BackgroundFetchEventDispatcherTest, DispatchFailEvent) {
  auto* worker =
      embedded_worker_test_helper()
          ->AddNewPendingServiceWorker<BackgroundFetchTestServiceWorker>(
              embedded_worker_test_helper());
  int64_t service_worker_registration_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId,
            service_worker_registration_id);

  BackgroundFetchRegistrationId registration_id(
      service_worker_registration_id, storage_key(), kExampleDeveloperId,
      kExampleUniqueId);

  {
    base::RunLoop run_loop;
    auto registration_data = CreateBackgroundFetchRegistrationData(
        kExampleDeveloperId, blink::mojom::BackgroundFetchResult::FAILURE,
        blink::mojom::BackgroundFetchFailureReason::QUOTA_EXCEEDED);
    event_dispatcher_->DispatchBackgroundFetchCompletionEvent(
        registration_id, std::move(registration_data), run_loop.QuitClosure());

    run_loop.Run();
  }

  ASSERT_TRUE(worker->last_registration());
  EXPECT_EQ(kExampleDeveloperId, worker->last_registration()->developer_id);

  worker->set_fail_fetch_fail_event(true);

  BackgroundFetchRegistrationId second_registration_id(
      service_worker_registration_id, storage_key(), kExampleDeveloperId2,
      kExampleUniqueId2);

  {
    base::RunLoop run_loop;
    auto registration_data = CreateBackgroundFetchRegistrationData(
        kExampleDeveloperId2, blink::mojom::BackgroundFetchResult::FAILURE,
        blink::mojom::BackgroundFetchFailureReason::QUOTA_EXCEEDED);
    event_dispatcher_->DispatchBackgroundFetchCompletionEvent(
        second_registration_id, std::move(registration_data),
        run_loop.QuitClosure());
    run_loop.Run();
  }

  ASSERT_TRUE(worker->last_registration());
  EXPECT_EQ(kExampleDeveloperId2, worker->last_registration()->developer_id);
}

TEST_F(BackgroundFetchEventDispatcherTest, DispatchFetchSuccessEvent) {
  auto* worker =
      embedded_worker_test_helper()
          ->AddNewPendingServiceWorker<BackgroundFetchTestServiceWorker>(
              embedded_worker_test_helper());
  int64_t service_worker_registration_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId,
            service_worker_registration_id);

  BackgroundFetchRegistrationId registration_id(
      service_worker_registration_id, storage_key(), kExampleDeveloperId,
      kExampleUniqueId);

  {
    base::RunLoop run_loop;
    auto registration_data = CreateBackgroundFetchRegistrationData(
        kExampleDeveloperId, blink::mojom::BackgroundFetchResult::SUCCESS,
        blink::mojom::BackgroundFetchFailureReason::NONE);
    event_dispatcher_->DispatchBackgroundFetchCompletionEvent(
        registration_id, std::move(registration_data), run_loop.QuitClosure());

    run_loop.Run();
  }

  ASSERT_TRUE(worker->last_registration());
  EXPECT_EQ(kExampleDeveloperId, worker->last_registration()->developer_id);

  worker->set_fail_fetched_event(true);

  BackgroundFetchRegistrationId second_registration_id(
      service_worker_registration_id, storage_key(), kExampleDeveloperId2,
      kExampleUniqueId2);

  {
    base::RunLoop run_loop;
    auto registration_data = CreateBackgroundFetchRegistrationData(
        kExampleDeveloperId2, blink::mojom::BackgroundFetchResult::SUCCESS,
        blink::mojom::BackgroundFetchFailureReason::NONE);
    event_dispatcher_->DispatchBackgroundFetchCompletionEvent(
        second_registration_id, std::move(registration_data),
        run_loop.QuitClosure());

    run_loop.Run();
  }

  ASSERT_TRUE(worker->last_registration());
  EXPECT_EQ(kExampleDeveloperId2, worker->last_registration()->developer_id);
}

}  // namespace
}  // namespace content
