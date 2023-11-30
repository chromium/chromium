// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONTROL_WRAPPER_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONTROL_WRAPPER_H_

#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "components/services/storage/privileged/mojom/indexed_db_control.mojom.h"
#include "components/services/storage/public/mojom/storage_policy_update.mojom.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "storage/browser/quota/storage_policy_observer.h"

namespace content {

// This wrapper is created, destroyed, and operated on the UI thread in the
// browser process. Its main purpose is to observe the `special_storage_policy`
// and forwards policy updates to the context, while also informing the policy
// when an origin is actively using IDB.
class IndexedDBControlWrapper : public storage::mojom::IndexedDBObserver {
 public:
  explicit IndexedDBControlWrapper(
      const base::FilePath& data_path,
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
      scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
      mojo::PendingRemote<storage::mojom::BlobStorageContext>
          blob_storage_context,
      mojo::PendingRemote<storage::mojom::FileSystemAccessContext>
          file_system_access_context,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner,
      scoped_refptr<base::SequencedTaskRunner> custom_task_runner);

  IndexedDBControlWrapper(const IndexedDBControlWrapper&) = delete;
  IndexedDBControlWrapper& operator=(const IndexedDBControlWrapper&) = delete;

  ~IndexedDBControlWrapper() override;

  // Returns the mojom interface to the `IndexedDBContextImpl`, creating the
  // context if it does not already exist.
  storage::mojom::IndexedDBControl& GetIndexedDBControl();

  // mojom::IndexedDBObserver:
  void OnIndexedDBListChanged(
      const storage::BucketLocator& bucket_locator) override;
  void OnIndexedDBContentChanged(
      const storage::BucketLocator& bucket_locator,
      const std::u16string& database_name,
      const std::u16string& object_store_name) override {}

 private:
  void OnSpecialStoragePolicyUpdated(
      std::vector<storage::mojom::StoragePolicyUpdatePtr> policy_updates);

  absl::optional<storage::StoragePolicyObserver> storage_policy_observer_;

  mojo::Remote<storage::mojom::IndexedDBControl> indexed_db_control_;
  std::unique_ptr<IndexedDBContextImpl> context_;

  // Used to observe `context_`. Note that `this` only observes `context_` if
  // `storage_policy_observer_` exists.
  mojo::Receiver<storage::mojom::IndexedDBObserver> receiver_{this};

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONTROL_WRAPPER_H_
