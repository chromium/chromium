// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_data_host_manager_impl.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/flat_tree.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_aggregatable_trigger_data.h"
#include "content/browser/attribution_reporting/attribution_aggregatable_values.h"
#include "content/browser/attribution_reporting/attribution_aggregation_keys.h"
#include "content/browser/attribution_reporting/attribution_filter_data.h"
#include "content/browser/attribution_reporting/attribution_header_utils.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/attribution_source_type.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
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

void ReportBadMessageInsecureReportingOrigin() {
  mojo::ReportBadMessage(
      "AttributionDataHost: Reporting origin must be secure.");
}

absl::optional<std::vector<AttributionAggregatableTriggerData>> FromMojo(
    std::vector<blink::mojom::AttributionAggregatableTriggerDataPtr> mojo) {
  if (mojo.size() > blink::kMaxAttributionAggregatableTriggerDataPerTrigger)
    return absl::nullopt;

  std::vector<AttributionAggregatableTriggerData> aggregatable_trigger_data;
  aggregatable_trigger_data.reserve(mojo.size());

  for (auto& aggregatable_trigger : mojo) {
    absl::optional<AttributionFilterData> filters =
        AttributionFilterData::FromTriggerFilterValues(
            std::move(aggregatable_trigger->filters->filter_values));
    if (!filters.has_value())
      return absl::nullopt;

    absl::optional<AttributionFilterData> not_filters =
        AttributionFilterData::FromTriggerFilterValues(
            std::move(aggregatable_trigger->not_filters->filter_values));
    if (!not_filters.has_value())
      return absl::nullopt;

    absl::optional<AttributionAggregatableTriggerData> data =
        AttributionAggregatableTriggerData::Create(
            aggregatable_trigger->key_piece,
            std::move(aggregatable_trigger->source_keys), std::move(*filters),
            std::move(*not_filters));
    if (!data.has_value())
      return absl::nullopt;

    aggregatable_trigger_data.push_back(std::move(*data));
  }

  return aggregatable_trigger_data;
}

enum class RegistrationType {
  kNone,
  kSource,
  kTrigger,
};

}  // namespace

struct AttributionDataHostManagerImpl::FrozenContext {
  // Top-level origin the data host was created in.
  // Logically const.
  url::Origin context_origin;

  // Source type of this context. Note that data hosts which result in
  // triggers still have a source type of` kEvent` as they share the same web
  // API surface.
  // Logically const.
  AttributionSourceType source_type;

  // For receivers with `source_type` `AttributionSourceType::kNavigation`,
  // the final committed site of the navigation associated with the data
  // host.
  //
  // For receivers with `source_type` `AttributionSourceType::kEvent`,
  // this is opaque by default.
  net::SchemefulSite destination;

  RegistrationType registration_type = RegistrationType::kNone;

  int num_data_registered = 0;

  // Logically const.
  base::TimeTicks register_time;
};

struct AttributionDataHostManagerImpl::DelayedTrigger {
  // Logically const.
  base::TimeTicks delay_until;

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

struct AttributionDataHostManagerImpl::NavigationRedirectSourceRegistrations {
  // Source origin to use for all registrations on a redirect chain. Will not
  // change over the course of the redirect chain.
  url::Origin source_origin;

  // Number of source data we are waiting to be decoded/received.
  size_t pending_source_data = 0;

  // Source data that has been received as part of this redirect chain. Sources
  // cannot be processed until `destination` is set.
  std::vector<StorableSource> sources;

  // The final, committed destination of the navigation associated with this.
  // This can be set before or after all `pending_source_data` is received.
  net::SchemefulSite destination;

  // The time the first registration header was received for the redirect chain.
  // Will not change over the course of the redirect chain.
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
  DCHECK(network::IsOriginPotentiallyTrustworthy(context_origin));

  receivers_.Add(this, std::move(data_host),
                 FrozenContext{.context_origin = std::move(context_origin),
                               .source_type = AttributionSourceType::kEvent,
                               .register_time = base::TimeTicks::Now()});
  data_hosts_in_source_mode_++;
}

bool AttributionDataHostManagerImpl::RegisterNavigationDataHost(
    mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host,
    const blink::AttributionSrcToken& attribution_src_token) {
  auto [it, inserted] = navigation_data_host_map_.try_emplace(
      attribution_src_token,
      NavigationDataHost{.data_host = std::move(data_host),
                         .register_time = base::TimeTicks::Now()});
  // Should only be possible with a misbehaving renderer.
  if (!inserted)
    return false;

  data_hosts_in_source_mode_++;

  RecordNavigationDataHostStatus(NavigationDataHostStatus::kRegistered);
  return true;
}

void AttributionDataHostManagerImpl::NotifyNavigationRedirectRegistation(
    const blink::AttributionSrcToken& attribution_src_token,
    const std::string& header_value,
    url::Origin reporting_origin,
    const url::Origin& source_origin) {
  if (!network::IsOriginPotentiallyTrustworthy(source_origin) ||
      !network::IsOriginPotentiallyTrustworthy(reporting_origin)) {
    return;
  }

  // Avoid costly isolated JSON parsing below if the header is obviously
  // invalid.
  if (header_value.empty())
    return;

  auto [it, inserted] = redirect_registrations_.try_emplace(
      attribution_src_token, NavigationRedirectSourceRegistrations{
                                 .source_origin = source_origin,
                                 .register_time = base::TimeTicks::Now()});

  // Redirect data may not be registered if the navigation is already finished.
  DCHECK(it->second.destination.opaque());

  // Treat ongoing redirect registrations within a chain as a data host for the
  // purpose of trigger queuing.
  if (inserted)
    data_hosts_in_source_mode_++;

  it->second.pending_source_data++;

  // Send the data to the decoder, but track that we are now waiting on a new
  // registration.
  data_decoder::DataDecoder::ParseJsonIsolated(
      header_value,
      base::BindOnce(&AttributionDataHostManagerImpl::OnRedirectSourceParsed,
                     weak_factory_.GetWeakPtr(), attribution_src_token,
                     std::move(reporting_origin)));
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

  if (it != navigation_data_host_map_.end()) {
    receivers_.Add(
        this, std::move(it->second.data_host),
        FrozenContext{.context_origin = source_origin,
                      .source_type = AttributionSourceType::kNavigation,
                      .destination = net::SchemefulSite(destination_origin),
                      .register_time = it->second.register_time});

    navigation_data_host_map_.erase(it);
    RecordNavigationDataHostStatus(NavigationDataHostStatus::kProcessed);
  } else {
    RecordNavigationDataHostStatus(NavigationDataHostStatus::kNotFound);
  }

  // Process any registrations on redirects for this navigation now that we know
  // the destination.
  auto redirect_it = redirect_registrations_.find(attribution_src_token);
  if (redirect_it == redirect_registrations_.end()) {
    return;
  }
  NavigationRedirectSourceRegistrations& registrations = redirect_it->second;
  registrations.destination = net::SchemefulSite(destination_origin);

  for (StorableSource& source : registrations.sources) {
    // The reporting origin has mis-configured the destination, ignore the
    // source.
    // TODO(apaseltiner): Report a DevTools/internals issue if the destinations
    // aren't matched.
    if (source.common_info().DestinationSite() != registrations.destination) {
      continue;
    }

    // Process the registration if the destination matched.
    attribution_manager_->HandleSource(std::move(source));
  }
  registrations.sources.clear();

  if (registrations.pending_source_data == 0u) {
    // We have finished processing all sources on this redirect chain, cleanup
    // the map.
    OnSourceEligibleDataHostFinished(registrations.register_time);
    redirect_registrations_.erase(redirect_it);
  }
}

void AttributionDataHostManagerImpl::NotifyNavigationFailure(
    const blink::AttributionSrcToken& attribution_src_token) {
  auto it = navigation_data_host_map_.find(attribution_src_token);
  if (it != navigation_data_host_map_.end()) {
    base::TimeTicks register_time = it->second.register_time;
    navigation_data_host_map_.erase(it);
    OnSourceEligibleDataHostFinished(register_time);
    RecordNavigationDataHostStatus(NavigationDataHostStatus::kNavigationFailed);
  }

  // We are not guaranteed to be processing redirect registrations for a given
  // navigation.
  auto redirect_it = redirect_registrations_.find(attribution_src_token);
  if (redirect_it != redirect_registrations_.end()) {
    OnSourceEligibleDataHostFinished(redirect_it->second.register_time);
    redirect_registrations_.erase(redirect_it);
  }
}

void AttributionDataHostManagerImpl::SourceDataAvailable(
    blink::mojom::AttributionSourceDataPtr data) {
  if (!network::IsOriginPotentiallyTrustworthy(data->reporting_origin)) {
    RecordSourceDataHandleStatus(DataHandleStatus::kUntrustworthyOrigin);
    ReportBadMessageInsecureReportingOrigin();
    return;
  }

  if (!network::IsOriginPotentiallyTrustworthy(data->destination)) {
    RecordSourceDataHandleStatus(DataHandleStatus::kUntrustworthyOrigin);
    mojo::ReportBadMessage(
        "AttributionDataHost: Destination origin must be secure.");
    return;
  }

  FrozenContext& context = receivers_.current_context();
  DCHECK(network::IsOriginPotentiallyTrustworthy(context.context_origin));

  // TODO(apaseltiner): Report a DevTools/internals issue if the destinations
  // aren't matched.
  if (context.source_type == AttributionSourceType::kNavigation &&
      net::SchemefulSite(data->destination) != context.destination) {
    RecordSourceDataHandleStatus(DataHandleStatus::kContextError);
    return;
  }

  if (context.registration_type == RegistrationType::kTrigger) {
    RecordSourceDataHandleStatus(DataHandleStatus::kContextError);
    mojo::ReportBadMessage(
        "AttributionDataHost: Cannot register sources after registering "
        "a trigger.");
    return;
  }

  context.registration_type = RegistrationType::kSource;

  base::Time source_time = base::Time::Now();

  // When converting mojo values to the browser process equivalents, it should
  // not be possible for there to be an error except in the case of a bad
  // renderer. All of the validation here is also performed renderer-side.

  absl::optional<AttributionFilterData> filter_data =
      AttributionFilterData::FromSourceFilterValues(
          std::move(data->filter_data->filter_values));
  if (!filter_data.has_value()) {
    RecordSourceDataHandleStatus(DataHandleStatus::kInvalidData);
    mojo::ReportBadMessage("AttributionDataHost: Invalid filter data.");
    return;
  }

  absl::optional<AttributionAggregationKeys> aggregation_keys =
      AttributionAggregationKeys::FromKeys(std::move(data->aggregation_keys));
  if (!aggregation_keys.has_value()) {
    RecordSourceDataHandleStatus(DataHandleStatus::kInvalidData);
    mojo::ReportBadMessage("AttributionDataHost: Invalid aggregatable source.");
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
      std::move(*aggregation_keys)));

  attribution_manager_->HandleSource(std::move(storable_source));
}

void AttributionDataHostManagerImpl::TriggerDataAvailable(
    blink::mojom::AttributionTriggerDataPtr data) {
  if (!network::IsOriginPotentiallyTrustworthy(data->reporting_origin)) {
    RecordTriggerDataHandleStatus(DataHandleStatus::kUntrustworthyOrigin);
    ReportBadMessageInsecureReportingOrigin();
    return;
  }

  FrozenContext& context = receivers_.current_context();
  DCHECK(network::IsOriginPotentiallyTrustworthy(context.context_origin));

  if (context.source_type == AttributionSourceType::kNavigation) {
    RecordTriggerDataHandleStatus(DataHandleStatus::kContextError);
    mojo::ReportBadMessage(
        "AttributionDataHost: Navigation-bound data hosts cannot register "
        "triggers.");
    return;
  }

  if (context.registration_type == RegistrationType::kSource) {
    RecordTriggerDataHandleStatus(DataHandleStatus::kContextError);
    mojo::ReportBadMessage(
        "AttributionDataHost: Cannot register triggers after registering "
        "a source.");
    return;
  }

  if (context.registration_type == RegistrationType::kNone) {
    OnSourceEligibleDataHostFinished(context.register_time);
    context.registration_type = RegistrationType::kTrigger;
  }

  absl::optional<AttributionFilterData> filters =
      AttributionFilterData::FromTriggerFilterValues(
          std::move(data->filters->filter_values));
  if (!filters.has_value()) {
    RecordTriggerDataHandleStatus(DataHandleStatus::kInvalidData);
    mojo::ReportBadMessage("AttributionDataHost: Invalid top-level filters.");
    return;
  }

  absl::optional<AttributionFilterData> not_filters =
      AttributionFilterData::FromTriggerFilterValues(
          std::move(data->not_filters->filter_values));
  if (!not_filters.has_value()) {
    RecordTriggerDataHandleStatus(DataHandleStatus::kInvalidData);
    mojo::ReportBadMessage(
        "AttributionDataHost: Invalid top-level negated filters.");
    return;
  }

  if (data->event_triggers.size() > blink::kMaxAttributionEventTriggerData) {
    RecordTriggerDataHandleStatus(DataHandleStatus::kInvalidData);
    mojo::ReportBadMessage("AttributionDataHost: Too many event triggers.");
    return;
  }

  std::vector<AttributionTrigger::EventTriggerData> event_triggers;
  event_triggers.reserve(data->event_triggers.size());

  for (auto& event_trigger : data->event_triggers) {
    absl::optional<AttributionFilterData> event_filters =
        AttributionFilterData::FromTriggerFilterValues(
            std::move(event_trigger->filters->filter_values));
    if (!event_filters.has_value()) {
      RecordTriggerDataHandleStatus(DataHandleStatus::kInvalidData);
      mojo::ReportBadMessage(
          "AttributionDataHost: Invalid event-trigger filters.");
      return;
    }

    absl::optional<AttributionFilterData> not_event_filters =
        AttributionFilterData::FromTriggerFilterValues(
            std::move(event_trigger->not_filters->filter_values));
    if (!not_event_filters.has_value()) {
      RecordTriggerDataHandleStatus(DataHandleStatus::kInvalidData);
      mojo::ReportBadMessage(
          "AttributionDataHost: Invalid event-trigger not_filters.");
      return;
    }

    event_triggers.emplace_back(
        event_trigger->data, event_trigger->priority,
        event_trigger->dedup_key
            ? absl::make_optional(event_trigger->dedup_key->value)
            : absl::nullopt,
        std::move(*event_filters), std::move(*not_event_filters));
  }

  absl::optional<std::vector<AttributionAggregatableTriggerData>>
      aggregatable_trigger_data =
          FromMojo(std::move(data->aggregatable_trigger_data));
  if (!aggregatable_trigger_data.has_value()) {
    RecordTriggerDataHandleStatus(DataHandleStatus::kInvalidData);
    mojo::ReportBadMessage(
        "AttributionDataHost: Invalid aggregatable trigger data.");
    return;
  }

  absl::optional<AttributionAggregatableValues> aggregatable_values =
      AttributionAggregatableValues::FromValues(
          std::move(data->aggregatable_values));
  if (!aggregatable_values.has_value()) {
    RecordTriggerDataHandleStatus(DataHandleStatus::kInvalidData);
    mojo::ReportBadMessage("AttributionDataHost: Invalid aggregatable values.");
    return;
  }

  RecordTriggerDataHandleStatus(DataHandleStatus::kSuccess);

  context.num_data_registered++;

  AttributionTrigger trigger(
      /*destination_origin=*/context.context_origin,
      std::move(data->reporting_origin), std::move(*filters),
      std::move(*not_filters),
      data->debug_key ? absl::make_optional(data->debug_key->value)
                      : absl::nullopt,
      std::move(event_triggers), std::move(*aggregatable_trigger_data),
      std::move(*aggregatable_values));

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
    DCHECK_NE(context.registration_type, RegistrationType::kNone);

    if (context.registration_type == RegistrationType::kTrigger) {
      base::UmaHistogramExactLinear("Conversions.RegisteredTriggersPerDataHost",
                                    context.num_data_registered, 101);
    } else {
      base::UmaHistogramExactLinear("Conversions.RegisteredSourcesPerDataHost",
                                    context.num_data_registered, 101);
    }
  }

  // If the receiver was handling triggers, there's nothing to do here.
  if (context.registration_type == RegistrationType::kTrigger)
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

void AttributionDataHostManagerImpl::OnRedirectSourceParsed(
    const blink::AttributionSrcToken& attribution_src_token,
    url::Origin reporting_origin,
    data_decoder::DataDecoder::ValueOrError result) {
  // TODO(johnidel): Add metrics regarding parsing failures / misconfigured
  // headers.
  auto it = redirect_registrations_.find(attribution_src_token);

  // The registration may no longer be tracked in the event the navigation
  // failed.
  if (it == redirect_registrations_.end())
    return;

  DCHECK_GE(it->second.pending_source_data, 0u);
  NavigationRedirectSourceRegistrations& registrations = it->second;
  registrations.pending_source_data--;

  absl::optional<StorableSource> source;
  if (result.has_value() && result->is_dict()) {
    // TODO(apaseltiner): Report a DevTools/internals issue if parsing fails.
    source = ParseSourceRegistration(
        std::move(result->GetDict()), /*source_time=*/base::Time::Now(),
        std::move(reporting_origin), registrations.source_origin,
        AttributionSourceType::kNavigation);
  }
  // Do not access `reporting_origin` below this line, it is no longer valid.

  // An opaque destination means that navigation has not finished, delay
  // handling.
  if (registrations.destination.opaque()) {
    if (source)
      registrations.sources.push_back(std::move(*source));
    return;
  }

  // Process the registration if it was valid.
  // TODO(apaseltiner): Report a DevTools/internals issue if the destinations
  // aren't matched.
  if (source &&
      source->common_info().DestinationSite() == registrations.destination) {
    attribution_manager_->HandleSource(std::move(*source));
  }

  if (registrations.pending_source_data == 0u) {
    // We have finished processing all sources on this redirect chain, cleanup
    // the map.
    OnSourceEligibleDataHostFinished(registrations.register_time);
    redirect_registrations_.erase(it);
  }
}

}  // namespace content
