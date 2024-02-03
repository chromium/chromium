// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_STATE_IMPL_LACROS_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_STATE_IMPL_LACROS_H_

#include <memory>

#include "base/values.h"
#include "chromeos/lacros/crosapi_pref_observer.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"

namespace content {

class ScopedAccessibilityMode;

class BrowserAccessibilityStateImplLacros
    : public BrowserAccessibilityStateImpl {
 public:
  BrowserAccessibilityStateImplLacros();
  ~BrowserAccessibilityStateImplLacros() override;

  BrowserAccessibilityStateImplLacros(
      const BrowserAccessibilityStateImplLacros&) = delete;
  BrowserAccessibilityStateImplLacros& operator=(
      const BrowserAccessibilityStateImplLacros&) = delete;

 private:
  void OnSpokenFeedbackPrefChanged(base::Value value);

  CrosapiPrefObserver crosapi_pref_observer_;

  // Applies AXMode::kScreenReader to all WebContents in the process while
  // spoken feedback is enabled.
  std::unique_ptr<ScopedAccessibilityMode> screen_reader_mode_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_STATE_IMPL_LACROS_H_
