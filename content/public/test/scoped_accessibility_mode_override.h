// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_SCOPED_ACCESSIBILITY_MODE_OVERRIDE_H_
#define CONTENT_PUBLIC_TEST_SCOPED_ACCESSIBILITY_MODE_OVERRIDE_H_

#include <memory>

#include "ui/accessibility/ax_mode.h"

namespace content {

class ScopedAccessibilityMode;
class WebContents;

class ScopedAccessibilityModeOverride {
 public:
  explicit ScopedAccessibilityModeOverride(ui::AXMode mode);
  ScopedAccessibilityModeOverride(WebContents* web_contents, ui::AXMode mode);
  ScopedAccessibilityModeOverride(ScopedAccessibilityModeOverride&&) noexcept;
  ScopedAccessibilityModeOverride& operator=(
      ScopedAccessibilityModeOverride&&) noexcept;
  ScopedAccessibilityModeOverride(const ScopedAccessibilityModeOverride&) =
      delete;
  ScopedAccessibilityModeOverride& operator=(
      const ScopedAccessibilityModeOverride&) = delete;
  ~ScopedAccessibilityModeOverride();

  void ResetMode();

 private:
  std::unique_ptr<ScopedAccessibilityMode> scoped_mode_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_SCOPED_ACCESSIBILITY_MODE_OVERRIDE_H_
