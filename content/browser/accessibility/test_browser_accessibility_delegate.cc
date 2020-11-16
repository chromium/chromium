// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/test_browser_accessibility_delegate.h"

namespace content {

TestBrowserAccessibilityDelegate::TestBrowserAccessibilityDelegate()
    : is_root_frame_(true),
      accelerated_widget_(gfx::kNullAcceleratedWidget),
      got_fatal_error_(false) {}

void TestBrowserAccessibilityDelegate::AccessibilityPerformAction(
    const ui::AXActionData& data) {}

bool TestBrowserAccessibilityDelegate::AccessibilityViewHasFocus() {
  return false;
}

void TestBrowserAccessibilityDelegate::AccessibilityViewSetFocus() {}

gfx::Rect TestBrowserAccessibilityDelegate::AccessibilityGetViewBounds() {
  return gfx::Rect();
}

float TestBrowserAccessibilityDelegate::AccessibilityGetDeviceScaleFactor() {
  return 1.0f;
}

void TestBrowserAccessibilityDelegate::AccessibilityFatalError() {
  got_fatal_error_ = true;
}

gfx::AcceleratedWidget
TestBrowserAccessibilityDelegate::AccessibilityGetAcceleratedWidget() {
  return accelerated_widget_;
}

gfx::NativeViewAccessible
TestBrowserAccessibilityDelegate::AccessibilityGetNativeViewAccessible() {
  return nullptr;
}

gfx::NativeViewAccessible TestBrowserAccessibilityDelegate::
    AccessibilityGetNativeViewAccessibleForWindow() {
  return nullptr;
}

WebContents* TestBrowserAccessibilityDelegate::AccessibilityWebContents() {
  return nullptr;
}

bool TestBrowserAccessibilityDelegate::AccessibilityIsMainFrame() {
  return is_root_frame_;
}

void TestBrowserAccessibilityDelegate::AccessibilityHitTest(
    const gfx::Point& point_in_frame_pixels,
    ax::mojom::Event opt_event_to_fire,
    int opt_request_id,
    base::OnceCallback<void(BrowserAccessibilityManager* hit_manager,
                            int hit_node_id)> opt_callback) {}

bool TestBrowserAccessibilityDelegate::got_fatal_error() const {
  return got_fatal_error_;
}

void TestBrowserAccessibilityDelegate::reset_got_fatal_error() {
  got_fatal_error_ = false;
}

}  // namespace content
