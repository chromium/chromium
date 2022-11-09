// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_DATABASE_DATABASE_QUOTA_CLIENT_H_
#define STORAGE_BROWSER_DATABASE_DATABASE_QUOTA_CLIENT_H_

#include "base/component_export.h"
#include "base/memory/raw_ref.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "storage/browser/quota/quota_client_type.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

namespace storage {

class DatabaseTracker;
struct BucketLocator;

// Integrates WebSQL databases with the quota management system.
//
// This interface is used on the IO thread by the quota manager.
class COMPONENT_EXPORT(STORAGE_BROWSER) DatabaseQuotaClient
    : public mojom::QuotaClient {
 public:
  explicit DatabaseQuotaClient(DatabaseTracker& tracker);

  DatabaseQuotaClient(const DatabaseQuotaClient&) = delete;
  DatabaseQuotaClient& operator=(const DatabaseQuotaClient&) = delete;

  ~DatabaseQuotaClient() override;

  // mojom::QuotaClient method overrides.
  void GetBucketUsage(const BucketLocator& bucket,
                      GetBucketUsageCallback callback) override;
  void GetStorageKeysForType(blink::mojom::StorageType type,
                             GetStorageKeysForTypeCallback callback) override;
  void DeleteBucketData(const BucketLocator& bucket,
                        DeleteBucketDataCallback callback) override;
  void PerformStorageCleanup(blink::mojom::StorageType type,
                             PerformStorageCleanupCallback callback) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // Reference use is safe here because the DatabaseTracker owns this.
  const raw_ref<DatabaseTracker> db_tracker_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_DATABASE_DATABASE_QUOTA_CLIENT_H_
