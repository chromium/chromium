// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_STATE_IMPL_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_STATE_IMPL_H_

#include <vector>

#include "base/time/time.h"
#include "build/build_config.h"
#include "components/metrics/metrics_provider.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_mode_observer.h"

namespace content {

struct FocusedNodeDetails;

// The BrowserAccessibilityState class is used to determine if Chrome should be
// customized for users with assistive technology, such as screen readers. We
// modify the behavior of certain user interfaces to provide a better experience
// for screen reader users. The way we detect a screen reader program is
// different for each platform.
//
// Screen Reader Detection
// (1) On windows many screen reader detection mechinisms will give false
// positives like relying on the SPI_GETSCREENREADER system parameter. In Chrome
// we attempt to dynamically detect a MSAA client screen reader by calling
// NotifiyWinEvent in NativeWidgetWin with a custom ID and wait to see if the ID
// is requested by a subsequent call to WM_GETOBJECT.
// (2) On mac we detect dynamically if VoiceOver is running.  We rely upon the
// undocumented accessibility attribute @"AXEnhancedUserInterface" which is set
// when VoiceOver is launched and unset when VoiceOver is closed.  This is an
// improvement over reading defaults preference values (which has no callback
// mechanism).
class CONTENT_EXPORT BrowserAccessibilityStateImpl
    : public BrowserAccessibilityState,
      public ui::AXModeObserver {
 public:
  BrowserAccessibilityStateImpl();

  BrowserAccessibilityStateImpl(const BrowserAccessibilityStateImpl&) = delete;
  BrowserAccessibilityStateImpl& operator=(
      const BrowserAccessibilityStateImpl&) = delete;

  ~BrowserAccessibilityStateImpl() override;

  static BrowserAccessibilityStateImpl* GetInstance();

  // This needs to be called explicitly by content::BrowserMainLoop during
  // initialization, in order to schedule tasks that need to be done, but
  // don't need to block the main thread.
  //
  // This is called explicitly and not automatically just by
  // instantiating this class so that tests can use
  // BrowserAccessibilityState without worrying about threading.
  virtual void InitBackgroundTasks();

  // BrowserAccessibilityState implementation.
  void EnableAccessibility() override;
  void DisableAccessibility() override;
  bool IsRendererAccessibilityEnabled() override;
  ui::AXMode GetAccessibilityMode() override;
  void AddAccessibilityModeFlags(ui::AXMode mode) override;
  void RemoveAccessibilityModeFlags(ui::AXMode mode) override;
  void ResetAccessibilityMode() override;
  void OnScreenReaderDetected() override;
  void OnScreenReaderStopped() override;
  bool IsAccessibleBrowser() override;
  void AddUIThreadHistogramCallback(base::OnceClosure callback) override;
  void AddOtherThreadHistogramCallback(base::OnceClosure callback) override;
  void UpdateUniqueUserHistograms() override;
  void UpdateHistogramsForTesting() override;
  void SetCaretBrowsingState(bool enabled) override;
#if BUILDFLAG(IS_ANDROID)
  void SetImageLabelsModeForProfile(bool enabled,
                                    BrowserContext* profile) override;
  bool HasSpokenFeedbackServicePresent() override;
#endif
  base::CallbackListSubscription RegisterFocusChangedCallback(
      FocusChangedCallback callback) override;

  // Returns whether caret browsing is enabled for the most recently
  // used profile.
  bool IsCaretBrowsingEnabled() const;

  // AXModeObserver
  void OnAXModeAdded(ui::AXMode mode) override;

  // The global accessibility mode is automatically enabled based on
  // usage of accessibility APIs. When we detect a significant amount
  // of user inputs within a certain time period, but no accessibility
  // API usage, we automatically disable accessibility.
  void OnUserInputEvent();
  void OnAccessibilityApiUsage();

  // Accessibility objects can have the "hot tracked" state set when
  // the mouse is hovering over them, but this makes tests flaky because
  // the test behaves differently when the mouse happens to be over an
  // element.  This is a global switch to not use the "hot tracked" state
  // in a test.
  void set_disable_hot_tracking_for_testing(bool disable_hot_tracking) {
    disable_hot_tracking_ = disable_hot_tracking;
  }
  bool disable_hot_tracking_for_testing() const {
    return disable_hot_tracking_;
  }

  // Calls InitBackgroundTasks with short delays for scheduled tasks,
  // and then calls the given completion callback when done.
  void CallInitBackgroundTasksForTesting(base::RepeatingClosure done_callback);

  // Notifies listeners that the focused element changed inside a WebContents.
  void OnFocusChangedInPage(const FocusedNodeDetails& details);

 protected:
  // Called a short while after startup to allow time for the accessibility
  // state to be determined. Updates histograms with the current state.
  // Two variants - one for things that must be run on the UI thread, and
  // another that can be run on another thread.
  virtual void UpdateHistogramsOnUIThread();
  virtual void UpdateHistogramsOnOtherThread();

 private:
  // Resets accessibility_mode_ to the default value.
  void ResetAccessibilityModeValue();

  // Called by `OnScreenReaderStopped` as a delayed task. If accessibility
  // support has not been re-enabled by the time the delay has expired, we reset
  // `accessibility_mode_` to the default value and notify all web contents.
  void MaybeResetAccessibilityMode();

  void OnOtherThreadDone();

  void UpdateAccessibilityActivityTask();

  ui::AXMode accessibility_mode_;

  base::TimeDelta histogram_delay_;

  std::vector<base::OnceClosure> ui_thread_histogram_callbacks_;
  std::vector<base::OnceClosure> other_thread_histogram_callbacks_;

  bool ui_thread_done_ = false;
  bool other_thread_done_ = false;
  base::RepeatingClosure background_thread_done_callback_;

  // Whether there is a pending task to run UpdateAccessibilityActivityTask.
  bool accessibility_update_task_pending_ = false;

  // Whether the force-renderer-accessibility flag is enabled.
  // Cached here so that we don't have to check base::CommandLine in
  // a function that's called frequently.
  bool force_renderer_accessibility_ = false;

  // Disable hot tracking, i.e. hover state - needed just to avoid flaky tests.
  bool disable_hot_tracking_ = false;

  // Keeps track of whether caret browsing is enabled for the most
  // recently used profile.
  bool caret_browsing_enabled_ = false;

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

  // The time of the most-recent, explicit request to disable accessibility
  // support. This is set in `OnScreenReaderStopped`. We keep track of this
  // in order to prevent destroying and/or (re)creating large accessibility
  // trees in response to an assistive technology being toggled.
  base::TimeTicks disable_accessibility_request_time_;

  base::RepeatingCallbackList<void(const FocusedNodeDetails&)>
      focus_changed_callbacks_;

  base::WeakPtrFactory<BrowserAccessibilityStateImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_STATE_IMPL_H_
