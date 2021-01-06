// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONTROL_WRAPPER_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONTROL_WRAPPER_H_

#include "base/threading/sequence_bound.h"
#include "components/services/storage/public/mojom/indexed_db_control.mojom.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"

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
          native_file_system_context,
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
  void ApplyPolicyUpdates(
      std::vector<storage::mojom::IndexedDBStoragePolicyUpdatePtr>
          policy_updates) override;
  void BindTestInterface(
      mojo::PendingReceiver<storage::mojom::IndexedDBControlTest> receiver)
      override;
  void AddObserver(
      mojo::PendingRemote<storage::mojom::IndexedDBObserver> observer) override;

  // TODO(enne): remove this once IndexedDB moves to storage service.
  IndexedDBContextImpl* GetIndexedDBContextInternal() { return context_.get(); }

 private:
  void OnSpecialStoragePolicyChanged();
  void TrackOriginPolicyState(const url::Origin& origin);
  bool ShouldPurgeOnShutdown(const GURL& origin);
  void BindRemoteIfNeeded();

  // Special storage policy may be null.
  scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy_;

  // Observer for the SpecialStoragePolicy on the IO thread.  May be null.
  class StoragePolicyObserver;
  base::SequenceBound<StoragePolicyObserver> storage_policy_observer_;

  mojo::Remote<storage::mojom::IndexedDBControl> indexed_db_control_;
  scoped_refptr<IndexedDBContextImpl> context_;

  struct OriginState {
    // Indicates that storage for this origin should be purged on shutdown.
    bool should_purge_on_shutdown = false;
    // Indicates the last value for |purge_on_shutdown| communicated to the
    // IndexedDB implementation.
    bool will_purge_on_shutdown = false;
  };
  // NOTE: The GURL key is specifically an origin GURL.
  // Special storage policy uses GURLs and not Origins, so it's simpler
  // to store everything in GURL form.
  std::map<GURL, OriginState> origin_state_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<IndexedDBControlWrapper> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(IndexedDBControlWrapper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONTROL_WRAPPER_H_
