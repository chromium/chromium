// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_ACCESSIBILITY_MODE_POLICY_H_
#define CONTENT_BROWSER_WEB_CONTENTS_ACCESSIBILITY_MODE_POLICY_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "content/common/content_export.h"
#include "ui/accessibility/ax_mode.h"

namespace content {

class WebContentsImpl;

// An abstract policy for applying accessibility mode flag changes to a
// WebContentsImpl. Each WebContentsImpl owns an instance of a type derived from
// this and calls through it when there is a change in mode flags. This
// interface allows concrete policies to defer application of mode flags based
// on per-policy factors.
class CONTENT_EXPORT AccessibilityModePolicy {
 public:
  // Returns a new instance for `web_contents` based on runtime configuration.
  static std::unique_ptr<AccessibilityModePolicy> Create(
      WebContentsImpl& web_contents);

  AccessibilityModePolicy(const AccessibilityModePolicy&) = delete;
  AccessibilityModePolicy& operator=(const AccessibilityModePolicy&) = delete;
  virtual ~AccessibilityModePolicy() = default;

  // A callback that can be run to apply (`apply` is true) or clear (`apply` is
  // false) the accessibility mode flags for the policy's WebContents.
  using ApplyOrClearMode = base::RepeatingCallback<void(bool apply)>;

  // Sets the callback to be used to apply or clear a new set of accessibility
  // mode flags.
  virtual void SetAccessibilityMode(ApplyOrClearMode apply_or_clear_mode) = 0;

 protected:
  AccessibilityModePolicy() = default;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_ACCESSIBILITY_MODE_POLICY_H_
