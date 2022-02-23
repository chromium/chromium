// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/commit_deferring_condition.h"

#include "content/browser/renderer_host/navigation_request.h"

namespace content {

CommitDeferringCondition::CommitDeferringCondition(
    NavigationHandle& navigation_handle)
    : navigation_handle_(navigation_handle.GetSafeRef()) {}

CommitDeferringCondition::~CommitDeferringCondition() = default;

}  // namespace content
