// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_state_impl_lacros.h"

#include <memory>

#include "content/public/browser/scoped_accessibility_mode.h"

namespace content {

BrowserAccessibilityStateImplLacros::BrowserAccessibilityStateImplLacros()
    : crosapi_pref_observer_(
          crosapi::mojom::PrefPath::kAccessibilitySpokenFeedbackEnabled,
          base::BindRepeating(
              &BrowserAccessibilityStateImplLacros::OnSpokenFeedbackPrefChanged,
              base::Unretained(this))) {}

BrowserAccessibilityStateImplLacros::~BrowserAccessibilityStateImplLacros() =
    default;

void BrowserAccessibilityStateImplLacros::OnSpokenFeedbackPrefChanged(
    base::Value value) {
  if (!value.GetIfBool().value_or(false)) {
    screen_reader_mode_.reset();
  } else if (!screen_reader_mode_) {
    screen_reader_mode_ = CreateScopedModeForProcess(ui::AXMode::kScreenReader);
  }
}

// static
std::unique_ptr<BrowserAccessibilityStateImpl>
BrowserAccessibilityStateImpl::Create() {
  return std::make_unique<BrowserAccessibilityStateImplLacros>();
}

}  // namespace content
