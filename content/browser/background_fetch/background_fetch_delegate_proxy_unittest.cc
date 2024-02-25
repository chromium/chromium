// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_delegate_proxy.h"

#include <set>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "content/browser/background_fetch/background_fetch_test_base.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/background_fetch_delegate.h"
#include "content/public/browser/background_fetch_description.h"
#include "content/public/browser/background_fetch_response.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_worker_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/background_fetch/background_fetch.mojom.h"

namespace content {

namespace {

const char kExampleUniqueId[] = "7e57ab1e-c0de-a150-ca75-1e75f005ba11";
const char kExampleUniqueId2[] = "17467386-60b4-4c5b-b66c-aabf793fd39b";
const int kIconDisplaySize = 192;

class FakeBackgroundFetchDelegate : public BackgroundFetchDelegate {
 public:
  FakeBackgroundFetchDelegate() = default;

  // BackgroundFetchDelegate implementation:
  void GetIconDisplaySize(
      BackgroundFetchDelegate::GetIconDisplaySizeCallback callback) override {
    std::move(callback).Run(gfx::Size(kIconDisplaySize, kIconDisplaySize));
  }
  void CreateDownloadJob(
      base::WeakPtr<Client> client,
      std::unique_ptr<BackgroundFetchDescription> fetch_description) override {
    job_id_to_client_.emplace(fetch_description->job_unique_id,
                              std::move(client));
  }
  void DownloadUrl(const std::string& job_unique_id,
                   const std::string& guid,
                   const std::string& method,
                   const GURL& url,
                   ::network::mojom::CredentialsMode credentials_mode,
                   const net::NetworkTrafficAnnotationTag& traffic_annotation,
                   const net::HttpRequestHeaders& headers,
                   bool has_request_body) override {
    if (!job_id_to_client_[job_unique_id])
      return;

    download_guid_to_job_id_map_[guid] = job_unique_id;
    download_guid_to_url_map_[guid] = url;

    auto response = std::make_unique<BackgroundFetchResponse>(
        std::vector<GURL>({url}),
        base::MakeRefCounted<net::HttpResponseHeaders>("200 OK"));

    job_id_to_client_[job_unique_id]->OnDownloadStarted(job_unique_id, guid,
                                                        std::move(response));
    if (complete_downloads_) {
      // Post a task so that Abort() can cancel this download before completing.
      GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&FakeBackgroundFetchDelegate::CompleteDownload,
                         base::Unretained(this), job_unique_id, guid));
    }
  }

  void Abort(const std::string& job_unique_id) override {
    aborted_jobs_.insert(job_unique_id);
  }

  void MarkJobComplete(const std::string& job_unique_id) override {}

  void UpdateUI(const std::string& job_unique_id,
                const std::optional<std::string>& title,
                const std::optional<SkBitmap>& icon) override {
    ++ui_update_count_;
  }

  void set_complete_downloads(bool complete_downloads) {
    complete_downloads_ = complete_downloads;
  }

  int ui_update_count_ = 0;

 private:
  void CompleteDownload(const std::string& job_unique_id,
                        const std::string& guid) {
    if (!job_id_to_client_[job_unique_id])
      return;

    if (aborted_jobs_.count(download_guid_to_job_id_map_[guid]))
      return;

    auto response = std::make_unique<BackgroundFetchResponse>(
        std::vector<GURL>({download_guid_to_url_map_[guid]}),
        base::MakeRefCounted<net::HttpResponseHeaders>("200 OK"));

    job_id_to_client_[job_unique_id]->OnDownloadComplete(
        job_unique_id, guid,
        std::make_unique<BackgroundFetchResult>(
            std::move(response), base::Time::Now(), base::FilePath(),
            std::nullopt /* blob_handle */, 10u));
    download_guid_to_url_map_.erase(guid);
  }

  std::set<std::string> aborted_jobs_;
  std::map<std::string, std::string> download_guid_to_job_id_map_;
  std::map<std::string, GURL> download_guid_to_url_map_;
  std::map<std::string, base::WeakPtr<Client>> job_id_to_client_;
  bool complete_downloads_ = true;
};

class FakeTestBrowserContext : public TestBrowserContext {
 public:
  FakeTestBrowserContext() = default;
  ~FakeTestBrowserContext() override = default;

  FakeBackgroundFetchDelegate* GetBackgroundFetchDelegate() override {
    if (!delegate_)
      delegate_ = std::make_unique<FakeBackgroundFetchDelegate>();
    return delegate_.get();
  }

 private:
  std::unique_ptr<FakeBackgroundFetchDelegate> delegate_;
};

class FakeController : public BackgroundFetchDelegateProxy::Controller {
 public:
  FakeController() {}

  void DidStartRequest(
      const std::string& guid,
      std::unique_ptr<BackgroundFetchResponse> response) override {
    request_started_ = true;
  }

  void DidUpdateRequest(const std::string& guid,
                        uint64_t bytes_uploaded,
                        uint64_t bytes_downloaded) override {}

  void DidCompleteRequest(
      const std::string& guid,
      std::unique_ptr<BackgroundFetchResult> result) override {
    request_completed_ = true;
  }

  void AbortFromDelegate(
      blink::mojom::BackgroundFetchFailureReason reason_to_abort) override {}

  void GetUploadData(
      const std::string& guid,
      BackgroundFetchDelegate::GetUploadDataCallback callback) override {}

  bool request_started_ = false;
  bool request_completed_ = false;
  base::WeakPtrFactory<FakeController> weak_ptr_factory_{this};
};

class BackgroundFetchDelegateProxyTest : public BackgroundFetchTestBase {
 public:
  BackgroundFetchDelegateProxyTest()
      : storage_partition_factory_(static_cast<StoragePartitionImpl*>(
            browser_context_.GetDefaultStoragePartition())) {
    delegate_proxy_ = std::make_unique<BackgroundFetchDelegateProxy>(
        storage_partition_factory_.GetWeakPtr());
    delegate_ = browser_context_.GetBackgroundFetchDelegate();
  }
  void DidGetIconDisplaySize(base::OnceClosure quit_closure,
                             gfx::Size* out_display_size,
                             const gfx::Size& display_size) {
    DCHECK(out_display_size);
    *out_display_size = display_size;
    std::move(quit_closure).Run();
  }

 protected:
  FakeTestBrowserContext browser_context_;
  raw_ptr<FakeBackgroundFetchDelegate> delegate_;
  std::unique_ptr<BackgroundFetchDelegateProxy> delegate_proxy_;
  base::WeakPtrFactory<StoragePartitionImpl> storage_partition_factory_;
};

scoped_refptr<BackgroundFetchRequestInfo> CreateRequestInfo(
    int request_index,
    blink::mojom::FetchAPIRequestPtr fetch_request) {
  auto request = base::MakeRefCounted<BackgroundFetchRequestInfo>(
      request_index, std::move(fetch_request), /* has_request_body= */ false);
  request->InitializeDownloadGuid();
  return request;
}

}  // namespace

TEST_F(BackgroundFetchDelegateProxyTest, StartRequest) {
  FakeController controller;
  auto fetch_request = blink::mojom::FetchAPIRequest::New();
  auto request =
      CreateRequestInfo(/* request_index= */ 0, std::move(fetch_request));

  EXPECT_FALSE(controller.request_started_);
  EXPECT_FALSE(controller.request_completed_);

  auto fetch_description = std::make_unique<BackgroundFetchDescription>(
      kExampleUniqueId, url::Origin(), /* title= */ "Job 1", SkBitmap(),
      /* completed_requests= */ 0, /* total_requests= */ 1,
      /* downloaded_bytes= */ 0u, /* uploaded_bytes= */ 0u,
      /* download_total= */ 0u, /* upload_total= */ 0u,
      /* outstanding_guids= */ std::vector<std::string>(),
      /* start_paused= */ false, /* isolation_info= */ std::nullopt);
  delegate_proxy_->CreateDownloadJob(controller.weak_ptr_factory_.GetWeakPtr(),
                                     std::move(fetch_description));

  delegate_proxy_->StartRequest(kExampleUniqueId, url::Origin(), request);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(controller.request_started_);
  EXPECT_TRUE(controller.request_completed_);
}

TEST_F(BackgroundFetchDelegateProxyTest, StartRequest_NotCompleted) {
  FakeController controller;
  auto fetch_request = blink::mojom::FetchAPIRequest::New();
  auto request =
      CreateRequestInfo(/* request_index= */ 0, std::move(fetch_request));

  EXPECT_FALSE(controller.request_started_);
  EXPECT_FALSE(controller.request_completed_);

  delegate_->set_complete_downloads(false);
  auto fetch_description = std::make_unique<BackgroundFetchDescription>(
      kExampleUniqueId, url::Origin(), /* title= */ "Job 1", SkBitmap(),
      /* completed_requests= */ 0, /* total_requests= */ 1,
      /* downloaded_bytes= */ 0u, /* uploaded_bytes= */ 0u,
      /* download_total= */ 0u, /* upload_total= */ 0u,
      /* outstanding_guids= */ std::vector<std::string>(),
      /* start_paused= */ false, /* isolation_info= */ std::nullopt);
  delegate_proxy_->CreateDownloadJob(controller.weak_ptr_factory_.GetWeakPtr(),
                                     std::move(fetch_description));

  delegate_proxy_->StartRequest(kExampleUniqueId, url::Origin(), request);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(controller.request_started_);
  EXPECT_FALSE(controller.request_completed_);
}

TEST_F(BackgroundFetchDelegateProxyTest, Abort) {
  FakeController controller;
  FakeController controller2;
  auto fetch_request = blink::mojom::FetchAPIRequest::New();
  auto fetch_request2 = blink::mojom::FetchAPIRequest::New();
  auto request =
      CreateRequestInfo(/* request_index= */ 0, std::move(fetch_request));
  auto request2 =
      CreateRequestInfo(/* request_index= */ 1, std::move(fetch_request2));

  EXPECT_FALSE(controller.request_started_);
  EXPECT_FALSE(controller.request_completed_);
  EXPECT_FALSE(controller2.request_started_);
  EXPECT_FALSE(controller2.request_completed_);

  auto fetch_description1 = std::make_unique<BackgroundFetchDescription>(
      kExampleUniqueId, url::Origin(), /* title= */ "Job 1", SkBitmap(),
      /* completed_requests= */ 0, /* total_requests= */ 1,
      /* downloaded_bytes= */ 0u, /* uploaded_bytes= */ 0u,
      /* download_total= */ 0u, /* upload_total= */ 0u,
      /* outstanding_guids= */ std::vector<std::string>(),
      /* start_paused= */ false, /* isolation_info= */ std::nullopt);
  delegate_proxy_->CreateDownloadJob(controller.weak_ptr_factory_.GetWeakPtr(),
                                     std::move(fetch_description1));

  auto fetch_description2 = std::make_unique<BackgroundFetchDescription>(
      kExampleUniqueId2, url::Origin(), /* title= */ "Job 2", SkBitmap(),
      /* completed_requests= */ 0, /* total_requests= */ 1,
      /* downloaded_bytes= */ 0u, /* uploaded_bytes= */ 0u,
      /* download_total= */ 0u, /* upload_total= */ 0u,
      /* outstanding_guids= */ std::vector<std::string>(),
      /* start_paused= */ false, /* isolation_info= */ std::nullopt);
  delegate_proxy_->CreateDownloadJob(controller2.weak_ptr_factory_.GetWeakPtr(),
                                     std::move(fetch_description2));

  delegate_proxy_->StartRequest(kExampleUniqueId, url::Origin(), request);
  delegate_proxy_->StartRequest(kExampleUniqueId2, url::Origin(), request2);
  delegate_proxy_->Abort(kExampleUniqueId);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(controller.request_completed_) << "Aborted job completed";
  EXPECT_TRUE(controller2.request_started_) << "Normal job did not start";
  EXPECT_TRUE(controller2.request_completed_) << "Normal job did not complete";
}

TEST_F(BackgroundFetchDelegateProxyTest, GetIconDisplaySize) {
  gfx::Size out_display_size;
  base::RunLoop run_loop;
  delegate_proxy_->GetIconDisplaySize(base::BindOnce(
      &BackgroundFetchDelegateProxyTest::DidGetIconDisplaySize,
      base::Unretained(this), run_loop.QuitClosure(), &out_display_size));
  run_loop.Run();
  EXPECT_EQ(out_display_size.width(), kIconDisplaySize);
  EXPECT_EQ(out_display_size.height(), kIconDisplaySize);
}

TEST_F(BackgroundFetchDelegateProxyTest, UpdateUI) {
  FakeController controller;
  auto fetch_request = blink::mojom::FetchAPIRequest::New();

  auto request =
      CreateRequestInfo(/* request_index= */ 0, std::move(fetch_request));
  auto fetch_description = std::make_unique<BackgroundFetchDescription>(
      kExampleUniqueId, url::Origin(), /* title= */ "Job 1 Started.",
      SkBitmap(),
      /* completed_requests= */ 0, /* total_requests= */ 1,
      /* downloaded_bytes= */ 0u, /* uploaded_bytes= */ 0u,
      /* download_total= */ 0u, /* upload_total= */ 0u,
      /* outstanding_guids= */ std::vector<std::string>(),
      /* start_paused= */ false,
      /* isolation_info= */ std::nullopt);

  delegate_proxy_->CreateDownloadJob(controller.weak_ptr_factory_.GetWeakPtr(),
                                     std::move(fetch_description));

  delegate_proxy_->StartRequest(kExampleUniqueId, url::Origin(), request);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(controller.request_started_);
  EXPECT_TRUE(controller.request_completed_);

  delegate_proxy_->UpdateUI(kExampleUniqueId, "Job 1 Complete!", std::nullopt,
                            base::DoNothing());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(delegate_->ui_update_count_, 1);
}

TEST_F(BackgroundFetchDelegateProxyTest, MultipleClients) {
  FakeController controller1, controller2;
  EXPECT_FALSE(controller1.request_started_);
  EXPECT_FALSE(controller1.request_completed_);
  EXPECT_FALSE(controller2.request_started_);
  EXPECT_FALSE(controller2.request_completed_);

  BackgroundFetchDelegateProxy delegate_proxy1(
      storage_partition_factory_.GetWeakPtr());
  BackgroundFetchDelegateProxy delegate_proxy2(
      storage_partition_factory_.GetWeakPtr());

  auto fetch_description1 = std::make_unique<BackgroundFetchDescription>(
      kExampleUniqueId, url::Origin(), /* title= */ "Job 1", SkBitmap(),
      /* completed_requests= */ 0, /* total_requests= */ 1,
      /* downloaded_bytes= */ 0u, /* uploaded_bytes= */ 0u,
      /* download_total= */ 0u, /* upload_total= */ 0u,
      /* outstanding_guids= */ std::vector<std::string>(),
      /* start_paused= */ false, /* isolation_info= */ std::nullopt);
  auto fetch_description2 = std::make_unique<BackgroundFetchDescription>(
      kExampleUniqueId2, url::Origin(), /* title= */ "Job 2", SkBitmap(),
      /* completed_requests= */ 0, /* total_requests= */ 1,
      /* downloaded_bytes= */ 0u, /* uploaded_bytes= */ 0u,
      /* download_total= */ 0u, /* upload_total= */ 0u,
      /* outstanding_guids= */ std::vector<std::string>(),
      /* start_paused= */ false, /* isolation_info= */ std::nullopt);

  delegate_proxy1.CreateDownloadJob(controller1.weak_ptr_factory_.GetWeakPtr(),
                                    std::move(fetch_description1));
  delegate_proxy2.CreateDownloadJob(controller2.weak_ptr_factory_.GetWeakPtr(),
                                    std::move(fetch_description2));

  auto request = CreateRequestInfo(/* request_index= */ 0,
                                   blink::mojom::FetchAPIRequest::New());
  delegate_proxy1.StartRequest(kExampleUniqueId, url::Origin(), request);
  delegate_proxy2.StartRequest(kExampleUniqueId2, url::Origin(), request);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(controller1.request_started_);
  EXPECT_TRUE(controller1.request_completed_);
  EXPECT_TRUE(controller2.request_started_);
  EXPECT_TRUE(controller2.request_completed_);
}

}  // namespace content
