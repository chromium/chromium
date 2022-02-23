// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_WORKLET_HOST_MANAGER_H_
#define CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_WORKLET_HOST_MANAGER_H_

#include <map>
#include <memory>

#include "content/common/content_export.h"

namespace content {

class SharedStorageDocumentServiceImpl;
class SharedStorageWorkletDriver;
class SharedStorageWorkletHost;

// Manages the creation and destruction of the `SharedStorageWorkletHost`. The
// manager is bound to the StoragePartition.
class CONTENT_EXPORT SharedStorageWorkletHostManager {
 public:
  SharedStorageWorkletHostManager();
  virtual ~SharedStorageWorkletHostManager();

  void OnDocumentServiceDestroyed(
      SharedStorageDocumentServiceImpl* document_service);

  SharedStorageWorkletHost* GetOrCreateSharedStorageWorkletHost(
      SharedStorageDocumentServiceImpl* document_service);

 protected:
  const std::map<SharedStorageDocumentServiceImpl*,
                 std::unique_ptr<SharedStorageWorkletHost>>&
  GetAttachedWorkletHostsForTesting() {
    return attached_shared_storage_worklet_hosts_;
  }

  const std::map<SharedStorageWorkletHost*,
                 std::unique_ptr<SharedStorageWorkletHost>>&
  GetKeepAliveWorkletHostsForTesting() {
    return keep_alive_shared_storage_worklet_hosts_;
  }

  void OnWorkletKeepAliveFinished(SharedStorageWorkletHost*);

  // virtual for testing
  virtual std::unique_ptr<SharedStorageWorkletHost>
  CreateSharedStorageWorkletHost(
      std::unique_ptr<SharedStorageWorkletDriver> driver,
      SharedStorageDocumentServiceImpl& document_service);

 private:
  // The hosts that are attached to the worklet's owner document. Those hosts
  // are created on demand when the `SharedStorageDocumentServiceImpl` requests
  // it. When the corresponding document is destructed (where it will call
  // `OnDocumentServiceDestroyed`), those hosts will either be removed from this
  // map entirely, or will be moved from this map to
  // `keep_alive_shared_storage_worklet_hosts_`, depending on whether there are
  // pending operations.
  std::map<SharedStorageDocumentServiceImpl*,
           std::unique_ptr<SharedStorageWorkletHost>>
      attached_shared_storage_worklet_hosts_;

  // The hosts that are detached from the worklet's owner document and have
  // entered keep-alive phase.
  std::map<SharedStorageWorkletHost*, std::unique_ptr<SharedStorageWorkletHost>>
      keep_alive_shared_storage_worklet_hosts_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_WORKLET_HOST_MANAGER_H_
