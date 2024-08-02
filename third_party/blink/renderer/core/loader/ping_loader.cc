/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "third_party/blink/renderer/core/loader/ping_loader.h"

#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/core/fileapi/file.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/loader/beacon_data.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/core/url/url_search_params.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
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
#include "third_party/blink/renderer/platform/network/parsed_content_type.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

bool SendBeaconCommon(const ScriptState& state,
                      LocalFrame* frame,
                      const KURL& url,
                      const BeaconData& beacon) {
  if (!frame->DomWindow()
           ->GetContentSecurityPolicyForWorld(&state.World())
           ->AllowConnectToSource(url, url, RedirectStatus::kNoRedirect)) {
    // We're simulating a network failure here, so we return 'true'.
    return true;
  }

  ResourceRequest request(url);
  request.SetHttpMethod(http_names::kPOST);
  request.SetKeepalive(true);
  request.SetRequestContext(mojom::blink::RequestContextType::BEACON);
  beacon.Serialize(request);
  FetchParameters params(std::move(request),
                         ResourceLoaderOptions(&state.World()));
  // The spec says:
  //  - If mimeType is not null:
  //   - If mimeType value is a CORS-safelisted request-header value for the
  //     Content-Type header, set corsMode to "no-cors".
  // As we don't support requests with non CORS-safelisted Content-Type, the
  // mode should always be "no-cors".
  params.MutableOptions().initiator_info.name =
      fetch_initiator_type_names::kBeacon;

  frame->Client()->DidDispatchPingLoader(url);

  FetchUtils::LogFetchKeepAliveRequestMetric(
      params.GetResourceRequest().GetRequestContext(),
      FetchUtils::FetchKeepAliveRequestState::kTotal);
  Resource* resource =
      RawResource::Fetch(params, frame->DomWindow()->Fetcher(), nullptr);
  return resource->GetStatus() != ResourceStatus::kLoadError;
}

}  // namespace

// http://www.whatwg.org/specs/web-apps/current-work/multipage/links.html#hyperlink-auditing
void PingLoader::SendLinkAuditPing(LocalFrame* frame,
                                   const KURL& ping_url,
                                   const KURL& destination_url) {
  if (!ping_url.ProtocolIsInHTTPFamily())
    return;

  ResourceRequest request(ping_url);
  request.SetHttpMethod(http_names::kPOST);
  request.SetHTTPContentType(AtomicString("text/ping"));
  request.SetHttpBody(EncodedFormData::Create(base::span_from_cstring("PING")));
  request.SetHttpHeaderField(http_names::kCacheControl,
                             AtomicString("max-age=0"));
  request.SetHttpHeaderField(http_names::kPingTo,
                             AtomicString(destination_url.GetString()));
  scoped_refptr<const SecurityOrigin> ping_origin =
      SecurityOrigin::Create(ping_url);
  if (ProtocolIs(frame->DomWindow()->Url().GetString(), "http") ||
      frame->DomWindow()->GetSecurityOrigin()->CanAccess(ping_origin.get())) {
    request.SetHttpHeaderField(
        http_names::kPingFrom,
        AtomicString(frame->DomWindow()->Url().GetString()));
  }

  request.SetKeepalive(true);
  request.SetReferrerString(Referrer::NoReferrer());
  request.SetReferrerPolicy(network::mojom::ReferrerPolicy::kNever);
  request.SetRequestContext(mojom::blink::RequestContextType::PING);
  FetchParameters params(
      std::move(request),
      ResourceLoaderOptions(frame->DomWindow()->GetCurrentWorld()));
  params.MutableOptions().initiator_info.name =
      fetch_initiator_type_names::kPing;

  frame->Client()->DidDispatchPingLoader(ping_url);
  FetchUtils::LogFetchKeepAliveRequestMetric(
      params.GetResourceRequest().GetRequestContext(),
      FetchUtils::FetchKeepAliveRequestState::kTotal);
  RawResource::Fetch(params, frame->DomWindow()->Fetcher(), nullptr);
}

void PingLoader::SendViolationReport(ExecutionContext* execution_context,
                                     const KURL& report_url,
                                     scoped_refptr<EncodedFormData> report,
                                     bool is_frame_ancestors_violation) {
  ResourceRequest request(report_url);
  request.SetHttpMethod(http_names::kPOST);
  request.SetHTTPContentType(AtomicString("application/csp-report"));
  request.SetKeepalive(true);
  request.SetHttpBody(std::move(report));
  request.SetCredentialsMode(network::mojom::CredentialsMode::kSameOrigin);
  request.SetRequestContext(mojom::blink::RequestContextType::CSP_REPORT);
  request.SetRequestDestination(network::mojom::RequestDestination::kReport);

  // For frame-ancestors violations, execution_context->GetSecurityOrigin() is
  // the origin of the embedding frame, while violations should be sent by the
  // (blocked) embedded frame.
  if (is_frame_ancestors_violation) {
    request.SetRequestorOrigin(SecurityOrigin::CreateUniqueOpaque());
  } else {
    request.SetRequestorOrigin(execution_context->GetSecurityOrigin());
  }

  request.SetRedirectMode(network::mojom::RedirectMode::kError);
  FetchParameters params(
      std::move(request),
      ResourceLoaderOptions(execution_context->GetCurrentWorld()));
  params.MutableOptions().initiator_info.name =
      fetch_initiator_type_names::kViolationreport;

  auto* window = DynamicTo<LocalDOMWindow>(execution_context);
  if (window && window->GetFrame())
    window->GetFrame()->Client()->DidDispatchPingLoader(report_url);

  FetchUtils::LogFetchKeepAliveRequestMetric(
      params.GetResourceRequest().GetRequestContext(),
      FetchUtils::FetchKeepAliveRequestState::kTotal);
  RawResource::Fetch(params, execution_context->Fetcher(), nullptr);
}

bool PingLoader::SendBeacon(const ScriptState& state,
                            LocalFrame* frame,
                            const KURL& beacon_url,
                            const String& data) {
  BeaconString beacon(data);
  return SendBeaconCommon(state, frame, beacon_url, beacon);
}

bool PingLoader::SendBeacon(const ScriptState& state,
                            LocalFrame* frame,
                            const KURL& beacon_url,
                            DOMArrayBufferView* data) {
  BeaconDOMArrayBufferView beacon(data);
  return SendBeaconCommon(state, frame, beacon_url, beacon);
}

bool PingLoader::SendBeacon(const ScriptState& state,
                            LocalFrame* frame,
                            const KURL& beacon_url,
                            DOMArrayBuffer* data) {
  BeaconDOMArrayBuffer beacon(data);
  return SendBeaconCommon(state, frame, beacon_url, beacon);
}

bool PingLoader::SendBeacon(const ScriptState& state,
                            LocalFrame* frame,
                            const KURL& beacon_url,
                            URLSearchParams* data) {
  BeaconURLSearchParams beacon(data);
  return SendBeaconCommon(state, frame, beacon_url, beacon);
}

bool PingLoader::SendBeacon(const ScriptState& state,
                            LocalFrame* frame,
                            const KURL& beacon_url,
                            FormData* data) {
  BeaconFormData beacon(data);
  return SendBeaconCommon(state, frame, beacon_url, beacon);
}

bool PingLoader::SendBeacon(const ScriptState& state,
                            LocalFrame* frame,
                            const KURL& beacon_url,
                            Blob* data) {
  BeaconBlob beacon(data);
  return SendBeaconCommon(state, frame, beacon_url, beacon);
}

}  // namespace blink
