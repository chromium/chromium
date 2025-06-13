// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_STATE_IMPL_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_STATE_IMPL_H_

#include <list>
#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "components/metrics/metrics_provider.h"
#include "content/browser/accessibility/scoped_mode_collection.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/render_widget_host.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/platform/assistive_tech.h"
#include "ui/accessibility/platform/ax_platform.h"

namespace content {

struct FocusedNodeDetails;
class WebContentsImpl;

// The BrowserAccessibilityState class is used to determine if Chrome should be
// customized for users with assistive technology, such as screen readers. We
// modify the behavior of certain user interfaces to provide a better experience
// for screen reader users. The way we detect a screen reader program is
// different for each platform.
//
// Screen Reader Detection
// (1) On Windows, many screen reader detection mechanisms will give false
//     positives, such as relying on the SPI_GETSCREENREADER system parameter.
//     In Chrome, we attempt to dynamically detect a MSAA client screen reader
//     by calling NotifyWinEvent in NativeWidgetWin with a custom ID and wait
//     to see if the ID is requested by a subsequent call to WM_GETOBJECT.
// (2) On macOS, we dynamically detect if VoiceOver is running by Key-Value
//     Observing changes to the "voiceOverEnabled" property in NSWorkspace. We
//     also monitor the undocumented accessibility attribute
//     @"AXEnhancedUserInterface", which is set by other assistive
//     technologies.
class CONTENT_EXPORT BrowserAccessibilityStateImpl
    : public BrowserAccessibilityState,
      public ui::AXPlatform::Delegate,
      public content::RenderWidgetHost::InputEventObserver,
      public ScopedModeCollection::Delegate {
 public:
  BrowserAccessibilityStateImpl(const BrowserAccessibilityStateImpl&) = delete;
  BrowserAccessibilityStateImpl& operator=(
      const BrowserAccessibilityStateImpl&) = delete;

  ~BrowserAccessibilityStateImpl() override;

  // Returns the single process-wide instance.
  static BrowserAccessibilityStateImpl* GetInstance();

  // Returns a new instance. Only one instance may be live in the process at any
  // time.
  static std::unique_ptr<BrowserAccessibilityStateImpl> Create();

  // BrowserAccessibilityState implementation.
  ui::AXMode GetAccessibilityMode() override;
  ui::AXMode GetAccessibilityModeForBrowserContext(
      BrowserContext* browser_context) override;
  // Any currently running assistive tech that should prevent accessibility from
  // being auto-disabled.
  ui::AssistiveTech ActiveAssistiveTech() const override;
  void SetPerformanceFilteringAllowed(bool enabled) override;
  bool IsPerformanceFilteringAllowed() override;
  base::CallbackListSubscription RegisterFocusChangedCallback(
      FocusChangedCallback callback) override;
  std::unique_ptr<ScopedAccessibilityMode> CreateScopedModeForProcess(
      ui::AXMode mode) override;
  std::unique_ptr<ScopedAccessibilityMode> CreateScopedModeForBrowserContext(
      BrowserContext* browser_context,
      ui::AXMode mode) override;
  std::unique_ptr<ScopedAccessibilityMode> CreateScopedModeForWebContents(
      WebContents* web_contents,
      ui::AXMode mode) override;
  void SetAXModeChangeAllowed(bool allowed) override;
  bool IsAXModeChangeAllowed() const override;
  void SetActivationFromPlatformEnabled(bool enabled) override;
  bool IsActivationFromPlatformEnabled() override;
  bool IsAccessibilityPerformanceMeasurementExperimentActive() const override;
  void NotifyWebContentsPreferencesChanged() const override;

  // ui::AXPlatform::Delegate:
  void OnMinimalPropertiesUsed() override;
  void OnPropertiesUsedInBrowserUI() override;
  void OnPropertiesUsedInWebContent() override;
  void OnInlineTextBoxesUsedInWebContent() override;
  void OnExtendedPropertiesUsedInWebContent() override;
  void OnHTMLAttributesUsed() override;
  void OnActionFromAssistiveTech() override;

  // content::RenderWidgetHost::InputEventObserver:
  void OnInputEvent(const RenderWidgetHost& widget,
                    const blink::WebInputEvent& event) override;

  // The global accessibility mode is automatically enabled based on
  // usage of accessibility APIs. When we detect a significant amount
  // of user inputs within a certain time period, but no accessibility
  // API usage, we automatically disable accessibility.
  void OnUserInputEvent();

  // Notifies listeners that the focused element changed inside a WebContents.
  void OnFocusChangedInPage(const FocusedNodeDetails& details);

  // Return true if auto-disable should be blocked.
  bool ShouldBlockAutoDisable();

  // Signal to BrowserAccessibilityState that a page navigation has occurred.
  void OnPageNavigationComplete();

  // Sets the initial accessibility mode for `web_contents` if it is not
  // hidden or if ProgressiveAccessibility is not enabled.
  void OnWebContentsInitialized(WebContentsImpl* web_contents);

  // Applies the effective accessibility mode for `web_contents` if
  // ProgressiveAccessibility is enabled.
  void OnWebContentsRevealed(WebContentsImpl* web_contents);

  // Tracks `web_contents` for the sake of disabling accessibility later if
  // ProgressiveAccessibility is enabled and disable_on_hide is selected. A
  // previously-hidden WebContents becomes eligible for disablement if it is
  // not among the last five to be hidden once it has been hidden for at least
  // five minutes.
  void OnWebContentsHidden(WebContentsImpl* web_contents);

  // Notifies the instance that `assistive_tech` is the most significant of any
  // assistive technologies discovered. AXPlatform observers are notified if
  // `assistive_tech` differs from the most recent discovery. Called by
  // subclasses or accessibility managers when they detect the presence of
  // assistive tech via platform-specific means.
  void OnAssistiveTechFound(ui::AssistiveTech assistive_tech);

  void SetDiscoverAssistiveTechnologyCallbackForTesting(
      base::RepeatingClosure callback) {
    discover_at_callback_for_testing_ = std::move(callback);
  }

  // A hidden WebContents is guaranteed to retain its accessibility state when
  // the ProgressiveAccessibility feature is in disable_on_hide mode for at
  // least five minutes, plus or minus twenty seconds.
  static base::TimeDelta GetRandomizedDisableDelay();
  static base::TimeDelta GetMaxDisableDelay();

  // The number of recently-hidden WebContents that will not have accessibility
  // disabled if the ProgressiveAccessibility feature is on in disable_on_hide
  // mode.
  static constexpr int kMaxPreservedWebContents = 5;

 protected:
  BrowserAccessibilityStateImpl();

  // Refreshes the assistive tech if an AXMode change indicates that the
  // presence of an active screen reader may have changed.
  // * Platforms that have a perfect signal for the presence of a screen reader
  // should not override this method: the default implementation treats the
  // screen reader flag as a deterministic indicator.
  // * Platforms such as Windows and Linux that require a slow computation
  // to determine the presence of a screen reader should begin the computation
  // when the presence of AXMode::kExtendedProperties is inconsistent with the
  // current known screen reader state.
  virtual void RefreshAssistiveTechIfNecessary(ui::AXMode new_mode);

  ui::AXPlatform& ax_platform() { return ax_platform_; }

 private:
  void UpdateAccessibilityActivityTask();

  // Stops tracking `web_contents` for disabling accessibility while it is
  // hidden.
  void OnDisablerDestroyedForWebContents(WebContentsImpl* web_contents);

  // Combines the effective accessibility mode for the process, for
  // `web_contents`'s BrowserContext, and for `web_contents` and applies it
  // to `web_contents` if ProgressiveAccessibility is disabled or if
  // `web_contents` is not hidden.
  void ApplyAccessibilityModeToWebContents(WebContentsImpl* web_contents,
                                           ui::AXMode process_mode,
                                           ui::AXMode browser_context_mode,
                                           ui::AXMode web_contents_mode);

  // ScopedModeCollection::Delegate:
  // Handles a change to the effective accessibility mode for the process.
  void OnModeChanged(ui::AXMode old_mode, ui::AXMode new_mode) override;

  // Filters out `kFromPlatform` from `mode` if activation from platform
  // integration is enabled; otherwise, filters all mode flags from `mode` if
  // `kFromPlatform` is present in it.
  ui::AXMode FilterModeFlags(ui::AXMode mode) override;

  // Handles a change to the effective accessibility mode for `browser_context`.
  void OnModeChangedForBrowserContext(BrowserContext* browser_context,
                                      ui::AXMode old_mode,
                                      ui::AXMode new_mode);

  // Handles a change to the effective accessibility mode for `web_contents`.
  void OnModeChangedForWebContents(WebContents* web_contents,
                                   ui::AXMode old_mode,
                                   ui::AXMode new_mode);

  // Add the AXModes + AXMode::kFromPlatform, when corresponding platform APIs
  // are used.
  void EnableAXModeFromPlatform(ui::AXMode modes_to_add);

  // Refreshes the instance's notion of active assistive technologies.
  // Implementations must call `OnAssistiveTechFound()` with the results of any
  // discovery.
  virtual void RefreshAssistiveTech();

  // Helper function to configure the accessibility performance experiment.
  std::unique_ptr<ScopedAccessibilityMode>
  ConfigureAccessibilityPerformanceExperiment();

  // Helper to disable and clean-up the accessibility performance experiment.
  void ExitPerformanceExperiment();

  // The process's single AXPlatform instance.
  ui::AXPlatform ax_platform_{*this};

  // Whether there is a pending task to run UpdateAccessibilityActivityTask.
  bool accessibility_update_task_pending_ = false;

  // Whether changes to the AXMode are allowed.
  // Changes are disallowed while running tests or when
  // --force-renderer-accessibility is used on the command line.
  bool allow_ax_mode_changes_ = true;

  // Keeps track of whether performance filtering is allowed for the device.
  // Default is true to defer to feature flag. Value may be set to false by
  // prefs.
  bool performance_filtering_allowed_ = true;

  // Tracks whether the accessibility engine has been used in any form during
  // the current session. Toggled to true when accessibility is first enabled,
  // and never toggled back to false.
  bool has_enabled_accessibility_in_session_ = false;

  // True if activation of accessibility from interactions with the platform's
  // accessibility integration is enabled.
  bool activation_from_platform_enabled_ = true;

  // Timer used to track the time between start-up and engine first-use.
  base::ElapsedTimer first_use_timer_;

  // Counter used to track the number of page navigations between start-up
  // and engine first-use.
  uint32_t num_page_navs_before_first_use_ = 0;

  // The time of the first user input event; if we receive multiple
  // user input events within a 30-second period and no
  base::TimeTicks first_user_input_event_time_;
  int user_input_event_count_ = 0;

  // The time accessibility became active, used to calculate active time.
  base::TimeTicks accessibility_active_start_time_;

  // The time accessibility became inactive, used to calculate inactive time.
  base::TimeTicks accessibility_inactive_start_time_;

  // The last time accessibility was active, used to calculate active time.
  base::TimeTicks accessibility_last_usage_time_;

  // The time accessibility was enabled, for statistics.
  base::TimeTicks accessibility_enabled_time_;

  // The time accessibility was auto-disabled, for statistics.
  base::TimeTicks accessibility_disabled_time_;

  base::RepeatingCallbackList<void(const FocusedNodeDetails&)>
      focus_changed_callbacks_;

  // The collection of active ScopedAccessibilityMode instances targeting all
  // WebContentses in the process.
  ScopedModeCollection scoped_modes_for_process_{*this};

  // A ScopedAccessibilityMode that holds the process-wide mode flags modified
  // via --force-renderer-accessibility on the command line.
  std::unique_ptr<ScopedAccessibilityMode> forced_accessibility_mode_;

  // A ScopedAccessibilityMode that holds process-wide mode flags required to
  // support the platform API calls being used.
  std::unique_ptr<ScopedAccessibilityMode> platform_ax_mode_;

  // Keeps track of whether the Accessibility Performance Measurement Experiment
  // is currently active. This is necessary because there are cases where we
  // don't want to make the experiment active, and checking the state of the
  // feature flag causes the study to be active. In this case, if the conditions
  // are met, this will contain the mode of the current experiment group,
  // nullptr otherwise.
  std::unique_ptr<ScopedAccessibilityMode> experiment_accessibility_mode_;

  // The most recently hidden WebContentses; used only when the disable-on-hide
  // feature of ProgressiveAccessibility is enabled. This container holds the
  // most-recently hidden WebContentses. Accessibility is disabled for each one
  // that is pushed out of this list when a sixth element is added.
  std::list<raw_ptr<WebContentsImpl>> last_hidden_;

  // Keeps track of whether screen reader presence was checked. Resets with
  // every new page load. A new check only occurs if kAXModeComplete is active
  // and a screen reader isn't running.
  bool has_recently_checked_for_screen_reader_ = false;

  base::RepeatingClosure discover_at_callback_for_testing_;
  friend class ui::AXPlatform;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_STATE_IMPL_H_
