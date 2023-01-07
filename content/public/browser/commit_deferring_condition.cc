// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/commit_deferring_condition.h"

#include "content/public/browser/navigation_handle.h"

namespace content {

CommitDeferringCondition::CommitDeferringCondition(
    NavigationHandle& navigation_handle)
    : navigation_handle_(navigation_handle.GetSafeRef()) {}

CommitDeferringCondition::~CommitDeferringCondition() = default;

}  // namespace content
