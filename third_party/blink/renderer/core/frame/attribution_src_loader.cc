// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/attribution_src_loader.h"

#include <stdint.h>

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/overloaded.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/types/expected.h"
#include "base/unguessable_token.h"
#include "components/attribution_reporting/data_host.mojom-blink.h"
#include "components/attribution_reporting/eligibility.h"
#include "components/attribution_reporting/os_registration.h"
#include "components/attribution_reporting/os_registration_error.mojom-shared.h"
#include "components/attribution_reporting/registrar.h"
#include "components/attribution_reporting/registrar_info.h"
#include "components/attribution_reporting/registration_eligibility.mojom-shared.h"
#include "components/attribution_reporting/registration_header_error.h"
#include "components/attribution_reporting/registration_info.h"
#include "components/attribution_reporting/source_registration.h"
#include "components/attribution_reporting/source_registration_error.mojom-shared.h"
#include "components/attribution_reporting/source_type.mojom-shared.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "components/attribution_reporting/trigger_registration_error.mojom-shared.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "services/network/public/cpp/attribution_utils.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/attribution.mojom-forward.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/navigation/impression.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/conversions/conversions.mojom-blink.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom-blink.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/space_split_string.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_issue.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/self_keep_alive.h"
#include "third_party/blink/renderer/platform/loader/cors/cors.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_context.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_utils.h"
#include "third_party/blink/renderer/platform/loader/fetch/raw_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

using ::attribution_reporting::IssueType;
using ::attribution_reporting::mojom::RegistrationEligibility;
using ::attribution_reporting::mojom::SourceType;
using ::network::mojom::AttributionReportingEligibility;

using mojom::blink::AttributionReportingIssueType;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AttributionSrcRequestStatus {
  kRequested = 0,
  kReceived = 1,
  kFailed = 2,
  kMaxValue = kFailed,
};

void RecordAttributionSrcRequestStatus(AttributionSrcRequestStatus status) {
  base::UmaHistogramEnumeration("Conversions.AttributionSrcRequestStatus",
                                status);
}

void LogAuditIssue(ExecutionContext* execution_context,
                   AttributionReportingIssueType issue_type,
                   HTMLElement* element,
                   std::optional<uint64_t> request_id,
                   const String& invalid_parameter) {
  String id_string;
  if (request_id) {
    id_string = IdentifiersFactory::SubresourceRequestId(*request_id);
  }

  AuditsIssue::ReportAttributionIssue(execution_context, issue_type, element,
                                      id_string, invalid_parameter);
}

base::expected<attribution_reporting::RegistrationInfo,
               attribution_reporting::RegistrationInfoError>
GetRegistrationInfo(const HTTPHeaderMap& map,
                    ExecutionContext* execution_context,
                    uint64_t request_id,
                    bool cross_app_web_enabled) {
  AtomicString info_header = map.Get(http_names::kAttributionReportingInfo);
  if (info_header.IsNull()) {
    return attribution_reporting::RegistrationInfo();
  }
  auto parsed_registration_info =
      attribution_reporting::RegistrationInfo::ParseInfo(
          StringUTF8Adaptor(info_header).AsStringView(), cross_app_web_enabled);
  if (!parsed_registration_info.has_value()) {
    LogAuditIssue(execution_context,
                  AttributionReportingIssueType::kInvalidInfoHeader,
                  /*element=*/nullptr, request_id,
                  /*invalid_parameter=*/info_header);
  }
  return parsed_registration_info;
}

template <typename Container>
Vector<KURL> ParseAttributionSrcUrls(AttributionSrcLoader& loader,
                                     LocalDOMWindow* window,
                                     const Container& strings,
                                     HTMLElement* element) {
  CHECK(window);

  if (!network::HasAttributionSupport(loader.GetSupport())) {
    LogAuditIssue(window, AttributionReportingIssueType::kNoWebOrOsSupport,
                  element,
                  /*request_id=*/std::nullopt,
                  /*invalid_parameter=*/String());
    return {};
  }

  Vector<KURL> urls;
  urls.reserve(base::checked_cast<wtf_size_t>(strings.size()));

  // TODO(crbug.com/1434306): Extract URL-invariant checks to avoid redundant
  // operations and DevTools issues.
  for (wtf_size_t i = 0; i < strings.size(); i++) {
    KURL url = window->CompleteURL(strings[i]);
    if (loader.CanRegister(url, element, /*request_id=*/std::nullopt)) {
      urls.emplace_back(std::move(url));
    }
  }

  return urls;
}

bool KeepaliveResponsesHandledInBrowser() {
  return base::FeatureList::IsEnabled(
             blink::features::kKeepAliveInBrowserMigration) &&
         base::FeatureList::IsEnabled(
             blink::features::kAttributionReportingInBrowserMigration);
}

// Keepalive requests will be serviced by `KeepAliveAttributionRequestHelper`
// except for requests fetched via a service worker as keep alive is not
// supported in service workers, See https://crbug.com/1519958 for details.
// TODO(https://crbug.com/1523862): Once service worker keep alive requests are
// supported, remove the condition `WasFetchedViaServiceWorker` to prevent
// responses from being processed twice.
bool ResponseHandledInBrowser(const ResourceRequestHead& request,
                              const ResourceResponse& response) {
  return KeepaliveResponsesHandledInBrowser() && request.GetKeepalive() &&
         !response.WasFetchedViaServiceWorker();
}

}  // namespace

struct AttributionSrcLoader::AttributionHeaders {
  AtomicString web_source;
  AtomicString web_trigger;
  AtomicString os_source;
  AtomicString os_trigger;
  uint64_t request_id;

  AttributionHeaders(const HTTPHeaderMap& map,
                     uint64_t request_id,
                     bool cross_app_web_enabled)
      : web_source(map.Get(http_names::kAttributionReportingRegisterSource)),
        web_trigger(map.Get(http_names::kAttributionReportingRegisterTrigger)),
        request_id(request_id) {
    if (cross_app_web_enabled) {
      os_source = map.Get(http_names::kAttributionReportingRegisterOSSource);
      os_trigger = map.Get(http_names::kAttributionReportingRegisterOSTrigger);
    }
  }

  int source_count() const {
    return (web_source.IsNull() ? 0 : 1) + (os_source.IsNull() ? 0 : 1);
  }

  int trigger_count() const {
    return (web_trigger.IsNull() ? 0 : 1) + (os_trigger.IsNull() ? 0 : 1);
  }

  int count() const { return source_count() + trigger_count(); }

  void LogOsSourceIgnored(ExecutionContext* execution_context) const {
    DCHECK(!os_source.IsNull());
    LogAuditIssue(execution_context,
                  AttributionReportingIssueType::kOsSourceIgnored,
                  /*element=*/nullptr, request_id,
                  /*invalid_parameter=*/os_source);
  }

  void LogOsTriggerIgnored(ExecutionContext* execution_context) const {
    DCHECK(!os_trigger.IsNull());
    LogAuditIssue(execution_context,
                  AttributionReportingIssueType::kOsTriggerIgnored,
                  /*element=*/nullptr, request_id,
                  /*invalid_parameter=*/os_trigger);
  }

  void LogSourceIgnored(ExecutionContext* execution_context) const {
    DCHECK(!web_source.IsNull());
    LogAuditIssue(execution_context,
                  AttributionReportingIssueType::kSourceIgnored,
                  /*element=*/nullptr, request_id,
                  /*invalid_parameter=*/web_source);
  }

  void LogTriggerIgnored(ExecutionContext* execution_context) const {
    DCHECK(!web_trigger.IsNull());
    LogAuditIssue(execution_context,
                  AttributionReportingIssueType::kTriggerIgnored,
                  /*element=*/nullptr, request_id,
                  /*invalid_parameter=*/web_trigger);
  }

  void MaybeLogAllSourceHeadersIgnored(
      ExecutionContext* execution_context) const {
    if (!web_source.IsNull()) {
      LogSourceIgnored(execution_context);
    }

    if (!os_source.IsNull()) {
      LogOsSourceIgnored(execution_context);
    }
  }

  void MaybeLogAllTriggerHeadersIgnored(
      ExecutionContext* execution_context) const {
    if (!web_trigger.IsNull()) {
      LogTriggerIgnored(execution_context);
    }

    if (!os_trigger.IsNull()) {
      LogOsTriggerIgnored(execution_context);
    }
  }

  // `is_source` is true for source registrations, and false for trigger
  // registrations.
  void LogIssues(ExecutionContext* execution_context,
                 attribution_reporting::IssueTypes issues,
                 bool is_source) const {
    for (IssueType issue_type : issues) {
      switch (issue_type) {
        case IssueType::kWebAndOsHeaders:
          LogAuditIssue(execution_context,
                        AttributionReportingIssueType::kWebAndOsHeaders,
                        /*element=*/nullptr, request_id,
                        /*invalid_parameter=*/String());
          break;
        case IssueType::kWebIgnored:
          if (is_source) {
            LogSourceIgnored(execution_context);
          } else {
            LogTriggerIgnored(execution_context);
          }
          break;
        case IssueType::kOsIgnored:
          if (is_source) {
            LogOsSourceIgnored(execution_context);
          } else {
            LogOsTriggerIgnored(execution_context);
          }
          break;
        case IssueType::kNoWebHeader:
          LogAuditIssue(
              execution_context,
              is_source
                  ? AttributionReportingIssueType::kNoRegisterSourceHeader
                  : AttributionReportingIssueType::kNoRegisterTriggerHeader,
              /*element=*/nullptr, request_id,
              /*invalid_parameter=*/String());
          break;
        case IssueType::kNoOsHeader:
          LogAuditIssue(
              execution_context,
              is_source
                  ? AttributionReportingIssueType::kNoRegisterOsSourceHeader
                  : AttributionReportingIssueType::kNoRegisterOsTriggerHeader,
              /*element=*/nullptr, request_id,
              /*invalid_parameter=*/String());
          break;
      }
    }
  }
};

class AttributionSrcLoader::ResourceClient
    : public GarbageCollected<AttributionSrcLoader::ResourceClient>,
      public RawResourceClient {
 public:
  ResourceClient(
      AttributionSrcLoader* loader,
      RegistrationEligibility eligibility,
      SourceType source_type,
      mojo::SharedRemote<attribution_reporting::mojom::blink::DataHost>
          data_host,
      network::mojom::AttributionSupport support)
      : loader_(loader),
        eligibility_(eligibility),
        source_type_(source_type),
        data_host_(std::move(data_host)),
        support_(support) {
    DCHECK(loader_);
    DCHECK(loader_->local_frame_);
    DCHECK(loader_->local_frame_->IsAttached());
    CHECK(data_host_.is_bound());
    CHECK_NE(support_, network::mojom::AttributionSupport::kUnset);
  }

  ~ResourceClient() override = default;

  ResourceClient(const ResourceClient&) = delete;
  ResourceClient(ResourceClient&&) = delete;

  ResourceClient& operator=(const ResourceClient&) = delete;
  ResourceClient& operator=(ResourceClient&&) = delete;

  void Trace(Visitor* visitor) const override {
    visitor->Trace(loader_);
    RawResourceClient::Trace(visitor);
  }

  void HandleResponseHeaders(
      attribution_reporting::SuitableOrigin reporting_origin,
      const AttributionHeaders&,
      const attribution_reporting::RegistrationInfo&,
      bool was_fetched_via_service_worker);

  void Finish();

 private:
  void HandleResponseHeaders(Resource* resource,
                             const ResourceResponse& response,
                             uint64_t request_id);

  void HandleSourceRegistration(
      const AttributionHeaders&,
      attribution_reporting::SuitableOrigin reporting_origin,
      const attribution_reporting::RegistrationInfo&,
      bool was_fetched_via_service_worker);

  void HandleTriggerRegistration(
      const AttributionHeaders&,
      attribution_reporting::SuitableOrigin reporting_origin,
      const attribution_reporting::RegistrationInfo&,
      bool was_fetched_via_service_worker);

  void LogAuditIssueAndMaybeReportHeaderError(
      const AttributionHeaders&,
      bool report_header_errors,
      attribution_reporting::RegistrationHeaderErrorDetails,
      attribution_reporting::SuitableOrigin reporting_origin);

  // RawResourceClient:
  String DebugName() const override;
  void ResponseReceived(Resource* resource,
                        const ResourceResponse& response) override;
  bool RedirectReceived(Resource* resource,
                        const ResourceRequest& request,
                        const ResourceResponse& response) override;
  void NotifyFinished(Resource* resource) override;

  const Member<AttributionSrcLoader> loader_;

  // Type of events this request can register.
  const RegistrationEligibility eligibility_;

  // Used to parse source registrations associated with this resource client.
  // Irrelevant for trigger registrations.
  const SourceType source_type_;

  // Remote used for registering responses with the browser-process.
  // Note that there's no check applied for `SharedRemote`, and it should be
  // memory safe as long as `SharedRemote::set_disconnect_handler` is not
  // installed. See https://crbug.com/1512895 for details.
  mojo::SharedRemote<attribution_reporting::mojom::blink::DataHost> data_host_;

  wtf_size_t num_registrations_ = 0;

  const network::mojom::AttributionSupport support_;

  SelfKeepAlive<ResourceClient> keep_alive_{this};
};

AttributionSrcLoader::AttributionSrcLoader(LocalFrame* frame)
    : local_frame_(frame) {
  DCHECK(local_frame_);
}

AttributionSrcLoader::~AttributionSrcLoader() = default;

void AttributionSrcLoader::Trace(Visitor* visitor) const {
  visitor->Trace(local_frame_);
}

void AttributionSrcLoader::RecordAttributionFeatureAllowed(bool enabled) {
  base::UmaHistogramBoolean("Conversions.AllowedByPermissionPolicy", enabled);
}

Vector<KURL> AttributionSrcLoader::ParseAttributionSrc(
    const AtomicString& attribution_src,
    HTMLElement* element) {
  CHECK(local_frame_);
  return ParseAttributionSrcUrls(*this, local_frame_->DomWindow(),
                                 SpaceSplitString(attribution_src), element);
}

void AttributionSrcLoader::Register(
    const AtomicString& attribution_src,
    HTMLElement* element,
    network::mojom::ReferrerPolicy referrer_policy) {
  CreateAndSendRequests(ParseAttributionSrc(attribution_src, element),
                        /*attribution_src_token=*/std::nullopt,
                        referrer_policy);
}

std::optional<Impression> AttributionSrcLoader::RegisterNavigationInternal(
    const KURL& navigation_url,
    Vector<KURL> attribution_src_urls,
    HTMLAnchorElementBase* element,
    bool has_transient_user_activation,
    network::mojom::ReferrerPolicy referrer_policy) {
  if (!has_transient_user_activation) {
    LogAuditIssue(local_frame_->DomWindow(),
                  AttributionReportingIssueType::
                      kNavigationRegistrationWithoutTransientUserActivation,
                  element,
                  /*request_id=*/std::nullopt,
                  /*invalid_parameter=*/String());
    return std::nullopt;
  }

  // TODO(apaseltiner): Add tests to ensure that this method can't be used to
  // register triggers.

  // TODO(crbug.com/1434306): Extract URL-invariant checks to avoid redundant
  // operations and DevTools issues.

  const Impression impression;

  if (CreateAndSendRequests(std::move(attribution_src_urls),
                            impression.attribution_src_token,
                            referrer_policy)) {
    return impression;
  }

  if (CanRegister(navigation_url, element, /*request_id=*/std::nullopt)) {
    return impression;
  }

  return std::nullopt;
}

std::optional<Impression> AttributionSrcLoader::RegisterNavigation(
    const KURL& navigation_url,
    const AtomicString& attribution_src,
    HTMLAnchorElementBase* element,
    bool has_transient_user_activation,
    network::mojom::ReferrerPolicy referrer_policy) {
  CHECK(!attribution_src.IsNull());
  CHECK(element);

  return RegisterNavigationInternal(
      navigation_url, ParseAttributionSrc(attribution_src, element), element,
      has_transient_user_activation, referrer_policy);
}

std::optional<Impression> AttributionSrcLoader::RegisterNavigation(
    const KURL& navigation_url,
    const WebVector<WebString>& attribution_srcs,
    bool has_transient_user_activation,
    network::mojom::ReferrerPolicy referrer_policy) {
  CHECK(local_frame_);
  return RegisterNavigationInternal(
      navigation_url,
      ParseAttributionSrcUrls(*this, local_frame_->DomWindow(),
                              attribution_srcs,
                              /*element=*/nullptr),
      /*element=*/nullptr, has_transient_user_activation, referrer_policy);
}

bool AttributionSrcLoader::CreateAndSendRequests(
    Vector<KURL> urls,
    std::optional<AttributionSrcToken> attribution_src_token,
    network::mojom::ReferrerPolicy referrer_policy) {
  // Detached frames cannot/should not register new attributionsrcs.
  if (!local_frame_->IsAttached() || urls.empty()) {
    return false;
  }

  if (Document* document = local_frame_->DomWindow()->document();
      document->IsPrerendering()) {
    document->AddPostPrerenderingActivationStep(
        WTF::BindOnce(base::IgnoreResult(&AttributionSrcLoader::DoRegistration),
                      WrapPersistentIfNeeded(this), std::move(urls),
                      attribution_src_token, referrer_policy));
    return false;
  }

  return DoRegistration(urls, attribution_src_token, referrer_policy);
}

bool AttributionSrcLoader::DoRegistration(
    const Vector<KURL>& urls,
    const std::optional<AttributionSrcToken> attribution_src_token,
    network::mojom::ReferrerPolicy referrer_policy) {
  DCHECK(!urls.empty());

  if (!local_frame_->IsAttached()) {
    return false;
  }

  const auto eligibility = attribution_src_token.has_value()
                               ? RegistrationEligibility::kSource
                               : RegistrationEligibility::kSourceOrTrigger;

  mojo::AssociatedRemote<mojom::blink::AttributionHost> conversion_host;
  local_frame_->GetRemoteNavigationAssociatedInterfaces()->GetInterface(
      &conversion_host);

  mojo::SharedRemote<attribution_reporting::mojom::blink::DataHost> data_host;

  if (KeepaliveResponsesHandledInBrowser() &&
      attribution_src_token.has_value()) {
    conversion_host->NotifyNavigationWithBackgroundRegistrationsWillStart(
        *attribution_src_token,
        /*expected_registrations=*/urls.size());
  }

  SourceType source_type;
  if (attribution_src_token.has_value()) {
    conversion_host->RegisterNavigationDataHost(
        data_host.BindNewPipeAndPassReceiver(), *attribution_src_token);
    source_type = SourceType::kNavigation;
  } else {
    conversion_host->RegisterDataHost(data_host.BindNewPipeAndPassReceiver(),
                                      eligibility,
                                      /*is_for_background_requests=*/true);
    source_type = SourceType::kEvent;
  }

  for (const KURL& url : urls) {
    ResourceRequest request(url);
    request.SetHttpMethod(http_names::kGET);

    request.SetKeepalive(true);
    request.SetRequestContext(
        mojom::blink::RequestContextType::ATTRIBUTION_SRC);
    request.SetReferrerPolicy(referrer_policy);

    request.SetAttributionReportingEligibility(
        attribution_src_token.has_value()
            ? AttributionReportingEligibility::kNavigationSource
            : AttributionReportingEligibility::kEventSourceOrTrigger);
    if (attribution_src_token.has_value()) {
      base::UnguessableToken token = attribution_src_token->value();
      request.SetAttributionReportingSrcToken(token);
    }

    FetchParameters params(
        std::move(request),
        ResourceLoaderOptions(local_frame_->DomWindow()->GetCurrentWorld()));
    params.MutableOptions().initiator_info.name =
        fetch_initiator_type_names::kAttributionsrc;

    FetchUtils::LogFetchKeepAliveRequestMetric(
        params.GetResourceRequest().GetRequestContext(),
        FetchUtils::FetchKeepAliveRequestState::kTotal);
    RawResource::Fetch(
        params, local_frame_->DomWindow()->Fetcher(),
        MakeGarbageCollected<ResourceClient>(this, eligibility, source_type,
                                             data_host, GetSupport()));

    RecordAttributionSrcRequestStatus(AttributionSrcRequestStatus::kRequested);
  }

  return true;
}

std::optional<attribution_reporting::SuitableOrigin>
AttributionSrcLoader::ReportingOriginForUrlIfValid(
    const KURL& url,
    HTMLElement* element,
    std::optional<uint64_t> request_id,
    bool log_issues) {
  LocalDOMWindow* window = local_frame_->DomWindow();
  DCHECK(window);

  auto maybe_log_audit_issue = [&](AttributionReportingIssueType issue_type,
                                   const SecurityOrigin* invalid_origin =
                                       nullptr) {
    if (!log_issues) {
      return;
    }

    LogAuditIssue(window, issue_type, element, request_id,
                  /*invalid_parameter=*/
                  invalid_origin ? invalid_origin->ToString() : String());
  };

  if (!RuntimeEnabledFeatures::AttributionReportingEnabled(window) &&
      !RuntimeEnabledFeatures::AttributionReportingCrossAppWebEnabled(window)) {
    return std::nullopt;
  }

  bool enabled = window->IsFeatureEnabled(
      mojom::blink::PermissionsPolicyFeature::kAttributionReporting);
  RecordAttributionFeatureAllowed(enabled);
  if (!enabled) {
    maybe_log_audit_issue(
        AttributionReportingIssueType::kPermissionPolicyDisabled);
    return std::nullopt;
  }

  if (!window->IsSecureContext()) {
    maybe_log_audit_issue(AttributionReportingIssueType::kInsecureContext,
                          window->GetSecurityContext().GetSecurityOrigin());
    return std::nullopt;
  }

  scoped_refptr<const SecurityOrigin> security_origin =
      SecurityOrigin::Create(url);

  std::optional<attribution_reporting::SuitableOrigin> reporting_origin =
      attribution_reporting::SuitableOrigin::Create(
          security_origin->ToUrlOrigin());

  if (!url.ProtocolIsInHTTPFamily() || !reporting_origin) {
    maybe_log_audit_issue(
        AttributionReportingIssueType::kUntrustworthyReportingOrigin,
        security_origin.get());
    return std::nullopt;
  }

  UseCounter::Count(window,
                    mojom::blink::WebFeature::kAttributionReportingAPIAll);

  UseCounter::Count(window, mojom::blink::WebFeature::kPrivacySandboxAdsAPIs);

  // The Attribution-Reporting-Support header is set on the request in the
  // network service and the context is unavailable. This is an approximate
  // proxy to when the header is set, and aligned with the counter for regular
  // Attribution Reporting API that sets the Attribution-Reporting-Eligible
  // header on the request.
  if (RuntimeEnabledFeatures::AttributionReportingCrossAppWebEnabled(window) &&
      base::FeatureList::IsEnabled(
          network::features::kAttributionReportingCrossAppWeb)) {
    UseCounter::Count(window,
                      mojom::blink::WebFeature::
                          kAttributionReportingCrossAppWebSupportHeader);
  }

  return reporting_origin;
}

bool AttributionSrcLoader::CanRegister(const KURL& url,
                                       HTMLElement* element,
                                       std::optional<uint64_t> request_id,
                                       bool log_issues) {
  return !!ReportingOriginForUrlIfValid(url, element, request_id, log_issues);
}

network::mojom::AttributionSupport AttributionSrcLoader::GetSupport() const {
  auto* page = local_frame_->GetPage();
  CHECK(page);
  return page->GetAttributionSupport();
}

bool AttributionSrcLoader::MaybeRegisterAttributionHeaders(
    const ResourceRequest& request,
    const ResourceResponse& response) {
  if (response.IsNull()) {
    return false;
  }

  // Attributionsrc requests will be serviced by the
  // `AttributionSrcLoader::ResourceClient`.
  if (request.GetRequestContext() ==
      mojom::blink::RequestContextType::ATTRIBUTION_SRC) {
    return false;
  }

  if (ResponseHandledInBrowser(request, response)) {
    return false;
  }

  const uint64_t request_id = request.InspectorId();
  const bool cross_app_web_enabled =
      RuntimeEnabledFeatures::AttributionReportingCrossAppWebEnabled(
          local_frame_->DomWindow()) &&
      base::FeatureList::IsEnabled(
          network::features::kAttributionReportingCrossAppWeb);

  AttributionHeaders headers(response.HttpHeaderFields(), request_id,
                             cross_app_web_enabled);

  // Only handle requests which are attempting to invoke the API.
  if (headers.count() == 0) {
    return false;
  }

  std::optional<attribution_reporting::SuitableOrigin> reporting_origin =
      ReportingOriginForUrlIfValid(response.ResponseUrl(),
                                   /*element=*/nullptr, request_id);
  if (!reporting_origin) {
    return false;
  }

  // Navigation sources are only processed on navigations, which are handled
  // by the browser, or on background attributionsrc requests on
  // navigations, which are handled by `ResourceClient`, so this branch
  // shouldn't be reachable in practice.
  CHECK_NE(request.GetAttributionReportingEligibility(),
           AttributionReportingEligibility::kNavigationSource);

  std::optional<RegistrationEligibility> registration_eligibility =
      attribution_reporting::GetRegistrationEligibility(
          request.GetAttributionReportingEligibility());
  if (!registration_eligibility.has_value()) {
    headers.MaybeLogAllSourceHeadersIgnored(local_frame_->DomWindow());
    headers.MaybeLogAllTriggerHeadersIgnored(local_frame_->DomWindow());
    return false;
  }

  network::mojom::AttributionSupport support =
      request.GetAttributionReportingSupport();

  // This could occur for responses loaded from memory cache.
  if (support == network::mojom::AttributionSupport::kUnset) {
    // `ResourceFetcher::DidLoadResourceFromMemoryCache()` early returns for
    // detached frames. We log metrics here to verify that this is never hit in
    // detached frames.
    const bool is_detached = !local_frame_->IsAttached();
    base::UmaHistogramBoolean(
        "Conversions.NonAttributionSrcRequestUnsetSupport.Detached",
        is_detached);

    if (is_detached) {
      // Attribution support is unknown from detached frames, therefore not
      // registering the response.
      return false;
    }

    support = GetSupport();
  }

  auto registration_info = GetRegistrationInfo(
      response.HttpHeaderFields(), local_frame_->DomWindow(), request_id,
      cross_app_web_enabled);
  if (!registration_info.has_value()) {
    return false;
  }

  if (Document* document = local_frame_->DomWindow()->document();
      document->IsPrerendering()) {
    document->AddPostPrerenderingActivationStep(WTF::BindOnce(
        &AttributionSrcLoader::RegisterAttributionHeaders,
        WrapPersistentIfNeeded(this), *registration_eligibility, support,
        *std::move(reporting_origin), std::move(headers), *registration_info,
        response.WasFetchedViaServiceWorker()));
  } else {
    RegisterAttributionHeaders(
        *registration_eligibility, support, *std::move(reporting_origin),
        headers, *registration_info, response.WasFetchedViaServiceWorker());
  }

  return true;
}

void AttributionSrcLoader::RegisterAttributionHeaders(
    RegistrationEligibility registration_eligibility,
    network::mojom::AttributionSupport support,
    attribution_reporting::SuitableOrigin reporting_origin,
    const AttributionHeaders& headers,
    const attribution_reporting::RegistrationInfo& registration_info,
    bool was_fetched_via_service_worker) {
  mojo::AssociatedRemote<mojom::blink::AttributionHost> conversion_host;
  local_frame_->GetRemoteNavigationAssociatedInterfaces()->GetInterface(
      &conversion_host);

  mojo::SharedRemote<attribution_reporting::mojom::blink::DataHost> data_host;
  conversion_host->RegisterDataHost(data_host.BindNewPipeAndPassReceiver(),
                                    registration_eligibility,
                                    /*is_for_background_requests=*/false);

  // Create a client to mimic processing of attributionsrc requests. Note we do
  // not share `DataHosts` for redirects chains.
  // TODO(johnidel): Consider refactoring this such that we can share clients
  // for redirect chain, or not create the client at all.
  auto* client = MakeGarbageCollected<ResourceClient>(
      this, registration_eligibility, SourceType::kEvent, std::move(data_host),
      support);
  client->HandleResponseHeaders(std::move(reporting_origin), headers,
                                registration_info,
                                was_fetched_via_service_worker);
  client->Finish();
}

String AttributionSrcLoader::ResourceClient::DebugName() const {
  return "AttributionSrcLoader::ResourceClient";
}

void AttributionSrcLoader::ResourceClient::ResponseReceived(
    Resource* resource,
    const ResourceResponse& response) {
  HandleResponseHeaders(resource, response, resource->InspectorId());
}

bool AttributionSrcLoader::ResourceClient::RedirectReceived(
    Resource* resource,
    const ResourceRequest& request,
    const ResourceResponse& response) {
  HandleResponseHeaders(resource, response, request.InspectorId());
  return true;
}

void AttributionSrcLoader::ResourceClient::NotifyFinished(Resource* resource) {
  ClearResource();

  if (resource->ErrorOccurred()) {
    RecordAttributionSrcRequestStatus(AttributionSrcRequestStatus::kFailed);
  } else {
    RecordAttributionSrcRequestStatus(AttributionSrcRequestStatus::kReceived);
  }

  Finish();
}

void AttributionSrcLoader::ResourceClient::Finish() {
  DCHECK(data_host_.is_bound());
  DCHECK(keep_alive_);

  // Eagerly reset the data host so that the receiver is closed and any buffered
  // triggers are flushed as soon as possible. See crbug.com/1336797 for
  // details.
  data_host_.reset();

  keep_alive_.Clear();

  if (num_registrations_ > 0) {
    // 1 more than `net::URLRequest::kMaxRedirects`.
    base::UmaHistogramExactLinear("Conversions.RegistrationsPerRedirectChain",
                                  num_registrations_, 21);
  }
}

void AttributionSrcLoader::ResourceClient::HandleResponseHeaders(
    Resource* resource,
    const ResourceResponse& response,
    uint64_t request_id) {
  const bool cross_app_web_enabled =
      RuntimeEnabledFeatures::AttributionReportingCrossAppWebEnabled(
          loader_->local_frame_->DomWindow()) &&
      base::FeatureList::IsEnabled(
          network::features::kAttributionReportingCrossAppWeb);
  AttributionHeaders headers(response.HttpHeaderFields(), request_id,
                             cross_app_web_enabled);
  const bool has_header = headers.count() > 0;
  base::UmaHistogramBoolean(
      "Conversions.HasAttributionHeaderInAttributionSrcResponse", has_header);

  if (!has_header) {
    return;
  }

  if (ResponseHandledInBrowser(resource->GetResourceRequest(), response)) {
    return;
  }

  std::optional<attribution_reporting::SuitableOrigin> reporting_origin =
      loader_->ReportingOriginForUrlIfValid(response.ResponseUrl(),
                                            /*element=*/nullptr, request_id);
  if (!reporting_origin) {
    return;
  }

  auto registration_info = GetRegistrationInfo(
      response.HttpHeaderFields(), loader_->local_frame_->DomWindow(),
      request_id, cross_app_web_enabled);
  if (!registration_info.has_value()) {
    return;
  }

  HandleResponseHeaders(*std::move(reporting_origin), headers,
                        *registration_info,
                        response.WasFetchedViaServiceWorker());
}

void AttributionSrcLoader::ResourceClient::HandleResponseHeaders(
    attribution_reporting::SuitableOrigin reporting_origin,
    const AttributionHeaders& headers,
    const attribution_reporting::RegistrationInfo& registration_info,
    bool was_fetched_via_service_worker) {
  DCHECK_GT(headers.count(), 0);

  switch (eligibility_) {
    case RegistrationEligibility::kSource:
      HandleSourceRegistration(headers, std::move(reporting_origin),
                               registration_info,
                               was_fetched_via_service_worker);
      break;
    case RegistrationEligibility::kTrigger:
      HandleTriggerRegistration(headers, std::move(reporting_origin),
                                registration_info,
                                was_fetched_via_service_worker);
      break;
    case RegistrationEligibility::kSourceOrTrigger: {
      const bool has_source = headers.source_count() > 0;
      const bool has_trigger = headers.trigger_count() > 0;

      if (has_source && has_trigger) {
        LogAuditIssue(loader_->local_frame_->DomWindow(),
                      AttributionReportingIssueType::kSourceAndTriggerHeaders,
                      /*element=*/nullptr, headers.request_id,
                      /*invalid_parameter=*/String());
        return;
      }

      if (has_source) {
        HandleSourceRegistration(headers, std::move(reporting_origin),
                                 registration_info,
                                 was_fetched_via_service_worker);
        break;
      }

      DCHECK(has_trigger);
      HandleTriggerRegistration(headers, std::move(reporting_origin),
                                registration_info,
                                was_fetched_via_service_worker);
      break;
    }
  }
}

void AttributionSrcLoader::ResourceClient::HandleSourceRegistration(
    const AttributionHeaders& headers,
    attribution_reporting::SuitableOrigin reporting_origin,
    const attribution_reporting::RegistrationInfo& registration_info,
    bool was_fetched_via_service_worker) {
  DCHECK_NE(eligibility_, RegistrationEligibility::kTrigger);

  headers.MaybeLogAllTriggerHeadersIgnored(loader_->local_frame_->DomWindow());

  const bool is_source = true;

  auto registrar_info = attribution_reporting::RegistrarInfo::Get(
      !headers.web_source.IsNull(), !headers.os_source.IsNull(), is_source,
      registration_info.preferred_platform, support_);

  headers.LogIssues(loader_->local_frame_->DomWindow(), registrar_info.issues,
                    is_source);

  if (!registrar_info.registrar.has_value()) {
    return;
  }

  switch (registrar_info.registrar.value()) {
    case attribution_reporting::Registrar::kWeb: {
      CHECK(!headers.web_source.IsNull());
      base::UmaHistogramCounts1M("Conversions.HeadersSize.RegisterSource",
                                 headers.web_source.length());
      auto source_data = attribution_reporting::SourceRegistration::Parse(
          StringUTF8Adaptor(headers.web_source).AsStringView(), source_type_);
      if (!source_data.has_value()) {
        LogAuditIssueAndMaybeReportHeaderError(
            headers, registration_info.report_header_errors,
            source_data.error(), std::move(reporting_origin));
        return;
      }

      // LINT.IfChange(DataAvailableCallSource)
      base::UmaHistogramEnumeration(
          "Conversions.DataAvailableCall.Source",
          attribution_reporting::mojom::blink::DataAvailableCallsite::kBlink);
      // LINT.ThenChange(//content/browser/attribution_reporting/attribution_data_host_manager_impl.cc:DataAvailableCallSource)
      data_host_->SourceDataAvailable(std::move(reporting_origin),
                                      *std::move(source_data),
                                      was_fetched_via_service_worker);
      ++num_registrations_;
      break;
    }
    case attribution_reporting::Registrar::kOs: {
      CHECK(!headers.os_source.IsNull());
      // Max header size is 256 KB, use 1M count to encapsulate.
      base::UmaHistogramCounts1M("Conversions.HeadersSize.RegisterOsSource",
                                 headers.os_source.length());

      UseCounter::Count(
          loader_->local_frame_->DomWindow(),
          mojom::blink::WebFeature::kAttributionReportingCrossAppWeb);

      auto registration_items =
          attribution_reporting::ParseOsSourceOrTriggerHeader(
              StringUTF8Adaptor(headers.os_source).AsStringView());
      if (!registration_items.has_value()) {
        LogAuditIssueAndMaybeReportHeaderError(
            headers, registration_info.report_header_errors,
            attribution_reporting::OsSourceRegistrationError(
                registration_items.error()),
            std::move(reporting_origin));
        return;
      }

      // LINT.IfChange(DataAvailableCallOsSource)
      base::UmaHistogramEnumeration(
          "Conversions.DataAvailableCall.OsSource",
          attribution_reporting::mojom::blink::DataAvailableCallsite::kBlink);
      // LINT.ThenChange(//content/browser/attribution_reporting/attribution_data_host_manager_impl.cc:DataAvailableCallOsSource)

      data_host_->OsSourceDataAvailable(std::move(reporting_origin),
                                        *std::move(registration_items),
                                        was_fetched_via_service_worker);
      ++num_registrations_;
    }
  }
}

void AttributionSrcLoader::ResourceClient::HandleTriggerRegistration(
    const AttributionHeaders& headers,
    attribution_reporting::SuitableOrigin reporting_origin,
    const attribution_reporting::RegistrationInfo& registration_info,
    bool was_fetched_via_service_worker) {
  DCHECK_NE(eligibility_, RegistrationEligibility::kSource);

  headers.MaybeLogAllSourceHeadersIgnored(loader_->local_frame_->DomWindow());

  const bool is_source = false;

  auto registrar_info = attribution_reporting::RegistrarInfo::Get(
      !headers.web_trigger.IsNull(), !headers.os_trigger.IsNull(), is_source,
      registration_info.preferred_platform, support_);

  headers.LogIssues(loader_->local_frame_->DomWindow(), registrar_info.issues,
                    is_source);

  if (!registrar_info.registrar.has_value()) {
    return;
  }

  switch (registrar_info.registrar.value()) {
    case attribution_reporting::Registrar::kWeb: {
      CHECK(!headers.web_trigger.IsNull());
      // Max header size is 256 KB, use 1M count to encapsulate.
      base::UmaHistogramCounts1M("Conversions.HeadersSize.RegisterTrigger",
                                 headers.web_trigger.length());

      auto trigger_data = attribution_reporting::TriggerRegistration::Parse(
          StringUTF8Adaptor(headers.web_trigger).AsStringView());
      if (!trigger_data.has_value()) {
        LogAuditIssueAndMaybeReportHeaderError(
            headers, registration_info.report_header_errors,
            trigger_data.error(), std::move(reporting_origin));
        return;
      }

      // LINT.IfChange(DataAvailableCallTrigger)
      base::UmaHistogramEnumeration(
          "Conversions.DataAvailableCall.Trigger",
          attribution_reporting::mojom::blink::DataAvailableCallsite::kBlink);
      // LINT.ThenChange(//content/browser/attribution_reporting/attribution_data_host_manager_impl.cc:DataAvailableCallTrigger)
      data_host_->TriggerDataAvailable(std::move(reporting_origin),
                                       *std::move(trigger_data),
                                       was_fetched_via_service_worker);
      ++num_registrations_;
      break;
    }
    case attribution_reporting::Registrar::kOs: {
      CHECK(!headers.os_trigger.IsNull());
      // Max header size is 256 KB, use 1M count to encapsulate.
      base::UmaHistogramCounts1M("Conversions.HeadersSize.RegisterOsTrigger",
                                 headers.os_trigger.length());

      UseCounter::Count(
          loader_->local_frame_->DomWindow(),
          mojom::blink::WebFeature::kAttributionReportingCrossAppWeb);

      auto registration_items =
          attribution_reporting::ParseOsSourceOrTriggerHeader(
              StringUTF8Adaptor(headers.os_trigger).AsStringView());
      if (!registration_items.has_value()) {
        LogAuditIssueAndMaybeReportHeaderError(
            headers, registration_info.report_header_errors,
            attribution_reporting::OsTriggerRegistrationError(
                registration_items.error()),
            std::move(reporting_origin));
        return;
      }
      // LINT.IfChange(DataAvailableCallOsTrigger)
      base::UmaHistogramEnumeration(
          "Conversions.DataAvailableCall.OsTrigger",
          attribution_reporting::mojom::blink::DataAvailableCallsite::kBlink);
      // LINT.ThenChange(//content/browser/attribution_reporting/attribution_data_host_manager_impl.cc:DataAvailableCallOsTrigger)
      data_host_->OsTriggerDataAvailable(std::move(reporting_origin),
                                         *std::move(registration_items),
                                         was_fetched_via_service_worker);
      ++num_registrations_;
      break;
    }
  }
}

void AttributionSrcLoader::ResourceClient::
    LogAuditIssueAndMaybeReportHeaderError(
        const AttributionHeaders& headers,
        bool report_header_errors,
        attribution_reporting::RegistrationHeaderErrorDetails error_details,
        attribution_reporting::SuitableOrigin reporting_origin) {
  AtomicString header;

  AttributionReportingIssueType issue_type = absl::visit(
      base::Overloaded{
          [&](attribution_reporting::mojom::SourceRegistrationError) {
            header = headers.web_source;
            return AttributionReportingIssueType::kInvalidRegisterSourceHeader;
          },

          [&](attribution_reporting::mojom::TriggerRegistrationError) {
            header = headers.web_trigger;
            return AttributionReportingIssueType::kInvalidRegisterTriggerHeader;
          },

          [&](attribution_reporting::OsSourceRegistrationError) {
            header = headers.os_source;
            return AttributionReportingIssueType::
                kInvalidRegisterOsSourceHeader;
          },

          [&](attribution_reporting::OsTriggerRegistrationError) {
            header = headers.os_trigger;
            return AttributionReportingIssueType::
                kInvalidRegisterOsTriggerHeader;
          },
      },
      error_details);

  CHECK(!header.IsNull());
  LogAuditIssue(loader_->local_frame_->DomWindow(), issue_type,
                /*element=*/nullptr, headers.request_id,
                /*invalid_parameter=*/header);
  if (report_header_errors) {
    data_host_->ReportRegistrationHeaderError(
        std::move(reporting_origin),
        attribution_reporting::RegistrationHeaderError(
            StringUTF8Adaptor(header).AsStringView(), error_details));
  }
}

}  // namespace blink
