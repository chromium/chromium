// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_DATABASE_DATABASE_QUOTA_CLIENT_H_
#define STORAGE_BROWSER_DATABASE_DATABASE_QUOTA_CLIENT_H_

#include <set>
#include <string>

#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "components/services/storage/public/cpp/storage_key_quota_client.h"
#include "storage/browser/quota/quota_client_type.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

namespace blink {
class StorageKey;
}  // namespace blink

namespace storage {

class DatabaseTracker;

// Integrates WebSQL databases with the quota management system.
//
// This interface is used on the IO thread by the quota manager.
class COMPONENT_EXPORT(STORAGE_BROWSER) DatabaseQuotaClient
    : public StorageKeyQuotaClient {
 public:
  explicit DatabaseQuotaClient(DatabaseTracker& tracker);

  DatabaseQuotaClient(const DatabaseQuotaClient&) = delete;
  DatabaseQuotaClient& operator=(const DatabaseQuotaClient&) = delete;

  ~DatabaseQuotaClient() override;

  // StorageKeyQuotaClient method overrides.
  void GetStorageKeyUsage(const blink::StorageKey& storage_key,
                          blink::mojom::StorageType type,
                          GetStorageKeyUsageCallback callback) override;
  void GetStorageKeysForType(blink::mojom::StorageType type,
                             GetStorageKeysForTypeCallback callback) override;
  void GetStorageKeysForHost(blink::mojom::StorageType type,
                             const std::string& host,
                             GetStorageKeysForHostCallback callback) override;
  void DeleteStorageKeyData(const blink::StorageKey& storage_key,
                            blink::mojom::StorageType type,
                            DeleteStorageKeyDataCallback callback) override;
  void PerformStorageCleanup(blink::mojom::StorageType type,
                             PerformStorageCleanupCallback callback) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // Reference use is safe here because the DatabaseTracker owns this.
  DatabaseTracker& db_tracker_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_DATABASE_DATABASE_QUOTA_CLIENT_H_
