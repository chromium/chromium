// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_data_host_manager_impl.h"

#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_aggregatable_source.h"
#include "content/browser/attribution_reporting/attribution_aggregatable_trigger.h"
#include "content/browser/attribution_reporting/attribution_filter_data.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/attribution_reporting.pb.h"
#include "content/browser/attribution_reporting/attribution_source_type.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/attribution_reporting/constants.h"
#include "url/origin.h"

namespace content {

namespace {

proto::AttributionAggregatableSource ConvertToProto(
    const blink::mojom::AttributionAggregatableSource& aggregatable_source) {
  proto::AttributionAggregatableSource result;

  for (const auto& [key_id, key_ptr] : aggregatable_source.keys) {
    DCHECK(key_ptr);
    proto::AttributionAggregatableKey key;
    key.set_high_bits(key_ptr->high_bits);
    key.set_low_bits(key_ptr->low_bits);

    (*result.mutable_keys())[key_id] = std::move(key);
  }

  return result;
}

}  // namespace

struct AttributionDataHostManagerImpl::FrozenContext {
  // Top-level origin the data host was created in.
  const url::Origin context_origin;

  // Source type of this context. Note that data hosts which result in
  // triggers still have a source type of` kEvent` as they share the same web
  // API surface.
  const AttributionSourceType source_type;

  // For receivers with `source_type` `AttributionSourceType::kNavigation`,
  // the final committed origin of the navigation associated with the data
  // host.
  //
  // For receivers with `source_type` `AttributionSourceType::kEvent`,
  // initialized to `absl::nullopt`. If the first call is to
  // `AttributionDataHostManagerImpl::SourceDataAvailable()`, set to the
  // source's destination. If the first call is to
  // `AttributionDataHostManagerImpl::TriggerDataAvailable()`, set to an opaque
  // origin.
  absl::optional<url::Origin> destination;

  int num_data_registered = 0;

  ~FrozenContext() {
    DCHECK_GE(num_data_registered, 0);

    if (num_data_registered == 0)
      return;

    DCHECK(destination.has_value());

    if (destination->opaque()) {
      base::UmaHistogramExactLinear("Conversions.RegisteredTriggersPerDataHost",
                                    num_data_registered, 101);
    } else {
      base::UmaHistogramExactLinear("Conversions.RegisteredSourcesPerDataHost",
                                    num_data_registered, 101);
    }
  }
};

AttributionDataHostManagerImpl::AttributionDataHostManagerImpl(
    AttributionManager* attribution_manager)
    : attribution_manager_(attribution_manager) {
  DCHECK(attribution_manager_);
}

AttributionDataHostManagerImpl::~AttributionDataHostManagerImpl() = default;

void AttributionDataHostManagerImpl::RegisterDataHost(
    mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host,
    url::Origin context_origin) {
  if (!network::IsOriginPotentiallyTrustworthy(context_origin))
    return;

  receivers_.Add(this, std::move(data_host),
                 FrozenContext{.context_origin = std::move(context_origin),
                               .source_type = AttributionSourceType::kEvent});
}

void AttributionDataHostManagerImpl::RegisterNavigationDataHost(
    mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host,
    const blink::AttributionSrcToken& attribution_src_token) {
  navigation_data_host_map_.emplace(attribution_src_token,
                                    std::move(data_host));
}

void AttributionDataHostManagerImpl::NotifyNavigationForDataHost(
    const blink::AttributionSrcToken& attribution_src_token,
    const url::Origin& source_origin,
    const url::Origin& destination_origin) {
  auto it = navigation_data_host_map_.find(attribution_src_token);

  // TODO(johnidel): Record metrics for how often this occurs.
  if (it == navigation_data_host_map_.end())
    return;

  receivers_.Add(
      this, std::move(it->second),
      FrozenContext{.context_origin = source_origin,
                    .source_type = AttributionSourceType::kNavigation,
                    .destination = destination_origin});

  navigation_data_host_map_.erase(it);
}

void AttributionDataHostManagerImpl::NotifyNavigationFailure(
    const blink::AttributionSrcToken& attribution_src_token) {
  // TODO(johnidel): Record metrics for how many potential sources are dropped.
  navigation_data_host_map_.erase(attribution_src_token);
}

void AttributionDataHostManagerImpl::SourceDataAvailable(
    blink::mojom::AttributionSourceDataPtr data) {
  // TODO(linnan): Log metrics for early returns.

  // The API is only allowed in secure contexts.
  if (!network::IsOriginPotentiallyTrustworthy(data->reporting_origin) ||
      !network::IsOriginPotentiallyTrustworthy(data->destination)) {
    return;
  }

  FrozenContext& context = receivers_.current_context();
  DCHECK(network::IsOriginPotentiallyTrustworthy(context.context_origin));

  switch (context.source_type) {
    case AttributionSourceType::kNavigation:
      DCHECK(context.destination.has_value());

      // For navigation sources verify the destination matches the final
      // navigation origin.
      if (net::SchemefulSite(data->destination) !=
          net::SchemefulSite(*context.destination)) {
        return;
      }
      break;
    case AttributionSourceType::kEvent:
      // For event sources verify that all sources are consistent.
      if (!context.destination.has_value()) {
        context.destination = data->destination;
      } else if (data->destination != *context.destination) {
        return;
      }
      break;
  }

  base::Time source_time = base::Time::Now();

  absl::optional<AttributionFilterData> filter_data =
      AttributionFilterData::FromSourceFilterValues(
          std::move(data->filter_data->filter_values));
  if (!filter_data.has_value())
    return;

  absl::optional<AttributionAggregatableSource> aggregatable_source =
      AttributionAggregatableSource::Create(
          ConvertToProto(*data->aggregatable_source));
  if (!aggregatable_source.has_value())
    return;

  context.num_data_registered++;

  StorableSource storable_source(CommonSourceInfo(
      data->source_event_id, context.context_origin,
      std::move(data->destination), std::move(data->reporting_origin),
      source_time,
      CommonSourceInfo::GetExpiryTime(data->expiry, source_time,
                                      context.source_type),
      context.source_type, data->priority, std::move(*filter_data),
      data->debug_key ? absl::make_optional(data->debug_key->value)
                      : absl::nullopt,
      std::move(*aggregatable_source)));

  attribution_manager_->HandleSource(std::move(storable_source));
}

void AttributionDataHostManagerImpl::TriggerDataAvailable(
    blink::mojom::AttributionTriggerDataPtr data) {
  // TODO(linnan): Log metrics for early returns.

  // The API is only allowed in secure contexts.
  if (!network::IsOriginPotentiallyTrustworthy(data->reporting_origin))
    return;

  FrozenContext& context = receivers_.current_context();
  DCHECK(network::IsOriginPotentiallyTrustworthy(context.context_origin));

  // Only possible in the case of a bad renderer, navigation bound data hosts
  // cannot register triggers.
  if (context.source_type == AttributionSourceType::kNavigation)
    return;

  if (!context.destination.has_value()) {
    context.destination = url::Origin();
  } else if (!context.destination->opaque()) {
    return;
  }

  absl::optional<AttributionFilterData> filters =
      AttributionFilterData::FromTriggerFilterValues(
          std::move(data->filters->filter_values));
  if (!filters.has_value())
    return;

  if (data->event_triggers.size() > blink::kMaxAttributionEventTriggerData)
    return;

  std::vector<AttributionTrigger::EventTriggerData> event_triggers;
  event_triggers.reserve(data->event_triggers.size());

  for (const auto& event_trigger : data->event_triggers) {
    absl::optional<AttributionFilterData> filters =
        AttributionFilterData::FromTriggerFilterValues(
            std::move(event_trigger->filters->filter_values));
    if (!filters.has_value())
      return;

    absl::optional<AttributionFilterData> not_filters =
        AttributionFilterData::FromTriggerFilterValues(
            std::move(event_trigger->not_filters->filter_values));
    if (!not_filters.has_value())
      return;

    event_triggers.emplace_back(
        event_trigger->data, event_trigger->priority,
        event_trigger->dedup_key
            ? absl::make_optional(event_trigger->dedup_key->value)
            : absl::nullopt,
        std::move(*filters), std::move(*not_filters));
  }

  absl::optional<AttributionAggregatableTrigger> aggregatable_trigger =
      AttributionAggregatableTrigger::FromMojo(
          std::move(data->aggregatable_trigger));
  if (!aggregatable_trigger.has_value())
    return;

  context.num_data_registered++;

  AttributionTrigger trigger(
      /*destination_origin=*/context.context_origin,
      std::move(data->reporting_origin), std::move(*filters),
      data->debug_key ? absl::make_optional(data->debug_key->value)
                      : absl::nullopt,
      std::move(event_triggers), std::move(*aggregatable_trigger));

  attribution_manager_->HandleTrigger(std::move(trigger));
}

}  // namespace content
