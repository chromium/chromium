// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/private_aggregation/private_aggregation_manager.h"

#include "content/browser/private_aggregation/private_aggregation_manager_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"

namespace content {

PrivateAggregationManager* PrivateAggregationManager::GetManager(
    BrowserContext& browser_context) {
  return static_cast<StoragePartitionImpl*>(
             browser_context.GetDefaultStoragePartition())
      ->GetPrivateAggregationManager();
}

}  // namespace content
