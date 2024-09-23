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
#include "base/ranges/algorithm.h"
#include "base/ranges/functional.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "components/attribution_reporting/aggregatable_dedup_key.h"
#include "components/attribution_reporting/aggregatable_utils.h"
#include "components/attribution_reporting/aggregatable_values.h"
#include "components/attribution_reporting/event_report_windows.h"
#include "components/attribution_reporting/event_trigger_data.h"
#include "components/attribution_reporting/features.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/source_registration.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "components/attribution_reporting/trigger_config.h"
#include "components/attribution_reporting/trigger_data_matching.mojom.h"
#include "content/browser/attribution_reporting/aggregatable_attribution_utils.h"
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
using ::attribution_reporting::EventReportWindows;
using ::attribution_reporting::SuitableOrigin;
using ::attribution_reporting::mojom::SourceType;
using ::attribution_reporting::mojom::TriggerDataMatching;

using AggregatableResult = AttributionTrigger::AggregatableResult;
using EventLevelResult = AttributionTrigger::EventLevelResult;
using StoredSourceData = AttributionStorageSql::StoredSourceData;

constexpr int64_t kUnsetRecordId = -1;

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
// LINT.ThenChange(//tools/metrics/histograms/metadata/attribution_reporting/enums.xml:AttributionSourceDestinationLimitResult)

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

bool IsSuccessResult(std::optional<EventLevelResult> result) {
  return result == EventLevelResult::kSuccess ||
         result == EventLevelResult::kSuccessDroppedLowerPriority;
}

bool IsSuccessResult(std::optional<AggregatableResult> result) {
  return result == AggregatableResult::kSuccess;
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(AttributionResult)
enum class AttributionResult {
  kEventLevelOnly = 0,
  kAggregatableOnly = 1,
  kBoth = 2,
  kMaxValue = kBoth,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/attribution_reporting/enums.xml:ConversionAttributionResult)

void RecordAttributionResult(AttributionResult result) {
  base::UmaHistogramEnumeration("Conversions.AttributionResult", result);
}

void RecordAttributionResult(const bool has_event_level_report,
                             const bool has_aggregatable_report) {
  if (has_event_level_report && has_aggregatable_report) {
    RecordAttributionResult(AttributionResult::kBoth);
  } else if (has_event_level_report) {
    RecordAttributionResult(AttributionResult::kEventLevelOnly);
  } else if (has_aggregatable_report) {
    RecordAttributionResult(AttributionResult::kAggregatableOnly);
  }
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
      delegate_->GetRandomizedResponse(
          common_info.source_type(), reg.trigger_specs, reg.event_level_epsilon,
          reg.attribution_scopes_data),
      [&](auto error) -> StoreSourceResult {
        DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
        switch (error) {
          case attribution_reporting::RandomizedResponseError::
              kExceedsChannelCapacityLimit:
            return make_result(StoreSourceResult::ExceedsMaxChannelCapacity(
                delegate_->config().privacy_math_config.GetMaxChannelCapacity(
                    common_info.source_type())));
          case attribution_reporting::RandomizedResponseError::
              kExceedsScopesChannelCapacityLimit:
            return make_result(
                StoreSourceResult::ExceedsMaxScopesChannelCapacity(
                    delegate_->config()
                        .privacy_math_config.GetMaxChannelCapacityScopes(
                            common_info.source_type())));
          case attribution_reporting::RandomizedResponseError::
              kExceedsTriggerStateCardinalityLimit:
            return make_result(
                StoreSourceResult::ExceedsMaxTriggerStateCardinality(
                    attribution_reporting::MaxTriggerStateCardinality()));
          case attribution_reporting::RandomizedResponseError::
              kExceedsMaxEventStatesLimit:
            return make_result(StoreSourceResult::ExceedsMaxEventStatesLimit(
                source.registration()
                    .attribution_scopes_data->max_event_states()));
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

  if (int64_t count = storage_.CountActiveSourcesWithSourceOrigin(
          common_info.source_origin(), source_time);
      count < 0) {
    return make_result(StoreSourceResult::InternalError());
  } else if (int64_t max = delegate_->GetMaxSourcesPerOrigin(); count >= max) {
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
    return make_result(StoreSourceResult::InsufficientSourceCapacity(max));
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

  if (!storage_.UpdateOrRemoveSourcesWithIncompatibleScopeFields(source,
                                                                 source_time) ||
      !storage_.RemoveSourcesWithOutdatedScopes(source, source_time)) {
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
      if (!storage_.StoreAttributionReport(
              stored_source->source_id(), trigger_time, report_time,
              /*external_report_id=*/delegate_->NewReportID(),
              /*trigger_debug_key=*/std::nullopt,
              /*context_origin=*/common_info.source_origin(),
              common_info.reporting_origin(), fake_report.trigger_data,
              /*priority=*/0)) {
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

  const attribution_reporting::TriggerRegistration& trigger_registration =
      trigger.registration();

  const base::Time trigger_time = base::Time::Now();

  AttributionInfo attribution_info(
      trigger_time, trigger_registration.debug_key,
      /*context_origin=*/trigger.destination_origin());

  // Declarations for all of the various pieces of information which may be
  // collected and/or returned as a result of computing new reports in order to
  // produce a `CreateReportResult`.
  std::optional<EventLevelResult> event_level_status;
  std::optional<AttributionReport> new_event_level_report;

  std::optional<AggregatableResult> aggregatable_status;
  std::optional<AttributionReport> new_aggregatable_report;

  std::optional<AttributionReport> replaced_event_level_report;
  std::optional<AttributionReport> dropped_event_level_report;

  std::optional<StoredSourceData> source_to_attribute;

  std::optional<base::Time> min_null_aggregatable_report_time;

  CreateReportResult::Limits limits;

  auto assemble_report_result =
      [&](std::optional<EventLevelResult> new_event_level_status,
          std::optional<AggregatableResult> new_aggregatable_status) {
        event_level_status = event_level_status.has_value()
                                 ? event_level_status
                                 : new_event_level_status;
        DCHECK(event_level_status.has_value());

        if (!IsSuccessResult(*event_level_status)) {
          new_event_level_report = std::nullopt;
          replaced_event_level_report = std::nullopt;
        }

        aggregatable_status = aggregatable_status.has_value()
                                  ? aggregatable_status
                                  : new_aggregatable_status;
        DCHECK(aggregatable_status.has_value());

        if (!IsSuccessResult(*aggregatable_status)) {
          new_aggregatable_report = std::nullopt;
        }

        if (event_level_status == EventLevelResult::kInternalError ||
            aggregatable_status == AggregatableResult::kInternalError) {
          min_null_aggregatable_report_time.reset();
        }

        return CreateReportResult(
            trigger_time, std::move(trigger), *event_level_status,
            *aggregatable_status, std::move(replaced_event_level_report),
            std::move(new_event_level_report),
            std::move(new_aggregatable_report),
            source_to_attribute
                ? std::make_optional(std::move(source_to_attribute->source))
                : std::nullopt,
            limits, std::move(dropped_event_level_report),
            min_null_aggregatable_report_time);
      };

  auto generate_null_reports_and_assemble_report_result =
      [&](std::optional<EventLevelResult> new_event_level_status,
          std::optional<AggregatableResult> new_aggregatable_status,
          AttributionStorageSql::Transaction& transaction)
          VALID_CONTEXT_REQUIRED(sequence_checker_) {
            DCHECK(!new_aggregatable_report.has_value());

            if (!GenerateNullAggregatableReportsAndStoreReports(
                    trigger, attribution_info,
                    source_to_attribute ? &source_to_attribute->source
                                        : nullptr,
                    new_aggregatable_report,
                    min_null_aggregatable_report_time) ||
                !transaction.Commit()) {
              min_null_aggregatable_report_time.reset();
            }

            return assemble_report_result(new_event_level_status,
                                          new_aggregatable_status);
          };

  if (trigger_registration.event_triggers.empty()) {
    event_level_status = EventLevelResult::kNotRegistered;
  }

  if (!trigger.HasAggregatableData()) {
    aggregatable_status = AggregatableResult::kNotRegistered;
  }

  if (event_level_status.has_value() && aggregatable_status.has_value()) {
    return assemble_report_result(/*new_event_level_status=*/std::nullopt,
                                  /*new_aggregatable_status=*/std::nullopt);
  }

  if (!storage_.LazyInit(
          AttributionStorageSql::DbCreationPolicy::kCreateIfAbsent)) {
    return assemble_report_result(EventLevelResult::kInternalError,
                                  AggregatableResult::kInternalError);
  }

  std::unique_ptr<AttributionStorageSql::Transaction> transaction =
      storage_.StartTransaction();
  if (!transaction) {
    return assemble_report_result(EventLevelResult::kInternalError,
                                  AggregatableResult::kInternalError);
  }

  std::optional<StoredSource::Id> source_id_to_attribute;
  std::vector<StoredSource::Id> source_ids_to_delete;
  std::vector<StoredSource::Id> source_ids_to_deactivate;
  if (!storage_.FindMatchingSourceForTrigger(
          trigger, trigger_time, source_id_to_attribute, source_ids_to_delete,
          source_ids_to_deactivate)) {
    return assemble_report_result(EventLevelResult::kInternalError,
                                  AggregatableResult::kInternalError);
  }
  if (!source_id_to_attribute.has_value()) {
    return generate_null_reports_and_assemble_report_result(
        EventLevelResult::kNoMatchingImpressions,
        AggregatableResult::kNoMatchingImpressions, *transaction);
  }

  source_to_attribute = storage_.ReadSourceToAttribute(*source_id_to_attribute);
  // This is only possible if there is a corrupt DB.
  if (!source_to_attribute.has_value()) {
    return assemble_report_result(EventLevelResult::kInternalError,
                                  AggregatableResult::kInternalError);
  }

  base::UmaHistogramBoolean(
      "Conversions.TriggerTimeLessThanSourceTime",
      trigger_time < source_to_attribute->source.source_time());

  const bool top_level_filters_match =
      source_to_attribute->source.filter_data().Matches(
          source_to_attribute->source.common_info().source_type(),
          source_to_attribute->source.source_time(), trigger_time,
          trigger_registration.filters);

  if (!top_level_filters_match) {
    return generate_null_reports_and_assemble_report_result(
        EventLevelResult::kNoMatchingSourceFilterData,
        AggregatableResult::kNoMatchingSourceFilterData, *transaction);
  }

  // Delete all unattributed sources and deactivate all attributed sources not
  // used.
  if (!storage_.DeleteSources(source_ids_to_delete) ||
      !storage_.DeactivateSources(source_ids_to_deactivate)) {
    return assemble_report_result(EventLevelResult::kInternalError,
                                  AggregatableResult::kInternalError);
  }

  std::optional<uint64_t> dedup_key;
  if (!event_level_status.has_value()) {
    if (EventLevelResult create_event_level_status =
            MaybeCreateEventLevelReport(attribution_info,
                                        source_to_attribute->source, trigger,
                                        new_event_level_report, dedup_key);
        create_event_level_status != EventLevelResult::kSuccess) {
      event_level_status = create_event_level_status;
    }
  }

  std::optional<uint64_t> aggregatable_dedup_key;
  if (!aggregatable_status.has_value()) {
    if (AggregatableResult create_aggregatable_status =
            MaybeCreateAggregatableAttributionReport(
                attribution_info, source_to_attribute->source, trigger,
                new_aggregatable_report, aggregatable_dedup_key,
                limits.max_aggregatable_reports_per_destination,
                limits.rate_limits_max_attributions);
        create_aggregatable_status != AggregatableResult::kSuccess) {
      aggregatable_status = create_aggregatable_status;
    }
  }

  if (event_level_status == EventLevelResult::kInternalError ||
      aggregatable_status == AggregatableResult::kInternalError) {
    return assemble_report_result(EventLevelResult::kInternalError,
                                  AggregatableResult::kInternalError);
  }

  if (event_level_status.has_value() && aggregatable_status.has_value()) {
    return generate_null_reports_and_assemble_report_result(
        /*new_event_level_status=*/std::nullopt,
        /*new_aggregatable_status=*/std::nullopt, *transaction);
  }

  switch (storage_.AttributionAllowedForReportingOriginLimit(
      attribution_info, source_to_attribute->source)) {
    case RateLimitResult::kAllowed:
      break;
    case RateLimitResult::kNotAllowed:
      limits.rate_limits_max_attribution_reporting_origins =
          delegate_->GetRateLimits().max_attribution_reporting_origins;
      new_aggregatable_report.reset();
      return generate_null_reports_and_assemble_report_result(
          EventLevelResult::kExcessiveReportingOrigins,
          AggregatableResult::kExcessiveReportingOrigins, *transaction);
    case RateLimitResult::kError:
      return assemble_report_result(EventLevelResult::kInternalError,
                                    AggregatableResult::kInternalError);
  }

  std::optional<EventLevelResult> store_event_level_status;
  if (!event_level_status.has_value()) {
    DCHECK(new_event_level_report.has_value());
    store_event_level_status = MaybeStoreEventLevelReport(
        *new_event_level_report, source_to_attribute->source, dedup_key,
        source_to_attribute->num_attributions, replaced_event_level_report,
        dropped_event_level_report,
        limits.max_event_level_reports_per_destination,
        limits.rate_limits_max_attributions);
  }

  std::optional<AggregatableResult> store_aggregatable_status;
  if (!aggregatable_status.has_value()) {
    DCHECK(new_aggregatable_report.has_value());
    store_aggregatable_status =
        storage_.MaybeStoreAggregatableAttributionReportData(
            *new_aggregatable_report, source_to_attribute->source.source_id(),
            source_to_attribute->source
                .remaining_aggregatable_attribution_budget(),
            source_to_attribute->num_aggregatable_attribution_reports,
            aggregatable_dedup_key, limits.max_aggregatable_reports_per_source);
  }

  if (store_event_level_status == EventLevelResult::kInternalError ||
      store_aggregatable_status == AggregatableResult::kInternalError) {
    return assemble_report_result(EventLevelResult::kInternalError,
                                  AggregatableResult::kInternalError);
  }

  if (!IsSuccessResult(store_event_level_status)) {
    new_event_level_report.reset();
  }

  if (!IsSuccessResult(store_aggregatable_status)) {
    new_aggregatable_report.reset();
  }

  // Stores null reports and the aggregatable report here to be in the same
  // transaction.
  if (!GenerateNullAggregatableReportsAndStoreReports(
          trigger, attribution_info, &source_to_attribute->source,
          new_aggregatable_report, min_null_aggregatable_report_time)) {
    min_null_aggregatable_report_time.reset();
    return assemble_report_result(EventLevelResult::kInternalError,
                                  AggregatableResult::kInternalError);
  }

  // Early exit if done modifying the storage. Noised reports still need to
  // clean sources.
  if (!IsSuccessResult(store_event_level_status) &&
      !IsSuccessResult(store_aggregatable_status) &&
      store_event_level_status != EventLevelResult::kNeverAttributedSource) {
    if (!transaction->Commit()) {
      return assemble_report_result(EventLevelResult::kInternalError,
                                    AggregatableResult::kInternalError);
    }

    return assemble_report_result(store_event_level_status,
                                  store_aggregatable_status);
  }

  // Based on the deletion logic here and the fact that we delete sources
  // with |num_attributions > 0| or |num_aggregatable_attribution_reports > 0|
  // when there is a new matching source in |StoreSource()|, we should be
  // guaranteed that these sources all have |num_attributions == 0| and
  // |num_aggregatable_attribution_reports == 0|, and that they never
  // contributed to a rate limit. Therefore, we don't need to call
  // |RateLimitTable::ClearDataForSourceIds()| here.

  // Reports which are dropped do not need to make any further changes.
  if (store_event_level_status == EventLevelResult::kNeverAttributedSource &&
      !IsSuccessResult(store_aggregatable_status)) {
    if (!transaction->Commit()) {
      return assemble_report_result(EventLevelResult::kInternalError,
                                    AggregatableResult::kInternalError);
    }

    return assemble_report_result(store_event_level_status,
                                  store_aggregatable_status);
  }

  RecordAttributionResult(IsSuccessResult(store_event_level_status),
                          IsSuccessResult(store_aggregatable_status));

  if (new_event_level_report.has_value() &&
      !storage_.AddRateLimitForAttribution(
          attribution_info, source_to_attribute->source,
          RateLimitTable::Scope::kEventLevelAttribution,
          new_event_level_report->id())) {
    return assemble_report_result(EventLevelResult::kInternalError,
                                  AggregatableResult::kInternalError);
  }

  if (new_aggregatable_report.has_value() &&
      !storage_.AddRateLimitForAttribution(
          attribution_info, source_to_attribute->source,
          RateLimitTable::Scope::kAggregatableAttribution,
          new_aggregatable_report->id())) {
    return assemble_report_result(EventLevelResult::kInternalError,
                                  AggregatableResult::kInternalError);
  }

  if (!transaction->Commit()) {
    return assemble_report_result(EventLevelResult::kInternalError,
                                  AggregatableResult::kInternalError);
  }

  return assemble_report_result(store_event_level_status,
                                store_aggregatable_status);
}

EventLevelResult AttributionResolverImpl::MaybeCreateEventLevelReport(
    const AttributionInfo& attribution_info,
    const StoredSource& source,
    const AttributionTrigger& trigger,
    std::optional<AttributionReport>& report,
    std::optional<uint64_t>& dedup_key) {
  if (source.attribution_logic() == StoredSource::AttributionLogic::kFalsely) {
    DCHECK_EQ(source.active_state(),
              StoredSource::ActiveState::kReachedEventLevelAttributionLimit);
    return EventLevelResult::kFalselyAttributedSource;
  }

  const CommonSourceInfo& common_info = source.common_info();

  const SourceType source_type = common_info.source_type();

  auto event_trigger = base::ranges::find_if(
      trigger.registration().event_triggers,
      [&](const attribution_reporting::EventTriggerData& event_trigger) {
        return source.filter_data().Matches(
            source_type, source.source_time(),
            /*trigger_time=*/attribution_info.time, event_trigger.filters);
      });

  if (event_trigger == trigger.registration().event_triggers.end()) {
    return EventLevelResult::kNoMatchingConfigurations;
  }

  if (event_trigger->dedup_key.has_value() &&
      base::Contains(source.dedup_keys(), *event_trigger->dedup_key)) {
    return EventLevelResult::kDeduplicated;
  }

  auto trigger_spec_it = source.trigger_specs().find(
      event_trigger->data, source.trigger_data_matching());
  if (!trigger_spec_it) {
    return EventLevelResult::kNoMatchingTriggerData;
  }

  auto [trigger_data, trigger_spec] = *trigger_spec_it;

  switch (trigger_spec.event_report_windows().FallsWithin(
      attribution_info.time - source.source_time())) {
    case EventReportWindows::WindowResult::kFallsWithin:
      break;
    case EventReportWindows::WindowResult::kNotStarted:
      return EventLevelResult::kReportWindowNotStarted;
    case EventReportWindows::WindowResult::kPassed:
      return EventLevelResult::kReportWindowPassed;
  }

  const base::Time report_time = delegate_->GetEventLevelReportTime(
      trigger_spec.event_report_windows(), source.source_time(),
      attribution_info.time);

  report = AttributionReport(
      attribution_info, AttributionReport::Id(kUnsetRecordId), report_time,
      /*initial_report_time=*/report_time, delegate_->NewReportID(),
      /*failed_send_attempts=*/0,
      AttributionReport::EventLevelData(trigger_data, event_trigger->priority,
                                        source),
      common_info.reporting_origin());

  dedup_key = event_trigger->dedup_key;

  return EventLevelResult::kSuccess;
}

AggregatableResult
AttributionResolverImpl::MaybeCreateAggregatableAttributionReport(
    const AttributionInfo& attribution_info,
    const StoredSource& source,
    const AttributionTrigger& trigger,
    std::optional<AttributionReport>& report,
    std::optional<uint64_t>& dedup_key,
    std::optional<int>& max_aggregatable_reports_per_destination,
    std::optional<int64_t>& rate_limits_max_attributions) {
  const attribution_reporting::TriggerRegistration& trigger_registration =
      trigger.registration();

  const CommonSourceInfo& common_info = source.common_info();

  if (attribution_info.time >= source.aggregatable_report_window_time()) {
    return AggregatableResult::kReportWindowPassed;
  }

  const SourceType source_type = common_info.source_type();

  auto matched_dedup_key = base::ranges::find_if(
      trigger.registration().aggregatable_dedup_keys,
      [&](const attribution_reporting::AggregatableDedupKey&
              aggregatable_dedup_key) {
        return source.filter_data().Matches(
            source_type, source.source_time(),
            /*trigger_time=*/attribution_info.time,
            aggregatable_dedup_key.filters);
      });

  if (matched_dedup_key !=
      trigger.registration().aggregatable_dedup_keys.end()) {
    dedup_key = matched_dedup_key->dedup_key;
  }

  if (dedup_key.has_value() &&
      base::Contains(source.aggregatable_dedup_keys(), *dedup_key)) {
    return AggregatableResult::kDeduplicated;
  }

  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      contributions = CreateAggregatableHistogram(
          source.filter_data(), source_type, source.source_time(),
          /*trigger_time=*/attribution_info.time, source.aggregation_keys(),
          trigger_registration.aggregatable_trigger_data,
          trigger_registration.aggregatable_values);
  if (contributions.empty()) {
    return AggregatableResult::kNoHistograms;
  }

  if (int64_t count = storage_.CountReportsWithDestinationSite(
          net::SchemefulSite(attribution_info.context_origin),
          AttributionReport::Type::kAggregatableAttribution);
      count < 0) {
    return AggregatableResult::kInternalError;
  } else if (max_aggregatable_reports_per_destination =
                 delegate_->GetMaxReportsPerDestination(
                     AttributionReport::Type::kAggregatableAttribution);
             count >= *max_aggregatable_reports_per_destination) {
    return AggregatableResult::kNoCapacityForConversionDestination;
  }

  switch (storage_.AttributionAllowedForAttributionLimit(
      attribution_info, source,
      RateLimitTable::Scope::kAggregatableAttribution)) {
    case RateLimitResult::kAllowed:
      break;
    case RateLimitResult::kNotAllowed:
      rate_limits_max_attributions =
          delegate_->GetRateLimits().max_attributions;
      return AggregatableResult::kExcessiveAttributions;
    case RateLimitResult::kError:
      return AggregatableResult::kInternalError;
  }

  base::Time report_time =
      GetAggregatableReportTime(trigger, attribution_info.time);

  report = AttributionReport(
      attribution_info, AttributionReport::Id(kUnsetRecordId), report_time,
      /*initial_report_time=*/report_time, delegate_->NewReportID(),
      /*failed_send_attempts=*/0,
      AttributionReport::AggregatableAttributionData(
          AttributionReport::CommonAggregatableData(
              trigger_registration.aggregation_coordinator_origin,
              trigger_registration.aggregatable_trigger_config),
          std::move(contributions), source),
      source.common_info().reporting_origin());

  return AggregatableResult::kSuccess;
}

bool AttributionResolverImpl::GenerateNullAggregatableReportsAndStoreReports(
    const AttributionTrigger& trigger,
    const AttributionInfo& attribution_info,
    const StoredSource* source,
    std::optional<AttributionReport>& new_aggregatable_report,
    std::optional<base::Time>& min_null_aggregatable_report_time) {
  std::optional<base::Time> attributed_source_time;

  if (new_aggregatable_report) {
    const auto* data =
        absl::get_if<AttributionReport::AggregatableAttributionData>(
            &new_aggregatable_report->data());
    DCHECK(data);
    attributed_source_time = data->source_time;

    DCHECK(source);

    std::optional<AttributionReport::Id> report_id =
        storage_.StoreAggregatableReport(
            source->source_id(), attribution_info.time,
            new_aggregatable_report->initial_report_time(),
            new_aggregatable_report->external_report_id(),
            attribution_info.debug_key, attribution_info.context_origin,
            new_aggregatable_report->reporting_origin(),
            data->common_data.aggregation_coordinator_origin,
            data->common_data.aggregatable_trigger_config, data->contributions);

    if (!report_id.has_value()) {
      return false;
    }

    new_aggregatable_report->set_id(*report_id);
  }

  if (trigger.HasAggregatableData()) {
    std::vector<attribution_reporting::NullAggregatableReport>
        null_aggregatable_reports =
            attribution_reporting::GetNullAggregatableReports(
                trigger.registration().aggregatable_trigger_config,
                attribution_info.time, attributed_source_time,
                [&](int lookback_day) {
                  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
                  return delegate_
                      ->GenerateNullAggregatableReportForLookbackDay(
                          lookback_day, trigger.registration()
                                            .aggregatable_trigger_config
                                            .source_registration_time_config());
                });

    for (const auto& null_aggregatable_report : null_aggregatable_reports) {
      base::Time report_time =
          GetAggregatableReportTime(trigger, attribution_info.time);
      min_null_aggregatable_report_time = AttributionReport::MinReportTime(
          min_null_aggregatable_report_time, report_time);

      if (!storage_.StoreNullReport(
              /*trigger_time=*/attribution_info.time, report_time,
              /*external_report_id=*/delegate_->NewReportID(),
              /*trigger_debug_key=*/std::nullopt,
              attribution_info.context_origin, trigger.reporting_origin(),
              trigger.registration().aggregation_coordinator_origin,
              trigger.registration().aggregatable_trigger_config,
              null_aggregatable_report.fake_source_time)) {
        return false;
      }
    }
  }

  return true;
}

base::Time AttributionResolverImpl::GetAggregatableReportTime(
    const AttributionTrigger& trigger,
    base::Time trigger_time) const {
  if (trigger.registration()
          .aggregatable_trigger_config
          .ShouldCauseAReportToBeSentUnconditionally()) {
    return trigger_time;
  }
  return delegate_->GetAggregatableReportTime(trigger_time);
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

// Checks whether a new report is allowed to be stored for the given source
// based on `GetDefaultAttributionsPerSource()`. If there's sufficient capacity,
// the new report should be stored. Otherwise, if all existing reports were from
// an earlier window, the corresponding source is deactivated and the new
// report should be dropped. Otherwise, If there's insufficient capacity, checks
// the new report's priority against all existing ones for the same source.
// If all existing ones have greater priority, the new report should be dropped;
// otherwise, the existing one with the lowest priority is deleted and the new
// one should be stored.
AttributionResolverImpl::ReplaceReportResult
AttributionResolverImpl::MaybeReplaceLowerPriorityEventLevelReport(
    const AttributionReport& report,
    const StoredSource& source,
    int num_attributions,
    std::optional<AttributionReport>& replaced_report) {
  DCHECK_GE(num_attributions, 0);

  const auto* data =
      absl::get_if<AttributionReport::EventLevelData>(&report.data());
  DCHECK(data);

  // TODO(crbug.com/40287976): The logic in this method doesn't properly handle
  // the case in which there are different report windows for different trigger
  // data. Prior to enabling `attribution_reporting::features::kTriggerConfig`,
  // this must be fixed.
  DCHECK(source.trigger_specs().SingleSharedSpec());

  // If there's already capacity for the new report, there's nothing to do.
  if (num_attributions < source.trigger_specs().max_event_level_reports()) {
    return ReplaceReportResult::kAddNewReport;
  }

  ASSIGN_OR_RETURN(
      const std::optional<AttributionStorageSql::ReportIdAndPriority>
          report_with_min_priority,
      storage_.GetReportWithMinPriority(source.source_id(),
                                        report.initial_report_time()),
      [](AttributionStorageSql::Error) { return ReplaceReportResult::kError; });

  // Deactivate the source at event-level as a new report will never be
  // generated in the future.
  if (!report_with_min_priority.has_value()) {
    if (!storage_.DeactivateSourceAtEventLevel(source.source_id())) {
      return ReplaceReportResult::kError;
    }
    return ReplaceReportResult::kDropNewReportSourceDeactivated;
  }

  // If the new report's priority is less than all existing ones, or if its
  // priority is equal to the minimum existing one and it is more recent, drop
  // it. We could explicitly check the trigger time here, but it would only
  // be relevant in the case of an ill-behaved clock, in which case the rest of
  // the attribution functionality would probably also break.
  if (data->priority <= report_with_min_priority->priority) {
    return ReplaceReportResult::kDropNewReport;
  }

  std::optional<AttributionReport> replaced =
      storage_.GetReport(report_with_min_priority->id);
  if (!replaced.has_value()) {
    return ReplaceReportResult::kError;
  }

  // Otherwise, delete the existing report with the lowest priority and the
  // corresponding attribution rate-limit record.
  if (!storage_.DeleteReport(report_with_min_priority->id) ||
      !storage_.DeleteAttributionRateLimit(
          RateLimitTable::Scope::kEventLevelAttribution, replaced->id())) {
    return ReplaceReportResult::kError;
  }

  replaced_report = std::move(replaced);
  return ReplaceReportResult::kReplaceOldReport;
}

EventLevelResult AttributionResolverImpl::MaybeStoreEventLevelReport(
    AttributionReport& report,
    const StoredSource& source,
    std::optional<uint64_t> dedup_key,
    int num_attributions,
    std::optional<AttributionReport>& replaced_report,
    std::optional<AttributionReport>& dropped_report,
    std::optional<int>& max_event_level_reports_per_destination,
    std::optional<int64_t>& rate_limits_max_attributions) {
  const auto* event_level_data =
      absl::get_if<AttributionReport::EventLevelData>(&report.data());
  DCHECK(event_level_data);

  if (source.active_state() ==
      StoredSource::ActiveState::kReachedEventLevelAttributionLimit) {
    dropped_report = std::move(report);
    return EventLevelResult::kExcessiveReports;
  }

  std::unique_ptr<AttributionStorageSql::Transaction> transaction =
      storage_.StartTransaction();
  if (!transaction) {
    return EventLevelResult::kInternalError;
  }

  const auto maybe_replace_lower_priority_report_result =
      MaybeReplaceLowerPriorityEventLevelReport(
          report, source, num_attributions, replaced_report);

  const auto commit_and_return = [&](EventLevelResult result) {
    return transaction->Commit() ? result : EventLevelResult::kInternalError;
  };

  switch (maybe_replace_lower_priority_report_result) {
    case ReplaceReportResult::kError:
      return EventLevelResult::kInternalError;
    case ReplaceReportResult::kDropNewReport:
    case ReplaceReportResult::kDropNewReportSourceDeactivated:
      dropped_report = std::move(report);

      return commit_and_return(maybe_replace_lower_priority_report_result ==
                                       ReplaceReportResult::kDropNewReport
                                   ? EventLevelResult::kPriorityTooLow
                                   : EventLevelResult::kExcessiveReports);
    case ReplaceReportResult::kAddNewReport: {
      switch (storage_.AttributionAllowedForAttributionLimit(
          report.attribution_info(), source,
          RateLimitTable::Scope::kEventLevelAttribution)) {
        case RateLimitResult::kAllowed:
          break;
        case RateLimitResult::kNotAllowed:
          rate_limits_max_attributions =
              delegate_->GetRateLimits().max_attributions;
          return commit_and_return(EventLevelResult::kExcessiveAttributions);
        case RateLimitResult::kError:
          return EventLevelResult::kInternalError;
      }

      if (int64_t count = storage_.CountReportsWithDestinationSite(
              net::SchemefulSite(report.attribution_info().context_origin),
              AttributionReport::Type::kEventLevel);
          count < 0) {
        return EventLevelResult::kInternalError;
      } else if (max_event_level_reports_per_destination =
                     delegate_->GetMaxReportsPerDestination(
                         AttributionReport::Type::kEventLevel);
                 count >= *max_event_level_reports_per_destination) {
        return commit_and_return(
            EventLevelResult::kNoCapacityForConversionDestination);
      }

      // Only increment the number of conversions associated with the source if
      // we are adding a new one, rather than replacing a dropped one.
      if (!storage_.IncrementNumAttributions(source.source_id())) {
        return EventLevelResult::kInternalError;
      }
      break;
    }
    case ReplaceReportResult::kReplaceOldReport:
      break;
  }

  // Reports with `AttributionLogic::kNever` should be included in all
  // attribution operations and matching, but only `kTruthfully` should generate
  // reports that get sent.
  const bool create_report =
      source.attribution_logic() == StoredSource::AttributionLogic::kTruthfully;

  if (create_report) {
    std::optional<AttributionReport::Id> report_id =
        storage_.StoreAttributionReport(
            source.source_id(), report.attribution_info().time,
            report.initial_report_time(), report.external_report_id(),
            report.attribution_info().debug_key,
            report.attribution_info().context_origin, report.reporting_origin(),
            event_level_data->trigger_data, event_level_data->priority);

    if (!report_id.has_value()) {
      return EventLevelResult::kInternalError;
    }

    report.set_id(*report_id);
  }

  // If a dedup key is present, store it. We do this regardless of whether
  // `create_report` is true to avoid leaking whether the report was actually
  // stored.
  if (dedup_key.has_value() &&
      !storage_.StoreDedupKey(source.source_id(), *dedup_key,
                              AttributionReport::Type::kEventLevel)) {
    return EventLevelResult::kInternalError;
  }

  if (!create_report) {
    return commit_and_return(EventLevelResult::kNeverAttributedSource);
  }

  if (maybe_replace_lower_priority_report_result ==
      ReplaceReportResult::kReplaceOldReport) {
    return commit_and_return(EventLevelResult::kSuccessDroppedLowerPriority);
  }

  return commit_and_return(EventLevelResult::kSuccess);
}

}  // namespace content
