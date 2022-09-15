// Copyright 2022 The Chromium Authors
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
GetSharedStorageWorkletHostManagerForStoragePartition(
    StoragePartition* storage_partition) {
  return static_cast<StoragePartitionImpl*>(storage_partition)
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

size_t GetAttachedSharedStorageWorkletHostsCount(
    StoragePartition* storage_partition) {
  SharedStorageWorkletHostManager* manager =
      GetSharedStorageWorkletHostManagerForStoragePartition(storage_partition);
  DCHECK(manager);
  return manager->GetAttachedWorkletHostsForTesting().size();
}

size_t GetKeepAliveSharedStorageWorkletHostsCount(
    StoragePartition* storage_partition) {
  SharedStorageWorkletHostManager* manager =
      GetSharedStorageWorkletHostManagerForStoragePartition(storage_partition);
  DCHECK(manager);
  return manager->GetKeepAliveWorkletHostsForTesting().size();
}

}  // namespace content
