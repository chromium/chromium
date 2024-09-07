// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_RESOLVER_DELEGATE_IMPL_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_RESOLVER_DELEGATE_IMPL_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/thread_annotations.h"
#include "components/attribution_reporting/source_type.mojom-forward.h"
#include "content/browser/attribution_reporting/attribution_resolver_delegate.h"
#include "content/common/content_export.h"

namespace attribution_reporting {
class AttributionScopesData;
}

namespace base {
class Time;
class TimeDelta;
}  // namespace base

namespace content {

struct AttributionConfig;

enum class AttributionNoiseMode {
  // Various aspects of the API are subject to noise:
  // - Sources are subject to randomized response
  // - Reports within a reporting window are shuffled
  // - Pending reports are randomly delayed when the browser comes online
  kDefault,
  // None of the above applies.
  kNone,
};

enum class AttributionDelayMode {
  // Reports are sent in reporting windows some time after attribution is
  // triggered.
  kDefault,
  // Reports are sent immediately after attribution is triggered.
  kNone,
};

// Implementation of the storage delegate. This class handles assigning
// report times to newly created reports. It
// also controls constants for AttributionResolver. This is owned by
// AttributionStorageSql, and should only be accessed on the attribution storage
// task runner.
class CONTENT_EXPORT AttributionResolverDelegateImpl
    : public AttributionResolverDelegate {
 public:
  static std::unique_ptr<AttributionResolverDelegate> CreateForTesting(
      AttributionNoiseMode noise_mode,
      AttributionDelayMode delay_mode,
      const AttributionConfig& config);

  explicit AttributionResolverDelegateImpl(
      AttributionNoiseMode noise_mode = AttributionNoiseMode::kDefault,
      AttributionDelayMode delay_mode = AttributionDelayMode::kDefault);
  AttributionResolverDelegateImpl(const AttributionResolverDelegateImpl&) =
      delete;
  AttributionResolverDelegateImpl& operator=(
      const AttributionResolverDelegateImpl&) = delete;
  AttributionResolverDelegateImpl(AttributionResolverDelegateImpl&&) = delete;
  AttributionResolverDelegateImpl& operator=(AttributionResolverDelegateImpl&&) =
      delete;
  ~AttributionResolverDelegateImpl() override;

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
  void ShuffleReports(std::vector<AttributionReport>& reports) override;
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

 protected:
  AttributionResolverDelegateImpl(AttributionNoiseMode noise_mode,
                                 AttributionDelayMode delay_mode,
                                 const AttributionConfig& config);

 private:
  const AttributionNoiseMode noise_mode_ GUARDED_BY_CONTEXT(sequence_checker_);
  const AttributionDelayMode delay_mode_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_RESOLVER_DELEGATE_IMPL_H_
