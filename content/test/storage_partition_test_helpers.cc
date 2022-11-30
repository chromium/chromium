// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/storage_partition_test_helpers.h"

#include "content/browser/storage_partition_impl.h"
#include "content/browser/worker_host/shared_worker_service_impl.h"
#include "content/browser/worker_host/test_shared_worker_service_impl.h"
#include "content/public/browser/storage_partition_config.h"

namespace content {

void InjectTestSharedWorkerService(StoragePartition* storage_partition) {
  auto* storage_partition_impl =
      static_cast<StoragePartitionImpl*>(storage_partition);

  storage_partition_impl->OverrideSharedWorkerServiceForTesting(
      std::make_unique<TestSharedWorkerServiceImpl>(
          storage_partition_impl,
          storage_partition_impl->GetServiceWorkerContext()));
}

void TerminateAllSharedWorkers(StoragePartition* storage_partition,
                               base::OnceClosure callback) {
  static_cast<TestSharedWorkerServiceImpl*>(
      storage_partition->GetSharedWorkerService())
      ->TerminateAllWorkers(std::move(callback));
}

StoragePartitionConfig CreateStoragePartitionConfigForTesting(
    bool in_memory,
    const std::string& partition_domain,
    const std::string& partition_name) {
  return StoragePartitionConfig(partition_domain, partition_name, in_memory);
}

CustomStoragePartitionForSomeSites::CustomStoragePartitionForSomeSites(
    const GURL& site_to_isolate)
    : site_to_isolate_(site_to_isolate) {}

StoragePartitionConfig
CustomStoragePartitionForSomeSites::GetStoragePartitionConfigForSite(
    BrowserContext* browser_context,
    const GURL& site) {
  // Override for |site_to_isolate_|.
  if (site == site_to_isolate_) {
    return StoragePartitionConfig::Create(
        browser_context, "blah_isolated_storage", "blah_isolated_storage",
        false /* in_memory */);
  }

  return StoragePartitionConfig::CreateDefault(browser_context);
}

}  // namespace content
