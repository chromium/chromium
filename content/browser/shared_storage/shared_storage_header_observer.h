// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_HEADER_OBSERVER_H_
#define CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_HEADER_OBSERVER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/services/storage/shared_storage/shared_storage_manager.h"
#include "content/browser/storage_partition_impl.h"
#include "content/common/content_export.h"
#include "services/network/public/mojom/optional_bool.mojom.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

namespace content {

class RenderFrameHost;

// Receives notifications from `StoragePartitionImpl` when a parsed
// "Shared-Storage-Write" header is received from the network service. The
// parsed header takes the form of a vector of StructPtrs bundling operation
// types with any parameters. The corresponding shared storage operations are
// invoked in the same order, omitting any operations that are missing any
// necessary parameters or for which any necessary parameters are invalid.
class CONTENT_EXPORT SharedStorageHeaderObserver {
 public:
  using OperationResult = storage::SharedStorageManager::OperationResult;
  using OperationType = network::mojom::SharedStorageOperationType;
  using OperationPtr = network::mojom::SharedStorageOperationPtr;

  explicit SharedStorageHeaderObserver(StoragePartitionImpl* storage_partition);
  virtual ~SharedStorageHeaderObserver();

  // If true, allows operations to bypass the permission check in
  // `IsSharedStorageAllowed()` for testing.
  static bool& GetBypassIsSharedStorageAllowedForTesting();

  // Called by `StoragePartitionImpl` to notify that a parsed
  // "Shared-Storage-Write" header `operations` for a request to
  // `request_origin` from `rfh` has been received. For each operation in
  // `operations`, validates each operation and then invokes each valid
  // operation in the order received, skipping any operations that are missing
  // any necessary parameters or for which any necessary parameters are
  // invalid..
  void HeaderReceived(const url::Origin& request_origin,
                      RenderFrameHost* rfh,
                      std::vector<OperationPtr> operations,
                      base::OnceClosure callback);

 protected:
  // virtual for testing.
  virtual void OnHeaderProcessed(const url::Origin& request_origin,
                                 const std::vector<bool>& header_results) {}
  virtual void OnOperationFinished(const url::Origin& request_origin,
                                   OperationPtr operation,
                                   OperationResult result) {}

 private:
  static bool& GetBypassIsSharedStorageAllowed();

  bool Invoke(const url::Origin& request_origin, OperationPtr operation);

  bool Set(const url::Origin& request_origin,
           std::string key,
           std::string value,
           network::mojom::OptionalBool ignore_if_present);
  bool Append(const url::Origin& request_origin,
              std::string key,
              std::string value);
  bool Delete(const url::Origin& request_origin, std::string key);
  bool Clear(const url::Origin& request_origin);

  storage::SharedStorageManager* GetSharedStorageManager();

  bool IsSharedStorageAllowed(RenderFrameHost* rfh,
                              const url::Origin& request_origin);

  // `storage_partition_` owns `this`, so it will outlive `this`.
  raw_ptr<StoragePartitionImpl, DanglingUntriaged> storage_partition_;

  base::WeakPtrFactory<SharedStorageHeaderObserver> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_HEADER_OBSERVER_H_
