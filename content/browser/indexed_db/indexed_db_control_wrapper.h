// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONTROL_WRAPPER_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONTROL_WRAPPER_H_

#include "base/threading/sequence_bound.h"
#include "components/services/storage/public/mojom/indexed_db_control.mojom.h"
#include "components/services/storage/public/mojom/storage_policy_update.mojom.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "storage/browser/quota/storage_policy_observer.h"

namespace content {

// All functions should be called on the UI thread.
class CONTENT_EXPORT IndexedDBControlWrapper
    : public storage::mojom::IndexedDBControl {
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
  ~IndexedDBControlWrapper() override;

  // mojom::IndexedDBControl implementation:
  void BindIndexedDB(
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::IDBFactory> receiver) override;
  void GetUsage(GetUsageCallback usage_callback) override;
  void DeleteForOrigin(const url::Origin& origin,
                       DeleteForOriginCallback callback) override;
  void ForceClose(const url::Origin& origin,
                  storage::mojom::ForceCloseReason reason,
                  base::OnceClosure callback) override;
  void GetConnectionCount(const url::Origin& origin,
                          GetConnectionCountCallback callback) override;
  void DownloadOriginData(const url::Origin& origin,
                          DownloadOriginDataCallback callback) override;
  void GetAllOriginsDetails(GetAllOriginsDetailsCallback callback) override;
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

  base::Optional<storage::StoragePolicyObserver> storage_policy_observer_;

  mojo::Remote<storage::mojom::IndexedDBControl> indexed_db_control_;
  scoped_refptr<IndexedDBContextImpl> context_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<IndexedDBControlWrapper> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(IndexedDBControlWrapper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONTROL_WRAPPER_H_
