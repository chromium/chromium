// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/immediate_accessibility_mode_policy.h"

#include "base/functional/callback.h"

namespace content {

void ImmediateAccessibilityModePolicy::SetAccessibilityMode(
    ApplyOrClearMode apply_or_clear_mode) {
  std::move(apply_or_clear_mode).Run(/*apply=*/true);
}

}  // namespace content
