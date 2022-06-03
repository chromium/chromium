// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_STATE_IMPL_ANDROID_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_STATE_IMPL_ANDROID_H_

#include "content/browser/accessibility/browser_accessibility_state_impl.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"

namespace content {

class BrowserContext;

class BrowserAccessibilityStateImplAndroid
    : public BrowserAccessibilityStateImpl {
 public:
  BrowserAccessibilityStateImplAndroid();
  ~BrowserAccessibilityStateImplAndroid() override {}

  void CollectAccessibilityServiceStats();

 protected:
  void UpdateHistogramsOnOtherThread() override;
  void UpdateUniqueUserHistograms() override;
  void SetImageLabelsModeForProfile(bool enabled,
                                    BrowserContext* profile) override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_STATE_IMPL_ANDROID_H_
