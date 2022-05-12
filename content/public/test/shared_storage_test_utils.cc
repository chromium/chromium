// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/shared_storage_test_utils.h"

#include <map>

#include "base/task/task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/shared_storage/shared_storage_document_service_impl.h"
#include "content/browser/shared_storage/shared_storage_worklet_host.h"
#include "content/browser/shared_storage/shared_storage_worklet_host_manager.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test_utils.h"

namespace content {

namespace {

SharedStorageWorkletHostManager*
GetSharedStorageWorkletHostManagerForRenderFrameHost(
    const ToRenderFrameHost& to_rfh) {
  return static_cast<StoragePartitionImpl*>(to_rfh.render_frame_host()
                                                ->GetBrowserContext()
                                                ->GetDefaultStoragePartition())
      ->GetSharedStorageWorkletHostManager();
}

}  // namespace

std::string GetSharedStorageDisabledMessage() {
  return kSharedStorageDisabledMessage;
}

void SetBypassIsSharedStorageAllowed(bool allow) {
  SharedStorageDocumentServiceImpl::
      GetBypassIsSharedStorageAllowedForTesting() = allow;
}

size_t GetAttachedWorkletHostsCountForRenderFrameHost(
    const ToRenderFrameHost& to_rfh) {
  SharedStorageWorkletHostManager* manager =
      GetSharedStorageWorkletHostManagerForRenderFrameHost(
          to_rfh.render_frame_host());
  DCHECK(manager);
  return manager->GetAttachedWorkletHostsForTesting().size();
}

size_t GetKeepAliveWorkletHostsCountForRenderFrameHost(
    const ToRenderFrameHost& to_rfh) {
  SharedStorageWorkletHostManager* manager =
      GetSharedStorageWorkletHostManagerForRenderFrameHost(
          to_rfh.render_frame_host());
  DCHECK(manager);
  return manager->GetKeepAliveWorkletHostsForTesting().size();
}

}  // namespace content
