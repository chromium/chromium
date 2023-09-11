// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_state_impl.h"

#include <stddef.h>

#include "base/check.h"
#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents_ax_mode_notifier.h"
#include "content/public/common/content_switches.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_mode_histogram_logger.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/color_utils.h"

namespace content {

namespace {

BrowserAccessibilityStateImpl* g_instance = nullptr;

// Auto-disable accessibility if this many seconds elapse with user input
// events but no accessibility API usage.
constexpr int kAutoDisableAccessibilityTimeSecs = 30;

// Minimum number of user input events with no accessibility API usage
// before auto-disabling accessibility.
constexpr int kAutoDisableAccessibilityEventCount = 3;

// Updating Active/Inactive time on every accessibility api calls would not be
// good for perf. Instead, delay the update task.
constexpr int kOnAccessibilityUsageUpdateDelaySecs = 5;

// How long to wait after `OnScreenReaderStopped` was called before actually
// disabling accessibility support. The main use case is when a screen reader
// or other client is toggled off and on in rapid succession. We don't want to
// destroy the full accessibility tree only to immediately recreate it because
// doing so is bad for performance.
constexpr int kDisableAccessibilitySupportDelaySecs = 2;

// Used for validating the 'basic' bundle parameter for
// --force-renderer-accessibility.
const char kAXModeBundleBasic[] = "basic";

// Used for validating the 'form-controls' bundle parameter for
// --force-renderer-accessibility.
const char kAXModeBundleFormControls[] = "form-controls";

// Update the accessibility histogram 45 seconds after initialization.
static const int ACCESSIBILITY_HISTOGRAM_DELAY_SECS = 45;

}  // namespace

// static
BrowserAccessibilityState* BrowserAccessibilityState::GetInstance() {
  return BrowserAccessibilityStateImpl::GetInstance();
}

// static
BrowserAccessibilityStateImpl* BrowserAccessibilityStateImpl::GetInstance() {
  CHECK(g_instance);
  return g_instance;
}

// On Android, Mac, Lacros, and Windows there are platform-specific subclasses.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_MAC) && \
    !BUILDFLAG(IS_CHROMEOS_LACROS)
// static
std::unique_ptr<BrowserAccessibilityStateImpl>
BrowserAccessibilityStateImpl::Create() {
  return base::WrapUnique(new BrowserAccessibilityStateImpl());
}
#endif

BrowserAccessibilityStateImpl::BrowserAccessibilityStateImpl()
    : BrowserAccessibilityState(),
      histogram_delay_(base::Seconds(ACCESSIBILITY_HISTOGRAM_DELAY_SECS)) {
  DCHECK_EQ(g_instance, nullptr);
  g_instance = this;

  bool disallow_changes = false;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableRendererAccessibility)) {
    disallow_changes = true;
  } else if (base::CommandLine::ForCurrentProcess()->HasSwitch(
                 switches::kForceRendererAccessibility)) {
#if BUILDFLAG(IS_WIN)
    std::string ax_mode_bundle = base::WideToUTF8(
        base::CommandLine::ForCurrentProcess()->GetSwitchValueNative(
            switches::kForceRendererAccessibility));
#else
    std::string ax_mode_bundle =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueNative(
            switches::kForceRendererAccessibility);
#endif

    if (ax_mode_bundle.empty()) {
      // For backwards compatibility, when --force-renderer-accessibility has no
      // parameter, use the complete bundle but allow changes.
      AddAccessibilityModeFlags(ui::kAXModeComplete);
    } else {
      // Support --force-renderer-accessibility=[basic|form-controls|complete]
      disallow_changes = true;
      if (ax_mode_bundle.compare(kAXModeBundleBasic) == 0) {
        AddAccessibilityModeFlags(ui::kAXModeBasic);
      } else if (ax_mode_bundle.compare(kAXModeBundleFormControls) == 0) {
        AddAccessibilityModeFlags(ui::kAXModeFormControls);
      } else {
        // If AXMode is 'complete' or invalid, default to complete bundle.
        AddAccessibilityModeFlags(ui::kAXModeComplete);
      }
    }
  }

  // Hook ourselves up to observe ax mode changes.
  ui::AXPlatformNode::AddAXModeObserver(this);

  if (disallow_changes) {
    DisallowAXModeChanges();
  }
}

void BrowserAccessibilityStateImpl::InitBackgroundTasks() {
  // Schedule calls to update histograms after a delay.
  //
  // The delay is necessary because assistive technology sometimes isn't
  // detected until after the user interacts in some way, so a reasonable delay
  // gives us better numbers.

  // Some things can be done on another thread safely.
  base::ThreadPool::PostDelayedTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(
          &BrowserAccessibilityStateImpl::UpdateHistogramsOnOtherThread,
          base::Unretained(this)),
      histogram_delay_);

  // Other things must be done on the UI thread (e.g. to access PrefService).
  GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&BrowserAccessibilityStateImpl::UpdateHistogramsOnUIThread,
                     base::Unretained(this)),
      histogram_delay_);
}

BrowserAccessibilityStateImpl::~BrowserAccessibilityStateImpl() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;

  // Remove ourselves from the AXMode global observer list.
  ui::AXPlatformNode::RemoveAXModeObserver(this);
}

void BrowserAccessibilityStateImpl::OnScreenReaderDetected() {
  // Clear any previous, now obsolete, request to disable support.
  disable_accessibility_request_time_ = base::TimeTicks();

  if (disallow_ax_mode_changes_) {
    return;
  }
  EnableAccessibility();
}

void BrowserAccessibilityStateImpl::OnScreenReaderStopped() {
  disable_accessibility_request_time_ = ui::EventTimeForNow();

  // If a screen reader or other client using accessibility API is toggled off
  // and on in short succession, we risk destroying and recreating large
  // accessibility trees unnecessarily which is bad for performance. So we post
  // a delayed task here, and only reset accessibility mode if nothing has
  // requested accessibility support be re-enabled after that delay has passed.
  GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &BrowserAccessibilityStateImpl::MaybeResetAccessibilityMode,
          weak_factory_.GetWeakPtr()),
      base::Seconds(kDisableAccessibilitySupportDelaySecs));
}

void BrowserAccessibilityStateImpl::EnableAccessibility() {
  // First disable any non-additive modes that restrict or filter the
  // information available in the tree.
  RemoveAccessibilityModeFlags(ui::kAXModeFormControls);

  AddAccessibilityModeFlags(ui::kAXModeComplete);
}

void BrowserAccessibilityStateImpl::DisableAccessibility() {
  ResetAccessibilityMode();
}

bool BrowserAccessibilityStateImpl::IsRendererAccessibilityEnabled() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableRendererAccessibility);
}

void BrowserAccessibilityStateImpl::ResetAccessibilityModeValue() {
  if (disallow_ax_mode_changes_) {
    return;
  }

  accessibility_mode_ = ui::AXMode();
}

void BrowserAccessibilityStateImpl::MaybeResetAccessibilityMode() {
  // `OnScreenReaderStopped` sets `disable_accessibility_request_time_`, and
  // `OnScreenReaderDetected` clears it. If we no longer have a request time
  // to disable accessibility, this delayed task is obsolete.
  if (disable_accessibility_request_time_.is_null())
    return;

  // `OnScreenReaderStopped` could be called multiple times prior to the delay
  // expiring. The value of `disable_accessibility_request_time_` is updated
  // for every call. If we're running this task prior to the delay expiring,
  // this request time to disable accessibility is obsolete.
  if ((base::TimeTicks::Now() - disable_accessibility_request_time_) <
      base::Seconds(kDisableAccessibilitySupportDelaySecs)) {
    return;
  }

  ResetAccessibilityMode();
}

void BrowserAccessibilityStateImpl::ResetAccessibilityMode() {
  ResetAccessibilityModeValue();

  // AXPlatformNode has its own AXMode. If we don't reset it when accessibility
  // support is auto-disabled, the next time a screen reader is detected
  // |AXPlatformNode::NotifyAddAXModeFlags| will return early due to the
  // AXPlatformNode's AXMode being unchanged (kAXModeComplete). As a result,
  // the observers are never notified and screen reader support fails to work.
  ui::AXPlatformNode::SetAXMode(accessibility_mode_);

  NotifyWebContentsToSetAXMode(accessibility_mode_);
}

bool BrowserAccessibilityStateImpl::IsAccessibleBrowser() {
  return accessibility_mode_ == ui::kAXModeComplete;
}

void BrowserAccessibilityStateImpl::AddUIThreadHistogramCallback(
    base::OnceClosure callback) {
  ui_thread_histogram_callbacks_.push_back(std::move(callback));
}

void BrowserAccessibilityStateImpl::AddOtherThreadHistogramCallback(
    base::OnceClosure callback) {
  other_thread_histogram_callbacks_.push_back(std::move(callback));
}

void BrowserAccessibilityStateImpl::UpdateHistogramsForTesting() {
  UpdateHistogramsOnUIThread();
  UpdateHistogramsOnOtherThread();
}

void BrowserAccessibilityStateImpl::SetCaretBrowsingState(bool enabled) {
  caret_browsing_enabled_ = enabled;
}

bool BrowserAccessibilityStateImpl::IsCaretBrowsingEnabled() const {
  return caret_browsing_enabled_;
}

void BrowserAccessibilityStateImpl::SetPerformanceFilteringAllowed(
    bool allowed) {
  performance_filtering_allowed_ = allowed;
}

bool BrowserAccessibilityStateImpl::IsPerformanceFilteringAllowed() {
  return performance_filtering_allowed_;
}

void BrowserAccessibilityStateImpl::UpdateHistogramsOnUIThread() {
  for (auto& callback : ui_thread_histogram_callbacks_)
    std::move(callback).Run();
  ui_thread_histogram_callbacks_.clear();

  UMA_HISTOGRAM_BOOLEAN(
      "Accessibility.ManuallyEnabled",
      !accessibility_mode_.is_mode_off() && disallow_ax_mode_changes_);

  ui_thread_done_ = true;
  if (other_thread_done_ && background_thread_done_callback_)
    std::move(background_thread_done_callback_).Run();
}

void BrowserAccessibilityStateImpl::UpdateHistogramsOnOtherThread() {
  for (auto& callback : other_thread_histogram_callbacks_)
    std::move(callback).Run();
  other_thread_histogram_callbacks_.clear();

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&BrowserAccessibilityStateImpl::OnOtherThreadDone,
                     base::Unretained(this)));
}

void BrowserAccessibilityStateImpl::OnOtherThreadDone() {
  other_thread_done_ = true;
  if (ui_thread_done_ && background_thread_done_callback_)
    std::move(background_thread_done_callback_).Run();
}

void BrowserAccessibilityStateImpl::UpdateAccessibilityActivityTask() {
  base::TimeTicks now = ui::EventTimeForNow();
  accessibility_last_usage_time_ = now;
  if (accessibility_active_start_time_.is_null())
    accessibility_active_start_time_ = now;
  // If accessibility was enabled but inactive until now, log the amount
  // of time between now and the last API usage.
  if (!accessibility_inactive_start_time_.is_null()) {
    base::UmaHistogramLongTimes("Accessibility.InactiveTime",
                                now - accessibility_inactive_start_time_);
    accessibility_inactive_start_time_ = base::TimeTicks();
  }
  accessibility_update_task_pending_ = false;
}

void BrowserAccessibilityStateImpl::OnAXModeAdded(ui::AXMode mode) {
  AddAccessibilityModeFlags(mode);
}

ui::AXMode BrowserAccessibilityStateImpl::GetAccessibilityMode() {
  // TODO(accessibility) Combine this with the AXMode we store in AXPlatformNode
  // into a single global AXMode tracker in ui/accessibility. The current
  // situation of storing in two places could lead to misalignment.
  CHECK_EQ(accessibility_mode_, ui::AXPlatformNode::GetAccessibilityMode())
      << "Accessibility modes in content and UI are misaligned.";
  return accessibility_mode_;
}

void BrowserAccessibilityStateImpl::OnUserInputEvent() {
  // No need to do anything if accessibility is off, or if it was forced on.
  if (accessibility_mode_.is_mode_off() || disallow_ax_mode_changes_) {
    return;
  }

  // If we get at least kAutoDisableAccessibilityEventCount user input
  // events, more than kAutoDisableAccessibilityTimeSecs apart, with
  // no accessibility API usage in-between disable accessibility.
  // (See also OnAccessibilityApiUsage()).
  base::TimeTicks now = ui::EventTimeForNow();
  user_input_event_count_++;
  if (user_input_event_count_ == 1) {
    first_user_input_event_time_ = now;
    return;
  }

  if (user_input_event_count_ < kAutoDisableAccessibilityEventCount)
    return;

  if (now - first_user_input_event_time_ >
      base::Seconds(kAutoDisableAccessibilityTimeSecs)) {
    if (!accessibility_active_start_time_.is_null()) {
      base::UmaHistogramLongTimes(
          "Accessibility.ActiveTime",
          accessibility_last_usage_time_ - accessibility_active_start_time_);

      // This will help track the time accessibility spends enabled, but
      // inactive.
      if (!features::IsAutoDisableAccessibilityEnabled())
        accessibility_inactive_start_time_ = accessibility_last_usage_time_;

      accessibility_active_start_time_ = base::TimeTicks();
    }

    // Check if the feature to auto-disable accessibility is even enabled.
    if (features::IsAutoDisableAccessibilityEnabled()) {
      base::UmaHistogramCounts1000("Accessibility.AutoDisabled.EventCount",
                                   user_input_event_count_);
      DCHECK(!accessibility_enabled_time_.is_null());
      base::UmaHistogramLongTimes("Accessibility.AutoDisabled.EnabledTime",
                                  now - accessibility_enabled_time_);

      accessibility_disabled_time_ = now;
      DisableAccessibility();
    }
  }
}

void BrowserAccessibilityStateImpl::OnAccessibilityApiUsage() {
  // See OnUserInputEvent for how this is used to disable accessibility.
  user_input_event_count_ = 0;

  // See comment above kOnAccessibilityUsageUpdateDelaySecs for why we post a
  // delayed task.
  if (!accessibility_update_task_pending_) {
    accessibility_update_task_pending_ = true;
    GetUIThreadTaskRunner({})->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &BrowserAccessibilityStateImpl::UpdateAccessibilityActivityTask,
            base::Unretained(this)),
        base::Seconds(kOnAccessibilityUsageUpdateDelaySecs));
  }
}

void BrowserAccessibilityStateImpl::UpdateUniqueUserHistograms() {}

#if BUILDFLAG(IS_ANDROID)
void BrowserAccessibilityStateImpl::SetImageLabelsModeForProfile(
    bool enabled,
    BrowserContext* profile) {}
#endif

void BrowserAccessibilityStateImpl::DisallowAXModeChanges() {
  disallow_ax_mode_changes_ = true;
  ui::AXPlatformNode::DisallowAXModeChanges();
}

void BrowserAccessibilityStateImpl::AddAccessibilityModeFlags(ui::AXMode mode) {
  if (disallow_ax_mode_changes_) {
    return;
  }

  // Adding an accessibility mode flag is generally the result of an
  // accessibility API call, so we should also reset the auto-disable
  // accessibility code. The only exception is in tests or when a user manually
  // toggles accessibility flags in chrome://accessibility.
  OnAccessibilityApiUsage();

  ui::AXMode previous_mode = accessibility_mode_;
  accessibility_mode_ |= mode;

  // Form controls mode is restrictive. There are other modes that should not be
  // used in combination with it.
  if (accessibility_mode_.HasExperimentalFlags(
          ui::AXMode::kExperimentalFormControls)) {
    CHECK(!accessibility_mode_.has_mode(ui::AXMode::kInlineTextBoxes));
    CHECK(!accessibility_mode_.has_mode(ui::AXMode::kScreenReader));
  }

  if (accessibility_mode_ == previous_mode)
    return;

  // Keep track of the total time accessibility is enabled, and the time
  // it was previously disabled.
  if (previous_mode.is_mode_off()) {
    base::TimeTicks now = ui::EventTimeForNow();
    accessibility_enabled_time_ = now;
    if (!accessibility_disabled_time_.is_null()) {
      base::UmaHistogramLongTimes("Accessibility.AutoDisabled.DisabledTime",
                                  now - accessibility_disabled_time_);
    }
  }

  // Proxy the AXMode to AXPlatformNode to enable accessibility.
  ui::AXPlatformNode::NotifyAddAXModeFlags(accessibility_mode_);

  ui::RecordAccessibilityModeHistograms(ui::AXHistogramPrefix::kNone,
                                        accessibility_mode_, previous_mode);

  NotifyWebContentsToAddAXMode(accessibility_mode_);

  // Add a crash key with the ax_mode, to enable searching for top crashes that
  // occur when accessibility is turned on. This adds it for the browser
  // process, and elsewhere the same key is added to renderer processes.
  static auto* ax_mode_crash_key = base::debug::AllocateCrashKeyString(
      "ax_mode", base::debug::CrashKeySize::Size64);
  if (ax_mode_crash_key) {
    base::debug::SetCrashKeyString(ax_mode_crash_key,
                                   accessibility_mode_.ToString());
  }
}

void BrowserAccessibilityStateImpl::RemoveAccessibilityModeFlags(
    ui::AXMode mode) {
  // Turning off accessibility or changing the mode will not be allowed if the
  // --force-renderer-accessibility or --disable-renderer-accessibility command
  // line flags are present, or during testing
  if (disallow_ax_mode_changes_) {
    return;
  }

  int raw_flags = accessibility_mode_.flags() ^
                  (mode.flags() & accessibility_mode_.flags());
  int raw_experimental_flags =
      accessibility_mode_.experimental_flags() ^
      (mode.experimental_flags() & accessibility_mode_.experimental_flags());
  accessibility_mode_ = ui::AXMode(raw_flags, raw_experimental_flags);

  // Proxy the new AXMode to AXPlatformNode.
  ui::AXPlatformNode::SetAXMode(accessibility_mode_);

  NotifyWebContentsToSetAXMode(accessibility_mode_);
}

base::CallbackListSubscription
BrowserAccessibilityStateImpl::RegisterFocusChangedCallback(
    FocusChangedCallback callback) {
  return focus_changed_callbacks_.Add(std::move(callback));
}

void BrowserAccessibilityStateImpl::CallInitBackgroundTasksForTesting(
    base::RepeatingClosure done_callback) {
  // Set the delay to 1 second, that ensures that we actually test having
  // a nonzero delay but the test still runs quickly.
  histogram_delay_ = base::Seconds(1);
  background_thread_done_callback_ = done_callback;
  InitBackgroundTasks();
}

void BrowserAccessibilityStateImpl::OnFocusChangedInPage(
    const FocusedNodeDetails& details) {
  focus_changed_callbacks_.Notify(details);
}

}  // namespace content
