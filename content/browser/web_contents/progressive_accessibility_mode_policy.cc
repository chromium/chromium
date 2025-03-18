// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/progressive_accessibility_mode_policy.h"

#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/visibility.h"
#include "ui/accessibility/platform/assistive_tech.h"

namespace content {

namespace {

// Returns true if the current known assistive tech interacting with the browser
// is a screen reader.
bool ScreenReaderInUse() {
  auto assistive_tech =
      BrowserAccessibilityState::GetInstance()->ActiveAssistiveTech();
  if (assistive_tech == ui::AssistiveTech::kUnknown) {
    // On some operating systems, we don't know if a screen reader is running
    // until some expensive operations are performed off-thread.
    // Report a false-positive in this case (assume there is a screen reader),
    // as this is less disruptive for the user than a false-negative
    // if it turns out to be incorrect, since disable-on-hide can
    // begin operating once a definitive signal is available.
    return true;
  }

  // For the most part, screen readers either don't interact well with
  // ProgressiveAccessibility or need more testing before we allow it to be
  // used with them. We need to make sure the user doesn't lose their place
  // when the accessibility tree is rebuilt, and that the screen reader doesn't
  // hold onto objects from dropped trees, causing leaks.
  return ui::IsScreenReader(assistive_tech);
}

}  // namespace

ProgressiveAccessibilityModePolicy::ProgressiveAccessibilityModePolicy(
    WebContentsImpl& web_contents,
    bool disable_on_hide)
    : WebContentsObserver(&web_contents), disable_on_hide_(disable_on_hide) {}

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
  if (web_contents_impl().IsBeingDestroyed()) {
    // Do nothing if the WebContents is being destroyed.
    return;
  }

  if (visibility == Visibility::HIDDEN &&
      (!disable_on_hide_ || ScreenReaderInUse())) {
    // Do nothing if the WebContents has been hidden and the policy is not
    // configured to disable accessibility upon hide or if a known screen reader
    // is in use.
    return;
  }

  // TODO(https://crbug.com/336843455): Propagate mode changes to inner
  // WebContentses if the kUpdateInnerWebContentsVisibility feature is disabled
  // or reverted.

  // Apply the latest changes if the WebContents has become un-hidden, or clear
  // the mode flags if is being hidden (and disable_on_hide_ is set).
  apply_or_clear_mode_.Run(/*apply=*/visibility != Visibility::HIDDEN);
}

// TODO(https://crbug.com/336843455): Observe InnerWebContentsAttached and
// tell the inner_web_contents' accessibility mode policy to apply its mode if
// this WC is visible but the attached is not. It would be more correct for the
// newly attached inner WC's visibility state to be updated in
// AttachInnerWebContents().

}  // namespace content
