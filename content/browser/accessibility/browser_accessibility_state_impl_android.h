// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_STATE_IMPL_ANDROID_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_STATE_IMPL_ANDROID_H_

#include "base/scoped_observation.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "ui/accessibility/android/accessibility_state.h"

namespace content {

class BrowserAccessibilityStateImplAndroid
    : public BrowserAccessibilityStateImpl,
      public ui::AccessibilityState::AccessibilityStateObserver {
 public:
  BrowserAccessibilityStateImplAndroid();
  ~BrowserAccessibilityStateImplAndroid() override;

  // ui::AccessibilityState::AccessibilityStateObserver:
  void OnAnimatorDurationScaleChanged() override;
  void RecordAccessibilityServiceInfoHistograms() override;

  // BrowserAccessibilityStateImpl implementation.
  void RefreshAssistiveTech() override;

 protected:
  void RecordAccessibilityServiceStatsHistogram(int event_type_mask,
                                                int feedback_type_mask,
                                                int flags_mask,
                                                int capabilities_mask,
                                                std::string histogram);

 private:
  base::ScopedObservation<ui::AccessibilityState,
                          ui::AccessibilityState::AccessibilityStateObserver>
      accessibility_state_observation_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_STATE_IMPL_ANDROID_H_
