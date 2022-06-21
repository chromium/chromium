// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/attribution_src_loader.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/structured_headers.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/navigation/impression.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom-blink.h"
#include "third_party/blink/public/mojom/conversions/conversions.mojom-blink.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/attribution_response_parsing.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
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
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
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

void MaybeLogAuditIssue(LocalFrame* frame,
                        AttributionReportingIssueType issue_type,
                        const absl::optional<String>& string,
                        HTMLElement* element,
                        absl::optional<uint64_t> request_id) {
  if (!frame->IsAttached())
    return;

  absl::optional<String> id_string;
  if (request_id)
    id_string = IdentifiersFactory::SubresourceRequestId(*request_id);

  AuditsIssue::ReportAttributionIssue(frame->DomWindow(), issue_type,
                                      frame->GetDevToolsFrameToken(), element,
                                      id_string, string);
}

}  // namespace

bool CanRegisterAttributionInContext(
    LocalFrame* frame,
    HTMLElement* element,
    absl::optional<uint64_t> request_id,
    AttributionSrcLoader::RegisterContext context,
    bool log_issues) {
  LocalDOMWindow* window = frame->DomWindow();
  DCHECK(window);

  if (!RuntimeEnabledFeatures::AttributionReportingEnabled(window))
    return false;

  const bool feature_policy_enabled = window->IsFeatureEnabled(
      mojom::blink::PermissionsPolicyFeature::kAttributionReporting);

  if (!feature_policy_enabled) {
    if (log_issues) {
      MaybeLogAuditIssue(
          frame, AttributionReportingIssueType::kPermissionPolicyDisabled,
          /*string=*/absl::nullopt, element, request_id);
    }
    return false;
  }

  // The API is only allowed in secure contexts.
  if (!window->IsSecureContext()) {
    if (log_issues) {
      MaybeLogAuditIssue(
          frame,
          context == AttributionSrcLoader::RegisterContext::kAttributionSrc
              ? AttributionReportingIssueType::
                    kAttributionSourceUntrustworthyOrigin
              : AttributionReportingIssueType::kAttributionUntrustworthyOrigin,
          frame->GetSecurityContext()->GetSecurityOrigin()->ToString(), element,
          request_id);
    }
    return false;
  }

  return true;
}

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

  // Public, may be called if a response was received prior to the client being
  // added to the resource.
  void HandleResponseHeaders(const ResourceResponse& response,
                             uint64_t request_id);

 private:
  void HandleSourceRegistration(const ResourceResponse& response,
                                uint64_t request_id);
  void HandleTriggerRegistration(const ResourceResponse& response);

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

  if (num_resource_clients_ >= kMaxConcurrentRequests)
    return nullptr;

  if (!src_url.ProtocolIsInHTTPFamily())
    return nullptr;

  if (!UrlCanRegisterAttribution(RegisterContext::kAttributionSrc, src_url,
                                 element, /*request_id=*/absl::nullopt)) {
    return nullptr;
  }

  LocalDOMWindow* window = local_frame_->DomWindow();
  Document* document = window->document();

  if (document->IsPrerendering()) {
    document->AddPostPrerenderingActivationStep(
        WTF::Bind(&AttributionSrcLoader::DoPrerenderingRegistration,
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

void AttributionSrcLoader::DoPrerenderingRegistration(
    const KURL& src_url,
    SrcType src_type,
    bool associated_with_navigation) {
  DoRegistration(src_url, src_type, associated_with_navigation);
}

bool AttributionSrcLoader::UrlCanRegisterAttribution(
    RegisterContext context,
    const KURL& url,
    HTMLElement* element,
    absl::optional<uint64_t> request_id) {
  LocalDOMWindow* window = local_frame_->DomWindow();
  DCHECK(window);

  if (!CanRegisterAttributionInContext(local_frame_, element, request_id,
                                       context,
                                       /*log_issues=*/true))
    return false;

  scoped_refptr<const SecurityOrigin> reporting_origin =
      SecurityOrigin::Create(url);
  if (!reporting_origin->IsPotentiallyTrustworthy()) {
    LogAuditIssue(
        context == RegisterContext::kAttributionSrc
            ? AttributionReportingIssueType::
                  kAttributionSourceUntrustworthyOrigin
            : AttributionReportingIssueType::kAttributionUntrustworthyOrigin,
        reporting_origin->ToString(), element, request_id);
    return false;
  }

  UseCounter::Count(window, mojom::blink::WebFeature::kConversionAPIAll);

  // Only record the ads APIs counter if enabled in that manner.
  if (RuntimeEnabledFeatures::PrivacySandboxAdsAPIsEnabled(window)) {
    UseCounter::Count(window, mojom::blink::WebFeature::kPrivacySandboxAdsAPIs);
  }

  return true;
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

  // Only handle requests which are attempting to invoke the API.
  if (!response.HttpHeaderFields().Contains(
          http_names::kAttributionReportingRegisterSource) &&
      !response.HttpHeaderFields().Contains(
          http_names::kAttributionReportingRegisterTrigger)) {
    return false;
  }

  if (!UrlCanRegisterAttribution(RegisterContext::kResource,
                                 response.CurrentRequestUrl(),
                                 /*element=*/nullptr, request.InspectorId())) {
    return false;
  }

  SrcType src_type = SrcType::kUndetermined;

  // Determine eligibility for this registration by considering first request
  // for a resource (even if `response` is for a redirect). This indicates
  // whether the redirect chain was configured for eligibility.
  // https://github.com/WICG/attribution-reporting-api/blob/main/EVENT.md#registering-attribution-sources
  const AtomicString& header_value =
      resource->GetResourceRequest().HttpHeaderField(
          http_names::kAttributionReportingEligible);

  if (header_value.IsNull()) {
    // All subresources are eligible to register triggers if they do *not*
    // specify the header.
    src_type = SrcType::kTrigger;
  } else {
    absl::optional<net::structured_headers::Dictionary> dict =
        net::structured_headers::ParseDictionary(
            StringUTF8Adaptor(header_value).AsStringPiece());
    if (!dict)
      return false;

    const bool allows_event_source =
        dict->contains(kAttributionEligibleEventSource);
    const bool allows_navigation_source =
        dict->contains(kAttributionEligibleNavigationSource);
    const bool allows_trigger = dict->contains(kAttributionEligibleTrigger);

    // TODO(johnidel): Consider logging a devtools issue here for early exits.
    if (allows_navigation_source) {
      return false;
    } else if (allows_event_source && allows_trigger) {
      // We use an undetermined SrcType which indicates either a source or
      // trigger may be registered.
      src_type = SrcType::kUndetermined;
    } else if (allows_event_source) {
      src_type = SrcType::kSource;
    } else if (allows_trigger) {
      src_type = SrcType::kTrigger;
    } else {
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
  client->HandleResponseHeaders(response, resource->InspectorId());
  return true;
}

void AttributionSrcLoader::RegisterTrigger(
    mojom::blink::AttributionTriggerDataPtr trigger_data) const {
  mojo::AssociatedRemote<mojom::blink::ConversionHost> conversion_host;
  local_frame_->GetRemoteNavigationAssociatedInterfaces()->GetInterface(
      &conversion_host);

  mojo::Remote<mojom::blink::AttributionDataHost> data_host;
  conversion_host->RegisterDataHost(data_host.BindNewPipeAndPassReceiver());
  data_host->TriggerDataAvailable(std::move(trigger_data));
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

  bool can_process_source =
      type_ == SrcType::kUndetermined || type_ == SrcType::kSource;
  if (can_process_source &&
      headers.Contains(http_names::kAttributionReportingRegisterSource)) {
    type_ = SrcType::kSource;
    HandleSourceRegistration(response, request_id);
    return;
  }

  // TODO(johnidel): Consider surfacing an error when source and trigger headers
  // are present together.
  bool can_process_trigger =
      type_ == SrcType::kUndetermined || type_ == SrcType::kTrigger;
  if (can_process_trigger &&
      headers.Contains(http_names::kAttributionReportingRegisterTrigger)) {
    type_ = SrcType::kTrigger;
    HandleTriggerRegistration(response);
  }
}

void AttributionSrcLoader::ResourceClient::HandleSourceRegistration(
    const ResourceResponse& response,
    uint64_t request_id) {
  DCHECK_EQ(type_, SrcType::kSource);

  mojom::blink::AttributionSourceDataPtr source_data =
      mojom::blink::AttributionSourceData::New();

  // Verify the current url is trustworthy and capable of registering sources.
  scoped_refptr<const SecurityOrigin> reporting_origin =
      SecurityOrigin::Create(response.CurrentRequestUrl());
  if (!reporting_origin->IsPotentiallyTrustworthy())
    return;
  source_data->reporting_origin = std::move(reporting_origin);

  const AtomicString& source_json =
      response.HttpHeaderField(http_names::kAttributionReportingRegisterSource);

  if (!attribution_response_parsing::ParseSourceRegistrationHeader(
          source_json, *source_data)) {
    loader_->LogAuditIssue(AttributionReportingIssueType::kInvalidHeader,
                           source_json,
                           /*element=*/nullptr, request_id);
    return;
  }

  data_host_->SourceDataAvailable(std::move(source_data));
}

void AttributionSrcLoader::ResourceClient::HandleTriggerRegistration(
    const ResourceResponse& response) {
  DCHECK_EQ(type_, SrcType::kTrigger);

  // TODO(apaseltiner): Report DevTools issue(s) if this fails.
  mojom::blink::AttributionTriggerDataPtr trigger_data =
      attribution_response_parsing::ParseAttributionTriggerData(response);

  if (!trigger_data)
    return;

  data_host_->TriggerDataAvailable(std::move(trigger_data));
}

void AttributionSrcLoader::LogAuditIssue(
    AttributionReportingIssueType issue_type,
    const absl::optional<String>& string,
    HTMLElement* element,
    absl::optional<uint64_t> request_id) {
  MaybeLogAuditIssue(local_frame_, issue_type, string, element, request_id);
}

}  // namespace blink
