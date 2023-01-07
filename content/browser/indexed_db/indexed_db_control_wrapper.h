// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONTROL_WRAPPER_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONTROL_WRAPPER_H_

#include "base/threading/sequence_bound.h"
#include "components/services/storage/privileged/mojom/indexed_db_control.mojom.h"
#include "components/services/storage/public/mojom/storage_policy_update.mojom.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "storage/browser/quota/storage_policy_observer.h"

namespace blink {
class StorageKey;
}

namespace content {

// All functions should be called on the UI thread.
class IndexedDBControlWrapper : public storage::mojom::IndexedDBControl {
 public:
  explicit IndexedDBControlWrapper(
      const base::FilePath& data_path,
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
      scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
      base::Clock* clock,
      mojo::PendingRemote<storage::mojom::BlobStorageContext>
          blob_storage_context,
      mojo::PendingRemote<storage::mojom::FileSystemAccessContext>
          file_system_access_context,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner,
      scoped_refptr<base::SequencedTaskRunner> custom_task_runner);

  IndexedDBControlWrapper(const IndexedDBControlWrapper&) = delete;
  IndexedDBControlWrapper& operator=(const IndexedDBControlWrapper&) = delete;

  ~IndexedDBControlWrapper() override;

  // mojom::IndexedDBControl implementation:
  void BindIndexedDB(
      const blink::StorageKey& storage_key,
      mojo::PendingReceiver<blink::mojom::IDBFactory> receiver) override;
  void BindIndexedDBForBucket(
      const storage::BucketLocator& bucket_locator,
      mojo::PendingReceiver<blink::mojom::IDBFactory> receiver) override;
  void GetUsage(GetUsageCallback usage_callback) override;
  void DeleteForStorageKey(const blink::StorageKey& storage_key,
                           DeleteForStorageKeyCallback callback) override;
  void ForceClose(storage::BucketId bucket_id,
                  storage::mojom::ForceCloseReason reason,
                  base::OnceClosure callback) override;
  void GetConnectionCount(storage::BucketId bucket_id,
                          GetConnectionCountCallback callback) override;
  void DownloadBucketData(storage::BucketId bucket_id,
                          DownloadBucketDataCallback callback) override;
  void GetAllBucketsDetails(GetAllBucketsDetailsCallback callback) override;
  void SetForceKeepSessionState() override;
  void ApplyPolicyUpdates(std::vector<storage::mojom::StoragePolicyUpdatePtr>
                              policy_updates) override;
  void BindTestInterface(
      mojo::PendingReceiver<storage::mojom::IndexedDBControlTest> receiver)
      override;
  void AddObserver(
      mojo::PendingRemote<storage::mojom::IndexedDBObserver> observer) override;

  // TODO(enne): remove this once IndexedDB moves to storage service.
  IndexedDBContextImpl* GetIndexedDBContextInternal() { return context_.get(); }

 private:
  void BindRemoteIfNeeded();

  absl::optional<storage::StoragePolicyObserver> storage_policy_observer_;

  mojo::Remote<storage::mojom::IndexedDBControl> indexed_db_control_;
  scoped_refptr<IndexedDBContextImpl> context_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<IndexedDBControlWrapper> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONTROL_WRAPPER_H_
