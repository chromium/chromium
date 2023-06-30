// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/attribution_src_loader.h"

#include <stdint.h>

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/types/expected.h"
#include "components/attribution_reporting/os_registration.h"
#include "components/attribution_reporting/registration_type.mojom-shared.h"
#include "components/attribution_reporting/source_registration.h"
#include "components/attribution_reporting/source_registration_error.mojom-shared.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "components/attribution_reporting/trigger_registration_error.mojom-shared.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "services/network/public/cpp/attribution_reporting_runtime_features.h"
#include "services/network/public/cpp/attribution_utils.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/trigger_verification.h"
#include "services/network/public/mojom/attribution.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/navigation/impression.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom-blink.h"
#include "third_party/blink/public/mojom/conversions/conversions.mojom-blink.h"
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
#include "third_party/blink/renderer/platform/wtf/gc_plugin.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

using ::attribution_reporting::mojom::RegistrationType;
using ::network::mojom::AttributionReportingEligibility;

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
                   absl::optional<uint64_t> request_id,
                   const String& invalid_parameter) {
  String id_string;
  if (request_id) {
    id_string = IdentifiersFactory::SubresourceRequestId(*request_id);
  }

  AuditsIssue::ReportAttributionIssue(execution_context, issue_type, element,
                                      id_string, invalid_parameter);
}

template <typename Container>
Vector<KURL> ParseAttributionSrcUrls(AttributionSrcLoader& loader,
                                     const Document& document,
                                     const Container& strings,
                                     HTMLElement* element) {
  Vector<KURL> urls;
  urls.reserve(base::checked_cast<wtf_size_t>(strings.size()));

  // TODO(crbug.com/1434306): Extract URL-invariant checks to avoid redundant
  // operations and DevTools issues.
  for (wtf_size_t i = 0; i < strings.size(); i++) {
    KURL url = document.CompleteURL(strings[i]);
    if (loader.CanRegister(url, element, /*request_id=*/absl::nullopt)) {
      urls.emplace_back(std::move(url));
    }
  }

  return urls;
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
                     bool cross_app_web_runtime_enabled)
      : web_source(map.Get(http_names::kAttributionReportingRegisterSource)),
        web_trigger(map.Get(http_names::kAttributionReportingRegisterTrigger)),
        request_id(request_id) {
    if (cross_app_web_runtime_enabled &&
        base::FeatureList::IsEnabled(
            network::features::kAttributionReportingCrossAppWeb)) {
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
};

class AttributionSrcLoader::ResourceClient
    : public GarbageCollected<AttributionSrcLoader::ResourceClient>,
      public RawResourceClient {
 public:
  ResourceClient(
      AttributionSrcLoader* loader,
      RegistrationType type,
      mojo::SharedRemote<mojom::blink::AttributionDataHost> data_host)
      : loader_(loader), type_(type), data_host_(std::move(data_host)) {
    DCHECK(loader_);
    DCHECK(loader_->local_frame_);
    DCHECK(loader_->local_frame_->IsAttached());
    CHECK(data_host_.is_bound());
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
      const Vector<network::TriggerVerification>&);

  void Finish();

 private:
  void HandleResponseHeaders(const ResourceResponse& response,
                             uint64_t request_id);

  void HandleSourceRegistration(
      const AttributionHeaders&,
      attribution_reporting::SuitableOrigin reporting_origin);

  void HandleTriggerRegistration(
      const AttributionHeaders&,
      attribution_reporting::SuitableOrigin reporting_origin,
      const Vector<network::TriggerVerification>&);

  [[nodiscard]] bool HasEitherWebOrOsHeader(int header_count,
                                            uint64_t request_id);

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
  const RegistrationType type_;

  // Remote used for registering responses with the browser-process.
  GC_PLUGIN_IGNORE("https://crbug.com/1381979")
  mojo::SharedRemote<mojom::blink::AttributionDataHost> data_host_;

  wtf_size_t num_registrations_ = 0;

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

Vector<KURL> AttributionSrcLoader::ParseAttributionSrc(
    const AtomicString& attribution_src,
    HTMLElement* element) {
  return ParseAttributionSrcUrls(*this, *local_frame_->GetDocument(),
                                 SpaceSplitString(attribution_src), element);
}

void AttributionSrcLoader::Register(const AtomicString& attribution_src,
                                    HTMLElement* element) {
  CreateAndSendRequests(ParseAttributionSrc(attribution_src, element), element,
                        /*attribution_src_token=*/absl::nullopt);
}

absl::optional<Impression> AttributionSrcLoader::RegisterNavigationInternal(
    const KURL& navigation_url,
    Vector<KURL> attribution_src_urls,
    HTMLAnchorElement* element) {
  // TODO(apaseltiner): Add tests to ensure that this method can't be used to
  // register triggers.

  // TODO(crbug.com/1434306): Extract URL-invariant checks to avoid redundant
  // operations and DevTools issues.

  const Impression impression{
      .runtime_features = GetRuntimeFeatures(),
  };

  if (CreateAndSendRequests(std::move(attribution_src_urls), element,
                            impression.attribution_src_token)) {
    return impression;
  }

  if (CanRegister(navigation_url, element, /*request_id=*/absl::nullopt)) {
    return impression;
  }

  return absl::nullopt;
}

absl::optional<Impression> AttributionSrcLoader::RegisterNavigation(
    const KURL& navigation_url,
    const AtomicString& attribution_src,
    HTMLAnchorElement* element) {
  CHECK(!attribution_src.IsNull());
  CHECK(element);

  return RegisterNavigationInternal(
      navigation_url, ParseAttributionSrc(attribution_src, element), element);
}

absl::optional<Impression> AttributionSrcLoader::RegisterNavigation(
    const KURL& navigation_url,
    const WebVector<WebString>& attribution_srcs) {
  return RegisterNavigationInternal(
      navigation_url,
      ParseAttributionSrcUrls(*this, *local_frame_->GetDocument(),
                              attribution_srcs,
                              /*element=*/nullptr),
      /*element=*/nullptr);
}

bool AttributionSrcLoader::CreateAndSendRequests(
    Vector<KURL> urls,
    HTMLElement* element,
    absl::optional<AttributionSrcToken> attribution_src_token) {
  // Detached frames cannot/should not register new attributionsrcs.
  if (!local_frame_->IsAttached() || urls.empty()) {
    return false;
  }

  if (Document* document = local_frame_->DomWindow()->document();
      document->IsPrerendering()) {
    document->AddPostPrerenderingActivationStep(WTF::BindOnce(
        base::IgnoreResult(&AttributionSrcLoader::DoRegistration),
        WrapPersistentIfNeeded(this), std::move(urls), attribution_src_token));
    return false;
  }

  return DoRegistration(urls, attribution_src_token);
}

bool AttributionSrcLoader::DoRegistration(
    const Vector<KURL>& urls,
    const absl::optional<AttributionSrcToken> attribution_src_token) {
  DCHECK(!urls.empty());

  if (!local_frame_->IsAttached()) {
    return false;
  }

  const auto src_type = attribution_src_token.has_value()
                            ? RegistrationType::kSource
                            : RegistrationType::kSourceOrTrigger;

  mojo::AssociatedRemote<mojom::blink::AttributionHost> conversion_host;
  local_frame_->GetRemoteNavigationAssociatedInterfaces()->GetInterface(
      &conversion_host);

  mojo::SharedRemote<mojom::blink::AttributionDataHost> data_host;

  if (attribution_src_token.has_value()) {
    conversion_host->RegisterNavigationDataHost(
        data_host.BindNewPipeAndPassReceiver(), *attribution_src_token);
  } else {
    conversion_host->RegisterDataHost(data_host.BindNewPipeAndPassReceiver(),
                                      src_type);
  }

  for (const KURL& url : urls) {
    // TODO(apaseltiner): Respect the referrerpolicy attribute of the
    // originating <a> or <img> tag, if present.
    ResourceRequest request(url);
    request.SetHttpMethod(http_names::kGET);

    request.SetKeepalive(true);
    request.SetRequestContext(
        mojom::blink::RequestContextType::ATTRIBUTION_SRC);

    request.SetAttributionReportingEligibility(
        attribution_src_token.has_value()
            ? AttributionReportingEligibility::kNavigationSource
            : AttributionReportingEligibility::kEventSourceOrTrigger);

    FetchParameters params(
        std::move(request),
        ResourceLoaderOptions(local_frame_->DomWindow()->GetCurrentWorld()));
    params.MutableOptions().initiator_info.name =
        fetch_initiator_type_names::kAttributionsrc;

    auto* client =
        MakeGarbageCollected<ResourceClient>(this, src_type, data_host);
    // TODO(https://crbug.com/1374121): If this registration is
    // `associated_with_navigation`, there is a risk that the navigation will
    // complete before the resource fetch here is complete. In this case, the
    // browser will mark the page as frozen. This will cause MojoURLLoaderClient
    // to store the request and never dispatch it, causing ResponseReceived() to
    // never be called.
    RawResource::Fetch(params, local_frame_->DomWindow()->Fetcher(), client);

    RecordAttributionSrcRequestStatus(AttributionSrcRequestStatus::kRequested);
  }

  return true;
}

absl::optional<attribution_reporting::SuitableOrigin>
AttributionSrcLoader::ReportingOriginForUrlIfValid(
    const KURL& url,
    HTMLElement* element,
    absl::optional<uint64_t> request_id,
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
    return absl::nullopt;
  }

  if (!window->IsFeatureEnabled(
          mojom::blink::PermissionsPolicyFeature::kAttributionReporting)) {
    maybe_log_audit_issue(
        AttributionReportingIssueType::kPermissionPolicyDisabled);
    return absl::nullopt;
  }

  if (!window->IsSecureContext()) {
    maybe_log_audit_issue(AttributionReportingIssueType::kInsecureContext,
                          window->GetSecurityContext().GetSecurityOrigin());
    return absl::nullopt;
  }

  scoped_refptr<const SecurityOrigin> security_origin =
      SecurityOrigin::Create(url);

  absl::optional<attribution_reporting::SuitableOrigin> reporting_origin =
      attribution_reporting::SuitableOrigin::Create(
          security_origin->ToUrlOrigin());

  if (!url.ProtocolIsInHTTPFamily() || !reporting_origin) {
    maybe_log_audit_issue(
        AttributionReportingIssueType::kUntrustworthyReportingOrigin,
        security_origin.get());
    return absl::nullopt;
  }

  UseCounter::Count(window, mojom::blink::WebFeature::kConversionAPIAll);

  // Only record the ads APIs counter if enabled in that manner.
  if (RuntimeEnabledFeatures::PrivacySandboxAdsAPIsEnabled(window)) {
    UseCounter::Count(window, mojom::blink::WebFeature::kPrivacySandboxAdsAPIs);
  }

  return reporting_origin;
}

bool AttributionSrcLoader::CanRegister(const KURL& url,
                                       HTMLElement* element,
                                       absl::optional<uint64_t> request_id,
                                       bool log_issues) {
  if (!ReportingOriginForUrlIfValid(url, element, request_id, log_issues)) {
    return false;
  }

  if (!network::HasAttributionSupport(GetSupport())) {
    if (log_issues) {
      LogAuditIssue(local_frame_->DomWindow(),
                    AttributionReportingIssueType::kNoWebOrOsSupport, element,
                    request_id,
                    /*invalid_parameter=*/String());
    }
    return false;
  }

  return true;
}

network::mojom::AttributionSupport AttributionSrcLoader::GetSupport() const {
  return Platform::Current()->GetAttributionReportingSupport();
}

network::AttributionReportingRuntimeFeatures
AttributionSrcLoader::GetRuntimeFeatures() const {
  network::AttributionReportingRuntimeFeatures runtime_features;
  if (RuntimeEnabledFeatures::AttributionReportingCrossAppWebEnabled(
          local_frame_->DomWindow())) {
    runtime_features.Put(
        network::AttributionReportingRuntimeFeature::kCrossAppWeb);
  }
  return runtime_features;
}

bool AttributionSrcLoader::MaybeRegisterAttributionHeaders(
    const ResourceRequest& request,
    const ResourceResponse& response,
    const Resource* resource) {
  DCHECK(resource);

  if (response.IsNull()) {
    return false;
  }

  // Attributionsrc requests will be serviced by the
  // `AttributionSrcLoader::ResourceClient`.
  if (request.GetRequestContext() ==
      mojom::blink::RequestContextType::ATTRIBUTION_SRC) {
    return false;
  }

  const uint64_t request_id = request.InspectorId();
  AttributionHeaders headers(
      response.HttpHeaderFields(), request_id,
      RuntimeEnabledFeatures::AttributionReportingCrossAppWebEnabled(
          local_frame_->DomWindow()));

  // Only handle requests which are attempting to invoke the API.
  if (headers.count() == 0) {
    return false;
  }

  absl::optional<attribution_reporting::SuitableOrigin> reporting_origin =
      ReportingOriginForUrlIfValid(response.ResponseUrl(),
                                   /*element=*/nullptr, request_id);
  if (!reporting_origin) {
    return false;
  }

  RegistrationType src_type;

  switch (request.GetAttributionReportingEligibility()) {
    case AttributionReportingEligibility::kEmpty:
      headers.MaybeLogAllSourceHeadersIgnored(local_frame_->DomWindow());
      headers.MaybeLogAllTriggerHeadersIgnored(local_frame_->DomWindow());
      return false;
    case AttributionReportingEligibility::kNavigationSource:
      // Navigation sources are only processed on navigations, which are handled
      // by the browser, or on background attributionsrc requests on
      // navigations, which are handled by `ResourceClient`, so this branch
      // shouldn't be reachable in practice.
      NOTREACHED();
      return false;
    case AttributionReportingEligibility::kEventSource:
      src_type = RegistrationType::kSource;
      break;
    case AttributionReportingEligibility::kUnset:
    case AttributionReportingEligibility::kTrigger:
      src_type = RegistrationType::kTrigger;
      break;
    case AttributionReportingEligibility::kEventSourceOrTrigger:
      src_type = RegistrationType::kSourceOrTrigger;
      break;
  }

  if (Document* document = local_frame_->DomWindow()->document();
      document->IsPrerendering()) {
    document->AddPostPrerenderingActivationStep(WTF::BindOnce(
        &AttributionSrcLoader::RegisterAttributionHeaders,
        WrapPersistentIfNeeded(this), src_type, std::move(*reporting_origin),
        std::move(headers), response.GetTriggerVerifications()));
  } else {
    RegisterAttributionHeaders(src_type, std::move(*reporting_origin), headers,
                               response.GetTriggerVerifications());
  }

  return true;
}

void AttributionSrcLoader::RegisterAttributionHeaders(
    RegistrationType src_type,
    attribution_reporting::SuitableOrigin reporting_origin,
    const AttributionHeaders& headers,
    const Vector<network::TriggerVerification>& trigger_verifications) {
  mojo::AssociatedRemote<mojom::blink::AttributionHost> conversion_host;
  local_frame_->GetRemoteNavigationAssociatedInterfaces()->GetInterface(
      &conversion_host);

  mojo::SharedRemote<mojom::blink::AttributionDataHost> data_host;
  conversion_host->RegisterDataHost(data_host.BindNewPipeAndPassReceiver(),
                                    src_type);

  // Create a client to mimic processing of attributionsrc requests. Note we do
  // not share `AttributionDataHosts` for redirects chains.
  // TODO(johnidel): Consider refactoring this such that we can share clients
  // for redirect chain, or not create the client at all.
  auto* client = MakeGarbageCollected<ResourceClient>(this, src_type,
                                                      std::move(data_host));
  client->HandleResponseHeaders(std::move(reporting_origin), headers,
                                trigger_verifications);
  client->Finish();
}

String AttributionSrcLoader::ResourceClient::DebugName() const {
  return "AttributionSrcLoader::ResourceClient";
}

void AttributionSrcLoader::ResourceClient::ResponseReceived(
    Resource* resource,
    const ResourceResponse& response) {
  HandleResponseHeaders(response, resource->InspectorId());
}

bool AttributionSrcLoader::ResourceClient::RedirectReceived(
    Resource* resource,
    const ResourceRequest& request,
    const ResourceResponse& response) {
  HandleResponseHeaders(response, request.InspectorId());
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
    const ResourceResponse& response,
    uint64_t request_id) {
  AttributionHeaders headers(
      response.HttpHeaderFields(), request_id,
      RuntimeEnabledFeatures::AttributionReportingCrossAppWebEnabled(
          loader_->local_frame_->DomWindow()));
  if (headers.count() == 0) {
    return;
  }

  absl::optional<attribution_reporting::SuitableOrigin> reporting_origin =
      loader_->ReportingOriginForUrlIfValid(response.ResponseUrl(),
                                            /*element=*/nullptr, request_id);
  if (!reporting_origin) {
    return;
  }

  HandleResponseHeaders(std::move(*reporting_origin), headers,
                        response.GetTriggerVerifications());
}

void AttributionSrcLoader::ResourceClient::HandleResponseHeaders(
    attribution_reporting::SuitableOrigin reporting_origin,
    const AttributionHeaders& headers,
    const Vector<network::TriggerVerification>& trigger_verifications) {
  DCHECK_GT(headers.count(), 0);

  switch (type_) {
    case RegistrationType::kSource:
      HandleSourceRegistration(headers, std::move(reporting_origin));
      break;
    case RegistrationType::kTrigger:
      HandleTriggerRegistration(headers, std::move(reporting_origin),
                                trigger_verifications);
      break;
    case RegistrationType::kSourceOrTrigger: {
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
        HandleSourceRegistration(headers, std::move(reporting_origin));
        break;
      }

      DCHECK(has_trigger);
      HandleTriggerRegistration(headers, std::move(reporting_origin),
                                trigger_verifications);
      break;
    }
  }
}

bool AttributionSrcLoader::ResourceClient::HasEitherWebOrOsHeader(
    int header_count,
    uint64_t request_id) {
  if (header_count == 1) {
    return true;
  }

  if (header_count > 1) {
    LogAuditIssue(loader_->local_frame_->DomWindow(),
                  AttributionReportingIssueType::kWebAndOsHeaders,
                  /*element=*/nullptr, request_id,
                  /*invalid_parameter=*/String());
  }

  return false;
}

void AttributionSrcLoader::ResourceClient::HandleSourceRegistration(
    const AttributionHeaders& headers,
    attribution_reporting::SuitableOrigin reporting_origin) {
  DCHECK_NE(type_, RegistrationType::kTrigger);

  headers.MaybeLogAllTriggerHeadersIgnored(loader_->local_frame_->DomWindow());

  if (!HasEitherWebOrOsHeader(headers.source_count(), headers.request_id)) {
    return;
  }

  if (!headers.web_source.IsNull()) {
    if (!network::HasAttributionWebSupport(loader_->GetSupport())) {
      headers.LogSourceIgnored(loader_->local_frame_->DomWindow());
      return;
    }
    auto source_data = attribution_reporting::SourceRegistration::Parse(
        StringUTF8Adaptor(headers.web_source).AsStringPiece());
    if (!source_data.has_value()) {
      LogAuditIssue(loader_->local_frame_->DomWindow(),
                    AttributionReportingIssueType::kInvalidRegisterSourceHeader,
                    /*element=*/nullptr, headers.request_id,
                    /*invalid_parameter=*/headers.web_source);
      return;
    }

    data_host_->SourceDataAvailable(std::move(reporting_origin),
                                    std::move(*source_data));
    ++num_registrations_;
    return;
  }

  DCHECK(!headers.os_source.IsNull());
  if (!network::HasAttributionOsSupport(loader_->GetSupport())) {
    headers.LogOsSourceIgnored(loader_->local_frame_->DomWindow());
    return;
  }

  UseCounter::Count(loader_->local_frame_->DomWindow(),
                    mojom::blink::WebFeature::kAttributionReportingCrossAppWeb);

  std::vector<attribution_reporting::OsRegistrationItem> registration_items =
      attribution_reporting::ParseOsSourceOrTriggerHeader(
          StringUTF8Adaptor(headers.os_source).AsStringPiece());
  if (registration_items.empty()) {
    LogAuditIssue(loader_->local_frame_->DomWindow(),
                  AttributionReportingIssueType::kInvalidRegisterOsSourceHeader,
                  /*element=*/nullptr, headers.request_id,
                  /*invalid_parameter=*/headers.os_source);
    return;
  }
  data_host_->OsSourceDataAvailable(std::move(registration_items));
  ++num_registrations_;
}

void AttributionSrcLoader::ResourceClient::HandleTriggerRegistration(
    const AttributionHeaders& headers,
    attribution_reporting::SuitableOrigin reporting_origin,
    const Vector<network::TriggerVerification>& trigger_verifications) {
  DCHECK_NE(type_, RegistrationType::kSource);

  headers.MaybeLogAllSourceHeadersIgnored(loader_->local_frame_->DomWindow());

  if (!HasEitherWebOrOsHeader(headers.trigger_count(), headers.request_id)) {
    return;
  }

  if (!headers.web_trigger.IsNull()) {
    if (!network::HasAttributionWebSupport(loader_->GetSupport())) {
      headers.LogTriggerIgnored(loader_->local_frame_->DomWindow());
      return;
    }
    auto trigger_data = attribution_reporting::TriggerRegistration::Parse(
        StringUTF8Adaptor(headers.web_trigger).AsStringPiece());
    if (!trigger_data.has_value()) {
      LogAuditIssue(
          loader_->local_frame_->DomWindow(),
          AttributionReportingIssueType::kInvalidRegisterTriggerHeader,
          /*element=*/nullptr, headers.request_id,
          /*invalid_parameter=*/headers.web_trigger);
      return;
    }

    data_host_->TriggerDataAvailable(std::move(reporting_origin),
                                     std::move(*trigger_data),
                                     std::move(trigger_verifications));
    ++num_registrations_;
    return;
  }

  DCHECK(!headers.os_trigger.IsNull());
  if (!network::HasAttributionOsSupport(loader_->GetSupport())) {
    headers.LogOsTriggerIgnored(loader_->local_frame_->DomWindow());
    return;
  }

  UseCounter::Count(loader_->local_frame_->DomWindow(),
                    mojom::blink::WebFeature::kAttributionReportingCrossAppWeb);

  std::vector<attribution_reporting::OsRegistrationItem> registration_items =
      attribution_reporting::ParseOsSourceOrTriggerHeader(
          StringUTF8Adaptor(headers.os_trigger).AsStringPiece());
  if (registration_items.empty()) {
    LogAuditIssue(
        loader_->local_frame_->DomWindow(),
        AttributionReportingIssueType::kInvalidRegisterOsTriggerHeader,
        /*element=*/nullptr, headers.request_id,
        /*invalid_parameter=*/headers.os_trigger);
    return;
  }
  data_host_->OsTriggerDataAvailable(std::move(registration_items));
  ++num_registrations_;
}

}  // namespace blink
