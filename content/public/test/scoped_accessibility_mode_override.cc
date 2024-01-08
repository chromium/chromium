// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/scoped_accessibility_mode_override.h"

#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/scoped_accessibility_mode.h"

namespace content {

ScopedAccessibilityModeOverride::ScopedAccessibilityModeOverride(
    ui::AXMode mode)
    : scoped_mode_(
          BrowserAccessibilityState::GetInstance()->CreateScopedModeForProcess(
              mode)) {}

ScopedAccessibilityModeOverride::~ScopedAccessibilityModeOverride() = default;

void ScopedAccessibilityModeOverride::ResetMode() {
  scoped_mode_.reset();
}

}  // namespace content
