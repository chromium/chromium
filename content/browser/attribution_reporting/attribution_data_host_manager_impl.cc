// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_data_host_manager_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
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
#include "base/functional/overloaded.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/attribution_reporting/attribution_scopes_data.h"
#include "components/attribution_reporting/attribution_scopes_set.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/data_host.mojom.h"
#include "components/attribution_reporting/os_registration.h"
#include "components/attribution_reporting/os_registration_error.mojom.h"
#include "components/attribution_reporting/registrar.h"
#include "components/attribution_reporting/registrar_info.h"
#include "components/attribution_reporting/registration_eligibility.mojom.h"
#include "components/attribution_reporting/registration_header_error.h"
#include "components/attribution_reporting/registration_info.h"
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
#include "content/browser/attribution_reporting/attribution_reporting.mojom-shared.h"
#include "content/browser/attribution_reporting/attribution_suitable_context.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/attribution_utils.h"
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
#include "services/network/public/cpp/attribution_utils.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/attribution.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using ::attribution_reporting::IssueType;
using ::attribution_reporting::Registrar;
using ::attribution_reporting::RegistrationHeaderErrorDetails;
using ::attribution_reporting::SuitableOrigin;
using ::attribution_reporting::mojom::OsRegistrationError;
using ::attribution_reporting::mojom::RegistrationEligibility;
using ::attribution_reporting::mojom::RegistrationType;
using ::attribution_reporting::mojom::SourceRegistrationError;
using ::attribution_reporting::mojom::SourceType;
using ::attribution_reporting::mojom::TriggerRegistrationError;
using ::blink::mojom::AttributionReportingIssueType;
using AttributionReportingOsRegistrar =
    ::content::ContentBrowserClient::AttributionReportingOsRegistrar;
using AttributionReportingOsRegistrars =
    ::content::ContentBrowserClient::AttributionReportingOsRegistrars;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(NavigationDataHostStatus)
enum class NavigationDataHostStatus {
  kRegistered = 0,
  kNotFound = 1,
  // Ineligible navigation data hosts (non top-level navigations, same document
  // navigations, etc) are dropped.
  kIneligible = 2,
  kProcessed = 3,

  kMaxValue = kProcessed,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/attribution_reporting/enums.xml:ConversionNavigationDataHostStatus)

void RecordNavigationDataHostStatus(NavigationDataHostStatus event) {
  base::UmaHistogramEnumeration("Conversions.NavigationDataHostStatus3", event);
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(RegistrationMethod)
enum class RegistrationMethod {
  kNavForeground = 0,
  kNavBackgroundBlink = 1,
  kNavBackgroundBlinkViaSW = 2,
  kNavBackgroundBrowser = 3,
  kFencedFrameBeacon = 4,
  kFencedFrameAutomaticBeacon = 5,
  kForegroundBlink = 6,
  kForegroundBlinkViaSW = 7,
  kBackgroundBlink = 8,
  kBackgroundBlinkViaSW = 9,
  kForegroundOrBackgroundBrowser = 10,
  kMaxValue = kForegroundOrBackgroundBrowser,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/attribution_reporting/enums.xml:ConversionsRegistrationMethod)

void RecordRegistrationMethod(RegistrationMethod method) {
  base::UmaHistogramEnumeration("Conversions.RegistrationMethod2", method);
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(RegisterDataHostOutcome)
enum class RegisterDataHostOutcome {
  kProcessedImmediately = 0,
  kDeferred = 1,
  kDropped = 2,
  kMaxValue = kDropped,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/attribution_reporting/enums.xml:ConversionRegisterDataHostOutcome)

void RecordRegisterDataHostHostOutcome(RegisterDataHostOutcome status) {
  base::UmaHistogramEnumeration("Conversions.RegisterDataHostOutcome", status);
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(NavigationUnexpectedRegistration)
enum class NavigationUnexpectedRegistration {
  kRegistrationAlreadyExists = 0,
  kRegistrationMissingUponReceivingData = 1,
  kMaxValue = kRegistrationMissingUponReceivingData,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/attribution_reporting/enums.xml:ConversionNavigationUnexpectedRegistration)

// See https://crbug.com/1500667 for details. There are assumptions that a
// navigation registration can only be registered once and that it must be
// registered and will be available when receiving data. Crashes challenges
// these assumptions.
void RecordNavigationUnexpectedRegistration(
    NavigationUnexpectedRegistration status) {
  base::UmaHistogramEnumeration("Conversions.NavigationUnexpectedRegistration",
                                status);
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(BackgroundNavigationOutcome)
enum class BackgroundNavigationOutcome {
  kTiedImmediately = 0,
  kTiedWithDelay = 1,
  kNeverTiedTimeout = 2,
  kNeverTiedIneligible = 3,
  kMaxValue = kNeverTiedIneligible,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/attribution_reporting/enums.xml:ConversionBackgroundNavigationOutcome)

void RecordBackgroundNavigationOutcome(BackgroundNavigationOutcome outcome) {
  base::UmaHistogramEnumeration("Conversions.BackgroundNavigation.Outcome",
                                outcome);
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(NavigationSourceScopesLimitOutcome)
enum class NavigationSourceScopesLimitOutcome {
  kNoScopesAllowed = 0,
  kNoScopesDropped = 1,
  kScopesAllowed = 2,
  kScopesDropped = 3,
  kMaxValue = kScopesDropped,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/attribution_reporting/enums.xml:ConversionNavigationSourceScopesLimitOutcome)

void RecordNavigationSourceScopesLimitOutcome(
    NavigationSourceScopesLimitOutcome outcome) {
  base::UmaHistogramEnumeration(
      "Conversions.NavigationSourceScopesLimitOutcome", outcome);
}

void RecordGoogleAmpViewerUsage(RegistrationType type,
                                bool is_context_google_amp_viewer) {
  switch (type) {
    case RegistrationType::kSource:
      base::UmaHistogramBoolean("Conversions.GoogleAmpViewer.Source",
                                is_context_google_amp_viewer);
      break;
    case RegistrationType::kTrigger:
      base::UmaHistogramBoolean("Conversions.GoogleAmpViewer.Trigger",
                                is_context_google_amp_viewer);
      break;
  }
}

bool BackgroundRegistrationsEnabled() {
  return (base::FeatureList::IsEnabled(
              blink::features::kKeepAliveInBrowserMigration) ||
          base::FeatureList::IsEnabled(blink::features::kFetchLaterAPI)) &&
         base::FeatureList::IsEnabled(
             blink::features::kAttributionReportingInBrowserMigration);
}

constexpr size_t kMaxDeferredReceiversPerNavigation = 30;

constexpr base::TimeDelta kWaitingOnNavigationRegistrationsTimeout =
    base::Seconds(20);

void MaybeLogAuditIssue(GlobalRenderFrameHostId render_frame_id,
                        const GURL& request_url,
                        const std::optional<std::string>& request_devtools_id,
                        std::optional<std::string> invalid_parameter,
                        AttributionReportingIssueType violation_type) {
  auto* render_frame_host = RenderFrameHost::FromID(render_frame_id);
  if (!render_frame_host) {
    return;
  }

  auto issue = blink::mojom::InspectorIssueInfo::New();

  issue->code = blink::mojom::InspectorIssueCode::kAttributionReportingIssue;

  auto details = blink::mojom::AttributionReportingIssueDetails::New();
  details->violation_type = violation_type;
  details->invalid_parameter = std::move(invalid_parameter);

  auto affected_request = blink::mojom::AffectedRequest::New();
  if (request_devtools_id.has_value()) {
    affected_request->request_id = request_devtools_id.value();
  }
  affected_request->url = request_url.spec();
  details->request = std::move(affected_request);

  issue->details = blink::mojom::InspectorIssueDetails::New();
  issue->details->attribution_reporting_issue_details = std::move(details);

  render_frame_host->ReportInspectorIssue(std::move(issue));
}

Registrar ConvertToRegistrar(AttributionReportingOsRegistrar os_registrar) {
  switch (os_registrar) {
    case AttributionReportingOsRegistrar::kWeb:
      return Registrar::kWeb;
    case AttributionReportingOsRegistrar::kOs:
      return Registrar::kOs;
    case AttributionReportingOsRegistrar::kDisabled:
      NOTREACHED();
  }
}

}  // namespace

struct AttributionDataHostManagerImpl::SequentialTimeoutsTimer::Timeout {
  base::TimeTicks time;
  base::OnceClosure callback;
};

AttributionDataHostManagerImpl::SequentialTimeoutsTimer::
    SequentialTimeoutsTimer(base::TimeDelta delay)
    : delay_(delay) {}

AttributionDataHostManagerImpl::SequentialTimeoutsTimer::
    ~SequentialTimeoutsTimer() = default;

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

class AttributionDataHostManagerImpl::NavigationForPendingRegistration {
 public:
  explicit NavigationForPendingRegistration(size_t pending_registrations_count)
      : pending_registrations_count_(pending_registrations_count) {}

  NavigationForPendingRegistration(const NavigationForPendingRegistration&) =
      delete;
  NavigationForPendingRegistration& operator=(
      const NavigationForPendingRegistration&) = delete;

  NavigationForPendingRegistration(NavigationForPendingRegistration&&) =
      default;
  NavigationForPendingRegistration& operator=(
      NavigationForPendingRegistration&&) = default;

  // Instances of the class are eagerly initialized in
  // `RegisterNavigationDataHost` as this is when we learn of the expected
  // `pending_registrations_count_`. However, until the context is set
  // (`SetContext`) or we learn that the navigation isn't eligible
  // (`DeclareIneligible`), a background registration cannot obtain any
  // information.
  bool CanBeUsed() const { return eligible_.has_value(); }

  // Returns true if the navigation for which the context is cached is eligible,
  // returns false otherwise. Should only be called if `CanBeUsed` returns true.
  bool IsEligible() const {
    CHECK(CanBeUsed());

    return eligible_.value();
  }

  void DeclareIneligible() {
    CHECK(!eligible_.has_value());

    eligible_ = false;
  }

  void Set(int64_t navigation_id) {
    CHECK(!eligible_.has_value());

    navigation_id_ = navigation_id;
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

  std::optional<int64_t> navigation_id() const { return navigation_id_; }

 private:
  size_t pending_registrations_count_;
  std::optional<bool> eligible_;
  std::optional<int64_t> navigation_id_;
};

class AttributionDataHostManagerImpl::RegistrationContext {
 public:
  RegistrationContext(AttributionSuitableContext suitable_context,
                      RegistrationEligibility registration_eligibility,
                      std::optional<std::string> devtools_request_id,
                      std::optional<int64_t> navigation_id,
                      RegistrationMethod method)
      : suitable_context_(std::move(suitable_context)),
        registration_eligibility_(registration_eligibility),
        devtools_request_id_(std::move(devtools_request_id)),
        method_(method),
        navigation_id_(navigation_id) {
    CHECK(!navigation_id_.has_value() ||
          registration_eligibility_ == RegistrationEligibility::kSource);
  }

  ~RegistrationContext() = default;

  RegistrationContext(const RegistrationContext&) = default;
  RegistrationContext& operator=(const RegistrationContext&) = default;

  RegistrationContext(RegistrationContext&&) = default;
  RegistrationContext& operator=(RegistrationContext&&) = default;

  const SuitableOrigin& context_origin() const {
    return suitable_context_.context_origin();
  }

  bool is_context_google_amp_viewer() const {
    return suitable_context_.is_context_google_amp_viewer();
  }

  RegistrationMethod GetRegistrationMethod(
      bool was_fetched_via_service_worker) const {
    switch (method_) {
      case RegistrationMethod::kNavBackgroundBlink:
        return was_fetched_via_service_worker
                   ? RegistrationMethod::kNavBackgroundBlinkViaSW
                   : method_;
      case RegistrationMethod::kForegroundBlink:
        return was_fetched_via_service_worker
                   ? RegistrationMethod::kForegroundBlinkViaSW
                   : method_;
      case RegistrationMethod::kBackgroundBlink:
        return was_fetched_via_service_worker
                   ? RegistrationMethod::kBackgroundBlinkViaSW
                   : method_;
      // Fetched via service worker is not applicable to foreground
      // registrations.
      case RegistrationMethod::kNavForeground:
      // keep alive is not supported in service workers. As such, for browser
      // registrations `was_fetched_via_serivce_worker` can only be false.
      // TODO(crbug.com/41496810): Once service worker keep alive
      // requests are supported, handle it here.
      case RegistrationMethod::kNavBackgroundBrowser:
      case RegistrationMethod::kForegroundOrBackgroundBrowser:
      // TODO(anthonygarant): propagate the information on whether fenced frame
      // registrations were fetched from a service worker or not.
      case RegistrationMethod::kFencedFrameBeacon:
      case RegistrationMethod::kFencedFrameAutomaticBeacon:
        CHECK(!was_fetched_via_service_worker);
        return method_;
      // When the context is created, we don't know if the registration was
      // processed via a service worker or not.
      case RegistrationMethod::kNavBackgroundBlinkViaSW:
      case RegistrationMethod::kForegroundBlinkViaSW:
      case RegistrationMethod::kBackgroundBlinkViaSW:
        NOTREACHED();
    }
  }

  RegistrationEligibility registration_eligibility() const {
    return registration_eligibility_;
  }

  bool is_within_fenced_frame() const {
    return suitable_context_.is_nested_within_fenced_frame();
  }

  const std::optional<std::string>& devtools_request_id() const {
    return devtools_request_id_;
  }

  GlobalRenderFrameHostId render_frame_id() const {
    return suitable_context_.root_render_frame_id();
  }

  const AttributionInputEvent& last_input_event() const {
    return suitable_context_.last_input_event();
  }

  AttributionReportingOsRegistrars os_registrars() const {
    return suitable_context_.os_registrars();
  }

  std::optional<int64_t> navigation_id() const { return navigation_id_; }

  void SetNavigation(int64_t navigation_id) {
    CHECK(!navigation_id_.has_value());
    navigation_id_.emplace(navigation_id);
  }

  // Contexts are considered equivalent if their properties are equals except
  // for those related to the registration channel.
  bool IsEquivalent(const RegistrationContext& other) const {
    // Ignores `devtools_request_id_`, `registration_eligibility_` and
    // `method_`.
    return suitable_context_ == other.suitable_context_ &&
           navigation_id_ == other.navigation_id_;
  }

  [[nodiscard]] bool CheckRegistrarSupport(Registrar, RegistrationType) const;

 private:
  // Context in which the attribution was initiated.
  AttributionSuitableContext suitable_context_;

  // Logically const.
  RegistrationEligibility registration_eligibility_;

  // For sources & triggers received through the data host, issues are
  // identified and reported in blink. As such, we don't need to plumb the
  // devtools request ID. A request might also not have a defined ID when there
  // are no devtools agents registered.
  std::optional<std::string> devtools_request_id_;

  // Sources and triggers can be received via different methods, we cache the
  // one that was used to create this context to then be able to record the
  // Conversions.RegistrationMethod histogram.
  RegistrationMethod method_;

  // When the registration is tied to a navigation, we store its id.
  std::optional<int64_t> navigation_id_;
};

struct AttributionDataHostManagerImpl::DeferredReceiver {
  mojo::PendingReceiver<attribution_reporting::mojom::DataHost> data_host;
  RegistrationContext context;
  base::TimeTicks initial_registration_time = base::TimeTicks::Now();
};

struct AttributionDataHostManagerImpl::HeaderPendingDecode {
  std::string header;
  SuitableOrigin reporting_origin;
  GURL reporting_url;
  bool report_header_errors;
  RegistrationType registration_type;

  HeaderPendingDecode(std::string header,
                      SuitableOrigin reporting_origin,
                      GURL reporting_url,
                      bool report_header_errors,
                      RegistrationType registration_type)
      : header(std::move(header)),
        reporting_origin(std::move(reporting_origin)),
        reporting_url(std::move(reporting_url)),
        report_header_errors(report_header_errors),
        registration_type(registration_type) {
    CHECK_EQ(*this->reporting_origin, url::Origin::Create(this->reporting_url));
  }

  HeaderPendingDecode(const HeaderPendingDecode&) = delete;
  HeaderPendingDecode& operator=(const HeaderPendingDecode&) = delete;

  HeaderPendingDecode(HeaderPendingDecode&&) = default;
  HeaderPendingDecode& operator=(HeaderPendingDecode&&) = default;
};

class AttributionDataHostManagerImpl::Registrations {
 public:
  Registrations(RegistrationsId id,
                RegistrationContext context,
                bool waiting_on_navigation,
                std::optional<int64_t> defer_until_navigation)
      : waiting_on_navigation_(waiting_on_navigation),
        defer_until_navigation_(defer_until_navigation),
        id_(id),
        context_(std::move(context)) {}

  Registrations(const Registrations&) = delete;
  Registrations& operator=(const Registrations&) = delete;

  Registrations(Registrations&&) = default;
  Registrations& operator=(Registrations&&) = default;

  const RegistrationContext& context() const { return context_; }

  const SuitableOrigin& context_origin() const {
    return context_.context_origin();
  }

  RegistrationEligibility eligibility() const {
    return context_.registration_eligibility();
  }

  bool has_pending_decodes() const {
    return !pending_os_decodes_.empty() || !pending_web_decodes_.empty() ||
           !pending_registration_data_.empty();
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

  std::optional<int64_t> navigation_id() const {
    return context_.navigation_id();
  }

  GlobalRenderFrameHostId render_frame_id() const {
    return context_.render_frame_id();
  }

  AttributionReportingOsRegistrars os_registrars() const {
    return context_.os_registrars();
  }

  const std::optional<std::string>& devtools_request_id() const {
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

  const base::circular_deque<PendingRegistrationData>&
  pending_registration_data() const {
    return pending_registration_data_;
  }

  base::circular_deque<PendingRegistrationData>& pending_registration_data() {
    return pending_registration_data_;
  }

  std::optional<int64_t> defer_until_navigation() const {
    return defer_until_navigation_;
  }

  bool operator<(const Registrations& other) const { return id_ < other.id_; }

  void CompleteRegistrations() {
    CHECK(!registrations_complete_);
    registrations_complete_ = true;
  }

  void SetNavigation(int64_t navigation_id) {
    CHECK(waiting_on_navigation_);
    context_.SetNavigation(navigation_id);
    waiting_on_navigation_ = false;
  }

  void ClearDeferUntilNavigation() { defer_until_navigation_.reset(); }

  friend bool operator<(const Registrations& a, const RegistrationsId& b) {
    return a.id_ < b;
  }

  friend bool operator<(const RegistrationsId& a, const Registrations& b) {
    return a < b.id_;
  }

  RegistrationsId id() const { return id_; }

  void MaybeLogIssue(const GURL& request_url,
                     std::optional<std::string> invalid_parameter,
                     AttributionReportingIssueType issue_type) const {
    MaybeLogAuditIssue(render_frame_id(), request_url, devtools_request_id(),
                       std::move(invalid_parameter), issue_type);
  }

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
  std::optional<int64_t> defer_until_navigation_;

  RegistrationsId id_;

  base::circular_deque<HeaderPendingDecode> pending_web_decodes_;

  base::circular_deque<HeaderPendingDecode> pending_os_decodes_;

  base::circular_deque<PendingRegistrationData> pending_registration_data_;

  RegistrationContext context_;
};

class AttributionDataHostManagerImpl::PendingRegistrationData {
 public:
  static std::optional<PendingRegistrationData> Get(
      const net::HttpResponseHeaders* headers,
      const Registrations& registrations,
      GURL reporting_url,
      SuitableOrigin reporting_origin) {
    if (!headers) {
      return std::nullopt;
    }

    const bool cross_app_web_enabled =
        base::FeatureList::IsEnabled(
            network::features::kAttributionReportingCrossAppWeb);

    std::optional<std::string> web_source_header = headers->GetNormalizedHeader(
        attribution_reporting::kAttributionReportingRegisterSourceHeader);

    std::optional<std::string> web_trigger_header =
        headers->GetNormalizedHeader(
            attribution_reporting::kAttributionReportingRegisterTriggerHeader);

    std::optional<std::string> os_source_header;
    std::optional<std::string> os_trigger_header;
    if (cross_app_web_enabled) {
      os_source_header = headers->GetNormalizedHeader(
          attribution_reporting::kAttributionReportingRegisterOsSourceHeader);

      os_trigger_header = headers->GetNormalizedHeader(
          attribution_reporting::kAttributionReportingRegisterOsTriggerHeader);
    }

    const bool has_source =
        web_source_header.has_value() || os_source_header.has_value();
    const bool has_trigger =
        web_trigger_header.has_value() || os_trigger_header.has_value();

    if (!has_source && !has_trigger) {
      return std::nullopt;
    }

    RegistrationType registration_type;
    std::optional<std::string> web_header;
    std::optional<std::string> os_header;
    switch (registrations.eligibility()) {
      case RegistrationEligibility::kSource:
        if (web_trigger_header.has_value()) {
          registrations.MaybeLogIssue(
              reporting_url, std::move(web_trigger_header),
              AttributionReportingIssueType::kTriggerIgnored);
        }
        if (os_trigger_header.has_value()) {
          registrations.MaybeLogIssue(
              reporting_url, std::move(os_trigger_header),
              AttributionReportingIssueType::kOsTriggerIgnored);
        }

        registration_type = RegistrationType::kSource;
        web_header = std::move(web_source_header);
        os_header = std::move(os_source_header);
        break;
      case RegistrationEligibility::kTrigger:
        if (web_source_header.has_value()) {
          registrations.MaybeLogIssue(
              reporting_url, std::move(web_source_header),
              AttributionReportingIssueType::kSourceIgnored);
        }
        if (os_source_header.has_value()) {
          registrations.MaybeLogIssue(
              reporting_url, std::move(os_source_header),
              AttributionReportingIssueType::kOsSourceIgnored);
        }

        registration_type = RegistrationType::kTrigger;
        web_header = std::move(web_trigger_header);
        os_header = std::move(os_trigger_header);
        break;
      case RegistrationEligibility::kSourceOrTrigger:
        if (has_source && has_trigger) {
          registrations.MaybeLogIssue(
              reporting_url,
              /*invalid_parameter=*/std::nullopt,
              AttributionReportingIssueType::kSourceAndTriggerHeaders);
          return std::nullopt;
        }
        if (has_source) {
          registration_type = RegistrationType::kSource;
          web_header = std::move(web_source_header);
          os_header = std::move(os_source_header);
        } else if (has_trigger) {
          registration_type = RegistrationType::kTrigger;
          web_header = std::move(web_trigger_header);
          os_header = std::move(os_trigger_header);
        }
        break;
    }

    if (!web_header.has_value() && !os_header.has_value()) {
      return std::nullopt;
    }

    std::string info_header =
        headers->GetNormalizedHeader(kAttributionReportingInfoHeader)
            .value_or(std::string());

    return PendingRegistrationData(
        std::move(info_header), std::move(web_header), std::move(os_header),
        registration_type, cross_app_web_enabled, std::move(reporting_url),
        std::move(reporting_origin));
  }

  PendingRegistrationData(const PendingRegistrationData&) = delete;
  PendingRegistrationData& operator=(const PendingRegistrationData&) = delete;

  PendingRegistrationData(PendingRegistrationData&&) = default;
  PendingRegistrationData& operator=(PendingRegistrationData&&) = default;

  std::optional<HeaderPendingDecode> ToHeaderPendingDecode(
      const Registrations& registrations,
      const attribution_reporting::RegistrarInfo& registrar_info,
      bool report_header_errors) && {
    for (IssueType issue_type : registrar_info.issues) {
      switch (issue_type) {
        case IssueType::kWebAndOsHeaders:
          registrations.MaybeLogIssue(
              reporting_url_, /*invalid_parameter=*/std::nullopt,
              AttributionReportingIssueType::kWebAndOsHeaders);
          break;
        case IssueType::kWebIgnored:
          registrations.MaybeLogIssue(
              reporting_url_, std::move(web_header_),
              is_source() ? AttributionReportingIssueType::kSourceIgnored
                          : AttributionReportingIssueType::kTriggerIgnored);
          break;
        case IssueType::kOsIgnored:
          registrations.MaybeLogIssue(
              reporting_url_, std::move(os_header_),
              is_source() ? AttributionReportingIssueType::kOsSourceIgnored
                          : AttributionReportingIssueType::kOsTriggerIgnored);
          break;
        case IssueType::kNoWebHeader:
          registrations.MaybeLogIssue(
              reporting_url_, /*invalid_parameter=*/std::nullopt,
              is_source()
                  ? AttributionReportingIssueType::kNoRegisterSourceHeader
                  : AttributionReportingIssueType::kNoRegisterTriggerHeader);
          break;
        case IssueType::kNoOsHeader:
          registrations.MaybeLogIssue(
              reporting_url_, /*invalid_parameter=*/std::nullopt,
              is_source()
                  ? AttributionReportingIssueType::kNoRegisterOsSourceHeader
                  : AttributionReportingIssueType::kNoRegisterOsTriggerHeader);
          break;
      }
    }

    if (!registrar_info.registrar.has_value()) {
      return std::nullopt;
    }

    std::optional<std::string> header;
    switch (*registrar_info.registrar) {
      case Registrar::kWeb:
        header = std::move(web_header_);
        break;
      case Registrar::kOs:
        header = std::move(os_header_);
        break;
    }

    CHECK(header.has_value());

    return HeaderPendingDecode(*std::move(header), std::move(reporting_origin_),
                               std::move(reporting_url_), report_header_errors,
                               type_);
  }

  const std::string& info_header() const { return info_header_; }

  bool has_web_header() const { return web_header_.has_value(); }

  bool has_os_header() const { return os_header_.has_value(); }

  bool is_source() const { return type_ == RegistrationType::kSource; }

  bool cross_app_web_enabled() const { return cross_app_web_enabled_; }

  void MaybeLogInvalidInfoHeader(const Registrations& registrations) && {
    registrations.MaybeLogIssue(
        reporting_url_, std::move(info_header_),
        AttributionReportingIssueType::kInvalidInfoHeader);
  }

 private:
  PendingRegistrationData(std::string info_header,
                          std::optional<std::string> web_header,
                          std::optional<std::string> os_header,
                          RegistrationType type,
                          bool cross_app_web_enabled,
                          GURL reporting_url,
                          SuitableOrigin reporting_origin)
      : info_header_(std::move(info_header)),
        web_header_(std::move(web_header)),
        os_header_(std::move(os_header)),
        type_(type),
        cross_app_web_enabled_(cross_app_web_enabled),
        reporting_url_(std::move(reporting_url)),
        reporting_origin_(std::move(reporting_origin)) {
    CHECK_EQ(*reporting_origin_, url::Origin::Create(reporting_url_));
  }

  std::string info_header_;
  std::optional<std::string> web_header_;
  std::optional<std::string> os_header_;
  RegistrationType type_;
  bool cross_app_web_enabled_ = false;
  GURL reporting_url_;
  SuitableOrigin reporting_origin_;
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AttributionDataHostManagerImpl::OsRegistrationsBufferFlushReason {
  kNavigationDone = 0,
  kBufferFull = 1,
  kTimeout = 2,
  kMaxValue = kTimeout,
};

class AttributionDataHostManagerImpl::OsRegistrationsBuffer {
 public:
  explicit OsRegistrationsBuffer(int64_t navigation_id)
      : navigation_id_(navigation_id) {}

  OsRegistrationsBuffer(const OsRegistrationsBuffer&) = delete;
  OsRegistrationsBuffer& operator=(const OsRegistrationsBuffer&) = delete;

  OsRegistrationsBuffer(OsRegistrationsBuffer&&) = default;
  OsRegistrationsBuffer& operator=(OsRegistrationsBuffer&&) = default;

  bool operator<(const OsRegistrationsBuffer& other) const {
    return navigation_id_ < other.navigation_id_;
  }

  friend bool operator<(int64_t navigation_id,
                        const OsRegistrationsBuffer& other) {
    return navigation_id < other.navigation_id_;
  }

  friend bool operator<(const OsRegistrationsBuffer& other,
                        int64_t navigation_id) {
    return other.navigation_id_ < navigation_id;
  }

  // Buffer `items` until the buffer is full. Returns items that were not added
  // to the buffer.
  std::vector<attribution_reporting::OsRegistrationItem> Buffer(
      std::vector<attribution_reporting::OsRegistrationItem> items,
      const RegistrationContext& registration_context) {
    // Only navigation-tied OS registrations should be buffered.
    CHECK(registration_context.navigation_id().has_value());
    CHECK_EQ(registration_context.navigation_id().value(), navigation_id_);
    if (!context_.has_value()) {
      context_ = registration_context;
    } else {
      // TODO(anthonygarant): Convert to CHECK after validating that the
      // contexts are always equivalent.
      base::UmaHistogramBoolean(
          "Conversions.OsRegistrationsBufferWithSameContext",
          context_->IsEquivalent(registration_context));
    }

    CHECK_LE(registrations_.size(), kMaxBufferSize);
    const size_t items_to_buffer =
        std::min(kMaxBufferSize - registrations_.size(), items.size());

    registrations_.reserve(registrations_.size() + items_to_buffer);
    std::move(items.begin(), items.begin() + items_to_buffer,
              std::back_inserter(registrations_));

    std::vector<attribution_reporting::OsRegistrationItem> non_buffered;
    non_buffered.reserve(items.size() - items_to_buffer);
    std::move(items.begin() + items_to_buffer, items.end(),
              std::back_inserter(non_buffered));

    return non_buffered;
  }

  bool IsEmpty() const { return registrations_.empty(); }

  bool IsFull() const { return registrations_.size() == kMaxBufferSize; }

  const RegistrationContext& context() const {
    CHECK(context_.has_value());
    return context_.value();
  }

  std::vector<attribution_reporting::OsRegistrationItem>
  TakeRegistrationItems() {
    return std::exchange(registrations_, {});
  }

 private:
  // TODO(crbug.com/40267739): update to 80 when supported by the OS.
  static constexpr size_t kMaxBufferSize = 20u;

  int64_t navigation_id_;

  std::optional<RegistrationContext> context_;
  std::vector<attribution_reporting::OsRegistrationItem> registrations_;
};

struct AttributionDataHostManagerImpl::
    ScopesAndCountForReportingOriginPerNavigation {
  int count = 0;
  attribution_reporting::AttributionScopesSet scopes;
};

AttributionDataHostManagerImpl::AttributionDataHostManagerImpl(
    AttributionManager* attribution_manager)
    : attribution_manager_(
          raw_ref<AttributionManager>::from_ptr(attribution_manager)),
      background_registrations_waiting_on_navigation_timer_(
          /*delay=*/base::Seconds(3)),
      navigations_waiting_on_background_registrations_timer_(
          /*delay=*/base::Seconds(3)),
      navigation_registrations_timer_(
          /*delay=*/kWaitingOnNavigationRegistrationsTimeout) {
  receivers_.set_disconnect_handler(base::BindRepeating(
      &AttributionDataHostManagerImpl::OnReceiverDisconnected,
      base::Unretained(this)));
}

AttributionDataHostManagerImpl::~AttributionDataHostManagerImpl() = default;

void AttributionDataHostManagerImpl::RegisterDataHost(
    mojo::PendingReceiver<attribution_reporting::mojom::DataHost> data_host,
    AttributionSuitableContext suitable_context,
    RegistrationEligibility registration_eligibility,
    bool is_for_background_requests) {
  int64_t last_navigation_id = suitable_context.last_navigation_id();
  RegistrationContext receiver_context(
      std::move(suitable_context), registration_eligibility,
      /*devtools_request_id=*/std::nullopt, /*navigation_id=*/std::nullopt,
      is_for_background_requests ? RegistrationMethod::kBackgroundBlink
                                 : RegistrationMethod::kForegroundBlink);
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
          receivers_it->second.emplace_back(std::move(data_host),
                                            std::move(receiver_context));
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
    mojo::PendingReceiver<attribution_reporting::mojom::DataHost> data_host,
    const blink::AttributionSrcToken& attribution_src_token) {
  auto [_, inserted] = navigation_data_host_map_.try_emplace(
      attribution_src_token, std::move(data_host));
  // Should only be possible with a misbehaving renderer.
  if (!inserted) {
    return false;
  }

  RecordNavigationDataHostStatus(NavigationDataHostStatus::kRegistered);
  return true;
}

void AttributionDataHostManagerImpl::ParseHeader(
    Registrations& registrations,
    HeaderPendingDecode pending_decode,
    Registrar registrar) {
  switch (registrations.eligibility()) {
    case RegistrationEligibility::kSourceOrTrigger:
      break;
    case RegistrationEligibility::kSource:
      CHECK_EQ(pending_decode.registration_type, RegistrationType::kSource);
      break;
    case RegistrationEligibility::kTrigger:
      CHECK_EQ(pending_decode.registration_type, RegistrationType::kTrigger);
      break;
  }

  const auto handle = [](base::circular_deque<HeaderPendingDecode>& queue,
                         HeaderPendingDecode&& pending_decode,
                         const char* source_header_size_metric,
                         const char* trigger_header_size_metric) {
    const bool is_source =
        pending_decode.registration_type == RegistrationType::kSource;

    // Max header size is 256 KB, use 1M count to encapsulate.
    base::UmaHistogramCounts1M(
        is_source ? source_header_size_metric : trigger_header_size_metric,
        pending_decode.header.size());

    queue.emplace_back(std::move(pending_decode));

    // Only perform the decode if it is the only one in the queue. Otherwise,
    // there's already an async decode in progress.
    return queue.size() == 1;
  };

  switch (registrar) {
    case Registrar::kWeb:
      if (handle(registrations.pending_web_decodes(), std::move(pending_decode),
                 "Conversions.HeadersSize.RegisterSource",
                 "Conversions.HeadersSize.RegisterTrigger")) {
        HandleNextWebDecode(registrations);
      }
      break;
    case Registrar::kOs:
      if (auto* rfh =
              RenderFrameHostImpl::FromID(registrations.render_frame_id())) {
        GetContentClient()->browser()->LogWebFeatureForCurrentPage(
            rfh, blink::mojom::WebFeature::kAttributionReportingCrossAppWeb);
      }
      if (handle(registrations.pending_os_decodes(), std::move(pending_decode),
                 "Conversions.HeadersSize.RegisterOsSource",
                 "Conversions.HeadersSize.RegisterOsTrigger")) {
        HandleNextOsDecode(registrations);
      }
      break;
  }
}

void AttributionDataHostManagerImpl::HandleRegistrationData(
    base::flat_set<Registrations>::iterator it,
    PendingRegistrationData pending_registration_data) {
  CHECK(it != registrations_.end());

  it->pending_registration_data().emplace_back(
      std::move(pending_registration_data));
  // Only perform the parsing if it is the only one in the queue. Otherwise,
  // there's already an async decode in progress.
  if (it->pending_registration_data().size() == 1) {
    HandleNextRegistrationData(it);
  }
}

void AttributionDataHostManagerImpl::HandleNextRegistrationData(
    base::flat_set<Registrations>::iterator it) {
  CHECK(it != registrations_.end());

  if (!it->IsReadyToProcess()) {
    return;
  }

  CHECK(!it->pending_registration_data().empty());

  do {
    auto& pending_registration_data = it->pending_registration_data().front();

    if (!pending_registration_data.info_header().empty()) {
      data_decoder_.ParseStructuredHeaderDictionary(
          pending_registration_data.info_header(),
          base::BindOnce(&AttributionDataHostManagerImpl::OnInfoHeaderParsed,
                         weak_factory_.GetWeakPtr(), it->id()));
      return;
    }

    HandleRegistrationInfo(*it, std::move(pending_registration_data),
                           attribution_reporting::RegistrationInfo());

    it->pending_registration_data().pop_front();
  } while (!it->pending_registration_data().empty());

  MaybeOnRegistrationsFinished(it);
}

void AttributionDataHostManagerImpl::OnInfoHeaderParsed(
    RegistrationsId id,
    InfoParseResult result) {
  auto it = registrations_.find(id);
  CHECK(it != registrations_.end());
  CHECK(!it->pending_registration_data().empty());

  {
    auto& pending_registration_data = it->pending_registration_data().front();

    base::expected<attribution_reporting::RegistrationInfo,
                   attribution_reporting::RegistrationInfoError>
        registration_info(
            base::unexpect,
            attribution_reporting::RegistrationInfoError::kRootInvalid);
    if (result.has_value()) {
      registration_info = attribution_reporting::RegistrationInfo::ParseInfo(
          *result, pending_registration_data.cross_app_web_enabled());
    }

    if (registration_info.has_value()) {
      HandleRegistrationInfo(*it, std::move(pending_registration_data),
                             *registration_info);
    } else {
      RecordRegistrationInfoError(registration_info.error());
      std::move(pending_registration_data).MaybeLogInvalidInfoHeader(*it);
    }
  }

  it->pending_registration_data().pop_front();

  if (!it->pending_registration_data().empty()) {
    HandleNextRegistrationData(it);
  } else {
    MaybeOnRegistrationsFinished(it);
  }
}

void AttributionDataHostManagerImpl::HandleRegistrationInfo(
    Registrations& registrations,
    PendingRegistrationData pending_registration_data,
    const attribution_reporting::RegistrationInfo& registration_info) {
  const bool is_source = pending_registration_data.is_source();

  const bool client_os_disabled =
      is_source ? registrations.os_registrars().source_registrar ==
                      AttributionReportingOsRegistrar::kDisabled
                : registrations.os_registrars().trigger_registrar ==
                      AttributionReportingOsRegistrar::kDisabled;

  auto registrar_info = attribution_reporting::RegistrarInfo::Get(
      pending_registration_data.has_web_header(),
      pending_registration_data.has_os_header(), is_source,
      registration_info.preferred_platform,
      AttributionManager::GetAttributionSupport(client_os_disabled));

  std::optional<HeaderPendingDecode> pending_decode =
      std::move(pending_registration_data)
          .ToHeaderPendingDecode(registrations, registrar_info,
                                 registration_info.report_header_errors);

  if (!pending_decode.has_value()) {
    return;
  }

  ParseHeader(registrations, *std::move(pending_decode),
              registrar_info.registrar.value());
}

void AttributionDataHostManagerImpl::HandleNextWebDecode(
    const Registrations& registrations) {
  CHECK(registrations.IsReadyToProcess());

  CHECK(!registrations.pending_web_decodes().empty());

  const auto& pending_decode = registrations.pending_web_decodes().front();

  data_decoder_.ParseJson(
      pending_decode.header,
      base::BindOnce(&AttributionDataHostManagerImpl::OnWebHeaderParsed,
                     weak_factory_.GetWeakPtr(), registrations.id()));
}

void AttributionDataHostManagerImpl::HandleNextOsDecode(
    const Registrations& registrations) {
  CHECK(registrations.IsReadyToProcess());

  CHECK(!registrations.pending_os_decodes().empty());

  const auto& pending_decode = registrations.pending_os_decodes().front();

  data_decoder_.ParseStructuredHeaderList(
      pending_decode.header,
      base::BindOnce(&AttributionDataHostManagerImpl::OnOsHeaderParsed,
                     weak_factory_.GetWeakPtr(), registrations.id()));
}

bool AttributionDataHostManagerImpl::
    NotifyNavigationWithBackgroundRegistrationsWillStart(
        const blink::AttributionSrcToken& attribution_src_token,
        size_t expected_registrations) {
  // Should only be possible with a misbehaving renderer.
  if (!BackgroundRegistrationsEnabled() || expected_registrations == 0) {
    return false;
  }

  // We use the field below to keep the navigation id available until we've
  // received all expected background registrations.
  auto [_, inserted] =
      navigations_waiting_on_background_registrations_.try_emplace(
          attribution_src_token,
          NavigationForPendingRegistration(expected_registrations));
  // Failure should only be possible with a misbehaving renderer.
  return inserted;
}

void AttributionDataHostManagerImpl::NotifyNavigationRegistrationStarted(
    AttributionSuitableContext suitable_context,
    const blink::AttributionSrcToken& attribution_src_token,
    int64_t navigation_id,
    std::string devtools_request_id) {
  if (auto [_, inserted] = registrations_.emplace(
          RegistrationsId(attribution_src_token),
          RegistrationContext(suitable_context,
                              RegistrationEligibility::kSource,
                              std::move(devtools_request_id), navigation_id,
                              RegistrationMethod::kNavForeground),
          /*waiting_on_navigation=*/false,
          /*defer_until_navigation=*/std::nullopt);
      !inserted) {
    RecordNavigationUnexpectedRegistration(
        NavigationUnexpectedRegistration::kRegistrationAlreadyExists);
    return;
  }

  MaybeStartNavigation(navigation_id);

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
    CHECK(inserted);

    receivers_.Add(
        this, std::move(it->second),
        RegistrationContext(std::move(suitable_context),
                            RegistrationEligibility::kSource,
                            /*devtools_request_id=*/std::nullopt, navigation_id,
                            RegistrationMethod::kNavBackgroundBlink));

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
        it->SetNavigation(navigation_id);
        RecordBackgroundNavigationOutcome(
            BackgroundNavigationOutcome::kTiedWithDelay);

        if (!it->pending_registration_data().empty()) {
          HandleNextRegistrationData(it);
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
    it->second.Set(navigation_id);
  }
}

bool AttributionDataHostManagerImpl::NotifyNavigationRegistrationData(
    const blink::AttributionSrcToken& attribution_src_token,
    const net::HttpResponseHeaders* headers,
    GURL reporting_url) {
  auto reporting_origin = SuitableOrigin::Create(reporting_url);
  CHECK(reporting_origin);

  auto it = registrations_.find(attribution_src_token);
  if (it == registrations_.end()) {
    RecordNavigationUnexpectedRegistration(
        NavigationUnexpectedRegistration::
            kRegistrationMissingUponReceivingData);
    return false;
  }
  CHECK(!it->registrations_complete());

  auto pending_registration_data = PendingRegistrationData::Get(
      headers, *it, std::move(reporting_url), *std::move(reporting_origin));

  if (!pending_registration_data.has_value()) {
    return false;
  }

  HandleRegistrationData(it, *std::move(pending_registration_data));
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
    // The background registration will no longer be present if it completed
    // without attempting to register any data.
    if (auto background_it = registrations_.find(id);
        background_it != registrations_.end()) {
      CHECK(!background_it->IsReadyToProcess());
      registrations_.erase(background_it);
    }
    RecordBackgroundNavigationOutcome(
        due_to_timeout ? BackgroundNavigationOutcome::kNeverTiedTimeout
                       : BackgroundNavigationOutcome::kNeverTiedIneligible);
  }
  BackgroundRegistrationsTied(attribution_src_token,
                              /*count=*/it->second.size(),
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
  if (navigation_data_host_map_.erase(attribution_src_token)) {
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
  // token. This indicates that the navigation was ineligible for registrations.
  auto waiting_it = navigations_waiting_on_background_registrations_.find(
      attribution_src_token);
  if (auto registrations_it = registrations_.find(attribution_src_token);
      registrations_it != registrations_.end()) {
    registrations_it->CompleteRegistrations();
    MaybeOnRegistrationsFinished(registrations_it);
  } else if (waiting_it !=
             navigations_waiting_on_background_registrations_.end()) {
    waiting_it->second.DeclareIneligible();
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
    AttributionSuitableContext suitable_context,
    RegistrationEligibility registration_eligibility,
    std::optional<blink::AttributionSrcToken> attribution_src_token,
    std::optional<std::string> devtools_request_id) {
  CHECK(BackgroundRegistrationsEnabled());

  std::optional<int64_t> navigation_id;

  if (attribution_src_token.has_value()) {
    const blink::AttributionSrcToken& token = attribution_src_token.value();

    if (auto nav_waiting_it =
            navigations_waiting_on_background_registrations_.find(token);
        nav_waiting_it !=
            navigations_waiting_on_background_registrations_.end() &&
        nav_waiting_it->second.CanBeUsed()) {
      if (!nav_waiting_it->second.IsEligible()) {
        RecordBackgroundNavigationOutcome(
            BackgroundNavigationOutcome::kNeverTiedIneligible);
        // Since the navigation is ineligible, we return early and avoid
        // creating a registration, all further requests related to this
        // background id will simply be dropped.
        BackgroundRegistrationsTied(token, /*count=*/1,
                                    /*due_to_timeout=*/false);
        return;
      }
      navigation_id = nav_waiting_it->second.navigation_id();
    } else {
      // Navigation has not started yet
      //
      // We start waiting on the navigation even it hasn't been "announced" via
      // `RegisterNavigationDataHost` yet because for a given navigation, we
      // have have no guarantee that `RegisterNavigationDataHost` gets be called
      // before `NotifyBackgroundRegistrationStarted`.
      auto [waiting_it, inserted] =
          background_registrations_waiting_on_navigation_.try_emplace(
              token, base::flat_set<BackgroundRegistrationsId>());
      waiting_it->second.emplace(id);
      if (inserted) {
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
      attribution_src_token.has_value() && !navigation_id.has_value();
  std::optional<int64_t> deferred_until;
  if (deferred_receivers_.contains(suitable_context.last_navigation_id()) &&
      registration_eligibility != RegistrationEligibility::kSource) {
    deferred_until = suitable_context.last_navigation_id();
  }

  bool navigation_tied =
      attribution_src_token.has_value() && navigation_id.has_value();

  auto [_, inserted] = registrations_.emplace(
      RegistrationsId(id),
      RegistrationContext(
          std::move(suitable_context), registration_eligibility,
          std::move(devtools_request_id), navigation_id,
          attribution_src_token.has_value()
              ? RegistrationMethod::kNavBackgroundBrowser
              : RegistrationMethod::kForegroundOrBackgroundBrowser),
      waiting_on_navigation, deferred_until);
  CHECK(inserted);

  // We must indicate that the background registration was tied to the
  // navigation only after the registration is inserted to avoid an inaccurate
  // state where all expected registrations are tied and there are no ongoing
  // registrations tied to the navigation.
  if (navigation_tied) {
    RecordBackgroundNavigationOutcome(
        BackgroundNavigationOutcome::kTiedImmediately);
    BackgroundRegistrationsTied(attribution_src_token.value(), /*count=*/1,
                                /*due_to_timeout=*/false);
  }
}

bool AttributionDataHostManagerImpl::NotifyBackgroundRegistrationData(
    BackgroundRegistrationsId id,
    const net::HttpResponseHeaders* headers,
    GURL reporting_url) {
  CHECK(BackgroundRegistrationsEnabled());

  auto it = registrations_.find(id);
  // If the registrations cannot be found, it means that it was dropped early
  // due to being tied to an ineligible navigation.
  if (it == registrations_.end()) {
    return false;
  }
  CHECK(!it->registrations_complete());

  auto reporting_origin = url::Origin::Create(reporting_url);
  auto suitable_reporting_origin = SuitableOrigin::Create(reporting_origin);
  if (!suitable_reporting_origin.has_value()) {
    it->MaybeLogIssue(
        reporting_url,
        /*invalid_parameter=*/reporting_origin.Serialize(),
        AttributionReportingIssueType::kUntrustworthyReportingOrigin);
    return false;
  }

  auto pending_registration_data =
      PendingRegistrationData::Get(headers, *it, std::move(reporting_url),
                                   *std::move(suitable_reporting_origin));

  if (!pending_registration_data.has_value()) {
    return false;
  }

  HandleRegistrationData(it, *std::move(pending_registration_data));
  return true;
}

void AttributionDataHostManagerImpl::NotifyBackgroundRegistrationCompleted(
    BackgroundRegistrationsId id) {
  CHECK(BackgroundRegistrationsEnabled());

  auto it = registrations_.find(id);
  // If the registrations cannot be found, it means that it was dropped early
  // due to being tied to an ineligible navigation.
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
    mojo::ReportBadMessage("DataHost: Not eligible for source.");
    return nullptr;
  }

  return &context;
}

const AttributionDataHostManagerImpl::RegistrationContext*
AttributionDataHostManagerImpl::GetReceiverRegistrationContextForTrigger() {
  const RegistrationContext& context = receivers_.current_context();

  if (context.registration_eligibility() == RegistrationEligibility::kSource) {
    mojo::ReportBadMessage("DataHost: Not eligible for trigger.");
    return nullptr;
  }

  return &context;
}

void AttributionDataHostManagerImpl::SourceDataAvailable(
    SuitableOrigin reporting_origin,
    attribution_reporting::SourceRegistration data,
    bool was_fetched_via_service_worker) {
  // LINT.IfChange(DataAvailableCallSource)
  base::UmaHistogramEnumeration(
      "Conversions.DataAvailableCall.Source",
      attribution_reporting::mojom::DataAvailableCallsite::kBrowser);
  // LINT.ThenChange(//third_party/blink/renderer/core/frame/attribution_src_loader.cc:DataAvailableCallSource)
  // This is validated by the Mojo typemapping.
  CHECK(reporting_origin.IsValid());

  const RegistrationContext* context =
      GetReceiverRegistrationContextForSource();
  if (!context) {
    return;
  }

  auto source_type = SourceType::kEvent;
  auto navigation_id = context->navigation_id();
  if (navigation_id.has_value()) {
    source_type = SourceType::kNavigation;
  }

  if (!data.IsValidForSourceType(source_type)) {
    mojo::ReportBadMessage("DataHost: Source invalid for source type.");
    return;
  }

  if (!context->CheckRegistrarSupport(Registrar::kWeb,
                                      RegistrationType::kSource)) {
    return;
  }

  RecordRegistrationMethod(
      context->GetRegistrationMethod(was_fetched_via_service_worker));
  RecordGoogleAmpViewerUsage(RegistrationType::kSource,
                             context->is_context_google_amp_viewer());

  if (navigation_id.has_value() &&
      !AddNavigationSourceRegistrationToBatchMap(
          *navigation_id, reporting_origin, data, context->render_frame_id(),
          context->devtools_request_id())) {
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
    bool was_fetched_via_service_worker) {
  // LINT.IfChange(DataAvailableCallTrigger)
  base::UmaHistogramEnumeration(
      "Conversions.DataAvailableCall.Trigger",
      attribution_reporting::mojom::DataAvailableCallsite::kBrowser);
  // LINT.ThenChange(//third_party/blink/renderer/core/frame/attribution_src_loader.cc:DataAvailableCallTrigger)
  // This is validated by the Mojo typemapping.
  CHECK(reporting_origin.IsValid());

  const RegistrationContext* context =
      GetReceiverRegistrationContextForTrigger();
  if (!context) {
    return;
  }

  if (!context->CheckRegistrarSupport(Registrar::kWeb,
                                      RegistrationType::kTrigger)) {
    return;
  }

  RecordRegistrationMethod(
      context->GetRegistrationMethod(was_fetched_via_service_worker));
  RecordGoogleAmpViewerUsage(RegistrationType::kTrigger,
                             context->is_context_google_amp_viewer());
  attribution_manager_->HandleTrigger(
      AttributionTrigger(std::move(reporting_origin), std::move(data),
                         /*destination_origin=*/context->context_origin(),
                         context->is_within_fenced_frame()),
      context->render_frame_id());
}

void AttributionDataHostManagerImpl::OsDataAvailable(
    std::vector<attribution_reporting::OsRegistrationItem> registration_items,
    bool was_fetched_via_service_worker,
    const char* data_available_call_metric,
    const RegistrationContext* context,
    RegistrationType registration_type) {
  base::UmaHistogramEnumeration(
      data_available_call_metric,
      attribution_reporting::mojom::DataAvailableCallsite::kBrowser);

  if (!context || registration_items.empty()) {
    return;
  }

  if (!context->CheckRegistrarSupport(Registrar::kOs, registration_type)) {
    return;
  }

  RecordRegistrationMethod(
      context->GetRegistrationMethod(was_fetched_via_service_worker));
  RecordGoogleAmpViewerUsage(registration_type,
                             context->is_context_google_amp_viewer());
  if (context->navigation_id().has_value()) {
    MaybeBufferOsRegistrations(context->navigation_id().value(),
                               std::move(registration_items), *context);
    return;
  }

  SubmitOsRegistrations(std::move(registration_items), *context,
                        registration_type);
}

void AttributionDataHostManagerImpl::OsSourceDataAvailable(
    std::vector<attribution_reporting::OsRegistrationItem> registration_items,
    bool was_fetched_via_service_worker) {
  OsDataAvailable(
      std::move(registration_items), was_fetched_via_service_worker,
      // LINT.IfChange(DataAvailableCallOsSource)
      /*data_available_call_metric=*/
      "Conversions.DataAvailableCall.OsSource",
      // LINT.ThenChange(//third_party/blink/renderer/core/frame/attribution_src_loader.cc:DataAvailableCallOsSource)
      GetReceiverRegistrationContextForSource(), RegistrationType::kSource);
}

void AttributionDataHostManagerImpl::OsTriggerDataAvailable(
    std::vector<attribution_reporting::OsRegistrationItem> registration_items,
    bool was_fetched_via_service_worker) {
  OsDataAvailable(
      std::move(registration_items), was_fetched_via_service_worker,
      // LINT.IfChange(DataAvailableCallOsTrigger)
      /*data_available_call_metric=*/
      "Conversions.DataAvailableCall.OsTrigger",
      // LINT.ThenChange(//third_party/blink/renderer/core/frame/attribution_src_loader.cc:DataAvailableCallOsTrigger)
      GetReceiverRegistrationContextForTrigger(), RegistrationType::kTrigger);
}

void AttributionDataHostManagerImpl::OnReceiverDisconnected() {
  const RegistrationContext& context = receivers_.current_context();

  if (std::optional<int64_t> navigation_id = context.navigation_id();
      navigation_id.has_value() &&
      ongoing_background_datahost_registrations_.erase(*navigation_id)) {
    MaybeDoneWithNavigation(*navigation_id,
                            /*due_to_timeout=*/false);
  }
}

void AttributionDataHostManagerImpl::NotifyFencedFrameReportingBeaconStarted(
    BeaconId beacon_id,
    AttributionSuitableContext suitable_context,
    std::optional<int64_t> navigation_id,
    std::string devtools_request_id) {
  if (navigation_id.has_value()) {
    MaybeStartNavigation(navigation_id.value());
  }

  auto [_, inserted] = registrations_.emplace(
      RegistrationsId(beacon_id),
      RegistrationContext(std::move(suitable_context),
                          RegistrationEligibility::kSource,
                          std::move(devtools_request_id), navigation_id,
                          navigation_id.has_value()
                              ? RegistrationMethod::kFencedFrameAutomaticBeacon
                              : RegistrationMethod::kFencedFrameBeacon),
      /*waiting_on_navigation=*/false,
      /*defer_until_navigation=*/std::nullopt);
  CHECK(inserted);
}

void AttributionDataHostManagerImpl::NotifyFencedFrameReportingBeaconData(
    BeaconId beacon_id,
    GURL reporting_url,
    const net::HttpResponseHeaders* headers,
    bool is_final_response) {
  auto it = registrations_.find(beacon_id);
  // This should not happen if `NotifyFencedFrameReportingBeaconStarted()` is
  // previously called and the method isn't called twice with
  // `is_final_response` being true.
  if (it == registrations_.end()) {
    return;
  }

  CHECK(!it->registrations_complete());
  if (is_final_response) {
    it->CompleteRegistrations();
  }

  std::optional<SuitableOrigin> suitable_reporting_origin =
      SuitableOrigin::Create(reporting_url);
  if (!suitable_reporting_origin) {
    MaybeOnRegistrationsFinished(it);
    return;
  }

  auto pending_registration_data =
      PendingRegistrationData::Get(headers, *it, std::move(reporting_url),
                                   *std::move(suitable_reporting_origin));

  if (!pending_registration_data.has_value()) {
    MaybeOnRegistrationsFinished(it);
    return;
  }

  if (auto* rfh = RenderFrameHostImpl::FromID(it->render_frame_id())) {
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        rfh, blink::mojom::WebFeature::kAttributionFencedFrameReportingBeacon);
  }

  HandleRegistrationData(it, *std::move(pending_registration_data));
}

base::WeakPtr<AttributionDataHostManager>
AttributionDataHostManagerImpl::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
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
      // We set `due_to_timeout` false here even when the call to this method is
      // due to a timeout as this value refers to the navigation timeout as
      // opposed to the background registrations timeout (this method).
      MaybeDoneWithNavigation(navigation_id.value(), /*due_to_timeout=*/false);
    }
  }
}

base::expected<void, SourceRegistrationError>
AttributionDataHostManagerImpl::HandleParsedWebSource(
    const Registrations& registrations,
    HeaderPendingDecode& pending_decode,
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.has_value()) {
    return base::unexpected(SourceRegistrationError::kInvalidJson);
  }

  auto source_type = registrations.navigation_id().has_value()
                         ? SourceType::kNavigation
                         : SourceType::kEvent;

  ASSIGN_OR_RETURN(auto registration,
                   attribution_reporting::SourceRegistration::Parse(
                       *std::move(result), source_type));

  if (auto navigation_id = registrations.navigation_id();
      navigation_id.has_value() &&
      !AddNavigationSourceRegistrationToBatchMap(
          *navigation_id, pending_decode.reporting_origin, registration,
          registrations.render_frame_id(),
          registrations.devtools_request_id())) {
    return base::ok();
  }

  attribution_manager_->HandleSource(
      StorableSource(std::move(pending_decode.reporting_origin),
                     std::move(registration), registrations.context_origin(),
                     source_type, registrations.is_within_fenced_frame()),
      registrations.render_frame_id());

  return base::ok();
}

base::expected<void, TriggerRegistrationError>
AttributionDataHostManagerImpl::HandleParsedWebTrigger(
    const Registrations& registrations,
    HeaderPendingDecode& pending_decode,
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.has_value()) {
    return base::unexpected(TriggerRegistrationError::kInvalidJson);
  }

  ASSIGN_OR_RETURN(
      auto registration,
      attribution_reporting::TriggerRegistration::Parse(*std::move(result)));

  attribution_manager_->HandleTrigger(
      AttributionTrigger(std::move(pending_decode.reporting_origin),
                         std::move(registration),
                         /*destination_origin=*/registrations.context_origin(),
                         registrations.is_within_fenced_frame()),
      registrations.render_frame_id());

  return base::ok();
}

void AttributionDataHostManagerImpl::OnWebHeaderParsed(
    RegistrationsId id,
    data_decoder::DataDecoder::ValueOrError result) {
  auto registrations = registrations_.find(id);
  CHECK(registrations != registrations_.end());

  CHECK(!registrations->pending_web_decodes().empty());

  base::expected<void, RegistrationHeaderErrorDetails> handle_result;

  auto& pending_decode = registrations->pending_web_decodes().front();
  switch (pending_decode.registration_type) {
    case RegistrationType::kSource:
      handle_result = HandleParsedWebSource(*registrations, pending_decode,
                                            std::move(result));
      break;
    case RegistrationType::kTrigger:
      handle_result = HandleParsedWebTrigger(*registrations, pending_decode,
                                             std::move(result));
      break;
  }

  if (handle_result.has_value()) {
    RecordRegistrationMethod(registrations->context().GetRegistrationMethod(
        /*was_fetched_via_service_worker=*/false));
    RecordGoogleAmpViewerUsage(
        pending_decode.registration_type,
        registrations->context().is_context_google_amp_viewer());
  } else {
    MaybeLogAuditIssueAndReportHeaderError(
        *registrations, std::move(pending_decode), handle_result.error());
  }

  registrations->pending_web_decodes().pop_front();

  if (!registrations->pending_web_decodes().empty()) {
    HandleNextWebDecode(*registrations);
  } else {
    MaybeOnRegistrationsFinished(registrations);
  }
}

void AttributionDataHostManagerImpl::MaybeBufferOsRegistrations(
    int64_t navigation_id,
    std::vector<attribution_reporting::OsRegistrationItem> items,
    const RegistrationContext& registrations_context) {
  auto it = os_buffers_.find(navigation_id);
  if (it == os_buffers_.end()) {
    // This can only happen if the buffer was cleared due to a timeout. Given
    // the 20s timeout duration, we do not expect to receive registrations after
    // a timeout. The OS would also eventually block them as they would use a
    // duplicated input event. Yet, we still send the registrations to the OS
    // and let the OS handle the duplication.
    base::UmaHistogramCounts100(
        "Conversions.OsRegistrationsSkipBufferRegistrationsSize", items.size());

    SubmitOsRegistrations(std::move(items), registrations_context,
                          RegistrationType::kSource);

    return;
  }

  std::vector<attribution_reporting::OsRegistrationItem> not_buffered =
      it->Buffer(std::move(items), registrations_context);
  while (it->IsFull()) {
    MaybeFlushOsRegistrationsBuffer(
        navigation_id, OsRegistrationsBufferFlushReason::kBufferFull);
    not_buffered = it->Buffer(std::move(not_buffered), registrations_context);
  }
  CHECK(not_buffered.empty());
}

void AttributionDataHostManagerImpl::OnOsHeaderParsed(RegistrationsId id,
                                                      OsParseResult result) {
  auto registrations = registrations_.find(id);
  CHECK(registrations != registrations_.end());

  CHECK(!registrations->pending_os_decodes().empty());

  auto& pending_decode = registrations->pending_os_decodes().front();

  base::expected<std::vector<attribution_reporting::OsRegistrationItem>,
                 OsRegistrationError>
      registration_items(base::unexpect, OsRegistrationError::kInvalidList);
  if (result.has_value()) {
    registration_items =
        attribution_reporting::ParseOsSourceOrTriggerHeader(*result);
  }

  if (registration_items.has_value()) {
    RecordRegistrationMethod(registrations->context().GetRegistrationMethod(
        /*was_fetched_via_service_worker=*/false));
    RecordGoogleAmpViewerUsage(
        pending_decode.registration_type,
        registrations->context().is_context_google_amp_viewer());

    if (registrations->navigation_id().has_value()) {
      MaybeBufferOsRegistrations(*registrations->navigation_id(),
                                 *std::move(registration_items),
                                 registrations->context());
    } else {
      SubmitOsRegistrations(*std::move(registration_items),
                            registrations->context(),
                            pending_decode.registration_type);
    }
  } else {
    RegistrationHeaderErrorDetails error_details;
    switch (pending_decode.registration_type) {
      case RegistrationType::kSource:
        error_details = attribution_reporting::OsSourceRegistrationError(
            registration_items.error());
        break;
      case RegistrationType::kTrigger:
        error_details = attribution_reporting::OsTriggerRegistrationError(
            registration_items.error());
        break;
    }
    MaybeLogAuditIssueAndReportHeaderError(
        *registrations, std::move(pending_decode), error_details);
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
  CHECK(it != registrations_.end());
  if (it->has_pending_decodes() || !it->registrations_complete()) {
    return;
  }

  std::optional<int64_t> navigation_id = it->navigation_id();
  registrations_.erase(it);
  if (navigation_id.has_value()) {
    MaybeDoneWithNavigation(navigation_id.value(), /*due_to_timeout=*/false);
  }
}

void AttributionDataHostManagerImpl::MaybeStartNavigation(
    int64_t navigation_id) {
  if (auto [_, inserted] = deferred_receivers_.try_emplace(
          navigation_id, std::vector<DeferredReceiver>());
      !inserted) {
    // We already have deferred receivers linked to the navigation.
    return;
  }

  auto [_, buffer_inserted] = os_buffers_.emplace(navigation_id);
  CHECK(buffer_inserted);

  navigation_registrations_timer_.Start(
      base::BindOnce(&AttributionDataHostManagerImpl::MaybeDoneWithNavigation,
                     weak_factory_.GetWeakPtr(), navigation_id,
                     /*due_to_timeout=*/true));
}

void AttributionDataHostManagerImpl::MaybeDoneWithNavigation(
    int64_t navigation_id,
    bool due_to_timeout) {
  if (due_to_timeout) {
    ongoing_background_datahost_registrations_.erase(navigation_id);
  } else {
    // There still is a connected datahost tied to the navigation that can
    // receive sources.
    if (ongoing_background_datahost_registrations_.contains(navigation_id)) {
      return;
    }

    for (const auto& registration : registrations_) {
      // There still is a registration tied to the navigation which can receive
      // more headers or is processing them.
      if (registration.navigation_id() == navigation_id) {
        return;
      }
    }

    for (const auto& [_, waiting] :
         navigations_waiting_on_background_registrations_) {
      // More background registrations tied to the navigation are expected.
      if (waiting.navigation_id() == navigation_id) {
        return;
      }
    }
  }

  // All expected registrations tied to the navigation have been received and
  // processed.
  MaybeFlushOsRegistrationsBuffer(
      navigation_id, due_to_timeout
                         ? OsRegistrationsBufferFlushReason::kTimeout
                         : OsRegistrationsBufferFlushReason::kNavigationDone);
  ClearRegistrationsForNavigationBatch(navigation_id);
  MaybeBindDeferredReceivers(navigation_id, due_to_timeout);
  ClearRegistrationsDeferUntilNavigation(navigation_id);
}

bool AttributionDataHostManagerImpl::AddNavigationSourceRegistrationToBatchMap(
    int64_t navigation_id,
    const SuitableOrigin& reporting_origin,
    const attribution_reporting::SourceRegistration& reg,
    const GlobalRenderFrameHostId& render_frame_id,
    const std::optional<std::string>& devtools_request_id) {
  auto [it, _] = registrations_count_and_set_scopes_per_navigation_.try_emplace(
      navigation_id,
      base::flat_map<SuitableOrigin,
                     ScopesAndCountForReportingOriginPerNavigation>());

  auto [it_inner, inserted_inner] = it->second.try_emplace(
      reporting_origin, ScopesAndCountForReportingOriginPerNavigation());
  it_inner->second.count++;

  std::optional<base::Value::Dict> invalid_parameter;

  if (const auto& scopes_data = reg.attribution_scopes_data) {
    if (inserted_inner ||
        it_inner->second.scopes == scopes_data->attribution_scopes_set()) {
      RecordNavigationSourceScopesLimitOutcome(
          NavigationSourceScopesLimitOutcome::kScopesAllowed);
      if (inserted_inner) {
        it_inner->second.scopes = scopes_data->attribution_scopes_set();
      }
    } else {
      invalid_parameter = scopes_data->ToJson();
      RecordNavigationSourceScopesLimitOutcome(
          NavigationSourceScopesLimitOutcome::kScopesDropped);
    }
  } else if (inserted_inner || it_inner->second.scopes.scopes().empty()) {
    RecordNavigationSourceScopesLimitOutcome(
        NavigationSourceScopesLimitOutcome::kNoScopesAllowed);
  } else {
    invalid_parameter.emplace();
    RecordNavigationSourceScopesLimitOutcome(
        NavigationSourceScopesLimitOutcome::kNoScopesDropped);
  }

  if (invalid_parameter.has_value()) {
    MaybeLogAuditIssue(render_frame_id,
                       /*request_url=*/reporting_origin->GetURL(),
                       devtools_request_id,
                       SerializeAttributionJson(*invalid_parameter),
                       AttributionReportingIssueType::
                           kNavigationRegistrationUniqueScopeAlreadySet);
  }

  return !invalid_parameter.has_value();
}

void AttributionDataHostManagerImpl::ClearRegistrationsForNavigationBatch(
    int64_t navigation_id) {
  auto it =
      registrations_count_and_set_scopes_per_navigation_.find(navigation_id);
  if (it == registrations_count_and_set_scopes_per_navigation_.end()) {
    return;
  }

  for (const auto& [_, object] : it->second) {
    base::UmaHistogramExactLinear(
        "Conversions.NavigationSourceRegistrationsPerReportingOriginPerBatch",
        object.count, /*exclusive_max=*/50);
  }
  registrations_count_and_set_scopes_per_navigation_.erase(it);
}

void AttributionDataHostManagerImpl::MaybeBindDeferredReceivers(
    int64_t navigation_id,
    bool due_to_timeout) {
  auto it = deferred_receivers_.find(navigation_id);
  if (it == deferred_receivers_.end()) {
    return;
  }

  base::UmaHistogramBoolean("Conversions.DeferredDataHostProcessedAfterTimeout",
                            due_to_timeout);
  for (auto& deferred_receiver : it->second) {
    base::UmaHistogramMediumTimes(
        "Conversions.ProcessRegisterDataHostDelay",
        base::TimeTicks::Now() - deferred_receiver.initial_registration_time);
    receivers_.Add(this, std::move(deferred_receiver.data_host),
                   std::move(deferred_receiver.context));
  }
  deferred_receivers_.erase(it);
}

void AttributionDataHostManagerImpl::ClearRegistrationsDeferUntilNavigation(
    int64_t navigation_id) {
  if (!BackgroundRegistrationsEnabled()) {
    return;
  }
  for (auto it = registrations_.begin(); it != registrations_.end(); ++it) {
    if (it->defer_until_navigation() == navigation_id) {
      it->ClearDeferUntilNavigation();
      if (!it->pending_registration_data().empty()) {
        HandleNextRegistrationData(it);
      }
    }
  }
}

void AttributionDataHostManagerImpl::MaybeFlushOsRegistrationsBuffer(
    int64_t navigation_id,
    OsRegistrationsBufferFlushReason reason) {
  auto it = os_buffers_.find(navigation_id);
  if (it == os_buffers_.end()) {
    return;
  }

  if (!it->IsEmpty()) {
    base::UmaHistogramEnumeration(
        "Conversions.OsRegistrationsBufferFlushReason", reason);
    SubmitOsRegistrations(it->TakeRegistrationItems(), it->context(),
                          RegistrationType::kSource);
  }

  // If we flushed the buffer due to it being full, the navigation is still
  // active, we don't erase the buffer.
  if (reason != OsRegistrationsBufferFlushReason::kBufferFull) {
    os_buffers_.erase(it);
  }
}

void AttributionDataHostManagerImpl::SubmitOsRegistrations(
    std::vector<attribution_reporting::OsRegistrationItem> items,
    const RegistrationContext& registration_context,
    RegistrationType type) {
  std::optional<AttributionInputEvent> input_event;
  base::UmaHistogramCounts100("Conversions.OsRegistrationItemsPerBatch",
                              items.size());

  AttributionReportingOsRegistrar os_registrar;

  switch (type) {
    case RegistrationType::kSource:
      // The OsRegistration uses the optional to determine if it's a source or a
      // trigger. However, we want to send an actual input event only when the
      // registration is tied to a navigation.
      input_event.emplace(registration_context.navigation_id().has_value()
                              ? registration_context.last_input_event()
                              : AttributionInputEvent());
      os_registrar = registration_context.os_registrars().source_registrar;
      break;
    case RegistrationType::kTrigger:
      os_registrar = registration_context.os_registrars().trigger_registrar;
      break;
  }

  attribution_manager_->HandleOsRegistration(OsRegistration(
      std::move(items),
      /*top_level_origin=*/registration_context.context_origin(),
      std::move(input_event), registration_context.is_within_fenced_frame(),
      registration_context.render_frame_id(),
      ConvertToRegistrar(os_registrar)));
}

void AttributionDataHostManagerImpl::MaybeLogAuditIssueAndReportHeaderError(
    const Registrations& registrations,
    HeaderPendingDecode pending_decode,
    RegistrationHeaderErrorDetails error_details) {
  auto&& [header, reporting_origin, reporting_url, report_header_errors,
          registration_type] = std::move(pending_decode);

  AttributionReportingIssueType issue_type = absl::visit(
      base::Overloaded{
          [](SourceRegistrationError error) {
            attribution_reporting::RecordSourceRegistrationError(error);
            return AttributionReportingIssueType::kInvalidRegisterSourceHeader;
          },

          [](TriggerRegistrationError error) {
            attribution_reporting::RecordTriggerRegistrationError(error);
            return AttributionReportingIssueType::kInvalidRegisterTriggerHeader;
          },

          [](attribution_reporting::OsSourceRegistrationError) {
            return AttributionReportingIssueType::
                kInvalidRegisterOsSourceHeader;
          },

          [](attribution_reporting::OsTriggerRegistrationError) {
            return AttributionReportingIssueType::
                kInvalidRegisterOsTriggerHeader;
          },
      },
      error_details);

  registrations.MaybeLogIssue(reporting_url,
                              /*invalid_parameter=*/header, issue_type);

  if (report_header_errors) {
    attribution_manager_->ReportRegistrationHeaderError(
        std::move(reporting_origin),
        attribution_reporting::RegistrationHeaderError(std::move(header),
                                                       error_details),
        registrations.context_origin(), registrations.is_within_fenced_frame(),
        registrations.render_frame_id());
  }
}

void AttributionDataHostManagerImpl::ReportRegistrationHeaderError(
    SuitableOrigin reporting_origin,
    attribution_reporting::RegistrationHeaderError error) {
  const RegistrationContext& context = receivers_.current_context();
  attribution_manager_->ReportRegistrationHeaderError(
      std::move(reporting_origin), std::move(error), context.context_origin(),
      context.is_within_fenced_frame(), context.render_frame_id());
}

bool AttributionDataHostManagerImpl::RegistrationContext::CheckRegistrarSupport(
    Registrar registrar,
    RegistrationType registration_type) const {
  const bool is_source = registration_type == RegistrationType::kSource;
  AttributionReportingOsRegistrar os_registrar =
      is_source ? os_registrars().source_registrar
                : os_registrars().trigger_registrar;

  network::mojom::AttributionSupport attribution_support =
      AttributionManager::GetAttributionSupport(
          /*client_os_disabled=*/os_registrar ==
          AttributionReportingOsRegistrar::kDisabled);

  switch (registrar) {
    case Registrar::kWeb:
      return network::HasAttributionWebSupport(attribution_support);
    case Registrar::kOs:
      return network::HasAttributionOsSupport(attribution_support);
  }

  return false;
}

}  // namespace content
