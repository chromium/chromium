// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_ACCESSIBILITY_STATE_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_ACCESSIBILITY_STATE_H_

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "content/common/content_export.h"
#include "ui/accessibility/ax_mode.h"

namespace ui {
enum class AssistiveTech;
}

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
  // WebContentses associated with the target will be notified. Calls that are
  // made in response to signals from the platform accessibility integration
  // (e.g., enabling accessibility when VoiceOver is detected) must include the
  // `AXMode::kFromPlatform` flag in addition to other flags.
  virtual std::unique_ptr<ScopedAccessibilityMode> CreateScopedModeForProcess(
      ui::AXMode mode) = 0;
  virtual std::unique_ptr<ScopedAccessibilityMode>
  CreateScopedModeForBrowserContext(BrowserContext* browser_context,
                                    ui::AXMode mode) = 0;
  virtual std::unique_ptr<ScopedAccessibilityMode>
  CreateScopedModeForWebContents(WebContents* web_contents,
                                 ui::AXMode mode) = 0;

  // Return the last active assistive technology. If multiple ATs are
  // running concurrently (rare case), the result will prefer a screen reader.
  // This will use the last known value, so it is possible for it to be out of
  // date for a short period of time. Use
  // AXModeObserver::OnAssistiveTechChanged() to get notifications for changes
  // to this state.
  virtual ui::AssistiveTech ActiveAssistiveTech() const = 0;

  // Update BrowserAccessibilityState with the current status of performance
  // filtering.
  virtual void SetPerformanceFilteringAllowed(bool allowed) = 0;

  // Returns whether performance filtering is allowed.
  virtual bool IsPerformanceFilteringAllowed() = 0;

  // Allows or disallows changes to the AXMode. This is useful for tests that
  // want to ensure that the AXMode is not changed after a certain point.
  virtual void SetAXModeChangeAllowed(bool allow) = 0;
  virtual bool IsAXModeChangeAllowed() const = 0;

  // Enables or disables activation of accessibility from interactions with the
  // platform's accessibility integration. Such activations are disabled by
  // default in tests; specifically, mode changes via calls to
  // `CreateScopedModeForProcess()` are ignored when the `AXMode` contains the
  // `AXMode::kFromPlatform` flag.
  virtual void SetActivationFromPlatformEnabled(bool enabled) = 0;
  virtual bool IsActivationFromPlatformEnabled() = 0;

  // Returns true if the current AXMode was set as part of the accessibility
  // performance measurement experiment.
  virtual bool IsAccessibilityPerformanceMeasurementExperimentActive()
      const = 0;

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
