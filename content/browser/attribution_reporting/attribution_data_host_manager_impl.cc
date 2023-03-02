// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_data_host_manager_impl.h"

#include <stddef.h>

#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/flat_tree.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/attribution_reporting/registration_type.mojom.h"
#include "components/attribution_reporting/source_registration.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "content/browser/attribution_reporting/attribution_beacon_id.h"
#include "content/browser/attribution_reporting/attribution_constants.h"
#include "content/browser/attribution_reporting/attribution_input_event.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/public/browser/global_routing_id.h"
#include "net/base/schemeful_site.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/trigger_attestation.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/conversions/attribution_reporting.mojom.h"
#include "url/origin.h"

namespace content {

namespace {

using ::attribution_reporting::SuitableOrigin;
using ::attribution_reporting::mojom::RegistrationType;
using ::attribution_reporting::mojom::SourceRegistrationError;
using ::attribution_reporting::mojom::SourceType;
using ::blink::mojom::AttributionNavigationType;

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
  kContextError = 1,

  kMaxValue = kContextError,
};

void RecordSourceDataHandleStatus(DataHandleStatus status) {
  base::UmaHistogramEnumeration("Conversions.SourceDataHandleStatus2", status);
}

void RecordTriggerDataHandleStatus(DataHandleStatus status) {
  base::UmaHistogramEnumeration("Conversions.TriggerDataHandleStatus2", status);
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
  base::UmaHistogramEnumeration("Conversions.NavigationDataHostStatus2", event);
}

const base::FeatureParam<base::TimeDelta> kTriggerDelay{
    &blink::features::kConversionMeasurement, "trigger_delay",
    base::Seconds(5)};

constexpr size_t kMaxDelayedTriggers = 30;

}  // namespace

class AttributionDataHostManagerImpl::ReceiverContext {
 public:
  ReceiverContext(SuitableOrigin context_origin,
                  RegistrationType registration_type,
                  base::TimeTicks register_time,
                  bool is_within_fenced_frame,
                  absl::optional<AttributionInputEvent> input_event,
                  absl::optional<AttributionNavigationType> nav_type,
                  GlobalRenderFrameHostId render_frame_id)
      : context_origin_(std::move(context_origin)),
        registration_type_(registration_type),
        register_time_(register_time),
        is_within_fenced_frame_(is_within_fenced_frame),
        input_event_(input_event),
        nav_type_(nav_type),
        render_frame_id_(render_frame_id) {
    DCHECK(!nav_type_ || registration_type_ == RegistrationType::kSource);
  }

  ~ReceiverContext() = default;

  ReceiverContext(const ReceiverContext&) = delete;
  ReceiverContext& operator=(const ReceiverContext&) = delete;

  ReceiverContext(ReceiverContext&&) = default;
  ReceiverContext& operator=(ReceiverContext&&) = default;

  const SuitableOrigin& context_origin() const { return context_origin_; }

  RegistrationType registration_type() const { return registration_type_; }

  void set_registration_type(RegistrationType type) {
    DCHECK_NE(type, RegistrationType::kSourceOrTrigger);
    registration_type_ = type;
  }

  size_t num_data_registered() const { return num_data_registered_; }

  base::TimeTicks register_time() const { return register_time_; }

  bool is_within_fenced_frame() const { return is_within_fenced_frame_; }

  absl::optional<AttributionNavigationType> nav_type() const {
    return nav_type_;
  }

  GlobalRenderFrameHostId render_frame_id() const { return render_frame_id_; }

  void IncrementNumDataRegistered() { ++num_data_registered_; }

 private:
  // Top-level origin the data host was created in.
  // Logically const.
  SuitableOrigin context_origin_;

  RegistrationType registration_type_;

  size_t num_data_registered_ = 0;

  // Logically const.
  base::TimeTicks register_time_;

  // Whether the attribution is registered within a fenced frame tree.
  // Logically const.
  bool is_within_fenced_frame_;

  // Input event associated with the navigation for navigation source data
  // hosts, `absl::nullopt` otherwise.
  // Logically const.
  absl::optional<AttributionInputEvent> input_event_;

  // Logically const.
  absl::optional<AttributionNavigationType> nav_type_;

  // The ID of the topmost render frame host.
  // Logically const.
  GlobalRenderFrameHostId render_frame_id_;
};

struct AttributionDataHostManagerImpl::DelayedTrigger {
  // Logically const.
  base::TimeTicks delay_until;

  AttributionTrigger trigger;

  GlobalRenderFrameHostId render_frame_id;

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
  AttributionInputEvent input_event;
};

struct AttributionDataHostManagerImpl::NavigationRedirectSourceRegistrations {
  // Source origin to use for all registrations on a redirect chain. Will not
  // change over the course of the redirect chain.
  SuitableOrigin source_origin;

  // Number of source data we are waiting to be decoded/received.
  size_t pending_source_data = 0;

  // True if navigation has completed, regardless of success or failure. If
  // true, no further calls will be made to
  // `NotifyNavigationRedirectRegistration()`.
  bool navigation_complete = false;

  // The time the first registration header was received for the redirect chain.
  // Will not change over the course of the redirect chain.
  base::TimeTicks register_time;

  // Input event associated with the navigation.
  AttributionInputEvent input_event;

  // Will not change over the course of the redirect chain.
  AttributionNavigationType nav_type;

  // Whether the navigation is initiated within a fenced frame. Will not
  // change over the course of the redirect chain.
  bool is_within_fenced_frame;

  GlobalRenderFrameHostId render_frame_id;
};

struct AttributionDataHostManagerImpl::BeaconSourceRegistrations {
  // Source origin to use for all registrations on a beacon. Will not
  // change over the course of the beacon.
  SuitableOrigin source_origin;

  // Number of source data we are waiting to be decoded/received.
  size_t pending_source_data = 0;

  // Navigation source data that has been received as part of this beacon.
  // Navigation sources cannot be processed until `navigation_complete` is set
  // to true.
  std::vector<StorableSource> sources;

  // True if navigation has completed. `absl::nullopt` if it's an event beacon.
  absl::optional<bool> navigation_complete;

  // True if the beacon has completed. If true, no further calls will be made to
  // `NotifyFencedFrameReportingBeaconData()`.
  bool beacon_complete = false;

  // The time the beacon was sent. Will be null when the beacon was started but
  // not actually sent.
  base::TimeTicks register_time;

  // Whether the beacon was initiated within a fenced frame.
  bool is_within_fenced_frame;

  // Input event associated with the navigation.
  absl::optional<AttributionInputEvent> input_event;

  GlobalRenderFrameHostId render_frame_id;
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
    SuitableOrigin context_origin,
    bool is_within_fenced_frame,
    RegistrationType registration_type,
    GlobalRenderFrameHostId render_frame_id) {
  receivers_.Add(this, std::move(data_host),
                 ReceiverContext(std::move(context_origin), registration_type,
                                 /*register_time=*/base::TimeTicks::Now(),
                                 is_within_fenced_frame,
                                 /*input_event=*/absl::nullopt,
                                 /*nav_type=*/absl::nullopt, render_frame_id));

  switch (registration_type) {
    case RegistrationType::kSourceOrTrigger:
    case RegistrationType::kSource:
      data_hosts_in_source_mode_++;
      break;
    case RegistrationType::kTrigger:
      break;
  }
}

bool AttributionDataHostManagerImpl::RegisterNavigationDataHost(
    mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host,
    const blink::AttributionSrcToken& attribution_src_token,
    AttributionInputEvent input_event) {
  auto [it, inserted] = navigation_data_host_map_.try_emplace(
      attribution_src_token,
      NavigationDataHost{.data_host = std::move(data_host),
                         .register_time = base::TimeTicks::Now(),
                         .input_event = input_event});
  // Should only be possible with a misbehaving renderer.
  if (!inserted) {
    return false;
  }

  data_hosts_in_source_mode_++;

  RecordNavigationDataHostStatus(NavigationDataHostStatus::kRegistered);
  return true;
}

void AttributionDataHostManagerImpl::NotifyNavigationRedirectRegistration(
    const blink::AttributionSrcToken& attribution_src_token,
    std::string header_value,
    SuitableOrigin reporting_origin,
    const SuitableOrigin& source_origin,
    AttributionInputEvent input_event,
    AttributionNavigationType nav_type,
    bool is_within_fenced_frame,
    GlobalRenderFrameHostId render_frame_id) {
  // Avoid costly isolated JSON parsing below if the header is obviously
  // invalid.
  if (header_value.empty()) {
    attribution_manager_->NotifyFailedSourceRegistration(
        header_value, source_origin, reporting_origin, SourceType::kNavigation,
        SourceRegistrationError::kInvalidJson);
    return;
  }

  auto [it, inserted] = redirect_registrations_.try_emplace(
      attribution_src_token,
      NavigationRedirectSourceRegistrations{
          .source_origin = source_origin,
          .register_time = base::TimeTicks::Now(),
          .input_event = input_event,
          .nav_type = nav_type,
          .is_within_fenced_frame = is_within_fenced_frame,
          .render_frame_id = render_frame_id});
  DCHECK(!it->second.navigation_complete);

  // Treat ongoing redirect registrations within a chain as a data host for the
  // purpose of trigger queuing.
  if (inserted) {
    data_hosts_in_source_mode_++;
  }

  it->second.pending_source_data++;

  // Send the data to the decoder, but track that we are now waiting on a new
  // registration.
  data_decoder::DataDecoder::ParseJsonIsolated(
      header_value,
      base::BindOnce(&AttributionDataHostManagerImpl::OnRedirectSourceParsed,
                     weak_factory_.GetWeakPtr(), attribution_src_token,
                     std::move(reporting_origin), header_value));
}

void AttributionDataHostManagerImpl::NotifyNavigationForDataHost(
    const blink::AttributionSrcToken& attribution_src_token,
    const SuitableOrigin& source_origin,
    AttributionNavigationType nav_type,
    bool is_within_fenced_frame,
    GlobalRenderFrameHostId render_frame_id) {
  auto it = navigation_data_host_map_.find(attribution_src_token);

  if (it != navigation_data_host_map_.end()) {
    receivers_.Add(
        this, std::move(it->second.data_host),
        ReceiverContext(source_origin, RegistrationType::kSource,
                        it->second.register_time, is_within_fenced_frame,
                        it->second.input_event, nav_type, render_frame_id));

    navigation_data_host_map_.erase(it);
    RecordNavigationDataHostStatus(NavigationDataHostStatus::kProcessed);
  } else {
    RecordNavigationDataHostStatus(NavigationDataHostStatus::kNotFound);
  }

  auto redirect_it = redirect_registrations_.find(attribution_src_token);
  if (redirect_it == redirect_registrations_.end()) {
    return;
  }

  NavigationRedirectSourceRegistrations& registrations = redirect_it->second;

  DCHECK(!registrations.navigation_complete);
  registrations.navigation_complete = true;

  if (registrations.pending_source_data == 0u) {
    // We have finished processing all sources on this redirect chain, cleanup
    // the map.
    OnSourceEligibleDataHostFinished(registrations.register_time);
    redirect_registrations_.erase(redirect_it);
  }
}

void AttributionDataHostManagerImpl::NotifyNavigationFailure(
    const absl::optional<blink::AttributionSrcToken>& attribution_src_token,
    int64_t navigation_id) {
  if (attribution_src_token) {
    auto it = navigation_data_host_map_.find(*attribution_src_token);
    if (it != navigation_data_host_map_.end()) {
      base::TimeTicks register_time = it->second.register_time;
      navigation_data_host_map_.erase(it);
      OnSourceEligibleDataHostFinished(register_time);
      RecordNavigationDataHostStatus(
          NavigationDataHostStatus::kNavigationFailed);
    }

    // We are not guaranteed to be processing redirect registrations for a given
    // navigation.
    auto redirect_it = redirect_registrations_.find(*attribution_src_token);
    if (redirect_it != redirect_registrations_.end()) {
      NavigationRedirectSourceRegistrations& registrations =
          redirect_it->second;

      DCHECK(!registrations.navigation_complete);
      registrations.navigation_complete = true;

      if (registrations.pending_source_data == 0u) {
        OnSourceEligibleDataHostFinished(redirect_it->second.register_time);
        redirect_registrations_.erase(redirect_it);
      }
    }
  }

  auto beacon_it =
      beacon_registrations_.find(BeaconId(NavigationBeaconId(navigation_id)));
  if (beacon_it != beacon_registrations_.end()) {
    OnSourceEligibleDataHostFinished(beacon_it->second.register_time);
    beacon_registrations_.erase(beacon_it);
  }
}

void AttributionDataHostManagerImpl::NotifyNavigationSuccess(
    int64_t navigation_id) {
  NavigationBeaconId beacon_id(navigation_id);

  auto it = beacon_registrations_.find(BeaconId(beacon_id));
  if (it == beacon_registrations_.end()) {
    return;
  }

  BeaconSourceRegistrations& registrations = it->second;
  DCHECK(registrations.navigation_complete == false);
  registrations.navigation_complete = true;

  for (StorableSource& source : registrations.sources) {
    attribution_manager_->HandleSource(std::move(source),
                                       registrations.render_frame_id);
  }
  registrations.sources.clear();

  MaybeOnBeaconRegistrationsFinished(beacon_id);
}

void AttributionDataHostManagerImpl::SourceDataAvailable(
    attribution_reporting::SuitableOrigin reporting_origin,
    attribution_reporting::SourceRegistration data) {
  // This is validated by the Mojo typemapping.
  DCHECK(reporting_origin.IsValid());

  ReceiverContext& context = receivers_.current_context();

  if (context.registration_type() == RegistrationType::kTrigger) {
    RecordSourceDataHandleStatus(DataHandleStatus::kContextError);
    mojo::ReportBadMessage("AttributionDataHost: Not eligible for sources.");
    return;
  }

  context.set_registration_type(RegistrationType::kSource);

  RecordSourceDataHandleStatus(DataHandleStatus::kSuccess);

  context.IncrementNumDataRegistered();

  auto source_type = SourceType::kEvent;
  if (auto nav_type = context.nav_type()) {
    source_type = SourceType::kNavigation;

    base::UmaHistogramEnumeration(
        "Conversions.SourceRegistration.NavigationType.Background", *nav_type);
  }

  attribution_manager_->HandleSource(
      StorableSource(std::move(reporting_origin), std::move(data),
                     /*source_time=*/base::Time::Now(),
                     /*source_origin=*/context.context_origin(), source_type,
                     context.is_within_fenced_frame()),
      context.render_frame_id());
}

void AttributionDataHostManagerImpl::TriggerDataAvailable(
    attribution_reporting::SuitableOrigin reporting_origin,
    attribution_reporting::TriggerRegistration data,
    // TODO(crbug.com/1401347): Propagate `attestation` to storage.
    absl::optional<network::TriggerAttestation> attestation) {
  // This is validated by the Mojo typemapping.
  DCHECK(reporting_origin.IsValid());

  ReceiverContext& context = receivers_.current_context();

  switch (context.registration_type()) {
    case RegistrationType::kSource:
      RecordTriggerDataHandleStatus(DataHandleStatus::kContextError);
      mojo::ReportBadMessage("AttributionDataHost: Not eligible for triggers.");
      return;
    case RegistrationType::kSourceOrTrigger:
      OnSourceEligibleDataHostFinished(context.register_time());
      context.set_registration_type(RegistrationType::kTrigger);
      break;
    case RegistrationType::kTrigger:
      break;
  }

  RecordTriggerDataHandleStatus(DataHandleStatus::kSuccess);

  context.IncrementNumDataRegistered();

  AttributionTrigger trigger(std::move(reporting_origin), std::move(data),
                             /*destination_origin=*/context.context_origin(),
                             std::move(attestation),
                             context.is_within_fenced_frame());

  // Handle the trigger immediately if we're not waiting for any sources to be
  // registered.
  if (data_hosts_in_source_mode_ == 0) {
    DCHECK(delayed_triggers_.empty());
    RecordTriggerQueueEvent(TriggerQueueEvent::kSkippedQueue);
    attribution_manager_->HandleTrigger(std::move(trigger),
                                        context.render_frame_id());
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
      .render_frame_id = context.render_frame_id(),
  });
  RecordTriggerQueueEvent(TriggerQueueEvent::kEnqueued);

  if (!trigger_timer_.IsRunning()) {
    SetTriggerTimer(delay);
  }
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

  attribution_manager_->HandleTrigger(std::move(delayed_trigger.trigger),
                                      delayed_trigger.render_frame_id);
  RecordTriggerQueueEvent(TriggerQueueEvent::kProcessedWithDelay);
  delayed_trigger.RecordDelay();

  if (!delayed_triggers_.empty()) {
    base::TimeDelta delay = delayed_triggers_.front().TimeUntil();
    SetTriggerTimer(delay);
  }
}

void AttributionDataHostManagerImpl::OnReceiverDisconnected() {
  const ReceiverContext& context = receivers_.current_context();

  const char* histogram_name = nullptr;
  switch (context.registration_type()) {
    case RegistrationType::kSourceOrTrigger:
      OnSourceEligibleDataHostFinished(context.register_time());
      DCHECK_EQ(context.num_data_registered(), 0u);
      return;
    case RegistrationType::kTrigger:
      histogram_name = "Conversions.RegisteredTriggersPerDataHost";
      break;
    case RegistrationType::kSource:
      OnSourceEligibleDataHostFinished(context.register_time());
      histogram_name = "Conversions.RegisteredSourcesPerDataHost";
      break;
  }

  if (size_t num = context.num_data_registered()) {
    base::UmaHistogramExactLinear(histogram_name, num, 101);
  }
}

void AttributionDataHostManagerImpl::OnSourceEligibleDataHostFinished(
    base::TimeTicks register_time) {
  if (register_time.is_null()) {
    return;
  }

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
  if (data_hosts_in_source_mode_ > 0) {
    return;
  }

  trigger_timer_.Stop();

  // Process triggers synchronously. This is OK, because the current
  // `kMaxDelayedTriggers` of 30 is relatively small and the attribution manager
  // only does a small amount of work and then posts a task to a different
  // sequence.
  static_assert(kMaxDelayedTriggers <= 30,
                "Consider using PostTask instead of handling triggers "
                "synchronously to avoid blocking for too long.");

  for (auto& delayed_trigger : delayed_triggers_) {
    attribution_manager_->HandleTrigger(std::move(delayed_trigger.trigger),
                                        delayed_trigger.render_frame_id);
    RecordTriggerQueueEvent(TriggerQueueEvent::kFlushed);
    delayed_trigger.RecordDelay();
  }

  delayed_triggers_.clear();
}

void AttributionDataHostManagerImpl::OnRedirectSourceParsed(
    const blink::AttributionSrcToken& attribution_src_token,
    const SuitableOrigin& reporting_origin,
    const std::string& header_value,
    data_decoder::DataDecoder::ValueOrError result) {
  auto it = redirect_registrations_.find(attribution_src_token);

  // The registration may no longer be tracked in the event the navigation
  // failed.
  if (it == redirect_registrations_.end()) {
    return;
  }

  DCHECK_GT(it->second.pending_source_data, 0u);
  NavigationRedirectSourceRegistrations& registrations = it->second;
  registrations.pending_source_data--;

  absl::optional<StorableSource> source =
      ParseStorableSource(std::move(result), header_value, reporting_origin,
                          registrations.source_origin, SourceType::kNavigation,
                          registrations.is_within_fenced_frame);

  if (source.has_value()) {
    base::UmaHistogramEnumeration(
        "Conversions.SourceRegistration.NavigationType.Foreground",
        registrations.nav_type);
    attribution_manager_->HandleSource(std::move(*source),
                                       registrations.render_frame_id);
  }

  if (registrations.pending_source_data == 0u &&
      registrations.navigation_complete) {
    // We have finished processing all sources on this redirect chain, cleanup
    // the map.
    OnSourceEligibleDataHostFinished(registrations.register_time);
    redirect_registrations_.erase(it);
  }
}

void AttributionDataHostManagerImpl::NotifyFencedFrameReportingBeaconStarted(
    BeaconId beacon_id,
    SuitableOrigin source_origin,
    bool is_within_fenced_frame,
    absl::optional<AttributionInputEvent> input_event,
    GlobalRenderFrameHostId render_frame_id) {
  bool is_navigation = absl::holds_alternative<NavigationBeaconId>(beacon_id);
  DCHECK_EQ(is_navigation, input_event.has_value());

  auto [it, inserted] = beacon_registrations_.try_emplace(
      beacon_id, BeaconSourceRegistrations{
                     .source_origin = std::move(source_origin),
                     .is_within_fenced_frame = is_within_fenced_frame,
                     .input_event = input_event,
                     .render_frame_id = render_frame_id});

  if (!inserted) {
    return;
  }

  if (is_navigation) {
    it->second.navigation_complete.emplace(false);
  }
}

void AttributionDataHostManagerImpl::NotifyFencedFrameReportingBeaconSent(
    BeaconId beacon_id) {
  auto it = beacon_registrations_.find(beacon_id);

  // The registration may no longer be tracked in the event the navigation
  // failed.
  if (it == beacon_registrations_.end()) {
    return;
  }

  it->second.register_time = base::TimeTicks::Now();

  // Treat ongoing beacon registrations as a data host for the purpose of
  // trigger queuing.
  data_hosts_in_source_mode_++;
}

void AttributionDataHostManagerImpl::NotifyFencedFrameReportingBeaconData(
    BeaconId beacon_id,
    url::Origin reporting_origin,
    const net::HttpResponseHeaders* headers,
    bool is_final_response) {
  auto it = beacon_registrations_.find(beacon_id);

  // The registration may no longer be tracked in the event the navigation
  // failed.
  if (it == beacon_registrations_.end()) {
    return;
  }

  DCHECK(!it->second.beacon_complete);
  it->second.beacon_complete = is_final_response;

  absl::optional<SuitableOrigin> suitable_reporting_origin =
      SuitableOrigin::Create(std::move(reporting_origin));
  if (!suitable_reporting_origin) {
    MaybeOnBeaconRegistrationsFinished(beacon_id);
    return;
  }

  if (!headers) {
    MaybeOnBeaconRegistrationsFinished(beacon_id);
    return;
  }

  std::string source_header;
  if (!headers->GetNormalizedHeader(kAttributionReportingRegisterSourceHeader,
                                    &source_header)) {
    MaybeOnBeaconRegistrationsFinished(beacon_id);
    return;
  }

  it->second.pending_source_data++;

  data_decoder::DataDecoder::ParseJsonIsolated(
      source_header,
      base::BindOnce(&AttributionDataHostManagerImpl::OnBeaconSourceParsed,
                     weak_factory_.GetWeakPtr(), beacon_id,
                     std::move(*suitable_reporting_origin), source_header));
}

void AttributionDataHostManagerImpl::OnBeaconSourceParsed(
    BeaconId beacon_id,
    const SuitableOrigin& reporting_origin,
    const std::string& header_value,
    data_decoder::DataDecoder::ValueOrError result) {
  auto it = beacon_registrations_.find(beacon_id);

  // The registration may no longer be tracked in the event the navigation
  // failed.
  if (it == beacon_registrations_.end()) {
    return;
  }

  BeaconSourceRegistrations& registrations = it->second;
  DCHECK_GT(registrations.pending_source_data, 0u);
  registrations.pending_source_data--;

  absl::optional<StorableSource> source =
      ParseStorableSource(std::move(result), header_value, reporting_origin,
                          registrations.source_origin,
                          absl::holds_alternative<NavigationBeaconId>(beacon_id)
                              ? SourceType::kNavigation
                              : SourceType::kEvent,
                          registrations.is_within_fenced_frame);

  if (source.has_value()) {
    if (registrations.navigation_complete.value_or(true)) {
      attribution_manager_->HandleSource(std::move(*source),
                                         registrations.render_frame_id);
    } else {
      registrations.sources.push_back(std::move(*source));
    }
  }

  MaybeOnBeaconRegistrationsFinished(beacon_id);
}

absl::optional<StorableSource>
AttributionDataHostManagerImpl::ParseStorableSource(
    data_decoder::DataDecoder::ValueOrError result,
    const std::string& header_value,
    const attribution_reporting::SuitableOrigin& reporting_origin,
    const attribution_reporting::SuitableOrigin& source_origin,
    SourceType source_type,
    bool is_within_fenced_frame) {
  base::expected<StorableSource, SourceRegistrationError> source =
      base::unexpected(SourceRegistrationError::kInvalidJson);
  if (result.has_value()) {
    if (result->is_dict()) {
      auto registration = attribution_reporting::SourceRegistration::Parse(
          std::move(*result).TakeDict());
      if (registration.has_value()) {
        source.emplace(reporting_origin, std::move(*registration),
                       /*source_time=*/base::Time::Now(), source_origin,
                       source_type, is_within_fenced_frame);
      } else {
        source = base::unexpected(registration.error());
      }
    } else {
      source = base::unexpected(SourceRegistrationError::kRootWrongType);
    }
  }

  if (!source.has_value()) {
    attribution_manager_->NotifyFailedSourceRegistration(
        header_value, source_origin, reporting_origin, source_type,
        source.error());
    attribution_reporting::RecordSourceRegistrationError(source.error());
    return absl::nullopt;
  }

  return std::move(*source);
}

void AttributionDataHostManagerImpl::MaybeOnBeaconRegistrationsFinished(
    BeaconId beacon_id) {
  auto it = beacon_registrations_.find(beacon_id);
  if (it == beacon_registrations_.end()) {
    return;
  }

  BeaconSourceRegistrations& registrations = it->second;
  if (registrations.pending_source_data == 0u &&
      registrations.navigation_complete.value_or(true) &&
      registrations.beacon_complete) {
    OnSourceEligibleDataHostFinished(registrations.register_time);
    beacon_registrations_.erase(it);
  }
}

}  // namespace content
