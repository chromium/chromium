// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_SCOPED_ACCESSIBILITY_MODE_OVERRIDE_H_
#define CONTENT_PUBLIC_TEST_SCOPED_ACCESSIBILITY_MODE_OVERRIDE_H_

#include "content/public/browser/browser_accessibility_state.h"
#include "ui/accessibility/ax_mode.h"

namespace content {

class ScopedAccessibilityModeOverride {
 public:
  explicit ScopedAccessibilityModeOverride(ui::AXMode mode) : mode_(mode) {
    BrowserAccessibilityState::GetInstance()->AddAccessibilityModeFlags(mode);
  }
  ~ScopedAccessibilityModeOverride() { ResetMode(); }

  void ResetMode() {
    BrowserAccessibilityState::GetInstance()->RemoveAccessibilityModeFlags(
        mode_);
  }

 private:
  const ui::AXMode mode_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_SCOPED_ACCESSIBILITY_MODE_OVERRIDE_H_
