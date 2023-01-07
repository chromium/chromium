// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregation_service.h"

#include "content/browser/aggregation_service/aggregation_service_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"

namespace content {

// static
AggregationService* AggregationService::GetService(
    BrowserContext* browser_context) {
  return static_cast<StoragePartitionImpl*>(
             browser_context->GetDefaultStoragePartition())
      ->GetAggregationService();
}

}  // namespace content