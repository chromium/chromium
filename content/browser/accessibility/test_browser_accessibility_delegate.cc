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

bool TestBrowserAccessibilityDelegate::AccessibilityViewHasFocus() const {
  return false;
}

void TestBrowserAccessibilityDelegate::AccessibilityViewSetFocus() {}

gfx::Rect TestBrowserAccessibilityDelegate::AccessibilityGetViewBounds() const {
  return gfx::Rect();
}

float TestBrowserAccessibilityDelegate::AccessibilityGetDeviceScaleFactor()
    const {
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

bool TestBrowserAccessibilityDelegate::AccessibilityIsMainFrame() const {
  return is_root_frame_;
}

bool TestBrowserAccessibilityDelegate::got_fatal_error() const {
  return got_fatal_error_;
}

void TestBrowserAccessibilityDelegate::reset_got_fatal_error() {
  got_fatal_error_ = false;
}

}  // namespace content
