// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_CONTEXT_IMPL_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_CONTEXT_IMPL_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list_threadsafe.h"
#include "base/threading/sequence_bound.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "content/common/content_export.h"
#include "content/public/browser/cache_storage_context.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "storage/browser/blob/mojom/blob_storage_context.mojom.h"
#include "storage/browser/quota/special_storage_policy.h"
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

class BrowserContext;
class ChromeBlobStorageContext;
class CacheStorageDispatcherHost;
class CacheStorageManager;

// An intermediate abstract interface that exposes the CacheManager() method.
// This is mainly used in some places instead of the full
// CacheStorageContextImpl to make it easier to write tests where we want to
// provide a specific manager instance.
class CONTENT_EXPORT CacheStorageContextWithManager
    : public CacheStorageContext {
 public:
  // Callable on any sequence.  May return nullptr during shutdown.
  virtual scoped_refptr<CacheStorageManager> CacheManager() = 0;

 protected:
  ~CacheStorageContextWithManager() override = default;
};

// One instance of this exists per StoragePartition, and services multiple
// child processes/origins. Most logic is delegated to the owned
// CacheStorageManager instance, which is only accessed on the target
// sequence.
class CONTENT_EXPORT CacheStorageContextImpl
    : public CacheStorageContextWithManager {
 public:
  explicit CacheStorageContextImpl(BrowserContext* browser_context);

  class Observer {
   public:
    virtual void OnCacheListChanged(const url::Origin& origin) = 0;
    virtual void OnCacheContentChanged(const url::Origin& origin,
                                       const std::string& cache_name) = 0;

   protected:
    virtual ~Observer() {}
  };

  using ObserverList = base::ObserverListThreadSafe<Observer>;

  // Init and Shutdown are for use on the UI thread when the profile,
  // storagepartition is being setup and torn down.
  void Init(const base::FilePath& user_data_directory,
            scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
            scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy);
  void Shutdown();

  // Only callable on the UI thread.
  void AddReceiver(mojo::PendingReceiver<blink::mojom::CacheStorage> receiver,
                   const url::Origin& origin);

  // If called on the cache_storage target sequence the real manager will be
  // returned directly.  If called on any other sequence then a cross-sequence
  // wrapper object will be created and returned instead.
  scoped_refptr<CacheStorageManager> CacheManager() override;

  bool is_incognito() const { return is_incognito_; }

  // This function must be called after this object is created but before any
  // CacheStorageCache operations. It must be called on the UI thread. If
  // |blob_storage_context| is NULL the function immediately returns without
  // forwarding to the CacheStorageManager.
  void SetBlobParametersForCache(
      ChromeBlobStorageContext* blob_storage_context);

  // CacheStorageContext
  void GetAllOriginsInfo(GetUsageInfoCallback callback) override;
  void DeleteForOrigin(const GURL& origin) override;

  // Callable on any sequence.
  void AddObserver(CacheStorageContextImpl::Observer* observer);
  void RemoveObserver(CacheStorageContextImpl::Observer* observer);

 protected:
  ~CacheStorageContextImpl() override;

 private:
  void CreateCacheStorageManagerOnTaskRunner(
      const base::FilePath& user_data_directory,
      scoped_refptr<base::SequencedTaskRunner> cache_task_runner,
      scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy);

  void ShutdownOnTaskRunner();

  void GetBlobStorageMojoContextOnIOThread(
      ChromeBlobStorageContext* blob_storage_context);

  void SetBlobParametersForCacheOnTaskRunner(
      scoped_refptr<BlobStorageContextWrapper> blob_storage_context);

  void CreateQuotaClientsOnIOThread(
      scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy);

  // Initialized at construction.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  const scoped_refptr<ObserverList> observers_;

  // Initialized in Init(); true if the user data directory is empty.
  bool is_incognito_ = false;

  // Initialized in Init().
  scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy_;

  // Created and accessed on the target sequence.  Released on the target
  // sequence in SHutdownOnTaskRunner() or the destructor via
  // SequencedTaskRunner::ReleaseSoon().
  scoped_refptr<CacheStorageManager> cache_manager_;

  // Initialized from the UI thread and bound to |task_runner_|.
  base::SequenceBound<CacheStorageDispatcherHost> dispatcher_host_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_CONTEXT_IMPL_H_
