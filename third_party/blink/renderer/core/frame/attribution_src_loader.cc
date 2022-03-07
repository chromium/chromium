// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/attribution_src_loader.h"

#include <utility>

#include "base/memory/scoped_refptr.h"
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

bool IsResponseParseError(
    attribution_response_parsing::ResponseParseStatus status) {
  switch (status) {
    case attribution_response_parsing::ResponseParseStatus::kSuccess:
    case attribution_response_parsing::ResponseParseStatus::kNotFound:
      return false;
    case attribution_response_parsing::ResponseParseStatus::kParseError:
    case attribution_response_parsing::ResponseParseStatus::kInvalidFormat:
      return true;
  }
}

}  // namespace

AttributionSrcLoader::AttributionSrcLoader(LocalFrame* frame)
    : local_frame_(frame) {}

AttributionSrcLoader::~AttributionSrcLoader() = default;

void AttributionSrcLoader::Register(const KURL& src_url,
                                    HTMLImageElement* element) {
  // Detached frames cannot/should not register new attributionsrcs.
  if (!local_frame_)
    return;

  if (!src_url.ProtocolIsInHTTPFamily())
    return;

  ExecutionContext* execution_context =
      local_frame_->GetDocument()->GetExecutionContext();
  if (!RuntimeEnabledFeatures::ConversionMeasurementEnabled(
          execution_context)) {
    return;
  }

  const bool feature_policy_enabled = execution_context->IsFeatureEnabled(
      mojom::blink::PermissionsPolicyFeature::kAttributionReporting);

  if (!feature_policy_enabled) {
    LogAuditIssue(AttributionReportingIssueType::kPermissionPolicyDisabled, "",
                  element);
    return;
  }

  // The API is only allowed in secure contexts.
  if (!execution_context->IsSecureContext()) {
    LogAuditIssue(
        AttributionReportingIssueType::kAttributionSourceUntrustworthyOrigin,
        local_frame_->GetSecurityContext()->GetSecurityOrigin()->ToString(),
        element);
    return;
  }

  auto reporting_origin = SecurityOrigin::CreateFromString(src_url);

  if (!reporting_origin->IsPotentiallyTrustworthy()) {
    LogAuditIssue(
        AttributionReportingIssueType::kAttributionSourceUntrustworthyOrigin,
        src_url.GetString(), element);
    return;
  }

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

  Resource* resource =
      RawResource::Fetch(params, local_frame_->DomWindow()->Fetcher(), this);
  if (!resource)
    return;

  mojo::Remote<mojom::blink::AttributionDataHost> data_host;

  mojo::AssociatedRemote<mojom::blink::ConversionHost> conversion_host;
  local_frame_->GetRemoteNavigationAssociatedInterfaces()->GetInterface(
      &conversion_host);
  conversion_host->RegisterDataHost(data_host.BindNewPipeAndPassReceiver());
  resource_context_map_.insert(
      resource, AttributionSrcContext{.type = AttributionSrcType::kUndetermined,
                                      .data_host = std::move(data_host)});
}

void AttributionSrcLoader::Shutdown() {
  local_frame_ = nullptr;
}

void AttributionSrcLoader::ResponseReceived(Resource* resource,
                                            const ResourceResponse& response) {
  HandleResponseHeaders(resource, response);
}

bool AttributionSrcLoader::RedirectReceived(Resource* resource,
                                            const ResourceRequest& request,
                                            const ResourceResponse& response) {
  HandleResponseHeaders(resource, response);
  return true;
}

void AttributionSrcLoader::NotifyFinished(Resource* resource) {
  DCHECK(resource_context_map_.Contains(resource));
  resource_context_map_.erase(resource);
}

void AttributionSrcLoader::HandleResponseHeaders(
    Resource* resource,
    const ResourceResponse& response) {
  auto it = resource_context_map_.find(resource);
  if (it == resource_context_map_.end())
    return;

  AttributionSrcContext& context = it->value;

  const auto& headers = response.HttpHeaderFields();

  bool can_process_source = context.type == AttributionSrcType::kUndetermined ||
                            context.type == AttributionSrcType::kSource;
  if (can_process_source &&
      headers.Contains(http_names::kAttributionReportingRegisterSource)) {
    context.type = AttributionSrcType::kSource;
    HandleSourceRegistration(resource, response, context);
    return;
  }

  // TODO(johnidel): Consider surfacing an error when source and trigger headers
  // are present together.
  bool can_process_trigger =
      context.type == AttributionSrcType::kUndetermined ||
      context.type == AttributionSrcType::kTrigger;
  if (can_process_trigger &&
      (headers.Contains(
           http_names::kAttributionReportingRegisterEventTrigger) ||
       (headers.Contains(
            http_names::kAttributionReportingRegisterAggregatableTriggerData) &&
        headers.Contains(
            http_names::kAttributionReportingRegisterAggregatableValues)))) {
    context.type = AttributionSrcType::kTrigger;
    HandleTriggerRegistration(resource, response, context);
  }

  // TODO(johnidel): Add parsing for trigger and filter headers.
}

void AttributionSrcLoader::HandleSourceRegistration(
    Resource* resource,
    const ResourceResponse& response,
    AttributionSrcContext& context) {
  auto it = resource_context_map_.find(resource);
  DCHECK_NE(it, resource_context_map_.end());

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

  const AtomicString& aggregatable_sources_json = response.HttpHeaderField(
      http_names::kAttributionReportingRegisterAggregatableSource);
  auto aggregatable_sources =
      attribution_response_parsing::ParseAttributionAggregatableSources(
          aggregatable_sources_json);
  if (IsResponseParseError(aggregatable_sources.status))
    return;

  source_data->aggregatable_sources = std::move(aggregatable_sources.value);

  context.data_host->SourceDataAvailable(std::move(source_data));
}

void AttributionSrcLoader::HandleTriggerRegistration(
    Resource* resource,
    const ResourceResponse& response,
    AttributionSrcContext& context) {
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

  const AtomicString& aggregatable_trigger_json = response.HttpHeaderField(
      http_names::kAttributionReportingRegisterAggregatableTriggerData);
  auto aggregatable_trigger =
      attribution_response_parsing::ParseAttributionAggregatableTrigger(
          aggregatable_trigger_json);
  if (IsResponseParseError(aggregatable_trigger.status))
    return;

  trigger_data->aggregatable_trigger = std::move(aggregatable_trigger.value);

  const AtomicString& aggregatable_values_json = response.HttpHeaderField(
      http_names::kAttributionReportingRegisterAggregatableValues);
  auto aggregatable_values =
      attribution_response_parsing::ParseAttributionAggregatableValues(
          aggregatable_values_json);
  if (IsResponseParseError(aggregatable_values.status))
    return;

  trigger_data->aggregatable_values = std::move(aggregatable_values.value);

  trigger_data->filters = mojom::blink::AttributionFilterData::New();

  const AtomicString& filter_json =
      response.HttpHeaderField(http_names::kAttributionReportingFilters);
  if (!filter_json.IsNull() && !attribution_response_parsing::ParseFilters(
                                   filter_json, *trigger_data->filters)) {
    return;
  }

  context.data_host->TriggerDataAvailable(std::move(trigger_data));
}

void AttributionSrcLoader::LogAuditIssue(
    AttributionReportingIssueType issue_type,
    const String& string,
    HTMLElement* element) {
  if (!local_frame_)
    return;
  AuditsIssue::ReportAttributionIssue(local_frame_->DomWindow(), issue_type,
                                      local_frame_->GetDevToolsFrameToken(),
                                      element, absl::nullopt, string);
}

}  // namespace blink
