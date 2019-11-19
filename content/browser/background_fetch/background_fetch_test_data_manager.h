// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_TEST_DATA_MANAGER_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_TEST_DATA_MANAGER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "content/browser/background_fetch/background_fetch_data_manager.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"
#include "url/origin.h"

namespace content {

class BrowserContext;
class CacheStorageManager;
class ChromeBlobStorageContext;
class MockQuotaManager;
class ServiceWorkerContextWrapper;
class StoragePartition;

// Arbitrary quota that is large enough for test purposes.
constexpr uint64_t kBackgroundFetchMaxQuotaBytes = 424242u;

// Test DataManager that sets up a CacheStorageManager suited for test
// environments. Tests can also optionally override FillServiceWorkerResponse by
// setting |mock_fill_response| to true.
class BackgroundFetchTestDataManager : public BackgroundFetchDataManager {
 public:
  BackgroundFetchTestDataManager(
      BrowserContext* browser_context,
      StoragePartition* storage_partition,
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context);

  ~BackgroundFetchTestDataManager() override;

  void InitializeOnCoreThread() override;

 private:
  friend class BackgroundFetchDataManagerTest;

  scoped_refptr<MockQuotaManager> mock_quota_manager_;
  BrowserContext* browser_context_;
  StoragePartition* storage_partition_;
  scoped_refptr<ChromeBlobStorageContext> blob_storage_context_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchTestDataManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_TEST_DATA_MANAGER_H_
