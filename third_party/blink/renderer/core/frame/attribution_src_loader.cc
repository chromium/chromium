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
#include "base/types/expected.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/attribution_reporting/eligibility.h"
#include "components/attribution_reporting/eligibility_error.mojom-shared.h"
#include "components/attribution_reporting/os_registration.h"
#include "components/attribution_reporting/os_support.mojom-shared.h"
#include "components/attribution_reporting/registration_type.mojom-shared.h"
#include "components/attribution_reporting/source_registration.h"
#include "components/attribution_reporting/source_registration_error.mojom-shared.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "components/attribution_reporting/trigger_registration_error.mojom-shared.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/trigger_attestation.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/navigation/impression.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom-blink.h"
#include "third_party/blink/public/mojom/conversions/conversions.mojom-blink.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_issue.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/self_keep_alive.h"
#include "third_party/blink/renderer/platform/loader/attribution_header_constants.h"
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

namespace blink {

namespace {

using ::attribution_reporting::mojom::EligibilityError;
using ::attribution_reporting::mojom::RegistrationType;

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
  if (request_id)
    id_string = IdentifiersFactory::SubresourceRequestId(*request_id);

  AuditsIssue::ReportAttributionIssue(execution_context, issue_type, element,
                                      id_string, invalid_parameter);
}

}  // namespace

struct AttributionSrcLoader::AttributionHeaders {
  AtomicString web_source;
  AtomicString web_trigger;
  AtomicString os_source;
  AtomicString os_trigger;
  uint64_t request_id;

  AttributionHeaders(const HTTPHeaderMap& map, uint64_t request_id)
      : web_source(map.Get(http_names::kAttributionReportingRegisterSource)),
        web_trigger(map.Get(http_names::kAttributionReportingRegisterTrigger)),
        request_id(request_id) {
    if (base::FeatureList::IsEnabled(
            blink::features::kAttributionReportingCrossAppWeb)) {
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

  void MaybeLogAllSourceHeadersIgnored(
      ExecutionContext* execution_context) const {
    if (!web_source.IsNull()) {
      LogAuditIssue(execution_context,
                    AttributionReportingIssueType::kSourceIgnored,
                    /*element=*/nullptr, request_id,
                    /*invalid_parameter=*/web_source);
    }

    if (!os_source.IsNull()) {
      LogOsSourceIgnored(execution_context);
    }
  }

  void MaybeLogAllTriggerHeadersIgnored(
      ExecutionContext* execution_context) const {
    if (!web_trigger.IsNull()) {
      LogAuditIssue(execution_context,
                    AttributionReportingIssueType::kTriggerIgnored,
                    /*element=*/nullptr, request_id,
                    /*invalid_parameter=*/web_trigger);
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
  // `associated_with_navigation` indicates whether the attribution data
  // produced by this client will need to be associated with a navigation.
  ResourceClient(AttributionSrcLoader* loader,
                 RegistrationType type,
                 bool associated_with_navigation)
      : loader_(loader), type_(type) {
    DCHECK(loader_);
    DCHECK(loader_->local_frame_);
    DCHECK(loader_->local_frame_->IsAttached());

    mojo::AssociatedRemote<mojom::blink::ConversionHost> conversion_host;
    loader_->local_frame_->GetRemoteNavigationAssociatedInterfaces()
        ->GetInterface(&conversion_host);

    if (associated_with_navigation) {
      // Create a new token which will be used to identify `data_host_` in the
      // browser process.
      attribution_src_token_ = AttributionSrcToken();
      conversion_host->RegisterNavigationDataHost(
          data_host_.BindNewPipeAndPassReceiver(), *attribution_src_token_);
    } else {
      // Send the data host normally.
      conversion_host->RegisterDataHost(data_host_.BindNewPipeAndPassReceiver(),
                                        type);
    }
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

  const absl::optional<AttributionSrcToken>& attribution_src_token() const {
    return attribution_src_token_;
  }

  void HandleResponseHeaders(
      attribution_reporting::SuitableOrigin reporting_origin,
      const AttributionHeaders&,
      const absl::optional<network::TriggerAttestation>& trigger_attestation);

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
      const absl::optional<network::TriggerAttestation>& trigger_attestation);

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

  // Type of events this request can register. In some cases, this will not be
  // assigned until the first event is received. A single attributionsrc
  // request can only register one type of event across redirects.
  RegistrationType type_;

  // Token used to identify an attributionsrc request in the browser process.
  // Only generated for attributionsrc requests that are associated with a
  // navigation.
  absl::optional<AttributionSrcToken> attribution_src_token_;

  // Remote used for registering responses with the browser-process.
  GC_PLUGIN_IGNORE("https://crbug.com/1381979")
  mojo::Remote<mojom::blink::AttributionDataHost> data_host_;

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

void AttributionSrcLoader::Register(const KURL& src_url, HTMLElement* element) {
  CreateAndSendRequest(src_url, element, RegistrationType::kSourceOrTrigger,
                       /*associated_with_navigation=*/false);
}

absl::optional<Impression> AttributionSrcLoader::RegisterNavigation(
    const KURL& src_url,
    mojom::blink::AttributionNavigationType nav_type,
    HTMLElement* element) {
  // TODO(apaseltiner): Add tests to ensure that this method can't be used to
  // register triggers.
  ResourceClient* client =
      CreateAndSendRequest(src_url, element, RegistrationType::kSource,
                           /*associated_with_navigation=*/true);
  if (!client)
    return absl::nullopt;

  DCHECK(client->attribution_src_token());
  return blink::Impression{
      .attribution_src_token = *client->attribution_src_token(),
      .nav_type = nav_type};
}

AttributionSrcLoader::ResourceClient*
AttributionSrcLoader::CreateAndSendRequest(const KURL& src_url,
                                           HTMLElement* element,
                                           RegistrationType src_type,
                                           bool associated_with_navigation) {
  // Detached frames cannot/should not register new attributionsrcs.
  if (!local_frame_->IsAttached())
    return nullptr;

  LocalDOMWindow* window = local_frame_->DomWindow();

  if (num_resource_clients_ >= kMaxConcurrentRequests) {
    LogAuditIssue(
        window, AttributionReportingIssueType::kTooManyConcurrentRequests,
        element, /*request_id=*/absl::nullopt,
        /*invalid_parameter=*/AtomicString::Number(kMaxConcurrentRequests));
    return nullptr;
  }

  if (!CanRegister(src_url, element, /*request_id=*/absl::nullopt))
    return nullptr;

  Document* document = window->document();

  if (document->IsPrerendering()) {
    document->AddPostPrerenderingActivationStep(
        WTF::BindOnce(base::IgnoreResult(&AttributionSrcLoader::DoRegistration),
                      WrapPersistentIfNeeded(this), src_url, src_type,
                      associated_with_navigation));
    return nullptr;
  }

  return DoRegistration(src_url, src_type, associated_with_navigation);
}

AttributionSrcLoader::ResourceClient* AttributionSrcLoader::DoRegistration(
    const KURL& src_url,
    RegistrationType src_type,
    bool associated_with_navigation) {
  if (!local_frame_->IsAttached())
    return nullptr;

  // TODO(apaseltiner): Respect the referrerpolicy attribute of the
  // originating <a> or <img> tag, if present.
  ResourceRequest request(src_url);
  request.SetHttpMethod(http_names::kGET);

  request.SetKeepalive(true);
  request.SetRequestContext(mojom::blink::RequestContextType::ATTRIBUTION_SRC);

  const char* eligible = [src_type,
                          associated_with_navigation]() -> const char* {
    switch (src_type) {
      case RegistrationType::kSource:
        return associated_with_navigation ? "navigation-source"
                                          : "event-source";
      case RegistrationType::kTrigger:
        NOTREACHED();
        return nullptr;
      case RegistrationType::kSourceOrTrigger:
        DCHECK(!associated_with_navigation);
        return kAttributionEligibleEventSourceAndTrigger;
    }
  }();

  request.SetHttpHeaderField(http_names::kAttributionReportingEligible,
                             eligible);

  FetchParameters params(
      std::move(request),
      ResourceLoaderOptions(local_frame_->DomWindow()->GetCurrentWorld()));
  params.MutableOptions().initiator_info.name =
      fetch_initiator_type_names::kAttributionsrc;

  auto* client = MakeGarbageCollected<ResourceClient>(
      this, src_type, associated_with_navigation);
  ++num_resource_clients_;

  // TODO(https://crbug.com/1374121): If this registration is
  // `associated_with_navigation`, there is a risk that the navigation will
  // complete before the resource fetch here is complete. In this case, the
  // browser will mark the page as frozen. This will cause MojoURLLoaderClient
  // to store the request and never dispatch it, causing ResponseReceived() to
  // never be called.
  RawResource::Fetch(params, local_frame_->DomWindow()->Fetcher(), client);

  RecordAttributionSrcRequestStatus(AttributionSrcRequestStatus::kRequested);

  return client;
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
    if (!log_issues)
      return;

    LogAuditIssue(window, issue_type, element, request_id,
                  /*invalid_parameter=*/
                  invalid_origin ? invalid_origin->ToString() : String());
  };

  if (!RuntimeEnabledFeatures::AttributionReportingEnabled(window))
    return absl::nullopt;

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
  return !!ReportingOriginForUrlIfValid(url, element, request_id, log_issues);
}

AtomicString AttributionSrcLoader::GetSupportHeader() const {
  return AtomicString(String::FromUTF8(attribution_reporting::GetSupportHeader(
      Platform::Current()->GetOsSupportForAttributionReporting())));
}

bool AttributionSrcLoader::HasOsSupport() const {
  return Platform::Current()->GetOsSupportForAttributionReporting() ==
         attribution_reporting::mojom::OsSupport::kEnabled;
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
  AttributionHeaders headers(response.HttpHeaderFields(), request_id);

  // Only handle requests which are attempting to invoke the API.
  if (headers.count() == 0) {
    return false;
  }

  absl::optional<attribution_reporting::SuitableOrigin> reporting_origin =
      ReportingOriginForUrlIfValid(response.ResponseUrl(),
                                   /*element=*/nullptr, request_id);
  if (!reporting_origin)
    return false;

  // Determine eligibility for this registration by considering first request
  // for a resource (even if `response` is for a redirect). This indicates
  // whether the redirect chain was configured for eligibility.
  // https://github.com/WICG/attribution-reporting-api/blob/main/EVENT.md#registering-attribution-sources
  const AtomicString& eligible_header =
      resource->GetResourceRequest().HttpHeaderField(
          http_names::kAttributionReportingEligible);

  auto src_type = attribution_reporting::ParseEligibleHeader(
      eligible_header.IsNull()
          ? absl::nullopt
          : absl::make_optional(
                StringUTF8Adaptor(eligible_header).AsStringPiece()));

  if (!src_type.has_value()) {
    switch (src_type.error()) {
      case EligibilityError::kInvalidStructuredHeader:
      case EligibilityError::kContainsNavigationSource:
        LogAuditIssue(local_frame_->DomWindow(),
                      AttributionReportingIssueType::kInvalidEligibleHeader,
                      /*element=*/nullptr, request_id,
                      /*invalid_parameter=*/eligible_header);
        break;
      case EligibilityError::kIneligible:
        headers.MaybeLogAllSourceHeadersIgnored(local_frame_->DomWindow());
        headers.MaybeLogAllTriggerHeadersIgnored(local_frame_->DomWindow());
        break;
    }
    return false;
  }

  // TODO(johnidel): We should consider updating the eligibility header based on
  // previously registered requests in the chain.

  if (Document* document = local_frame_->DomWindow()->document();
      document->IsPrerendering()) {
    document->AddPostPrerenderingActivationStep(WTF::BindOnce(
        &AttributionSrcLoader::RegisterAttributionHeaders,
        WrapPersistentIfNeeded(this), *src_type, std::move(*reporting_origin),
        std::move(headers), response.GetTriggerAttestation()));
  } else {
    RegisterAttributionHeaders(*src_type, std::move(*reporting_origin), headers,
                               response.GetTriggerAttestation());
  }

  return true;
}

void AttributionSrcLoader::RegisterAttributionHeaders(
    RegistrationType src_type,
    attribution_reporting::SuitableOrigin reporting_origin,
    const AttributionHeaders& headers,
    const absl::optional<network::TriggerAttestation>& trigger_attestation) {
  // Create a client to mimic processing of attributionsrc requests. Note we do
  // not share `AttributionDataHosts` for redirects chains.
  // TODO(johnidel): Consider refactoring this such that we can share clients
  // for redirect chain, or not create the client at all.
  auto* client = MakeGarbageCollected<ResourceClient>(
      this, src_type, /*associated_with_navigation=*/false);
  client->HandleResponseHeaders(std::move(reporting_origin), headers,
                                trigger_attestation);
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

  DCHECK_GT(loader_->num_resource_clients_, 0u);
  --loader_->num_resource_clients_;

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
}

void AttributionSrcLoader::ResourceClient::HandleResponseHeaders(
    const ResourceResponse& response,
    uint64_t request_id) {
  AttributionHeaders headers(response.HttpHeaderFields(), request_id);
  if (headers.count() == 0) {
    return;
  }

  absl::optional<attribution_reporting::SuitableOrigin> reporting_origin =
      loader_->ReportingOriginForUrlIfValid(response.ResponseUrl(),
                                            /*element=*/nullptr, request_id);
  if (!reporting_origin)
    return;

  HandleResponseHeaders(std::move(*reporting_origin), headers,
                        response.GetTriggerAttestation());
}

void AttributionSrcLoader::ResourceClient::HandleResponseHeaders(
    attribution_reporting::SuitableOrigin reporting_origin,
    const AttributionHeaders& headers,
    const absl::optional<network::TriggerAttestation>& trigger_attestation) {
  DCHECK_GT(headers.count(), 0);

  switch (type_) {
    case RegistrationType::kSource:
      HandleSourceRegistration(headers, std::move(reporting_origin));
      break;
    case RegistrationType::kTrigger:
      HandleTriggerRegistration(headers, std::move(reporting_origin),
                                trigger_attestation);
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
                                trigger_attestation);
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

  type_ = RegistrationType::kSource;

  if (!headers.web_source.IsNull()) {
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
    return;
  }

  DCHECK(!headers.os_source.IsNull());
  if (!loader_->HasOsSupport()) {
    headers.LogOsSourceIgnored(loader_->local_frame_->DomWindow());
    return;
  }

#if BUILDFLAG(IS_ANDROID)
  GURL registration_url = attribution_reporting::ParseOsSourceOrTriggerHeader(
      StringUTF8Adaptor(headers.os_source).AsStringPiece());
  if (!registration_url.is_valid()) {
    LogAuditIssue(loader_->local_frame_->DomWindow(),
                  AttributionReportingIssueType::kInvalidRegisterOsSourceHeader,
                  /*element=*/nullptr, headers.request_id,
                  /*invalid_parameter=*/headers.os_source);
    return;
  }
  data_host_->OsSourceDataAvailable(KURL(registration_url));
#else
  NOTREACHED();
#endif
}

void AttributionSrcLoader::ResourceClient::HandleTriggerRegistration(
    const AttributionHeaders& headers,
    attribution_reporting::SuitableOrigin reporting_origin,
    const absl::optional<network::TriggerAttestation>& trigger_attestation) {
  DCHECK_NE(type_, RegistrationType::kSource);

  headers.MaybeLogAllSourceHeadersIgnored(loader_->local_frame_->DomWindow());

  if (!HasEitherWebOrOsHeader(headers.trigger_count(), headers.request_id)) {
    return;
  }

  type_ = RegistrationType::kTrigger;

  if (!headers.web_trigger.IsNull()) {
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
                                     std::move(trigger_attestation));
    return;
  }

  DCHECK(!headers.os_trigger.IsNull());
  if (!loader_->HasOsSupport()) {
    headers.LogOsTriggerIgnored(loader_->local_frame_->DomWindow());
    return;
  }

#if BUILDFLAG(IS_ANDROID)
  GURL registration_url = attribution_reporting::ParseOsSourceOrTriggerHeader(
      StringUTF8Adaptor(headers.os_trigger).AsStringPiece());
  if (!registration_url.is_valid()) {
    LogAuditIssue(
        loader_->local_frame_->DomWindow(),
        AttributionReportingIssueType::kInvalidRegisterOsTriggerHeader,
        /*element=*/nullptr, headers.request_id,
        /*invalid_parameter=*/headers.os_trigger);
    return;
  }
  data_host_->OsTriggerDataAvailable(KURL(registration_url));
#else
  NOTREACHED();
#endif
}

}  // namespace blink
