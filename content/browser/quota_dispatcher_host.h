// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_QUOTA_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_QUOTA_DISPATCHER_HOST_H_

#include "base/macros.h"
#include "content/public/browser/quota_permission_context.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "storage/browser/quota/quota_manager.h"
#include "third_party/blink/public/mojom/quota/quota_dispatcher_host.mojom.h"

namespace storage {
class QuotaManager;
}

namespace url {
class Origin;
}

namespace content {
class QuotaPermissionContext;
class RenderProcessHost;

class QuotaDispatcherHost : public blink::mojom::QuotaDispatcherHost {
 public:
  static void CreateForWorker(
      mojo::PendingReceiver<blink::mojom::QuotaDispatcherHost> receiver,
      RenderProcessHost* host,
      const url::Origin& origin);

  static void CreateForFrame(
      RenderProcessHost* host,
      int render_frame_id,
      mojo::PendingReceiver<blink::mojom::QuotaDispatcherHost> receiver);

  QuotaDispatcherHost(int process_id,
                      int render_frame_id,
                      storage::QuotaManager* quota_manager,
                      scoped_refptr<QuotaPermissionContext> permission_context);

  ~QuotaDispatcherHost() override;

  // blink::mojom::QuotaDispatcherHost:
  void QueryStorageUsageAndQuota(
      const url::Origin& origin,
      blink::mojom::StorageType storage_type,
      QueryStorageUsageAndQuotaCallback callback) override;
  void RequestStorageQuota(const url::Origin& origin,
                           blink::mojom::StorageType storage_type,
                           uint64_t requested_size,
                           RequestStorageQuotaCallback callback) override;

 private:
  void DidQueryStorageUsageAndQuota(QueryStorageUsageAndQuotaCallback callback,
                                    blink::mojom::QuotaStatusCode status,
                                    int64_t usage,
                                    int64_t quota,
                                    blink::mojom::UsageBreakdownPtr);
  void DidGetPersistentUsageAndQuota(const url::Origin& origin,
                                     blink::mojom::StorageType storage_type,
                                     uint64_t requested_quota,
                                     RequestStorageQuotaCallback callback,
                                     blink::mojom::QuotaStatusCode status,
                                     int64_t usage,
                                     int64_t quota);
  void DidGetPermissionResponse(
      const url::Origin& origin,
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

  // The ID of this process.
  const int process_id_;
  // The ID of this render frame, MSG_ROUTING_NONE for workers.
  const int render_frame_id_;

  scoped_refptr<storage::QuotaManager> quota_manager_;
  scoped_refptr<QuotaPermissionContext> permission_context_;

  base::WeakPtrFactory<QuotaDispatcherHost> weak_factory_{this};

  DISALLOW_IMPLICIT_CONSTRUCTORS(QuotaDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_QUOTA_DISPATCHER_HOST_H_
