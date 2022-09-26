// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/attribution_src_loader.h"

#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/structured_headers.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/frame/frame_policy.h"
#include "third_party/blink/public/common/navigation/impression.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom-blink.h"
#include "third_party/blink/public/mojom/conversions/conversions.mojom-blink.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/attribution_response_parsing.h"
#include "third_party/blink/renderer/core/frame/frame_owner.h"
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
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

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

void MaybeLogSourceIgnored(ExecutionContext* execution_context,
                           uint64_t request_id,
                           const String& json) {
  if (json.IsNull())
    return;

  LogAuditIssue(execution_context,
                AttributionReportingIssueType::kSourceIgnored,
                /*element=*/nullptr, request_id,
                /*invalid_parameter=*/json);
}

void MaybeLogTriggerIgnored(ExecutionContext* execution_context,
                            uint64_t request_id,
                            const String& json) {
  if (json.IsNull())
    return;

  LogAuditIssue(execution_context,
                AttributionReportingIssueType::kTriggerIgnored,
                /*element=*/nullptr, request_id,
                /*invalid_parameter=*/json);
}

bool IsValidReportingOrigin(const SecurityOrigin* origin) {
  return origin && origin->IsPotentiallyTrustworthy() &&
         (origin->Protocol() == WTF::g_https_atom ||
          origin->Protocol() == WTF::g_http_atom);
}

bool SubframeHasAllowedContainerPolicy(LocalFrame* frame) {
  DCHECK(frame->Parent());
  const FramePolicy& frame_policy = frame->Owner()->GetFramePolicy();
  const SecurityOrigin* origin =
      frame->GetSecurityContext()->GetSecurityOrigin();
  for (const auto& decl : frame_policy.container_policy) {
    if (decl.feature ==
        mojom::blink::PermissionsPolicyFeature::kAttributionReporting) {
      return decl.Contains(origin->ToUrlOrigin());
    }
  }
  return false;
}

}  // namespace

class AttributionSrcLoader::ResourceClient
    : public GarbageCollected<AttributionSrcLoader::ResourceClient>,
      public RawResourceClient {
 public:
  // `associated_with_navigation` indicates whether the attribution data
  // produced by this client will need to be associated with a navigation.
  ResourceClient(AttributionSrcLoader* loader,
                 SrcType type,
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
      conversion_host->RegisterDataHost(
          data_host_.BindNewPipeAndPassReceiver());
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
      scoped_refptr<const SecurityOrigin> reporting_origin,
      const AtomicString& source_json,
      const AtomicString& trigger_json,
      uint64_t request_id);

  void Finish();

 private:
  void HandleResponseHeaders(const ResourceResponse& response,
                             uint64_t request_id);

  void HandleSourceRegistration(
      const AtomicString& json,
      scoped_refptr<const SecurityOrigin> reporting_origin,
      uint64_t request_id);
  void HandleTriggerRegistration(
      const AtomicString& json,
      scoped_refptr<const SecurityOrigin> reporting_origin,
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
  SrcType type_;

  // Token used to identify an attributionsrc request in the browser process.
  // Only generated for attributionsrc requests that are associated with a
  // navigation.
  absl::optional<AttributionSrcToken> attribution_src_token_;

  // Remote used for registering responses with the browser-process.
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
  CreateAndSendRequest(src_url, element, SrcType::kUndetermined,
                       /*associated_with_navigation=*/false);
}

absl::optional<Impression> AttributionSrcLoader::RegisterNavigation(
    const KURL& src_url,
    HTMLElement* element) {
  // TODO(apaseltiner): Add tests to ensure that this method can't be used to
  // register triggers.
  ResourceClient* client =
      CreateAndSendRequest(src_url, element, SrcType::kSource,
                           /*associated_with_navigation=*/true);
  if (!client)
    return absl::nullopt;

  DCHECK(client->attribution_src_token());
  return blink::Impression{.attribution_src_token =
                               *client->attribution_src_token()};
}

AttributionSrcLoader::ResourceClient*
AttributionSrcLoader::CreateAndSendRequest(const KURL& src_url,
                                           HTMLElement* element,
                                           SrcType src_type,
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
        WTF::Bind(base::IgnoreResult(&AttributionSrcLoader::DoRegistration),
                  WrapPersistentIfNeeded(this), src_url, src_type,
                  associated_with_navigation));
    return nullptr;
  }

  return DoRegistration(src_url, src_type, associated_with_navigation);
}

AttributionSrcLoader::ResourceClient* AttributionSrcLoader::DoRegistration(
    const KURL& src_url,
    SrcType src_type,
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
      case SrcType::kSource:
        return associated_with_navigation ? kAttributionEligibleNavigationSource
                                          : kAttributionEligibleEventSource;
      case SrcType::kTrigger:
        NOTREACHED();
        return nullptr;
      case SrcType::kUndetermined:
        DCHECK(!associated_with_navigation);
        return kAttributionEligibleEventSourceAndTrigger;
    }
  }();

  request.SetHttpHeaderField(http_names::kAttributionReportingEligible,
                             eligible);

  FetchParameters params(std::move(request),
                         local_frame_->DomWindow()->GetCurrentWorld());
  params.MutableOptions().initiator_info.name =
      fetch_initiator_type_names::kAttributionsrc;

  auto* client = MakeGarbageCollected<ResourceClient>(
      this, src_type, associated_with_navigation);
  ++num_resource_clients_;
  RawResource::Fetch(params, local_frame_->DomWindow()->Fetcher(), client);

  RecordAttributionSrcRequestStatus(AttributionSrcRequestStatus::kRequested);

  return client;
}

scoped_refptr<const SecurityOrigin>
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
    return nullptr;

  if (!window->IsFeatureEnabled(
          mojom::blink::PermissionsPolicyFeature::kAttributionReporting)) {
    maybe_log_audit_issue(
        AttributionReportingIssueType::kPermissionPolicyDisabled);
    return nullptr;
  }

  if (local_frame_->Parent() &&
      !SubframeHasAllowedContainerPolicy(local_frame_)) {
    maybe_log_audit_issue(
        AttributionReportingIssueType::kPermissionPolicyNotDelegated);
  }

  if (!window->IsSecureContext()) {
    maybe_log_audit_issue(AttributionReportingIssueType::kInsecureContext,
                          window->GetSecurityContext().GetSecurityOrigin());
    return nullptr;
  }

  scoped_refptr<const SecurityOrigin> reporting_origin =
      SecurityOrigin::Create(url);
  if (!url.ProtocolIsInHTTPFamily() ||
      !reporting_origin->IsPotentiallyTrustworthy()) {
    maybe_log_audit_issue(
        AttributionReportingIssueType::kUntrustworthyReportingOrigin,
        reporting_origin.get());
    return nullptr;
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

  const auto& response_headers = response.HttpHeaderFields();
  const AtomicString& source_json =
      response_headers.Get(http_names::kAttributionReportingRegisterSource);
  const AtomicString& trigger_json =
      response_headers.Get(http_names::kAttributionReportingRegisterTrigger);

  // Only handle requests which are attempting to invoke the API.
  if (source_json.IsNull() && trigger_json.IsNull())
    return false;

  const uint64_t request_id = request.InspectorId();
  scoped_refptr<const SecurityOrigin> reporting_origin =
      ReportingOriginForUrlIfValid(response.ResponseUrl(),
                                   /*element=*/nullptr, request_id);
  if (!reporting_origin)
    return false;

  SrcType src_type = SrcType::kUndetermined;

  // Determine eligibility for this registration by considering first request
  // for a resource (even if `response` is for a redirect). This indicates
  // whether the redirect chain was configured for eligibility.
  // https://github.com/WICG/attribution-reporting-api/blob/main/EVENT.md#registering-attribution-sources
  const AtomicString& eligible_header =
      resource->GetResourceRequest().HttpHeaderField(
          http_names::kAttributionReportingEligible);

  if (eligible_header.IsNull()) {
    // All subresources are eligible to register triggers if they do *not*
    // specify the header.
    src_type = SrcType::kTrigger;
  } else {
    absl::optional<net::structured_headers::Dictionary> dict =
        net::structured_headers::ParseDictionary(
            StringUTF8Adaptor(eligible_header).AsStringPiece());
    if (!dict || dict->contains(kAttributionEligibleNavigationSource)) {
      LogAuditIssue(local_frame_->DomWindow(),
                    AttributionReportingIssueType::kInvalidEligibleHeader,
                    /*element=*/nullptr, request_id,
                    /*invalid_parameter=*/eligible_header);
      return false;
    }

    const bool allows_event_source =
        dict->contains(kAttributionEligibleEventSource);
    const bool allows_trigger = dict->contains(kAttributionEligibleTrigger);

    if (allows_event_source && allows_trigger) {
      // We use an undetermined SrcType which indicates either a source or
      // trigger may be registered.
      src_type = SrcType::kUndetermined;
    } else if (allows_event_source) {
      src_type = SrcType::kSource;
    } else if (allows_trigger) {
      src_type = SrcType::kTrigger;
    } else {
      MaybeLogSourceIgnored(local_frame_->DomWindow(), request_id, source_json);
      MaybeLogTriggerIgnored(local_frame_->DomWindow(), request_id,
                             trigger_json);
      return false;
    }
  }

  // TODO(johnidel): We should consider updating the eligibility header based on
  // previously registered requests in the chain.

  // Create a client to mimic processing of attributionsrc requests. Note we do
  // not share `AttributionDataHosts` for redirects chains.
  // TODO(johnidel): Consider refactoring this such that we can share clients
  // for redirect chain, or not create the client at all.

  auto* client = MakeGarbageCollected<ResourceClient>(
      this, src_type, /*associated_with_navigation=*/false);
  client->HandleResponseHeaders(std::move(reporting_origin), source_json,
                                trigger_json, resource->InspectorId());
  client->Finish();
  return true;
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
  const auto& headers = response.HttpHeaderFields();
  const AtomicString& source_json =
      headers.Get(http_names::kAttributionReportingRegisterSource);
  const AtomicString& trigger_json =
      headers.Get(http_names::kAttributionReportingRegisterTrigger);

  if (source_json.IsNull() && trigger_json.IsNull())
    return;

  scoped_refptr<const SecurityOrigin> reporting_origin =
      loader_->ReportingOriginForUrlIfValid(response.ResponseUrl(),
                                            /*element=*/nullptr, request_id);
  if (!reporting_origin)
    return;

  HandleResponseHeaders(std::move(reporting_origin), source_json, trigger_json,
                        request_id);
}

void AttributionSrcLoader::ResourceClient::HandleResponseHeaders(
    scoped_refptr<const SecurityOrigin> reporting_origin,
    const AtomicString& source_json,
    const AtomicString& trigger_json,
    uint64_t request_id) {
  DCHECK(IsValidReportingOrigin(reporting_origin.get()));
  DCHECK(!source_json.IsNull() || !trigger_json.IsNull());

  switch (type_) {
    case SrcType::kSource:
      MaybeLogTriggerIgnored(loader_->local_frame_->DomWindow(), request_id,
                             trigger_json);

      if (!source_json.IsNull()) {
        HandleSourceRegistration(source_json, std::move(reporting_origin),
                                 request_id);
      }
      break;
    case SrcType::kTrigger:
      MaybeLogSourceIgnored(loader_->local_frame_->DomWindow(), request_id,
                            source_json);

      if (!trigger_json.IsNull()) {
        HandleTriggerRegistration(trigger_json, std::move(reporting_origin),
                                  request_id);
      }
      break;
    case SrcType::kUndetermined:
      if (!source_json.IsNull() && !trigger_json.IsNull()) {
        LogAuditIssue(loader_->local_frame_->DomWindow(),
                      AttributionReportingIssueType::kSourceAndTriggerHeaders,
                      /*element=*/nullptr, request_id,
                      /*invalid_parameter=*/String());
        return;
      }

      if (!source_json.IsNull()) {
        type_ = SrcType::kSource;
        HandleSourceRegistration(source_json, std::move(reporting_origin),
                                 request_id);
        return;
      }

      if (!trigger_json.IsNull()) {
        type_ = SrcType::kTrigger;
        HandleTriggerRegistration(trigger_json, std::move(reporting_origin),
                                  request_id);
      }

      break;
  }
}

void AttributionSrcLoader::ResourceClient::HandleSourceRegistration(
    const AtomicString& json,
    scoped_refptr<const SecurityOrigin> reporting_origin,
    uint64_t request_id) {
  DCHECK_EQ(type_, SrcType::kSource);
  DCHECK(!json.IsNull());
  DCHECK(IsValidReportingOrigin(reporting_origin.get()));

  auto source_data = mojom::blink::AttributionSourceData::New();
  source_data->reporting_origin = std::move(reporting_origin);

  if (!attribution_response_parsing::ParseSourceRegistrationHeader(
          json, *source_data)) {
    LogAuditIssue(loader_->local_frame_->DomWindow(),
                  AttributionReportingIssueType::kInvalidRegisterSourceHeader,
                  /*element=*/nullptr, request_id,
                  /*invalid_parameter=*/json);
    return;
  }

  data_host_->SourceDataAvailable(std::move(source_data));
}

void AttributionSrcLoader::ResourceClient::HandleTriggerRegistration(
    const AtomicString& json,
    scoped_refptr<const SecurityOrigin> reporting_origin,
    uint64_t request_id) {
  DCHECK_EQ(type_, SrcType::kTrigger);
  DCHECK(!json.IsNull());
  DCHECK(IsValidReportingOrigin(reporting_origin.get()));

  auto trigger_data = mojom::blink::AttributionTriggerData::New();
  trigger_data->reporting_origin = std::move(reporting_origin);

  if (!attribution_response_parsing::ParseTriggerRegistrationHeader(
          json, *trigger_data)) {
    LogAuditIssue(loader_->local_frame_->DomWindow(),
                  AttributionReportingIssueType::kInvalidRegisterTriggerHeader,
                  /*element=*/nullptr, request_id,
                  /*invalid_parameter=*/json);
    return;
  }

  data_host_->TriggerDataAvailable(std::move(trigger_data));
}

}  // namespace blink
