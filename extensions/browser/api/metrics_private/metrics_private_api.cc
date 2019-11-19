// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/metrics_private/metrics_private_api.h"

#include <limits.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/hash/hash.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "components/variations/variations_associated_data.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/metrics_private/metrics_private_delegate.h"
#include "extensions/common/api/metrics_private.h"

namespace extensions {

namespace GetVariationParams = api::metrics_private::GetVariationParams;
namespace RecordUserAction = api::metrics_private::RecordUserAction;
namespace RecordValue = api::metrics_private::RecordValue;
namespace RecordBoolean = api::metrics_private::RecordBoolean;
namespace RecordEnumerationValue = api::metrics_private::RecordEnumerationValue;
namespace RecordSparseHashable = api::metrics_private::RecordSparseHashable;
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
}  // namespace

ExtensionFunction::ResponseAction
MetricsPrivateGetIsCrashReportingEnabledFunction::Run() {
  MetricsPrivateDelegate* delegate =
      ExtensionsAPIClient::Get()->GetMetricsPrivateDelegate();

  return RespondNow(OneArgument(std::make_unique<base::Value>(
      delegate && delegate->IsCrashReportingEnabled())));
}

ExtensionFunction::ResponseAction MetricsPrivateGetFieldTrialFunction::Run() {
  std::string name;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &name));

  return RespondNow(OneArgument(
      std::make_unique<base::Value>(base::FieldTrialList::FindFullName(name))));
}

ExtensionFunction::ResponseAction
MetricsPrivateGetVariationParamsFunction::Run() {
  std::unique_ptr<GetVariationParams::Params> params(
      GetVariationParams::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  GetVariationParams::Results::Params result;
  std::unique_ptr<base::DictionaryValue> dict;
  if (variations::GetVariationParams(params->name,
                                     &result.additional_properties)) {
    dict = result.ToValue();
  }
  return RespondNow(dict ? OneArgument(std::move(dict)) : NoArguments());
}

ExtensionFunction::ResponseAction
MetricsPrivateRecordUserActionFunction::Run() {
  std::unique_ptr<RecordUserAction::Params> params(
      RecordUserAction::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

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
  if (buckets > static_cast<size_t>(max - min + 2))
    buckets = max - min + 2;

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
  if (counter)
    counter->Add(sample);
}

ExtensionFunction::ResponseAction MetricsPrivateRecordValueFunction::Run() {
  std::unique_ptr<RecordValue::Params> params(
      RecordValue::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // Get the histogram parameters from the metric type object.
  std::string type = api::metrics_private::ToString(params->metric.type);

  base::HistogramType histogram_type(
      type == "histogram-linear" ? base::LINEAR_HISTOGRAM : base::HISTOGRAM);
  RecordValue(params->metric.metric_name, histogram_type, params->metric.min,
              params->metric.max, params->metric.buckets, params->value);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
MetricsPrivateRecordSparseHashableFunction::Run() {
  auto params = RecordSparseHashable::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  base::UmaHistogramSparse(params->metric_name, base::Hash(params->value));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
MetricsPrivateRecordSparseValueFunction::Run() {
  std::unique_ptr<RecordSparseValue::Params> params(
      RecordSparseValue::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  base::UmaHistogramSparse(params->metric_name, params->value);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction MetricsPrivateRecordBooleanFunction::Run() {
  std::unique_ptr<RecordBoolean::Params> params(
      RecordBoolean::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  base::UmaHistogramBoolean(params->metric_name, params->value);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
MetricsPrivateRecordEnumerationValueFunction::Run() {
  std::unique_ptr<RecordEnumerationValue::Params> params(
      RecordEnumerationValue::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  // Uses UmaHistogramExactLinear instead of UmaHistogramEnumeration
  // because we don't have an enum type on params->value.
  base::UmaHistogramExactLinear(params->metric_name, params->value,
                                params->enum_size);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
MetricsPrivateRecordPercentageFunction::Run() {
  std::unique_ptr<RecordPercentage::Params> params(
      RecordPercentage::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  RecordValue(params->metric_name, base::LINEAR_HISTOGRAM, 1, 101, 102,
              params->value);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction MetricsPrivateRecordCountFunction::Run() {
  std::unique_ptr<RecordCount::Params> params(
      RecordCount::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  RecordValue(params->metric_name, base::HISTOGRAM, 1, 1000000, 50,
              params->value);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
MetricsPrivateRecordSmallCountFunction::Run() {
  std::unique_ptr<RecordSmallCount::Params> params(
      RecordSmallCount::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  RecordValue(params->metric_name, base::HISTOGRAM, 1, 100, 50, params->value);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
MetricsPrivateRecordMediumCountFunction::Run() {
  std::unique_ptr<RecordMediumCount::Params> params(
      RecordMediumCount::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  RecordValue(params->metric_name, base::HISTOGRAM, 1, 10000, 50,
              params->value);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction MetricsPrivateRecordTimeFunction::Run() {
  std::unique_ptr<RecordTime::Params> params(
      RecordTime::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  static const int kTenSecMs = 10 * 1000;
  RecordValue(params->metric_name, base::HISTOGRAM, 1, kTenSecMs, 50,
              params->value);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
MetricsPrivateRecordMediumTimeFunction::Run() {
  std::unique_ptr<RecordMediumTime::Params> params(
      RecordMediumTime::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  static const int kThreeMinMs = 3 * 60 * 1000;
  RecordValue(params->metric_name, base::HISTOGRAM, 1, kThreeMinMs, 50,
              params->value);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction MetricsPrivateRecordLongTimeFunction::Run() {
  std::unique_ptr<RecordLongTime::Params> params(
      RecordLongTime::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  static const int kOneHourMs = 60 * 60 * 1000;
  RecordValue(params->metric_name, base::HISTOGRAM, 1, kOneHourMs, 50,
              params->value);
  return RespondNow(NoArguments());
}

}  // namespace extensions
