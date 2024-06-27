// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_CONTEXT_IMPL_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_CONTEXT_IMPL_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/sequence_bound.h"
#include "components/services/storage/public/cpp/quota_error_or.h"
#include "components/services/storage/public/mojom/blob_storage_context.mojom.h"
#include "components/services/storage/public/mojom/cache_storage_control.mojom.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/cross_origin_embedder_policy.mojom.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom-forward.h"
#include "url/origin.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}

namespace storage {
struct BucketLocator;
struct BucketInfo;
}

namespace content {

class CacheStorageDispatcherHost;
class CacheStorageManager;

// This class is an implementation of the CacheStorageControl mojom that is
// called from the browser.  One instance of this exists per StoragePartition,
// and services multiple child processes/origins.  (Compare this with
// CacheStorageDispatcherHost which handles renderer <-> storage service mojo
// messages.)  All functions must be called on the same sequence that the
// object is constructed on.
class CONTENT_EXPORT CacheStorageContextImpl
    : public storage::mojom::CacheStorageControl {
 public:
  explicit CacheStorageContextImpl(
      scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy);
  ~CacheStorageContextImpl() override;

  static scoped_refptr<base::SequencedTaskRunner> CreateSchedulerTaskRunner();

  void Init(mojo::PendingReceiver<storage::mojom::CacheStorageControl> control,
            const base::FilePath& user_data_directory,
            mojo::PendingReceiver<storage::mojom::QuotaClient>
                cache_storage_client_remote,
            mojo::PendingReceiver<storage::mojom::QuotaClient>
                background_fetch_client_remote,
            mojo::PendingRemote<storage::mojom::BlobStorageContext>
                blob_storage_context);

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

  scoped_refptr<CacheStorageManager> cache_manager() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return cache_manager_;
  }

  bool is_incognito() const { return is_incognito_; }

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  void AddReceiverWithBucketInfo(
      const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy,
      mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
          coep_reporter_remote,
      const network::DocumentIsolationPolicy& document_isolation_policy,
      const blink::StorageKey& storage_key,
      storage::mojom::CacheStorageOwner owner,
      mojo::PendingReceiver<blink::mojom::CacheStorage> receiver,
      storage::QuotaErrorOr<storage::BucketInfo> result);

  // The set of storage keys whose storage should be cleared on shutdown.
  std::set<url::Origin> origins_to_purge_on_shutdown_;

  // Initialized in Init(); true if the user data directory is empty.
  bool is_incognito_ = false;

  // Released during Shutdown() or the destructor.
  scoped_refptr<CacheStorageManager> cache_manager_;

  mojo::ReceiverSet<storage::mojom::CacheStorageControl> receivers_;

  std::unique_ptr<CacheStorageDispatcherHost> dispatcher_host_;

  const scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy_;

  base::WeakPtrFactory<CacheStorageContextImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_CONTEXT_IMPL_H_
