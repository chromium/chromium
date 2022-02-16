// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/attribution_src_loader.h"

#include <utility>

#include "base/time/time.h"
#include "services/network/public/mojom/referrer_policy.mojom-blink.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom-blink.h"
#include "third_party/blink/public/mojom/conversions/conversions.mojom-blink.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/platform/json/json_parser.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
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
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

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
  resource_data_host_map_.insert(resource, std::move(data_host));
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
  DCHECK(resource_data_host_map_.Contains(resource));
  resource_data_host_map_.erase(resource);
}

void AttributionSrcLoader::HandleResponseHeaders(
    Resource* resource,
    const ResourceResponse& response) {
  if (!resource_data_host_map_.Contains(resource))
    return;

  const auto& headers = response.HttpHeaderFields();
  if (headers.Contains(http_names::kAttributionReportingRegisterSource))
    HandleSourceRegistration(resource, response);

  // TODO(johnidel): Add parsing for trigger and filter headers.
}

void AttributionSrcLoader::HandleSourceRegistration(
    Resource* resource,
    const ResourceResponse& response) {
  auto it = resource_data_host_map_.find(resource);
  DCHECK_NE(it, resource_data_host_map_.end());

  mojom::blink::AttributionSourceDataPtr source_data =
      mojom::blink::AttributionSourceData::New();

  // Verify the current url is trustworthy and capable of registering sources.
  scoped_refptr<const SecurityOrigin> reporting_origin =
      SecurityOrigin::CreateFromString(response.CurrentRequestUrl());
  if (!reporting_origin->IsPotentiallyTrustworthy())
    return;
  source_data->reporting_origin =
      SecurityOrigin::Create(response.CurrentRequestUrl());

  // Populate attribution data from provided JSON.
  std::unique_ptr<JSONValue> json = ParseJSON(response.HttpHeaderField(
      http_names::kAttributionReportingRegisterSource));

  // TODO(johnidel): Log a devtools issues if JSON parsing fails and on
  // individual early exits below.
  if (!json)
    return;

  JSONObject* object = JSONObject::Cast(json.get());
  if (!object)
    return;

  String event_id_string;
  if (!object->GetString("source_event_id", &event_id_string))
    return;
  bool event_id_is_valid = false;
  uint64_t event_id = event_id_string.ToUInt64Strict(&event_id_is_valid);

  // For source registrations where there is no mechanism to raise an error,
  // such as on an img element, it is more useful to log the source with
  // default data so that a reporting origin can learn the failure mode.
  source_data->source_event_id = event_id_is_valid ? event_id : 0;

  String destination_string;
  if (!object->GetString("destination", &destination_string))
    return;
  scoped_refptr<const SecurityOrigin> destination =
      SecurityOrigin::CreateFromString(destination_string);
  if (!destination->IsPotentiallyTrustworthy())
    return;
  source_data->destination = std::move(destination);

  // Treat invalid expiry, priority, and debug key as if they were not set.
  String priority_string;
  if (object->GetString("source_priority", &priority_string)) {
    bool priority_is_valid = false;
    int64_t priority = priority_string.ToInt64Strict(&priority_is_valid);
    if (priority_is_valid)
      source_data->priority = priority;
  }

  String expiry_string;
  if (object->GetString("expiry", &expiry_string)) {
    bool expiry_is_valid = false;
    int64_t expiry = expiry_string.ToInt64Strict(&expiry_is_valid);
    if (expiry_is_valid)
      source_data->expiry = base::Seconds(expiry);
  }

  String debug_key_string;
  if (object->GetString("debug_key", &debug_key_string)) {
    bool debug_key_is_valid = false;
    uint64_t debug_key = debug_key_string.ToUInt64Strict(&debug_key_is_valid);
    if (debug_key_is_valid) {
      source_data->debug_key =
          mojom::blink::AttributionDebugKey::New(debug_key);
    }
  }

  it->value->SourceDataAvailable(std::move(source_data));
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
