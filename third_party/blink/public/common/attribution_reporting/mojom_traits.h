// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_ATTRIBUTION_REPORTING_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_ATTRIBUTION_REPORTING_MOJOM_TRAITS_H_

#include <stdint.h>

#include <utility>

#include "base/time/time.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/os_registration.h"
#include "components/attribution_reporting/source_registration.h"
#include "components/attribution_reporting/suitable_origin.h"
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

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_ATTRIBUTION_REPORTING_MOJOM_TRAITS_H_
