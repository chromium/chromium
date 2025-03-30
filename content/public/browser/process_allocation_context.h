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
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(ProcessAllocationSource)
enum class ProcessAllocationSource : uint8_t {
  kRFHInitRoot = 0,
  kNavigationRequest = 1,
  kOverrideNavigationParams = 2,
  kCanRequestURL = 3,
  kAuctionProcessManager = 4,
  kServiceWorkerProcessManager = 5,
  kSharedStorageRenderThreadWorkletDriver = 6,
  kSharedWorker = 7,
  // For callsites where GetOrCreateProcess is not expected to create a new
  // process.
  kNoProcessCreationExpected = 8,
  kTest = 9,
  kMaxValue = kTest,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/browser/enums.xml:ProcessAllocationSource)

// ProcessAllocationSource records when a renderer process is created during the
// navigation. The value is only meaningful when ProcessAllocationSource is
// kNavigationRequest.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(ProcessAllocationNavigationStage)
enum class ProcessAllocationNavigationStage : uint8_t {
  kNoURLLoader = 0,
  kBeforeNetworkRequest = 1,
  kAfterNetworkRequest = 2,
  kHandlingEarlyHints = 3,
  kAfterResponse = 4,
  kAfterFailure = 5,
  kMaxValue = kAfterFailure,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/browser/enums.xml:ProcessAllocationNavigationStage)

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

  bool IsForNavigation() const;

  ProcessAllocationSource source;
  // The navigation_context will be only available when source is
  // kNavigationRequest.
  std::optional<NavigationProcessAllocationContext> navigation_context;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PROCESS_ALLOCATION_CONTEXT_H_
