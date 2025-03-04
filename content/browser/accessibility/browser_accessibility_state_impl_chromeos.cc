// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_state_impl.h"

namespace content {

class BrowserAccessibilityStateImplChromeOS
    : public BrowserAccessibilityStateImpl {
 public:
  BrowserAccessibilityStateImplChromeOS() = default;

 protected:
  // BrowserAccessibilityStateImpl:
  void SetKnownScreenReaderAppActive(bool is_active) override {
    is_chromevox_active_ = is_active;

    // Set/clear crash key (similar to crash keys for other screen readers).
    static auto* ax_chromevox_crash_key = base::debug::AllocateCrashKeyString(
        "ax_chromevox", base::debug::CrashKeySize::Size32);
    if (is_active) {
      base::debug::SetCrashKeyString(ax_chromevox_crash_key, "true");
    } else {
      base::debug::ClearCrashKeyString(ax_chromevox_crash_key);
    }
  }

  BrowserAccessibilityState::AssistiveTech ActiveKnownAssistiveTech() override {
    return is_chromevox_active_ ? kChromeVox : kNone;
  }

 private:
  bool is_chromevox_active_ = false;
};

// static
std::unique_ptr<BrowserAccessibilityStateImpl>
BrowserAccessibilityStateImpl::Create() {
  return std::make_unique<BrowserAccessibilityStateImplChromeOS>();
}

}  // namespace content
