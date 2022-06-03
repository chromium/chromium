// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_WORKLET_HOST_MANAGER_H_
#define CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_WORKLET_HOST_MANAGER_H_

#include <map>
#include <memory>

#include "content/common/content_export.h"

namespace content {

class SharedStorageWorkletHost;
class SharedStorageDocumentServiceImpl;
class SharedStorageWorkletDriver;
class RenderFrameHost;

// Manages the creation and destruction of worklet hosts. The manager is bound
// to the StoragePartition.
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
  GetWorkletHostsForTesting() {
    return shared_storage_worklet_hosts_;
  }

  // virtual for testing
  virtual std::unique_ptr<SharedStorageWorkletHost>
  CreateSharedStorageWorkletHost(
      std::unique_ptr<SharedStorageWorkletDriver> driver,
      RenderFrameHost& render_frame_host);

 private:
  // Those worklet hosts are created on demand when the
  // `SharedStorageDocumentServiceImpl` requests it. They will be removed from
  // the map when the corresponding document is destructed (where it will call
  // `OnDocumentServiceDestroyed`).
  std::map<SharedStorageDocumentServiceImpl*,
           std::unique_ptr<SharedStorageWorkletHost>>
      shared_storage_worklet_hosts_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_WORKLET_HOST_MANAGER_H_
