// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/process_visibility_util.h"

#include "content/common/process_visibility_tracker.h"

namespace content {

void OnBrowserVisibilityChanged(bool visible) {
  ProcessVisibilityTracker::GetInstance()->OnProcessVisibilityChanged(visible);
}

}  // namespace content
