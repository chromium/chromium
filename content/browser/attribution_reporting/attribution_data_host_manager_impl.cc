// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_data_host_manager_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/circular_deque.h"
#include "base/containers/contains.h"
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
#include "components/attribution_reporting/trigger_registration_error.mojom-shared.h"
#include "content/browser/attribution_reporting/attribution_background_registrations_id.h"
#include "content/browser/attribution_reporting/attribution_beacon_id.h"
#include "content/browser/attribution_reporting/attribution_constants.h"
#include "content/browser/attribution_reporting/attribution_input_event.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/attribution_os_level_manager.h"
#include "content/browser/attribution_reporting/attribution_reporting.mojom-shared.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/os_registration.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
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
#include "third_party/blink/public/common/features.h"
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
using ::attribution_reporting::mojom::RegistrationType;
using ::attribution_reporting::mojom::SourceRegistrationError;
using ::attribution_reporting::mojom::SourceType;
using ::attribution_reporting::mojom::TriggerRegistrationError;
using ::blink::mojom::AttributionReportingIssueType;

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

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class NavigationUnexpectedRegistration {
  kRegistrationAlreadyExists = 0,
  kRegistrationMissingUponReceivingData = 1,
  kMaxValue = kRegistrationMissingUponReceivingData,
};

// See https://crbug.com/1500667 for details. There are assumptions that a
// navigation registration can only be registered once and that it must be
// registered and will be available when receiving data. Crashes challenges
// these assumptions.
void RecordNavigationUnexpectedRegistration(
    NavigationUnexpectedRegistration status) {
  base::UmaHistogramEnumeration("Conversions.NavigationUnexpectedRegistration",
                                status);
}

enum class BackgroundNavigationOutcome {
  kTiedImmediately = 0,
  kTiedWithDelay = 1,
  kNeverTiedTimeout = 2,
  kNeverTiedIneligle = 3,
  kMaxValue = kNeverTiedIneligle,
};

void RecordBackgroundNavigationOutcome(BackgroundNavigationOutcome outcome) {
  base::UmaHistogramEnumeration("Conversions.BackgroundNavigation.Outcome",
                                outcome);
}

bool BackgroundRegistrationsEnabled() {
  return base::FeatureList::IsEnabled(
             blink::features::kKeepAliveInBrowserMigration) &&
         base::FeatureList::IsEnabled(
             blink::features::kAttributionReportingInBrowserMigration);
}

constexpr size_t kMaxDeferredReceiversPerNavigation = 30;

class RegistrationNavigationContext {
 public:
  RegistrationNavigationContext(int64_t navigation_id,
                                AttributionInputEvent input_event)
      : navigation_id_(navigation_id), input_event_(std::move(input_event)) {}

  ~RegistrationNavigationContext() = default;

  RegistrationNavigationContext(const RegistrationNavigationContext&) = default;
  RegistrationNavigationContext& operator=(
      const RegistrationNavigationContext&) = default;

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
                   const GURL& request_url,
                   const std::string& request_devtools_id,
                   absl::optional<std::string> invalid_parameter,
                   AttributionReportingIssueType violation_type) {
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

AttributionDataHostManagerImpl::SequentialTimeoutsTimer::
    SequentialTimeoutsTimer(base::TimeDelta delay)
    : delay_(delay) {}

AttributionDataHostManagerImpl::SequentialTimeoutsTimer::
    ~SequentialTimeoutsTimer() = default;

AttributionDataHostManagerImpl::SequentialTimeoutsTimer::Timeout::Timeout(
    base::TimeTicks time,
    base::OnceClosure callback)
    : time(time), callback(std::move(callback)) {}

AttributionDataHostManagerImpl::SequentialTimeoutsTimer::Timeout::~Timeout() =
    default;

AttributionDataHostManagerImpl::SequentialTimeoutsTimer::Timeout::Timeout(
    Timeout&&) = default;

AttributionDataHostManagerImpl::SequentialTimeoutsTimer::Timeout&
AttributionDataHostManagerImpl::SequentialTimeoutsTimer::Timeout::Timeout::
operator=(Timeout&&) = default;

void AttributionDataHostManagerImpl::SequentialTimeoutsTimer::Start(
    base::OnceClosure callback) {
  timeouts_.emplace_back(base::TimeTicks::Now() + delay_, std::move(callback));

  MaybeStartTimer();
}

void AttributionDataHostManagerImpl::SequentialTimeoutsTimer::
    MaybeStartTimer() {
  if (timeouts_.empty() || timer_.IsRunning()) {
    return;
  }

  timer_.Start(FROM_HERE, timeouts_.front().time - base::TimeTicks::Now(),
               base::BindOnce(&SequentialTimeoutsTimer::ProcessTimeout,
                              // Timer is owned by `this`.
                              base::Unretained(this)));
}

void AttributionDataHostManagerImpl::SequentialTimeoutsTimer::ProcessTimeout() {
  CHECK(!timeouts_.empty());
  {
    Timeout& timeout = timeouts_.front();
    CHECK_LE(timeout.time, base::TimeTicks::Now());

    std::move(timeout.callback).Run();
  }
  timeouts_.pop_front();

  MaybeStartTimer();
}

class AttributionDataHostManagerImpl::NavigationContextForPendingRegistration {
 public:
  explicit NavigationContextForPendingRegistration(
      size_t pending_registrations_count)
      : pending_registrations_count_(pending_registrations_count) {}

  // Instances of the class are eagerly initialized in
  // `RegisterNavigationDataHost` as this is when we learn of the expected
  // `pending_registrations_count_`. However, until the context is set
  // (`SetContext`) or we learn that the navigation isn't eligible
  // (`DeclareUneligible`), a background registration cannot obtain any
  // information.
  bool CanBeUsed() const { return eligible_.has_value(); }

  // Returns true if the navigation for which the context is cached is eligible,
  // returns false otherwise. Should only be called if `CanBeUsed` returns true.
  bool IsEligible() const {
    CHECK(CanBeUsed());

    return eligible_.value();
  }

  // Returns a copy of the navigation context. Should only be called if
  // `CanBeUsed` and `IsEligible` return true.
  RegistrationNavigationContext GetContext() const {
    CHECK(context_.has_value());

    return context_.value();
  }

  void DeclareUneligible() {
    CHECK(!eligible_.has_value());

    eligible_ = false;
  }

  void SetContext(RegistrationNavigationContext context) {
    CHECK(!eligible_.has_value());

    context_ = std::move(context);
    eligible_ = true;
  }

  void DecreasePendingRegistrationsCount(size_t by) {
    // `by` can be greater than `pending_registrations_count_` only with a
    // misbehaving renderer that called `RegisterNavigationDataHost` with a
    // value for `expected_registrations` lower than the real number of expected
    // registrations.
    pending_registrations_count_ -= std::min(by, pending_registrations_count_);
  }

  bool HasPendingRegistrations() const {
    return pending_registrations_count_ > 0;
  }

  absl::optional<int64_t> navigation_id() const {
    if (!context_.has_value()) {
      return absl::nullopt;
    };

    return context_->navigation_id();
  }

 private:
  size_t pending_registrations_count_;
  absl::optional<bool> eligible_;
  absl::optional<RegistrationNavigationContext> context_;
};

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

  void SetNavigation(RegistrationNavigationContext navigation) {
    CHECK(!navigation_.has_value());
    navigation_ = std::move(navigation);
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

struct AttributionDataHostManagerImpl::DeferredReceiver {
  mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host;
  RegistrationContext context;
  base::TimeTicks initial_registration_time = base::TimeTicks::Now();
};

struct AttributionDataHostManagerImpl::HeaderPendingDecode {
  std::string header;
  SuitableOrigin reporting_origin;
  GURL reporting_url;
  absl::optional<std::vector<network::TriggerVerification>> verifications;

  HeaderPendingDecode(
      std::string header,
      SuitableOrigin reporting_origin,
      GURL reporting_url,
      absl::optional<std::vector<network::TriggerVerification>> verifications)
      : header(std::move(header)),
        reporting_origin(std::move(reporting_origin)),
        reporting_url(std::move(reporting_url)),
        verifications(std::move(verifications)) {
    DCHECK_EQ(url::Origin::Create(this->reporting_origin->GetURL()),
              url::Origin::Create(this->reporting_url));
  }

  RegistrationType GetType() const {
    return verifications.has_value() ? RegistrationType::kTrigger
                                     : RegistrationType::kSource;
  }
};

class AttributionDataHostManagerImpl::Registrations {
 public:
  Registrations(RegistrationsId id,
                RegistrationContext context,
                bool waiting_on_navigation,
                absl::optional<int64_t> defer_until_navigation)
      : waiting_on_navigation_(waiting_on_navigation),
        defer_until_navigation_(defer_until_navigation),
        id_(id),
        context_(std::move(context)) {}

  Registrations(const Registrations&) = delete;
  Registrations& operator=(const Registrations&) = delete;

  Registrations(Registrations&&) = default;
  Registrations& operator=(Registrations&&) = default;

  const SuitableOrigin& context_origin() const {
    return context_.context_origin();
  }

  RegistrationEligibility eligibility() const {
    return context_.registration_eligibility();
  }

  bool has_pending_decodes() const {
    return !pending_os_decodes_.empty() || !pending_web_decodes_.empty();
  }

  bool registrations_complete() const { return registrations_complete_; }

  bool is_within_fenced_frame() const {
    return context_.is_within_fenced_frame();
  }

  bool IsReadyToProcess() const {
    if (waiting_on_navigation_) {
      return false;
    }
    if (defer_until_navigation_.has_value()) {
      return false;
    }
    return true;
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

  absl::optional<int64_t> defer_until_navigation() const {
    return defer_until_navigation_;
  }

  bool operator<(const Registrations& other) const { return Id() < other.Id(); }

  void CompleteRegistrations() {
    DCHECK(!registrations_complete_);
    registrations_complete_ = true;
  }

  void SetNavigation(RegistrationNavigationContext navigation) {
    CHECK(waiting_on_navigation_);
    context_.SetNavigation(std::move(navigation));
    waiting_on_navigation_ = false;
  }

  void ClearDeferUntilNavigation() { defer_until_navigation_.reset(); }

  friend bool operator<(const Registrations& a, const RegistrationsId& b) {
    return a.Id() < b;
  }

  friend bool operator<(const RegistrationsId& a, const Registrations& b) {
    return a < b.Id();
  }

  RegistrationsId Id() const { return id_; }

 private:
  // True if navigation or beacon has completed.
  bool registrations_complete_ = false;

  // True for background registrations tied to a navigation for which we haven't
  // received the navigation context yet. The background registration is created
  // in `NotifyNavigationRegistrationStarted`. The navigation context is
  // received in `NotifyNavigationRegistrationStarted`. The order of these calls
  // is not guaranteed, as such, if `NotifyNavigationRegistrationStarted` is
  // received first, we indicate using this property that we are still waiting
  // on the navigation context.
  bool waiting_on_navigation_ = false;

  // Indicates that the registration should not be processed until all source
  // registrations linked to the navigation complete. When they complete,
  // `ClearDeferUntilNavigation` should be called.
  absl::optional<int64_t> defer_until_navigation_;

  RegistrationsId id_;

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
  RegistrationType type;
  std::string header;

  [[nodiscard]] static absl::optional<RegistrarAndHeader> Get(
      const net::HttpResponseHeaders* headers,
      bool cross_app_web_runtime_enabled,
      RegistrationEligibility eligibility,
      base::RepeatingCallback<void(AttributionReportingIssueType)>
          log_audit_issue) {
    if (!headers) {
      return absl::nullopt;
    }

    std::string web_source;
    const bool has_web_source = headers->GetNormalizedHeader(
        kAttributionReportingRegisterSourceHeader, &web_source);

    std::string web_trigger;
    const bool has_web_trigger = headers->GetNormalizedHeader(
        kAttributionReportingRegisterTriggerHeader, &web_trigger);

    // Note that it's important that the browser process check both the
    // base::Feature (which is set from the browser, so trustworthy) and the
    // runtime feature (which can be spoofed in a compromised renderer, so is
    // best-effort).
    const bool cross_app_web_enabled =
        cross_app_web_runtime_enabled &&
        base::FeatureList::IsEnabled(
            network::features::kAttributionReportingCrossAppWeb);

    std::string os_source;
    const bool has_os_source =
        cross_app_web_enabled &&
        headers->GetNormalizedHeader(
            kAttributionReportingRegisterOsSourceHeader, &os_source);

    std::string os_trigger;
    const bool has_os_trigger =
        cross_app_web_enabled &&
        headers->GetNormalizedHeader(
            kAttributionReportingRegisterOsTriggerHeader, &os_trigger);

    const bool has_source = has_web_source || has_os_source;
    const bool has_trigger = has_web_trigger || has_os_trigger;

    absl::optional<RegistrationType> registration_type;
    switch (eligibility) {
      case RegistrationEligibility::kSourceOrTrigger:
        if (has_source && has_trigger) {
          log_audit_issue.Run(
              AttributionReportingIssueType::kSourceAndTriggerHeaders);
          return absl::nullopt;
        }

        if (has_source) {
          registration_type = RegistrationType::kSource;
        } else if (has_trigger) {
          registration_type = RegistrationType::kTrigger;
        }
        break;
      case RegistrationEligibility::kSource:
        if (has_os_trigger) {
          log_audit_issue.Run(AttributionReportingIssueType::kOsTriggerIgnored);
        }
        if (has_web_trigger) {
          log_audit_issue.Run(AttributionReportingIssueType::kTriggerIgnored);
        }
        if (has_source) {
          registration_type = RegistrationType::kSource;
        }
        break;
      case RegistrationEligibility::kTrigger:
        if (has_os_source) {
          log_audit_issue.Run(AttributionReportingIssueType::kOsSourceIgnored);
        }
        if (has_web_source) {
          log_audit_issue.Run(AttributionReportingIssueType::kSourceIgnored);
        }
        if (has_trigger) {
          registration_type = RegistrationType::kTrigger;
        }
        break;
    }
    // No eligibile header available.
    if (!registration_type.has_value()) {
      return absl::nullopt;
    }

    switch (registration_type.value()) {
      case RegistrationType::kSource:
        if (has_os_source && has_web_source) {
          log_audit_issue.Run(AttributionReportingIssueType::kWebAndOsHeaders);
          return absl::nullopt;
        }
        if (has_os_source) {
          // Max header size is 256 KB, use 1M count to encapsulate.
          base::UmaHistogramCounts1M("Conversions.HeadersSize.RegisterOsSource",
                                     os_source.size());
          return RegistrarAndHeader{.registrar = Registrar::kOs,
                                    .type = RegistrationType::kSource,
                                    .header = std::move(os_source)};
        }
        if (has_web_source) {
          // Max header size is 256 KB, use 1M count to encapsulate.
          base::UmaHistogramCounts1M("Conversions.HeadersSize.RegisterSource",
                                     web_source.size());
          return RegistrarAndHeader{.registrar = Registrar::kWeb,
                                    .type = RegistrationType::kSource,
                                    .header = std::move(web_source)};
        }
        break;
      case RegistrationType::kTrigger:
        if (has_os_trigger && has_web_trigger) {
          log_audit_issue.Run(AttributionReportingIssueType::kWebAndOsHeaders);
          return absl::nullopt;
        }
        if (has_os_trigger) {
          return RegistrarAndHeader{.registrar = Registrar::kOs,
                                    .type = RegistrationType::kTrigger,
                                    .header = std::move(os_trigger)};
        }
        if (has_web_trigger) {
          return RegistrarAndHeader{.registrar = Registrar::kWeb,
                                    .type = RegistrationType::kTrigger,
                                    .header = std::move(web_trigger)};
        }
        break;
    }

    // It should always hit an early return.
    NOTREACHED_NORETURN();
  }
};

AttributionDataHostManagerImpl::AttributionDataHostManagerImpl(
    AttributionManager* attribution_manager)
    : attribution_manager_(
          raw_ref<AttributionManager>::from_ptr(attribution_manager)),
      background_registrations_waiting_on_navigation_timer_(
          /*delay=*/base::Seconds(3)),
      navigations_waiting_on_background_registrations_timer_(
          /*delay=*/base::Seconds(3)),
      deferred_receivers_timer_(/*delay=*/base::Seconds(10)) {
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
    const blink::AttributionSrcToken& attribution_src_token,
    size_t expected_registrations) {
  auto [it, inserted] = navigation_data_host_map_.try_emplace(
      attribution_src_token, std::move(data_host));
  // Should only be possible with a misbehaving renderer.
  if (!inserted) {
    return false;
  }

  if (BackgroundRegistrationsEnabled()) {
    // Should only be possible with a misbehaving renderer.
    if (expected_registrations == 0) {
      return false;
    }
    // When the `kKeepAliveInBrowserMigration` and
    // `kAttributionReportingInBrowserMigration` features are enabled, we expect
    // to receive background registrations through
    // `NotifyBackgroundRegistrations...` calls. We use the field below to keep
    // the navigation context available until we've received all expected
    // background registrations. When using the datahost, it is not necessary,
    // as the navigation context is kept on the datahost context.
    auto [it_unused, waiting_inserted] =
        navigations_waiting_on_background_registrations_.try_emplace(
            attribution_src_token,
            NavigationContextForPendingRegistration(expected_registrations));
    if (!waiting_inserted) {
      // Should only be possible with a misbehaving renderer.
      return false;
    }
  }

  RecordNavigationDataHostStatus(NavigationDataHostStatus::kRegistered);
  return true;
}

void AttributionDataHostManagerImpl::ParseHeader(
    base::flat_set<Registrations>::iterator it,
    HeaderPendingDecode pending_decode,
    Registrar registrar) {
  DCHECK(it != registrations_.end());

  switch (it->eligibility()) {
    case RegistrationEligibility::kSourceOrTrigger:
      break;
    case RegistrationEligibility::kSource:
      CHECK_EQ(pending_decode.GetType(), RegistrationType::kSource);
      break;
    case RegistrationEligibility::kTrigger:
      CHECK_EQ(pending_decode.GetType(), RegistrationType::kTrigger);
      break;
  }

  network::mojom::AttributionSupport attribution_support =
      AttributionManager::GetAttributionSupport(
          content::WebContents::FromRenderFrameHost(
              RenderFrameHost::FromID(it->render_frame_id())));
  switch (registrar) {
    case Registrar::kWeb:
      if (!network::HasAttributionWebSupport(attribution_support)) {
        CHECK(it->devtools_request_id());
        LogAuditIssue(
            it->render_frame_id(),
            /*request_url=*/pending_decode.reporting_url,
            *it->devtools_request_id(),
            /*invalid_parameter=*/absl::nullopt,
            /*violation_type=*/
            blink::mojom::AttributionReportingIssueType::kNoWebOrOsSupport);
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
      if (!network::HasAttributionOsSupport(attribution_support)) {
        CHECK(it->devtools_request_id());
        LogAuditIssue(
            it->render_frame_id(),
            /*request_url=*/pending_decode.reporting_url,
            *it->devtools_request_id(),
            /*invalid_parameter=*/absl::nullopt,
            /*violation_type=*/
            blink::mojom::AttributionReportingIssueType::kNoWebOrOsSupport);
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
    const Registrations& registrations) {
  if (!registrations.IsReadyToProcess()) {
    return;
  }

  DCHECK(!registrations.pending_web_decodes().empty());

  const auto& pending_decode = registrations.pending_web_decodes().front();

  data_decoder_.ParseJson(
      pending_decode.header,
      base::BindOnce(&AttributionDataHostManagerImpl::OnWebHeaderParsed,
                     weak_factory_.GetWeakPtr(), registrations.Id(),
                     pending_decode.GetType(),
                     std::move(pending_decode.verifications)));
}

void AttributionDataHostManagerImpl::HandleNextOsDecode(
    const Registrations& registrations) {
  if (!registrations.IsReadyToProcess()) {
    return;
  }

  DCHECK(!registrations.pending_os_decodes().empty());

  const auto& pending_decode = registrations.pending_os_decodes().front();

  data_decoder_.ParseStructuredHeaderList(
      pending_decode.header,
      base::BindOnce(&AttributionDataHostManagerImpl::OnOsHeaderParsed,
                     weak_factory_.GetWeakPtr(), registrations.Id(),
                     pending_decode.GetType()));
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
      RegistrationsId(attribution_src_token),
      RegistrationContext(
          /*context_origin=*/source_origin, RegistrationEligibility::kSource,
          is_within_fenced_frame, render_frame_id,
          std::move(devtools_request_id),
          RegistrationNavigationContext(navigation_id, input_event)),
      /*waiting_on_navigation=*/false,
      /*defer_until_navigation=*/absl::nullopt);
  if (!registration_inserted) {
    RecordNavigationUnexpectedRegistration(
        NavigationUnexpectedRegistration::kRegistrationAlreadyExists);
    return;
  }

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
        ongoing_background_datahost_registrations_.emplace(navigation_id);
    DCHECK(inserted);

    receivers_.Add(
        this, std::move(it->second),
        RegistrationContext(
            /*context_origin=*/source_origin, RegistrationEligibility::kSource,
            is_within_fenced_frame, render_frame_id,
            /*devtools_request_id=*/absl::nullopt,
            RegistrationNavigationContext(navigation_id, input_event)));

    navigation_data_host_map_.erase(it);
    RecordNavigationDataHostStatus(NavigationDataHostStatus::kProcessed);
  } else {
    RecordNavigationDataHostStatus(NavigationDataHostStatus::kNotFound);
  }

  if (auto waiting_ids_it =
          background_registrations_waiting_on_navigation_.find(
              attribution_src_token);
      waiting_ids_it != background_registrations_waiting_on_navigation_.end()) {
    for (BackgroundRegistrationsId id : waiting_ids_it->second) {
      // The background registration will no longer be present if it completed
      // without attempting to register any data.
      if (auto it = registrations_.find(id); it != registrations_.end()) {
        it->SetNavigation(
            RegistrationNavigationContext(navigation_id, input_event));
        RecordBackgroundNavigationOutcome(
            BackgroundNavigationOutcome::kTiedWithDelay);

        if (!it->pending_web_decodes().empty()) {
          HandleNextWebDecode(*it);
        }
        if (!it->pending_os_decodes().empty()) {
          HandleNextOsDecode(*it);
        }
      }
    }
    BackgroundRegistrationsTied(attribution_src_token,
                                waiting_ids_it->second.size(),
                                /*due_to_timeout=*/false);
    background_registrations_waiting_on_navigation_.erase(waiting_ids_it);
  }

  if (auto it = navigations_waiting_on_background_registrations_.find(
          attribution_src_token);
      it != navigations_waiting_on_background_registrations_.end()) {
    it->second.SetContext(
        RegistrationNavigationContext(navigation_id, input_event));
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

  auto it = registrations_.find(attribution_src_token);
  if (it == registrations_.end()) {
    RecordNavigationUnexpectedRegistration(
        NavigationUnexpectedRegistration::
            kRegistrationMissingUponReceivingData);
    return false;
  }
  CHECK(!it->registrations_complete());

  auto header = RegistrarAndHeader::Get(
      headers,
      runtime_features.Has(
          network::AttributionReportingRuntimeFeature::kCrossAppWeb),
      it->eligibility(),
      base::BindRepeating(&LogAuditIssue, it->render_frame_id(),
                          /*request_url=*/reporting_url,
                          *it->devtools_request_id(),
                          /*invalid_parameter=*/absl::nullopt));
  if (!header.has_value()) {
    return false;
  }

  ParseHeader(it,
              HeaderPendingDecode(std::move(header->header),
                                  std::move(reporting_origin.value()),
                                  std::move(reporting_url),
                                  /*verifications=*/absl::nullopt),
              header->registrar);
  return true;
}

void AttributionDataHostManagerImpl::
    MaybeClearBackgroundRegistrationsWaitingOnNavigation(
        const blink::AttributionSrcToken& attribution_src_token,
        bool due_to_timeout) {
  auto it = background_registrations_waiting_on_navigation_.find(
      attribution_src_token);
  if (it == background_registrations_waiting_on_navigation_.end()) {
    return;
  }

  for (BackgroundRegistrationsId id : it->second) {
    // The background registration will not longer be present if it completed
    // without attempting to register any data.
    if (auto background_it = registrations_.find(id);
        background_it != registrations_.end()) {
      CHECK(!background_it->IsReadyToProcess());
      registrations_.erase(background_it);
    }
    RecordBackgroundNavigationOutcome(
        due_to_timeout ? BackgroundNavigationOutcome::kNeverTiedTimeout
                       : BackgroundNavigationOutcome::kNeverTiedIneligle);
  }
  BackgroundRegistrationsTied(attribution_src_token,
                              /*count*/ it->second.size(),
                              // We set `due_to_timeout` false here even when
                              // the call to this method is due to a timeout as
                              // this value refers to the navigation context
                              // timeout as opposed to the background
                              // registrations timeout.
                              /*due_to_timeout=*/false);
  background_registrations_waiting_on_navigation_.erase(it);
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

  // Background registrations are expected to be tied at navigation start
  // time. If the navigation has completed and some registrations are still
  // waiting, they are considered ineligible and can be cleared.
  MaybeClearBackgroundRegistrationsWaitingOnNavigation(
      attribution_src_token,
      /*due_to_timeout=*/false);

  // It is possible to have no registration stored if
  // `NotifyNavigationRegistrationStarted` wasn't previously called for this
  // token. This indicates that the navigation was ineligble for registrations.
  auto registrations_it = registrations_.find(attribution_src_token);
  auto waiting_it = navigations_waiting_on_background_registrations_.find(
      attribution_src_token);
  if (registrations_it != registrations_.end()) {
    registrations_it->CompleteRegistrations();
    MaybeOnRegistrationsFinished(registrations_it);
  } else if (waiting_it !=
             navigations_waiting_on_background_registrations_.end()) {
    waiting_it->second.DeclareUneligible();
  }

  if (waiting_it != navigations_waiting_on_background_registrations_.end()) {
    // If we are still waiting on background registrations to start after the
    // navigation ends, we start a timeout to ensures that we don't wait
    // indefinitely. This could happen if:
    // - `RegisterNavigationDataHost` was called with an erroneous (too high)
    //   number of `expected_registrations`, so we never receive enough
    //   background registrations to clear the context.
    // - For unknown reasons, we don't receive all `expected_registrations`.
    CHECK(waiting_it->second.CanBeUsed());
    navigations_waiting_on_background_registrations_timer_.Start(base::BindOnce(
        &AttributionDataHostManagerImpl::BackgroundRegistrationsTied,
        weak_factory_.GetWeakPtr(), attribution_src_token, /*count=*/0,
        /*due_to_timeout=*/true));
  }
}

void AttributionDataHostManagerImpl::NotifyBackgroundRegistrationStarted(
    BackgroundRegistrationsId id,
    const attribution_reporting::SuitableOrigin& context_origin,
    bool is_within_fenced_frame,
    attribution_reporting::mojom::RegistrationEligibility
        registration_eligibility,
    GlobalRenderFrameHostId render_frame_id,
    int64_t last_navigation_id,
    absl::optional<blink::AttributionSrcToken> attribution_src_token,
    std::string devtools_request_id) {
  CHECK(BackgroundRegistrationsEnabled());

  absl::optional<RegistrationNavigationContext> navigation_context;

  if (attribution_src_token.has_value()) {
    const blink::AttributionSrcToken& token = attribution_src_token.value();

    auto nav_waiting_it =
        navigations_waiting_on_background_registrations_.find(token);
    if (nav_waiting_it !=
            navigations_waiting_on_background_registrations_.end() &&
        nav_waiting_it->second.CanBeUsed()) {
      if (!nav_waiting_it->second.IsEligible()) {
        RecordBackgroundNavigationOutcome(
            BackgroundNavigationOutcome::kNeverTiedIneligle);
        // Since the navigation is ineligible, we return early and avoid
        // creating a registration, all further requests related to this
        // background id will simply be dropped.
        BackgroundRegistrationsTied(token, 1, /*due_to_timeout=*/false);
        return;
      }
      RecordBackgroundNavigationOutcome(
          BackgroundNavigationOutcome::kTiedImmediately);
      navigation_context = nav_waiting_it->second.GetContext();
      BackgroundRegistrationsTied(token, 1, /*due_to_timeout=*/false);
    } else {
      // Navigation has not started yet
      //
      // We start waiting on the navigation even it hasn't been "announced" via
      // `RegisterNavigationDataHost` yet because for a given navigation, we
      // have have no guarantee that `RegisterNavigationDataHost` gets be called
      // before `NotifyBackgroundRegistrationStarted`.
      auto waiting_it =
          background_registrations_waiting_on_navigation_.find(token);
      if (waiting_it != background_registrations_waiting_on_navigation_.end()) {
        waiting_it->second.emplace(id);
      } else {
        background_registrations_waiting_on_navigation_.emplace(
            token, base::flat_set<BackgroundRegistrationsId>{id});

        // Ensures that we don't wait indefinitely. This could happen if:
        // - The tied navigation never starts nor ends.
        // - The navigation starts, ends, its cached context timeout and we then
        //    receive a background registration tied to it.
        background_registrations_waiting_on_navigation_timer_.Start(
            base::BindOnce(
                &AttributionDataHostManagerImpl::
                    MaybeClearBackgroundRegistrationsWaitingOnNavigation,
                weak_factory_.GetWeakPtr(), token, /*due_to_timeout=*/true));
      }
    }
  }

  bool waiting_on_navigation =
      attribution_src_token.has_value() && !navigation_context.has_value();
  absl::optional<int64_t> deferred_until;
  if (deferred_receivers_.contains(last_navigation_id) &&
      registration_eligibility != RegistrationEligibility::kSource) {
    deferred_until = last_navigation_id;
  }

  auto [it_unused, inserted] = registrations_.emplace(
      RegistrationsId(id),
      RegistrationContext(std::move(context_origin), registration_eligibility,
                          is_within_fenced_frame, render_frame_id,
                          std::move(devtools_request_id),
                          std::move(navigation_context)),
      waiting_on_navigation, deferred_until);
  CHECK(inserted);
}

bool AttributionDataHostManagerImpl::NotifyBackgroundRegistrationData(
    BackgroundRegistrationsId id,
    const net::HttpResponseHeaders* headers,
    GURL reporting_url,
    network::AttributionReportingRuntimeFeatures runtime_features,
    std::vector<network::TriggerVerification> trigger_verifications) {
  CHECK(BackgroundRegistrationsEnabled());

  auto it = registrations_.find(id);
  // If the registrations cannot be found, it means that it was dropped early
  // due to being tied to an ineligle navigation.
  if (it == registrations_.end()) {
    return false;
  }
  CHECK(!it->registrations_complete());

  auto header = RegistrarAndHeader::Get(
      headers,
      runtime_features.Has(
          network::AttributionReportingRuntimeFeature::kCrossAppWeb),
      it->eligibility(),
      base::BindRepeating(&LogAuditIssue, it->render_frame_id(),
                          /*request_url=*/reporting_url,
                          *it->devtools_request_id(),
                          /*invalid_parameter=*/absl::nullopt));
  if (!header.has_value()) {
    return false;
  }

  auto reporting_origin =
      SuitableOrigin::Create(url::Origin::Create(reporting_url));
  CHECK(reporting_origin);

  absl::optional<std::vector<network::TriggerVerification>> verifications;
  if (header->type == RegistrationType::kTrigger) {
    verifications = std::move(trigger_verifications);
  }
  ParseHeader(
      it,
      HeaderPendingDecode(std::move(header->header),
                          std::move(reporting_origin.value()),
                          std::move(reporting_url), std::move(verifications)),
      header->registrar);
  return true;
}

void AttributionDataHostManagerImpl::NotifyBackgroundRegistrationCompleted(
    BackgroundRegistrationsId id) {
  CHECK(BackgroundRegistrationsEnabled());

  auto it = registrations_.find(id);
  // If the registrations cannot be found, it means that it was dropped early
  // due to being tied to an ineligle navigation.
  if (it == registrations_.end()) {
    return;
  }

  it->CompleteRegistrations();
  MaybeOnRegistrationsFinished(it);
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

  if (!data.IsValidForSourceType(source_type)) {
    mojo::ReportBadMessage(
        "AttributionDataHost: Source invalid for source type.");
    return;
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
    attribution_manager_->HandleOsRegistration(OsRegistration(
        std::move(item.url), item.debug_reporting, context->context_origin(),
        input_event, context->is_within_fenced_frame(),
        context->render_frame_id()));
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
    attribution_manager_->HandleOsRegistration(OsRegistration(
        std::move(item.url), item.debug_reporting, context->context_origin(),
        /*input_event=*/absl::nullopt, context->is_within_fenced_frame(),
        context->render_frame_id()));
  }
}

void AttributionDataHostManagerImpl::OnReceiverDisconnected() {
  const RegistrationContext& context = receivers_.current_context();

  if (context.navigation().has_value()) {
    if (auto it = ongoing_background_datahost_registrations_.find(
            context.navigation()->navigation_id());
        it != ongoing_background_datahost_registrations_.end()) {
      ongoing_background_datahost_registrations_.erase(it);
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
      RegistrationsId(beacon_id),
      RegistrationContext(/*context_origin=*/std::move(source_origin),
                          RegistrationEligibility::kSource,
                          is_within_fenced_frame, render_frame_id,
                          std::move(devtools_request_id),
                          std::move(navigation)),
      /*waiting_on_navigation=*/false,
      /*defer_until_navigation=*/absl::nullopt);
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
  // `AttributionHost::NotifyFencedFrameReportingBeaconStarted()` and
  // therefore not being tracked.
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

  auto header = RegistrarAndHeader::Get(
      headers,
      runtime_features.Has(
          network::AttributionReportingRuntimeFeature::kCrossAppWeb),
      it->eligibility(),
      base::BindRepeating(&LogAuditIssue, it->render_frame_id(),
                          /*request_url=*/reporting_url,
                          *it->devtools_request_id(),
                          /*invalid_parameter=*/absl::nullopt));
  if (!header.has_value()) {
    MaybeOnRegistrationsFinished(it);
    return;
  }

  if (auto* rfh = RenderFrameHostImpl::FromID(it->render_frame_id())) {
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        rfh, blink::mojom::WebFeature::kAttributionFencedFrameReportingBeacon);
  }

  ParseHeader(it,
              HeaderPendingDecode(std::move(header->header),
                                  std::move(suitable_reporting_origin.value()),
                                  std::move(reporting_url),
                                  /*verifications=*/absl::nullopt),
              header->registrar);
}

void AttributionDataHostManagerImpl::BackgroundRegistrationsTied(
    const blink::AttributionSrcToken& token,
    size_t count,
    bool due_to_timeout) {
  auto it = navigations_waiting_on_background_registrations_.find(token);
  if (it == navigations_waiting_on_background_registrations_.end()) {
    return;
  }

  if (!due_to_timeout) {
    it->second.DecreasePendingRegistrationsCount(/*by=*/count);
  }

  if (due_to_timeout || !it->second.HasPendingRegistrations()) {
    auto navigation_id = it->second.navigation_id();
    navigations_waiting_on_background_registrations_.erase(it);
    if (navigation_id.has_value()) {
      MaybeBindDeferredReceivers(navigation_id.value(),
                                 // We set `due_to_timeout` false here even when
                                 // the call to this method is due to a timeout
                                 // as this value refers to the deferred
                                 // receivers timeout.
                                 /*due_to_timeout=*/false);
    }
  }
}

void AttributionDataHostManagerImpl::HandleParsedWebSource(
    const Registrations& registrations,
    const HeaderPendingDecode& pending_decode,
    data_decoder::DataDecoder::ValueOrError result) {
  auto source_type = registrations.navigation_id().has_value()
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
                         std::move(*result).TakeDict(), source_type));

    return StorableSource(pending_decode.reporting_origin,
                          std::move(registration),
                          registrations.context_origin(), source_type,
                          registrations.is_within_fenced_frame());
  }();
  if (source.has_value()) {
    attribution_manager_->HandleSource(std::move(*source),
                                       registrations.render_frame_id());
  } else {
    CHECK(registrations.devtools_request_id());
    LogAuditIssue(registrations.render_frame_id(),
                  /*request_url=*/pending_decode.reporting_url,
                  *registrations.devtools_request_id(),
                  /*invalid_parameter=*/pending_decode.header,
                  /*violation_type=*/
                  AttributionReportingIssueType::kInvalidRegisterSourceHeader);
    attribution_reporting::RecordSourceRegistrationError(source.error());
  }
}

void AttributionDataHostManagerImpl::HandleParsedWebTrigger(
    const Registrations& registrations,
    const HeaderPendingDecode& pending_decode,
    std::vector<network::TriggerVerification> trigger_verifications,
    data_decoder::DataDecoder::ValueOrError result) {
  auto trigger =
      [&]() -> base::expected<AttributionTrigger, TriggerRegistrationError> {
    if (!result.has_value()) {
      return base::unexpected(TriggerRegistrationError::kInvalidJson);
    }
    if (!result->is_dict()) {
      return base::unexpected(TriggerRegistrationError::kRootWrongType);
    }

    ASSIGN_OR_RETURN(auto registration,
                     attribution_reporting::TriggerRegistration::Parse(
                         std::move(*result).TakeDict()));
    return AttributionTrigger(
        pending_decode.reporting_origin, std::move(registration),
        /*destination_origin=*/registrations.context_origin(),
        /*verifications=*/std::move(trigger_verifications),
        registrations.is_within_fenced_frame());
  }();
  if (trigger.has_value()) {
    attribution_manager_->HandleTrigger(std::move(*trigger),
                                        registrations.render_frame_id());
  } else {
    // TODO(https://crbug.com/1457238): Notify of failed trigger registration.
  }
}

void AttributionDataHostManagerImpl::OnWebHeaderParsed(
    RegistrationsId id,
    RegistrationType type,
    absl::optional<std::vector<network::TriggerVerification>>
        trigger_verifications,
    data_decoder::DataDecoder::ValueOrError result) {
  auto registrations = registrations_.find(id);
  DCHECK(registrations != registrations_.end());

  DCHECK(!registrations->pending_web_decodes().empty());
  {
    const auto& pending_decode = registrations->pending_web_decodes().front();
    switch (type) {
      case RegistrationType::kSource: {
        CHECK(!trigger_verifications.has_value());
        HandleParsedWebSource(*registrations, pending_decode,
                              std::move(result));
        break;
      }
      case RegistrationType::kTrigger: {
        CHECK(trigger_verifications.has_value());
        HandleParsedWebTrigger(*registrations, pending_decode,
                               std::move(trigger_verifications.value()),
                               std::move(result));
        break;
      }
    }
  }

  registrations->pending_web_decodes().pop_front();

  if (!registrations->pending_web_decodes().empty()) {
    HandleNextWebDecode(*registrations);
  } else {
    MaybeOnRegistrationsFinished(registrations);
  }
}

void AttributionDataHostManagerImpl::OnOsHeaderParsed(
    RegistrationsId id,
    RegistrationType registration_type,
    OsParseResult result) {
  auto registrations = registrations_.find(id);
  DCHECK(registrations != registrations_.end());

  DCHECK(!registrations->pending_os_decodes().empty());
  {
    if (result.has_value()) {
      std::vector<attribution_reporting::OsRegistrationItem>
          registration_items =
              attribution_reporting::ParseOsSourceOrTriggerHeader(*result);

      absl::optional<AttributionInputEvent> input_event;
      if (registration_type == RegistrationType::kSource) {
        input_event = registrations->input_event()
                          ? *registrations->input_event()
                          : AttributionInputEvent();
      }
      for (auto& item : registration_items) {
        attribution_manager_->HandleOsRegistration(
            OsRegistration(std::move(item.url), item.debug_reporting,
                           registrations->context_origin(), input_event,
                           registrations->is_within_fenced_frame(),
                           registrations->render_frame_id()));
      }
    } else {
      const auto& pending_decode = registrations->pending_os_decodes().front();
      LogAuditIssue(
          registrations->render_frame_id(),
          /*request_url=*/pending_decode.reporting_url,
          *registrations->devtools_request_id(),
          /*invalid_parameter=*/pending_decode.header,
          /*violation_type=*/
          AttributionReportingIssueType::kInvalidRegisterOsSourceHeader);
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
    base::flat_set<Registrations>::const_iterator it) {
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

void AttributionDataHostManagerImpl::MaybeSetupDeferredReceivers(
    int64_t navigation_id) {
  auto [it, inserted] = deferred_receivers_.try_emplace(
      navigation_id, std::vector<DeferredReceiver>());

  if (!inserted) {
    // We already have deferred receivers linked to the navigation.
    return;
  }

  deferred_receivers_timer_.Start(base::BindOnce(
      &AttributionDataHostManagerImpl::MaybeBindDeferredReceivers,
      weak_factory_.GetWeakPtr(), navigation_id,
      /*due_to_timeout=*/true));
}

void AttributionDataHostManagerImpl::MaybeBindDeferredReceivers(
    int64_t navigation_id,
    bool due_to_timeout) {
  if (due_to_timeout) {
    // We cleanup and bind the deferred receivers on timeout
    if (const auto& it =
            ongoing_background_datahost_registrations_.find(navigation_id);
        it != ongoing_background_datahost_registrations_.end()) {
      ongoing_background_datahost_registrations_.erase(it);
    }
  } else {
    // We skip binding the receiver if any registrations are still ongoing
    if (base::Contains(ongoing_background_datahost_registrations_,
                       navigation_id)) {
      return;
    }

    for (const auto& registration : registrations_) {
      if (registration.navigation_id() == navigation_id) {
        return;
      }
    }

    for (const auto& waiting :
         navigations_waiting_on_background_registrations_) {
      if (waiting.second.navigation_id() == navigation_id) {
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

    if (BackgroundRegistrationsEnabled()) {
      for (auto& registration : registrations_) {
        if (registration.defer_until_navigation() == navigation_id) {
          registration.ClearDeferUntilNavigation();
          if (!registration.pending_web_decodes().empty()) {
            HandleNextWebDecode(registration);
          }
          if (!registration.pending_os_decodes().empty()) {
            HandleNextOsDecode(registration);
          }
        }
      }
    }
  }
}

}  // namespace content
