// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/attribution_src_loader.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/referrer_policy.mojom-blink.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom-blink.h"
#include "third_party/blink/public/mojom/conversions/conversions.mojom-blink.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/public/platform/web_impression.h"
#include "third_party/blink/renderer/core/frame/attribution_response_parsing.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
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
#include "third_party/blink/renderer/platform/weborigin/referrer.h"
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

bool ContainsTriggerHeaders(const HTTPHeaderMap& headers) {
  return headers.Contains(
             http_names::kAttributionReportingRegisterEventTrigger) ||
         (headers.Contains(
              http_names::
                  kAttributionReportingRegisterAggregatableTriggerData) &&
          headers.Contains(
              http_names::kAttributionReportingRegisterAggregatableValues));
}

void RecordAttributionSrcRequestStatus(AttributionSrcRequestStatus status) {
  base::UmaHistogramEnumeration("Conversions.AttributionSrcRequestStatus",
                                status);
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

 private:
  void HandleResponseHeaders(const ResourceResponse& response);
  void HandleSourceRegistration(const ResourceResponse& response);
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

void AttributionSrcLoader::Register(const KURL& src_url,
                                    HTMLImageElement* element) {
  RegisterResult result;
  CreateAndSendRequest(src_url, element, SrcType::kUndetermined,
                       /*associated_with_navigation=*/false, result);
}

AttributionSrcLoader::RegisterResult AttributionSrcLoader::RegisterSources(
    const KURL& src_url) {
  RegisterResult result;
  CreateAndSendRequest(src_url, /*element=*/nullptr, SrcType::kSource,
                       /*associated_with_navigation=*/false, result);
  return result;
}

absl::optional<WebImpression> AttributionSrcLoader::RegisterNavigation(
    const KURL& src_url) {
  // TODO(apaseltiner): Add tests to ensure that this method can't be used to
  // register triggers.
  RegisterResult result;
  ResourceClient* client =
      CreateAndSendRequest(src_url, nullptr, SrcType::kSource,
                           /*associated_with_navigation=*/true, result);
  if (!client)
    return absl::nullopt;

  DCHECK(client->attribution_src_token());
  blink::WebImpression source;
  source.attribution_src_token = client->attribution_src_token();
  return source;
}

AttributionSrcLoader::ResourceClient*
AttributionSrcLoader::CreateAndSendRequest(
    const KURL& src_url,
    HTMLElement* element,
    SrcType src_type,
    bool associated_with_navigation,
    RegisterResult& out_register_result) {
  // Detached frames cannot/should not register new attributionsrcs.
  if (!local_frame_->IsAttached()) {
    out_register_result = RegisterResult::kFailedToRegister;
    return nullptr;
  }

  if (num_resource_clients_ >= kMaxConcurrentRequests) {
    out_register_result = RegisterResult::kFailedToRegister;
    return nullptr;
  }

  if (!src_url.ProtocolIsInHTTPFamily()) {
    out_register_result = RegisterResult::kInvalidProtocol;
    return nullptr;
  }

  if (RegisterResult result =
          CanRegisterAttribution(RegisterContext::kAttributionSrc, src_url,
                                 element, /*request_id=*/absl::nullopt);
      result != RegisterResult::kSuccess) {
    out_register_result = result;
    return nullptr;
  }

  LocalDOMWindow* window = local_frame_->DomWindow();
  Document* document = window->document();

  if (document->IsPrerendering()) {
    document->AddPostPrerenderingActivationStep(
        WTF::Bind(&AttributionSrcLoader::DoPrerenderingRegistration,
                  WrapPersistentIfNeeded(this), src_url, src_type,
                  associated_with_navigation));
    out_register_result = RegisterResult::kSuccess;
    return nullptr;
  }

  out_register_result = RegisterResult::kSuccess;
  return DoRegistration(src_url, src_type, associated_with_navigation);
}

AttributionSrcLoader::ResourceClient* AttributionSrcLoader::DoRegistration(
    const KURL& src_url,
    SrcType src_type,
    bool associated_with_navigation) {
  if (!local_frame_->IsAttached())
    return nullptr;

  ResourceRequest request(src_url);
  request.SetHttpMethod(http_names::kGET);

  request.SetKeepalive(true);
  request.SetReferrerString(Referrer::NoReferrer());
  request.SetReferrerPolicy(network::mojom::ReferrerPolicy::kNever);
  request.SetRequestContext(mojom::blink::RequestContextType::ATTRIBUTION_SRC);
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

AttributionSrcLoader::RegisterResult
AttributionSrcLoader::CanRegisterAttribution(
    RegisterContext context,
    const KURL& url,
    HTMLElement* element,
    const absl::optional<String>& request_id) {
  LocalDOMWindow* window = local_frame_->DomWindow();
  DCHECK(window);

  if (!RuntimeEnabledFeatures::AttributionReportingEnabled(window))
    return RegisterResult::kNotAllowed;

  const bool feature_policy_enabled = window->IsFeatureEnabled(
      mojom::blink::PermissionsPolicyFeature::kAttributionReporting);

  if (!feature_policy_enabled) {
    LogAuditIssue(AttributionReportingIssueType::kPermissionPolicyDisabled, "",
                  element, request_id);
    return RegisterResult::kNotAllowed;
  }

  // The API is only allowed in secure contexts.
  if (!window->IsSecureContext()) {
    LogAuditIssue(
        context == RegisterContext::kAttributionSrc
            ? AttributionReportingIssueType::
                  kAttributionSourceUntrustworthyOrigin
            : AttributionReportingIssueType::kAttributionUntrustworthyOrigin,
        local_frame_->GetSecurityContext()->GetSecurityOrigin()->ToString(),
        element, request_id);
    return RegisterResult::kInsecureContext;
  }

  scoped_refptr<const SecurityOrigin> reporting_origin =
      SecurityOrigin::Create(url);
  if (!reporting_origin->IsPotentiallyTrustworthy()) {
    LogAuditIssue(
        context == RegisterContext::kAttributionSrc
            ? AttributionReportingIssueType::
                  kAttributionSourceUntrustworthyOrigin
            : AttributionReportingIssueType::kAttributionUntrustworthyOrigin,
        reporting_origin->ToString(), element, request_id);
    return RegisterResult::kUntrustworthyOrigin;
  }

  UseCounter::Count(window, mojom::blink::WebFeature::kConversionAPIAll);

  // Only record the ads APIs counter if enabled in that manner.
  if (RuntimeEnabledFeatures::PrivacySandboxAdsAPIsEnabled(window)) {
    UseCounter::Count(window, mojom::blink::WebFeature::kPrivacySandboxAdsAPIs);
  }

  return RegisterResult::kSuccess;
}

void AttributionSrcLoader::MaybeRegisterTrigger(
    const ResourceRequest& request,
    const ResourceResponse& response) {
  if (request.GetRequestContext() ==
      mojom::blink::RequestContextType::ATTRIBUTION_SRC) {
    return;
  }

  if (!ContainsTriggerHeaders(response.HttpHeaderFields()))
    return;

  if (CanRegisterAttribution(
          RegisterContext::kResourceTrigger, response.CurrentRequestUrl(),
          /*element=*/nullptr,
          IdentifiersFactory::SubresourceRequestId(request.InspectorId())) !=
      RegisterResult::kSuccess) {
    return;
  }

  mojom::blink::AttributionTriggerDataPtr trigger_data =
      attribution_response_parsing::ParseAttributionTriggerData(response);

  if (!trigger_data)
    return;

  LocalDOMWindow* window = local_frame_->DomWindow();
  DCHECK(window);
  Document* document = window->document();
  DCHECK(document);

  if (document->IsPrerendering()) {
    document->AddPostPrerenderingActivationStep(
        WTF::Bind(&AttributionSrcLoader::RegisterTrigger,
                  WrapWeakPersistent(this), std::move(trigger_data)));
  } else {
    RegisterTrigger(std::move(trigger_data));
  }
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
  HandleResponseHeaders(response);
}

bool AttributionSrcLoader::ResourceClient::RedirectReceived(
    Resource* resource,
    const ResourceRequest& request,
    const ResourceResponse& response) {
  HandleResponseHeaders(response);
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

  keep_alive_.Clear();
}

void AttributionSrcLoader::ResourceClient::HandleResponseHeaders(
    const ResourceResponse& response) {
  const auto& headers = response.HttpHeaderFields();

  bool can_process_source =
      type_ == SrcType::kUndetermined || type_ == SrcType::kSource;
  if (can_process_source &&
      headers.Contains(http_names::kAttributionReportingRegisterSource)) {
    type_ = SrcType::kSource;
    HandleSourceRegistration(response);
    return;
  }

  // TODO(johnidel): Consider surfacing an error when source and trigger headers
  // are present together.
  bool can_process_trigger =
      type_ == SrcType::kUndetermined || type_ == SrcType::kTrigger;
  if (can_process_trigger && ContainsTriggerHeaders(headers)) {
    type_ = SrcType::kTrigger;
    HandleTriggerRegistration(response);
  }

  // TODO(johnidel): Add parsing for trigger and filter headers.
}

void AttributionSrcLoader::ResourceClient::HandleSourceRegistration(
    const ResourceResponse& response) {
  DCHECK_EQ(type_, SrcType::kSource);

  mojom::blink::AttributionSourceDataPtr source_data =
      mojom::blink::AttributionSourceData::New();

  // Verify the current url is trustworthy and capable of registering sources.
  scoped_refptr<const SecurityOrigin> reporting_origin =
      SecurityOrigin::Create(response.CurrentRequestUrl());
  if (!reporting_origin->IsPotentiallyTrustworthy())
    return;
  source_data->reporting_origin = std::move(reporting_origin);

  if (!attribution_response_parsing::ParseSourceRegistrationHeader(
          response.HttpHeaderField(
              http_names::kAttributionReportingRegisterSource),
          *source_data)) {
    return;
  }

  source_data->aggregatable_source =
      mojom::blink::AttributionAggregatableSource::New();

  const AtomicString& aggregatable_source_json = response.HttpHeaderField(
      http_names::kAttributionReportingRegisterAggregatableSource);
  if (!aggregatable_source_json.IsNull() &&
      !attribution_response_parsing::ParseAttributionAggregatableSource(
          aggregatable_source_json, *source_data->aggregatable_source)) {
    return;
  }

  data_host_->SourceDataAvailable(std::move(source_data));
}

void AttributionSrcLoader::ResourceClient::HandleTriggerRegistration(
    const ResourceResponse& response) {
  DCHECK_EQ(type_, SrcType::kTrigger);

  mojom::blink::AttributionTriggerDataPtr trigger_data =
      attribution_response_parsing::ParseAttributionTriggerData(response);

  if (!trigger_data)
    return;

  data_host_->TriggerDataAvailable(std::move(trigger_data));
}

void AttributionSrcLoader::LogAuditIssue(
    AttributionReportingIssueType issue_type,
    const String& string,
    HTMLElement* element,
    const absl::optional<String>& request_id) {
  if (!local_frame_->IsAttached())
    return;
  AuditsIssue::ReportAttributionIssue(local_frame_->DomWindow(), issue_type,
                                      local_frame_->GetDevToolsFrameToken(),
                                      element, request_id, string);
}

}  // namespace blink
