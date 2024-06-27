// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_CONTROL_WRAPPER_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_CONTROL_WRAPPER_H_

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/services/storage/public/mojom/cache_storage_control.mojom.h"
#include "content/browser/cache_storage/cache_storage_context_impl.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "storage/browser/quota/storage_policy_observer.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

namespace content {

// This class is a browser-side implementation of the browser <-> storage
// service mojo for cache storage. It wraps mojo calls to track storage keys
// usage and forwards them to the storage service remote. All functions should
// be called on the UI thread.
class CacheStorageControlWrapper : public storage::mojom::CacheStorageControl {
 public:
  CacheStorageControlWrapper(
      scoped_refptr<base::SequencedTaskRunner> io_task_runner,
      const base::FilePath& user_data_directory,
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
      scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
      mojo::PendingRemote<storage::mojom::BlobStorageContext>
          blob_storage_context);
  ~CacheStorageControlWrapper() override;

  CacheStorageControlWrapper(const CacheStorageControlWrapper&) = delete;
  CacheStorageControlWrapper& operator=(const CacheStorageControlWrapper&) =
      delete;

  storage::mojom::CacheStorageControl* GetCacheStorageControl() {
    return cache_storage_control_.get();
  }

  // storage::mojom::CacheStorageControl implementation.
  void AddReceiver(
      const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy,
      mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
          coep_reporter_remote,
      const network::DocumentIsolationPolicy& document_isolation_policy,
      const storage::BucketLocator& bucket,
      storage::mojom::CacheStorageOwner owner,
      mojo::PendingReceiver<blink::mojom::CacheStorage> receiver) override;
  void AddObserver(mojo::PendingRemote<storage::mojom::CacheStorageObserver>
                       observer) override;
  void ApplyPolicyUpdates(std::vector<storage::mojom::StoragePolicyUpdatePtr>
                              policy_updates) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  std::optional<storage::StoragePolicyObserver> storage_policy_observer_;

  base::SequenceBound<CacheStorageContextImpl> cache_storage_context_;
  mojo::Remote<storage::mojom::CacheStorageControl> cache_storage_control_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_CONTROL_WRAPPER_H_
