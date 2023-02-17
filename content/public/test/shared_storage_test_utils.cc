// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/shared_storage_test_utils.h"

#include <map>

#include "base/functional/overloaded.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/shared_storage/shared_storage_document_service_impl.h"
#include "content/browser/shared_storage/shared_storage_worklet_host.h"
#include "content/browser/shared_storage/shared_storage_worklet_host_manager.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/test/fenced_frame_test_utils.h"
#include "url/gurl.h"

namespace content {

SharedStorageWorkletHostManager*
GetSharedStorageWorkletHostManagerForStoragePartition(
    StoragePartition* storage_partition) {
  return static_cast<StoragePartitionImpl*>(storage_partition)
      ->GetSharedStorageWorkletHostManager();
}

std::string GetSharedStorageDisabledMessage() {
  return kSharedStorageDisabledMessage;
}

std::string GetSharedStorageSelectURLDisabledMessage() {
  return kSharedStorageSelectURLDisabledMessage;
}

std::string GetSharedStorageAddModuleDisabledMessage() {
  return kSharedStorageAddModuleDisabledMessage;
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

RenderFrameHost* CreateFencedFrame(RenderFrameHost* root,
                                   const FencedFrameNavigationTarget& target) {
  FrameTreeNode* root_node =
      static_cast<RenderFrameHostImpl*>(root)->frame_tree_node();
  size_t initial_child_count = root_node->child_count();

  EXPECT_TRUE(ExecJs(root,
                     "var f = document.createElement('fencedframe');"
                     "f.mode = 'opaque-ads';"
                     "document.body.appendChild(f);"));

  EXPECT_EQ(initial_child_count + 1, root_node->child_count());
  FrameTreeNode* fenced_frame_root_node =
      GetFencedFrameRootNode(root_node->child_at(initial_child_count));

  TestFrameNavigationObserver observer(
      fenced_frame_root_node->current_frame_host());

  EvalJsResult result = EvalJs(
      root,
      absl::visit(
          base::Overloaded{
              [](const GURL& url) { return JsReplace("f.src = $1;", url); },
              [](const std::string& config) {
                return JsReplace("f.config =  window[$1]", config);
              },
          },
          target));

  observer.Wait();

  EXPECT_TRUE(result.error.empty());
  if (absl::holds_alternative<GURL>(target)) {
    EXPECT_EQ(result, absl::get<GURL>(target).spec());
  }

  return fenced_frame_root_node->current_frame_host();
}

}  // namespace content
