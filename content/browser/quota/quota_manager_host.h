// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_QUOTA_QUOTA_MANAGER_HOST_H_
#define CONTENT_BROWSER_QUOTA_QUOTA_MANAGER_HOST_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/quota/quota_change_dispatcher.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "storage/browser/quota/quota_manager.h"
#include "third_party/blink/public/mojom/quota/quota_manager_host.mojom.h"

namespace blink {
class StorageKey;
}

namespace storage {
class QuotaManager;
}

namespace content {

// Implements the Quota (Storage) API for a single StorageKey.
//
// QuotaContext indirectly owns all QuotaManagerHost instances associated with a
// StoragePartition. A new instance is created for every incoming mojo
// connection from a frame or worker. Instances are destroyed when their
// corresponding mojo connections are closed, or when QuotaContext is destroyed.
//
// This class is thread-hostile and must only be used on the browser's IO
// thread. This requirement is a consequence of interacting with
// storage::QuotaManager, which must be used from the IO thread. This situation
// is likely to change when QuotaManager moves to the Storage Service.
class QuotaManagerHost : public blink::mojom::QuotaManagerHost {
 public:
  // The owner must guarantee that `quota_manager` outlives this instance.
  QuotaManagerHost(
      const blink::StorageKey& storage_key,
      storage::QuotaManager* quota_manager,
      scoped_refptr<QuotaChangeDispatcher> quota_change_dispatcher);

  QuotaManagerHost(const QuotaManagerHost&) = delete;
  QuotaManagerHost& operator=(const QuotaManagerHost&) = delete;

  ~QuotaManagerHost() override;

  // blink::mojom::QuotaManagerHost:
  void AddChangeListener(
      mojo::PendingRemote<blink::mojom::QuotaChangeListener> mojo_listener,
      AddChangeListenerCallback callback) override;
  void QueryStorageUsageAndQuota(
      QueryStorageUsageAndQuotaCallback callback) override;

 private:
  void DidQueryStorageUsageAndQuota(QueryStorageUsageAndQuotaCallback callback,
                                    blink::mojom::QuotaStatusCode status,
                                    int64_t usage,
                                    int64_t quota,
                                    blink::mojom::UsageBreakdownPtr);

  // The storage key of the frame or worker connected to this host.
  const blink::StorageKey storage_key_;

  // Raw pointer use is safe because the QuotaContext that indirectly owns this
  // QuotaManagerHost owner holds a reference to the QuotaManager. Therefore
  // the QuotaManager is guaranteed to outlive this QuotaManagerHost.
  const raw_ptr<storage::QuotaManager> quota_manager_;

  scoped_refptr<QuotaChangeDispatcher> quota_change_dispatcher_;

  base::WeakPtrFactory<QuotaManagerHost> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_QUOTA_QUOTA_MANAGER_HOST_H_
