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
#include "base/memory/raw_ref.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/attribution_reporting/os_registration.h"
#include "components/attribution_reporting/registration_eligibility.mojom.h"
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
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using ::attribution_reporting::SuitableOrigin;
using ::attribution_reporting::mojom::RegistrationEligibility;
using ::attribution_reporting::mojom::SourceRegistrationError;
using ::attribution_reporting::mojom::SourceType;

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

const base::TimeDelta kDeferredReceiversTimeout = base::Seconds(10);

constexpr size_t kMaxDeferredReceiversPerNavigation = 30;

class RegistrationNavigationContext {
 public:
  RegistrationNavigationContext(int64_t navigation_id,
                                AttributionInputEvent input_event)
      : navigation_id_(navigation_id), input_event_(std::move(input_event)) {}

  ~RegistrationNavigationContext() = default;

  RegistrationNavigationContext(const RegistrationNavigationContext&) = delete;
  RegistrationNavigationContext& operator=(
      const RegistrationNavigationContext&) = delete;

  RegistrationNavigationContext(RegistrationNavigationContext&&) = default;
  RegistrationNavigationContext& operator=(RegistrationNavigationContext&&) =
      default;

  int64_t navigation_id() const { return navigation_id_; }

  const AttributionInputEvent& input_event() const { return input_event_; }

 private:
  // We store the navigation_id on the registration context to support trigger
  // buffering. Will not change over the course of the redirect chain.
  // Logically const.
  int64_t navigation_id_;

  // Input event associated with the navigation for navigation source data
  // hosts. The underlying Java object will be null for event sources.
  // Logically const.
  AttributionInputEvent input_event_;
};

void LogAuditIssue(GlobalRenderFrameHostId render_frame_id,
                   blink::mojom::AttributionReportingIssueType violation_type,
                   const GURL& request_url,
                   const std::string& request_devtools_id,
                   absl::optional<std::string> invalid_parameter) {
  auto* render_frame_host = RenderFrameHost::FromID(render_frame_id);
  if (!render_frame_host) {
    return;
  }

  auto issue = blink::mojom::InspectorIssueInfo::New();

  issue->code = blink::mojom::InspectorIssueCode::kAttributionReportingIssue;

  auto details = blink::mojom::AttributionReportingIssueDetails::New();
  details->violation_type = violation_type;
  if (invalid_parameter.has_value()) {
    details->invalid_parameter = std::move(invalid_parameter.value());
  }

  auto affected_request = blink::mojom::AffectedRequest::New();
  affected_request->request_id = request_devtools_id;
  affected_request->url = request_url.spec();
  details->request = std::move(affected_request);

  issue->details = blink::mojom::InspectorIssueDetails::New();
  issue->details->attribution_reporting_issue_details = std::move(details);

  render_frame_host->ReportInspectorIssue(std::move(issue));
}

}  // namespace

class AttributionDataHostManagerImpl::RegistrationContext {
 public:
  RegistrationContext(SuitableOrigin context_origin,
                      RegistrationEligibility registration_eligibility,
                      bool is_within_fenced_frame,
                      GlobalRenderFrameHostId render_frame_id,
                      absl::optional<std::string> devtools_request_id,
                      absl::optional<RegistrationNavigationContext> navigation)
      : context_origin_(std::move(context_origin)),
        registration_eligibility_(registration_eligibility),
        is_within_fenced_frame_(is_within_fenced_frame),
        render_frame_id_(render_frame_id),
        devtools_request_id_(std::move(devtools_request_id)),
        navigation_(std::move(navigation)) {
    CHECK(!navigation_.has_value() ||
          registration_eligibility_ == RegistrationEligibility::kSource);
  }

  ~RegistrationContext() = default;

  RegistrationContext(const RegistrationContext&) = delete;
  RegistrationContext& operator=(const RegistrationContext&) = delete;

  RegistrationContext(RegistrationContext&&) = default;
  RegistrationContext& operator=(RegistrationContext&&) = default;

  const SuitableOrigin& context_origin() const { return context_origin_; }

  RegistrationEligibility registration_eligibility() const {
    return registration_eligibility_;
  }

  bool is_within_fenced_frame() const { return is_within_fenced_frame_; }

  const absl::optional<std::string>& devtools_request_id() const {
    return devtools_request_id_;
  }

  GlobalRenderFrameHostId render_frame_id() const { return render_frame_id_; }

  const absl::optional<RegistrationNavigationContext>& navigation() const {
    return navigation_;
  }

 private:
  // Top-level origin the data host was created in.
  // Logically const.
  SuitableOrigin context_origin_;

  // Logically const.
  RegistrationEligibility registration_eligibility_;

  // Whether the attribution is registered within a fenced frame tree.
  // Logically const.
  bool is_within_fenced_frame_;

  // The ID of the topmost render frame host.
  // Logically const.
  GlobalRenderFrameHostId render_frame_id_;

  // For sources & triggers received through the data host, issues are
  // identified and reported in blink. As such, we don't need to plumb the
  // devtools request ID.
  absl::optional<std::string> devtools_request_id_;

  // When the registration is tied to a navigation, we store additional context
  // on the navigation.
  absl::optional<RegistrationNavigationContext> navigation_;
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
  RegistrationContext context;
  base::TimeTicks initial_registration_time = base::TimeTicks::Now();
};

struct AttributionDataHostManagerImpl::HeaderPendingDecode {
  std::string header;
  SuitableOrigin reporting_origin;
  GURL reporting_url;

  HeaderPendingDecode(std::string header,
                      SuitableOrigin reporting_origin,
                      GURL reporting_url)
      : header(std::move(header)),
        reporting_origin(std::move(reporting_origin)),
        reporting_url(std::move(reporting_url)) {
    DCHECK_EQ(url::Origin::Create(this->reporting_origin->GetURL()),
              url::Origin::Create(this->reporting_url));
  }
};

class AttributionDataHostManagerImpl::SourceRegistrations {
 public:
  SourceRegistrations(SourceRegistrationsId id, RegistrationContext context)
      : id_(id), context_(std::move(context)) {}

  SourceRegistrations(const SourceRegistrations&) = delete;
  SourceRegistrations& operator=(const SourceRegistrations&) = delete;

  SourceRegistrations(SourceRegistrations&&) = default;
  SourceRegistrations& operator=(SourceRegistrations&&) = default;

  const SuitableOrigin& source_origin() const {
    return context_.context_origin();
  }

  bool has_pending_decodes() const {
    return !pending_os_decodes_.empty() || !pending_web_decodes_.empty();
  }

  bool registrations_complete() const { return registrations_complete_; }

  bool is_within_fenced_frame() const {
    return context_.is_within_fenced_frame();
  }

  absl::optional<int64_t> navigation_id() const {
    if (context_.navigation().has_value()) {
      return context_.navigation()->navigation_id();
    }

    return absl::nullopt;
  }

  const AttributionInputEvent* input_event() const {
    if (context_.navigation().has_value()) {
      return &context_.navigation()->input_event();
    }

    return nullptr;
  }

  GlobalRenderFrameHostId render_frame_id() const {
    return context_.render_frame_id();
  }

  const absl::optional<std::string>& devtools_request_id() const {
    return context_.devtools_request_id();
  }

  const base::circular_deque<HeaderPendingDecode>& pending_web_decodes() const {
    return pending_web_decodes_;
  }

  base::circular_deque<HeaderPendingDecode>& pending_web_decodes() {
    return pending_web_decodes_;
  }

  const base::circular_deque<HeaderPendingDecode>& pending_os_decodes() const {
    return pending_os_decodes_;
  }

  base::circular_deque<HeaderPendingDecode>& pending_os_decodes() {
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

  SourceRegistrationsId Id() const { return id_; }

 private:
  // True if navigation or beacon has completed.
  bool registrations_complete_ = false;

  SourceRegistrationsId id_;

  base::circular_deque<HeaderPendingDecode> pending_web_decodes_;

  base::circular_deque<HeaderPendingDecode> pending_os_decodes_;

  RegistrationContext context_;
};

enum class AttributionDataHostManagerImpl::Registrar {
  kWeb,
  kOs,
};

struct AttributionDataHostManagerImpl::RegistrarAndHeader {
  Registrar registrar;
  std::string header;

  [[nodiscard]] static absl::optional<
      base::expected<RegistrarAndHeader,
                     blink::mojom::AttributionReportingIssueType>>
  Get(const net::HttpResponseHeaders* headers,
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
    if (has_web && has_os) {
      return base::unexpected(
          blink::mojom::AttributionReportingIssueType::kWebAndOsHeaders);
    }

    if (has_web) {
      return RegistrarAndHeader{.registrar = Registrar::kWeb,
                                .header = std::move(web_source)};
    }
    if (has_os) {
      return RegistrarAndHeader{.registrar = Registrar::kOs,
                                .header = std::move(os_source)};
    }

    return absl::nullopt;
  }
};

AttributionDataHostManagerImpl::AttributionDataHostManagerImpl(
    AttributionManager* attribution_manager)
    : attribution_manager_(
          raw_ref<AttributionManager>::from_ptr(attribution_manager)) {
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
    RegistrationEligibility registration_eligibility,
    GlobalRenderFrameHostId render_frame_id,
    int64_t last_navigation_id) {
  RegistrationContext receiver_context(
      std::move(context_origin), registration_eligibility,
      is_within_fenced_frame, render_frame_id,
      /*devtools_request_id=*/absl::nullopt, /*navigation=*/absl::nullopt);

  switch (registration_eligibility) {
    case RegistrationEligibility::kTrigger:
    case RegistrationEligibility::kSourceOrTrigger:
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
    case RegistrationEligibility::kSource:
      break;
  }

  RecordRegisterDataHostHostOutcome(
      RegisterDataHostOutcome::kProcessedImmediately);
  receivers_.Add(this, std::move(data_host), std::move(receiver_context));
}

bool AttributionDataHostManagerImpl::RegisterNavigationDataHost(
    mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host,
    const blink::AttributionSrcToken& attribution_src_token) {
  auto [it, inserted] = navigation_data_host_map_.try_emplace(
      attribution_src_token, std::move(data_host));
  // Should only be possible with a misbehaving renderer.
  if (!inserted) {
    return false;
  }

  RecordNavigationDataHostStatus(NavigationDataHostStatus::kRegistered);
  return true;
}

void AttributionDataHostManagerImpl::ParseSource(
    base::flat_set<SourceRegistrations>::iterator it,
    HeaderPendingDecode pending_decode,
    Registrar registrar) {
  DCHECK(it != registrations_.end());

  switch (registrar) {
    case Registrar::kWeb:
      if (!network::HasAttributionWebSupport(
              AttributionManager::GetSupport())) {
        CHECK(it->devtools_request_id());
        LogAuditIssue(
            it->render_frame_id(),
            /*violation_type=*/
            blink::mojom::AttributionReportingIssueType::kNoWebOrOsSupport,
            /*request_url=*/pending_decode.reporting_url,
            *it->devtools_request_id(),
            /*invalid_parameter=*/absl::nullopt);
        MaybeOnRegistrationsFinished(it);
        break;
      }
      it->pending_web_decodes().emplace_back(std::move(pending_decode));
      // Only perform the decode if it is the only one in the queue. Otherwise,
      // there's already an async decode in progress.
      if (it->pending_web_decodes().size() == 1) {
        HandleNextWebDecode(*it);
      }
      break;
    case Registrar::kOs:
      if (!network::HasAttributionOsSupport(AttributionManager::GetSupport())) {
        CHECK(it->devtools_request_id());
        LogAuditIssue(
            it->render_frame_id(),
            /*violation_type=*/
            blink::mojom::AttributionReportingIssueType::kNoWebOrOsSupport,
            /*request_url=*/pending_decode.reporting_url,
            *it->devtools_request_id(),
            /*invalid_parameter=*/absl::nullopt);
        MaybeOnRegistrationsFinished(it);
        break;
      }
      if (auto* rfh = RenderFrameHostImpl::FromID(it->render_frame_id())) {
        GetContentClient()->browser()->LogWebFeatureForCurrentPage(
            rfh, blink::mojom::WebFeature::kAttributionReportingCrossAppWeb);
      }

      it->pending_os_decodes().emplace_back(std::move(pending_decode));
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

  const auto& pending_decode = registrations.pending_os_decodes().front();

  data_decoder_.ParseStructuredHeaderList(
      pending_decode.header,
      base::BindOnce(&AttributionDataHostManagerImpl::OnOsSourceParsed,
                     weak_factory_.GetWeakPtr(), registrations.Id()));
}

void AttributionDataHostManagerImpl::NotifyNavigationRegistrationStarted(
    const blink::AttributionSrcToken& attribution_src_token,
    AttributionInputEvent input_event,
    const SuitableOrigin& source_origin,
    bool is_within_fenced_frame,
    GlobalRenderFrameHostId render_frame_id,
    int64_t navigation_id,
    std::string devtools_request_id) {
  auto [_, registration_inserted] = registrations_.emplace(
      SourceRegistrationsId(attribution_src_token),
      RegistrationContext(
          /*context_origin=*/source_origin, RegistrationEligibility::kSource,
          is_within_fenced_frame, render_frame_id,
          std::move(devtools_request_id),
          RegistrationNavigationContext(navigation_id, input_event)));
  DCHECK(registration_inserted);
  MaybeSetupDeferredReceivers(navigation_id);

  // A navigation-associated interface is used for
  // `blink::mojom::ConversionHost` and an `AssociatedReceiver` is used on the
  // browser side, therefore it's guaranteed that
  // `AttributionHost::RegisterNavigationHost()` is called before
  // `AttributionHost::DidStartNavigation()`.
  if (auto it = navigation_data_host_map_.find(attribution_src_token);
      it != navigation_data_host_map_.end()) {
    // We defer trigger registrations until background registrations complete;
    // when the navigation data host disconnects.
    auto [__, inserted] =
        ongoing_background_registrations_.emplace(navigation_id);
    DCHECK(inserted);

    receivers_.Add(this, std::move(it->second),
                   RegistrationContext(
                       /*context_origin=*/source_origin,
                       RegistrationEligibility::kSource, is_within_fenced_frame,
                       render_frame_id, /*devtools_request_id=*/absl::nullopt,
                       RegistrationNavigationContext(navigation_id,
                                                     std::move(input_event))));

    navigation_data_host_map_.erase(it);
    RecordNavigationDataHostStatus(NavigationDataHostStatus::kProcessed);
  } else {
    RecordNavigationDataHostStatus(NavigationDataHostStatus::kNotFound);
  }
}

bool AttributionDataHostManagerImpl::NotifyNavigationRegistrationData(
    const blink::AttributionSrcToken& attribution_src_token,
    const net::HttpResponseHeaders* headers,
    GURL reporting_url,
    network::AttributionReportingRuntimeFeatures runtime_features) {
  auto reporting_origin =
      SuitableOrigin::Create(url::Origin::Create(reporting_url));
  CHECK(reporting_origin);

  auto header = RegistrarAndHeader::Get(
      headers, runtime_features.Has(
                   network::AttributionReportingRuntimeFeature::kCrossAppWeb));
  if (!header.has_value()) {
    return false;
  }

  auto it = registrations_.find(attribution_src_token);
  CHECK(it != registrations_.end());
  CHECK(!it->registrations_complete());

  if (!header.value().has_value()) {
    CHECK(it->devtools_request_id());
    LogAuditIssue(it->render_frame_id(), /*violation_type=*/header->error(),
                  /*request_url=*/reporting_url, *it->devtools_request_id(),
                  /*invalid_parameter=*/absl::nullopt);
    return false;
  }

  ParseSource(it,
              HeaderPendingDecode(std::move(header->value().header),
                                  std::move(reporting_origin.value()),
                                  std::move(reporting_url)),
              header->value().registrar);
  return true;
}

void AttributionDataHostManagerImpl::NotifyNavigationRegistrationCompleted(
    const blink::AttributionSrcToken& attribution_src_token) {
  // The eligible data host should have been bound in
  // `NotifyNavigationRegistrationStarted()`. For non-top level navigation and
  // same document navigation, `AttributionHost::RegisterNavigationDataHost()`
  // will be called but not `NotifyNavigationRegistrationStarted()`, therefore
  // these navigations would still be tracked.
  if (auto it = navigation_data_host_map_.find(attribution_src_token);
      it != navigation_data_host_map_.end()) {
    navigation_data_host_map_.erase(it);
    RecordNavigationDataHostStatus(NavigationDataHostStatus::kIneligible);
  }

  // It is possible to have no registration stored if
  // `NotifyNavigationRegistrationStarted` wasn't previously called for this
  // token. `NotifyNavigationRegistrationCompleted` is still called for these
  // requests to the support cleanup above of `navigation_data_host_map_`.
  if (auto it = registrations_.find(attribution_src_token);
      it != registrations_.end()) {
    it->CompleteRegistrations();
    MaybeOnRegistrationsFinished(it);
  }
}

const AttributionDataHostManagerImpl::RegistrationContext*
AttributionDataHostManagerImpl::GetReceiverRegistrationContextForSource() {
  const RegistrationContext& context = receivers_.current_context();

  if (context.registration_eligibility() == RegistrationEligibility::kTrigger) {
    mojo::ReportBadMessage("AttributionDataHost: Not eligible for source.");
    return nullptr;
  }

  return &context;
}

const AttributionDataHostManagerImpl::RegistrationContext*
AttributionDataHostManagerImpl::GetReceiverRegistrationContextForTrigger() {
  const RegistrationContext& context = receivers_.current_context();

  if (context.registration_eligibility() == RegistrationEligibility::kSource) {
    mojo::ReportBadMessage("AttributionDataHost: Not eligible for trigger.");
    return nullptr;
  }

  return &context;
}

void AttributionDataHostManagerImpl::SourceDataAvailable(
    SuitableOrigin reporting_origin,
    attribution_reporting::SourceRegistration data) {
  // This is validated by the Mojo typemapping.
  DCHECK(reporting_origin.IsValid());

  const RegistrationContext* context =
      GetReceiverRegistrationContextForSource();
  if (!context) {
    return;
  }

  auto source_type = SourceType::kEvent;
  if (context->navigation().has_value()) {
    source_type = SourceType::kNavigation;
  }

  attribution_manager_->HandleSource(
      StorableSource(std::move(reporting_origin), std::move(data),
                     /*source_origin=*/context->context_origin(), source_type,
                     context->is_within_fenced_frame()),
      context->render_frame_id());
}

void AttributionDataHostManagerImpl::TriggerDataAvailable(
    SuitableOrigin reporting_origin,
    attribution_reporting::TriggerRegistration data,
    std::vector<network::TriggerVerification> verifications) {
  // This is validated by the Mojo typemapping.
  DCHECK(reporting_origin.IsValid());

  const RegistrationContext* context =
      GetReceiverRegistrationContextForTrigger();
  if (!context) {
    return;
  }

  attribution_manager_->HandleTrigger(
      AttributionTrigger(std::move(reporting_origin), std::move(data),
                         /*destination_origin=*/context->context_origin(),
                         std::move(verifications),
                         context->is_within_fenced_frame()),
      context->render_frame_id());
}

void AttributionDataHostManagerImpl::OsSourceDataAvailable(
    std::vector<attribution_reporting::OsRegistrationItem> registration_items) {
  const RegistrationContext* context =
      GetReceiverRegistrationContextForSource();
  if (!context) {
    return;
  }

  AttributionInputEvent input_event;
  if (context->navigation().has_value()) {
    input_event = context->navigation()->input_event();
  }
  for (auto& item : registration_items) {
    attribution_manager_->HandleOsRegistration(
        OsRegistration(std::move(item.url), item.debug_reporting,
                       context->context_origin(), input_event,
                       context->is_within_fenced_frame()),
        context->render_frame_id());
  }
}

void AttributionDataHostManagerImpl::OsTriggerDataAvailable(
    std::vector<attribution_reporting::OsRegistrationItem> registration_items) {
  const RegistrationContext* context =
      GetReceiverRegistrationContextForTrigger();
  if (!context) {
    return;
  }

  for (auto& item : registration_items) {
    attribution_manager_->HandleOsRegistration(
        OsRegistration(std::move(item.url), item.debug_reporting,
                       context->context_origin(),
                       /*input_event=*/absl::nullopt,
                       context->is_within_fenced_frame()),
        context->render_frame_id());
  }
}

void AttributionDataHostManagerImpl::OnReceiverDisconnected() {
  const RegistrationContext& context = receivers_.current_context();

  if (context.navigation().has_value()) {
    if (auto it = ongoing_background_registrations_.find(
            context.navigation()->navigation_id());
        it != ongoing_background_registrations_.end()) {
      ongoing_background_registrations_.erase(it);
      MaybeBindDeferredReceivers(context.navigation()->navigation_id(),
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
    GlobalRenderFrameHostId render_frame_id,
    std::string devtools_request_id) {
  absl::optional<RegistrationNavigationContext> navigation;
  if (navigation_id.has_value()) {
    MaybeSetupDeferredReceivers(navigation_id.value());
    navigation = RegistrationNavigationContext(navigation_id.value(),
                                               std::move(input_event));
  }

  auto [it, inserted] = registrations_.emplace(
      SourceRegistrationsId(beacon_id),
      RegistrationContext(/*context_origin=*/std::move(source_origin),
                          RegistrationEligibility::kSource,
                          is_within_fenced_frame, render_frame_id,
                          std::move(devtools_request_id),
                          std::move(navigation)));
  CHECK(inserted);
}

void AttributionDataHostManagerImpl::NotifyFencedFrameReportingBeaconData(
    BeaconId beacon_id,
    network::AttributionReportingRuntimeFeatures runtime_features,
    GURL reporting_url,
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
      SuitableOrigin::Create(reporting_url);
  if (!suitable_reporting_origin) {
    MaybeOnRegistrationsFinished(it);
    return;
  }

  auto attribution_header = RegistrarAndHeader::Get(
      headers, runtime_features.Has(
                   network::AttributionReportingRuntimeFeature::kCrossAppWeb));
  if (!attribution_header.has_value()) {
    MaybeOnRegistrationsFinished(it);
    return;
  }

  if (!attribution_header->has_value()) {
    CHECK(it->devtools_request_id());
    LogAuditIssue(it->render_frame_id(),
                  /*violation_type=*/attribution_header->error(),
                  /*request_url=*/reporting_url, *it->devtools_request_id(),
                  /*invalid_parameter=*/absl::nullopt);
    return;
  }

  if (auto* rfh = RenderFrameHostImpl::FromID(it->render_frame_id())) {
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        rfh, blink::mojom::WebFeature::kAttributionFencedFrameReportingBeacon);
  }

  ParseSource(it,
              HeaderPendingDecode(std::move(attribution_header.value()->header),
                                  std::move(suitable_reporting_origin.value()),
                                  std::move(reporting_url)),
              attribution_header.value()->registrar);
}

void AttributionDataHostManagerImpl::OnWebSourceParsed(
    SourceRegistrationsId id,
    data_decoder::DataDecoder::ValueOrError result) {
  auto registrations = registrations_.find(id);
  DCHECK(registrations != registrations_.end());

  DCHECK(!registrations->pending_web_decodes().empty());
  {
    const auto& pending_decode = registrations->pending_web_decodes().front();

    auto source_type = registrations->navigation_id().has_value()
                           ? SourceType::kNavigation
                           : SourceType::kEvent;

    auto source =
        [&]() -> base::expected<StorableSource, SourceRegistrationError> {
      if (!result.has_value()) {
        return base::unexpected(SourceRegistrationError::kInvalidJson);
      }
      if (!result->is_dict()) {
        return base::unexpected(SourceRegistrationError::kRootWrongType);
      }
      ASSIGN_OR_RETURN(auto registration,
                       attribution_reporting::SourceRegistration::Parse(
                           std::move(*result).TakeDict()));
      return StorableSource(pending_decode.reporting_origin,
                            std::move(registration),
                            registrations->source_origin(), source_type,
                            registrations->is_within_fenced_frame());
    }();

    if (source.has_value()) {
      attribution_manager_->HandleSource(std::move(*source),
                                         registrations->render_frame_id());
    } else {
      CHECK(registrations->devtools_request_id());
      LogAuditIssue(registrations->render_frame_id(),
                    /*violation_type=*/
                    blink::mojom::AttributionReportingIssueType::
                        kInvalidRegisterSourceHeader,
                    /*request_url=*/pending_decode.reporting_url,
                    *registrations->devtools_request_id(),
                    /*invalid_parameter=*/pending_decode.header);
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
    if (result.has_value()) {
      std::vector<attribution_reporting::OsRegistrationItem>
          registration_items =
              attribution_reporting::ParseOsSourceOrTriggerHeader(*result);

      AttributionInputEvent input_event = registrations->input_event()
                                              ? *registrations->input_event()
                                              : AttributionInputEvent();
      for (auto& item : registration_items) {
        attribution_manager_->HandleOsRegistration(
            OsRegistration(std::move(item.url), item.debug_reporting,
                           registrations->source_origin(), input_event,
                           registrations->is_within_fenced_frame()),
            registrations->render_frame_id());
      }
    } else {
      const auto& pending_decode = registrations->pending_os_decodes().front();
      LogAuditIssue(registrations->render_frame_id(),
                    /*violation_type=*/
                    blink::mojom::AttributionReportingIssueType::
                        kInvalidRegisterOsSourceHeader,
                    /*request_url=*/pending_decode.reporting_url,
                    *registrations->devtools_request_id(),
                    /*invalid_parameter=*/pending_decode.header);
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
