// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_state_impl.h"

#import <Cocoa/Cocoa.h>

#include "base/metrics/histogram_macros.h"

@interface NSWorkspace (Partials)

@property(readonly) BOOL accessibilityDisplayShouldDifferentiateWithoutColor;
@property(readonly) BOOL accessibilityDisplayShouldIncreaseContrast;
@property(readonly) BOOL accessibilityDisplayShouldReduceTransparency;

@end

namespace content {

void BrowserAccessibilityStateImpl::PlatformInitialize() {}

void BrowserAccessibilityStateImpl::UpdatePlatformSpecificHistograms() {
  // NOTE: This function is running on the file thread.
  NSWorkspace* workspace = [NSWorkspace sharedWorkspace];

  SEL sel = @selector(accessibilityDisplayShouldIncreaseContrast);
  if (![workspace respondsToSelector:sel])
    return;

  UMA_HISTOGRAM_BOOLEAN(
      "Accessibility.Mac.DifferentiateWithoutColor",
      workspace.accessibilityDisplayShouldDifferentiateWithoutColor);
  UMA_HISTOGRAM_BOOLEAN(
      "Accessibility.Mac.IncreaseContrast",
      workspace.accessibilityDisplayShouldIncreaseContrast);
  UMA_HISTOGRAM_BOOLEAN(
      "Accessibility.Mac.ReduceTransparency",
      workspace.accessibilityDisplayShouldReduceTransparency);

  UMA_HISTOGRAM_BOOLEAN(
      "Accessibility.Mac.FullKeyboardAccessEnabled",
      static_cast<NSApplication*>(NSApp).fullKeyboardAccessEnabled);
}

}  // namespace content
