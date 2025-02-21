// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_PROGRESSIVE_ACCESSIBILITY_MODE_POLICY_H_
#define CONTENT_BROWSER_WEB_CONTENTS_PROGRESSIVE_ACCESSIBILITY_MODE_POLICY_H_

#include "base/functional/callback.h"
#include "content/browser/web_contents/accessibility_mode_policy.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/accessibility/ax_mode.h"

namespace content {

// A policy that applies mode flags immediately if the WebContents is not
// hidden. Otherwise, the mode flags are applied the next time the WebContents
// becomes un-hidden (visible or occluded). If `disable_on_hide` is true, the
// mode flags are cleared when then WebContents is hidden.
class CONTENT_EXPORT ProgressiveAccessibilityModePolicy
    : public AccessibilityModePolicy,
      public WebContentsObserver {
 public:
  explicit ProgressiveAccessibilityModePolicy(WebContentsImpl& web_contents,
                                              bool disable_on_hide);
  ~ProgressiveAccessibilityModePolicy() override;

  // AccessibilityModePolicy:
  void SetAccessibilityMode(ApplyOrClearMode apply_or_clear_mode) override;

  // WebContentsObserver:
  void OnVisibilityChanged(Visibility visibility) override;

 private:
  // Returns the WebContents to which this policy applies.
  WebContentsImpl& web_contents_impl() {
    return reinterpret_cast<WebContentsImpl&>(*web_contents());
  }

  // When true, mode flags are cleared when then WebContents is hidden.
  const bool disable_on_hide_;

  ApplyOrClearMode apply_or_clear_mode_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_PROGRESSIVE_ACCESSIBILITY_MODE_POLICY_H_
