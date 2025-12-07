// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/process_allocation_context.h"

#include "base/feature_list.h"

namespace content {

namespace {

// The feature is added for the boosting the renderer priority taken
// by the RFHInitRoot() to at least the same as a spare renderer.
// The renderer taken by RFHInitRoot() may benefit subsequent navigations
// in that new frame.
BASE_FEATURE(kTreatRFHInitRootAsForNavigation,
             base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace

bool ProcessAllocationContext::IsForNavigation() const {
  switch (source) {
    case ProcessAllocationSource::kRFHInitRoot:
      return base::FeatureList::IsEnabled(kTreatRFHInitRootAsForNavigation);
    case ProcessAllocationSource::kNavigationRequest:
      return true;
    case ProcessAllocationSource::kOverrideNavigationParams:
    case ProcessAllocationSource::kCanRequestURL:
    case ProcessAllocationSource::kAuctionProcessManager:
    case ProcessAllocationSource::kServiceWorkerProcessManager:
    case ProcessAllocationSource::kSharedStorageRenderThreadWorkletDriver:
    case ProcessAllocationSource::kSharedWorker:
    case ProcessAllocationSource::kNoProcessCreationExpected:
    case ProcessAllocationSource::kTest:
    case ProcessAllocationSource::kEmbedder:
      return false;
  }
}

ProcessAllocationContext ProcessAllocationContext::CreateForNavigationRequest(
    ProcessAllocationNavigationStage stage,
    int64_t navigation_id,
    bool is_outermost_main_frame) {
  return ProcessAllocationContext{
      ProcessAllocationSource::kNavigationRequest,
      NavigationProcessAllocationContext{
          stage, navigation_id, RequiresNewProcessForCoop(false),
          IsOutermostMainFrame(is_outermost_main_frame)}};
}

}  // namespace content
