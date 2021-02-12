// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_CONTEXT_IMPL_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_CONTEXT_IMPL_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/sequence_bound.h"
#include "components/services/storage/public/mojom/blob_storage_context.mojom.h"
#include "components/services/storage/public/mojom/cache_storage_control.mojom.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "content/common/content_export.h"
#include "content/public/browser/cache_storage_context.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/cross_origin_embedder_policy.mojom.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom-forward.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}

namespace storage {
class QuotaManagerProxy;
}

namespace url {
class Origin;
}

namespace content {

class CacheStorageDispatcherHost;
class CacheStorageManager;

// One instance of this exists per StoragePartition, and services multiple
// child processes/origins. Most logic is delegated to the owned
// CacheStorageManager instance, which is only accessed on the target
// sequence.
class CONTENT_EXPORT CacheStorageContextImpl : public CacheStorageContext {
 public:
  CacheStorageContextImpl();

  // Init and Shutdown are for use on the UI thread when the profile,
  // storagepartition is being setup and torn down.
  void Init(const base::FilePath& user_data_directory,
            scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
            mojo::PendingRemote<storage::mojom::BlobStorageContext>
                blob_storage_context);
  void Shutdown();

  void Bind(mojo::PendingReceiver<storage::mojom::CacheStorageControl> control);

  // storage::mojom::CacheStorageControl implementation.
  void AddReceiver(
      const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy,
      mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
          coep_reporter_remote,
      const url::Origin& origin,
      storage::mojom::CacheStorageOwner owner,
      mojo::PendingReceiver<blink::mojom::CacheStorage> receiver) override;
  void DeleteForOrigin(const url::Origin& origin) override;
  void GetAllOriginsInfo(
      storage::mojom::CacheStorageControl::GetAllOriginsInfoCallback callback)
      override;
  void AddObserver(mojo::PendingRemote<storage::mojom::CacheStorageObserver>
                       observer) override;
  void ApplyPolicyUpdates(std::vector<storage::mojom::StoragePolicyUpdatePtr>
                              policy_updates) override;

  scoped_refptr<CacheStorageManager> cache_manager() {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    return cache_manager_;
  }

  bool is_incognito() const { return is_incognito_; }

 protected:
  ~CacheStorageContextImpl() override;

 private:
  void CreateCacheStorageManagerOnTaskRunner(
      const base::FilePath& user_data_directory,
      scoped_refptr<base::SequencedTaskRunner> cache_task_runner,
      scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
      mojo::PendingRemote<storage::mojom::BlobStorageContext>
          blob_storage_context);

  void ShutdownOnTaskRunner(std::set<url::Origin> origins_to_purge_on_shutdown);

  // Initialized at construction.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // The set of origins whose storage should be cleared on shutdown.
  std::set<url::Origin> origins_to_purge_on_shutdown_;

  // Initialized in Init(); true if the user data directory is empty.
  bool is_incognito_ = false;

  // Created and accessed on the target sequence.  Released on the target
  // sequence in ShutdownOnTaskRunner() or the destructor via
  // SequencedTaskRunner::ReleaseSoon().
  scoped_refptr<CacheStorageManager> cache_manager_;

  mojo::ReceiverSet<storage::mojom::CacheStorageControl> receivers_;

  // Initialized from the UI thread and bound to |task_runner_|.
  base::SequenceBound<CacheStorageDispatcherHost> dispatcher_host_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_CONTEXT_IMPL_H_
