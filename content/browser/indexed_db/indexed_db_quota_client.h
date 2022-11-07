// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_QUOTA_CLIENT_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_QUOTA_CLIENT_H_

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "content/common/content_export.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_task.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

namespace storage {
struct BucketLocator;
}  // namespace storage

namespace content {
class IndexedDBContextImpl;

// Integrates IndexedDB with the quota management system.
//
// Each instance is owned by an IndexedDBContextImpl.
class IndexedDBQuotaClient : public storage::mojom::QuotaClient {
 public:
  CONTENT_EXPORT explicit IndexedDBQuotaClient(
      IndexedDBContextImpl& indexed_db_context);

  IndexedDBQuotaClient(const IndexedDBQuotaClient&) = delete;
  IndexedDBQuotaClient& operator=(const IndexedDBQuotaClient&) = delete;

  CONTENT_EXPORT ~IndexedDBQuotaClient() override;

  // storage::mojom::QuotaClient implementation:
  void GetBucketUsage(const storage::BucketLocator& bucket,
                      GetBucketUsageCallback callback) override;
  void GetStorageKeysForType(blink::mojom::StorageType type,
                             GetStorageKeysForTypeCallback callback) override;
  void DeleteBucketData(const storage::BucketLocator& bucket,
                        DeleteBucketDataCallback callback) override;
  void PerformStorageCleanup(blink::mojom::StorageType type,
                             PerformStorageCleanupCallback callback) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // Raw pointer use is safe here because the IndexedDBContextImpl owns this.
  const raw_ref<IndexedDBContextImpl> indexed_db_context_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<IndexedDBQuotaClient> weak_ptr_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_QUOTA_CLIENT_H_
