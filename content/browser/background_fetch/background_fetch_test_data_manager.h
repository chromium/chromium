// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_TEST_DATA_MANAGER_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_TEST_DATA_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/background_fetch/background_fetch_data_manager.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"

namespace storage {
class MockQuotaManager;
}  // namespace storage

namespace content {

class BrowserContext;
class CacheStorageManager;
class ChromeBlobStorageContext;
class ServiceWorkerContextWrapper;
class StoragePartitionImpl;

// Arbitrary quota that is large enough for test purposes.
constexpr uint64_t kBackgroundFetchMaxQuotaBytes = 424242u;

// Test DataManager that sets up a CacheStorageManager suited for test
// environments. Tests can also optionally override FillServiceWorkerResponse by
// setting |mock_fill_response| to true.
class BackgroundFetchTestDataManager : public BackgroundFetchDataManager {
 public:
  BackgroundFetchTestDataManager(
      BrowserContext* browser_context,
      base::WeakPtr<StoragePartitionImpl> storage_partition,
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context);

  BackgroundFetchTestDataManager(const BackgroundFetchTestDataManager&) =
      delete;
  BackgroundFetchTestDataManager& operator=(
      const BackgroundFetchTestDataManager&) = delete;

  ~BackgroundFetchTestDataManager() override;

  void Initialize() override;

 private:
  friend class BackgroundFetchDataManagerTest;

  scoped_refptr<storage::MockQuotaManager> mock_quota_manager_;
  raw_ptr<BrowserContext> browser_context_;
  base::WeakPtr<StoragePartition> storage_partition_;
  scoped_refptr<ChromeBlobStorageContext> blob_storage_context_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_TEST_DATA_MANAGER_H_
