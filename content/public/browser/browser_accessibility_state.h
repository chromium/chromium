// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_ACCESSIBILITY_STATE_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_ACCESSIBILITY_STATE_H_

#include "base/callback_forward.h"

#include "content/common/content_export.h"
#include "ui/accessibility/ax_mode.h"

namespace content {

// The BrowserAccessibilityState class is used to determine if the browser
// should be customized for users with assistive technology, such as screen
// readers.
class CONTENT_EXPORT BrowserAccessibilityState {
 public:
  virtual ~BrowserAccessibilityState() { }

  // Returns the singleton instance.
  static BrowserAccessibilityState* GetInstance();

  // Enables accessibility for all running tabs.
  virtual void EnableAccessibility() = 0;

  // Disables accessibility for all running tabs. (Only if accessibility is not
  // required by a command line flag or by a platform requirement.)
  virtual void DisableAccessibility() = 0;

  virtual bool IsRendererAccessibilityEnabled() = 0;

  virtual ui::AXMode GetAccessibilityMode() = 0;

  // Adds the given accessibility mode flags to the current accessibility
  // mode bitmap.
  virtual void AddAccessibilityModeFlags(ui::AXMode mode) = 0;

  // Remove the given accessibility mode flags from the current accessibility
  // mode bitmap.
  virtual void RemoveAccessibilityModeFlags(ui::AXMode mode) = 0;

  // Resets accessibility to the platform default for all running tabs.
  // This is probably off, but may be on, if --force_renderer_accessibility is
  // passed, or EditableTextOnly if this is Win7.
  virtual void ResetAccessibilityMode() = 0;

  // Called when screen reader client is detected.
  virtual void OnScreenReaderDetected() = 0;

  // Returns true if the browser should be customized for accessibility.
  virtual bool IsAccessibleBrowser() = 0;

  // Add a callback method that will be called once, a small while after the
  // browser starts up, when accessibility state histograms are updated.
  // Use this to register a method to update additional accessibility
  // histograms.
  //
  // Use this variant for a callback that must be run on the UI thread,
  // for example something that needs to access prefs.
  virtual void AddUIThreadHistogramCallback(base::OnceClosure callback) = 0;

  // Use this variant for a callback that's better to run on another
  // thread, for example something that may block or run slowly.
  virtual void AddOtherThreadHistogramCallback(base::OnceClosure callback) = 0;

  virtual void UpdateHistogramsForTesting() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_ACCESSIBILITY_STATE_H_
