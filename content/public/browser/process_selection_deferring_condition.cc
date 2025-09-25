// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/process_selection_deferring_condition.h"

#include "content/public/browser/navigation_handle.h"

namespace content {
ProcessSelectionDeferringCondition::ProcessSelectionDeferringCondition(
    NavigationHandle& navigation_handle)
    : navigation_handle_(navigation_handle.GetSafeRef()) {}

ProcessSelectionDeferringCondition::~ProcessSelectionDeferringCondition() =
    default;
}  // namespace content
