// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_job_controller.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/guid.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "components/download/public/common/download_item.h"
#include "content/browser/background_fetch/background_fetch_constants.h"
#include "content/browser/background_fetch/background_fetch_context.h"
#include "content/browser/background_fetch/background_fetch_data_manager.h"
#include "content/browser/background_fetch/background_fetch_registration_id.h"
#include "content/browser/background_fetch/background_fetch_scheduler.h"
#include "content/browser/background_fetch/background_fetch_test_base.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/background_fetch_delegate.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/fake_download_item.h"
#include "content/public/test/mock_download_manager.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/background_fetch/background_fetch.mojom.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"

using blink::FetchAPIRequestHeadersMap;
using testing::_;

namespace content {
namespace {

const int64_t kExampleServiceWorkerRegistrationId = 1;
const char kExampleDeveloperId[] = "my-example-id";
const char kExampleResponseData[] = "My response data";

enum class JobCompletionStatus { kRunning, kCompleted, kAborted };

}  // namespace

class BackgroundFetchJobControllerTest : public BackgroundFetchTestBase {
 public:
  BackgroundFetchJobControllerTest() = default;
  ~BackgroundFetchJobControllerTest() override = default;

  // Returns the status for the active job for |registration_id|. The
  // registration should only ever exist in |finished_requests_| in case the
  // request was aborted, given the absence of a scheduler.
  JobCompletionStatus GetCompletionStatus(
      const BackgroundFetchRegistrationId& registration_id) {
    if (finished_requests_.count(registration_id)) {
      DCHECK_NE(finished_requests_[registration_id],
                blink::mojom::BackgroundFetchFailureReason::NONE);

      return JobCompletionStatus::kAborted;
    }

    DCHECK(pending_requests_counts_.count(registration_id));
    if (!pending_requests_counts_[registration_id])
      return JobCompletionStatus::kCompleted;

    return JobCompletionStatus::kRunning;
  }

  // To be called when a request for |registration_id| has finished.
  void OnRequestFinished(
      const BackgroundFetchRegistrationId& registration_id,
      scoped_refptr<content::BackgroundFetchRequestInfo> request_info) {
    DCHECK(pending_requests_counts_.count(registration_id));

    EXPECT_GE(pending_requests_counts_[registration_id], 1);
    pending_requests_counts_[registration_id]--;
  }

  // To be called when a request for |registration_id| has finished.
  // Moves |request_info| to |out_request_info|.
  void GetRequestInfoOnRequestFinished(
      scoped_refptr<content::BackgroundFetchRequestInfo>* out_request_info,
      const BackgroundFetchRegistrationId& registration_id,
      scoped_refptr<content::BackgroundFetchRequestInfo> request_info) {
    DCHECK(pending_requests_counts_.count(registration_id));
    DCHECK(out_request_info);

    EXPECT_GE(pending_requests_counts_[registration_id], 1);
    pending_requests_counts_[registration_id]--;
    *out_request_info = request_info;
  }

  // Creates a new Background Fetch registration, whose id will be stored in the
  // |*registration_id|, and registers it with the DataManager for the included
  // |request_data|. If |auto_complete_requests| is true, the request will
  // immediately receive a successful response. Should be wrapped in
  // ASSERT_NO_FATAL_FAILURE().
  std::vector<scoped_refptr<BackgroundFetchRequestInfo>>
  CreateRegistrationForRequests(
      BackgroundFetchRegistrationId* registration_id,
      std::map<GURL, /* method= */ std::string> request_data,
      bool auto_complete_requests) {
    DCHECK(registration_id);

    // New |unique_id|, since this is a new Background Fetch registration.
    *registration_id = BackgroundFetchRegistrationId(
        kExampleServiceWorkerRegistrationId, origin(), kExampleDeveloperId,
        base::GenerateGUID());

    std::vector<scoped_refptr<BackgroundFetchRequestInfo>> request_infos;
    int request_counter = 0;
    for (const auto& pair : request_data) {
      blink::mojom::FetchAPIRequestPtr request_ptr = CreateFetchAPIRequest(
          GURL(pair.first), pair.second, FetchAPIRequestHeadersMap(),
          blink::mojom::Referrer::New(), false);
      auto request = base::MakeRefCounted<BackgroundFetchRequestInfo>(
          request_counter++, std::move(request_ptr),
          /* has_request_body= */ false);
      request->InitializeDownloadGuid();
      request->set_can_populate_body(true);
      request_infos.push_back(request);
    }

    pending_requests_counts_[*registration_id] = request_data.size();

    if (auto_complete_requests) {
      // Provide fake responses for the given |request_data| pairs.
      for (const auto& pair : request_data) {
        CreateRequestWithProvidedResponse(
            /* method= */ pair.second, /* url= */ pair.first,
            TestResponseBuilder(/* response_code= */ 200)
                .SetResponseData(kExampleResponseData)
                .Build());
      }
    }

    return request_infos;
  }

  // Creates a new BackgroundFetchJobController instance.
  std::unique_ptr<BackgroundFetchJobController> CreateJobController(
      const BackgroundFetchRegistrationId& registration_id,
      int total_downloads) {
    auto controller = std::make_unique<BackgroundFetchJobController>(
        /* data_manager= */ nullptr, delegate_proxy_.get(), registration_id,
        blink::mojom::BackgroundFetchOptions::New(), SkBitmap(),
        /* bytes_downloaded= */ 0u,
        /* bytes_uploaded= */ 0u,
        /* upload_total= */ 0u,
        base::BindRepeating(
            &BackgroundFetchJobControllerTest::DidUpdateProgress,
            base::Unretained(this)),
        base::BindOnce(&BackgroundFetchJobControllerTest::DidFinishJob,
                       base::Unretained(this)));

    controller->InitializeRequestStatus(/* completed_downloads= */ 0,
                                        total_downloads,
                                        /* outstanding_guids= */ {},
                                        /* start_paused= */ false);

    return controller;
  }

  void AddControllerToSchedulerMap(
      const std::string& unique_id,
      std::unique_ptr<BackgroundFetchJobController> controller) {
    scheduler()->job_controllers_[unique_id] = std::move(controller);
  }

  // BackgroundFetchTestBase overrides:
  void SetUp() override {
    BackgroundFetchTestBase::SetUp();

    StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
        BrowserContext::GetDefaultStoragePartition(browser_context()));

    delegate_proxy_ =
        std::make_unique<BackgroundFetchDelegateProxy>(browser_context());

    context_ = base::MakeRefCounted<BackgroundFetchContext>(
        browser_context(),
        base::WrapRefCounted(embedded_worker_test_helper()->context_wrapper()),
        base::WrapRefCounted(partition->GetCacheStorageContext()),
        /* quota_manager_proxy= */ nullptr, devtools_context());
  }

  void TearDown() override {
    BackgroundFetchTestBase::TearDown();
    context_ = nullptr;

    // Give pending shutdown operations a chance to finish.
    base::RunLoop().RunUntilIdle();
  }

 protected:
  scoped_refptr<BackgroundFetchContext> context_;

  uint64_t last_downloaded_ = 0;

  std::map<BackgroundFetchRegistrationId, int> pending_requests_counts_;
  std::map<BackgroundFetchRegistrationId,
           blink::mojom::BackgroundFetchFailureReason>
      finished_requests_;

  // Closure that will be invoked every time the JobController receives a
  // progress update from a download.
  base::RepeatingClosure job_progress_closure_;

  std::unique_ptr<BackgroundFetchDelegateProxy> delegate_proxy_;
  BackgroundFetchDelegate* delegate_;

  BackgroundFetchScheduler* scheduler() { return context_->scheduler_.get(); }

 private:
  void DidUpdateProgress(
      const std::string& unique_id,
      const blink::mojom::BackgroundFetchRegistrationData& registration) {
    last_downloaded_ = registration.downloaded;

    if (job_progress_closure_)
      job_progress_closure_.Run();
  }

  void DidFinishJob(
      const BackgroundFetchRegistrationId& registration_id,
      blink::mojom::BackgroundFetchFailureReason reason_to_abort,
      base::OnceCallback<void(blink::mojom::BackgroundFetchError)> callback) {
    auto iter = pending_requests_counts_.find(registration_id);
    DCHECK(iter != pending_requests_counts_.end());

    finished_requests_[registration_id] = reason_to_abort;
    pending_requests_counts_.erase(iter);
  }

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchJobControllerTest);
};

TEST_F(BackgroundFetchJobControllerTest, SingleRequestJob) {
  BackgroundFetchRegistrationId registration_id;

  auto requests = CreateRegistrationForRequests(
      &registration_id, {{GURL("https://example.com/funny_cat.png"), "GET"}},
      /* auto_complete_requests= */ true);

  EXPECT_EQ(JobCompletionStatus::kRunning,
            GetCompletionStatus(registration_id));

  std::unique_ptr<BackgroundFetchJobController> controller =
      CreateJobController(registration_id, requests.size());

  controller->StartRequest(
      requests[0],
      base::BindOnce(&BackgroundFetchJobControllerTest::OnRequestFinished,
                     base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(JobCompletionStatus::kCompleted,
            GetCompletionStatus(registration_id));
}

TEST_F(BackgroundFetchJobControllerTest, SingleRequestJobWithInsecureOrigin) {
  BackgroundFetchRegistrationId registration_id;

  auto requests = CreateRegistrationForRequests(
      &registration_id, {{GURL("http://example.com/funny_cat.png"), "GET"}},
      /* auto_complete_requests= */ true);

  EXPECT_EQ(JobCompletionStatus::kRunning,
            GetCompletionStatus(registration_id));

  std::unique_ptr<BackgroundFetchJobController> controller =
      CreateJobController(registration_id, requests.size());

  controller->StartRequest(
      requests[0],
      base::BindOnce(
          &BackgroundFetchJobControllerTest::GetRequestInfoOnRequestFinished,
          base::Unretained(this), &requests[0]));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(JobCompletionStatus::kCompleted,
            GetCompletionStatus(registration_id));
  EXPECT_FALSE(requests[0]->IsResultSuccess());
}

TEST_F(BackgroundFetchJobControllerTest, MultipleRequestJob) {
  BackgroundFetchRegistrationId registration_id;

  auto requests = CreateRegistrationForRequests(
      &registration_id,
      {{GURL("https://example.com/funny_cat.png"), "GET"},
       {GURL("https://example.com/scary_cat.png"), "GET"},
       {GURL("https://example.com/crazy_cat.png"), "GET"}},
      /* auto_complete_requests= */ true);

  EXPECT_EQ(JobCompletionStatus::kRunning,
            GetCompletionStatus(registration_id));

  std::unique_ptr<BackgroundFetchJobController> controller =
      CreateJobController(registration_id, requests.size());

  controller->StartRequest(
      requests[0],
      base::BindOnce(&BackgroundFetchJobControllerTest::OnRequestFinished,
                     base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(JobCompletionStatus::kRunning,
            GetCompletionStatus(registration_id));

  controller->StartRequest(
      requests[1],
      base::BindOnce(&BackgroundFetchJobControllerTest::OnRequestFinished,
                     base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(JobCompletionStatus::kRunning,
            GetCompletionStatus(registration_id));

  controller->StartRequest(
      requests[2],
      base::BindOnce(&BackgroundFetchJobControllerTest::OnRequestFinished,
                     base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(JobCompletionStatus::kCompleted,
            GetCompletionStatus(registration_id));
}

TEST_F(BackgroundFetchJobControllerTest, MultipleRequestsJobWithMixedContent) {
  BackgroundFetchRegistrationId registration_id;

  auto requests = CreateRegistrationForRequests(
      &registration_id,
      {{GURL("http://example.com/funny_cat.png"), "GET"},
       {GURL("https://example.com/scary_cat.png"), "GET"}},
      /* auto_complete_requests= */ true);

  EXPECT_EQ(JobCompletionStatus::kRunning,
            GetCompletionStatus(registration_id));

  std::unique_ptr<BackgroundFetchJobController> controller =
      CreateJobController(registration_id, requests.size());

  controller->StartRequest(
      requests[0],
      base::BindOnce(
          &BackgroundFetchJobControllerTest::GetRequestInfoOnRequestFinished,
          base::Unretained(this), &requests[0]));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(JobCompletionStatus::kRunning,
            GetCompletionStatus(registration_id));
  EXPECT_FALSE(requests[0]->IsResultSuccess());

  controller->StartRequest(
      requests[1],
      base::BindOnce(
          &BackgroundFetchJobControllerTest::GetRequestInfoOnRequestFinished,
          base::Unretained(this), &requests[1]));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(JobCompletionStatus::kCompleted,
            GetCompletionStatus(registration_id));
  EXPECT_TRUE(requests[1]->IsResultSuccess());
}

TEST_F(BackgroundFetchJobControllerTest, InProgressBytes) {
  BackgroundFetchRegistrationId registration_id;

  auto requests = CreateRegistrationForRequests(
      &registration_id,
      {{GURL("https://example.com/upload?id=1"), "PUT"},
       {GURL("https://example.com/upload?id=2"), "PUT"}},
      /* auto_complete_requests= */ true);

  std::unique_ptr<BackgroundFetchJobController> controller =
      CreateJobController(registration_id, requests.size());

  controller->StartRequest(requests[0], base::DoNothing());
  controller->StartRequest(requests[1], base::DoNothing());

  // Send fake update event.
  controller->DidUpdateRequest(requests[0]->download_guid(),
                               /* uploaded_bytes= */ 10u,
                               /* downloaded_bytes= */ 20u);
  controller->DidUpdateRequest(requests[1]->download_guid(),
                               /* uploaded_bytes= */ 30u,
                               /* downloaded_bytes= */ 40u);

  EXPECT_EQ(controller->GetInProgressDownloadedBytes(), 20u + 40u);
  EXPECT_EQ(controller->GetInProgressUploadedBytes(), 10u + 30u);
}

TEST_F(BackgroundFetchJobControllerTest, Abort) {
  BackgroundFetchRegistrationId registration_id;

  auto requests = CreateRegistrationForRequests(
      &registration_id, {{GURL("https://example.com/funny_cat.png"), "GET"}},
      /* auto_complete_requests= */ true);

  EXPECT_EQ(JobCompletionStatus::kRunning,
            GetCompletionStatus(registration_id));

  std::unique_ptr<BackgroundFetchJobController> controller =
      CreateJobController(registration_id, requests.size());

  controller->StartRequest(
      requests[0],
      base::BindOnce(&BackgroundFetchJobControllerTest::OnRequestFinished,
                     base::Unretained(this)));

  controller->Abort(
      blink::mojom::BackgroundFetchFailureReason::CANCELLED_FROM_UI,
      base::DoNothing());

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(JobCompletionStatus::kAborted,
            GetCompletionStatus(registration_id));
}

TEST_F(BackgroundFetchJobControllerTest, Progress) {
  BackgroundFetchRegistrationId registration_id;

  auto requests = CreateRegistrationForRequests(
      &registration_id, {{GURL("https://example.com/funny_cat.png"), "GET"}},
      /* auto_complete_requests= */ true);

  EXPECT_EQ(JobCompletionStatus::kRunning,
            GetCompletionStatus(registration_id));

  std::unique_ptr<BackgroundFetchJobController> controller =
      CreateJobController(registration_id, requests.size());

  controller->StartRequest(
      requests[0],
      base::BindOnce(&BackgroundFetchJobControllerTest::OnRequestFinished,
                     base::Unretained(this)));

  {
    base::RunLoop run_loop;
    job_progress_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  EXPECT_GT(last_downloaded_, 0u);
  EXPECT_LT(last_downloaded_, strlen(kExampleResponseData));
  EXPECT_EQ(JobCompletionStatus::kRunning,
            GetCompletionStatus(registration_id));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(JobCompletionStatus::kCompleted,
            GetCompletionStatus(registration_id));
  EXPECT_EQ(last_downloaded_, strlen(kExampleResponseData));
}

TEST_F(BackgroundFetchJobControllerTest, ServiceWorkerRegistrationDeleted) {
  BackgroundFetchRegistrationId registration_id;

  auto requests = CreateRegistrationForRequests(
      &registration_id, {{GURL("https://example.com/funny_cat.png"), "GET"}},
      /* auto_complete_requests= */ true);

  EXPECT_EQ(JobCompletionStatus::kRunning,
            GetCompletionStatus(registration_id));

  std::unique_ptr<BackgroundFetchJobController> controller =
      CreateJobController(registration_id, requests.size());

  AddControllerToSchedulerMap(registration_id.unique_id(),
                              std::move(controller));
  scheduler()->OnRegistrationDeleted(kExampleServiceWorkerRegistrationId,
                                     GURL("https://example.com/funny_cat.png"));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(JobCompletionStatus::kAborted,
            GetCompletionStatus(registration_id));
}

TEST_F(BackgroundFetchJobControllerTest, ServiceWorkerDatabaseDeleted) {
  BackgroundFetchRegistrationId registration_id;

  auto requests = CreateRegistrationForRequests(
      &registration_id, {{GURL("https://example.com/funny_cat.png"), "GET"}},
      /* auto_complete_requests= */ true);

  EXPECT_EQ(JobCompletionStatus::kRunning,
            GetCompletionStatus(registration_id));

  std::unique_ptr<BackgroundFetchJobController> controller =
      CreateJobController(registration_id, requests.size());

  AddControllerToSchedulerMap(registration_id.unique_id(),
                              std::move(controller));

  scheduler()->OnStorageWiped();

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(JobCompletionStatus::kAborted,
            GetCompletionStatus(registration_id));
}

}  // namespace content
