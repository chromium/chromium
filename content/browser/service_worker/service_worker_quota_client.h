// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_QUOTA_CLIENT_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_QUOTA_CLIENT_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "storage/browser/quota/quota_client_type.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

namespace storage {
struct BucketLocator;
}  // namespace storage

namespace content {
class ServiceWorkerContextCore;

class ServiceWorkerQuotaClient : public storage::mojom::QuotaClient {
 public:
  // `context` must outlive this instance. This is true because `context` owns
  // this instance.
  explicit ServiceWorkerQuotaClient(ServiceWorkerContextCore& context);

  ServiceWorkerQuotaClient(const ServiceWorkerQuotaClient&) = delete;
  ServiceWorkerQuotaClient& operator=(const ServiceWorkerQuotaClient&) = delete;

  ~ServiceWorkerQuotaClient() override;

  // Called when an error causes the ServiceWorkerContextCore to be rebuilt.
  void ResetContext(ServiceWorkerContextCore& new_context) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    context_ = &new_context;
  }

  // storage::mojom::QuotaClient override methods.
  void GetBucketUsage(const storage::BucketLocator& bucket,
                      GetBucketUsageCallback callback) override;
  void GetStorageKeysForType(blink::mojom::StorageType type,
                             GetStorageKeysForTypeCallback callback) override;
  void DeleteBucketData(const storage::BucketLocator& bucket,
                        DeleteBucketDataCallback callback) override;
  void PerformStorageCleanup(blink::mojom::StorageType type,
                             PerformStorageCleanupCallback callback) override;

 private:
  friend class ServiceWorkerContextWrapper;
  friend class ServiceWorkerQuotaClientTest;

  SEQUENCE_CHECKER(sequence_checker_);

  // The raw pointer is safe because `context_` owns this instance.
  //
  // The pointer is guaranteed to be non-null. It is not a reference because
  // ResetContext() changes the object it points to.
  raw_ptr<ServiceWorkerContextCore> context_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_QUOTA_CLIENT_H_
