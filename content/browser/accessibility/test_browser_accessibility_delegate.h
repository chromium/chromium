// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_TEST_BROWSER_ACCESSIBILITY_DELEGATE_H_
#define CONTENT_BROWSER_ACCESSIBILITY_TEST_BROWSER_ACCESSIBILITY_DELEGATE_H_

#include "content/browser/accessibility/browser_accessibility_manager.h"

namespace content {

class TestBrowserAccessibilityDelegate : public BrowserAccessibilityDelegate {
 public:
  TestBrowserAccessibilityDelegate();

  void AccessibilityPerformAction(const ui::AXActionData& data) override;
  bool AccessibilityViewHasFocus() const override;
  void AccessibilityViewSetFocus() override;
  gfx::Rect AccessibilityGetViewBounds() const override;
  float AccessibilityGetDeviceScaleFactor() const override;
  void AccessibilityFatalError() override;
  gfx::AcceleratedWidget AccessibilityGetAcceleratedWidget() override;
  gfx::NativeViewAccessible AccessibilityGetNativeViewAccessible() override;
  gfx::NativeViewAccessible AccessibilityGetNativeViewAccessibleForWindow()
      override;
  WebContents* AccessibilityWebContents() override;
  bool AccessibilityIsMainFrame() const override;

  bool got_fatal_error() const;
  void reset_got_fatal_error();

  bool is_root_frame_;
  gfx::AcceleratedWidget accelerated_widget_;

 private:
  bool got_fatal_error_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_TEST_BROWSER_ACCESSIBILITY_DELEGATE_H_
