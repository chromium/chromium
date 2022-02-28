// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_data_host_manager_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_aggregatable_sources.h"
#include "content/browser/attribution_reporting/attribution_host_utils.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/attribution_reporting.pb.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/attribution_reporting/constants.h"
#include "url/origin.h"

namespace content {

namespace {

bool IsFilterDataValid(const blink::mojom::AttributionFilterData& filter_data) {
  if (filter_data.filter_values.size() > blink::kMaxAttributionFiltersPerSource)
    return false;

  for (const auto& [filter, values] : filter_data.filter_values) {
    if (filter.size() > blink::kMaxBytesPerAttributionFilterString)
      return false;

    if (values.size() > blink::kMaxValuesPerAttributionFilter)
      return false;

    for (const auto& value : values) {
      if (value.size() > blink::kMaxBytesPerAttributionFilterString)
        return false;
    }
  }

  return true;
}

proto::AttributionAggregatableSources ConvertToProto(
    const blink::mojom::AttributionAggregatableSources& aggregatable_sources) {
  proto::AttributionAggregatableSources result;

  for (const auto& [key_id, key_ptr] : aggregatable_sources.sources) {
    DCHECK(key_ptr);
    proto::AttributionAggregatableKey key;
    key.set_high_bits(key_ptr->high_bits);
    key.set_low_bits(key_ptr->low_bits);

    (*result.mutable_sources())[key_id] = std::move(key);
  }

  return result;
}

}  // namespace

AttributionDataHostManagerImpl::AttributionDataHostManagerImpl(
    BrowserContext* browser_context,
    AttributionManager* attribution_manager)
    : browser_context_(browser_context),
      attribution_manager_(attribution_manager) {
  DCHECK(browser_context_);
  DCHECK(attribution_manager_);

  // It's safe to use `base::Unretained()` as `receivers_` is owned by `this`
  // and will be deleted before `this`.
  receivers_.set_disconnect_handler(base::BindRepeating(
      &AttributionDataHostManagerImpl::OnDataHostDisconnected,
      base::Unretained(this)));
}

AttributionDataHostManagerImpl::~AttributionDataHostManagerImpl() = default;

void AttributionDataHostManagerImpl::RegisterDataHost(
    mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host,
    url::Origin context_origin) {
  receivers_.Add(
      this, std::move(data_host),
      FrozenContext{.context_origin = std::move(context_origin),
                    .source_type = CommonSourceInfo::SourceType::kEvent});
}

void AttributionDataHostManagerImpl::SourceDataAvailable(
    blink::mojom::AttributionSourceDataPtr data) {
  // TODO(linnan): Log metrics for early returns.

  auto result = receiver_source_destinations_.emplace(
      receivers_.current_receiver(), data->destination);
  if (!result.second && data->destination != result.first->second)
    return;

  const FrozenContext& context = receivers_.current_context();
  base::Time source_time = base::Time::Now();
  const url::Origin& reporting_origin = data->reporting_origin;

  const bool allowed =
      GetContentClient()->browser()->IsConversionMeasurementOperationAllowed(
          browser_context_,
          ContentBrowserClient::ConversionMeasurementOperation::kImpression,
          &context.context_origin, /*conversion_origin=*/nullptr,
          &reporting_origin);
  if (!allowed)
    return;

  // The API is only allowed in secure contexts.
  if (!attribution_host_utils::IsOriginTrustworthyForAttributions(
          context.context_origin) ||
      !attribution_host_utils::IsOriginTrustworthyForAttributions(
          reporting_origin) ||
      !attribution_host_utils::IsOriginTrustworthyForAttributions(
          data->destination)) {
    return;
  }

  if (!IsFilterDataValid(*data->filter_data))
    return;

  absl::optional<AttributionAggregatableSources> aggregatable_sources =
      AttributionAggregatableSources::Create(
          ConvertToProto(*data->aggregatable_sources));
  if (!aggregatable_sources.has_value())
    return;

  StorableSource storable_source(CommonSourceInfo(
      data->source_event_id, context.context_origin, data->destination,
      reporting_origin, source_time,
      CommonSourceInfo::GetExpiryTime(data->expiry, source_time,
                                      context.source_type),
      context.source_type, data->priority,
      data->debug_key ? absl::make_optional(data->debug_key->value)
                      : absl::nullopt,
      std::move(*aggregatable_sources)));

  attribution_manager_->HandleSource(std::move(storable_source));
}

void AttributionDataHostManagerImpl::OnDataHostDisconnected() {
  receiver_source_destinations_.erase(receivers_.current_receiver());
}

}  // namespace content
