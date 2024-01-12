// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SCOPED_ACCESSIBILITY_MODE_H_
#define CONTENT_PUBLIC_BROWSER_SCOPED_ACCESSIBILITY_MODE_H_

#include "content/common/content_export.h"
#include "ui/accessibility/ax_mode.h"

namespace content {

// An object that binds the application of one or more accessibility mode flags
// to the process, a BrowserContext, or a WebContents for the duration of its
// lifetime. See BrowserAccessibilityState for factory functions.
class CONTENT_EXPORT ScopedAccessibilityMode {
 public:
  ScopedAccessibilityMode(const ScopedAccessibilityMode&) = delete;
  ScopedAccessibilityMode& operator=(const ScopedAccessibilityMode&) = delete;
  virtual ~ScopedAccessibilityMode() = default;

  // Returns the accessibility mode flags applied by this instance to
  // WebContentses in its scope.
  ui::AXMode mode() const { return mode_; }

 protected:
  explicit ScopedAccessibilityMode(ui::AXMode mode) : mode_(mode) {}

 private:
  const ui::AXMode mode_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SCOPED_ACCESSIBILITY_MODE_H_
