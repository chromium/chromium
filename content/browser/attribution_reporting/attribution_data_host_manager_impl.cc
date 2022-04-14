// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_data_host_manager_impl.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/metrics/field_trial_params.h"
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
#include "third_party/blink/public/common/features.h"
#include "url/origin.h"

namespace content {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class TriggerQueueEvent {
  kSkippedQueue = 0,
  kDropped = 1,
  kEnqueued = 2,
  kProcessedWithDelay = 3,
  kFlushed = 4,

  kMaxValue = kFlushed,
};

void RecordTriggerQueueEvent(TriggerQueueEvent event) {
  base::UmaHistogramEnumeration("Conversions.TriggerQueueEvents", event);
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class DataHandleStatus {
  kSuccess = 0,
  kUntrustworthyOrigin = 1,
  kContextError = 2,
  kInvalidData = 3,

  kMaxValue = kInvalidData,
};

void RecordSourceDataHandleStatus(DataHandleStatus status) {
  base::UmaHistogramEnumeration("Conversions.SourceDataHandleStatus", status);
}

void RecordTriggerDataHandleStatus(DataHandleStatus status) {
  base::UmaHistogramEnumeration("Conversions.TriggerDataHandleStatus", status);
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class NavigationDataHostStatus {
  kRegistered = 0,
  kNotFound = 1,
  kNavigationFailed = 2,
  kProcessed = 3,

  kMaxValue = kProcessed,
};

void RecordNavigationDataHostStatus(NavigationDataHostStatus event) {
  base::UmaHistogramEnumeration("Conversions.NavigationDataHostStatus", event);
}

const base::FeatureParam<base::TimeDelta> kTriggerDelay{
    &blink::features::kConversionMeasurement, "trigger_delay",
    base::Seconds(5)};

constexpr size_t kMaxDelayedTriggers = 30;

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

  const base::TimeTicks register_time;
};

struct AttributionDataHostManagerImpl::DelayedTrigger {
  const base::TimeTicks delay_until;
  AttributionTrigger trigger;

  base::TimeDelta TimeUntil() const {
    return delay_until - base::TimeTicks::Now();
  }

  void RecordDelay() const {
    base::TimeTicks original_time = delay_until - kTriggerDelay.Get();
    base::UmaHistogramMediumTimes("Conversions.TriggerQueueDelay",
                                  base::TimeTicks::Now() - original_time);
  }
};

struct AttributionDataHostManagerImpl::NavigationDataHost {
  mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host;
  base::TimeTicks register_time;
};

AttributionDataHostManagerImpl::AttributionDataHostManagerImpl(
    AttributionManager* attribution_manager)
    : attribution_manager_(attribution_manager) {
  DCHECK(attribution_manager_);

  receivers_.set_disconnect_handler(base::BindRepeating(
      &AttributionDataHostManagerImpl::OnReceiverDisconnected,
      base::Unretained(this)));
}

AttributionDataHostManagerImpl::~AttributionDataHostManagerImpl() = default;

void AttributionDataHostManagerImpl::RegisterDataHost(
    mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host,
    url::Origin context_origin) {
  if (!network::IsOriginPotentiallyTrustworthy(context_origin))
    return;

  receivers_.Add(this, std::move(data_host),
                 FrozenContext{.context_origin = std::move(context_origin),
                               .source_type = AttributionSourceType::kEvent,
                               .register_time = base::TimeTicks::Now()});
  data_hosts_in_source_mode_++;
}

void AttributionDataHostManagerImpl::RegisterNavigationDataHost(
    mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host,
    const blink::AttributionSrcToken& attribution_src_token) {
  navigation_data_host_map_.emplace(
      attribution_src_token,
      NavigationDataHost{.data_host = std::move(data_host),
                         .register_time = base::TimeTicks::Now()});
  data_hosts_in_source_mode_++;

  RecordNavigationDataHostStatus(NavigationDataHostStatus::kRegistered);
}

void AttributionDataHostManagerImpl::NotifyNavigationForDataHost(
    const blink::AttributionSrcToken& attribution_src_token,
    const url::Origin& source_origin,
    const url::Origin& destination_origin) {
  if (!network::IsOriginPotentiallyTrustworthy(source_origin) ||
      !network::IsOriginPotentiallyTrustworthy(destination_origin)) {
    NotifyNavigationFailure(attribution_src_token);
    return;
  }

  auto it = navigation_data_host_map_.find(attribution_src_token);

  if (it == navigation_data_host_map_.end()) {
    RecordNavigationDataHostStatus(NavigationDataHostStatus::kNotFound);
    return;
  }

  receivers_.Add(
      this, std::move(it->second.data_host),
      FrozenContext{.context_origin = source_origin,
                    .source_type = AttributionSourceType::kNavigation,
                    .destination = destination_origin,
                    .register_time = it->second.register_time});

  navigation_data_host_map_.erase(it);

  RecordNavigationDataHostStatus(NavigationDataHostStatus::kProcessed);
}

void AttributionDataHostManagerImpl::NotifyNavigationFailure(
    const blink::AttributionSrcToken& attribution_src_token) {
  auto it = navigation_data_host_map_.find(attribution_src_token);
  DCHECK(it != navigation_data_host_map_.end());

  base::TimeTicks register_time = it->second.register_time;
  navigation_data_host_map_.erase(it);
  OnSourceEligibleDataHostFinished(register_time);

  RecordNavigationDataHostStatus(NavigationDataHostStatus::kNavigationFailed);
}

void AttributionDataHostManagerImpl::SourceDataAvailable(
    blink::mojom::AttributionSourceDataPtr data) {
  // The API is only allowed in secure contexts.
  if (!network::IsOriginPotentiallyTrustworthy(data->reporting_origin) ||
      !network::IsOriginPotentiallyTrustworthy(data->destination)) {
    RecordSourceDataHandleStatus(DataHandleStatus::kUntrustworthyOrigin);
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
        RecordSourceDataHandleStatus(DataHandleStatus::kContextError);
        return;
      }
      break;
    case AttributionSourceType::kEvent:
      // For event sources verify that all sources are consistent.
      if (!context.destination.has_value()) {
        context.destination = data->destination;
      } else if (data->destination != *context.destination) {
        RecordSourceDataHandleStatus(DataHandleStatus::kContextError);
        return;
      }
      break;
  }

  base::Time source_time = base::Time::Now();

  absl::optional<AttributionFilterData> filter_data =
      AttributionFilterData::FromSourceFilterValues(
          std::move(data->filter_data->filter_values));
  if (!filter_data.has_value()) {
    RecordSourceDataHandleStatus(DataHandleStatus::kInvalidData);
    return;
  }

  absl::optional<AttributionAggregatableSource> aggregatable_source =
      AttributionAggregatableSource::Create(
          ConvertToProto(*data->aggregatable_source));
  if (!aggregatable_source.has_value()) {
    RecordSourceDataHandleStatus(DataHandleStatus::kInvalidData);
    return;
  }

  RecordSourceDataHandleStatus(DataHandleStatus::kSuccess);

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
  // The API is only allowed in secure contexts.
  if (!network::IsOriginPotentiallyTrustworthy(data->reporting_origin)) {
    RecordTriggerDataHandleStatus(DataHandleStatus::kUntrustworthyOrigin);
    return;
  }

  FrozenContext& context = receivers_.current_context();
  DCHECK(network::IsOriginPotentiallyTrustworthy(context.context_origin));

  // Only possible in the case of a bad renderer, navigation bound data hosts
  // cannot register triggers.
  if (context.source_type == AttributionSourceType::kNavigation) {
    RecordTriggerDataHandleStatus(DataHandleStatus::kContextError);
    return;
  }

  if (!context.destination.has_value()) {
    context.destination = url::Origin();
    OnSourceEligibleDataHostFinished(context.register_time);
  } else if (!context.destination->opaque()) {
    RecordTriggerDataHandleStatus(DataHandleStatus::kContextError);
    return;
  }

  absl::optional<AttributionFilterData> filters =
      AttributionFilterData::FromTriggerFilterValues(
          std::move(data->filters->filter_values));
  if (!filters.has_value()) {
    RecordTriggerDataHandleStatus(DataHandleStatus::kInvalidData);
    return;
  }

  if (data->event_triggers.size() > blink::kMaxAttributionEventTriggerData) {
    RecordTriggerDataHandleStatus(DataHandleStatus::kInvalidData);
    return;
  }

  std::vector<AttributionTrigger::EventTriggerData> event_triggers;
  event_triggers.reserve(data->event_triggers.size());

  for (const auto& event_trigger : data->event_triggers) {
    absl::optional<AttributionFilterData> filters =
        AttributionFilterData::FromTriggerFilterValues(
            std::move(event_trigger->filters->filter_values));
    if (!filters.has_value()) {
      RecordTriggerDataHandleStatus(DataHandleStatus::kInvalidData);
      return;
    }

    absl::optional<AttributionFilterData> not_filters =
        AttributionFilterData::FromTriggerFilterValues(
            std::move(event_trigger->not_filters->filter_values));
    if (!not_filters.has_value()) {
      RecordTriggerDataHandleStatus(DataHandleStatus::kInvalidData);
      return;
    }

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
  if (!aggregatable_trigger.has_value()) {
    RecordTriggerDataHandleStatus(DataHandleStatus::kInvalidData);
    return;
  }

  RecordTriggerDataHandleStatus(DataHandleStatus::kSuccess);

  context.num_data_registered++;

  AttributionTrigger trigger(
      /*destination_origin=*/context.context_origin,
      std::move(data->reporting_origin), std::move(*filters),
      data->debug_key ? absl::make_optional(data->debug_key->value)
                      : absl::nullopt,
      std::move(event_triggers), std::move(*aggregatable_trigger));

  // Handle the trigger immediately if we're not waiting for any sources to be
  // registered.
  if (data_hosts_in_source_mode_ == 0) {
    DCHECK(delayed_triggers_.empty());
    RecordTriggerQueueEvent(TriggerQueueEvent::kSkippedQueue);
    attribution_manager_->HandleTrigger(std::move(trigger));
    return;
  }

  // Otherwise, buffer triggers for `kTriggerDelay` if we haven't exceeded the
  // maximum queue length. This gives sources time to be registered prior to
  // attribution, which helps ensure that navigation sources are stored before
  // attribution occurs on the navigation destination. Note that this is not a
  // complete fix, as sources taking longer to register than `kTriggerDelay`
  // will still fail to be found during attribution.
  //
  // TODO(crbug.com/1309173): Implement a better solution to this problem.

  if (delayed_triggers_.size() >= kMaxDelayedTriggers) {
    RecordTriggerQueueEvent(TriggerQueueEvent::kDropped);
    return;
  }

  const base::TimeDelta delay = kTriggerDelay.Get();

  delayed_triggers_.emplace_back(DelayedTrigger{
      .delay_until = base::TimeTicks::Now() + delay,
      .trigger = std::move(trigger),
  });
  RecordTriggerQueueEvent(TriggerQueueEvent::kEnqueued);

  if (!trigger_timer_.IsRunning())
    SetTriggerTimer(delay);
}

void AttributionDataHostManagerImpl::SetTriggerTimer(base::TimeDelta delay) {
  DCHECK(!delayed_triggers_.empty());
  trigger_timer_.Start(FROM_HERE, delay, this,
                       &AttributionDataHostManagerImpl::ProcessDelayedTrigger);
}

void AttributionDataHostManagerImpl::ProcessDelayedTrigger() {
  DCHECK(!delayed_triggers_.empty());

  DelayedTrigger delayed_trigger = std::move(delayed_triggers_.front());
  delayed_triggers_.pop_front();
  DCHECK_LE(delayed_trigger.delay_until, base::TimeTicks::Now());

  attribution_manager_->HandleTrigger(std::move(delayed_trigger.trigger));
  RecordTriggerQueueEvent(TriggerQueueEvent::kProcessedWithDelay);
  delayed_trigger.RecordDelay();

  if (!delayed_triggers_.empty()) {
    base::TimeDelta delay = delayed_triggers_.front().TimeUntil();
    SetTriggerTimer(delay);
  }
}

void AttributionDataHostManagerImpl::OnReceiverDisconnected() {
  const FrozenContext& context = receivers_.current_context();

  DCHECK_GE(context.num_data_registered, 0);

  if (context.num_data_registered > 0) {
    DCHECK(context.destination.has_value());

    if (context.destination->opaque()) {
      base::UmaHistogramExactLinear("Conversions.RegisteredTriggersPerDataHost",
                                    context.num_data_registered, 101);
    } else {
      base::UmaHistogramExactLinear("Conversions.RegisteredSourcesPerDataHost",
                                    context.num_data_registered, 101);
    }
  }

  // If the receiver was handling triggers, there's nothing to do here.
  if (context.destination.has_value() && context.destination->opaque())
    return;

  OnSourceEligibleDataHostFinished(context.register_time);
}

void AttributionDataHostManagerImpl::OnSourceEligibleDataHostFinished(
    base::TimeTicks register_time) {
  // Decrement the number of receivers in source mode and flush triggers if
  // applicable.
  //
  // Note that flushing is best-effort.
  // Sources/triggers which are registered after the trigger count towards this
  // limit as well, but that is intentional to keep this simple.
  //
  // TODO(apaseltiner): Should we flush triggers when the
  // `AttributionDataHostManagerImpl` is about to be destroyed?

  base::UmaHistogramMediumTimes("Conversions.SourceEligibleDataHostLifeTime",
                                base::TimeTicks::Now() - register_time);

  DCHECK_GT(data_hosts_in_source_mode_, 0u);
  data_hosts_in_source_mode_--;
  if (data_hosts_in_source_mode_ > 0)
    return;

  trigger_timer_.Stop();

  // Process triggers synchronously. This is OK, because the current
  // `kMaxDelayedTriggers` of 30 is relatively small and the attribution manager
  // only does a small amount of work and then posts a task to a different
  // sequence.
  static_assert(kMaxDelayedTriggers <= 30,
                "Consider using PostTask instead of handling triggers "
                "synchronously to avoid blocking for too long.");

  for (auto& delayed_trigger : delayed_triggers_) {
    attribution_manager_->HandleTrigger(std::move(delayed_trigger.trigger));
    RecordTriggerQueueEvent(TriggerQueueEvent::kFlushed);
    delayed_trigger.RecordDelay();
  }

  delayed_triggers_.clear();
}

}  // namespace content
