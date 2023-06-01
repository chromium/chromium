// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_SHARED_STORAGE_HEADER_OBSERVER_H_
#define CONTENT_PUBLIC_TEST_TEST_SHARED_STORAGE_HEADER_OBSERVER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "components/services/storage/shared_storage/shared_storage_manager.h"
#include "content/browser/shared_storage/shared_storage_header_observer.h"
#include "content/public/test/shared_storage_test_utils.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"
#include "url/origin.h"

namespace content {

using OperationResult = storage::SharedStorageManager::OperationResult;
using OperationType = network::mojom::SharedStorageOperationType;
using OperationPtr = network::mojom::SharedStorageOperationPtr;

class StoragePartition;

class TestSharedStorageHeaderObserver : public SharedStorageHeaderObserver {
 public:
  explicit TestSharedStorageHeaderObserver(StoragePartition* storage_partition);

  ~TestSharedStorageHeaderObserver() override;

  const std::vector<SharedStorageWriteOperationAndResult>& operations() const {
    return operations_;
  }

  const std::vector<std::pair<url::Origin, std::vector<bool>>>& header_results()
      const {
    return header_results_;
  }

  base::WeakPtr<TestSharedStorageHeaderObserver> GetMutableWeakPtr() const {
    return weak_ptr_factory_.GetMutableWeakPtr();
  }

  void WaitForOperations(size_t expected_total);

 private:
  // SharedStorageHeaderObserver:
  void OnHeaderProcessed(const url::Origin& request_origin,
                         const std::vector<bool>& header_results) override;
  void OnOperationFinished(const url::Origin& request_origin,
                           OperationPtr operation,
                           OperationResult result) override;

  std::unique_ptr<base::RunLoop> loop_;
  size_t expected_total_;
  std::vector<SharedStorageWriteOperationAndResult> operations_;
  std::vector<std::pair<url::Origin, std::vector<bool>>> header_results_;
  base::WeakPtrFactory<TestSharedStorageHeaderObserver> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_SHARED_STORAGE_HEADER_OBSERVER_H_
