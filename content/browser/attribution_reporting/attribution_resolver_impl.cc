// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_resolver_impl.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/overloaded.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "components/attribution_reporting/aggregatable_utils.h"
#include "components/attribution_reporting/event_report_windows.h"
#include "components/attribution_reporting/features.h"
#include "components/attribution_reporting/source_registration.h"
#include "components/attribution_reporting/trigger_config.h"
#include "components/attribution_reporting/trigger_data_matching.mojom.h"
#include "content/browser/attribution_reporting/aggregatable_debug_rate_limit_table.h"
#include "content/browser/attribution_reporting/aggregatable_debug_report.h"
#include "content/browser/attribution_reporting/attribution_config.h"
#include "content/browser/attribution_reporting/attribution_info.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_resolver_delegate.h"
#include "content/browser/attribution_reporting/attribution_storage_sql.h"
#include "content/browser/attribution_reporting/create_report_result.h"
#include "content/browser/attribution_reporting/process_aggregatable_debug_report_result.mojom.h"
#include "content/browser/attribution_reporting/rate_limit_result.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/store_source_result.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

namespace {
using ProcessAggregatableDebugReportStatus =
    ::attribution_reporting::mojom::ProcessAggregatableDebugReportResult;
using ::attribution_reporting::mojom::TriggerDataMatching;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(DestinationLimitResult)
enum class DestinationLimitResult {
  // Destinations allowed without hitting the limit.
  kAllowed = 0,
  // Destinations allowed but hitting the limit, deactivating destinations with
  // lowest priority or time.
  kAllowedLimitHit = 1,
  // Destinations not allowed due to lower priority while hitting the limit.
  kNotAllowed = 2,
  kMaxValue = kNotAllowed,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:AttributionSourceDestinationLimitResult)

DestinationLimitResult GetDestinationLimitResult(
    const std::vector<StoredSource::Id>& sources_to_deactivate) {
  const bool destination_limit_hit = !sources_to_deactivate.empty();

  if (!base::FeatureList::IsEnabled(attribution_reporting::features::
                                        kAttributionSourceDestinationLimit)) {
    return destination_limit_hit ? DestinationLimitResult::kNotAllowed
                                 : DestinationLimitResult::kAllowed;
  }

  DestinationLimitResult result =
      destination_limit_hit
          ? (base::Contains(sources_to_deactivate,
                            StoredSource::Id(RateLimitTable::kUnsetRecordId))
                 ? DestinationLimitResult::kNotAllowed
                 : DestinationLimitResult::kAllowedLimitHit)
          : DestinationLimitResult::kAllowed;

  base::UmaHistogramEnumeration("Conversions.SourceDestinationLimitResult",
                                result);
  return result;
}

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

  CHECK(!source.registration().debug_key.has_value() ||
        source.common_info().debug_cookie_set());

  bool is_noised = false;
  std::optional<int> destination_limit;

  const base::Time source_time = base::Time::Now();

  const auto make_result = [&](StoreSourceResult::Result&& result) {
    if (absl::holds_alternative<StoreSourceResult::InternalError>(result)) {
      is_noised = false;
      destination_limit.reset();
    }
    return StoreSourceResult(std::move(source), is_noised, source_time,
                             destination_limit, std::move(result));
  };

  // TODO(crbug.com/40287976): Support multiple specs.
  if (source.registration().trigger_specs.specs().size() > 1u) {
    return make_result(StoreSourceResult::InternalError());
  }

  const CommonSourceInfo& common_info = source.common_info();
  const attribution_reporting::SourceRegistration& reg = source.registration();

  ASSIGN_OR_RETURN(
      const auto randomized_response_data,
      delegate_->GetRandomizedResponse(common_info.source_type(),
                                       reg.trigger_specs,
                                       reg.event_level_epsilon),
      [&](auto error) -> StoreSourceResult {
        DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
        switch (error) {
          case attribution_reporting::RandomizedResponseError::
              kExceedsChannelCapacityLimit:
            return make_result(StoreSourceResult::ExceedsMaxChannelCapacity(
                delegate_->GetMaxChannelCapacity(common_info.source_type())));
          case attribution_reporting::RandomizedResponseError::
              kExceedsTriggerStateCardinalityLimit:
            return make_result(
                StoreSourceResult::ExceedsMaxTriggerStateCardinality(
                    attribution_reporting::MaxTriggerStateCardinality()));
        }
      });
  DCHECK(attribution_reporting::IsValid(randomized_response_data.response(),
                                        reg.trigger_specs));

  // Force the creation of the database if it doesn't exist, as we need to
  // persist the source.
  if (!storage_.LazyInit(
          AttributionStorageSql::DbCreationPolicy::kCreateIfAbsent)) {
    return make_result(StoreSourceResult::InternalError());
  }

  // Only delete expired impressions periodically to avoid excessive DB
  // operations.
  const base::TimeDelta delete_frequency =
      delegate_->GetDeleteExpiredSourcesFrequency();
  DCHECK_GE(delete_frequency, base::TimeDelta());
  if (source_time - last_deleted_expired_sources_ >= delete_frequency) {
    if (!storage_.DeleteExpiredSources()) {
      return make_result(StoreSourceResult::InternalError());
    }
    last_deleted_expired_sources_ = source_time;
  }

  if (!storage_.HasCapacityForStoringSource(common_info.source_origin(),
                                            source_time)) {
    if (int64_t file_size = storage_.StorageFileSizeKB(); file_size > -1) {
      base::UmaHistogramCounts10M(
          "Conversions.Storage.Sql.FileSizeSourcesPerOriginLimitReached2",
          file_size);
      std::optional<int64_t> number_of_sources = storage_.NumberOfSources();
      if (number_of_sources.has_value()) {
        CHECK_GT(*number_of_sources, 0);
        base::UmaHistogramCounts1M(
            "Conversions.Storage.Sql.FileSizeSourcesPerOriginLimitReached2."
            "PerSource",
            file_size * 1024 / *number_of_sources);
      }
    }
    return make_result(StoreSourceResult::InsufficientSourceCapacity(
        delegate_->GetMaxSourcesPerOrigin()));
  }

  switch (storage_.SourceAllowedForReportingOriginPerSiteLimit(source,
                                                               source_time)) {
    case RateLimitResult::kAllowed:
      break;
    case RateLimitResult::kNotAllowed:
      return make_result(StoreSourceResult::ReportingOriginsPerSiteLimitReached(
          delegate_->GetRateLimits()
              .max_reporting_origins_per_source_reporting_site));
    case RateLimitResult::kError:
      return make_result(StoreSourceResult::InternalError());
  }

  RateLimitTable::DestinationRateLimitResult destination_rate_limit_result =
      storage_.SourceAllowedForDestinationRateLimit(source, source_time);
  base::UmaHistogramEnumeration("Conversions.DestinationRateLimitResult",
                                destination_rate_limit_result);

  bool hit_global_destination_limit = false;

  switch (destination_rate_limit_result) {
    case RateLimitTable::DestinationRateLimitResult::kAllowed:
      break;
    case RateLimitTable::DestinationRateLimitResult::kHitGlobalLimit:
      hit_global_destination_limit = true;
      break;
    case RateLimitTable::DestinationRateLimitResult::kHitReportingLimit:
      return make_result(StoreSourceResult::DestinationReportingLimitReached(
          delegate_->GetDestinationRateLimit().max_per_reporting_site));
    case RateLimitTable::DestinationRateLimitResult::kHitBothLimits:
      return make_result(StoreSourceResult::DestinationBothLimitsReached(
          delegate_->GetDestinationRateLimit().max_per_reporting_site));
    case RateLimitTable::DestinationRateLimitResult::kError:
      return make_result(StoreSourceResult::InternalError());
  }

  if (base::FeatureList::IsEnabled(attribution_reporting::features::
                                       kAttributionSourceDestinationLimit)) {
    switch (storage_.SourceAllowedForDestinationPerDayRateLimit(source,
                                                                source_time)) {
      case RateLimitResult::kAllowed:
        break;
      case RateLimitResult::kNotAllowed:
        return make_result(
            StoreSourceResult::DestinationPerDayReportingLimitReached(
                delegate_->GetDestinationRateLimit()
                    .max_per_reporting_site_per_day));
      case RateLimitResult::kError:
        return make_result(StoreSourceResult::InternalError());
    }
  }

  base::expected<std::vector<StoredSource::Id>, RateLimitTable::Error>
      source_ids_to_deactivate =
          storage_.GetSourcesToDeactivateForDestinationLimit(source,
                                                             source_time);
  if (!source_ids_to_deactivate.has_value()) {
    return make_result(StoreSourceResult::InternalError());
  }

  switch (GetDestinationLimitResult(*source_ids_to_deactivate)) {
    case DestinationLimitResult::kNotAllowed:
      return make_result(
          StoreSourceResult::InsufficientUniqueDestinationCapacity(
              delegate_->GetMaxDestinationsPerSourceSiteReportingSite()));
    case DestinationLimitResult::kAllowedLimitHit:
      destination_limit.emplace(
          delegate_->GetMaxDestinationsPerSourceSiteReportingSite());
      break;
    case DestinationLimitResult::kAllowed:
      break;
  }

  is_noised = randomized_response_data.response().has_value();

  std::unique_ptr<AttributionStorageSql::Transaction> transaction =
      storage_.StartTransaction();
  if (!transaction) {
    return make_result(StoreSourceResult::InternalError());
  }

  if (!storage_.DeactivateSourcesForDestinationLimit(*source_ids_to_deactivate,
                                                     source_time)) {
    return make_result(StoreSourceResult::InternalError());
  }

  const auto commit_and_return = [&](StoreSourceResult::Result&& result) {
    return transaction->Commit()
               ? make_result(std::move(result))
               : make_result(StoreSourceResult::InternalError());
  };

  // IMPORTANT: The following rate-limits are shared across reporting sites and
  // therefore security sensitive. It's important to ensure that these
  // rate-limits are checked as last steps in source registration to avoid
  // side-channel leakage of the cross-origin data.

  if (hit_global_destination_limit) {
    return commit_and_return(
        StoreSourceResult::DestinationGlobalLimitReached());
  }

  switch (storage_.SourceAllowedForReportingOriginLimit(source, source_time)) {
    case RateLimitResult::kAllowed:
      break;
    case RateLimitResult::kNotAllowed:
      return commit_and_return(StoreSourceResult::ExcessiveReportingOrigins());
    case RateLimitResult::kError:
      return make_result(StoreSourceResult::InternalError());
  }

  const base::Time aggregatable_report_window_time =
      source_time + reg.aggregatable_report_window;

  int num_attributions = 0;
  auto attribution_logic = StoredSource::AttributionLogic::kTruthfully;
  bool event_level_active = true;
  if (const auto& response = randomized_response_data.response()) {
    num_attributions = response->size();
    attribution_logic = num_attributions == 0
                            ? StoredSource::AttributionLogic::kNever
                            : StoredSource::AttributionLogic::kFalsely;
    event_level_active = num_attributions == 0;
  }

  std::optional<StoredSource> stored_source =
      storage_.InsertSource(source, source_time, num_attributions,
                            event_level_active, randomized_response_data.rate(),
                            attribution_logic, aggregatable_report_window_time);
  if (!stored_source.has_value()) {
    return make_result(StoreSourceResult::InternalError());
  }

  if (!storage_.AddRateLimitForSource(
          *stored_source, source.registration().destination_limit_priority)) {
    return make_result(StoreSourceResult::InternalError());
  }

  std::optional<base::Time> min_fake_report_time;

  if (attribution_logic == StoredSource::AttributionLogic::kFalsely) {
    for (const auto& fake_report : *randomized_response_data.response()) {
      auto trigger_spec_it = stored_source->trigger_specs().find(
          fake_report.trigger_data, TriggerDataMatching::kExact);

      const attribution_reporting::EventReportWindows& windows =
          (*trigger_spec_it).second.event_report_windows();

      base::Time report_time =
          windows.ReportTimeAtWindow(source_time, fake_report.window_index);
      // The report start time will always fall within a report window, no
      // matter the report window's end time.
      base::Time trigger_time =
          windows.StartTimeAtWindow(source_time, fake_report.window_index);
      DCHECK_EQ(windows.ComputeReportTime(source_time, trigger_time),
                report_time);

      // Set the `context_origin` to be the source origin for fake reports,
      // as these reports are generated only via the source site's context.
      // The fake destinations are not relevant to the context that
      // actually created the report.
      AttributionReport fake_attribution_report(
          AttributionInfo(trigger_time,
                          /*debug_key=*/std::nullopt,
                          /*context_origin=*/common_info.source_origin()),
          AttributionReport::Id(-1), report_time,
          /*initial_report_time=*/report_time, delegate_->NewReportID(),
          /*failed_send_attempts=*/0,
          AttributionReport::EventLevelData(fake_report.trigger_data,
                                            /*priority=*/0, *stored_source),
          stored_source->common_info().reporting_origin());
      if (!storage_.StoreAttributionReport(fake_attribution_report,
                                           &*stored_source)) {
        return make_result(StoreSourceResult::InternalError());
      }

      if (!min_fake_report_time.has_value() ||
          report_time < *min_fake_report_time) {
        min_fake_report_time = report_time;
      }
    }
  }

  if (attribution_logic != StoredSource::AttributionLogic::kTruthfully) {
    if (!storage_.AddRateLimitForAttribution(
            AttributionInfo(/*time=*/source_time,
                            /*debug_key=*/std::nullopt,
                            /*context_origin=*/common_info.source_origin()),
            *stored_source, RateLimitTable::Scope::kEventLevelAttribution,
            AttributionReport::Id(RateLimitTable::kUnsetRecordId))) {
      return make_result(StoreSourceResult::InternalError());
    }
  }

  if (!transaction->Commit()) {
    return make_result(StoreSourceResult::InternalError());
  }

  static_assert(AttributionStorageSql::kCurrentVersionNumber < 86);
  base::UmaHistogramCustomCounts("Conversions.DbVersionOnSourceStored",
                                 AttributionStorageSql::kCurrentVersionNumber,
                                 /*min=*/56,
                                 /*exclusive_max=*/86, /*buckets=*/30);

  return make_result(StoreSourceResult::Success(min_fake_report_time,
                                                stored_source->source_id()));
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

  std::vector<AttributionReport> reports =
      storage_.GetAttributionReports(max_report_time, limit);
  delegate_->ShuffleReports(reports);
  return reports;
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

  if (auto delay = delegate_->GetOfflineReportDelayConfig()) {
    storage_.AdjustOfflineReportTimes(delay->min, delay->max);
  }

  return storage_.GetNextReportTime(base::Time::Min());
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
