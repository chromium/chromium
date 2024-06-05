// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_resolver_impl.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "components/attribution_reporting/aggregatable_utils.h"
#include "content/browser/attribution_reporting/aggregatable_debug_rate_limit_table.h"
#include "content/browser/attribution_reporting/aggregatable_debug_report.h"
#include "content/browser/attribution_reporting/attribution_config.h"
#include "content/browser/attribution_reporting/attribution_resolver_delegate.h"
#include "content/browser/attribution_reporting/attribution_storage_sql.h"
#include "content/browser/attribution_reporting/create_report_result.h"
#include "content/browser/attribution_reporting/process_aggregatable_debug_report_result.mojom.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/store_source_result.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

namespace {
using ProcessAggregatableDebugReportStatus =
    ::attribution_reporting::mojom::ProcessAggregatableDebugReportResult;
}  // namespace

AttributionResolverImpl::AttributionResolverImpl(
    const base::FilePath& user_data_directory,
    std::unique_ptr<AttributionResolverDelegate> delegate)
    : delegate_(std::move(delegate)),
      storage_(user_data_directory, delegate_.get()) {
  DCHECK(delegate_);
}

AttributionResolverImpl::~AttributionResolverImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

StoreSourceResult AttributionResolverImpl::StoreSource(StorableSource source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return storage_.StoreSource(std::move(source));
}

CreateReportResult AttributionResolverImpl::MaybeCreateAndStoreReport(
    AttributionTrigger trigger) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return storage_.MaybeCreateAndStoreReport(std::move(trigger));
}

std::vector<AttributionReport> AttributionResolverImpl::GetAttributionReports(
    base::Time max_report_time,
    int limit) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return storage_.GetAttributionReports(max_report_time, limit);
}

std::optional<base::Time> AttributionResolverImpl::GetNextReportTime(
    base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return storage_.GetNextReportTime(time);
}

std::optional<AttributionReport> AttributionResolverImpl::GetReport(
    AttributionReport::Id id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return storage_.GetReport(id);
}

std::vector<StoredSource> AttributionResolverImpl::GetActiveSources(int limit) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return storage_.GetActiveSources(limit);
}

std::set<AttributionDataModel::DataKey>
AttributionResolverImpl::GetAllDataKeys() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return storage_.GetAllDataKeys();
}

void AttributionResolverImpl::DeleteByDataKey(
    const AttributionDataModel::DataKey& datakey) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ClearData(base::Time::Min(), base::Time::Max(),
            base::BindRepeating(std::equal_to<blink::StorageKey>(),
                                blink::StorageKey::CreateFirstParty(
                                    datakey.reporting_origin())),
            /*delete_rate_limit_data=*/true);
}

bool AttributionResolverImpl::DeleteReport(AttributionReport::Id report_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return storage_.DeleteReport(report_id);
}

bool AttributionResolverImpl::UpdateReportForSendFailure(
    AttributionReport::Id report_id,
    base::Time new_report_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return storage_.UpdateReportForSendFailure(report_id, new_report_time);
}

std::optional<base::Time> AttributionResolverImpl::AdjustOfflineReportTimes() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return storage_.AdjustOfflineReportTimes();
}

void AttributionResolverImpl::ClearData(
    base::Time delete_begin,
    base::Time delete_end,
    StoragePartition::StorageKeyMatcherFunction filter,
    bool delete_rate_limit_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SCOPED_UMA_HISTOGRAM_TIMER("Conversions.ClearDataTime");

  if (filter.is_null() && (delete_begin.is_null() || delete_begin.is_min()) &&
      delete_end.is_max()) {
    storage_.ClearAllDataAllTime(delete_rate_limit_data);
    return;
  }

  // Measure the time it takes to perform a clear with a filter separately from
  // the above histogram.
  SCOPED_UMA_HISTOGRAM_TIMER("Conversions.Storage.ClearDataWithFilterDuration");
  storage_.ClearDataWithFilter(delete_begin, delete_end, std::move(filter),
                               delete_rate_limit_data);
}

ProcessAggregatableDebugReportResult
AttributionResolverImpl::ProcessAggregatableDebugReport(
    AggregatableDebugReport report,
    std::optional<int> remaining_budget,
    std::optional<StoredSource::Id> source_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto make_result = [&](ProcessAggregatableDebugReportStatus result) {
    switch (result) {
      case ProcessAggregatableDebugReportStatus::kSuccess:
        break;
      case ProcessAggregatableDebugReportStatus::kNoDebugData:
      case ProcessAggregatableDebugReportStatus::kInsufficientBudget:
      case ProcessAggregatableDebugReportStatus::kExcessiveReports:
      case ProcessAggregatableDebugReportStatus::kGlobalRateLimitReached:
      case ProcessAggregatableDebugReportStatus::kReportingSiteRateLimitReached:
      case ProcessAggregatableDebugReportStatus::kBothRateLimitsReached:
      case ProcessAggregatableDebugReportStatus::kInternalError:
        report.ToNull();
        break;
    }

    base::UmaHistogramEnumeration(
        "Conversions.AggregatableDebugReport.ProcessResult", result);

    return ProcessAggregatableDebugReportResult(std::move(report), result);
  };

  report.set_report_id(delegate_->NewReportID());

  if (report.contributions().empty()) {
    return make_result(ProcessAggregatableDebugReportStatus::kNoDebugData);
  }

  int num_reports = 0;

  if (source_id.has_value()) {
    std::optional<AttributionStorageSql::AggregatableDebugSourceData>
        source_data = storage_.GetAggregatableDebugSourceData(*source_id);
    if (!source_data.has_value() ||
        !attribution_reporting::IsRemainingAggregatableBudgetInRange(
            source_data->remaining_budget) ||
        source_data->num_reports < 0) {
      return make_result(ProcessAggregatableDebugReportStatus::kInternalError);
    }

    if (remaining_budget.has_value()) {
      // Source aggregatable debug report should be the first aggregatable debug
      // report created for this source.
      if (source_data->remaining_budget != remaining_budget ||
          source_data->num_reports != num_reports) {
        return make_result(
            ProcessAggregatableDebugReportStatus::kInternalError);
      }
    }

    remaining_budget = source_data->remaining_budget;
    num_reports = source_data->num_reports;
  }

  // `remaining_budget` is `std::nullopt` for `kTriggerNoMatchingSource` debug
  // report. In this case, the total budget is required to not exceed the
  // maximum budget per source.
  int effective_remaining_budget =
      remaining_budget.value_or(attribution_reporting::kMaxAggregatableValue);
  CHECK(attribution_reporting::IsRemainingAggregatableBudgetInRange(
      effective_remaining_budget));
  if (report.BudgetRequired() > effective_remaining_budget) {
    return make_result(
        ProcessAggregatableDebugReportStatus::kInsufficientBudget);
  }

  int max_reports_per_source =
      delegate_->GetAggregatableDebugRateLimit().max_reports_per_source;
  CHECK_GT(max_reports_per_source, 0);

  if (num_reports >= max_reports_per_source) {
    return make_result(ProcessAggregatableDebugReportStatus::kExcessiveReports);
  }

  switch (storage_.AggregatableDebugReportAllowedForRateLimit(report)) {
    case AggregatableDebugRateLimitTable::Result::kAllowed:
      break;
    case AggregatableDebugRateLimitTable::Result::kHitGlobalLimit:
      return make_result(
          ProcessAggregatableDebugReportStatus::kGlobalRateLimitReached);
    case AggregatableDebugRateLimitTable::Result::kHitReportingLimit:
      return make_result(
          ProcessAggregatableDebugReportStatus::kReportingSiteRateLimitReached);
    case AggregatableDebugRateLimitTable::Result::kHitBothLimits:
      return make_result(
          ProcessAggregatableDebugReportStatus::kBothRateLimitsReached);
    case AggregatableDebugRateLimitTable::Result::kError:
      return make_result(ProcessAggregatableDebugReportStatus::kInternalError);
  }

  if (!storage_.AdjustForAggregatableDebugReport(report, source_id)) {
    return make_result(ProcessAggregatableDebugReportStatus::kInternalError);
  }

  return make_result(ProcessAggregatableDebugReportStatus::kSuccess);
}

void AttributionResolverImpl::SetDelegate(
    std::unique_ptr<AttributionResolverDelegate> delegate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(delegate);
  storage_.SetDelegate(delegate.get());
  delegate_ = std::move(delegate);
}

}  // namespace content
