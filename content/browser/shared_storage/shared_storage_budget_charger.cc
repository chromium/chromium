// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_storage/shared_storage_budget_charger.h"

#include "components/services/storage/shared_storage/shared_storage_manager.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/navigation_handle.h"

namespace content {

SharedStorageBudgetCharger::SharedStorageBudgetCharger(
    WebContents* web_contents)
    : WebContentsObserver(web_contents),
      WebContentsUserData<SharedStorageBudgetCharger>(*web_contents) {}

SharedStorageBudgetCharger::~SharedStorageBudgetCharger() = default;

void SharedStorageBudgetCharger::DidStartNavigation(
    NavigationHandle* navigation_handle) {
  // We are only interested in renderer-initiated navigations taking place in
  // the main frame of the primary frame tree.
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->IsRendererInitiated()) {
    return;
  }

  RenderFrameHostImpl* initiator_frame_host =
      navigation_handle->GetInitiatorFrameToken().has_value()
          ? RenderFrameHostImpl::FromFrameToken(
                navigation_handle->GetInitiatorProcessId(),
                navigation_handle->GetInitiatorFrameToken().value())
          : nullptr;

  base::UmaHistogramBoolean(
      "Navigation.MainFrame.RendererInitiated.InitiatorFramePresentAtStart",
      initiator_frame_host);

  // Skip if we cannot find the initiator frame host. This can happen when the
  // initiator frame starts a top navigation and then triggers its own
  // destruction by navigating to a cross-origin frame, so that it may no longer
  // exist by the time we get here.
  //
  // The risk of getting unlimited budget this way seems to be small: the ideal
  // timing can vary from time to time, and whether the timing exist at all also
  // depends on the ordering of messaging.
  //
  // For now, allow the leak and track with UMA (and revisit as needed).
  // https://crbug.com/1331111
  if (!initiator_frame_host)
    return;

  storage::SharedStorageManager* shared_storage_manager =
      initiator_frame_host->GetStoragePartition()->GetSharedStorageManager();

  std::vector<const SharedStorageBudgetMetadata*>
      shared_storage_budget_metadata = initiator_frame_host->frame_tree_node()
                                           ->FindSharedStorageBudgetMetadata();

  for (const auto* metadata : shared_storage_budget_metadata) {
    if (metadata->top_navigated) {
      continue;
    }

    // We only want to charge the budget the first time a navigation leaves the
    // fenced frame, across all fenced frames navigated to the same urn.
    // We can do this even though the pointer is const because
    // `top_navigated` is a mutable field of `SharedStorageBudgetMetadata`.
    metadata->top_navigated = true;

    if (metadata->budget_to_charge == 0) {
      continue;
    }

    shared_storage_manager->MakeBudgetWithdrawal(
        metadata->site, metadata->budget_to_charge, base::DoNothing());
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SharedStorageBudgetCharger);

}  // namespace content
