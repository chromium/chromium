// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/attribution_src_loader.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/referrer_policy.mojom-blink.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom-blink.h"
#include "third_party/blink/public/mojom/conversions/conversions.mojom-blink.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/attribution_response_parsing.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
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

// Represents what events are able to be registered from an attributionsrc.
enum class AttributionSrcType { kUndetermined, kSource, kTrigger };

}  // namespace

class AttributionSrcLoader::ResourceClient
    : public GarbageCollected<AttributionSrcLoader::ResourceClient>,
      public RawResourceClient {
 public:
  explicit ResourceClient(AttributionSrcLoader* loader) : loader_(loader) {
    DCHECK(loader_);
    DCHECK(loader_->local_frame_);
    DCHECK(loader_->local_frame_->IsAttached());

    mojo::AssociatedRemote<mojom::blink::ConversionHost> conversion_host;
    loader_->local_frame_->GetRemoteNavigationAssociatedInterfaces()
        ->GetInterface(&conversion_host);
    conversion_host->RegisterDataHost(data_host_.BindNewPipeAndPassReceiver());
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
  AttributionSrcType type_ = AttributionSrcType::kUndetermined;

  // Remote used for registering responses with the browser-process.
  mojo::Remote<mojom::blink::AttributionDataHost> data_host_;
};

AttributionSrcLoader::AttributionSrcLoader(LocalFrame* frame)
    : local_frame_(frame) {
  DCHECK(local_frame_);
}

AttributionSrcLoader::~AttributionSrcLoader() = default;

void AttributionSrcLoader::Trace(Visitor* visitor) const {
  visitor->Trace(local_frame_);
  visitor->Trace(resource_clients_);
}

AttributionSrcLoader::RegisterResult AttributionSrcLoader::Register(
    const KURL& src_url,
    HTMLImageElement* element) {
  // Detached frames cannot/should not register new attributionsrcs.
  if (!local_frame_->IsAttached())
    return RegisterResult::kSuccess;

  if (!src_url.ProtocolIsInHTTPFamily())
    return RegisterResult::kInvalidProtocol;

  LocalDOMWindow* window = local_frame_->DomWindow();
  Document* document = window->document();

  if (!RuntimeEnabledFeatures::ConversionMeasurementEnabled(window))
    return RegisterResult::kNotAllowed;

  const bool feature_policy_enabled = window->IsFeatureEnabled(
      mojom::blink::PermissionsPolicyFeature::kAttributionReporting);

  if (!feature_policy_enabled) {
    LogAuditIssue(AttributionReportingIssueType::kPermissionPolicyDisabled, "",
                  element);
    return RegisterResult::kNotAllowed;
  }

  // The API is only allowed in secure contexts.
  if (!window->IsSecureContext()) {
    LogAuditIssue(
        AttributionReportingIssueType::kAttributionSourceUntrustworthyOrigin,
        local_frame_->GetSecurityContext()->GetSecurityOrigin()->ToString(),
        element);
    return RegisterResult::kInsecureContext;
  }

  auto reporting_origin = SecurityOrigin::CreateFromString(src_url);

  if (!reporting_origin->IsPotentiallyTrustworthy()) {
    LogAuditIssue(
        AttributionReportingIssueType::kAttributionSourceUntrustworthyOrigin,
        src_url.GetString(), element);
    return RegisterResult::kUntrustworthyOrigin;
  }

  if (document->IsPrerendering()) {
    document->AddPostPrerenderingActivationStep(
        WTF::Bind(&AttributionSrcLoader::DoRegistration,
                  WrapPersistentIfNeeded(this), src_url));
  } else {
    DoRegistration(src_url);
  }

  return RegisterResult::kSuccess;
}

void AttributionSrcLoader::DoRegistration(const KURL& src_url) {
  if (!local_frame_->IsAttached())
    return;

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

  auto* client = MakeGarbageCollected<ResourceClient>(this);
  resource_clients_.insert(client);
  RawResource::Fetch(params, local_frame_->DomWindow()->Fetcher(), client);
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

  DCHECK(loader_->resource_clients_.Contains(this));
  loader_->resource_clients_.erase(this);
}

void AttributionSrcLoader::ResourceClient::HandleResponseHeaders(
    const ResourceResponse& response) {
  const auto& headers = response.HttpHeaderFields();

  bool can_process_source = type_ == AttributionSrcType::kUndetermined ||
                            type_ == AttributionSrcType::kSource;
  if (can_process_source &&
      headers.Contains(http_names::kAttributionReportingRegisterSource)) {
    type_ = AttributionSrcType::kSource;
    HandleSourceRegistration(response);
    return;
  }

  // TODO(johnidel): Consider surfacing an error when source and trigger headers
  // are present together.
  bool can_process_trigger = type_ == AttributionSrcType::kUndetermined ||
                             type_ == AttributionSrcType::kTrigger;
  if (can_process_trigger &&
      (headers.Contains(
           http_names::kAttributionReportingRegisterEventTrigger) ||
       (headers.Contains(
            http_names::kAttributionReportingRegisterAggregatableTriggerData) &&
        headers.Contains(
            http_names::kAttributionReportingRegisterAggregatableValues)))) {
    type_ = AttributionSrcType::kTrigger;
    HandleTriggerRegistration(response);
  }

  // TODO(johnidel): Add parsing for trigger and filter headers.
}

void AttributionSrcLoader::ResourceClient::HandleSourceRegistration(
    const ResourceResponse& response) {
  DCHECK_EQ(type_, AttributionSrcType::kSource);

  mojom::blink::AttributionSourceDataPtr source_data =
      mojom::blink::AttributionSourceData::New();

  // Verify the current url is trustworthy and capable of registering sources.
  scoped_refptr<const SecurityOrigin> reporting_origin =
      SecurityOrigin::CreateFromString(response.CurrentRequestUrl());
  if (!reporting_origin->IsPotentiallyTrustworthy())
    return;
  source_data->reporting_origin =
      SecurityOrigin::Create(response.CurrentRequestUrl());

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
  DCHECK_EQ(type_, AttributionSrcType::kTrigger);

  mojom::blink::AttributionTriggerDataPtr trigger_data =
      mojom::blink::AttributionTriggerData::New();

  // Verify the current url is trustworthy and capable of registering triggers.
  scoped_refptr<const SecurityOrigin> reporting_origin =
      SecurityOrigin::CreateFromString(response.CurrentRequestUrl());
  if (!reporting_origin->IsPotentiallyTrustworthy())
    return;
  trigger_data->reporting_origin =
      SecurityOrigin::Create(response.CurrentRequestUrl());

  // Populate event triggers.
  const AtomicString& event_triggers_json = response.HttpHeaderField(
      http_names::kAttributionReportingRegisterEventTrigger);
  if (!event_triggers_json.IsNull() &&
      !attribution_response_parsing::ParseEventTriggerData(
          event_triggers_json, trigger_data->event_triggers)) {
    return;
  }

  trigger_data->filters = mojom::blink::AttributionFilterData::New();

  const AtomicString& filter_json =
      response.HttpHeaderField(http_names::kAttributionReportingFilters);
  if (!filter_json.IsNull() && !attribution_response_parsing::ParseFilters(
                                   filter_json, *trigger_data->filters)) {
    return;
  }

  trigger_data->aggregatable_trigger =
      mojom::blink::AttributionAggregatableTrigger::New();

  const AtomicString& aggregatable_trigger_json = response.HttpHeaderField(
      http_names::kAttributionReportingRegisterAggregatableTriggerData);
  if (!aggregatable_trigger_json.IsNull() &&
      !attribution_response_parsing::ParseAttributionAggregatableTriggerData(
          aggregatable_trigger_json,
          trigger_data->aggregatable_trigger->trigger_data)) {
    return;
  }

  const AtomicString& aggregatable_values_json = response.HttpHeaderField(
      http_names::kAttributionReportingRegisterAggregatableValues);
  if (!aggregatable_values_json.IsNull() &&
      !attribution_response_parsing::ParseAttributionAggregatableValues(
          aggregatable_values_json,
          trigger_data->aggregatable_trigger->values)) {
    return;
  }

  trigger_data->debug_key =
      attribution_response_parsing::ParseDebugKey(response.HttpHeaderField(
          http_names::kAttributionReportingTriggerDebugKey));

  data_host_->TriggerDataAvailable(std::move(trigger_data));
}

void AttributionSrcLoader::LogAuditIssue(
    AttributionReportingIssueType issue_type,
    const String& string,
    HTMLElement* element) {
  if (!local_frame_->IsAttached())
    return;
  AuditsIssue::ReportAttributionIssue(local_frame_->DomWindow(), issue_type,
                                      local_frame_->GetDevToolsFrameToken(),
                                      element, absl::nullopt, string);
}

}  // namespace blink
