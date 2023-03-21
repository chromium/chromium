// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_data_host_manager_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/function_ref.h"
#include "base/functional/overloaded.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/attribution_reporting/os_support.mojom.h"
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
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/trigger_attestation.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom.h"
#include "third_party/blink/public/mojom/conversions/attribution_reporting.mojom.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/attribution_reporting/os_registration.h"
#include "content/browser/attribution_reporting/attribution_os_level_manager.h"
#include "content/browser/attribution_reporting/os_registration.h"
#include "net/http/structured_headers.h"
#include "url/gurl.h"
#endif

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

enum class Registrar {
  kWeb,
  kOs,
};

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
                  AttributionInputEvent input_event,
                  absl::optional<AttributionNavigationType> nav_type,
                  GlobalRenderFrameHostId render_frame_id)
      : context_origin_(std::move(context_origin)),
        registration_type_(registration_type),
        register_time_(register_time),
        is_within_fenced_frame_(is_within_fenced_frame),
        input_event_(std::move(input_event)),
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

  const AttributionInputEvent& input_event() const { return input_event_; }

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
  // hosts. The underlying Java object will be null for event sources.
  // Logically const.
  AttributionInputEvent input_event_;

  // Logically const.
  absl::optional<AttributionNavigationType> nav_type_;

  // The ID of the topmost render frame host.
  // Logically const.
  GlobalRenderFrameHostId render_frame_id_;
};

struct AttributionDataHostManagerImpl::DelayedTrigger {
  // Logically const.
  base::TimeTicks delay_until;

  TriggerPayload trigger;

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

class AttributionDataHostManagerImpl::SourceRegistrations {
 public:
  struct NavigationRedirect {
    blink::AttributionSrcToken attribution_src_token;

    // Will not change over the course of the redirect chain.
    AttributionNavigationType nav_type;
  };

  using Data = absl::variant<NavigationRedirect, BeaconId>;

  SourceRegistrations(SuitableOrigin source_origin,
                      base::TimeTicks register_time,
                      bool is_within_fenced_frame,
                      AttributionInputEvent input_event,
                      GlobalRenderFrameHostId render_frame_id,
                      Data data)
      : source_origin_(std::move(source_origin)),
        register_time_(register_time),
        is_within_fenced_frame_(is_within_fenced_frame),
        input_event_(std::move(input_event)),
        render_frame_id_(render_frame_id),
        data_(data) {}

  SourceRegistrations(const SourceRegistrations&) = delete;
  SourceRegistrations& operator=(const SourceRegistrations&) = delete;

  SourceRegistrations(SourceRegistrations&&) = default;
  SourceRegistrations& operator=(SourceRegistrations&&) = default;

  const SuitableOrigin& source_origin() const { return source_origin_; }

  size_t pending_source_data() const { return pending_source_data_; }

  bool registrations_complete() const { return registrations_complete_; }

  base::TimeTicks register_time() const { return register_time_; }

  bool is_within_fenced_frame() const { return is_within_fenced_frame_; }

  const AttributionInputEvent& input_event() const { return input_event_; }

  GlobalRenderFrameHostId render_frame_id() const { return render_frame_id_; }

  const Data& data() const { return data_; }

  bool operator<(const SourceRegistrations& other) const {
    return Id() < other.Id();
  }

  void CompleteRegistrations() {
    DCHECK(!registrations_complete_);
    registrations_complete_ = true;
  }

  void set_register_time() {
    DCHECK(register_time_.is_null());
    register_time_ = base::TimeTicks::Now();
  }

  void IncrementPendingSourceData() { ++pending_source_data_; }

  void DecrementPendingSourceData() {
    DCHECK_GT(pending_source_data_, 0u);
    --pending_source_data_;
  }

  friend bool operator<(const SourceRegistrations& a,
                        const SourceRegistrationsId& b) {
    return a.Id() < b;
  }

  friend bool operator<(const SourceRegistrationsId& a,
                        const SourceRegistrations& b) {
    return a < b.Id();
  }

  SourceRegistrationsId Id() const {
    return absl::visit(
        base::Overloaded{
            [](const NavigationRedirect& redirect) {
              return SourceRegistrationsId(redirect.attribution_src_token);
            },
            [](const BeaconId& beacon_id) {
              return SourceRegistrationsId(beacon_id);
            },
        },
        data_);
  }

 private:
  // Source origin to use for all registrations on a navigation redirect or
  // beacon chain. Will not change over the course of the chain.
  SuitableOrigin source_origin_;

  // Number of source data we are waiting to be decoded/received.
  size_t pending_source_data_ = 0;

  // True if navigation or beacon has completed.
  bool registrations_complete_ = false;

  // The time the first registration header was received. Will be null when the
  // beacon was started but no data was received yet.
  base::TimeTicks register_time_;

  // Whether the registration was initiated within a fenced frame.
  bool is_within_fenced_frame_;

  // Input event associated with the navigation.
  // The underlying Java object will be null for event beacons.
  AttributionInputEvent input_event_;

  GlobalRenderFrameHostId render_frame_id_;

  Data data_;
};

struct AttributionDataHostManagerImpl::RegistrarAndHeader {
  Registrar registrar;
  std::string header;

  [[nodiscard]] static absl::optional<RegistrarAndHeader> Get(
      const net::HttpResponseHeaders* headers) {
    if (!headers) {
      return absl::nullopt;
    }

    std::string web_source;
    const bool has_web = headers->GetNormalizedHeader(
        kAttributionReportingRegisterSourceHeader, &web_source);

    std::string os_source;
    const bool has_os =
        base::FeatureList::IsEnabled(
            blink::features::kAttributionReportingCrossAppWeb) &&
        headers->GetNormalizedHeader(
            kAttributionReportingRegisterOsSourceHeader, &os_source);

    if (has_web == has_os) {
      // TODO: Report a DevTools issue if both headers are present.
      return absl::nullopt;
    }

    if (has_web) {
      return RegistrarAndHeader{.registrar = Registrar::kWeb,
                                .header = std::move(web_source)};
    }

    DCHECK(has_os);
    return RegistrarAndHeader{.registrar = Registrar::kOs,
                              .header = std::move(os_source)};
  }
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
                                 /*input_event=*/AttributionInputEvent(),
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
                         .input_event = std::move(input_event)});
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
    const net::HttpResponseHeaders* headers,
    SuitableOrigin reporting_origin,
    const SuitableOrigin& source_origin,
    AttributionInputEvent input_event,
    AttributionNavigationType nav_type,
    bool is_within_fenced_frame,
    GlobalRenderFrameHostId render_frame_id) {
  const auto attribution_header = RegistrarAndHeader::Get(headers);
  if (!attribution_header) {
    return;
  }

  auto [it, inserted] = registrations_.emplace(
      source_origin,
      /*register_time=*/base::TimeTicks::Now(), is_within_fenced_frame,
      std::move(input_event), render_frame_id,
      SourceRegistrations::NavigationRedirect{
          .attribution_src_token = attribution_src_token,
          .nav_type = nav_type,
      });
  DCHECK(!it->registrations_complete());

  // Treat ongoing redirect registrations within a chain as a data host for the
  // purpose of trigger queuing.
  if (inserted) {
    data_hosts_in_source_mode_++;
  }

  ParseSource(it, std::move(reporting_origin), *attribution_header);
}

void AttributionDataHostManagerImpl::ParseSource(
    base::flat_set<SourceRegistrations>::iterator it,
    SuitableOrigin reporting_origin,
    const RegistrarAndHeader& header) {
  DCHECK(it != registrations_.end());

  switch (header.registrar) {
    case Registrar::kWeb:
      it->IncrementPendingSourceData();
      data_decoder::DataDecoder::ParseJsonIsolated(
          header.header,
          base::BindOnce(&AttributionDataHostManagerImpl::OnWebSourceParsed,
                         weak_factory_.GetWeakPtr(), it->Id(),
                         std::move(reporting_origin), header.header));
      break;
    case Registrar::kOs:
      if (AttributionManager::GetOsSupport() ==
          attribution_reporting::mojom::OsSupport::kDisabled) {
        // TODO: Report a DevTools issue.
        MaybeOnRegistrationsFinished(it);
        break;
      }
#if BUILDFLAG(IS_ANDROID)
      it->IncrementPendingSourceData();
      data_decoder::DataDecoder::ParseStructuredHeaderItemIsolated(
          header.header,
          base::BindOnce(&AttributionDataHostManagerImpl::OnOsSourceParsed,
                         weak_factory_.GetWeakPtr(), it->Id()));
#else
      NOTREACHED();
#endif
      break;
  }
}

void AttributionDataHostManagerImpl::NotifyNavigationForDataHost(
    const blink::AttributionSrcToken& attribution_src_token,
    const SuitableOrigin& source_origin,
    AttributionNavigationType nav_type,
    bool is_within_fenced_frame,
    GlobalRenderFrameHostId render_frame_id) {
  if (auto it = navigation_data_host_map_.find(attribution_src_token);
      it != navigation_data_host_map_.end()) {
    receivers_.Add(
        this, std::move(it->second.data_host),
        ReceiverContext(source_origin, RegistrationType::kSource,
                        it->second.register_time, is_within_fenced_frame,
                        std::move(it->second.input_event), nav_type,
                        render_frame_id));

    navigation_data_host_map_.erase(it);
    RecordNavigationDataHostStatus(NavigationDataHostStatus::kProcessed);
  } else {
    RecordNavigationDataHostStatus(NavigationDataHostStatus::kNotFound);
  }

  if (auto it = registrations_.find(attribution_src_token);
      it != registrations_.end()) {
    it->CompleteRegistrations();
    MaybeOnRegistrationsFinished(it);
  }
}

void AttributionDataHostManagerImpl::NotifyNavigationFailure(
    const blink::AttributionSrcToken& attribution_src_token) {
  if (auto it = navigation_data_host_map_.find(attribution_src_token);
      it != navigation_data_host_map_.end()) {
    base::TimeTicks register_time = it->second.register_time;
    navigation_data_host_map_.erase(it);
    OnSourceEligibleDataHostFinished(register_time);
    RecordNavigationDataHostStatus(NavigationDataHostStatus::kNavigationFailed);
  }

  // We are not guaranteed to be processing redirect registrations for a given
  // navigation.
  if (auto it = registrations_.find(attribution_src_token);
      it != registrations_.end()) {
    it->CompleteRegistrations();
    MaybeOnRegistrationsFinished(it);
  }
}

const AttributionDataHostManagerImpl::ReceiverContext*
AttributionDataHostManagerImpl::GetReceiverContextForSource() {
  ReceiverContext& context = receivers_.current_context();

  if (context.registration_type() == RegistrationType::kTrigger) {
    mojo::ReportBadMessage("AttributionDataHost: Not eligible for sources.");
    return nullptr;
  }

  context.set_registration_type(RegistrationType::kSource);
  context.IncrementNumDataRegistered();

  return &context;
}

void AttributionDataHostManagerImpl::SourceDataAvailable(
    attribution_reporting::SuitableOrigin reporting_origin,
    attribution_reporting::SourceRegistration data) {
  // This is validated by the Mojo typemapping.
  DCHECK(reporting_origin.IsValid());

  const ReceiverContext* context = GetReceiverContextForSource();
  if (!context) {
    return;
  }

  auto source_type = SourceType::kEvent;
  if (auto nav_type = context->nav_type()) {
    source_type = SourceType::kNavigation;

    base::UmaHistogramEnumeration(
        "Conversions.SourceRegistration.NavigationType.Background", *nav_type);
  }

  attribution_manager_->HandleSource(
      StorableSource(std::move(reporting_origin), std::move(data),
                     /*source_time=*/base::Time::Now(),
                     /*source_origin=*/context->context_origin(), source_type,
                     context->is_within_fenced_frame()),
      context->render_frame_id());
}

void AttributionDataHostManagerImpl::MaybeBufferTrigger(
    base::FunctionRef<TriggerPayload(const ReceiverContext&)> make_trigger) {
  ReceiverContext& context = receivers_.current_context();

  switch (context.registration_type()) {
    case RegistrationType::kSource:
      mojo::ReportBadMessage("AttributionDataHost: Not eligible for triggers.");
      return;
    case RegistrationType::kSourceOrTrigger:
      OnSourceEligibleDataHostFinished(context.register_time());
      context.set_registration_type(RegistrationType::kTrigger);
      break;
    case RegistrationType::kTrigger:
      break;
  }

  context.IncrementNumDataRegistered();
  auto trigger = make_trigger(context);

  // Handle the trigger immediately if we're not waiting for any sources to be
  // registered.
  if (data_hosts_in_source_mode_ == 0) {
    DCHECK(delayed_triggers_.empty());
    RecordTriggerQueueEvent(TriggerQueueEvent::kSkippedQueue);
    HandleTrigger(std::move(trigger), context.render_frame_id());
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

void AttributionDataHostManagerImpl::TriggerDataAvailable(
    attribution_reporting::SuitableOrigin reporting_origin,
    attribution_reporting::TriggerRegistration data,
    absl::optional<network::TriggerAttestation> attestation) {
  // This is validated by the Mojo typemapping.
  DCHECK(reporting_origin.IsValid());

  MaybeBufferTrigger([&](const ReceiverContext& context) {
    return AttributionTrigger(std::move(reporting_origin), std::move(data),
                              /*destination_origin=*/context.context_origin(),
                              std::move(attestation),
                              context.is_within_fenced_frame());
  });
}

#if BUILDFLAG(IS_ANDROID)

void AttributionDataHostManagerImpl::OsSourceDataAvailable(
    const GURL& registration_url) {
  const ReceiverContext* context = GetReceiverContextForSource();
  if (!context) {
    return;
  }

  attribution_manager_->HandleOsRegistration(
      OsRegistration(registration_url, context->context_origin(),
                     context->input_event()),
      context->render_frame_id());
}

void AttributionDataHostManagerImpl::OsTriggerDataAvailable(
    const GURL& registration_url) {
  MaybeBufferTrigger([&](const ReceiverContext& context) {
    return OsRegistration(registration_url, context.context_origin(),
                          /*input_event=*/absl::nullopt);
  });
}

#endif  // BUILDFLAG(IS_ANDROID)

void AttributionDataHostManagerImpl::SetTriggerTimer(base::TimeDelta delay) {
  DCHECK(!delayed_triggers_.empty());
  trigger_timer_.Start(FROM_HERE, delay, this,
                       &AttributionDataHostManagerImpl::ProcessDelayedTrigger);
}

void AttributionDataHostManagerImpl::HandleTrigger(
    TriggerPayload trigger,
    GlobalRenderFrameHostId render_frame_id) {
#if BUILDFLAG(IS_ANDROID)
  absl::visit(base::Overloaded{
                  [&](AttributionTrigger trigger) {
                    attribution_manager_->HandleTrigger(std::move(trigger),
                                                        render_frame_id);
                  },
                  [&](OsRegistration trigger) {
                    DCHECK(!trigger.input_event.has_value());
                    attribution_manager_->HandleOsRegistration(
                        std::move(trigger), render_frame_id);
                  },
              },
              std::move(trigger));
#else
  attribution_manager_->HandleTrigger(std::move(trigger), render_frame_id);
#endif
}

void AttributionDataHostManagerImpl::ProcessDelayedTrigger() {
  DCHECK(!delayed_triggers_.empty());

  {
    DelayedTrigger& delayed_trigger = delayed_triggers_.front();
    DCHECK_LE(delayed_trigger.delay_until, base::TimeTicks::Now());

    HandleTrigger(std::move(delayed_trigger.trigger),
                  delayed_trigger.render_frame_id);
    RecordTriggerQueueEvent(TriggerQueueEvent::kProcessedWithDelay);
    delayed_trigger.RecordDelay();
  }
  delayed_triggers_.pop_front();

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
    HandleTrigger(std::move(delayed_trigger.trigger),
                  delayed_trigger.render_frame_id);
    RecordTriggerQueueEvent(TriggerQueueEvent::kFlushed);
    delayed_trigger.RecordDelay();
  }

  delayed_triggers_.clear();
}

void AttributionDataHostManagerImpl::NotifyFencedFrameReportingBeaconStarted(
    BeaconId beacon_id,
    SuitableOrigin source_origin,
    bool is_within_fenced_frame,
    AttributionInputEvent input_event,
    GlobalRenderFrameHostId render_frame_id) {
  auto [it, inserted] = registrations_.emplace(
      std::move(source_origin),
      /*register_time=*/base::TimeTicks(), is_within_fenced_frame,
      std::move(input_event), render_frame_id, beacon_id);
  DCHECK(inserted);
}

void AttributionDataHostManagerImpl::NotifyFencedFrameReportingBeaconSent(
    BeaconId beacon_id) {
  auto it = registrations_.find(beacon_id);

  // The registration may no longer be tracked in the event the navigation
  // failed.
  if (it == registrations_.end()) {
    return;
  }

  it->set_register_time();

  // Treat ongoing beacon registrations as a data host for the purpose of
  // trigger queuing. Navigation beacon is sent before the navigation commits,
  // therefore registering source eligible data host when the beacon is sent
  // ensures that triggers registered on the landing page are properly queued in
  // the case that the beacon response is delivered late.
  data_hosts_in_source_mode_++;
}

void AttributionDataHostManagerImpl::NotifyFencedFrameReportingBeaconData(
    BeaconId beacon_id,
    url::Origin reporting_origin,
    const net::HttpResponseHeaders* headers,
    bool is_final_response) {
  auto it = registrations_.find(beacon_id);

  // The registration may no longer be tracked in the event the navigation
  // failed.
  if (it == registrations_.end()) {
    return;
  }

  DCHECK(!it->registrations_complete());
  if (is_final_response) {
    it->CompleteRegistrations();
  }

  absl::optional<SuitableOrigin> suitable_reporting_origin =
      SuitableOrigin::Create(std::move(reporting_origin));
  if (!suitable_reporting_origin) {
    MaybeOnRegistrationsFinished(it);
    return;
  }

  const auto attribution_header = RegistrarAndHeader::Get(headers);
  if (!attribution_header) {
    MaybeOnRegistrationsFinished(it);
    return;
  }

  if (auto* rfh = RenderFrameHostImpl::FromID(it->render_frame_id())) {
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        rfh, blink::mojom::WebFeature::kAttributionFencedFrameReportingBeacon);
  }

  ParseSource(it, std::move(*suitable_reporting_origin), *attribution_header);
}

void AttributionDataHostManagerImpl::OnSourceParsed(
    SourceRegistrationsId id,
    base::FunctionRef<void(const SourceRegistrations&)> handle_result) {
  auto it = registrations_.find(id);

  // The registration may no longer be tracked in the event the navigation
  // failed.
  if (it == registrations_.end()) {
    return;
  }

  it->DecrementPendingSourceData();
  handle_result(*it);
  MaybeOnRegistrationsFinished(it);
}

void AttributionDataHostManagerImpl::OnWebSourceParsed(
    SourceRegistrationsId id,
    const SuitableOrigin& reporting_origin,
    const std::string& header_value,
    data_decoder::DataDecoder::ValueOrError result) {
  OnSourceParsed(id, [&](const SourceRegistrations& registrations) {
    auto source_type = SourceType::kNavigation;
    if (const auto* beacon_id = absl::get_if<BeaconId>(&registrations.data());
        beacon_id && absl::holds_alternative<EventBeaconId>(*beacon_id)) {
      source_type = SourceType::kEvent;
    }

    base::expected<StorableSource, SourceRegistrationError> source =
        base::unexpected(SourceRegistrationError::kInvalidJson);
    if (result.has_value()) {
      if (result->is_dict()) {
        auto registration = attribution_reporting::SourceRegistration::Parse(
            std::move(*result).TakeDict());
        if (registration.has_value()) {
          source.emplace(reporting_origin, std::move(*registration),
                         /*source_time=*/base::Time::Now(),
                         registrations.source_origin(), source_type,
                         registrations.is_within_fenced_frame());
        } else {
          source = base::unexpected(registration.error());
        }
      } else {
        source = base::unexpected(SourceRegistrationError::kRootWrongType);
      }
    }

    if (source.has_value()) {
      attribution_manager_->HandleSource(std::move(*source),
                                         registrations.render_frame_id());

      if (const auto* redirect =
              absl::get_if<SourceRegistrations::NavigationRedirect>(
                  &registrations.data())) {
        base::UmaHistogramEnumeration(
            "Conversions.SourceRegistration.NavigationType.Foreground",
            redirect->nav_type);
      }
    } else {
      attribution_manager_->NotifyFailedSourceRegistration(
          header_value, registrations.source_origin(), reporting_origin,
          source_type, source.error());
      attribution_reporting::RecordSourceRegistrationError(source.error());
    }
  });
}

#if BUILDFLAG(IS_ANDROID)
void AttributionDataHostManagerImpl::OnOsSourceParsed(SourceRegistrationsId id,
                                                      OsParseResult result) {
  OnSourceParsed(id, [&](const SourceRegistrations& registrations) {
    // TODO: Report parsing errors to DevTools.
    if (result.has_value()) {
      GURL registration_url =
          attribution_reporting::ParseOsSourceOrTriggerHeader(*result);

      attribution_manager_->HandleOsRegistration(
          OsRegistration(std::move(registration_url),
                         registrations.source_origin(),
                         registrations.input_event()),
          registrations.render_frame_id());
    }
  });
}
#endif  // BUILDFLAG(IS_ANDROID)

void AttributionDataHostManagerImpl::MaybeOnRegistrationsFinished(
    base::flat_set<SourceRegistrations>::const_iterator it) {
  DCHECK(it != registrations_.end());

  if (it->pending_source_data() > 0u || !it->registrations_complete()) {
    return;
  }

  OnSourceEligibleDataHostFinished(it->register_time());
  registrations_.erase(it);
}

}  // namespace content
