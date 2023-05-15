// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_data_host_manager_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

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
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/os_registration.h"
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
#include "content/browser/attribution_reporting/attribution_os_level_manager.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/os_registration.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/http/http_response_headers.h"
#include "net/http/structured_headers.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/attribution_reporting_runtime_features.h"
#include "services/network/public/cpp/attribution_utils.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/trigger_verification.h"
#include "services/network/public/mojom/attribution.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom.h"
#include "third_party/blink/public/mojom/conversions/attribution_reporting.mojom.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"
#include "url/gurl.h"
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
enum class NavigationDataHostStatus {
  kRegistered = 0,
  kNotFound = 1,
  // Ineligible navigation data hosts (non top-level navigations, same document
  // navigations, etc) are dropped.
  kIneligible = 2,
  kProcessed = 3,

  kMaxValue = kProcessed,
};

void RecordNavigationDataHostStatus(NavigationDataHostStatus event) {
  base::UmaHistogramEnumeration("Conversions.NavigationDataHostStatus3", event);
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class RegisterDataHostOutcome {
  kProcessedImmediately = 0,
  kDeferred = 1,
  kDropped = 2,
  kMaxValue = kDropped,
};

void RecordRegisterDataHostHostOutcome(RegisterDataHostOutcome status) {
  base::UmaHistogramEnumeration("Conversions.RegisterDataHostOutcome", status);
}

enum class Registrar {
  kWeb,
  kOs,
};

const base::TimeDelta kDeferredReceiversTimeout = base::Seconds(10);

constexpr size_t kMaxDeferredReceiversPerNavigation = 30;

struct PendingWebDecode {
  std::string header;
  SuitableOrigin reporting_origin;

  PendingWebDecode(std::string header, SuitableOrigin reporting_origin)
      : header(std::move(header)),
        reporting_origin(std::move(reporting_origin)) {}
};

}  // namespace

class AttributionDataHostManagerImpl::ReceiverContext {
 public:
  ReceiverContext(SuitableOrigin context_origin,
                  RegistrationType registration_type,
                  bool is_within_fenced_frame,
                  AttributionInputEvent input_event,
                  absl::optional<AttributionNavigationType> nav_type,
                  GlobalRenderFrameHostId render_frame_id,
                  absl::optional<int64_t> navigation_id)
      : context_origin_(std::move(context_origin)),
        registration_type_(registration_type),
        is_within_fenced_frame_(is_within_fenced_frame),
        input_event_(std::move(input_event)),
        nav_type_(nav_type),
        render_frame_id_(render_frame_id),
        navigation_id_(navigation_id) {
    DCHECK(!nav_type_ || registration_type_ == RegistrationType::kSource);
    DCHECK(!navigation_id_ || registration_type_ == RegistrationType::kSource);
  }

  ~ReceiverContext() = default;

  ReceiverContext(const ReceiverContext&) = delete;
  ReceiverContext& operator=(const ReceiverContext&) = delete;

  ReceiverContext(ReceiverContext&&) = default;
  ReceiverContext& operator=(ReceiverContext&&) = default;

  const SuitableOrigin& context_origin() const { return context_origin_; }

  RegistrationType registration_type() const { return registration_type_; }

  bool is_within_fenced_frame() const { return is_within_fenced_frame_; }

  absl::optional<int64_t> navigation_id() const { return navigation_id_; }

  absl::optional<AttributionNavigationType> nav_type() const {
    return nav_type_;
  }

  GlobalRenderFrameHostId render_frame_id() const { return render_frame_id_; }

  const AttributionInputEvent& input_event() const { return input_event_; }

 private:
  // Top-level origin the data host was created in.
  // Logically const.
  SuitableOrigin context_origin_;

  // Logically const.
  RegistrationType registration_type_;

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

  // When the receiver is tied to a navigation, we store the navigation_id
  // to be able to bind deferred receivers when it disconnects.
  absl::optional<int64_t> navigation_id_;
};

struct AttributionDataHostManagerImpl::DeferredReceiverTimeout {
  int64_t navigation_id;
  base::TimeTicks timeout_time;

  base::TimeDelta TimeUntilTimeout() const {
    return timeout_time - base::TimeTicks::Now();
  }
};

struct AttributionDataHostManagerImpl::DeferredReceiver {
  mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host;
  ReceiverContext context;
  base::TimeTicks initial_registration_time = base::TimeTicks::Now();
};

struct AttributionDataHostManagerImpl::NavigationDataHost {
  mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host;
  AttributionInputEvent input_event;
};

class AttributionDataHostManagerImpl::SourceRegistrations {
 public:
  struct ForegroundNavigation {
    blink::AttributionSrcToken attribution_src_token;

    // Will not change over the course of the redirect chain.
    AttributionNavigationType nav_type;
    int64_t navigation_id;
  };

  struct Beacon {
    BeaconId id;
    absl::optional<int64_t> navigation_id;
  };

  using Data = absl::variant<ForegroundNavigation, Beacon>;

  SourceRegistrations(SuitableOrigin source_origin,
                      bool is_within_fenced_frame,
                      AttributionInputEvent input_event,
                      GlobalRenderFrameHostId render_frame_id,
                      Data data)
      : source_origin_(std::move(source_origin)),
        is_within_fenced_frame_(is_within_fenced_frame),
        input_event_(std::move(input_event)),
        render_frame_id_(render_frame_id),
        data_(data) {}

  SourceRegistrations(const SourceRegistrations&) = delete;
  SourceRegistrations& operator=(const SourceRegistrations&) = delete;

  SourceRegistrations(SourceRegistrations&&) = default;
  SourceRegistrations& operator=(SourceRegistrations&&) = default;

  const SuitableOrigin& source_origin() const { return source_origin_; }

  bool has_pending_decodes() const {
    return !pending_os_decodes_.empty() || !pending_web_decodes_.empty();
  }

  bool registrations_complete() const { return registrations_complete_; }

  bool is_within_fenced_frame() const { return is_within_fenced_frame_; }

  absl::optional<int64_t> navigation_id() const {
    return absl::visit(
        base::Overloaded{
            [](const ForegroundNavigation& navigation) {
              return absl::make_optional(navigation.navigation_id);
            },
            [](const Beacon& beacon) { return beacon.navigation_id; }},
        data_);
  }

  const AttributionInputEvent& input_event() const { return input_event_; }

  GlobalRenderFrameHostId render_frame_id() const { return render_frame_id_; }

  const Data& data() const { return data_; }

  const base::circular_deque<PendingWebDecode>& pending_web_decodes() const {
    return pending_web_decodes_;
  }

  base::circular_deque<PendingWebDecode>& pending_web_decodes() {
    return pending_web_decodes_;
  }

  const base::circular_deque<std::string>& pending_os_decodes() const {
    return pending_os_decodes_;
  }

  base::circular_deque<std::string>& pending_os_decodes() {
    return pending_os_decodes_;
  }

  bool operator<(const SourceRegistrations& other) const {
    return Id() < other.Id();
  }

  void CompleteRegistrations() {
    DCHECK(!registrations_complete_);
    registrations_complete_ = true;
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
            [](const ForegroundNavigation& navigation) {
              return SourceRegistrationsId(navigation.attribution_src_token);
            },
            [](const Beacon& beacon) {
              return SourceRegistrationsId(beacon.id);
            },
        },
        data_);
  }

 private:
  // Source origin to use for all registrations on a navigation redirect or
  // beacon chain. Will not change over the course of the chain.
  SuitableOrigin source_origin_;

  // True if navigation or beacon has completed.
  bool registrations_complete_ = false;

  // Whether the registration was initiated within a fenced frame.
  bool is_within_fenced_frame_;

  // Input event associated with the navigation.
  // The underlying Java object will be null for event beacons.
  AttributionInputEvent input_event_;

  GlobalRenderFrameHostId render_frame_id_;

  Data data_;

  base::circular_deque<PendingWebDecode> pending_web_decodes_;

  base::circular_deque<std::string> pending_os_decodes_;
};

struct AttributionDataHostManagerImpl::RegistrarAndHeader {
  Registrar registrar;
  std::string header;

  [[nodiscard]] static absl::optional<RegistrarAndHeader> Get(
      const net::HttpResponseHeaders* headers,
      bool cross_app_web_runtime_enabled) {
    if (!headers) {
      return absl::nullopt;
    }

    std::string web_source;
    const bool has_web = headers->GetNormalizedHeader(
        kAttributionReportingRegisterSourceHeader, &web_source);

    std::string os_source;
    // Note that it's important that the browser process check both the
    // base::Feature (which is set from the browser, so trustworthy) and the
    // runtime feature (which can be spoofed in a compromised renderer, so is
    // best-effort).
    const bool has_os =
        cross_app_web_runtime_enabled &&
        base::FeatureList::IsEnabled(
            network::features::kAttributionReportingCrossAppWeb) &&
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

// TODO(anthonygarant): Should we bind all `deferred_receivers_` when the
// `AttributionDataHostManagerImpl` is about to be destroyed?
AttributionDataHostManagerImpl::~AttributionDataHostManagerImpl() = default;

void AttributionDataHostManagerImpl::RegisterDataHost(
    mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host,
    SuitableOrigin context_origin,
    bool is_within_fenced_frame,
    RegistrationType registration_type,
    GlobalRenderFrameHostId render_frame_id,
    int64_t last_navigation_id) {
  ReceiverContext receiver_context(std::move(context_origin), registration_type,
                                   is_within_fenced_frame,
                                   /*input_event=*/AttributionInputEvent(),
                                   /*nav_type=*/absl::nullopt, render_frame_id,
                                   /*navigation_id=*/absl::nullopt);

  switch (registration_type) {
    case RegistrationType::kTrigger:
    case RegistrationType::kSourceOrTrigger:
      // We only defer trigger registrations as handling them before a source
      // can lead to a non-match that could otherwise be one.
      if (auto receivers_it = deferred_receivers_.find(last_navigation_id);
          receivers_it != deferred_receivers_.end()) {
        // We limit the number of deferred receivers to prevent excessive memory
        // usage. In case the limit is reached, we drop the receiver.
        if (receivers_it->second.size() < kMaxDeferredReceiversPerNavigation) {
          RecordRegisterDataHostHostOutcome(RegisterDataHostOutcome::kDeferred);
          receivers_it->second.emplace_back(DeferredReceiver{
              .data_host = std::move(data_host),
              .context = std::move(receiver_context),
          });
        } else {
          RecordRegisterDataHostHostOutcome(RegisterDataHostOutcome::kDropped);
        }
        return;
      }
      break;
    case RegistrationType::kSource:
      break;
  }

  RecordRegisterDataHostHostOutcome(
      RegisterDataHostOutcome::kProcessedImmediately);
  receivers_.Add(this, std::move(data_host), std::move(receiver_context));
}

bool AttributionDataHostManagerImpl::RegisterNavigationDataHost(
    mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host,
    const blink::AttributionSrcToken& attribution_src_token,
    AttributionInputEvent input_event) {
  auto [it, inserted] = navigation_data_host_map_.try_emplace(
      attribution_src_token,
      NavigationDataHost{.data_host = std::move(data_host),
                         .input_event = std::move(input_event)});
  // Should only be possible with a misbehaving renderer.
  if (!inserted) {
    return false;
  }

  RecordNavigationDataHostStatus(NavigationDataHostStatus::kRegistered);
  return true;
}

void AttributionDataHostManagerImpl::ParseSource(
    base::flat_set<SourceRegistrations>::iterator it,
    SuitableOrigin reporting_origin,
    RegistrarAndHeader header) {
  DCHECK(it != registrations_.end());

  switch (header.registrar) {
    case Registrar::kWeb:
      if (!network::HasAttributionWebSupport(
              AttributionManager::GetSupport())) {
        // TODO(crbug.com/1426450): Report a DevTools issue.
        MaybeOnRegistrationsFinished(it);
        break;
      }
      it->pending_web_decodes().emplace_back(std::move(header.header),
                                             std::move(reporting_origin));
      // Only perform the decode if it is the only one in the queue. Otherwise,
      // there's already an async decode in progress.
      if (it->pending_web_decodes().size() == 1) {
        HandleNextWebDecode(*it);
      }
      break;
    case Registrar::kOs:
      if (!network::HasAttributionOsSupport(AttributionManager::GetSupport())) {
        // TODO(crbug.com/1426450): Report a DevTools issue.
        MaybeOnRegistrationsFinished(it);
        break;
      }
      if (auto* rfh = RenderFrameHostImpl::FromID(it->render_frame_id())) {
        GetContentClient()->browser()->LogWebFeatureForCurrentPage(
            rfh, blink::mojom::WebFeature::kAttributionReportingCrossAppWeb);
      }

      it->pending_os_decodes().emplace_back(std::move(header.header));
      // Only perform the decode if it is the only one in the queue. Otherwise,
      // there's already an async decode in progress.
      if (it->pending_os_decodes().size() == 1) {
        HandleNextOsDecode(*it);
      }
      break;
  }
}

void AttributionDataHostManagerImpl::HandleNextWebDecode(
    const SourceRegistrations& registrations) {
  DCHECK(!registrations.pending_web_decodes().empty());

  const auto& pending_decode = registrations.pending_web_decodes().front();

  data_decoder_.ParseJson(
      pending_decode.header,
      base::BindOnce(&AttributionDataHostManagerImpl::OnWebSourceParsed,
                     weak_factory_.GetWeakPtr(), registrations.Id()));
}

void AttributionDataHostManagerImpl::HandleNextOsDecode(
    const SourceRegistrations& registrations) {
  DCHECK(!registrations.pending_os_decodes().empty());

  const auto& header = registrations.pending_os_decodes().front();

  data_decoder_.ParseStructuredHeaderList(
      header, base::BindOnce(&AttributionDataHostManagerImpl::OnOsSourceParsed,
                             weak_factory_.GetWeakPtr(), registrations.Id()));
}

void AttributionDataHostManagerImpl::NotifyNavigationRegistrationStarted(
    const blink::AttributionSrcToken& attribution_src_token,
    const SuitableOrigin& source_origin,
    AttributionNavigationType nav_type,
    bool is_within_fenced_frame,
    GlobalRenderFrameHostId render_frame_id,
    int64_t navigation_id) {
  // A navigation-associated interface is used for
  // `blink::mojom::ConversionHost` and an `AssociatedReceiver` is used on the
  // browser side, therefore it's guaranteed that
  // `AttributionHost::RegisterNavigationHost()` is called before
  // `AttributionHost::DidStartNavigation()`.
  if (auto it = navigation_data_host_map_.find(attribution_src_token);
      it != navigation_data_host_map_.end()) {
    // We defer trigger registrations until background registrations complete;
    // when the navigation data host disconnects.
    auto [_, inserted] =
        ongoing_background_registrations_.emplace(navigation_id);
    DCHECK(inserted);
    MaybeSetupDeferredReceivers(navigation_id);

    receivers_.Add(this, std::move(it->second.data_host),
                   ReceiverContext(source_origin, RegistrationType::kSource,
                                   is_within_fenced_frame,
                                   std::move(it->second.input_event), nav_type,
                                   render_frame_id, navigation_id));

    navigation_data_host_map_.erase(it);
    RecordNavigationDataHostStatus(NavigationDataHostStatus::kProcessed);
  } else {
    RecordNavigationDataHostStatus(NavigationDataHostStatus::kNotFound);
  }
}

void AttributionDataHostManagerImpl::NotifyNavigationRegistrationData(
    const blink::AttributionSrcToken& attribution_src_token,
    const net::HttpResponseHeaders* headers,
    SuitableOrigin reporting_origin,
    const SuitableOrigin& source_origin,
    AttributionInputEvent input_event,
    AttributionNavigationType nav_type,
    bool is_within_fenced_frame,
    GlobalRenderFrameHostId render_frame_id,
    int64_t navigation_id,
    network::AttributionReportingRuntimeFeatures runtime_features,
    bool is_final_response) {
  if (auto header = RegistrarAndHeader::Get(
          headers,
          runtime_features.Has(
              network::AttributionReportingRuntimeFeature::kCrossAppWeb))) {
    auto [it, inserted] = registrations_.emplace(
        source_origin, is_within_fenced_frame, std::move(input_event),
        render_frame_id,
        SourceRegistrations::ForegroundNavigation{
            .attribution_src_token = attribution_src_token,
            .nav_type = nav_type,
            .navigation_id = navigation_id,
        });
    DCHECK(!it->registrations_complete());

    // We defer trigger registrations until source parsing completes.
    MaybeSetupDeferredReceivers(navigation_id);
    ParseSource(it, std::move(reporting_origin), std::move(*header));
  }

  if (is_final_response) {
    // The eligible data host should have been bound in
    // `NotifyNavigationStartedForDataHost()`.
    // For non-top level navigation and same document navigation,
    // `AttributionHost::RegisterNavigationDataHost()` will be called but not
    // `NotifyNavigationStartedForDataHost()`, therefore these navigations would
    // still be tracked.
    if (auto it = navigation_data_host_map_.find(attribution_src_token);
        it != navigation_data_host_map_.end()) {
      navigation_data_host_map_.erase(it);
      RecordNavigationDataHostStatus(NavigationDataHostStatus::kIneligible);
    }

    // We are not guaranteed to be processing registrations for a given
    // navigation.
    if (auto it = registrations_.find(attribution_src_token);
        it != registrations_.end()) {
      it->CompleteRegistrations();
      MaybeOnRegistrationsFinished(it);
    }
  }
}

const AttributionDataHostManagerImpl::ReceiverContext*
AttributionDataHostManagerImpl::GetReceiverContextForSource() {
  const ReceiverContext& context = receivers_.current_context();

  if (context.registration_type() == RegistrationType::kTrigger) {
    mojo::ReportBadMessage("AttributionDataHost: Not eligible for source.");
    return nullptr;
  }

  return &context;
}

const AttributionDataHostManagerImpl::ReceiverContext*
AttributionDataHostManagerImpl::GetReceiverContextForTrigger() {
  const ReceiverContext& context = receivers_.current_context();

  if (context.registration_type() == RegistrationType::kSource) {
    mojo::ReportBadMessage("AttributionDataHost: Not eligible for trigger.");
    return nullptr;
  }

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
                     /*source_origin=*/context->context_origin(), source_type,
                     context->is_within_fenced_frame()),
      context->render_frame_id());
}

void AttributionDataHostManagerImpl::TriggerDataAvailable(
    attribution_reporting::SuitableOrigin reporting_origin,
    attribution_reporting::TriggerRegistration data,
    absl::optional<network::TriggerVerification> verification) {
  // This is validated by the Mojo typemapping.
  DCHECK(reporting_origin.IsValid());

  const ReceiverContext* context = GetReceiverContextForTrigger();
  if (!context) {
    return;
  }

  attribution_manager_->HandleTrigger(
      AttributionTrigger(std::move(reporting_origin), std::move(data),
                         /*destination_origin=*/context->context_origin(),
                         std::move(verification),
                         context->is_within_fenced_frame()),
      context->render_frame_id());
}

void AttributionDataHostManagerImpl::OsSourceDataAvailable(
    std::vector<GURL> registration_urls) {
  const ReceiverContext* context = GetReceiverContextForSource();
  if (!context) {
    return;
  }

  for (GURL& url : registration_urls) {
    attribution_manager_->HandleOsRegistration(
        OsRegistration(std::move(url), context->context_origin(),
                       context->input_event()),
        context->render_frame_id());
  }
}

void AttributionDataHostManagerImpl::OsTriggerDataAvailable(
    std::vector<GURL> registration_urls) {
  const ReceiverContext* context = GetReceiverContextForTrigger();
  if (!context) {
    return;
  }

  for (GURL& url : registration_urls) {
    attribution_manager_->HandleOsRegistration(
        OsRegistration(std::move(url), context->context_origin(),
                       /*input_event=*/absl::nullopt),
        context->render_frame_id());
  }
}

void AttributionDataHostManagerImpl::OnReceiverDisconnected() {
  const ReceiverContext& context = receivers_.current_context();

  if (context.navigation_id().has_value()) {
    if (auto it = ongoing_background_registrations_.find(
            context.navigation_id().value());
        it != ongoing_background_registrations_.end()) {
      ongoing_background_registrations_.erase(it);
      MaybeBindDeferredReceivers(context.navigation_id().value(),
                                 /*due_to_timeout=*/false);
    }
  }
}

void AttributionDataHostManagerImpl::NotifyFencedFrameReportingBeaconStarted(
    BeaconId beacon_id,
    absl::optional<int64_t> navigation_id,
    SuitableOrigin source_origin,
    bool is_within_fenced_frame,
    AttributionInputEvent input_event,
    GlobalRenderFrameHostId render_frame_id) {
  if (navigation_id.has_value()) {
    MaybeSetupDeferredReceivers(navigation_id.value());
  }

  auto [it, inserted] =
      registrations_.emplace(std::move(source_origin), is_within_fenced_frame,
                             std::move(input_event), render_frame_id,
                             SourceRegistrations::Beacon{
                                 .id = beacon_id,
                                 .navigation_id = navigation_id,
                             });
  DCHECK(inserted);
}

void AttributionDataHostManagerImpl::NotifyFencedFrameReportingBeaconData(
    BeaconId beacon_id,
    network::AttributionReportingRuntimeFeatures runtime_features,
    url::Origin reporting_origin,
    const net::HttpResponseHeaders* headers,
    bool is_final_response) {
  auto it = registrations_.find(beacon_id);
  // This may happen if validation failed in
  // `AttributionHost::NotifyFencedFrameReportingBeaconStarted()` and therefore
  // not being tracked.
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

  auto attribution_header = RegistrarAndHeader::Get(
      headers, runtime_features.Has(
                   network::AttributionReportingRuntimeFeature::kCrossAppWeb));
  if (!attribution_header) {
    MaybeOnRegistrationsFinished(it);
    return;
  }

  if (auto* rfh = RenderFrameHostImpl::FromID(it->render_frame_id())) {
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        rfh, blink::mojom::WebFeature::kAttributionFencedFrameReportingBeacon);
  }

  ParseSource(it, std::move(*suitable_reporting_origin),
              std::move(*attribution_header));
}

void AttributionDataHostManagerImpl::OnWebSourceParsed(
    SourceRegistrationsId id,
    data_decoder::DataDecoder::ValueOrError result) {
  auto registrations = registrations_.find(id);
  DCHECK(registrations != registrations_.end());

  DCHECK(!registrations->pending_web_decodes().empty());
  {
    const auto& pending_decode = registrations->pending_web_decodes().front();

    auto source_type = SourceType::kNavigation;
    if (const auto* beacon =
            absl::get_if<SourceRegistrations::Beacon>(&registrations->data());
        beacon && !beacon->navigation_id.has_value()) {
      source_type = SourceType::kEvent;
    }

    auto source =
        [&]() -> base::expected<StorableSource, SourceRegistrationError> {
      if (!result.has_value()) {
        return base::unexpected(SourceRegistrationError::kInvalidJson);
      }
      if (!result->is_dict()) {
        return base::unexpected(SourceRegistrationError::kRootWrongType);
      }
      auto registration = attribution_reporting::SourceRegistration::Parse(
          std::move(*result).TakeDict());
      if (!registration.has_value()) {
        return base::unexpected(registration.error());
      }
      return StorableSource(pending_decode.reporting_origin,
                            std::move(*registration),
                            registrations->source_origin(), source_type,
                            registrations->is_within_fenced_frame());
    }();

    if (source.has_value()) {
      attribution_manager_->HandleSource(std::move(*source),
                                         registrations->render_frame_id());

      if (const auto* navigation =
              absl::get_if<SourceRegistrations::ForegroundNavigation>(
                  &registrations->data())) {
        base::UmaHistogramEnumeration(
            "Conversions.SourceRegistration.NavigationType.Foreground",
            navigation->nav_type);
      }
    } else {
      attribution_manager_->NotifyFailedSourceRegistration(
          pending_decode.header, registrations->source_origin(),
          pending_decode.reporting_origin, source_type, source.error());
      attribution_reporting::RecordSourceRegistrationError(source.error());
    }
  }

  registrations->pending_web_decodes().pop_front();

  if (!registrations->pending_web_decodes().empty()) {
    HandleNextWebDecode(*registrations);
  } else {
    MaybeOnRegistrationsFinished(registrations);
  }
}

void AttributionDataHostManagerImpl::OnOsSourceParsed(SourceRegistrationsId id,
                                                      OsParseResult result) {
  auto registrations = registrations_.find(id);
  DCHECK(registrations != registrations_.end());

  DCHECK(!registrations->pending_os_decodes().empty());
  {
    // TODO: Report parsing errors to DevTools.
    if (result.has_value()) {
      std::vector<GURL> registration_urls =
          attribution_reporting::ParseOsSourceOrTriggerHeader(*result);

      for (GURL& url : registration_urls) {
        attribution_manager_->HandleOsRegistration(
            OsRegistration(std::move(url), registrations->source_origin(),
                           registrations->input_event()),
            registrations->render_frame_id());
      }
    }
  }

  registrations->pending_os_decodes().pop_front();

  if (!registrations->pending_os_decodes().empty()) {
    HandleNextOsDecode(*registrations);
  } else {
    MaybeOnRegistrationsFinished(registrations);
  }
}

void AttributionDataHostManagerImpl::MaybeOnRegistrationsFinished(
    base::flat_set<SourceRegistrations>::const_iterator it) {
  DCHECK(it != registrations_.end());
  if (it->has_pending_decodes() || !it->registrations_complete()) {
    return;
  }

  absl::optional<int64_t> navigation_id = it->navigation_id();
  registrations_.erase(it);
  if (navigation_id.has_value()) {
    MaybeBindDeferredReceivers(navigation_id.value(), /*due_to_timeout=*/false);
  }
}

void AttributionDataHostManagerImpl::StartDeferredReceiversTimeoutTimer(
    base::TimeDelta delay) {
  DCHECK(!deferred_receivers_timeouts_.empty());
  deferred_receivers_timeouts_timer_.Start(
      FROM_HERE, delay,
      base::BindOnce(
          &AttributionDataHostManagerImpl::ProcessDeferredReceiversTimeout,
          weak_factory_.GetWeakPtr()));
}

void AttributionDataHostManagerImpl::ProcessDeferredReceiversTimeout() {
  DCHECK(!deferred_receivers_timeouts_.empty());
  {
    const DeferredReceiverTimeout& deferred_receiver =
        deferred_receivers_timeouts_.front();
    DCHECK_LE(deferred_receiver.timeout_time, base::TimeTicks::Now());
    MaybeBindDeferredReceivers(deferred_receiver.navigation_id,
                               /*due_to_timeout=*/true);
  }
  deferred_receivers_timeouts_.pop_front();

  while (!deferred_receivers_timeouts_.empty()) {
    const DeferredReceiverTimeout& t = deferred_receivers_timeouts_.front();
    if (!deferred_receivers_.contains(t.navigation_id)) {
      // We don't start a timer for deferred receivers that have already been
      // bound.
      deferred_receivers_timeouts_.pop_front();
    } else {
      StartDeferredReceiversTimeoutTimer(t.TimeUntilTimeout());
      break;
    }
  }
}

void AttributionDataHostManagerImpl::MaybeSetupDeferredReceivers(
    int64_t navigation_id) {
  auto [it, inserted] = deferred_receivers_.try_emplace(
      navigation_id, std::vector<DeferredReceiver>());

  if (!inserted) {
    // We already have deferred receivers linked to the navigation.
    return;
  }

  deferred_receivers_timeouts_.emplace_back(DeferredReceiverTimeout{
      .navigation_id = navigation_id,
      .timeout_time = base::TimeTicks::Now() + kDeferredReceiversTimeout,
  });
  if (!deferred_receivers_timeouts_timer_.IsRunning()) {
    StartDeferredReceiversTimeoutTimer(kDeferredReceiversTimeout);
  }
}

void AttributionDataHostManagerImpl::MaybeBindDeferredReceivers(
    int64_t navigation_id,
    bool due_to_timeout) {
  if (due_to_timeout) {
    // We cleanup and bind the deferred receivers on timeout
    if (const auto& it = ongoing_background_registrations_.find(navigation_id);
        it != ongoing_background_registrations_.end()) {
      ongoing_background_registrations_.erase(it);
    }
  } else {
    // We skip binding the receiver if any registrations are still ongoing
    if (ongoing_background_registrations_.find(navigation_id) !=
        ongoing_background_registrations_.end()) {
      return;
    }

    for (const auto& registration : registrations_) {
      if (registration.navigation_id() == navigation_id) {
        return;
      }
    }
  }

  if (auto it = deferred_receivers_.find(navigation_id);
      it != deferred_receivers_.end()) {
    base::UmaHistogramBoolean(
        "Conversions.DeferredDataHostProcessedAfterTimeout", due_to_timeout);
    for (auto& deferred_receiver : it->second) {
      base::UmaHistogramMediumTimes(
          "Conversions.ProcessRegisterDataHostDelay",
          base::TimeTicks::Now() - deferred_receiver.initial_registration_time);
      receivers_.Add(this, std::move(deferred_receiver.data_host),
                     std::move(deferred_receiver.context));
    }

    deferred_receivers_.erase(it);
  }
}

}  // namespace content
