// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/browser/web_test_attribution_manager.h"

#include <utility>

#include "content/public/browser/storage_partition.h"

namespace content {

WebTestAttributionManager::WebTestAttributionManager(
    StoragePartition& storage_partition)
    : storage_partition_(&storage_partition) {}

void WebTestAttributionManager::Reset(ResetCallback callback) {
  storage_partition_->ResetAttributionManagerForTesting(std::move(callback));
}

}  // namespace content
