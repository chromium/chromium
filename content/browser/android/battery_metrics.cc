// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/battery_metrics.h"

#include "base/android/application_status_listener.h"
#include "base/android/radio_utils.h"
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
#include "net/android/network_library.h"
#include "net/android/traffic_stats.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/webpreferences/web_preferences.mojom.h"

BASE_FEATURE(kForegroundRadioStateCountWakeups,
             "ForegroundRadioStateCountWakeups",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Keeps reporting of the battery metrics on the UI thread, where it may cause
// jank. This is used for a holdback experiment to estimate the jank reduction
// won by moving reporting to the thread pool.
// TODO(eseckler): Remove once holdback experiment is complete.
BASE_FEATURE(kAndroidBatteryMetricsReportOnUIThread,
             "AndroidBatteryMetricsReportOnUIThread",
             base::FEATURE_DISABLED_BY_DEFAULT);

namespace content {
namespace {

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

void Report30SecondRadioUsage(int64_t tx_bytes, int64_t rx_bytes, int wakeups) {
  if (!base::android::RadioUtils::IsSupported())
    return;

  if (base::android::RadioUtils::GetConnectionType() ==
      base::android::RadioConnectionType::kWifi) {
    absl::optional<int32_t> maybe_level = net::android::GetWifiSignalLevel();
    if (!maybe_level.has_value())
      return;

    base::android::RadioSignalLevel wifi_level =
        static_cast<base::android::RadioSignalLevel>(*maybe_level);
    UMA_HISTOGRAM_ENUMERATION("Power.ForegroundRadio.SignalLevel.Wifi",
                              wifi_level);

    // Traffic sent over network during the last 30 seconds in kibibytes.
    UMA_HISTOGRAM_SCALED_ENUMERATION(
        "Power.ForegroundRadio.SentKiB.Wifi.30Seconds", wifi_level, tx_bytes,
        1024);

    // Traffic received over network during the last 30 seconds in kibibytes.
    UMA_HISTOGRAM_SCALED_ENUMERATION(
        "Power.ForegroundRadio.ReceivedKiB.Wifi.30Seconds", wifi_level,
        rx_bytes, 1024);
  } else {
    absl::optional<base::android::RadioSignalLevel> maybe_level =
        base::android::RadioUtils::GetCellSignalLevel();
    if (!maybe_level.has_value())
      return;

    base::android::RadioSignalLevel cell_level = *maybe_level;
    UMA_HISTOGRAM_ENUMERATION("Power.ForegroundRadio.SignalLevel.Cell",
                              cell_level);

    // Traffic sent over network during the last 30 seconds in kibibytes.
    UMA_HISTOGRAM_SCALED_ENUMERATION(
        "Power.ForegroundRadio.SentKiB.Cell.30Seconds", cell_level, tx_bytes,
        1024);

    // Traffic received over network during the last 30 seconds in kibibytes.
    UMA_HISTOGRAM_SCALED_ENUMERATION(
        "Power.ForegroundRadio.ReceivedKiB.Cell.30Seconds", cell_level,
        rx_bytes, 1024);

    // Number of radio wakeups during the last 30 seconds.
    if (base::FeatureList::IsEnabled(kForegroundRadioStateCountWakeups) &&
        wakeups > 0) {
      static const int kMaxLevel =
          static_cast<int>(base::android::RadioSignalLevel::kMaxValue);
      static const char kWakeupsHistogramName[] =
          "Power.ForegroundRadio.Wakeups.Cell.30Seconds";
      STATIC_HISTOGRAM_POINTER_BLOCK(
          kWakeupsHistogramName,
          AddCount(static_cast<int>(cell_level), wakeups),
          base::Histogram::FactoryGet(
              kWakeupsHistogramName, 0, kMaxLevel, kMaxLevel + 1,
              base::HistogramBase::kUmaTargetedHistogramFlag));
    }
  }
}

void Report30SecondDrain(int capacity_consumed, bool is_exclusive_measurement) {
  // Drain over the last 30 seconds in uAh. We assume a max current of 10A which
  // translates to a little under 100mAh capacity drain over 30 seconds.
  UMA_HISTOGRAM_COUNTS_100000("Power.ForegroundBatteryDrain.30Seconds",
                              capacity_consumed);

  // Record a separate metric for power drain that was completely observed while
  // we were the foreground app. This avoids attributing power draw from other
  // apps to us.
  if (is_exclusive_measurement) {
    UMA_HISTOGRAM_COUNTS_100000(
        "Power.ForegroundBatteryDrain.30Seconds.Exclusive", capacity_consumed);
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

  // TODO(eseckler): Remove conditional once
  // kAndroidBatteryMetricsReportOnUIThread is gone.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
        ->PostTask(
            FROM_HERE,
            base::BindOnce(&ReportDarkModeDrains, capacity_consumed_avg,
                           is_exclusive_measurement, num_sampling_periods));
  }
}

}  // namespace

// static
constexpr base::TimeDelta AndroidBatteryMetrics::kMetricsInterval;
constexpr base::TimeDelta AndroidBatteryMetrics::kRadioStateInterval;

// static
void AndroidBatteryMetrics::CreateInstance() {
  static base::NoDestructor<AndroidBatteryMetrics> instance;
}

AndroidBatteryMetrics::AndroidBatteryMetrics()
    : task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  // TODO(eseckler): Remove conditional once
  // kAndroidBatteryMetricsReportOnUIThread is gone.
  if (base::FeatureList::IsEnabled(kAndroidBatteryMetricsReportOnUIThread)) {
    // Initializing on the current (UI) thread registers all observers on the UI
    // thread, such that all notifications will be received on the UI thread,
    // too.
    InitializeOnSequence();
  } else {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&AndroidBatteryMetrics::InitializeOnSequence,
                                  base::Unretained(this)));
  }
}

AndroidBatteryMetrics::~AndroidBatteryMetrics() {
  // Never called, this is a no-destruct singleton.
  NOTREACHED();
}

void AndroidBatteryMetrics::InitializeOnSequence() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  on_battery_power_ =
      base::PowerMonitor::AddPowerStateObserverAndReturnOnBatteryState(this);
  base::PowerMonitor::AddPowerThermalObserver(this);
  content::ProcessVisibilityTracker::GetInstance()->AddObserver(this);
  UpdateMetricsEnabled();
}

void AndroidBatteryMetrics::OnVisibilityChanged(bool visible) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  app_visible_ = visible;
  UpdateMetricsEnabled();
}

void AndroidBatteryMetrics::OnPowerStateChange(bool on_battery_power) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  on_battery_power_ = on_battery_power;
  UpdateMetricsEnabled();
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

  if (!app_visible_)
    return;

  base::UmaHistogramEnumeration(
      "Power.ForegroundThermalState.ChangeEvent.Android", new_state);
}

void AndroidBatteryMetrics::OnSpeedLimitChange(int speed_limit) {}

void AndroidBatteryMetrics::UpdateMetricsEnabled() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // We want to attribute battery drain to chromium while the embedding app is
  // visible. Battery drain will only be reflected in remaining battery capacity
  // when the device is not on a charger.
  bool should_be_enabled = app_visible_ && on_battery_power_;

  if (should_be_enabled && !metrics_timer_.IsRunning()) {
    // Capture first capacity measurement and enable the repeating timer.
    last_remaining_capacity_uah_ =
        base::PowerMonitor::GetRemainingBatteryCapacity();
    if (!net::android::traffic_stats::GetTotalTxBytes(&last_tx_bytes_))
      last_tx_bytes_ = -1;
    if (!net::android::traffic_stats::GetTotalRxBytes(&last_rx_bytes_))
      last_rx_bytes_ = -1;
    skipped_timers_ = 0;
    observed_capacity_drops_ = 0;

    metrics_timer_.Start(
        FROM_HERE, kMetricsInterval,
        base::BindRepeating(&AndroidBatteryMetrics::CaptureAndReportMetrics,
                            base::Unretained(this),
                            /*disabling=*/false));
    if (base::FeatureList::IsEnabled(kForegroundRadioStateCountWakeups)) {
      radio_state_timer_.Start(FROM_HERE, kRadioStateInterval, this,
                               &AndroidBatteryMetrics::MonitorRadioState);
    }
  } else if (!should_be_enabled && metrics_timer_.IsRunning()) {
    // Capture one last measurement before disabling the timer.
    CaptureAndReportMetrics(/*disabling=*/true);
    metrics_timer_.Stop();
    if (base::FeatureList::IsEnabled(kForegroundRadioStateCountWakeups)) {
      radio_state_timer_.Stop();
    }
  }
}

void AndroidBatteryMetrics::MonitorRadioState() {
  auto maybe_activity = base::android::RadioUtils::GetCellDataActivity();
  if (!maybe_activity.has_value())
    return;

  if (last_activity_ == base::android::RadioDataActivity::kDormant &&
      *maybe_activity != base::android::RadioDataActivity::kDormant) {
    TRACE_EVENT_INSTANT0("power", "RadioWakeup", TRACE_EVENT_SCOPE_GLOBAL);
    ++radio_wakeups_;
  }
  if (last_activity_ != base::android::RadioDataActivity::kDormant &&
      *maybe_activity == base::android::RadioDataActivity::kDormant) {
    TRACE_EVENT_INSTANT0("power", "RadioDormant", TRACE_EVENT_SCOPE_GLOBAL);
  }
  last_activity_ = *maybe_activity;
}

void AndroidBatteryMetrics::UpdateAndReportRadio() {
  int64_t tx_bytes;
  int64_t rx_bytes;
  if (!net::android::traffic_stats::GetTotalTxBytes(&tx_bytes))
    tx_bytes = -1;
  if (!net::android::traffic_stats::GetTotalRxBytes(&rx_bytes))
    rx_bytes = -1;

  if (last_tx_bytes_ > 0 && last_rx_bytes_ > 0 && tx_bytes >= last_tx_bytes_ &&
      rx_bytes >= last_rx_bytes_) {
    Report30SecondRadioUsage(tx_bytes - last_tx_bytes_,
                             rx_bytes - last_rx_bytes_, radio_wakeups_);
  }
  last_tx_bytes_ = tx_bytes;
  last_rx_bytes_ = rx_bytes;
  radio_wakeups_ = 0;
}

void AndroidBatteryMetrics::CaptureAndReportMetrics(bool disabling) {
  int remaining_capacity_uah =
      base::PowerMonitor::GetRemainingBatteryCapacity();

  if (remaining_capacity_uah >= last_remaining_capacity_uah_) {
    // No change in battery capacity, or it increased. The latter could happen
    // if we detected the switch off battery power to a charger late, or if the
    // device reports bogus values. We don't change last_remaining_capacity_uah_
    // here to avoid overreporting in case of fluctuating values.
    skipped_timers_++;
    Report30SecondDrain(0, IsMeasuringDrainExclusively());
    UpdateAndReportRadio();

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
  Report30SecondDrain(capacity_consumed, IsMeasuringDrainExclusively());
  UpdateAndReportRadio();

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
  static constexpr base::Histogram::Sample kSampleFactor = 100;
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

}  // namespace content
