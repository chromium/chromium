// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_ACCESSIBILITY_STATE_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_ACCESSIBILITY_STATE_H_

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "content/common/content_export.h"
#include "ui/accessibility/ax_mode.h"

namespace content {

class BrowserContext;
struct FocusedNodeDetails;
class ScopedAccessibilityMode;
class WebContents;

// The BrowserAccessibilityState class is used to determine if the browser
// should be customized for users with assistive technology, such as screen
// readers.
class CONTENT_EXPORT BrowserAccessibilityState {
 public:
  virtual ~BrowserAccessibilityState() = default;

  // Returns the singleton instance.
  static BrowserAccessibilityState* GetInstance();

  // Enables accessibility for all running tabs.
  virtual void EnableAccessibility() = 0;

  // Disables accessibility for all running tabs. (Only if accessibility is not
  // required by a command line flag or by a platform requirement.)
  virtual void DisableAccessibility() = 0;

  // Returns true if renderer accessibility is not disabled via
  // --disable-renderer-accessibility on the process's command line.
  virtual bool IsRendererAccessibilityEnabled() = 0;

  // Returns the effective accessibility mode for the process. Individual
  // WebContentses may have an effective mode that is a superset of this as a
  // result of any live ScopedAccessibilityMode instances targeting them
  // directly or targeting their BrowserContext.
  virtual ui::AXMode GetAccessibilityMode() = 0;

  // Returns the accessibility mode for `browser_context`, which is the union of
  // all mode flags applied to the process and to `browser_context` itself.
  virtual ui::AXMode GetAccessibilityModeForBrowserContext(
      BrowserContext* browser_context) = 0;

  // The following methods apply `mode` throughout the lifetime of the returned
  // scoper to a) the process, b) a specific BrowserContext, or c) a specific
  // WebContents (colloquially referred to as the "target" of the scoper).
  // Creation and deletion of a scoper will each result in recomputation of the
  // effective accessibility mode for its target. If the effective mode changes,
  // WebContentses associated with the target will be notified.
  virtual std::unique_ptr<ScopedAccessibilityMode> CreateScopedModeForProcess(
      ui::AXMode mode) = 0;
  virtual std::unique_ptr<ScopedAccessibilityMode>
  CreateScopedModeForBrowserContext(BrowserContext* browser_context,
                                    ui::AXMode mode) = 0;
  virtual std::unique_ptr<ScopedAccessibilityMode>
  CreateScopedModeForWebContents(WebContents* web_contents,
                                 ui::AXMode mode) = 0;

  // Note: Prefer the three methods above to add/remove mode flags, as they
  // allow callers to do so without interfering with other controllers of
  // accessibility. The methods below effectively modify a single
  // `ScopedAccessibilityMode` instance targeting the whole process, and put
  // callers at risk of stepping on one another.

  // DEPRECATED. Adds the given accessibility mode flags to the process,
  // impacting all WebContents.
  virtual void AddAccessibilityModeFlags(ui::AXMode mode) = 0;

  // DEPRECATED. Remove the given accessibility mode flags from the current
  // accessibility mode bitmap.
  virtual void RemoveAccessibilityModeFlags(ui::AXMode mode) = 0;

  // DEPRECATED. Resets accessibility to the platform default for all running
  // tabs. This is probably off, but may be on, if
  // --force_renderer_accessibility is passed, or EditableTextOnly if this is
  // Win7.
  virtual void ResetAccessibilityMode() = 0;

  // Called when an accessibility client is detected, using a heuristic.
  // These methods indicate the presence of AXMode::kExtendedProperties, which
  // is a misnomer because it is used by many clients, and not just screen
  // readers. Methods with "KnownScreenReader" or KnownAssistiveTech" in the
  // name deal with actual screen reader or assistive tech usage.
  virtual void OnScreenReaderDetected() = 0;

  // Called when kExtendedProperties mode should be turned off.
  virtual void OnScreenReaderStopped() = 0;

  // Some platforms have a strong signal indicating the presence of a
  // screen reader and can call in to let us know when one has
  // been enabled/disabled. This should be called for screen readers only.
  virtual void SetKnownScreenReaderAppActive(bool is_active) = 0;

  enum AssistiveTech {
    // Use kUnknown if dependent on an expensive computation in
    // UpdateKnownAssistiveTechSlow() that hasn't yet run.
    kNone = 0,
    kUnknown = 1,
    kChromeVox = 2,
    kJaws = 3,
    kNarrator = 4,
    kNvda = 5,
    kOrca = 6,
    kSupernova = 7,
    kTalkback = 8,
    kVoiceOver = 9,
    kZoomText = 10,
    kZdsr = 11,
    kMaxValue = 11
  };

  virtual AssistiveTech ActiveKnownAssistiveTech() = 0;

  // Returns true if there is an ActiveKnownAssistiveTech() matching a
  // screen reader. Note, on some platforms this is slow to compute.
  virtual bool IsKnownScreenReaderActiveSlow() = 0;

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

  // Fire frequent metrics signals to ensure users keeping browser open multiple
  // days are counted each day, not only at launch. This is necessary, because
  // UMA only aggregates uniques on a daily basis,
  virtual void UpdateUniqueUserHistograms() = 0;

  virtual void UpdateHistogramsForTesting() = 0;

  // Update BrowserAccessibilityState with the current status of performance
  // filtering.
  virtual void SetPerformanceFilteringAllowed(bool allowed) = 0;

  // Returns whether performance filtering is allowed.
  virtual bool IsPerformanceFilteringAllowed() = 0;

  // Allows or disallows changes to the AXMode. This is useful for tests that
  // want to ensure that the AXMode is not changed after a certain point.
  virtual void SetAXModeChangeAllowed(bool allow) = 0;
  virtual bool IsAXModeChangeAllowed() const = 0;

  // Notifies web contents that preferences have changed.
  virtual void NotifyWebContentsPreferencesChanged() const = 0;

  using FocusChangedCallback =
      base::RepeatingCallback<void(const FocusedNodeDetails&)>;

  // Registers a callback method that is called whenever the focused element
  // has changed inside a WebContents.
  virtual base::CallbackListSubscription RegisterFocusChangedCallback(
      FocusChangedCallback callback) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_ACCESSIBILITY_STATE_H_
