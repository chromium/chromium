// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_QUOTA_CLIENT_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_QUOTA_CLIENT_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "content/common/content_export.h"
#include "storage/browser/quota/quota_client.h"
#include "storage/browser/quota/quota_client_type.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

namespace url {
class Origin;
}  // namespace url

namespace content {
class ServiceWorkerContextWrapper;

class ServiceWorkerQuotaClient : public storage::QuotaClient {
 public:
  CONTENT_EXPORT explicit ServiceWorkerQuotaClient(
      ServiceWorkerContextWrapper* context);

  ServiceWorkerQuotaClient(const ServiceWorkerQuotaClient&) = delete;
  ServiceWorkerQuotaClient& operator=(const ServiceWorkerQuotaClient&) = delete;

  // QuotaClient method overrides
  void OnQuotaManagerDestroyed() override {}
  void GetOriginUsage(const url::Origin& origin,
                      blink::mojom::StorageType type,
                      GetOriginUsageCallback callback) override;
  void GetOriginsForType(blink::mojom::StorageType type,
                         GetOriginsForTypeCallback callback) override;
  void GetOriginsForHost(blink::mojom::StorageType type,
                         const std::string& host,
                         GetOriginsForHostCallback callback) override;
  void DeleteOriginData(const url::Origin& origin,
                        blink::mojom::StorageType type,
                        DeleteOriginDataCallback callback) override;
  void PerformStorageCleanup(blink::mojom::StorageType type,
                             PerformStorageCleanupCallback callback) override;

 private:
  friend class ServiceWorkerContextWrapper;
  friend class ServiceWorkerQuotaClientTest;

  ~ServiceWorkerQuotaClient() override;

  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<ServiceWorkerContextWrapper> context_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_QUOTA_CLIENT_H_
