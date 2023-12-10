// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_TEST_CONFIGURABLE_STORAGE_DELEGATE_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_TEST_CONFIGURABLE_STORAGE_DELEGATE_H_

#include <vector>

#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_config.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_storage_delegate.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

class ConfigurableStorageDelegate : public AttributionStorageDelegate {
 public:
  ConfigurableStorageDelegate();
  ~ConfigurableStorageDelegate() override;

  // AttributionStorageDelegate:
  base::Time GetEventLevelReportTime(
      const attribution_reporting::EventReportWindows& event_report_windows,
      base::Time source_time,
      base::Time trigger_time) const override;
  base::Time GetAggregatableReportTime(base::Time trigger_time) const override;
  base::TimeDelta GetDeleteExpiredSourcesFrequency() const override;
  base::TimeDelta GetDeleteExpiredRateLimitsFrequency() const override;
  base::Uuid NewReportID() const override;
  absl::optional<OfflineReportDelayConfig> GetOfflineReportDelayConfig()
      const override;
  void ShuffleReports(std::vector<AttributionReport>&) override;
  void ShuffleTriggerVerifications(
      std::vector<network::TriggerVerification>&) override;
  double GetRandomizedResponseRate(
      const attribution_reporting::TriggerSpecs&,
      attribution_reporting::MaxEventLevelReports,
      attribution_reporting::EventLevelEpsilon) const override;
  GetRandomizedResponseResult GetRandomizedResponse(
      attribution_reporting::mojom::SourceType,
      const attribution_reporting::TriggerSpecs&,
      attribution_reporting::MaxEventLevelReports,
      attribution_reporting::EventLevelEpsilon,
      base::Time source_time) const override;
  std::vector<NullAggregatableReport> GetNullAggregatableReports(
      const AttributionTrigger&,
      base::Time trigger_time,
      absl::optional<base::Time> attributed_source_time) const override;

  void set_max_sources_per_origin(int max);

  void set_max_reports_per_destination(AttributionReport::Type, int max);

  void set_max_destinations_per_source_site_reporting_site(int max);

  void set_rate_limits(AttributionConfig::RateLimitConfig);

  void set_destination_rate_limit(AttributionConfig::DestinationRateLimit);

  void set_delete_expired_sources_frequency(base::TimeDelta frequency);

  void set_delete_expired_rate_limits_frequency(base::TimeDelta frequency);

  void set_report_delay(base::TimeDelta report_delay);

  void set_offline_report_delay_config(
      absl::optional<OfflineReportDelayConfig>);

  void set_reverse_reports_on_shuffle(bool reverse);

  void set_reverse_verifications_on_shuffle(bool reverse);

  // Note that this is *not* used to produce a randomized response; that
  // is controlled deterministically by `set_randomized_response()`.
  void set_randomized_response_rate(double rate);

  void set_randomized_response(RandomizedResponse);
  void set_exceeds_channel_capacity_limit(bool);

  void set_null_aggregatable_reports(std::vector<NullAggregatableReport>);

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

  absl::optional<OfflineReportDelayConfig> offline_report_delay_config_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // If true, `ShuffleReports()` reverses the reports to allow testing the
  // proper call from `AttributionStorage::GetAttributionReports()`.
  bool reverse_reports_on_shuffle_ GUARDED_BY_CONTEXT(sequence_checker_) =
      false;

  // If true, `ShuffleTriggerVerifications()` reverses the verifications.
  bool reverse_verifications_on_shuffle_ GUARDED_BY_CONTEXT(sequence_checker_) =
      false;

  double randomized_response_rate_ GUARDED_BY_CONTEXT(sequence_checker_) = 0.0;

  RandomizedResponse randomized_response_ GUARDED_BY_CONTEXT(sequence_checker_);

  bool exceeds_channel_capacity_limit_ GUARDED_BY_CONTEXT(sequence_checker_) =
      false;

  std::vector<NullAggregatableReport> null_aggregatable_reports_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_TEST_CONFIGURABLE_STORAGE_DELEGATE_H_
