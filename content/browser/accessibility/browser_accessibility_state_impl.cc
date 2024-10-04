// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_state_impl.h"

#include <stddef.h>

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/scoped_accessibility_mode.h"
#include "content/public/common/content_switches.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/accessibility_switches.h"
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

// A holder of a ScopedModeCollection targeting a specific BrowserContext or
// WebContents. The collection is bound to the lifetime of the target.
class ModeCollectionForTarget : public base::SupportsUserData::Data {
 public:
  ModeCollectionForTarget(
      base::SupportsUserData* target,
      ScopedModeCollection::OnModeChangedCallback on_mode_changed)
      : scoped_mode_collection_(base::BindRepeating(
            &ModeCollectionForTarget::OnModeChangedForTarget,
            base::Unretained(this),  // Safe because `this` owns the collection.
            base::Unretained(target),  // `target` outlives `this`.
            std::move(on_mode_changed))) {}
  ModeCollectionForTarget(const ModeCollectionForTarget&) = delete;
  ModeCollectionForTarget& operator=(const ModeCollectionForTarget&) = delete;

  static ui::AXMode GetAccessibilityMode(base::SupportsUserData* target) {
    auto* instance = FromTarget(target);
    return instance ? instance->scoped_mode_collection_.accessibility_mode()
                    : ui::AXMode();
  }

  // Adds a new scoper targeting `target` (a BrowserContext or a WebContents)
  // that applies the accessibility mode flags in `mode`. `on_changed_function`
  // is a pointer to a member function of `BrowserAccessibilityStateImpl` that
  // is called when the effective mode for `target` changes; see
  // `ScopedModeCollection::OnModeChangedCallback`. It is bound into a callback
  // (along with `impl`) when this is the first addition for `target`;
  // otherwise, it (and `impl`) are ignored.
  template <class Target>
  static std::unique_ptr<ScopedAccessibilityMode> Add(
      Target* target,
      void (BrowserAccessibilityStateImpl::*on_changed_function)(Target*,
                                                                 ui::AXMode,
                                                                 ui::AXMode),
      BrowserAccessibilityStateImpl* impl,
      ui::AXMode mode) {
    auto* instance = FromTarget(target);
    if (!instance) {
      auto holder = std::make_unique<ModeCollectionForTarget>(
          target,
          base::BindRepeating(on_changed_function, base::Unretained(impl),
                              base::Unretained(target)));
      instance = holder.get();
      target->SetUserData(&kUserDataKey, std::move(holder));
    }
    return instance->scoped_mode_collection_.Add(mode);
  }

 private:
  static ModeCollectionForTarget* FromTarget(base::SupportsUserData* target) {
    return static_cast<ModeCollectionForTarget*>(
        target->GetUserData(&ModeCollectionForTarget::kUserDataKey));
  }

  void OnModeChangedForTarget(
      base::SupportsUserData* target,
      base::RepeatingCallback<void(ui::AXMode, ui::AXMode)> impl_callback,
      ui::AXMode old_mode,
      ui::AXMode new_mode) {
    // If the collection is no longer bound to the target, the target is in the
    // process of being destroyed. Ignore changes when this is the case.
    if (auto* const collection = FromTarget(target); collection) {
      std::move(impl_callback).Run(old_mode, new_mode);
    }
  }

  static const int kUserDataKey = 0;

  ScopedModeCollection scoped_mode_collection_;
};

// static
const int ModeCollectionForTarget::kUserDataKey;

// Returns a subset of `mode` for delivery to a WebContents.
ui::AXMode FilterAccessibilityModeInvariants(ui::AXMode mode) {
  // Strip kLabelImages if kScreenReader is absent.
  // TODO(grt): kLabelImages is a feature of //chrome. Find a way to
  // achieve this filtering without teaching //content about it. Perhaps via
  // the delegate interface to be added in support of https://crbug.com/1470199.
  if (ui::AXMode(mode.flags() ^ ui::AXMode::kScreenReader)
          .has_mode(ui::AXMode::kLabelImages | ui::AXMode::kScreenReader)) {
    mode.set_mode(ui::AXMode::kLabelImages, false);
  }

  // Modes above kNativeAPIs and kWebContents require kWebContents. Some
  // components may enable higher bits, but those should only be given to a
  // WebContents if that WebContents also has the kWebContents mode enabled;
  // see `content::RenderFrameHostImpl::UpdateAccessibilityMode()` and
  // `content::RenderAccessibilityManager::SetMode()`.
  if (!mode.has_mode(ui::AXMode::kWebContents)) {
    return mode & ui::AXMode::kNativeAPIs;
  }

  // Form controls mode is restrictive. There are other modes that should not be
  // used in combination with it. This could occur if something that needs
  // screen reader mode is turned on after forms control mode. In that case,
  // forms mode must be removed.
  if (mode.has_mode(ui::AXMode::kInlineTextBoxes) ||
      mode.has_mode(ui::AXMode::kScreenReader)) {
    return ui::AXMode(mode.flags(), mode.experimental_flags() &
                                        ~ui::AXMode::kExperimentalFormControls);
  }

  return mode;
}

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
      ax_platform_(*this),
      histogram_delay_(base::Seconds(ACCESSIBILITY_HISTOGRAM_DELAY_SECS)),
      scoped_modes_for_process_(base::BindRepeating(
          &BrowserAccessibilityStateImpl::OnModeChangedForProcess,
          base::Unretained(this))),
      process_accessibility_mode_(scoped_modes_for_process_.Add(ui::AXMode())) {
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
#if BUILDFLAG(IS_ANDROID)
        AddAccessibilityModeFlags(ui::kAXModeFormControls);
#endif
      } else {
        // If AXMode is 'complete' or invalid, default to complete bundle.
        AddAccessibilityModeFlags(ui::kAXModeComplete);
      }
    }
  }

  SetAXModeChangeAllowed(!disallow_changes);
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
}

void BrowserAccessibilityStateImpl::OnScreenReaderDetected() {
  // Clear any previous, now obsolete, request to disable support.
  disable_accessibility_request_time_ = base::TimeTicks();

  if (!allow_ax_mode_changes_) {
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
  if (!allow_ax_mode_changes_) {
    return;
  }

  // Enabling accessibility is generally the result of an accessibility API
  // call, so we should also reset the auto-disable accessibility code. The only
  // exception is in tests or when a user manually toggles accessibility flags
  // in chrome://accessibility.
  OnAccessibilityApiUsage();

  const ui::AXMode previous_mode = process_accessibility_mode_->mode();

  // First disable any non-additive modes that restrict or filter the
  // information available in the tree.
  const ui::AXMode new_mode =
      (previous_mode & ~ui::kAXModeFormControls) | ui::kAXModeComplete;

  process_accessibility_mode_ = CreateScopedModeForProcess(new_mode);
}

void BrowserAccessibilityStateImpl::DisableAccessibility() {
  ResetAccessibilityMode();
}

bool BrowserAccessibilityStateImpl::IsRendererAccessibilityEnabled() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableRendererAccessibility);
}

void BrowserAccessibilityStateImpl::MaybeResetAccessibilityMode() {
  // `OnScreenReaderStopped` sets `disable_accessibility_request_time_`, and
  // `OnScreenReaderDetected` clears it. If we no longer have a request time
  // to disable accessibility, this delayed task is obsolete.
  if (disable_accessibility_request_time_.is_null()) {
    return;
  }

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
  if (!allow_ax_mode_changes_) {
    return;
  }

  process_accessibility_mode_ = CreateScopedModeForProcess(ui::AXMode());
}

bool BrowserAccessibilityStateImpl::IsAccessibleBrowser() {
  return GetAccessibilityMode() == ui::kAXModeComplete;
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

void BrowserAccessibilityStateImpl::SetPerformanceFilteringAllowed(
    bool allowed) {
  performance_filtering_allowed_ = allowed;
}

bool BrowserAccessibilityStateImpl::IsPerformanceFilteringAllowed() {
  return performance_filtering_allowed_;
}

void BrowserAccessibilityStateImpl::UpdateHistogramsOnUIThread() {
  for (auto& callback : ui_thread_histogram_callbacks_) {
    std::move(callback).Run();
  }
  ui_thread_histogram_callbacks_.clear();

  UMA_HISTOGRAM_BOOLEAN(
      "Accessibility.ManuallyEnabled",
      !GetAccessibilityMode().is_mode_off() && !allow_ax_mode_changes_);

  ui_thread_done_ = true;
  if (other_thread_done_ && background_thread_done_callback_) {
    std::move(background_thread_done_callback_).Run();
  }
}

void BrowserAccessibilityStateImpl::UpdateHistogramsOnOtherThread() {
  for (auto& callback : other_thread_histogram_callbacks_) {
    std::move(callback).Run();
  }
  other_thread_histogram_callbacks_.clear();

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&BrowserAccessibilityStateImpl::OnOtherThreadDone,
                     base::Unretained(this)));
}

void BrowserAccessibilityStateImpl::OnOtherThreadDone() {
  other_thread_done_ = true;
  if (ui_thread_done_ && background_thread_done_callback_) {
    std::move(background_thread_done_callback_).Run();
  }
}

void BrowserAccessibilityStateImpl::UpdateAccessibilityActivityTask() {
  if (!g_instance) {
    // There can be a race on shutdown since this is posted as a delayed task.
    return;
  }
  base::TimeTicks now = ui::EventTimeForNow();
  accessibility_last_usage_time_ = now;
  if (accessibility_active_start_time_.is_null()) {
    accessibility_active_start_time_ = now;
  }
  // If accessibility was enabled but inactive until now, log the amount
  // of time between now and the last API usage.
  if (!accessibility_inactive_start_time_.is_null()) {
    base::UmaHistogramLongTimes("Accessibility.InactiveTime",
                                now - accessibility_inactive_start_time_);
    accessibility_inactive_start_time_ = base::TimeTicks();
  }
  accessibility_update_task_pending_ = false;
}

ui::AXMode BrowserAccessibilityStateImpl::GetAccessibilityMode() {
  return scoped_modes_for_process_.accessibility_mode();
}

ui::AXMode BrowserAccessibilityStateImpl::GetAccessibilityModeForBrowserContext(
    BrowserContext* browser_context) {
  return FilterAccessibilityModeInvariants(
      GetAccessibilityMode() |
      ModeCollectionForTarget::GetAccessibilityMode(browser_context));
}

void BrowserAccessibilityStateImpl::OnUserInputEvent() {
  // No need to do anything if accessibility is off, or if it was forced on.
  if (GetAccessibilityMode().is_mode_off() || !allow_ax_mode_changes_) {
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

  if (user_input_event_count_ < kAutoDisableAccessibilityEventCount) {
    return;
  }

  if (now - first_user_input_event_time_ >
      base::Seconds(kAutoDisableAccessibilityTimeSecs)) {
    if (!accessibility_active_start_time_.is_null()) {
      base::UmaHistogramLongTimes(
          "Accessibility.ActiveTime",
          accessibility_last_usage_time_ - accessibility_active_start_time_);

      // This will help track the time accessibility spends enabled, but
      // inactive.
      if (!features::IsAutoDisableAccessibilityEnabled()) {
        accessibility_inactive_start_time_ = accessibility_last_usage_time_;
      }

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

void BrowserAccessibilityStateImpl::UpdateUniqueUserHistograms() {}

void BrowserAccessibilityStateImpl::SetAXModeChangeAllowed(bool allowed) {
  allow_ax_mode_changes_ = allowed;
  ui::AXPlatformNode::SetAXModeChangeAllowed(allowed);
}

bool BrowserAccessibilityStateImpl::IsAXModeChangeAllowed() const {
  return allow_ax_mode_changes_;
}

void BrowserAccessibilityStateImpl::AddAccessibilityModeFlags(ui::AXMode mode) {
  if (!allow_ax_mode_changes_) {
    return;
  }

  // Adding an accessibility mode flag is generally the result of an
  // accessibility API call, so we should also reset the auto-disable
  // accessibility code. The only exception is in tests or when a user manually
  // toggles accessibility flags in chrome://accessibility.
  OnAccessibilityApiUsage();

  // Update process_accessibility_mode_ via SetProcessMode so that the remainder
  // of processing is identical to when AXPlatformNode::NotifyAddAXModeFlags()
  // is called -- it will defer to AXPlatform::SetMode() to update the global
  // set of flags. AXPlatform, itself, defers to its Delegate, which is this
  // instance. This ensures that calls to AddAccessibilityModeFlags() and direct
  // calls to AXPlatformNode::NotifyAddAXModeFlags() down in //ui each follow
  // the same codepath to set the global mode flags, notify observers, dispatch
  // to WebContents, and record metrics.
  SetProcessMode(process_accessibility_mode_->mode() | mode);
}

void BrowserAccessibilityStateImpl::RemoveAccessibilityModeFlags(
    ui::AXMode mode) {
  // Turning off accessibility or changing the mode will not be allowed if the
  // --force-renderer-accessibility or --disable-renderer-accessibility command
  // line flags are present, or during testing
  if (!allow_ax_mode_changes_) {
    return;
  }

  process_accessibility_mode_ =
      CreateScopedModeForProcess(process_accessibility_mode_->mode() & ~mode);
}

base::CallbackListSubscription
BrowserAccessibilityStateImpl::RegisterFocusChangedCallback(
    FocusChangedCallback callback) {
  return focus_changed_callbacks_.Add(std::move(callback));
}

// Returns the effective mode for the process, taking all process-wide scopers
// into account.
ui::AXMode BrowserAccessibilityStateImpl::GetProcessMode() {
  return GetAccessibilityMode();
}

// Replaces the scoper that backs the legacy process-wide mode with one applying
// `new_mode`.
void BrowserAccessibilityStateImpl::SetProcessMode(ui::AXMode new_mode) {
  const ui::AXMode previous_mode = GetAccessibilityMode();
  if (new_mode == previous_mode) {
    return;
  }

  process_accessibility_mode_ = CreateScopedModeForProcess(new_mode);
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

void BrowserAccessibilityStateImpl::OnInputEvent(
    const blink::WebInputEvent& event) {
  // |this| observer cares about user input events (specifically keyboard,
  // mouse & touch events) to decide if the accessibility APIs can be disabled.
  if (event.GetType() == blink::WebInputEvent::Type::kMouseDown ||
      event.GetType() == blink::WebInputEvent::Type::kGestureTapDown ||
      event.GetType() == blink::WebInputEvent::Type::kTouchStart ||
      event.GetType() == blink::WebInputEvent::Type::kRawKeyDown ||
      event.GetType() == blink::WebInputEvent::Type::kKeyDown) {
    OnUserInputEvent();
  }
}

std::unique_ptr<ScopedAccessibilityMode>
BrowserAccessibilityStateImpl::CreateScopedModeForProcess(ui::AXMode mode) {
  return scoped_modes_for_process_.Add(mode);
}

void BrowserAccessibilityStateImpl::OnModeChangedForProcess(
    ui::AXMode old_mode,
    ui::AXMode new_mode) {
  ui::RecordAccessibilityModeHistograms(ui::AXHistogramPrefix::kNone, new_mode,
                                        old_mode);

  // Add a crash key with the ax_mode, to enable searching for top crashes that
  // occur when accessibility is turned on. This adds it for the browser
  // process, and elsewhere the same key is added to renderer processes.
  static auto* const ax_mode_crash_key = base::debug::AllocateCrashKeyString(
      "ax_mode", base::debug::CrashKeySize::Size64);
  if (ax_mode_crash_key) {
    base::debug::SetCrashKeyString(ax_mode_crash_key, new_mode.ToString());
  }

  // Combine the new mode for the process with the effective mode for each
  // WebContents and its associated BrowserContext.
  base::ranges::for_each(
      WebContentsImpl::GetAllWebContents(),
      [new_mode](WebContentsImpl* web_contents) {
        if (!web_contents->IsBeingDestroyed()) {
          web_contents->SetAccessibilityMode(FilterAccessibilityModeInvariants(
              new_mode |
              ModeCollectionForTarget::GetAccessibilityMode(
                  web_contents->GetBrowserContext()) |
              ModeCollectionForTarget::GetAccessibilityMode(web_contents)));
        }
      });

  // Handle additions to the process's mode flags.
  if (const auto additions = new_mode & ~old_mode; !additions.is_mode_off()) {
    // Keep track of the total time accessibility is enabled, and the time
    // it was previously disabled.
    if (old_mode.is_mode_off()) {
      base::TimeTicks now = ui::EventTimeForNow();
      accessibility_enabled_time_ = now;
      if (!accessibility_disabled_time_.is_null()) {
        base::UmaHistogramLongTimes("Accessibility.AutoDisabled.DisabledTime",
                                    now - accessibility_disabled_time_);
      }
    }

    // Broadcast the new mode flags, if any, to the AXModeObservers.
    ax_platform_.NotifyModeAdded(additions);
  }
}

std::unique_ptr<ScopedAccessibilityMode>
BrowserAccessibilityStateImpl::CreateScopedModeForBrowserContext(
    BrowserContext* browser_context,
    ui::AXMode mode) {
  return ModeCollectionForTarget::Add(
      browser_context,
      &BrowserAccessibilityStateImpl::OnModeChangedForBrowserContext, this,
      mode);
}

void BrowserAccessibilityStateImpl::OnModeChangedForBrowserContext(
    BrowserContext* browser_context,
    ui::AXMode old_mode,
    ui::AXMode new_mode) {
  // Combine the effective modes for the process and for `browser_context`.
  ui::AXMode mode_for_context = GetAccessibilityMode() | new_mode;

  // Combine this with the effective mode for each WebContents associated with
  // `browser_context`.
  base::ranges::for_each(
      WebContentsImpl::GetAllWebContents(),
      [browser_context, mode_for_context](WebContentsImpl* web_contents) {
        if (!web_contents->IsBeingDestroyed() &&
            web_contents->GetBrowserContext() == browser_context) {
          web_contents->SetAccessibilityMode(FilterAccessibilityModeInvariants(
              mode_for_context |
              ModeCollectionForTarget::GetAccessibilityMode(web_contents)));
        }
      });
}

std::unique_ptr<ScopedAccessibilityMode>
BrowserAccessibilityStateImpl::CreateScopedModeForWebContents(
    WebContents* web_contents,
    ui::AXMode mode) {
  return ModeCollectionForTarget::Add(
      web_contents, &BrowserAccessibilityStateImpl::OnModeChangedForWebContents,
      this, mode);
}

void BrowserAccessibilityStateImpl::OnModeChangedForWebContents(
    WebContents* web_contents,
    ui::AXMode old_mode,
    ui::AXMode new_mode) {
  if (web_contents->IsBeingDestroyed()) {
    return;
  }

  // Combine the effective modes for the process, `web_contents`'s
  // BrowserContext, and for `web_contents.
  static_cast<WebContentsImpl*>(web_contents)
      ->SetAccessibilityMode(FilterAccessibilityModeInvariants(
          GetAccessibilityMode() |
          ModeCollectionForTarget::GetAccessibilityMode(
              web_contents->GetBrowserContext()) |
          new_mode));
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
