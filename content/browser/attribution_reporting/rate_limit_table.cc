// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/rate_limit_table.h"

#include <stdint.h>

#include <limits>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/memory/raw_ref.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/ranges/functional.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/attribution_reporting/source_registration.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "content/browser/attribution_reporting/attribution_config.h"
#include "content/browser/attribution_reporting/attribution_info.h"
#include "content/browser/attribution_reporting/attribution_resolver_delegate.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/rate_limit_result.h"
#include "content/browser/attribution_reporting/sql_queries.h"
#include "content/browser/attribution_reporting/sql_utils.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "net/base/schemeful_site.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

namespace content {

namespace {

bool IsAttribution(RateLimitTable::Scope scope) {
  switch (scope) {
    case RateLimitTable::Scope::kSource:
      return false;
    case RateLimitTable::Scope::kEventLevelAttribution:
    case RateLimitTable::Scope::kAggregatableAttribution:
      return true;
  }

  NOTREACHED();
}

}  // namespace

RateLimitTable::RateLimitTable(const AttributionResolverDelegate* delegate)
    : delegate_(
          raw_ref<const AttributionResolverDelegate>::from_ptr(delegate)) {}

RateLimitTable::~RateLimitTable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool RateLimitTable::CreateTable(sql::Database* db) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // All columns in this table are const.
  // |source_id| is the primary key of a row in the |impressions| table,
  // though the row may not exist.
  // |scope| is a serialized `RateLimitTable::Scope`.
  // |source_site| is the eTLD+1 of the impression.
  // |destination_site| is the destination of the conversion.
  // |context_origin| is the source origin for `kSource` or the destination
  // origin for `kEventLevelAttribution` or `kAggregatableAttribution`.
  // |reporting_origin| is the reporting origin of the impression/conversion.
  // |time| is the time of the source registration.
  // |source_expiry_or_attribution_time| is either the source's expiry time or
  // the attribution time, depending on |scope|.
  // |report_id| is the report ID for `kEventLevelAttribution` or
  // `kAggregatableAttribution` and is set to -1 for `kSource`. Note that -1 is
  // also set for `kEventLevelAttribution` records associated with fake reports,
  // as well as the attribution records from migration.
  // |deactivated_for_source_destination_limit| indicates whether the record
  // should be considered for source destination limit. This is only relevant
  // for `kSource` and is set to 0 for `kEventLevelAttribution` and
  // `kAggregatableAttribution`.
  // |destination_limit_priority| indicates the priority of the record in
  // regards of source destination limit. This is only relevant for `kSource`
  // and is set to 0 for `kEventLevelAttribution` and
  // `kAggregatableAttribution`.
  static constexpr char kRateLimitTableSql[] =
      "CREATE TABLE rate_limits("
      "id INTEGER PRIMARY KEY NOT NULL,"
      "scope INTEGER NOT NULL,"
      "source_id INTEGER NOT NULL,"
      "source_site TEXT NOT NULL,"
      "destination_site TEXT NOT NULL,"
      "context_origin TEXT NOT NULL,"
      "reporting_origin TEXT NOT NULL,"
      "reporting_site TEXT NOT NULL,"
      "time INTEGER NOT NULL,"
      "source_expiry_or_attribution_time INTEGER NOT NULL,"
      "report_id INTEGER NOT NULL,"
      "deactivated_for_source_destination_limit INTEGER NOT NULL,"
      "destination_limit_priority INTEGER NOT NULL)";
  if (!db->Execute(kRateLimitTableSql)) {
    return false;
  }

  // Optimizes calls to `AllowedForReportingOriginLimit()` and
  // `AttributionAllowedForAttributionLimit()`.
  static constexpr char kRateLimitReportingOriginIndexSql[] =
      "CREATE INDEX rate_limit_reporting_origin_idx "
      "ON rate_limits(scope,source_site,destination_site)";
  if (!db->Execute(kRateLimitReportingOriginIndexSql)) {
    return false;
  }

  // Optimizes calls to |DeleteExpiredRateLimits()|, |ClearAllDataInRange()|,
  // |ClearDataForOriginsInRange()|.
  static constexpr char kRateLimitTimeIndexSql[] =
      "CREATE INDEX rate_limit_time_idx ON rate_limits(time)";
  if (!db->Execute(kRateLimitTimeIndexSql)) {
    return false;
  }

  // Optimizes calls to |ClearDataForSourceIds()|.
  static constexpr char kRateLimitImpressionIdIndexSql[] =
      "CREATE INDEX rate_limit_source_id_idx "
      "ON rate_limits(source_id)";
  if (!db->Execute(kRateLimitImpressionIdIndexSql)) {
    return false;
  }

  // Optimizes calls to `DeleteAttributionRateLimit()`.
  static constexpr char kRateLimitReportIdIndexSql[] =
      "CREATE INDEX rate_limit_report_id_idx "
      "ON rate_limits(scope,report_id)"
      "WHERE " RATE_LIMIT_ATTRIBUTION_CONDITION
      " AND " RATE_LIMIT_REPORT_ID_SET_CONDITION;
  return db->Execute(kRateLimitReportIdIndexSql);
}

bool RateLimitTable::AddRateLimitForSource(sql::Database* db,
                                           const StoredSource& source,
                                           int64_t destination_limit_priority) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return AddRateLimit(db, source, /*trigger_time=*/std::nullopt,
                      /*context_origin=*/source.common_info().source_origin(),
                      Scope::kSource,
                      /*report_id=*/std::nullopt, destination_limit_priority);
}

bool RateLimitTable::AddRateLimitForAttribution(
    sql::Database* db,
    const AttributionInfo& attribution_info,
    const StoredSource& source,
    Scope scope,
    AttributionReport::Id report_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return AddRateLimit(db, source, attribution_info.time,
                      attribution_info.context_origin, scope, report_id,
                      /*destination_limit_priority=*/std::nullopt);
}

bool RateLimitTable::AddRateLimit(
    sql::Database* db,
    const StoredSource& source,
    std::optional<base::Time> trigger_time,
    const attribution_reporting::SuitableOrigin& context_origin,
    Scope scope,
    std::optional<AttributionReport::Id> report_id,
    std::optional<int64_t> destination_limit_priority) {
  const bool is_attribution = IsAttribution(scope);
  CHECK_EQ(trigger_time.has_value(), is_attribution);
  CHECK_EQ(report_id.has_value(), is_attribution);
  CHECK_NE(destination_limit_priority.has_value(), is_attribution);

  const CommonSourceInfo& common_info = source.common_info();

  // Only delete expired rate limits periodically to avoid excessive DB
  // operations.
  const base::TimeDelta delete_frequency =
      delegate_->GetDeleteExpiredRateLimitsFrequency();
  DCHECK_GE(delete_frequency, base::TimeDelta());
  const base::Time now = base::Time::Now();
  if (now - last_cleared_ >= delete_frequency) {
    if (!DeleteExpiredRateLimits(db)) {
      return false;
    }
    last_cleared_ = now;
  }

  base::Time source_expiry_or_attribution_time;
  int64_t report_id_value = kUnsetRecordId;
  int64_t destination_limit_priority_value = 0;
  if (is_attribution) {
    source_expiry_or_attribution_time = *trigger_time;
    report_id_value = **report_id;
  } else {
    scope = Scope::kSource;
    source_expiry_or_attribution_time = source.expiry_time();
    destination_limit_priority_value = *destination_limit_priority;
  }

  static constexpr char kStoreRateLimitSql[] =
      "INSERT INTO rate_limits"
      "(scope,source_id,source_site,destination_site,context_origin,"
      "reporting_origin,reporting_site,time,source_expiry_or_attribution_time,"
      "report_id,deactivated_for_source_destination_limit,"
      "destination_limit_priority)"
      "VALUES(?,?,?,?,?,?,?,?,?,?,0,?)";
  sql::Statement statement(
      db->GetCachedStatement(SQL_FROM_HERE, kStoreRateLimitSql));

  statement.BindInt(0, static_cast<int>(scope));
  statement.BindInt64(1, *source.source_id());
  statement.BindString(2, common_info.source_site().Serialize());
  statement.BindString(4, context_origin.Serialize());
  statement.BindString(5, common_info.reporting_origin().Serialize());
  statement.BindString(
      6, net::SchemefulSite(common_info.reporting_origin()).Serialize());
  statement.BindTime(7, source.source_time());
  statement.BindTime(8, source_expiry_or_attribution_time);
  statement.BindInt64(9, report_id_value);
  statement.BindInt64(10, destination_limit_priority_value);

  const auto insert_row = [&](const net::SchemefulSite& site) {
    statement.BindString(3, site.Serialize());
    return statement.Run();
  };

  if (scope == Scope::kAggregatableAttribution ||
      (source.attribution_logic() ==
           StoredSource::AttributionLogic::kTruthfully &&
       scope == Scope::kEventLevelAttribution)) {
    return insert_row(net::SchemefulSite(context_origin));
  }

  sql::Transaction transaction(db);
  if (!transaction.Begin()) {
    return false;
  }
  for (const auto& site : source.destination_sites().destinations()) {
    statement.Reset(/*clear_bound_vars=*/false);
    if (!insert_row(site)) {
      return false;
    }
  }
  return transaction.Commit();
}

RateLimitResult RateLimitTable::AttributionAllowedForAttributionLimit(
    sql::Database* db,
    const AttributionInfo& attribution_info,
    const StoredSource& source,
    Scope scope) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(IsAttribution(scope));

  const CommonSourceInfo& common_info = source.common_info();

  const AttributionConfig::RateLimitConfig& rate_limits =
      delegate_->GetRateLimits();
  DCHECK_GT(rate_limits.time_window, base::TimeDelta());
  DCHECK_GT(rate_limits.max_attributions, 0);

  base::Time min_timestamp = attribution_info.time - rate_limits.time_window;

  sql::Statement statement(db->GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kRateLimitAttributionAllowedSql));
  statement.BindInt(0, static_cast<int>(scope));
  statement.BindString(
      1, net::SchemefulSite(attribution_info.context_origin).Serialize());
  statement.BindString(2, common_info.source_site().Serialize());
  statement.BindString(
      3, net::SchemefulSite(common_info.reporting_origin()).Serialize());
  statement.BindTime(4, min_timestamp);

  if (!statement.Step()) {
    return RateLimitResult::kError;
  }

  int64_t count = statement.ColumnInt64(0);

  return count < rate_limits.max_attributions ? RateLimitResult::kAllowed
                                              : RateLimitResult::kNotAllowed;
}

RateLimitResult RateLimitTable::SourceAllowedForReportingOriginLimit(
    sql::Database* db,
    const StorableSource& source,
    base::Time source_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return AllowedForReportingOriginLimit(
      db, /*is_source=*/true, source.common_info(), source_time,
      source.registration().destination_set.destinations());
}

RateLimitResult RateLimitTable::SourceAllowedForReportingOriginPerSiteLimit(
    sql::Database* db,
    const StorableSource& source,
    base::Time source_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  size_t max_origins =
      static_cast<size_t>(delegate_->GetRateLimits()
                              .max_reporting_origins_per_source_reporting_site);

  base::Time min_timestamp =
      source_time - delegate_->GetRateLimits().origins_per_site_window;

  sql::Statement statement(db->GetCachedStatement(
      SQL_FROM_HERE,
      attribution_queries::kRateLimitSelectSourceReportingOriginsBySiteSql));
  statement.BindString(0, source.common_info().source_site().Serialize());
  statement.BindString(
      1,
      net::SchemefulSite(source.common_info().reporting_origin()).Serialize());
  statement.BindTime(2, min_timestamp);

  const std::string serialized_reporting_origin =
      source.common_info().reporting_origin().Serialize();
  std::set<std::string> reporting_origins;
  while (statement.Step()) {
    std::string_view origin = statement.ColumnStringView(0);

    if (origin == serialized_reporting_origin) {
      return RateLimitResult::kAllowed;
    }

    // Note: In C++23 this can be `insert(origin)` instead to avoid copying the
    // string when the value is already contained.
    reporting_origins.insert(std::string(origin));
    if (reporting_origins.size() == max_origins) {
      return RateLimitResult::kNotAllowed;
    }
  }

  if (!statement.Succeeded()) {
    return RateLimitResult::kError;
  }

  return RateLimitResult::kAllowed;
}

namespace {

struct DestinationAttribute {
  int64_t priority = std::numeric_limits<int64_t>::min();
  base::Time time = base::Time::Min();

  bool operator<(const DestinationAttribute& other) const {
    return std::tie(priority, time) < std::tie(other.priority, other.time);
  }
};

struct DestinationData {
  DestinationAttribute attribute;
  std::vector<StoredSource::Id> sources;

  DestinationData() = default;

  DestinationData(const DestinationData&) = delete;
  DestinationData& operator=(const DestinationData&) = delete;

  DestinationData(DestinationData&&) = default;
  DestinationData& operator=(DestinationData&&) = default;

  void Assign(std::vector<StoredSource::Id>& source_ids) && {
    if (source_ids.empty()) {
      source_ids = std::move(sources);
    } else {
      source_ids.insert(source_ids.end(), sources.begin(), sources.end());
    }
  }
};

using DestinationDataMap = std::map<std::string, DestinationData>;

void AddDestination(DestinationDataMap& destination_datas,
                    std::string destination,
                    StoredSource::Id source_id,
                    DestinationAttribute attribute) {
  auto [destination_data, _] =
      destination_datas.try_emplace(std::move(destination), DestinationData());
  destination_data->second.attribute =
      std::max(attribute, destination_data->second.attribute);
  destination_data->second.sources.push_back(source_id);
}

// Returns source IDs of the unselected destinations.
std::vector<StoredSource::Id> SelectDestinations(
    DestinationDataMap destination_datas,
    size_t destinations_allowed) {
  if (destination_datas.size() <= destinations_allowed) {
    return {};
  }

  // Currently the limit on production is 100 and the maximum size of
  // `destination_datas` is 100 + 3 (max destinations per source) = 103,
  // therefore it's more efficient to find the bottom destinations than the top
  // and delete the selected destinations.
  size_t to_select = destination_datas.size() - destinations_allowed;

  const auto cmp = [](const DestinationDataMap::node_type& a,
                      const DestinationDataMap::node_type& b) {
    return std::tie(a.mapped().attribute, a.key()) <
           std::tie(b.mapped().attribute, b.key());
  };

  std::vector<DestinationDataMap::node_type> selected;
  selected.reserve(to_select);

  while (!destination_datas.empty() && selected.size() < to_select) {
    selected.emplace_back(destination_datas.extract(destination_datas.begin()));
  }

  base::ranges::make_heap(selected, cmp);

  while (!destination_datas.empty()) {
    auto destination = destination_datas.extract(destination_datas.begin());

    if (cmp(destination, selected.front())) {
      base::ranges::pop_heap(selected, cmp);
      std::swap(selected.back(), destination);
      base::ranges::push_heap(selected, cmp);
    }
  }

  std::vector<StoredSource::Id> source_ids;

  for (auto& destination : selected) {
    std::move(destination.mapped()).Assign(source_ids);
  }

  DeduplicateSourceIds(source_ids);

  return source_ids;
}

}  // namespace

base::expected<std::vector<StoredSource::Id>, RateLimitTable::Error>
RateLimitTable::GetSourcesToDeactivateForDestinationLimit(
    sql::Database* db,
    const StorableSource& source,
    base::Time source_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DestinationDataMap destination_datas;

  for (const auto& destination :
       source.registration().destination_set.destinations()) {
    AddDestination(
        destination_datas, destination.Serialize(),
        StoredSource::Id(kUnsetRecordId),
        DestinationAttribute(source.registration().destination_limit_priority,
                             source_time));
  }

  // Check the number of unique destinations covered by all source registrations
  // whose [source_time, source_expiry_or_attribution_time] intersect with the
  // current source_time.
  sql::Statement statement(db->GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kRateLimitSourceAllowedSql));

  const CommonSourceInfo& common_info = source.common_info();
  statement.BindString(0, common_info.source_site().Serialize());
  statement.BindString(
      1, net::SchemefulSite(common_info.reporting_origin()).Serialize());
  statement.BindTime(2, source_time);

  while (statement.Step()) {
    const int64_t source_id = statement.ColumnInt64(3);
    // `source_id` should not be unset.
    // Note that this could occur in practice, e.g. with deliberate DB
    // modification or corruption, which would cause this to continue failing
    // until the offending row expires.
    if (source_id == kUnsetRecordId) {
      return base::unexpected(Error());
    }
    AddDestination(destination_datas, /*destination=*/statement.ColumnString(0),
                   StoredSource::Id(source_id),
                   DestinationAttribute(/*priority=*/statement.ColumnInt64(2),
                                        /*time=*/statement.ColumnTime(1)));
  }

  if (!statement.Succeeded()) {
    return base::unexpected(Error());
  }

  const int limit = delegate_->GetMaxDestinationsPerSourceSiteReportingSite();
  DCHECK_GT(limit, 0);

  return SelectDestinations(std::move(destination_datas), limit);
}

bool RateLimitTable::DeactivateSourcesForDestinationLimit(
    sql::Database* db,
    base::span<const StoredSource::Id> source_ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sql::Transaction transaction(db);
  if (!transaction.Begin()) {
    return false;
  }

  sql::Statement statement(db->GetCachedStatement(
      SQL_FROM_HERE,
      attribution_queries::kDeactivateForSourceDestinationLimitSql));

  for (StoredSource::Id id : source_ids) {
    statement.Reset(/*clear_bound_vars=*/true);
    statement.BindInt64(0, *id);
    if (!statement.Run()) {
      return false;
    }
  }

  return transaction.Commit();
}

RateLimitTable::DestinationRateLimitResult
RateLimitTable::SourceAllowedForDestinationRateLimit(
    sql::Database* db,
    const StorableSource& source,
    base::Time source_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sql::Statement statement(db->GetCachedStatement(
      SQL_FROM_HERE,
      attribution_queries::kRateLimitSourceAllowedDestinationRateLimitSql));

  AttributionConfig::DestinationRateLimit destination_rate_limit =
      delegate_->GetDestinationRateLimit();

  const CommonSourceInfo& common_info = source.common_info();
  statement.BindString(0, common_info.source_site().Serialize());
  statement.BindTime(1, source_time);
  statement.BindTime(2, source_time - destination_rate_limit.rate_limit_window);

  base::flat_set<net::SchemefulSite> destination_sites =
      source.registration().destination_set.destinations();
  base::flat_set<net::SchemefulSite> same_reporting_destination_sites =
      destination_sites;

  const std::string serialized_reporting_site =
      net::SchemefulSite(common_info.reporting_origin()).Serialize();

  while (statement.Step()) {
    net::SchemefulSite destination_site =
        net::SchemefulSite::Deserialize(statement.ColumnStringView(0));

    if (serialized_reporting_site == statement.ColumnStringView(1)) {
      same_reporting_destination_sites.insert(destination_site);
    }

    destination_sites.insert(std::move(destination_site));
  }

  if (!statement.Succeeded()) {
    return DestinationRateLimitResult::kError;
  }

  const int global_limit = destination_rate_limit.max_total;
  DCHECK_GT(global_limit, 0);

  const int reporting_limit = destination_rate_limit.max_per_reporting_site;
  DCHECK_GT(reporting_limit, 0);

  bool global_limit_hit =
      destination_sites.size() > static_cast<size_t>(global_limit);
  bool reporting_limit_hit = same_reporting_destination_sites.size() >
                             static_cast<size_t>(reporting_limit);

  if (global_limit_hit && reporting_limit_hit) {
    return DestinationRateLimitResult::kHitBothLimits;
  }

  if (!global_limit_hit && !reporting_limit_hit) {
    return DestinationRateLimitResult::kAllowed;
  }

  return global_limit_hit ? DestinationRateLimitResult::kHitGlobalLimit
                          : DestinationRateLimitResult::kHitReportingLimit;
}

RateLimitResult RateLimitTable::SourceAllowedForDestinationPerDayRateLimit(
    sql::Database* db,
    const StorableSource& source,
    base::Time source_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sql::Statement statement(db->GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::
                         kRateLimitSourceAllowedDestinationPerDayRateLimitSql));

  const CommonSourceInfo& common_info = source.common_info();
  statement.BindString(0, common_info.source_site().Serialize());
  statement.BindString(
      1, net::SchemefulSite(common_info.reporting_origin()).Serialize());
  statement.BindTime(2, source_time);
  statement.BindTime(
      3, source_time -
             AttributionConfig::DestinationRateLimit::kPerDayRateLimitWindow);

  const int limit =
      delegate_->GetDestinationRateLimit().max_per_reporting_site_per_day;
  DCHECK_GT(limit, 0);

  base::flat_set<net::SchemefulSite> destination_sites =
      source.registration().destination_set.destinations();

  while (statement.Step()) {
    destination_sites.insert(
        net::SchemefulSite::Deserialize(statement.ColumnStringView(0)));

    if (destination_sites.size() > static_cast<size_t>(limit)) {
      return RateLimitResult::kNotAllowed;
    }
  }

  return statement.Succeeded() ? RateLimitResult::kAllowed
                               : RateLimitResult::kError;
}

RateLimitResult RateLimitTable::AttributionAllowedForReportingOriginLimit(
    sql::Database* db,
    const AttributionInfo& attribution_info,
    const StoredSource& source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return AllowedForReportingOriginLimit(
      db, /*is_source=*/false, source.common_info(), attribution_info.time,
      {net::SchemefulSite(attribution_info.context_origin)});
}

RateLimitResult RateLimitTable::AllowedForReportingOriginLimit(
    sql::Database* db,
    bool is_source,
    const CommonSourceInfo& common_info,
    base::Time time,
    const base::flat_set<net::SchemefulSite>& destination_sites) {
  const AttributionConfig::RateLimitConfig& rate_limits =
      delegate_->GetRateLimits();
  DCHECK_GT(rate_limits.time_window, base::TimeDelta());

  sql::Statement statement;

  int64_t max;
  if (is_source) {
    max = rate_limits.max_source_registration_reporting_origins;
    statement.Assign(db->GetCachedStatement(
        SQL_FROM_HERE,
        attribution_queries::kRateLimitSelectSourceReportingOriginsSql));
  } else {
    max = rate_limits.max_attribution_reporting_origins;
    statement.Assign(db->GetCachedStatement(
        SQL_FROM_HERE,
        attribution_queries::kRateLimitSelectAttributionReportingOriginsSql));
  }
  DCHECK_GT(max, 0);

  const std::string serialized_reporting_origin =
      common_info.reporting_origin().Serialize();

  base::Time min_timestamp = time - rate_limits.time_window;

  statement.BindString(0, common_info.source_site().Serialize());
  statement.BindTime(2, min_timestamp);

  for (const auto& destination : destination_sites) {
    base::flat_set<std::string> reporting_origins;
    statement.Reset(/*clear_bound_vars=*/false);
    statement.BindString(1, destination.Serialize());

    while (statement.Step()) {
      std::string_view reporting_origin = statement.ColumnStringView(0);

      // The origin isn't new, so it doesn't change the count.
      if (reporting_origin == serialized_reporting_origin) {
        break;
      }

      // Note: In C++23 this can be `insert(origin)` instead to avoid copying
      // the string when the value is already contained.
      reporting_origins.insert(std::string(reporting_origin));

      if (reporting_origins.size() == static_cast<size_t>(max)) {
        return RateLimitResult::kNotAllowed;
      }
    }

    if (!statement.Succeeded()) {
      return RateLimitResult::kError;
    }
  }

  return RateLimitResult::kAllowed;
}

bool RateLimitTable::DeleteAttributionRateLimit(
    sql::Database* db,
    Scope scope,
    AttributionReport::Id report_id) {
  CHECK(IsAttribution(scope));

  sql::Statement statement(db->GetCachedStatement(
      SQL_FROM_HERE,
      attribution_queries::kDeleteAttributionRateLimitByReportIdSql));
  statement.BindInt(0, static_cast<int>(scope));
  statement.BindInt64(1, *report_id);
  return statement.Run();
}

bool RateLimitTable::ClearAllDataInRange(sql::Database* db,
                                         base::Time delete_begin,
                                         base::Time delete_end) {
  DCHECK(!((delete_begin.is_null() || delete_begin.is_min()) &&
           delete_end.is_max()));

  // TODO(linnan): Optimize using a more appropriate index.
  sql::Statement statement(db->GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kDeleteRateLimitRangeSql));
  statement.BindTime(0, delete_begin);
  statement.BindTime(1, delete_end);
  return statement.Run();
}

bool RateLimitTable::ClearAllDataAllTime(sql::Database* db) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  static constexpr char kDeleteAllRateLimitsSql[] = "DELETE FROM rate_limits";
  sql::Statement statement(
      db->GetCachedStatement(SQL_FROM_HERE, kDeleteAllRateLimitsSql));
  return statement.Run();
}

bool RateLimitTable::ClearDataForOriginsInRange(
    sql::Database* db,
    base::Time delete_begin,
    base::Time delete_end,
    StoragePartition::StorageKeyMatcherFunction filter) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (filter.is_null()) {
    return ClearAllDataInRange(db, delete_begin, delete_end);
  }

  static constexpr char kDeleteSql[] = "DELETE FROM rate_limits WHERE id=?";
  sql::Statement delete_statement(
      db->GetCachedStatement(SQL_FROM_HERE, kDeleteSql));

  sql::Transaction transaction(db);
  if (!transaction.Begin()) {
    return false;
  }

  // TODO(linnan): Optimize using a more appropriate index.
  sql::Statement select_statement(db->GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kSelectRateLimitsForDeletionSql));
  select_statement.BindTime(0, delete_begin);
  select_statement.BindTime(1, delete_end);

  while (select_statement.Step()) {
    int64_t rate_limit_id = select_statement.ColumnInt64(0);
    if (filter.Run(blink::StorageKey::CreateFirstParty(
            DeserializeOrigin(select_statement.ColumnStringView(1))))) {
      // See https://www.sqlite.org/isolation.html for why it's OK for this
      // DELETE to be interleaved in the surrounding SELECT.
      delete_statement.Reset(/*clear_bound_vars=*/false);
      delete_statement.BindInt64(0, rate_limit_id);
      if (!delete_statement.Run()) {
        return false;
      }
    }
  }

  if (!select_statement.Succeeded()) {
    return false;
  }

  return transaction.Commit();
}

bool RateLimitTable::DeleteExpiredRateLimits(sql::Database* db) {
  base::Time now = base::Time::Now();
  base::Time timestamp = now - delegate_->GetRateLimits().time_window;

  // Attribution rate limit entries can be deleted as long as their time falls
  // outside the rate limit window. For source entries, if the expiry time has
  // not passed, keep entries around to ensure
  // `SourceAllowedForDestinationLimit()` is computed properly.
  sql::Statement statement(db->GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kDeleteExpiredRateLimitsSql));
  statement.BindTime(0, timestamp);
  statement.BindTime(1, now);
  return statement.Run();
}

bool RateLimitTable::ClearDataForSourceIds(
    sql::Database* db,
    base::span<const StoredSource::Id> source_ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sql::Transaction transaction(db);
  if (!transaction.Begin()) {
    return false;
  }

  sql::Statement statement(db->GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kDeleteRateLimitsBySourceIdSql));

  for (StoredSource::Id id : source_ids) {
    statement.Reset(/*clear_bound_vars=*/true);
    statement.BindInt64(0, *id);
    if (!statement.Run()) {
      return false;
    }
  }

  return transaction.Commit();
}

void RateLimitTable::AppendRateLimitDataKeys(
    sql::Database* db,
    std::set<AttributionDataModel::DataKey>& keys) {
  sql::Statement statement(db->GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kGetRateLimitDataKeysSql));

  while (statement.Step()) {
    url::Origin reporting_origin =
        DeserializeOrigin(statement.ColumnStringView(0));
    if (reporting_origin.opaque()) {
      continue;
    }
    keys.emplace(std::move(reporting_origin));
  }
}

void RateLimitTable::SetDelegate(const AttributionResolverDelegate& delegate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_ = delegate;
}

}  // namespace content
