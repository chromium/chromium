// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_TEST_CONFIGURABLE_STORAGE_DELEGATE_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_TEST_CONFIGURABLE_STORAGE_DELEGATE_H_

#include <optional>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "components/attribution_reporting/privacy_math.h"
#include "content/browser/attribution_reporting/attribution_config.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_resolver_delegate.h"

namespace attribution_reporting {
class AttributionScopesData;
}

namespace content {

class ConfigurableStorageDelegate : public AttributionResolverDelegate {
 public:
  ConfigurableStorageDelegate();
  ~ConfigurableStorageDelegate() override;

  // AttributionResolverDelegate:
  base::Time GetEventLevelReportTime(
      const attribution_reporting::EventReportWindows& event_report_windows,
      base::Time source_time,
      base::Time trigger_time) const override;
  base::Time GetAggregatableReportTime(base::Time trigger_time) const override;
  base::TimeDelta GetDeleteExpiredSourcesFrequency() const override;
  base::TimeDelta GetDeleteExpiredRateLimitsFrequency() const override;
  base::Uuid NewReportID() const override;
  std::optional<OfflineReportDelayConfig> GetOfflineReportDelayConfig()
      const override;
  void ShuffleReports(std::vector<AttributionReport>&) override;
  std::optional<double> GetRandomizedResponseRate(
      const attribution_reporting::TriggerSpecs&,
      attribution_reporting::EventLevelEpsilon) const override;
  GetRandomizedResponseResult GetRandomizedResponse(
      attribution_reporting::mojom::SourceType,
      const attribution_reporting::TriggerSpecs&,
      attribution_reporting::EventLevelEpsilon,
      const std::optional<attribution_reporting::AttributionScopesData>&)
      override;
  bool GenerateNullAggregatableReportForLookbackDay(
      int lookback_day,
      attribution_reporting::mojom::SourceRegistrationTimeConfig)
      const override;

  void set_max_sources_per_origin(int max);

  void set_max_reports_per_destination(AttributionReport::Type, int max);

  void set_max_destinations_per_source_site_reporting_site(int max);

  void set_rate_limits(AttributionConfig::RateLimitConfig);

  void set_destination_rate_limit(AttributionConfig::DestinationRateLimit);

  void set_aggregatable_debug_rate_limit(
      AttributionConfig::AggregatableDebugRateLimit);

  void set_delete_expired_sources_frequency(base::TimeDelta frequency);

  void set_delete_expired_rate_limits_frequency(base::TimeDelta frequency);

  void set_report_delay(base::TimeDelta report_delay);

  void set_offline_report_delay_config(std::optional<OfflineReportDelayConfig>);

  void set_reverse_reports_on_shuffle(bool reverse);

  // Note that this is *not* used to produce a randomized response; that
  // is controlled deterministically by `set_randomized_response()`.
  void set_randomized_response_rate(double rate);

  void set_randomized_response(attribution_reporting::RandomizedResponse);
  void set_exceeds_channel_capacity_limit(bool);

  void set_null_aggregatable_reports_lookback_days(
      base::flat_set<int> null_aggregatable_reports_lookback_days);

  void use_realistic_report_times();

  // Detaches the delegate from its current sequence in preparation for being
  // moved to storage, which runs on its own sequence.
  void DetachFromSequence();

 private:
  base::TimeDelta delete_expired_sources_frequency_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::TimeDelta delete_expired_rate_limits_frequency_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::TimeDelta report_delay_ GUARDED_BY_CONTEXT(sequence_checker_);

  bool use_realistic_report_times_ GUARDED_BY_CONTEXT(sequence_checker_) =
      false;

  std::optional<OfflineReportDelayConfig> offline_report_delay_config_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // If true, `ShuffleReports()` reverses the reports to allow testing the
  // proper call from `AttributionStorage::GetAttributionReports()`.
  bool reverse_reports_on_shuffle_ GUARDED_BY_CONTEXT(sequence_checker_) =
      false;

  double randomized_response_rate_ GUARDED_BY_CONTEXT(sequence_checker_) = 0.0;

  attribution_reporting::RandomizedResponse randomized_response_
      GUARDED_BY_CONTEXT(sequence_checker_);

  bool exceeds_channel_capacity_limit_ GUARDED_BY_CONTEXT(sequence_checker_) =
      false;

  base::flat_set<int> null_aggregatable_reports_lookback_days_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_TEST_CONFIGURABLE_STORAGE_DELEGATE_H_
