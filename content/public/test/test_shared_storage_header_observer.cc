// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_shared_storage_header_observer.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "components/services/storage/shared_storage/shared_storage_manager.h"
#include "content/browser/shared_storage/shared_storage_header_observer.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/test/shared_storage_test_utils.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"

namespace content {

TestSharedStorageHeaderObserver::TestSharedStorageHeaderObserver(
    StoragePartition* storage_partition)
    : SharedStorageHeaderObserver(
          static_cast<StoragePartitionImpl*>(storage_partition)) {}

TestSharedStorageHeaderObserver::~TestSharedStorageHeaderObserver() = default;

void TestSharedStorageHeaderObserver::WaitForOperations(size_t expected_total) {
  DCHECK(!loop_);
  if (operations_.size() >= expected_total) {
    return;
  }
  expected_total_ = expected_total;
  loop_ = std::make_unique<base::RunLoop>();
  loop_->Run();
  loop_.reset();
}

void TestSharedStorageHeaderObserver::OnHeaderProcessed(
    const url::Origin& request_origin,
    const std::vector<bool>& header_results) {
  header_results_.emplace_back(request_origin, header_results);
}

void TestSharedStorageHeaderObserver::OnOperationFinished(
    const url::Origin& request_origin,
    OperationPtr operation,
    OperationResult result) {
  operations_.emplace_back(request_origin, std::move(operation), result);

  if (loop_ && loop_->running() && operations_.size() >= expected_total_) {
    loop_->Quit();
  }
}

}  // namespace content
