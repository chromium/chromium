// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_DATABASE_DATABASE_QUOTA_CLIENT_H_
#define STORAGE_BROWSER_DATABASE_DATABASE_QUOTA_CLIENT_H_

#include <set>
#include <string>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "components/services/storage/public/cpp/origin_quota_client.h"
#include "storage/browser/quota/quota_client_type.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/origin.h"

namespace storage {

class DatabaseTracker;

// Integrates WebSQL databases with the quota management system.
//
// This interface is used on the IO thread by the quota manager.
class COMPONENT_EXPORT(STORAGE_BROWSER) DatabaseQuotaClient
    : public OriginQuotaClient {
 public:
  explicit DatabaseQuotaClient(DatabaseTracker& tracker);

  DatabaseQuotaClient(const DatabaseQuotaClient&) = delete;
  DatabaseQuotaClient& operator=(const DatabaseQuotaClient&) = delete;

  ~DatabaseQuotaClient() override;

  // OriginQuotaClient method overrides.
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
  SEQUENCE_CHECKER(sequence_checker_);

  // Reference use is safe here because the DatabaseTracker owns this.
  DatabaseTracker& db_tracker_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_DATABASE_DATABASE_QUOTA_CLIENT_H_
