// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/metrics_private/metrics_private_api.h"

#include <limits.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/hash/hash.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/metrics_hashes.h"
#include "base/metrics/statistics_recorder.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "content/public/browser/histogram_fetcher.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/metrics_private/metrics_private_delegate.h"
#include "extensions/common/api/metrics_private.h"

namespace extensions {

namespace GetVariationParams = api::metrics_private::GetVariationParams;
namespace RecordUserAction = api::metrics_private::RecordUserAction;
namespace RecordValue = api::metrics_private::RecordValue;
namespace RecordBoolean = api::metrics_private::RecordBoolean;
namespace RecordEnumerationValue = api::metrics_private::RecordEnumerationValue;
namespace RecordSparseValueWithHashMetricName =
    api::metrics_private::RecordSparseValueWithHashMetricName;
namespace RecordSparseValueWithPersistentHash =
    api::metrics_private::RecordSparseValueWithPersistentHash;
namespace RecordSparseValue = api::metrics_private::RecordSparseValue;
namespace RecordPercentage = api::metrics_private::RecordPercentage;
namespace RecordCount = api::metrics_private::RecordCount;
namespace RecordSmallCount = api::metrics_private::RecordSmallCount;
namespace RecordMediumCount = api::metrics_private::RecordMediumCount;
namespace RecordTime = api::metrics_private::RecordTime;
namespace RecordMediumTime = api::metrics_private::RecordMediumTime;
namespace RecordLongTime = api::metrics_private::RecordLongTime;

namespace {

const size_t kMaxBuckets = 10000;  // We don't ever want more than these many
                                   // buckets; there is no real need for them
                                   // and would cause crazy memory usage

// Amount of time to give other processes to report their histograms.
constexpr base::TimeDelta kHistogramsRefreshTimeout = base::Seconds(10);

}  // namespace

ExtensionFunction::ResponseAction
MetricsPrivateGetIsCrashReportingEnabledFunction::Run() {
  MetricsPrivateDelegate* delegate =
      ExtensionsAPIClient::Get()->GetMetricsPrivateDelegate();

  return RespondNow(
      WithArguments(delegate && delegate->IsCrashReportingEnabled()));
}

ExtensionFunction::ResponseAction MetricsPrivateGetFieldTrialFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 1);
  EXTENSION_FUNCTION_VALIDATE(args()[0].is_string());
  const std::string& name = args()[0].GetString();

  return RespondNow(WithArguments(base::FieldTrialList::FindFullName(name)));
}

ExtensionFunction::ResponseAction
MetricsPrivateGetVariationParamsFunction::Run() {
  std::optional<GetVariationParams::Params> params =
      GetVariationParams::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  GetVariationParams::Results::Params result;
  if (base::GetFieldTrialParams(params->name, &result.additional_properties)) {
    return RespondNow(WithArguments(result.ToValue()));
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
MetricsPrivateRecordUserActionFunction::Run() {
  std::optional<RecordUserAction::Params> params =
      RecordUserAction::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  base::RecordComputedAction(params->name);
  return RespondNow(NoArguments());
}

void MetricsHistogramHelperFunction::RecordValue(const std::string& name,
                                                 base::HistogramType type,
                                                 int min,
                                                 int max,
                                                 size_t buckets,
                                                 int sample) {
  // Make sure toxic values don't get to internal code.
  // Fix for maximums
  min = std::min(min, INT_MAX - 3);
  max = std::min(max, INT_MAX - 3);
  buckets = std::min(buckets, kMaxBuckets);
  // Fix for minimums.
  min = std::max(min, 1);
  max = std::max(max, min + 1);
  buckets = std::max(buckets, static_cast<size_t>(3));
  // Trim buckets down to a maximum of the given range + over/underflow buckets
  if (buckets > static_cast<size_t>(max - min + 2)) {
    buckets = max - min + 2;
  }

  base::HistogramBase* counter;
  if (type == base::LINEAR_HISTOGRAM) {
    counter = base::LinearHistogram::FactoryGet(
        name, min, max, buckets,
        base::HistogramBase::kUmaTargetedHistogramFlag);
  } else {
    counter = base::Histogram::FactoryGet(
        name, min, max, buckets,
        base::HistogramBase::kUmaTargetedHistogramFlag);
  }

  // The histogram can be NULL if it is constructed with bad arguments.  Ignore
  // that data for this API.  An error message will be logged.
  if (counter) {
    counter->Add(sample);
  }
}

ExtensionFunction::ResponseAction MetricsPrivateRecordValueFunction::Run() {
  std::optional<RecordValue::Params> params =
      RecordValue::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  // Get the histogram parameters from the metric type object.
  std::string type = api::metrics_private::ToString(params->metric.type);

  base::HistogramType histogram_type(
      type == "histogram-linear" ? base::LINEAR_HISTOGRAM : base::HISTOGRAM);
  RecordValue(params->metric.metric_name, histogram_type, params->metric.min,
              params->metric.max, params->metric.buckets, params->value);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
MetricsPrivateRecordSparseValueWithHashMetricNameFunction::Run() {
  auto params = RecordSparseValueWithHashMetricName::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  base::UmaHistogramSparse(params->metric_name,
                           base::HashMetricName(params->value));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
MetricsPrivateRecordSparseValueWithPersistentHashFunction::Run() {
  auto params = RecordSparseValueWithPersistentHash::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  base::UmaHistogramSparse(params->metric_name,
                           base::PersistentHash(params->value));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
MetricsPrivateRecordSparseValueFunction::Run() {
  std::optional<RecordSparseValue::Params> params =
      RecordSparseValue::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  base::UmaHistogramSparse(params->metric_name, params->value);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction MetricsPrivateRecordBooleanFunction::Run() {
  std::optional<RecordBoolean::Params> params =
      RecordBoolean::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  base::UmaHistogramBoolean(params->metric_name, params->value);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
MetricsPrivateRecordEnumerationValueFunction::Run() {
  std::optional<RecordEnumerationValue::Params> params =
      RecordEnumerationValue::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  // Uses UmaHistogramExactLinear instead of UmaHistogramEnumeration
  // because we don't have an enum type on params->value.
  base::UmaHistogramExactLinear(params->metric_name, params->value,
                                params->enum_size);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
MetricsPrivateRecordPercentageFunction::Run() {
  std::optional<RecordPercentage::Params> params =
      RecordPercentage::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  RecordValue(params->metric_name, base::LINEAR_HISTOGRAM, 1, 101, 102,
              params->value);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction MetricsPrivateRecordCountFunction::Run() {
  std::optional<RecordCount::Params> params =
      RecordCount::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  RecordValue(params->metric_name, base::HISTOGRAM, 1, 1000000, 50,
              params->value);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
MetricsPrivateRecordSmallCountFunction::Run() {
  std::optional<RecordSmallCount::Params> params =
      RecordSmallCount::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  RecordValue(params->metric_name, base::HISTOGRAM, 1, 100, 50, params->value);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
MetricsPrivateRecordMediumCountFunction::Run() {
  std::optional<RecordMediumCount::Params> params =
      RecordMediumCount::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  RecordValue(params->metric_name, base::HISTOGRAM, 1, 10000, 50,
              params->value);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction MetricsPrivateRecordTimeFunction::Run() {
  std::optional<RecordTime::Params> params = RecordTime::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  static const int kTenSecMs = 10 * 1000;
  RecordValue(params->metric_name, base::HISTOGRAM, 1, kTenSecMs, 50,
              params->value);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
MetricsPrivateRecordMediumTimeFunction::Run() {
  std::optional<RecordMediumTime::Params> params =
      RecordMediumTime::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  static const int kThreeMinMs = 3 * 60 * 1000;
  RecordValue(params->metric_name, base::HISTOGRAM, 1, kThreeMinMs, 50,
              params->value);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction MetricsPrivateRecordLongTimeFunction::Run() {
  std::optional<RecordLongTime::Params> params =
      RecordLongTime::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  static const int kOneHourMs = 60 * 60 * 1000;
  RecordValue(params->metric_name, base::HISTOGRAM, 1, kOneHourMs, 50,
              params->value);
  return RespondNow(NoArguments());
}

MetricsPrivateGetHistogramFunction::~MetricsPrivateGetHistogramFunction() =
    default;

ExtensionFunction::ResponseAction MetricsPrivateGetHistogramFunction::Run() {
  std::optional<api::metrics_private::GetHistogram::Params> params =
      api::metrics_private::GetHistogram::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  // Collect histogram data from other processes before responding. Otherwise,
  // we'd report stale data for histograms that are e.g. recorded by renderers.
  content::FetchHistogramsAsynchronously(
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      base::BindOnce(
          &MetricsPrivateGetHistogramFunction::RespondOnHistogramsFetched, this,
          params->name),
      kHistogramsRefreshTimeout);
  return RespondLater();
}

void MetricsPrivateGetHistogramFunction::RespondOnHistogramsFetched(
    const std::string& name) {
  // Incorporate the data collected by content::FetchHistogramsAsynchronously().
  base::StatisticsRecorder::ImportProvidedHistogramsSync();
  Respond(GetHistogram(name));
}

ExtensionFunction::ResponseValue
MetricsPrivateGetHistogramFunction::GetHistogram(const std::string& name) {
  const base::HistogramBase* histogram =
      base::StatisticsRecorder::FindHistogram(name);
  if (!histogram) {
    return Error(base::StrCat({"Histogram ", name, " not found"}));
  }

  std::unique_ptr<base::HistogramSamples> samples =
      histogram->SnapshotSamples();
  api::metrics_private::Histogram result;
  result.sum = samples->sum();

  for (std::unique_ptr<base::SampleCountIterator> it = samples->Iterator();
       !it->Done(); it->Next()) {
    base::HistogramBase::Sample min = 0;
    int64_t max = 0;
    base::HistogramBase::Count count = 0;
    it->Get(&min, &max, &count);

    api::metrics_private::HistogramBucket bucket;
    bucket.min = min;
    bucket.max = max;
    bucket.count = count;
    result.buckets.push_back(std::move(bucket));
  }

  return WithArguments(result.ToValue());
}

}  // namespace extensions
