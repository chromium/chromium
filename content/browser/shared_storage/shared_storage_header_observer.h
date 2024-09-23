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
#include "content/browser/navigation_or_document_handle.h"
#include "content/browser/shared_storage/shared_storage_worklet_host_manager.h"
#include "content/browser/storage_partition_impl.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/message.h"
#include "services/network/public/mojom/optional_bool.mojom.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"
#include "url/origin.h"

namespace content {

using AccessType =
    SharedStorageWorkletHostManager::SharedStorageObserverInterface::AccessType;

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
  using ContextType = StoragePartitionImpl::ContextType;

  // Enum for tracking how often the `PermissionsPolicy` double check runs along
  // with its results. Recorded to UMA; always add new values to the end and do
  // not reorder or delete values from this list. If you add any entries to this
  // enum, you must also update the corresponding enum
  // `SharedStorageHeaderObserverPermissionsPolicyDoubleCheckStatus` at
  // tools/metrics/histograms/metadata/storage/enums.xml.
  enum class PermissionsPolicyDoubleCheckStatus {
    // RFH is null, so no double check is run. Any previous permissions
    // policy checks were only done in the renderer; hence operations
    // are dropped.
    kSubresourceSourceNoRFH = 0,
    // RFH has not yet committed. Defer the operations until a corresponding
    // commit notification is received. If none is received, they will be
    // dropped when RFH dies.
    kSubresourceSourceDefer = 1,
    // RFH's LifecycleState is neither kPendingCommit nor kActive. We do not
    // handle these cases as the PermissionsPolicy that we have access to may
    // not be correct. Any operations are dropped.
    kSubresourceSourceOtherLifecycleState = 2,
    // RFH is non-null but has no `PermissionsPolicy`, so no double
    // check is run. Any previous permissions policy checks were only
    // done in the renderer; hence operations are dropped.
    kSubresourceSourceNoPolicy = 3,
    // RFH is non-null but has no `PermissionsPolicy`, so no double
    // check is run, but the request source is an iframe navigation, so
    // a previous browser-side permissions policy check was run in
    // `NavigationRequest`. Hence it is ok to skip the double-check and
    // proceed with the operations.
    kNavigationSourceNoPolicy = 4,
    // The request source is a navigation request for a main frame,
    // which is not supported.
    kDisallowedMainFrameNavigation = 5,
    // A double check is run and the feature is disabled so
    // operations are dropped.
    kDisabled = 6,
    // A double check is run and the feature is enabled so
    // operations are processed.
    kEnabled = 7,

    // Keep this at the end and equal to the last entry.
    kMaxValue = kEnabled,
  };

  explicit SharedStorageHeaderObserver(StoragePartitionImpl* storage_partition);
  virtual ~SharedStorageHeaderObserver();

  // Called by `StoragePartitionImpl` to notify that a parsed
  // "Shared-Storage-Write" header `operations` for a request to
  // `request_origin` from `rfh` has been received. For each operation in
  // `operations`, validates each operation and then invokes each valid
  // operation in the order received, skipping any operations that are missing
  // any necessary parameters or for which any necessary parameters are
  // invalid..
  void HeaderReceived(const url::Origin& request_origin,
                      ContextType context_type,
                      NavigationOrDocumentHandle* navigation_or_document_handle,
                      std::vector<OperationPtr> operations,
                      base::OnceClosure callback,
                      mojo::ReportBadMessageCallback bad_message_callback,
                      bool can_defer);

 protected:
  // virtual for testing.
  virtual void OnHeaderProcessed(const url::Origin& request_origin,
                                 const std::vector<bool>& header_results) {}
  virtual void OnOperationFinished(const url::Origin& request_origin,
                                   OperationPtr operation,
                                   OperationResult result) {}

 private:
  bool Invoke(const url::Origin& request_origin,
              FrameTreeNodeId main_frame_id,
              OperationPtr operation);

  bool Set(const url::Origin& request_origin,
           FrameTreeNodeId main_frame_id,
           std::string key,
           std::string value,
           network::mojom::OptionalBool ignore_if_present);
  bool Append(const url::Origin& request_origin,
              FrameTreeNodeId main_frame_id,
              std::string key,
              std::string value);
  bool Delete(const url::Origin& request_origin,
              FrameTreeNodeId main_frame_id,
              std::string key);
  bool Clear(const url::Origin& request_origin, FrameTreeNodeId main_frame_id);

  storage::SharedStorageManager* GetSharedStorageManager();

  PermissionsPolicyDoubleCheckStatus DoPermissionsPolicyDoubleCheck(
      const url::Origin& request_origin,
      ContextType context_type,
      NavigationOrDocumentHandle* navigation_or_document_handle);

  bool IsSharedStorageAllowedBySiteSettings(
      NavigationOrDocumentHandle* navigation_or_document_handle,
      const url::Origin& request_origin,
      std::string* out_debug_message = nullptr);

  void NotifySharedStorageAccessed(AccessType type,
                                   FrameTreeNodeId main_frame_id,
                                   const url::Origin& request_origin,
                                   const SharedStorageEventParams& params);

  // `storage_partition_` owns `this`, so it will outlive `this`.
  raw_ptr<StoragePartitionImpl> storage_partition_;

  base::WeakPtrFactory<SharedStorageHeaderObserver> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_HEADER_OBSERVER_H_
