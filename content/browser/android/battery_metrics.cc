// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/battery_metrics.h"

#include "base/android/application_status_listener.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/power_monitor/power_monitor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/trace_event/application_state_proto_android.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/mojom/webpreferences/web_preferences.mojom.h"

namespace content {
namespace {

using ScenarioScope = performance_scenarios::ScenarioScope;
using LoadingScenario = performance_scenarios::LoadingScenario;
using InputScenario = performance_scenarios::InputScenario;

perfetto::protos::pbzero::DeviceThermalState ToTraceEnum(
    base::PowerThermalObserver::DeviceThermalState state) {
  switch (state) {
    case base::PowerThermalObserver::DeviceThermalState::kUnknown:
      return perfetto::protos::pbzero::DEVICE_THERMAL_STATE_UNKNOWN;
    case base::PowerThermalObserver::DeviceThermalState::kNominal:
      return perfetto::protos::pbzero::DEVICE_THERMAL_STATE_NOMINAL;
    case base::PowerThermalObserver::DeviceThermalState::kFair:
      return perfetto::protos::pbzero::DEVICE_THERMAL_STATE_FAIR;
    case base::PowerThermalObserver::DeviceThermalState::kSerious:
      return perfetto::protos::pbzero::DEVICE_THERMAL_STATE_SERIOUS;
    case base::PowerThermalObserver::DeviceThermalState::kCritical:
      return perfetto::protos::pbzero::DEVICE_THERMAL_STATE_CRITICAL;
  }
}

int GetLoadingScenarioPriority(LoadingScenario scenario) {
  switch (scenario) {
    case LoadingScenario::kNoPageLoading:
      return 0;
    case LoadingScenario::kBackgroundPageLoading:
      return 1;
    case LoadingScenario::kVisiblePageLoading:
      return 2;
    case LoadingScenario::kFocusedPageLoading:
      return 3;
  }
  NOTREACHED();
}

int GetInputScenarioPriority(InputScenario scenario) {
  switch (scenario) {
    case InputScenario::kNoInput:
      return 0;
    case InputScenario::kTyping:
      return 1;
    case InputScenario::kTap:
      return 2;
    case InputScenario::kScroll:
      return 3;
  }
  NOTREACHED();
}

std::string GetLoadingScenarioSuffix(std::optional<LoadingScenario> scenario) {
  // LINT.IfChange(LoadingScenarioSuffix)
  if (!scenario.has_value()) {
    return "UnknownLoadingScenario";
  }
  switch (*scenario) {
    case LoadingScenario::kFocusedPageLoading:
      return "FocusedPageLoading";
    case LoadingScenario::kVisiblePageLoading:
      return "VisiblePageLoading";
    case LoadingScenario::kBackgroundPageLoading:
      return "BackgroundPageLoading";
    case LoadingScenario::kNoPageLoading:
      return "NoPageLoading";
  }
  // LINT.ThenChange(/tools/metrics/histograms/metadata/power/histograms.xml:LoadingScenarioSuffix)
  NOTREACHED();
}

std::string GetInputScenarioSuffix(std::optional<InputScenario> scenario) {
  // LINT.IfChange(InputScenarioSuffix)
  if (!scenario.has_value()) {
    return "UnknownInputScenario";
  }
  switch (*scenario) {
    case InputScenario::kScroll:
      return "Scroll";
    case InputScenario::kTap:
      return "Tap";
    case InputScenario::kTyping:
      return "Typing";
    case InputScenario::kNoInput:
      return "NoInput";
  }
  // LINT.ThenChange(/tools/metrics/histograms/metadata/power/histograms.xml:InputScenarioSuffix)
  NOTREACHED();
}

void Report30SecondDrain(int capacity_consumed,
                         bool is_exclusive_measurement,
                         const std::string& scenario) {
  // Drain over the last 30 seconds in uAh. We assume a max current of 10A which
  // translates to a little under 100mAh capacity drain over 30 seconds.
  UMA_HISTOGRAM_COUNTS_100000("Power.ForegroundBatteryDrain.30Seconds",
                              capacity_consumed);
  base::UmaHistogramCounts100000(
      std::string("Power.ForegroundBatteryDrainPerScenario.30Seconds.") +
          scenario,
      capacity_consumed);

  // Record a separate metric for power drain that was completely observed while
  // we were the foreground app. This avoids attributing power draw from other
  // apps to us.
  if (is_exclusive_measurement) {
    UMA_HISTOGRAM_COUNTS_100000(
        "Power.ForegroundBatteryDrain.30Seconds.Exclusive", capacity_consumed);
    base::UmaHistogramCounts100000(
        std::string(
            "Power.ForegroundBatteryDrainPerScenario.30Seconds.Exclusive.") +
            scenario,
        capacity_consumed);
  }
}

base::HistogramBase* GetAvgBatteryDrainHistogram(const char* suffix) {
  static constexpr char kAvgDrainHistogramPrefix[] =
      "Power.ForegroundBatteryDrain.30SecondsAvg2";
  return base::Histogram::FactoryGet(
      std::string(kAvgDrainHistogramPrefix) + suffix, 1, 100000, 50,
      base::HistogramBase::kUmaTargetedHistogramFlag);
}

// Dark mode histograms are reported on the UI thread, because they depend on
// the current darkening state of the web contents -- which we can only inspect
// on the UI thread.
void ReportDarkModeDrains(int capacity_consumed_avg,
                          bool is_exclusive_measurement,
                          int num_sampling_periods) {
  size_t no_darkening_count = 0, user_agent_darkening_count = 0,
         web_page_or_user_agent_darkening_count = 0,
         web_page_darkening_count = 0;
  auto all_webcontents = WebContentsImpl::GetAllWebContents();
  auto total_webcontents_count = all_webcontents.size();
  for (WebContentsImpl* wc : all_webcontents) {
    auto dark_theme = wc->GetOrCreateWebPreferences().preferred_color_scheme ==
                      blink::mojom::PreferredColorScheme::kDark;
    auto force_dark = wc->GetOrCreateWebPreferences().force_dark_mode_enabled;

    if (force_dark) {
      if (dark_theme) {
        web_page_or_user_agent_darkening_count++;
      } else {
        user_agent_darkening_count++;
      }
    } else {
      if (dark_theme) {
        web_page_darkening_count++;
      } else {
        no_darkening_count++;
      }
    }
  }

  base::HistogramBase* dark_mode_histogram = nullptr;
  base::HistogramBase* exclusive_dark_mode_histogram = nullptr;

  if (user_agent_darkening_count + web_page_or_user_agent_darkening_count ==
      total_webcontents_count) {
    // All WebContents have at least user-agent darkening.
    dark_mode_histogram = GetAvgBatteryDrainHistogram(".ForcedDarkMode");
    exclusive_dark_mode_histogram =
        GetAvgBatteryDrainHistogram(".Exclusive.ForcedDarkMode");
  } else if (web_page_darkening_count == total_webcontents_count) {
    // All WebContents have only web page darkening.
    dark_mode_histogram = GetAvgBatteryDrainHistogram(".DarkMode");
    exclusive_dark_mode_histogram =
        GetAvgBatteryDrainHistogram(".Exclusive.DarkMode");
  } else if (no_darkening_count == total_webcontents_count) {
    // None of the WebContents have any darkening.
    dark_mode_histogram = GetAvgBatteryDrainHistogram(".LightMode");
    exclusive_dark_mode_histogram =
        GetAvgBatteryDrainHistogram(".Exclusive.LightMode");
  } else {
    // Some WebContents have some kind of darkening and some might not have any.
    dark_mode_histogram = GetAvgBatteryDrainHistogram(".MixedMode");
    exclusive_dark_mode_histogram =
        GetAvgBatteryDrainHistogram(".Exclusive.MixedMode");
  }
  DCHECK(dark_mode_histogram);
  DCHECK(exclusive_dark_mode_histogram);

  dark_mode_histogram->AddCount(capacity_consumed_avg, num_sampling_periods);
  if (is_exclusive_measurement) {
    exclusive_dark_mode_histogram->AddCount(capacity_consumed_avg,
                                            num_sampling_periods);
  }
}

void ReportAveragedDrain(int capacity_consumed,
                         bool is_exclusive_measurement,
                         int num_sampling_periods) {
  // Averaged drain over 30 second intervals in uAh. We assume a max current of
  // 10A which translates to a little under 100mAh capacity drain over 30
  // seconds.
  auto capacity_consumed_avg = capacity_consumed / num_sampling_periods;

  GetAvgBatteryDrainHistogram("")->AddCount(capacity_consumed_avg,
                                            num_sampling_periods);
  if (is_exclusive_measurement) {
    GetAvgBatteryDrainHistogram(".Exclusive")
        ->AddCount(capacity_consumed_avg, num_sampling_periods);
  }

  GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
      ->PostTask(
          FROM_HERE,
          base::BindOnce(&ReportDarkModeDrains, capacity_consumed_avg,
                         is_exclusive_measurement, num_sampling_periods));
}

}  // namespace

// static
constexpr base::TimeDelta AndroidBatteryMetrics::kMetricsInterval;

// static
void AndroidBatteryMetrics::CreateInstance() {
  static base::NoDestructor<AndroidBatteryMetrics> instance;
}

AndroidBatteryMetrics::AndroidBatteryMetrics()
    : task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AndroidBatteryMetrics::InitializeOnSequence,
                                base::Unretained(this)));
}

AndroidBatteryMetrics::~AndroidBatteryMetrics() {
  // Never called, this is a no-destruct singleton.
  NOTREACHED();
}

void AndroidBatteryMetrics::InitializeOnSequence() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* power_monitor = base::PowerMonitor::GetInstance();
  battery_power_status_ =
      power_monitor->AddPowerStateObserverAndReturnBatteryPowerStatus(this);
  power_monitor->AddPowerThermalObserver(this);
  // The observer is never removed because this class uses base::NoDestructor.
  content::ProcessVisibilityTracker::GetInstance()->AddObserver(this);
  // TODO(b/339859756): Update this call to take into account the unknown battery
  // status.
  UpdateMetricsEnabled();
}

void AndroidBatteryMetrics::TryObservePerformanceScenarios() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_observing_performance_scenarios_) {
    return;
  }
  // We're using ScenarioScope::kGlobal because we're interested in the overall
  // app state, not just the current process.
  if (auto observer_list =
          performance_scenarios::PerformanceScenarioObserverList::GetForScope(
              ScenarioScope::kGlobal)) {
    is_observing_performance_scenarios_ = true;
    performance_scenario_tracker_.UpdateLoadingScenario(
        performance_scenarios::GetLoadingScenario(ScenarioScope::kGlobal)
            ->load(std::memory_order_relaxed));
    performance_scenario_tracker_.UpdateInputScenario(
        performance_scenarios::GetInputScenario(ScenarioScope::kGlobal)
            ->load(std::memory_order_relaxed));
    observer_list->AddObserver(this);
  }
}

void AndroidBatteryMetrics::OnVisibilityChanged(bool visible) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  app_visible_ = visible;
  UpdateMetricsEnabled();
}

void AndroidBatteryMetrics::OnBatteryPowerStatusChange(
    base::PowerStateObserver::BatteryPowerStatus battery_power_status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  battery_power_status_ = battery_power_status;
  UpdateMetricsEnabled();
}

void AndroidBatteryMetrics::OnLoadingScenarioChanged(
    ScenarioScope scope,
    LoadingScenario old_scenario,
    LoadingScenario new_scenario) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  performance_scenario_tracker_.UpdateLoadingScenario(new_scenario);
}

void AndroidBatteryMetrics::OnInputScenarioChanged(ScenarioScope scope,
                                                   InputScenario old_scenario,
                                                   InputScenario new_scenario) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  performance_scenario_tracker_.UpdateInputScenario(new_scenario);
}

void AndroidBatteryMetrics::OnThermalStateChange(DeviceThermalState new_state) {
  TRACE_EVENT_INSTANT(
      "power", "OnThermalStateChange", perfetto::Track::Global(0),
      [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        event->set_chrome_application_state_info()->set_application_state(
            base::trace_event::ApplicationStateToTraceEnum(
                base::android::ApplicationStatusListener::GetState()));
        event->set_device_thermal_state(ToTraceEnum(new_state));
      });
}

void AndroidBatteryMetrics::OnSpeedLimitChange(int speed_limit) {}

void AndroidBatteryMetrics::UpdateMetricsEnabled() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TryObservePerformanceScenarios();

  // We want to attribute battery drain to chromium while the embedding app is
  // visible. Battery drain will only be reflected in remaining battery capacity
  // when the device is not on a charger.
  bool should_be_enabled =
      app_visible_ && (battery_power_status_ ==
                       PowerStateObserver::BatteryPowerStatus::kBatteryPower);

  if (should_be_enabled && !metrics_timer_.IsRunning()) {
    // Capture first capacity measurement and enable the repeating timer.
    last_remaining_capacity_uah_ =
        base::PowerMonitor::GetInstance()->GetRemainingBatteryCapacity();
    skipped_timers_ = 0;
    observed_capacity_drops_ = 0;
    performance_scenario_tracker_.UseLatestScenarios();

    metrics_timer_.Start(
        FROM_HERE, kMetricsInterval,
        base::BindRepeating(&AndroidBatteryMetrics::CaptureAndReportMetrics,
                            base::Unretained(this),
                            /*disabling=*/false));
  } else if (!should_be_enabled && metrics_timer_.IsRunning()) {
    // Capture one last measurement before disabling the timer.
    CaptureAndReportMetrics(/*disabling=*/true);
    metrics_timer_.Stop();
  }
}

void AndroidBatteryMetrics::CaptureAndReportMetrics(bool disabling) {
  int remaining_capacity_uah =
      base::PowerMonitor::GetInstance()->GetRemainingBatteryCapacity();

  const std::string scenario = performance_scenario_tracker_.GetMetricSuffix();
  performance_scenario_tracker_.UseLatestScenarios();

  if (remaining_capacity_uah >= last_remaining_capacity_uah_) {
    // No change in battery capacity, or it increased. The latter could happen
    // if we detected the switch off battery power to a charger late, or if the
    // device reports bogus values. We don't change last_remaining_capacity_uah_
    // here to avoid overreporting in case of fluctuating values.
    skipped_timers_++;
    Report30SecondDrain(0, IsMeasuringDrainExclusively(), scenario);

    if (disabling) {
      // Disabling the timer, but without a change in capacity counter -- We
      // should still emit values for the elapsed time intervals into the
      // average histograms. We exclude exclusive metrics here, because these
      // metrics exclude the measurements before the first capacity drop and
      // after the last drop. Member fields will be reset when tracking
      // is resumed after foregrounding again later.
      ReportAveragedDrain(0, /*is_exclusive_measurement=*/false,
                          skipped_timers_);
    }

    return;
  }
  observed_capacity_drops_++;

  // Report the consumed capacity delta over the last 30 seconds.
  int capacity_consumed = last_remaining_capacity_uah_ - remaining_capacity_uah;
  Report30SecondDrain(capacity_consumed, IsMeasuringDrainExclusively(),
                      scenario);

  // Also record drain over 30 second intervals, but averaged since the last
  // time we recorded an increase (or started recording samples). Because the
  // underlying battery capacity counter is often low-resolution (usually
  // between .5 and 50 mAh), it may only increment after multiple sampling
  // points. For example, a 20 mAh drop over two successive periods of 30
  // seconds will be reported as two samples of 10 mAh.
  ReportAveragedDrain(capacity_consumed, IsMeasuringDrainExclusively(),
                      skipped_timers_ + 1);

  // Also track the total capacity consumed in a single-bucket-histogram,
  // emitting one sample for every 100 uAh drained.
  static constexpr base::Histogram::Sample32 kSampleFactor = 100;
  UMA_HISTOGRAM_SCALED_EXACT_LINEAR("Power.ForegroundBatteryDrain",
                                    /*sample=*/1, capacity_consumed,
                                    /*sample_max=*/1, kSampleFactor);
  if (IsMeasuringDrainExclusively()) {
    UMA_HISTOGRAM_SCALED_EXACT_LINEAR("Power.ForegroundBatteryDrain.Exclusive",
                                      /*sample=*/1, capacity_consumed,
                                      /*sample_max=*/1, kSampleFactor);
  }

  last_remaining_capacity_uah_ = remaining_capacity_uah;
  skipped_timers_ = 0;
}

bool AndroidBatteryMetrics::IsMeasuringDrainExclusively() const {
  return observed_capacity_drops_ >= 2;
}

void AndroidBatteryMetrics::PerformanceScenarioTracker::UpdateLoadingScenario(
    performance_scenarios::LoadingScenario new_scenario) {
  latest_loading_scenario_ = new_scenario;
  if (!loading_scenario_to_report_.has_value() ||
      GetLoadingScenarioPriority(*loading_scenario_to_report_) <
          GetLoadingScenarioPriority(new_scenario)) {
    loading_scenario_to_report_ = new_scenario;
  }
}

void AndroidBatteryMetrics::PerformanceScenarioTracker::UpdateInputScenario(
    performance_scenarios::InputScenario new_scenario) {
  latest_input_scenario_ = new_scenario;
  if (!input_scenario_to_report_.has_value() ||
      GetInputScenarioPriority(*input_scenario_to_report_) <
          GetInputScenarioPriority(new_scenario)) {
    input_scenario_to_report_ = new_scenario;
  }
}

std::string AndroidBatteryMetrics::PerformanceScenarioTracker::GetMetricSuffix()
    const {
  return GetLoadingScenarioSuffix(loading_scenario_to_report_) + "_" +
         GetInputScenarioSuffix(input_scenario_to_report_);
}

void AndroidBatteryMetrics::PerformanceScenarioTracker::UseLatestScenarios() {
  loading_scenario_to_report_ = latest_loading_scenario_;
  input_scenario_to_report_ = latest_input_scenario_;
}

}  // namespace content
