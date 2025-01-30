// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/progressive_accessibility_mode_policy.h"

#include "content/public/browser/visibility.h"

namespace content {

ProgressiveAccessibilityModePolicy::ProgressiveAccessibilityModePolicy(
    WebContentsImpl& web_contents)
    : WebContentsObserver(&web_contents) {}

ProgressiveAccessibilityModePolicy::~ProgressiveAccessibilityModePolicy() =
    default;

void ProgressiveAccessibilityModePolicy::SetAccessibilityMode(
    ApplyOrClearMode apply_or_clear_mode) {
  apply_or_clear_mode_ = std::move(apply_or_clear_mode);

  // TODO(https://crbug.com/336843455): Walk up the chain of outer WebContents
  // to check for visibility if the kUpdateInnerWebContentsVisibility feature is
  // disabled or reverted.
  if (web_contents_impl().GetVisibility() != Visibility::HIDDEN) {
    apply_or_clear_mode_.Run(/*apply=*/true);
  }
}

void ProgressiveAccessibilityModePolicy::OnVisibilityChanged(
    Visibility visibility) {
  // TODO(https://crbug.com/336843455): Propagate mode changes to inner
  // WebContentses if the kUpdateInnerWebContentsVisibility feature is disabled
  // or reverted.
  if (!web_contents_impl().IsBeingDestroyed()) {
    apply_or_clear_mode_.Run(/*apply=*/visibility != Visibility::HIDDEN);
  }
}

// TODO(https://crbug.com/336843455): Observe InnerWebContentsAttached and
// tell the inner_web_contents' accessibility mode policy to apply its mode if
// this WC is visible but the attached is not. It would be more correct for the
// newly attached inner WC's visibility state to be updated in
// AttachInnerWebContents().

}  // namespace content
