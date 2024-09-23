// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_scheduler.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/uuid.h"
#include "content/browser/background_fetch/background_fetch_job_controller.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"
#include "content/browser/background_fetch/background_fetch_test_base.h"
#include "content/browser/background_fetch/background_fetch_test_data_manager.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "url/origin.h"

namespace content {

class FakeController : public BackgroundFetchJobController {
 public:
  FakeController(BackgroundFetchDataManager* data_manager,
                 BackgroundFetchDelegateProxy* delegate_proxy,
                 const BackgroundFetchRegistrationId& registration_id,
                 std::vector<std::string>* controller_sequence_list,
                 FinishedCallback finished_callback)
      : BackgroundFetchJobController(
            data_manager,
            delegate_proxy,
            registration_id,
            blink::mojom::BackgroundFetchOptions::New(),
            SkBitmap(),
            /* bytes_downloaded= */ 0u,
            /* bytes_uploaded= */ 0u,
            /* upload_total= */ 0u,
            base::DoNothing(),
            std::move(finished_callback)),
        controller_sequence_list_(controller_sequence_list) {
    DCHECK(controller_sequence_list_);
  }

  ~FakeController() override = default;

  void DidCompleteRequest(
      const std::string& guid,
      std::unique_ptr<BackgroundFetchResult> result) override {
    // Record the completed request. Store everything after the origin and the
    // slash, to be able to directly compare with the provided requests.
    controller_sequence_list_->push_back(
        result->response->url_chain[0].path().substr(1));

    // Continue normally.
    BackgroundFetchJobController::DidCompleteRequest(guid, std::move(result));
  }

 private:
  raw_ptr<std::vector<std::string>> controller_sequence_list_;
};

class BackgroundFetchSchedulerTest : public BackgroundFetchTestBase {
 public:
  BackgroundFetchSchedulerTest() = default;

  void SetUp() override {
    BackgroundFetchTestBase::SetUp();
    data_manager_ = std::make_unique<BackgroundFetchTestDataManager>(
        browser_context(), storage_partition(),
        embedded_worker_test_helper()->context_wrapper());
    data_manager_->Initialize();

    delegate_proxy_ =
        std::make_unique<BackgroundFetchDelegateProxy>(storage_partition());

    auto* background_fetch_context =
        storage_partition()->GetBackgroundFetchContext();
    scheduler_ = std::make_unique<BackgroundFetchScheduler>(
        background_fetch_context, data_manager_.get(), nullptr,
        delegate_proxy_.get(), devtools_context(),
        embedded_worker_test_helper()->context_wrapper());
  }

  void TearDown() override {
    data_manager_ = nullptr;
    delegate_proxy_ = nullptr;
    scheduler_ = nullptr;
    controller_sequence_list_.clear();
    BackgroundFetchTestBase::TearDown();
  }

 protected:
  void InitializeControllerWithRequests(
      const blink::StorageKey& storage_key,
      const std::vector<std::string>& requests) {
    std::vector<blink::mojom::FetchAPIRequestPtr> fetch_requests;
    for (auto& request : requests) {
      auto fetch_request = blink::mojom::FetchAPIRequest::New();
      fetch_request->referrer = blink::mojom::Referrer::New();
      fetch_request->url = GURL(storage_key.origin().GetURL().spec() + request);
      CreateRequestWithProvidedResponse(fetch_request->method,
                                        fetch_request->url,
                                        TestResponseBuilder(200).Build());
      fetch_requests.push_back(std::move(fetch_request));
    }

    int64_t sw_id = RegisterServiceWorkerForOrigin(storage_key.origin());
    BackgroundFetchRegistrationId registration_id(
        sw_id, storage_key, base::Uuid::GenerateRandomV4().AsLowercaseString(),
        base::Uuid::GenerateRandomV4().AsLowercaseString());
    data_manager_->CreateRegistration(
        registration_id, std::move(fetch_requests),
        blink::mojom::BackgroundFetchOptions::New(), SkBitmap(),
        /* start_paused= */ false, net::IsolationInfo(), base::DoNothing());
    task_environment_.RunUntilIdle();

    auto controller = std::make_unique<FakeController>(
        data_manager_.get(), delegate_proxy_.get(), registration_id,
        &controller_sequence_list_,
        base::BindOnce(&BackgroundFetchSchedulerTest::DidJobFinish,
                       base::Unretained(this)));
    controller->InitializeRequestStatus(/* completed_downloads= */ 0,
                                        requests.size(),
                                        /* active_fetch_requests= */ {},
                                        /* start_paused= */ false,
                                        /* isolation_info= */ std::nullopt);
    scheduler_->job_controllers_[registration_id.unique_id()] =
        std::move(controller);
    scheduler_->controller_ids_.push_back(registration_id);
  }

  void RunSchedulerToCompletion() {
    scheduler_->ScheduleDownload();
    task_environment_.RunUntilIdle();
  }

  void DidJobFinish(
      const BackgroundFetchRegistrationId& registration_id,
      blink::mojom::BackgroundFetchFailureReason failure_reason,
      base::OnceCallback<void(blink::mojom::BackgroundFetchError)> callback) {
    DCHECK_EQ(failure_reason, blink::mojom::BackgroundFetchFailureReason::NONE);
    base::EraseIf(scheduler_->active_controllers_,
                  [&registration_id](auto* controller) {
                    return controller->registration_id() == registration_id;
                  });
    scheduler_->job_controllers_.erase(registration_id.unique_id());
    --scheduler_->num_active_registrations_;
    scheduler_->ScheduleDownload();
  }

 protected:
  void MakeSchedulerSequential() {
    scheduler_->max_running_downloads_ = 1;
    scheduler_->max_active_registrations_ = 1;
  }

  void MakeSchedulerConcurrent() {
    scheduler_->max_running_downloads_ = 2;
    scheduler_->max_active_registrations_ = 2;
  }

  std::vector<std::string> controller_sequence_list_;

 private:
  std::unique_ptr<BackgroundFetchDelegateProxy> delegate_proxy_;
  std::unique_ptr<BackgroundFetchTestDataManager> data_manager_;
  std::unique_ptr<BackgroundFetchScheduler> scheduler_;
};

TEST_F(BackgroundFetchSchedulerTest, SingleControllerSynchronous) {
  MakeSchedulerSequential();

  std::vector<std::string> requests = {"A1", "A2", "A3", "A4"};
  InitializeControllerWithRequests(storage_key(), requests);
  RunSchedulerToCompletion();
  EXPECT_EQ(requests, controller_sequence_list_);
}

TEST_F(BackgroundFetchSchedulerTest, SingleControllerConcurrent) {
  MakeSchedulerConcurrent();

  std::vector<std::string> requests = {"A1", "A2", "A3", "A4"};
  InitializeControllerWithRequests(storage_key(), requests);
  RunSchedulerToCompletion();
  EXPECT_EQ(requests, controller_sequence_list_);
}

TEST_F(BackgroundFetchSchedulerTest, TwoControllersSynchronous) {
  MakeSchedulerSequential();

  std::vector<std::string> all_requests = {"A1", "A2", "A3", "A4",
                                           "B1", "B2", "B3"};

  // Create a controller with A1 -> A4.
  InitializeControllerWithRequests(
      blink::StorageKey::CreateFromStringForTesting("https://A.com"),
      std::vector<std::string>(all_requests.begin(), all_requests.begin() + 4));

  // Create a controller with B1 -> B4.
  InitializeControllerWithRequests(
      blink::StorageKey::CreateFromStringForTesting("https://B.com"),
      std::vector<std::string>(all_requests.begin() + 4, all_requests.end()));

  RunSchedulerToCompletion();
  EXPECT_EQ(all_requests, controller_sequence_list_);
}

TEST_F(BackgroundFetchSchedulerTest, TwoControllersConcurrent) {
  MakeSchedulerConcurrent();

  std::vector<std::string> all_requests = {"A1", "A2", "A3", "A4",
                                           "B1", "B2", "B3"};

  // Create a controller with A1 -> A4.
  InitializeControllerWithRequests(
      blink::StorageKey::CreateFromStringForTesting("https://A.com"),
      std::vector<std::string>(all_requests.begin(), all_requests.begin() + 4));

  // Create a controller with B1 -> B4.
  InitializeControllerWithRequests(
      blink::StorageKey::CreateFromStringForTesting("https://B.com"),
      std::vector<std::string>(all_requests.begin() + 4, all_requests.end()));

  RunSchedulerToCompletion();

  std::vector<std::string> expected_sequence_list = {"A1", "B1", "A2", "B2",
                                                     "A3", "B3", "A4"};
  EXPECT_EQ(expected_sequence_list, controller_sequence_list_);
}

TEST_F(BackgroundFetchSchedulerTest, TwoControllersConcurrentSameOrigin) {
  MakeSchedulerConcurrent();

  std::vector<std::string> all_requests = {"A1", "A2", "A3", "A4",
                                           "B1", "B2", "B3"};

  // Create a controller with A1 -> A4.
  InitializeControllerWithRequests(
      storage_key(),
      std::vector<std::string>(all_requests.begin(), all_requests.begin() + 4));

  // Create a controller with B1 -> B4.
  InitializeControllerWithRequests(
      storage_key(),
      std::vector<std::string>(all_requests.begin() + 4, all_requests.end()));

  RunSchedulerToCompletion();
  EXPECT_EQ(all_requests, controller_sequence_list_);
}

}  // namespace content
