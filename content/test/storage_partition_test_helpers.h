// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_STORAGE_PARTITION_TEST_HELPERS_H_
#define CONTENT_TEST_STORAGE_PARTITION_TEST_HELPERS_H_

#include "base/functional/callback.h"
#include "content/test/test_content_browser_client.h"

namespace content {
class StoragePartition;
class StoragePartitionConfig;

// Replaces the SharedWorkerService implementation with a test-specific one that
// tracks running shared workers.
void InjectTestSharedWorkerService(StoragePartition* storage_partition);

// Terminates all workers and notifies when complete. This is used for
// testing when it is important to make sure that all shared worker activity
// has stopped. Can only be used if InjectTestSharedWorkerService() was called.
void TerminateAllSharedWorkers(StoragePartition* storage_partition,
                               base::OnceClosure callback);

StoragePartitionConfig CreateStoragePartitionConfigForTesting(
    bool in_memory = false,
    const std::string& partition_domain = "",
    const std::string& partition_name = "");

// Class that requests that all pages belonging to the provided site get loaded
// in a non-default StoragePartition.
class CustomStoragePartitionForSomeSites : public TestContentBrowserClient {
 public:
  explicit CustomStoragePartitionForSomeSites(const GURL& site_to_isolate);

  StoragePartitionConfig GetStoragePartitionConfigForSite(
      BrowserContext* browser_context,
      const GURL& site) override;

 private:
  GURL site_to_isolate_;
};

}  // namespace content

#endif  // CONTENT_TEST_STORAGE_PARTITION_TEST_HELPERS_H_
