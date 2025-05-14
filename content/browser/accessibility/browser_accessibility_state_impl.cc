// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_state_impl.h"

#include <stddef.h>

#include <algorithm>
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
#include "base/rand_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/browser/accessibility/render_accessibility_host.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/scoped_accessibility_mode.h"
#include "content/public/browser/web_contents_user_data.h"
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

// Parameter values for --force-renderer-accessibility=[bundle-name].
const char kAXModeBundleBasic[] = "basic";
const char kAXModeBundleFormControls[] = "form-controls";
const char kAXModeBundleComplete[] = "complete";

// A data holder attached to a WebContents while it is hidden and has
// accessibility enabled. Used only when the disable_on_hide feature of
// ProgressiveAccessibility is enabled and an active screen reader has not been
// detected.
//
// An instance of this class is attached to a WebContents when it is hidden
// (thereby recording the TimeTicks at which the hide event took place). Its
// `Schedule()` method can later be called to schedule disablement of
// accessibility after the WebContents has been hidden for at least five
// minutes (+/- a randomizer of up to twenty seconds).
//
// The instance is removed from the WebContents and destroyed on the first of:
// *  the WebContents is destroyed (by virtue of being a WebContentsUserData),
// *  the WebContents is revealed (see
//    `BrowserAccessibilityStateImpl::OnWebContentsRevealed()`),
// *  an active screen reader is detected (see
//    `BrowserAccessibilityStateImpl::OnAssistiveTechFound()`), or
// *  the task to disable accessibility runs.
class AccessibilityDisabler
    : public WebContentsUserData<AccessibilityDisabler> {
 public:
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  // Constructs an instance for `web_contents`; see comment above for
  // details. `callback` will be run if this instance is destroyed (either
  // because `web_contents` is destroyed or because `Remove()` is called) before
  // `Schedule()` is called.
  using OnDestroyedBeforeScheduleCallback =
      base::OnceCallback<void(WebContentsImpl* web_contents)>;
  AccessibilityDisabler(WebContents* web_contents,
                        OnDestroyedBeforeScheduleCallback callback)
      : WebContentsUserData(*web_contents),
        on_destroyed_before_schedule_(std::move(callback)) {}

  // This destructor is run either when the WebContents to which this instance
  // is attached is destroyed or when `Remove()` is called.
  ~AccessibilityDisabler() override {
    // If the the instance still has the on_destroyed_before_schedule_ callback,
    // then `Schedule()` has not yet been called. Run the callback now so that
    // the BrowserAccessibilityStateImpl can remove the WebContents from its
    // last_hidden_ collection.
    if (on_destroyed_before_schedule_) {
      std::move(on_destroyed_before_schedule_)
          .Run(&static_cast<WebContentsImpl&>(GetWebContents()));
    }
  }

  // Removes (and destroys) an instance attached to `web_contents`.
  static void Remove(WebContentsImpl* web_contents) {
    web_contents->RemoveUserData(UserDataKey());
  }

  // Schedules a task that will disable accessibility for `web_contents` once
  // it has been hidden for at least five minutes +/- twenty seconds.
  static void Schedule(WebContentsImpl* web_contents) {
    auto* disabler = FromWebContents(web_contents);
    CHECK(disabler);
    // Elapsed ticks since the WebContents was hidden.
    const base::TimeDelta since_hidden =
        base::TimeTicks::Now() - disabler->hide_instant_;
    // Ticks until accessibility should be disabled.
    const base::TimeDelta disable_in =
        BrowserAccessibilityStateImpl::GetRandomizedDisableDelay() -
        since_hidden;

    disabler->disable_ax_timer_.Start(
        FROM_HERE, std::max(disable_in, base::TimeDelta()), disabler,
        &AccessibilityDisabler::DisableAccessibility);

    // Now that this WebContents has been scheduled for disablement, it is no
    // longer in the BrowserAccessibilityStateImpl's last_hidden_ collection,
    // therefore it is no longer necessary to notify it upon destruction.
    disabler->on_destroyed_before_schedule_.Reset();
  }

 private:
  void DisableAccessibility() {
    base::UmaHistogramBoolean("Accessibility.DisabledAfterHide", true);
    auto& web_contents = static_cast<WebContentsImpl&>(GetWebContents());
    web_contents.SetAccessibilityMode({});
    web_contents.RemoveUserData(UserDataKey());  // deletes `this`.
  }

  // A callback to be run if the WebContents is destroyed before `Schedule()` is
  // called.
  OnDestroyedBeforeScheduleCallback on_destroyed_before_schedule_;

  // The time the WebContents was hidden.
  base::TimeTicks hide_instant_{base::TimeTicks::Now()};

  // A timer to disable accessibility after a delay.
  base::OneShotTimer disable_ax_timer_;
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(AccessibilityDisabler);

// A holder of a ScopedModeCollection targeting a specific BrowserContext or
// WebContents. The collection is bound to the lifetime of the target.
class ModeCollectionForTarget : public base::SupportsUserData::Data,
                                public ScopedModeCollection::Delegate {
 public:
  using OnModeChangedCallback =
      base::RepeatingCallback<void(ui::AXMode old_mode, ui::AXMode new_mode)>;
  ModeCollectionForTarget(base::SupportsUserData* target,
                          OnModeChangedCallback on_mode_changed)
      : target_(target), on_mode_changed_(std::move(on_mode_changed)) {}
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

  void OnModeChanged(ui::AXMode old_mode, ui::AXMode new_mode) override {
    // If the collection is no longer bound to the target, the target is in the
    // process of being destroyed. Ignore changes when this is the case.
    if (auto* const collection = FromTarget(target_); collection) {
      on_mode_changed_.Run(old_mode, new_mode);
    }
  }

  ui::AXMode FilterModeFlags(ui::AXMode mode) override { return mode; }

  static const int kUserDataKey = 0;

  raw_ptr<base::SupportsUserData> target_;
  OnModeChangedCallback on_mode_changed_;
  ScopedModeCollection scoped_mode_collection_{*this};
};

// static
const int ModeCollectionForTarget::kUserDataKey;

// Returns a subset of `mode` for delivery to a WebContents.
ui::AXMode FilterAccessibilityModeInvariants(ui::AXMode mode) {
  // kFromPlatform is never sent to WebContents.
  CHECK(!mode.has_mode(ui::AXMode::kFromPlatform));

  // Strip kLabelImages if kExtendedProperties is absent.
  // TODO(grt): kLabelImages is a feature of //chrome. Find a way to
  // achieve this filtering without teaching //content about it. Perhaps via
  // the delegate interface to be added in support of https://crbug.com/1470199.
  if (ui::AXMode(mode.flags() ^ ui::AXMode::kExtendedProperties)
          .has_mode(ui::AXMode::kLabelImages |
                    ui::AXMode::kExtendedProperties)) {
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
      mode.has_mode(ui::AXMode::kExtendedProperties)) {
    return ui::AXMode(mode.flags(),
                      mode.filter_flags() & ~ui::AXMode::kFormsAndLabelsOnly);
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

// On Android, Mac, Windows and Linux there are platform-specific subclasses.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_MAC) && \
    !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS)
// static
std::unique_ptr<BrowserAccessibilityStateImpl>
BrowserAccessibilityStateImpl::Create() {
  return base::WrapUnique(new BrowserAccessibilityStateImpl());
}
#endif

namespace {

constexpr base::TimeDelta kDisableDelay = base::Minutes(5);
constexpr int kDisableDelayVarianceSeconds = 20;

}  // namespace

// static
base::TimeDelta BrowserAccessibilityStateImpl::GetRandomizedDisableDelay() {
  const base::TimeDelta variance = base::Seconds(base::RandInt(
      -kDisableDelayVarianceSeconds, kDisableDelayVarianceSeconds));
  return kDisableDelay + variance;
}

// static
base::TimeDelta BrowserAccessibilityStateImpl::GetMaxDisableDelay() {
  return kDisableDelay + base::Seconds(kDisableDelayVarianceSeconds);
}

BrowserAccessibilityStateImpl::BrowserAccessibilityStateImpl()
    : platform_ax_mode_(CreateScopedModeForProcess(ui::AXMode())) {
  DCHECK_EQ(g_instance, nullptr);
  g_instance = this;

  bool disallow_changes = false;
  ui::AXMode initial_mode;
  auto& command_line = *base::CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(
          switches::kDisablePlatformAccessibilityIntegration)) {
    SetActivationFromPlatformEnabled(/*enabled=*/false);
  }

  if (command_line.HasSwitch(switches::kDisableRendererAccessibility)) {
    disallow_changes = true;
  } else if (command_line.HasSwitch(switches::kForceRendererAccessibility)) {
#if BUILDFLAG(IS_WIN)
    std::string ax_mode_bundle =
        base::WideToUTF8(command_line.GetSwitchValueNative(
            switches::kForceRendererAccessibility));
#else
    std::string ax_mode_bundle = command_line.GetSwitchValueNative(
        switches::kForceRendererAccessibility);
#endif

    if (ax_mode_bundle.empty()) {
      // For backwards compatibility, when --force-renderer-accessibility has no
      // parameter, use the screen reader bundle but allow changes.
      // This is the best general choice in development and testing scenarios.
      initial_mode = ui::kAXModeComplete | ui::AXMode::kScreenReader;
    } else {
      // Support
      // --force-renderer-accessibility=[basic|form-controls|complete|
      //                                 screen-reader]
      if (ax_mode_bundle.compare(kAXModeBundleBasic) == 0) {
        initial_mode = ui::kAXModeBasic;
      } else if (ax_mode_bundle.compare(kAXModeBundleFormControls) == 0) {
        initial_mode = ui::kAXModeFormControls;
      } else if (ax_mode_bundle.compare(kAXModeBundleComplete) == 0) {
        initial_mode = ui::kAXModeComplete;
      } else {
        // If 'screen-reader', or invalid, default to screen reader bundle,
        // which is the most useful in development and testing scenarios.
        initial_mode = ui::kAXModeComplete | ui::AXMode::kScreenReader;
      }
      disallow_changes = true;
    }
  }

  if (::features::IsAccessibilityOnScreenAXModeEnabled()) {
    initial_mode |= ui::kAXModeOnScreen;
  }

  // Create an initial process-wide ScopedAccessibilityMode whether any flags
  // are enabled or not. Always creating a ScopedAccessibilityMode
  // (even if it holds a mode with all flags off) allows us to avoid null
  // checks elsewhere, thereby simplifying other logic.
  forced_accessibility_mode_ = CreateScopedModeForProcess(initial_mode);

  // Configure the performance experiment if no command-line switches were used.
  if (!disallow_changes && initial_mode.is_mode_off()) {
    experiment_accessibility_mode_ =
        ConfigureAccessibilityPerformanceExperiment();
  }

  UMA_HISTOGRAM_BOOLEAN("Accessibility.ManuallyEnabled",
                        !initial_mode.is_mode_off());

  SetAXModeChangeAllowed(!disallow_changes);
}

BrowserAccessibilityStateImpl::~BrowserAccessibilityStateImpl() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;

  CHECK(last_hidden_.empty());
}

void BrowserAccessibilityStateImpl::OnAssistiveTechFound(
    ui::AssistiveTech assistive_tech) {
  const bool was_screenreader_active = ax_platform_.IsScreenReaderActive();
  ax_platform_.NotifyAssistiveTechChanged(assistive_tech);

  // Terminate disable_on_hide if a screen reader has just become active. Do
  // this without first checking the feature to avoid activating the field trial
  // when it's not already active. Performing this removal when the feature is
  // off is harmless.
  if (!was_screenreader_active && ax_platform_.IsScreenReaderActive()) {
    // Cancel all disablers. There is one for each WebContents in `last_hidden_`
    // and one for each that has had `AccessibilityDisabler::Schedule()` called.
    // Since these are not specifically tracked, remove a potential disabler
    // from every WebContents. OnDisablerDestroyedForWebContents will be called
    // to remove a WebContents from `last_hidden_` if its disabler has not yet
    // been scheduled.
    std::ranges::for_each(WebContentsImpl::GetAllWebContents(),
                          [](WebContentsImpl* web_contents) {
                            if (!web_contents->IsBeingDestroyed() &&
                                !web_contents->IsNeverComposited()) {
                              AccessibilityDisabler::Remove(web_contents);
                            }
                          });
  }
}

void BrowserAccessibilityStateImpl::RefreshAssistiveTech() {
  bool sr_active = GetAccessibilityMode().has_mode(ui::AXMode::kScreenReader);
  OnAssistiveTechFound(sr_active ? ui::AssistiveTech::kGenericScreenReader
                                 : ui::AssistiveTech::kNone);
}

std::unique_ptr<ScopedAccessibilityMode>
BrowserAccessibilityStateImpl::ConfigureAccessibilityPerformanceExperiment() {
  if (!features::IsAccessibilityPerformanceMeasurementExperimentEnabled()) {
    // This is the control group.
    return nullptr;
  }

  // Checking the flag is what causes the study to be active, so we need to
  // configure the AXModes based on which experiment arm we are in.

  switch (features::GetAccessibilityPerformanceMeasurementExperimentGroup()) {
    case features::AccessibilityPerformanceMeasurementExperimentGroup::
        kAXModeComplete:
      return CreateScopedModeForProcess(ui::kAXModeComplete);
    case features::AccessibilityPerformanceMeasurementExperimentGroup::
        kWebContentsOnly:
      // TODO(accessibility): there seems to be a strange naming here.
      // kWebContentsOnly helper function in ax_mode.h defines almost a
      // kAXModeComplete. However, in experiment setup discussions, we wanted
      // more likely kAXModeBasic, where only the real, AXMode, kWebContents
      // is set. Which one is it?
      return CreateScopedModeForProcess(ui::kAXModeBasic);
    case features::AccessibilityPerformanceMeasurementExperimentGroup::
        kAXModeCompleteNoInlineTextBoxes:
      return CreateScopedModeForProcess(ui::kAXModeComplete &
                                        ~ui::AXMode::kInlineTextBoxes);
    case features::AccessibilityPerformanceMeasurementExperimentGroup::
        kRendererSerializationOnly:
      RenderAccessibilityHost::SetRendererSerializationExperimentEnabled(true);
      return CreateScopedModeForProcess(ui::kAXModeComplete);
  }

  NOTREACHED();
}

void BrowserAccessibilityStateImpl::RefreshAssistiveTechIfNecessary(
    ui::AXMode new_mode) {
  // Platforms that use this default implementation have a perfect signal
  // for screen reader launches. These platforms use AXMode::kScreenReader to
  // actively indicate that a screen reader is active.
  // Other platforms don't have this perfect signal and compute this off-thread,
  // adding/removing AXMode::kScreenReader after detection is complete.
  bool was_screen_reader_active = ax_platform_.IsScreenReaderActive();
  bool has_screen_reader_mode = new_mode.has_mode(ui::AXMode::kScreenReader);
  if (was_screen_reader_active != has_screen_reader_mode) {
    RefreshAssistiveTech();
  }
}

ui::AssistiveTech BrowserAccessibilityStateImpl::ActiveAssistiveTech() const {
  return ax_platform_.active_assistive_tech();
}

void BrowserAccessibilityStateImpl::SetPerformanceFilteringAllowed(
    bool allowed) {
  performance_filtering_allowed_ = allowed;
}

bool BrowserAccessibilityStateImpl::IsPerformanceFilteringAllowed() {
  return performance_filtering_allowed_;
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

bool BrowserAccessibilityStateImpl::ShouldBlockAutoDisable() {
  // This condition should only occur if a known assistive tech is active.
  // * If the assistive tech is actually still active, it indicates an error
  // with the heuristic, and we should notify a histogram so that we can
  // gather data and improve the heuristic's logic, as well as block the auto
  // disable from occurring.
  // * If the assistive tech is no longer active, then it has been unloaded
  // and it is fine to auto-disable.
  // Reaching here should be a rare case, and therefore we call the 'slow'
  // code (uses system calls on Windows/Linux) to update the running active
  // assistive tech state, before we make a determination.
  return ActiveAssistiveTech() != ui::AssistiveTech::kNone;
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
  // TODO(accessibility) This heuristic will possibly be removed because it's
  // easy for user input events to occur without causing any changes to the
  // a11y tree, or firing any events that an assistive tech would process.
  // However, we should also consider whether to use this heuristic in addition
  // to the focus/load complete one. Some categories of AT don't listen to focus
  // or load complete either e.g. Select to Speak. It may not be necessary for
  // Select-To-Speak to block auto disable if the disabling is lazy, e.g. on
  // next page load and just for this WebContents.
  base::TimeTicks now = ui::EventTimeForNow();
  user_input_event_count_++;
  if (user_input_event_count_ == 1) {
    first_user_input_event_time_ = now;
    return;
  }

  if (user_input_event_count_ < kAutoDisableAccessibilityEventCount) {
    return;
  }

  if (ShouldBlockAutoDisable()) {
    base::UmaHistogramEnumeration(
        "Accessibility.AutoDisabled.BlockedAfter.UserInput",
        ActiveAssistiveTech());
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

      // TODO(accessibility) Reimplement by making a11y dormant as opposed to
      // turning off flags, which leads to thrashing.
    }
  }
}

void BrowserAccessibilityStateImpl::SetAXModeChangeAllowed(bool allowed) {
  allow_ax_mode_changes_ = allowed;
  ui::AXPlatformNode::SetAXModeChangeAllowed(allowed);
}

bool BrowserAccessibilityStateImpl::IsAXModeChangeAllowed() const {
  return allow_ax_mode_changes_;
}

void BrowserAccessibilityStateImpl::SetActivationFromPlatformEnabled(
    bool enabled) {
  if (activation_from_platform_enabled_ == enabled) {
    return;
  }
  activation_from_platform_enabled_ = enabled;
  scoped_modes_for_process_.Recompute(MakePassKey());
}

bool BrowserAccessibilityStateImpl::IsActivationFromPlatformEnabled() {
  return activation_from_platform_enabled_;
}

bool BrowserAccessibilityStateImpl::
    IsAccessibilityPerformanceMeasurementExperimentActive() const {
  return experiment_accessibility_mode_.get();
}

void BrowserAccessibilityStateImpl::NotifyWebContentsPreferencesChanged()
    const {
  for (WebContentsImpl* wc : WebContentsImpl::GetAllWebContents()) {
    wc->OnWebPreferencesChanged();
  }
}

base::CallbackListSubscription
BrowserAccessibilityStateImpl::RegisterFocusChangedCallback(
    FocusChangedCallback callback) {
  return focus_changed_callbacks_.Add(std::move(callback));
}

void BrowserAccessibilityStateImpl::EnableAXModeFromPlatform(
    ui::AXMode modes_to_add) {
  ui::AXMode old_mode = platform_ax_mode_->mode();
  ui::AXMode new_mode = old_mode | modes_to_add;
  if (old_mode != new_mode) {
    platform_ax_mode_ =
        CreateScopedModeForProcess(new_mode | ui::AXMode::kFromPlatform);
  }

  // If AXMode::kWebContent is being requested, turn off auto-disable.
  // TODO(accessibility) Re-work the auto-disable feature.
  // Platform accessibility API usage affects auto-disable.
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

void BrowserAccessibilityStateImpl::OnMinimalPropertiesUsed() {
  // When only basic minimal functionality is used, just enable kNativeAPIs.
  // Enabling kNativeAPIs gives little perf impact, but allows these APIs to
  // interact with the BrowserAccessibilityManager allowing ATs to be able at
  // least find the document without using any advanced APIs.
  EnableAXModeFromPlatform(ui::AXMode::kNativeAPIs);
}

void BrowserAccessibilityStateImpl::OnPropertiesUsedInBrowserUI() {
  EnableAXModeFromPlatform(ui::AXMode::kNativeAPIs);
}

void BrowserAccessibilityStateImpl::OnPropertiesUsedInWebContent() {
  // When accessibility APIs have been used in content, enable basic web
  // accessibility support. Full screen reader support is detected later when
  // specific more advanced APIs are accessed.
  EnableAXModeFromPlatform(ui::kAXModeBasic);
}

void BrowserAccessibilityStateImpl::OnInlineTextBoxesUsedInWebContent() {
  EnableAXModeFromPlatform(ui::kAXModeBasic | ui::AXMode::kInlineTextBoxes);
}

void BrowserAccessibilityStateImpl::OnExtendedPropertiesUsedInWebContent() {
  EnableAXModeFromPlatform(ui::kAXModeBasic | ui::AXMode::kExtendedProperties);
}

void BrowserAccessibilityStateImpl::OnHTMLAttributesUsed() {
  EnableAXModeFromPlatform(ui::kAXModeBasic | ui::AXMode::kHTML);
}

void BrowserAccessibilityStateImpl::OnActionFromAssistiveTech() {
  // Ensure that auto-disable is turned off, e.g. if screen reader scrolls
  // content into view.
  EnableAXModeFromPlatform(ui::AXMode::kNativeAPIs);
}

void BrowserAccessibilityStateImpl::OnPageNavigationComplete() {
  ++num_page_navs_before_first_use_;
}

void BrowserAccessibilityStateImpl::OnWebContentsInitialized(
    WebContentsImpl* web_contents) {
  const ui::AXMode effective_mode = FilterAccessibilityModeInvariants(
      GetAccessibilityMode() |
      ModeCollectionForTarget::GetAccessibilityMode(
          web_contents->GetBrowserContext()) |
      ui::AXMode());

  // Return early to avoid activating the field trial when accessibility is not
  // enabled.
  if (effective_mode.is_mode_off()) {
    return;
  }

  // Do not set any initial accessibility mode if ProgressiveAccessibility is
  // enabled and the WebContents is initially hidden. This behavior is the same
  // for both the only_enable and disable_on_hide variants of the feature.
  if (web_contents->GetVisibility() == Visibility::HIDDEN &&
      base::FeatureList::IsEnabled(features::kProgressiveAccessibility)) {
    return;
  }

  web_contents->SetAccessibilityMode(effective_mode);
}

void BrowserAccessibilityStateImpl::OnWebContentsRevealed(
    WebContentsImpl* web_contents) {
  // Unconditionally cancel the disabler; even if the "disable_on_hide" mode is
  // not selected. Do this without first checking the feature to avoid
  // activating the field trial when it's not already active. Performing this
  // removal when the feature is off is harmless. When the feature is active,
  // this removal will call OnDisablerDestroyedForWebContents to remove
  // `web_contents` from `last_hidden_` if the disabler has not yet been
  // scheduled.
  AccessibilityDisabler::Remove(web_contents);

  const ui::AXMode effective_mode = FilterAccessibilityModeInvariants(
      GetAccessibilityMode() |
      ModeCollectionForTarget::GetAccessibilityMode(
          web_contents->GetBrowserContext()) |
      ModeCollectionForTarget::GetAccessibilityMode(web_contents));

  // Return early to avoid activating the field trial when accessibility is not
  // enabled.
  if (effective_mode == web_contents->GetAccessibilityMode()) {
    return;
  }

  // No special behavior when ProgressiveAccessibility is not enabled.
  if (!base::FeatureList::IsEnabled(features::kProgressiveAccessibility)) {
    return;
  }

  // Send the current mode flags to the WebContents and its renderers.
  web_contents->SetAccessibilityMode(effective_mode);
}

void BrowserAccessibilityStateImpl::OnWebContentsHidden(
    WebContentsImpl* web_contents) {
  // Return early to avoid activating the field trial when accessibility is not
  // enabled.
  if (web_contents->GetAccessibilityMode().is_mode_off()) {
    return;
  }

  // No special behavior if ProgressiveAccessibility is not enabled, the
  // "disable_on_hide" mode is not selected, or if a screen reader has been
  // detected. This final limitation in in place because screen readers may lose
  // their "point of regard" if the accessibility tree is destroyed and rebuilt;
  // and because functional and fast accessibility is required to serve users of
  // screen readers.
  if (!base::FeatureList::IsEnabled(features::kProgressiveAccessibility) ||
      features::kProgressiveAccessibilityModeParam.Get() !=
          features::ProgressiveAccessibilityMode::kDisableOnHide ||
      ax_platform_.IsScreenReaderActive()) {
    return;
  }

  // Add `web_contents` to the list of the last five hidden WCs.
  CHECK(!base::Contains(last_hidden_, web_contents));
  last_hidden_.push_back(web_contents);

  // Create the disabler for this WebContents. The provided callback will be run
  // if `web_contents` is destroyed before the disabler's `Schedule()` method is
  // called. This is the period in which the WebContents is in this instance's
  // `last_hidden_` collection. `Unretained` is safe here because this instance
  // outlives all WebContents.
  AccessibilityDisabler::CreateForWebContents(
      web_contents,
      base::BindOnce(
          &BrowserAccessibilityStateImpl::OnDisablerDestroyedForWebContents,
          base::Unretained(this)));

  // If there was a sixth, schedule it for dropping 5m after it was hidden.
  if (last_hidden_.size() > kMaxPreservedWebContents) {
    AccessibilityDisabler::Schedule(last_hidden_.front().get());
    last_hidden_.pop_front();
  }
}

void BrowserAccessibilityStateImpl::OnDisablerDestroyedForWebContents(
    WebContentsImpl* web_contents) {
  // Remove `web_contents` from the list of last five hidden WCs.
  CHECK(std::erase(last_hidden_, web_contents));
}

void BrowserAccessibilityStateImpl::OnInputEvent(
    const RenderWidgetHost& widget,
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
  auto scoped_mode_for_process = scoped_modes_for_process_.Add(mode);
  if (!mode.is_mode_off()) {
    // A new mode is being added while the performance experiment may be
    // running, which indicates that user is turning on accessibility features.
    // Stop the experiment if it is running.
    experiment_accessibility_mode_.reset();
    RenderAccessibilityHost::SetRendererSerializationExperimentEnabled(false);
  }
  return scoped_mode_for_process;
}

void BrowserAccessibilityStateImpl::ApplyAccessibilityModeToWebContents(
    WebContentsImpl* web_contents,
    ui::AXMode process_mode,
    ui::AXMode browser_context_mode,
    ui::AXMode web_contents_mode) {
  const ui::AXMode effective_mode = FilterAccessibilityModeInvariants(
      process_mode | browser_context_mode | web_contents_mode);

  // Nothing to do if no change in the WebContents's accessibility mode.
  if (effective_mode == web_contents->GetAccessibilityMode()) {
    return;
  }

  // Unconditionally update the WebContents when turning accessibility off.
  // TODO(accessibility): If there is evidence of jank induced by accessibility
  // being turned off for all WebContentses at once (e.g., if VoiceOver is
  // turned off), consider putting WCs in a queue (maybe only hidden ones) and
  // sending the empty effective mode one at a time with some delay between
  // each.
  if (effective_mode.is_mode_off()) {
    web_contents->SetAccessibilityMode(effective_mode);
    return;
  }

  // Unconditionally update the WebContents if ProgressiveAccessibility is not
  // enabled.
  if (!base::FeatureList::IsEnabled(features::kProgressiveAccessibility)) {
    web_contents->SetAccessibilityMode(effective_mode);
    return;
  }

  // Only update the WebContents if it is not hidden.
  if (web_contents->GetVisibility() != Visibility::HIDDEN) {
    web_contents->SetAccessibilityMode(effective_mode);
  }  // else the WebContents will be updated when it is revealed.
}

// This ScopedModeCollection::Delegate override is called by
// scoped_modes_for_process_ when the effective mode for the collection of
// scopers targeting the process changes.
void BrowserAccessibilityStateImpl::OnModeChanged(ui::AXMode old_mode,
                                                  ui::AXMode new_mode) {
  ui::RecordAccessibilityModeHistograms(ui::AXHistogramPrefix::kNone, new_mode,
                                        old_mode);

  // Track the time since start-up before the kWebContents mode was enabled,
  // ensuring we record this value only one time.
  if (!has_enabled_accessibility_in_session_ &&
      new_mode.has_mode(ui::AXMode::kWebContents)) {
    has_enabled_accessibility_in_session_ = true;
    UMA_HISTOGRAM_LONG_TIMES_100("Accessibility.EngineUse.TimeUntilStart",
                                 first_use_timer_.Elapsed());
    UMA_HISTOGRAM_COUNTS_10000("Accessibility.EngineUse.PageNavsUntilStart",
                               num_page_navs_before_first_use_);
  }

  RefreshAssistiveTechIfNecessary(new_mode);

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
  std::ranges::for_each(
      WebContentsImpl::GetAllWebContents(),
      [this, new_mode](WebContentsImpl* web_contents) {
        if (!web_contents->IsBeingDestroyed() &&
            !web_contents->IsNeverComposited()) {
          ApplyAccessibilityModeToWebContents(
              web_contents, new_mode,
              ModeCollectionForTarget::GetAccessibilityMode(
                  web_contents->GetBrowserContext()),
              ModeCollectionForTarget::GetAccessibilityMode(web_contents));
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

// This ScopedModeCollection::Delegate override is called by
// scoped_modes_for_process_ when recomputing the effective mode for the
// collection of scopers targeting the process.
ui::AXMode BrowserAccessibilityStateImpl::FilterModeFlags(ui::AXMode mode) {
  if (activation_from_platform_enabled_) {
    // Allow mode changes with `kFromPlatform`, but filter out that one bit.
    // It need not be sent to renderers.
    return mode & ~ui::AXMode(ui::AXMode::kFromPlatform);
  }
  // Otherwise, ignore any mode change with `kFromPlatform`.
  return mode.has_mode(ui::AXMode::kFromPlatform) ? ui::AXMode() : mode;
}

std::unique_ptr<ScopedAccessibilityMode>
BrowserAccessibilityStateImpl::CreateScopedModeForBrowserContext(
    BrowserContext* browser_context,
    ui::AXMode mode) {
  // kFromPlatform is only permissible for process-wide scopers.
  CHECK(!mode.has_mode(ui::AXMode::kFromPlatform));
  auto scoped_mode = ModeCollectionForTarget::Add(
      browser_context,
      &BrowserAccessibilityStateImpl::OnModeChangedForBrowserContext, this,
      mode);
  if (!mode.is_mode_off()) {
    // A new mode is being added while the performance experiment may be
    // running, which indicates that user is turning on accessibility features.
    // Stop the experiment if it is running.
    experiment_accessibility_mode_.reset();
    RenderAccessibilityHost::SetRendererSerializationExperimentEnabled(false);
  }
  return scoped_mode;
}

void BrowserAccessibilityStateImpl::OnModeChangedForBrowserContext(
    BrowserContext* browser_context,
    ui::AXMode old_mode,
    ui::AXMode new_mode) {
  // Combine this with the effective mode for each WebContents associated with
  // `browser_context`.
  std::ranges::for_each(
      WebContentsImpl::GetAllWebContents(),
      [this, browser_context, process_mode = GetAccessibilityMode(),
       new_mode](WebContentsImpl* web_contents) {
        if (!web_contents->IsBeingDestroyed() &&
            !web_contents->IsNeverComposited() &&
            web_contents->GetBrowserContext() == browser_context) {
          ApplyAccessibilityModeToWebContents(
              web_contents, process_mode, new_mode,
              ModeCollectionForTarget::GetAccessibilityMode(web_contents));
        }
      });
}

std::unique_ptr<ScopedAccessibilityMode>
BrowserAccessibilityStateImpl::CreateScopedModeForWebContents(
    WebContents* web_contents,
    ui::AXMode mode) {
  // WebContents that are never shown must never have accessibility enabled.
  CHECK(!web_contents->IsNeverComposited());
  // kFromPlatform is only permissible for process-wide scopers.
  CHECK(!mode.has_mode(ui::AXMode::kFromPlatform));
  auto scoped_mode = ModeCollectionForTarget::Add(
      web_contents, &BrowserAccessibilityStateImpl::OnModeChangedForWebContents,
      this, mode);
  if (!mode.is_mode_off()) {
    // A new mode is being added while the performance experiment may be
    // running, which indicates that user is turning on accessibility features.
    // Stop the experiment if it is running.
    experiment_accessibility_mode_.reset();
    RenderAccessibilityHost::SetRendererSerializationExperimentEnabled(false);
  }
  return scoped_mode;
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
  ApplyAccessibilityModeToWebContents(
      static_cast<WebContentsImpl*>(web_contents), GetAccessibilityMode(),
      ModeCollectionForTarget::GetAccessibilityMode(
          web_contents->GetBrowserContext()),
      new_mode);
}

void BrowserAccessibilityStateImpl::OnFocusChangedInPage(
    const FocusedNodeDetails& details) {
  focus_changed_callbacks_.Notify(details);
}

}  // namespace content
