// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_test_data_manager.h"

#include <utility>

#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

namespace {

class MockBGFQuotaManagerProxy : public storage::MockQuotaManagerProxy {
 public:
  explicit MockBGFQuotaManagerProxy(storage::MockQuotaManager* quota_manager)
      : storage::MockQuotaManagerProxy(
            quota_manager,
            base::SingleThreadTaskRunner::GetCurrentDefault().get()) {}

  // Ignore quota client, it is irrelevant for these tests.
  void RegisterClient(
      mojo::PendingRemote<storage::mojom::QuotaClient> client,
      storage::QuotaClientType client_type,
      const base::flat_set<blink::mojom::StorageType>& storage_types) override {
  }

  void GetUsageAndQuota(
      const blink::StorageKey& storage_key,
      blink::mojom::StorageType type,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      UsageAndQuotaCallback callback) override {
    DCHECK(callback_task_runner);

    // While this DCHECK is true, the PostTask() below isn't strictly necessary.
    // The callback could be Run() directly.
    DCHECK(callback_task_runner->RunsTasksInCurrentSequence());

    callback_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), blink::mojom::QuotaStatusCode::kOk,
                       /* usage= */ 0, kBackgroundFetchMaxQuotaBytes));
  }

 protected:
  ~MockBGFQuotaManagerProxy() override = default;
};

}  // namespace

BackgroundFetchTestDataManager::BackgroundFetchTestDataManager(
    BrowserContext* browser_context,
    base::WeakPtr<StoragePartitionImpl> storage_partition,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context)
    : BackgroundFetchDataManager(storage_partition,
                                 service_worker_context,
                                 /* quota_manager_proxy= */ nullptr),
      browser_context_(browser_context),
      storage_partition_(storage_partition) {}

void BackgroundFetchTestDataManager::Initialize() {
  // CacheStorage uses the default QuotaManager and not the mock one in this
  // class.  Set QuotaSettings appropriately so that all platforms have quota.
  // The mock one is still used for testing quota exceeded scenarios in
  // DatabaseTask.
  storage::QuotaSettings settings;
  settings.per_storage_key_quota = kBackgroundFetchMaxQuotaBytes;
  settings.pool_size = settings.per_storage_key_quota * 5;
  settings.must_remain_available = 0;
  settings.refresh_interval = base::TimeDelta::Max();
  storage_partition_->GetQuotaManager()->SetQuotaSettings(settings);

  blob_storage_context_ = ChromeBlobStorageContext::GetFor(browser_context_);
  // Wait for ChromeBlobStorageContext to finish initializing.
  base::RunLoop().RunUntilIdle();

  mock_quota_manager_ = base::MakeRefCounted<storage::MockQuotaManager>(
      storage_partition_->GetPath().empty(), storage_partition_->GetPath(),
      base::SingleThreadTaskRunner::GetCurrentDefault().get(),
      base::MakeRefCounted<storage::MockSpecialStoragePolicy>());

  quota_manager_proxy_ =
      base::MakeRefCounted<MockBGFQuotaManagerProxy>(mock_quota_manager_.get());

  mojo::PendingRemote<storage::mojom::BlobStorageContext> remote;

  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  GetIOThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&ChromeBlobStorageContext::BindMojoContext,
                     blob_storage_context_,
                     remote.InitWithNewPipeAndPassReceiver()),
      run_loop.QuitClosure());
  run_loop.Run();
}

BackgroundFetchTestDataManager::~BackgroundFetchTestDataManager() = default;

}  // namespace content
