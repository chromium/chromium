// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PROCESS_ALLOCATION_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_PROCESS_ALLOCATION_CONTEXT_H_

#include <stdint.h>

#include <optional>

namespace content {

// ProcessAllocationSource records the source calling
// SiteInstance::GetOrCreateProcess().
enum class ProcessAllocationSource : uint8_t {
  kRFHInitRoot = 0,
  kNavigationRequest,
  kOverrideNavigationParams,
  kCanRequestURL,
  kAuctionProcessManager,
  kServiceWorkerProcessManager,
  kSharedStorageRenderThreadWorkletDriver,
  kSharedWorker,
  // For callsites where GetOrCreateProcess is not expected to create a new
  // process.
  kNoProcessCreationExpected,
  kTest,
  kMaxValue = kTest,
};

// ProcessAllocationSource records when a renderer process is created during the
// navigation. The value is only meaningful when ProcessAllocationSource is
// kNavigationRequest.
enum class ProcessAllocationNavigationStage : uint8_t {
  kNoURLLoader = 0,
  kBeforeNetworkRequest,
  kAfterNetworkRequest,
  kHandlingEarlyHints,
  kAfterResponse,
  kAfterFailure,
  kMaxValue = kAfterFailure,
};

struct NavigationProcessAllocationContext {
  ProcessAllocationNavigationStage stage;
  // The navigation ID that triggered the process allocation.
  int64_t navigation_id;
  // Whether the process allocation is caused by a COOP header.
  // TODO(crbug.com/394732486): The field is added to investigate
  // the process reuse failure when navigating to COOP sites.
  bool requires_new_process_for_coop;
};

struct ProcessAllocationContext {
  static ProcessAllocationContext CreateForNavigationRequest(
      ProcessAllocationNavigationStage stage,
      int64_t navigation_id);

  ProcessAllocationSource source;
  // The navigation_context will be only available when source is
  // kNavigationRequest.
  std::optional<NavigationProcessAllocationContext> navigation_context;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PROCESS_ALLOCATION_CONTEXT_H_
