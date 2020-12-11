// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_QUOTA_QUOTA_MANAGER_HOST_H_
#define CONTENT_BROWSER_QUOTA_QUOTA_MANAGER_HOST_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/quota/quota_change_dispatcher.h"
#include "content/public/browser/quota_permission_context.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "storage/browser/quota/quota_manager.h"
#include "third_party/blink/public/mojom/quota/quota_manager_host.mojom.h"

namespace storage {
class QuotaManager;
}

namespace url {
class Origin;
}

namespace content {
class QuotaPermissionContext;

// Implements the Quota (Storage) API for a single origin.
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
  // The owner must guarantee that |quota_manager| and |permission_context|
  // outlive this instance.
  QuotaManagerHost(
      int process_id,
      int render_frame_id,
      const url::Origin& origin,
      storage::QuotaManager* quota_manager,
      QuotaPermissionContext* permission_context,
      scoped_refptr<QuotaChangeDispatcher> quota_change_dispatcher);

  QuotaManagerHost(const QuotaManagerHost&) = delete;
  QuotaManagerHost& operator=(const QuotaManagerHost&) = delete;

  ~QuotaManagerHost() override;

  // blink::mojom::QuotaManagerHost:
  void AddChangeListener(
      mojo::PendingRemote<blink::mojom::QuotaChangeListener> mojo_listener,
      AddChangeListenerCallback callback) override;
  void QueryStorageUsageAndQuota(
      blink::mojom::StorageType storage_type,
      QueryStorageUsageAndQuotaCallback callback) override;
  void RequestStorageQuota(blink::mojom::StorageType storage_type,
                           uint64_t requested_size,
                           RequestStorageQuotaCallback callback) override;

 private:
  void DidQueryStorageUsageAndQuota(QueryStorageUsageAndQuotaCallback callback,
                                    blink::mojom::QuotaStatusCode status,
                                    int64_t usage,
                                    int64_t quota,
                                    blink::mojom::UsageBreakdownPtr);
  void DidGetPersistentUsageAndQuota(blink::mojom::StorageType storage_type,
                                     uint64_t requested_quota,
                                     RequestStorageQuotaCallback callback,
                                     blink::mojom::QuotaStatusCode status,
                                     int64_t usage,
                                     int64_t quota);
  void DidGetPermissionResponse(
      uint64_t requested_quota,
      int64_t current_usage,
      int64_t current_quota,
      RequestStorageQuotaCallback callback,
      QuotaPermissionContext::QuotaPermissionResponse response);
  void DidSetHostQuota(int64_t current_usage,
                       RequestStorageQuotaCallback callback,
                       blink::mojom::QuotaStatusCode status,
                       int64_t new_quota);
  void DidGetTemporaryUsageAndQuota(int64_t requested_quota,
                                    RequestStorageQuotaCallback callback,
                                    blink::mojom::QuotaStatusCode status,
                                    int64_t usage,
                                    int64_t quota);

  // The ID of the renderer process connected to this host.
  const int process_id_;

  // The ID of the RenderFrame connected to this host.
  //
  // MSG_ROUTING_NONE if this host is connected to a worker.
  const int render_frame_id_;

  // The origin of the frame or worker connected to this host.
  const url::Origin origin_;

  // Raw pointer use is safe because the QuotaContext that indirectly owns this
  // QuotaManagerHost owner holds a reference to the QuotaManager. Therefore
  // the QuotaManager is guaranteed to outlive this QuotaManagerHost.
  storage::QuotaManager* const quota_manager_;

  // Raw pointer use is safe because the QuotaContext that indirectly owns this
  // QuotaManagerHost owner holds a reference to the QuotaPermissionContext.
  // Therefore the QuotaPermissionContext is guaranteed to outlive this
  // QuotaManagerHost.
  QuotaPermissionContext* const permission_context_;

  scoped_refptr<QuotaChangeDispatcher> quota_change_dispatcher_;

  base::WeakPtrFactory<QuotaManagerHost> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_QUOTA_QUOTA_MANAGER_HOST_H_
