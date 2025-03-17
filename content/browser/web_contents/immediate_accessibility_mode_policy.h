// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_IMMEDIATE_ACCESSIBILITY_MODE_POLICY_H_
#define CONTENT_BROWSER_WEB_CONTENTS_IMMEDIATE_ACCESSIBILITY_MODE_POLICY_H_

#include "base/memory/raw_ref.h"
#include "content/browser/web_contents/accessibility_mode_policy.h"
#include "content/common/content_export.h"

namespace content {

class WebContentsImpl;

// A policy that applies mode flags immediately and never clears them.
class CONTENT_EXPORT ImmediateAccessibilityModePolicy
    : public AccessibilityModePolicy {
 public:
  explicit ImmediateAccessibilityModePolicy(WebContentsImpl& web_contents)
      : web_contents_(web_contents) {}

  // AccessibilityModePolicy:
  void SetAccessibilityMode(ApplyOrClearMode apply_or_clear_mode) override;

 private:
  const raw_ref<WebContentsImpl> web_contents_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_IMMEDIATE_ACCESSIBILITY_MODE_POLICY_H_
