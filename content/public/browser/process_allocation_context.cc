// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/process_allocation_context.h"

namespace content {

ProcessAllocationContext ProcessAllocationContext::CreateForNavigationRequest(
    ProcessAllocationNavigationStage stage,
    int64_t navigation_id) {
  return ProcessAllocationContext{
      ProcessAllocationSource::kNavigationRequest,
      NavigationProcessAllocationContext{stage, navigation_id}};
}

}  // namespace content
