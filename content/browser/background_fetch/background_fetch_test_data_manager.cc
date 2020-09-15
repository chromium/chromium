// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_test_data_manager.h"

#include <utility>

#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "content/browser/cache_storage/legacy/legacy_cache_storage_manager.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"

namespace content {

namespace {

class MockBGFQuotaManagerProxy : public storage::MockQuotaManagerProxy {
 public:
  explicit MockBGFQuotaManagerProxy(storage::MockQuotaManager* quota_manager)
      : storage::MockQuotaManagerProxy(
            quota_manager,
            base::ThreadTaskRunnerHandle::Get().get()) {}

  // Ignore quota client, it is irrelevant for these tests.
  void RegisterClient(
      scoped_refptr<storage::QuotaClient> client,
      storage::QuotaClientType client_type,
      const std::vector<blink::mojom::StorageType>& storage_types) override {}

  void GetUsageAndQuota(base::SequencedTaskRunner* original_task_runner,
                        const url::Origin& origin,
                        blink::mojom::StorageType type,
                        UsageAndQuotaCallback callback) override {
    DCHECK(original_task_runner);
    std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk, /* usage= */ 0,
                            kBackgroundFetchMaxQuotaBytes);
  }

 protected:
  ~MockBGFQuotaManagerProxy() override = default;
};

}  // namespace

BackgroundFetchTestDataManager::BackgroundFetchTestDataManager(
    BrowserContext* browser_context,
    StoragePartition* storage_partition,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context)
    : BackgroundFetchDataManager(browser_context,
                                 service_worker_context,
                                 /* cache_storage_context= */ nullptr,
                                 /* quota_manager_proxy= */ nullptr),
      browser_context_(browser_context),
      storage_partition_(storage_partition) {}

void BackgroundFetchTestDataManager::InitializeOnCoreThread() {
  blob_storage_context_ = ChromeBlobStorageContext::GetFor(browser_context_);
  // Wait for ChromeBlobStorageContext to finish initializing.
  base::RunLoop().RunUntilIdle();

  mock_quota_manager_ = base::MakeRefCounted<storage::MockQuotaManager>(
      storage_partition_->GetPath().empty(), storage_partition_->GetPath(),
      base::ThreadTaskRunnerHandle::Get().get(),
      base::MakeRefCounted<storage::MockSpecialStoragePolicy>());

  quota_manager_proxy_ =
      base::MakeRefCounted<MockBGFQuotaManagerProxy>(mock_quota_manager_.get());

  cache_manager_ = LegacyCacheStorageManager::Create(
      storage_partition_->GetPath(), base::ThreadTaskRunnerHandle::Get(),
      base::ThreadTaskRunnerHandle::Get(), quota_manager_proxy_,
      base::MakeRefCounted<CacheStorageContextImpl::ObserverList>());
  DCHECK(cache_manager_);

  mojo::PendingRemote<storage::mojom::BlobStorageContext> remote;

  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  base::PostTaskAndReply(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&ChromeBlobStorageContext::BindMojoContext,
                     blob_storage_context_,
                     remote.InitWithNewPipeAndPassReceiver()),
      run_loop.QuitClosure());
  run_loop.Run();

  auto context =
      base::MakeRefCounted<BlobStorageContextWrapper>(std::move(remote));
  cache_manager_->SetBlobParametersForCache(std::move(context));
}

BackgroundFetchTestDataManager::~BackgroundFetchTestDataManager() = default;

}  // namespace content
