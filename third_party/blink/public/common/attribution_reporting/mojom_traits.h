// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_ATTRIBUTION_REPORTING_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_ATTRIBUTION_REPORTING_MOJOM_TRAITS_H_

#include <stdint.h>

#include <utility>

#include "base/time/time.h"
#include "components/attribution_reporting/aggregatable_trigger_data.h"
#include "components/attribution_reporting/aggregatable_values.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/event_trigger_data.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/os_registration.h"
#include "components/attribution_reporting/source_registration.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom-shared.h"
#include "url/gurl.h"
#include "url/mojom/origin_mojom_traits.h"
#include "url/mojom/url_gurl_mojom_traits.h"
#include "url/origin.h"

namespace mojo {

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::AttributionDebugKeyDataView, uint64_t> {
  static uint64_t value(uint64_t debug_key) { return debug_key; }

  static bool Read(blink::mojom::AttributionDebugKeyDataView data,
                   uint64_t* out) {
    *out = data.value();
    return true;
  }
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::AttributionSuitableOriginDataView,
                 attribution_reporting::SuitableOrigin> {
  static const url::Origin& origin(
      const attribution_reporting::SuitableOrigin& origin) {
    return *origin;
  }

  static bool Read(blink::mojom::AttributionSuitableOriginDataView data,
                   attribution_reporting::SuitableOrigin* out) {
    url::Origin origin;
    if (!data.ReadOrigin(&origin))
      return false;

    auto suitable_origin =
        attribution_reporting::SuitableOrigin::Create(std::move(origin));
    if (!suitable_origin)
      return false;

    *out = std::move(*suitable_origin);
    return true;
  }
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::AttributionTriggerDedupKeyDataView, uint64_t> {
  static uint64_t value(uint64_t debug_key) { return debug_key; }

  static bool Read(blink::mojom::AttributionTriggerDedupKeyDataView data,
                   uint64_t* out) {
    *out = data.value();
    return true;
  }
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::AttributionOsSourceDataView,
                 attribution_reporting::OsSource> {
  static const GURL& url(const attribution_reporting::OsSource& os_source) {
    return os_source.url();
  }

  // TODO(apaseltiner): Define this in a separate .cc file.
  static bool Read(blink::mojom::AttributionOsSourceDataView data,
                   attribution_reporting::OsSource* out) {
    GURL url;
    if (!data.ReadUrl(&url))
      return false;

    absl::optional<attribution_reporting::OsSource> os_source =
        attribution_reporting::OsSource::Create(std::move(url));
    if (!os_source.has_value())
      return false;

    *out = std::move(*os_source);
    return true;
  }
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::AttributionOsTriggerDataView,
                 attribution_reporting::OsTrigger> {
  static const GURL& url(const attribution_reporting::OsTrigger& os_trigger) {
    return os_trigger.url();
  }

  // TODO(apaseltiner): Define this in a separate .cc file.
  static bool Read(blink::mojom::AttributionOsTriggerDataView data,
                   attribution_reporting::OsTrigger* out) {
    GURL url;
    if (!data.ReadUrl(&url))
      return false;

    absl::optional<attribution_reporting::OsTrigger> os_trigger =
        attribution_reporting::OsTrigger::Create(std::move(url));
    if (!os_trigger.has_value())
      return false;

    *out = std::move(*os_trigger);
    return true;
  }
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::AttributionFilterDataDataView,
                 attribution_reporting::FilterData> {
  static const attribution_reporting::FilterValues& filter_values(
      const attribution_reporting::FilterData& filter_data) {
    return filter_data.filter_values();
  }

  // TODO(apaseltiner): Define this in a separate .cc file.
  static bool Read(blink::mojom::AttributionFilterDataDataView data,
                   attribution_reporting::FilterData* out) {
    attribution_reporting::FilterValues filter_values;
    if (!data.ReadFilterValues(&filter_values))
      return false;

    absl::optional<attribution_reporting::FilterData> filter_data =
        attribution_reporting::FilterData::Create(std::move(filter_values));
    if (!filter_data.has_value())
      return false;

    *out = std::move(*filter_data);
    return true;
  }
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::AttributionAggregationKeysDataView,
                 attribution_reporting::AggregationKeys> {
  static const attribution_reporting::AggregationKeys::Keys& keys(
      const attribution_reporting::AggregationKeys& aggregation_keys) {
    return aggregation_keys.keys();
  }

  // TODO(apaseltiner): Define this in a separate .cc file.
  static bool Read(blink::mojom::AttributionAggregationKeysDataView data,
                   attribution_reporting::AggregationKeys* out) {
    attribution_reporting::AggregationKeys::Keys keys;
    if (!data.ReadKeys(&keys))
      return false;

    absl::optional<attribution_reporting::AggregationKeys> aggregation_keys =
        attribution_reporting::AggregationKeys::FromKeys(std::move(keys));
    if (!aggregation_keys.has_value())
      return false;

    *out = std::move(*aggregation_keys);
    return true;
  }
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::AttributionSourceDataDataView,
                 attribution_reporting::SourceRegistration> {
  static const attribution_reporting::SuitableOrigin& destination(
      const attribution_reporting::SourceRegistration& source) {
    return source.destination;
  }

  static const attribution_reporting::SuitableOrigin& reporting_origin(
      const attribution_reporting::SourceRegistration& source) {
    return source.reporting_origin;
  }

  static uint64_t source_event_id(
      const attribution_reporting::SourceRegistration& source) {
    return source.source_event_id;
  }

  static absl::optional<base::TimeDelta> expiry(
      const attribution_reporting::SourceRegistration& source) {
    return source.expiry;
  }

  static absl::optional<base::TimeDelta> event_report_window(
      const attribution_reporting::SourceRegistration& source) {
    return source.event_report_window;
  }

  static absl::optional<base::TimeDelta> aggregatable_report_window(
      const attribution_reporting::SourceRegistration& source) {
    return source.aggregatable_report_window;
  }

  static int64_t priority(
      const attribution_reporting::SourceRegistration& source) {
    return source.priority;
  }

  static absl::optional<uint64_t> debug_key(
      const attribution_reporting::SourceRegistration& source) {
    return source.debug_key;
  }

  static const attribution_reporting::FilterData& filter_data(
      const attribution_reporting::SourceRegistration& source) {
    return source.filter_data;
  }

  static const attribution_reporting::AggregationKeys& aggregation_keys(
      const attribution_reporting::SourceRegistration& source) {
    return source.aggregation_keys;
  }

  static bool debug_reporting(
      const attribution_reporting::SourceRegistration& source) {
    return source.debug_reporting;
  }

  // TODO(apaseltiner): Define this in a separate .cc file.
  static bool Read(blink::mojom::AttributionSourceDataDataView data,
                   attribution_reporting::SourceRegistration* out) {
    if (!data.ReadDestination(&out->destination))
      return false;

    if (!data.ReadReportingOrigin(&out->reporting_origin))
      return false;

    if (!data.ReadExpiry(&out->expiry))
      return false;

    if (!data.ReadEventReportWindow(&out->event_report_window))
      return false;

    if (!data.ReadAggregatableReportWindow(&out->aggregatable_report_window))
      return false;

    if (!data.ReadDebugKey(&out->debug_key))
      return false;

    if (!data.ReadFilterData(&out->filter_data))
      return false;

    if (!data.ReadAggregationKeys(&out->aggregation_keys))
      return false;

    out->source_event_id = data.source_event_id();
    out->priority = data.priority();
    out->debug_reporting = data.debug_reporting();
    return true;
  }
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::AttributionFiltersDataView,
                 attribution_reporting::Filters> {
  static const attribution_reporting::FilterValues& filter_values(
      const attribution_reporting::Filters& filters) {
    return filters.filter_values();
  }

  static bool Read(blink::mojom::AttributionFiltersDataView data,
                   attribution_reporting::Filters* out) {
    attribution_reporting::FilterValues filter_values;
    if (!data.ReadFilterValues(&filter_values))
      return false;

    absl::optional<attribution_reporting::Filters> filters =
        attribution_reporting::Filters::Create(std::move(filter_values));
    if (!filters.has_value())
      return false;

    *out = std::move(*filters);
    return true;
  }
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::EventTriggerDataDataView,
                 attribution_reporting::EventTriggerData> {
  static uint64_t data(const attribution_reporting::EventTriggerData& data) {
    return data.data;
  }

  static int64_t priority(const attribution_reporting::EventTriggerData& data) {
    return data.priority;
  }

  static absl::optional<uint64_t> dedup_key(
      const attribution_reporting::EventTriggerData& data) {
    return data.dedup_key;
  }

  static const attribution_reporting::Filters& filters(
      const attribution_reporting::EventTriggerData& data) {
    return data.filters;
  }

  static const attribution_reporting::Filters& not_filters(
      const attribution_reporting::EventTriggerData& data) {
    return data.not_filters;
  }

  static bool Read(blink::mojom::EventTriggerDataDataView data,
                   attribution_reporting::EventTriggerData* out) {
    if (!data.ReadDedupKey(&out->dedup_key))
      return false;

    if (!data.ReadFilters(&out->filters))
      return false;

    if (!data.ReadNotFilters(&out->not_filters))
      return false;

    out->data = data.data();
    out->priority = data.priority();
    return true;
  }
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::AttributionAggregatableTriggerDataDataView,
                 attribution_reporting::AggregatableTriggerData> {
  static absl::uint128 key_piece(
      const attribution_reporting::AggregatableTriggerData& data) {
    return data.key_piece();
  }

  static const attribution_reporting::AggregatableTriggerData::Keys&
  source_keys(const attribution_reporting::AggregatableTriggerData& data) {
    return data.source_keys();
  }

  static const attribution_reporting::Filters& filters(
      const attribution_reporting::AggregatableTriggerData& data) {
    return data.filters();
  }

  static const attribution_reporting::Filters& not_filters(
      const attribution_reporting::AggregatableTriggerData& data) {
    return data.not_filters();
  }

  static bool Read(
      blink::mojom::AttributionAggregatableTriggerDataDataView data,
      attribution_reporting::AggregatableTriggerData* out) {
    absl::uint128 key_piece;
    if (!data.ReadKeyPiece(&key_piece))
      return false;

    attribution_reporting::AggregatableTriggerData::Keys source_keys;
    if (!data.ReadSourceKeys(&source_keys))
      return false;

    attribution_reporting::Filters filters;
    if (!data.ReadFilters(&filters))
      return false;

    attribution_reporting::Filters not_filters;
    if (!data.ReadNotFilters(&not_filters))
      return false;

    auto aggregatable_trigger_data =
        attribution_reporting::AggregatableTriggerData::Create(
            key_piece, std::move(source_keys), std::move(filters),
            std::move(not_filters));
    if (!aggregatable_trigger_data)
      return false;

    *out = std::move(*aggregatable_trigger_data);
    return true;
  }
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::AttributionTriggerDataDataView,
                 attribution_reporting::TriggerRegistration> {
  static const attribution_reporting::SuitableOrigin& reporting_origin(
      const attribution_reporting::TriggerRegistration& trigger) {
    return trigger.reporting_origin;
  }

  static const std::vector<attribution_reporting::EventTriggerData>&
  event_triggers(const attribution_reporting::TriggerRegistration& trigger) {
    return trigger.event_triggers.vec();
  }

  static const attribution_reporting::Filters& filters(
      const attribution_reporting::TriggerRegistration& trigger) {
    return trigger.filters;
  }

  static const attribution_reporting::Filters& not_filters(
      const attribution_reporting::TriggerRegistration& trigger) {
    return trigger.not_filters;
  }

  static const std::vector<attribution_reporting::AggregatableTriggerData>&
  aggregatable_trigger_data(
      const attribution_reporting::TriggerRegistration& trigger) {
    return trigger.aggregatable_trigger_data.vec();
  }

  static const attribution_reporting::AggregatableValues::Values&
  aggregatable_values(
      const attribution_reporting::TriggerRegistration& trigger) {
    return trigger.aggregatable_values.values();
  }

  static absl::optional<uint64_t> debug_key(
      const attribution_reporting::TriggerRegistration& trigger) {
    return trigger.debug_key;
  }

  static absl::optional<uint64_t> aggregatable_dedup_key(
      const attribution_reporting::TriggerRegistration& trigger) {
    return trigger.aggregatable_dedup_key;
  }

  static bool debug_reporting(
      const attribution_reporting::TriggerRegistration& trigger) {
    return trigger.debug_reporting;
  }

  static bool Read(blink::mojom::AttributionTriggerDataDataView data,
                   attribution_reporting::TriggerRegistration* out) {
    if (!data.ReadReportingOrigin(&out->reporting_origin))
      return false;

    std::vector<attribution_reporting::EventTriggerData> event_triggers;
    if (!data.ReadEventTriggers(&event_triggers))
      return false;

    auto event_triggers_list =
        attribution_reporting::EventTriggerDataList::Create(
            std::move(event_triggers));
    if (!event_triggers_list)
      return false;

    out->event_triggers = std::move(*event_triggers_list);

    if (!data.ReadFilters(&out->filters))
      return false;

    if (!data.ReadNotFilters(&out->not_filters))
      return false;

    std::vector<attribution_reporting::AggregatableTriggerData>
        aggregatable_trigger_data;
    if (!data.ReadAggregatableTriggerData(&aggregatable_trigger_data))
      return false;

    auto aggregatable_trigger_data_list =
        attribution_reporting::AggregatableTriggerDataList::Create(
            std::move(aggregatable_trigger_data));
    if (!aggregatable_trigger_data_list)
      return false;

    out->aggregatable_trigger_data = std::move(*aggregatable_trigger_data_list);

    attribution_reporting::AggregatableValues::Values values;
    if (!data.ReadAggregatableValues(&values))
      return false;

    auto aggregatable_values =
        attribution_reporting::AggregatableValues::Create(std::move(values));
    if (!aggregatable_values)
      return false;

    out->aggregatable_values = std::move(*aggregatable_values);

    if (!data.ReadDebugKey(&out->debug_key))
      return false;

    if (!data.ReadAggregatableDedupKey(&out->aggregatable_dedup_key))
      return false;

    out->debug_reporting = data.debug_reporting();
    return true;
  }
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_ATTRIBUTION_REPORTING_MOJOM_TRAITS_H_
