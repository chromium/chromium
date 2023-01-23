/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2012 Intel Inc. All rights reserved.
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
 */

#include "third_party/blink/renderer/core/timing/performance_resource_timing.h"

#include "base/notreached.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/timing/performance_mark_or_measure.mojom-blink.h"
#include "third_party/blink/public/mojom/timing/resource_timing.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/core/timing/performance_entry.h"
#include "third_party/blink/renderer/core/timing/performance_mark.h"
#include "third_party/blink/renderer/core/timing/performance_measure.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_timing.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_timing_info.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

// This method is used to populate ResourceTimingInfo member field values for
// navigation timing. See
// https://w3c.github.io/resource-timing/#dfn-setup-the-resource-timing-entry.
void PopulateResourceTimingInfo(ResourceTimingInfo* info,
                                LocalDOMWindow* window) {
  DCHECK(window);
  DCHECK(info);

  info->SetName(
      !info->FinalResponse().CurrentRequestUrl().GetString().empty()
          ? AtomicString(info->FinalResponse().CurrentRequestUrl().GetString())
          : g_empty_atom);

  DCHECK(window->document()->Loader());

  info->SetDeliveryType(
      window->document()->Loader()->GetNavigationDeliveryType(),
      info->CacheState());
  info->SetIsSecureTransport(base::Contains(url::GetSecureSchemes(),
                                            window->Url().Protocol().Ascii()));
  info->SetContextType(mojom::blink::RequestContextType::HYPERLINK);
  info->SetRequestDestination(network::mojom::RequestDestination::kDocument);

  info->SetAllowTimingDetails(true);

  info->SetDidReuseConnection(info->FinalResponse().ConnectionReused());

  info->SetAllowNegativeValue(false);

  info->SetIsSecureTransport(base::Contains(url::GetSecureSchemes(),
                                            window->Url().Protocol().Ascii()));

  info->SetEncodedBodySize(info->FinalResponse().EncodedBodyLength());

  info->SetDecodedBodySize(info->FinalResponse().DecodedBodyLength());

  info->SetAlpnNegotiatedProtocol(
      info->FinalResponse().AlpnNegotiatedProtocol());

  info->SetConnectionInfo(info->FinalResponse().ConnectionInfoString());
}

}  // namespace

PerformanceResourceTiming::PerformanceResourceTiming(
    const mojom::blink::ResourceTimingInfo& info,
    base::TimeTicks time_origin,
    bool cross_origin_isolated_capability,
    const AtomicString& initiator_type,
    LocalDOMWindow* source)
    : PerformanceEntry(AtomicString(info.name),
                       Performance::MonotonicTimeToDOMHighResTimeStamp(
                           time_origin,
                           info.start_time,
                           info.allow_negative_values,
                           cross_origin_isolated_capability),
                       Performance::MonotonicTimeToDOMHighResTimeStamp(
                           time_origin,
                           info.response_end,
                           info.allow_negative_values,
                           cross_origin_isolated_capability),
                       source),
      initiator_type_(initiator_type.empty()
                          ? fetch_initiator_type_names::kOther
                          : initiator_type),
      time_origin_(time_origin),
      cross_origin_isolated_capability_(cross_origin_isolated_capability),
      resource_timing_info_(ResourceTimingInfo::FromMojo(info)),
      resource_load_timing_(ResourceLoadTiming::FromMojo(info.timing.get())),
      server_timing_(
          PerformanceServerTiming::FromParsedServerTiming(info.server_timing)) {
}

// This constructor is for PerformanceNavigationTiming.
// The navigation_id for navigation timing is always 1.
PerformanceResourceTiming::PerformanceResourceTiming(
    ResourceTimingInfo& info,
    const AtomicString& initiator_type,
    base::TimeTicks time_origin,
    bool cross_origin_isolated_capability,
    HeapVector<Member<PerformanceServerTiming>> server_timing,
    LocalDOMWindow& source_window)
    : PerformanceEntry(
          !info.FinalResponse().CurrentRequestUrl().GetString().empty()
              ? AtomicString(
                    info.FinalResponse().CurrentRequestUrl().GetString())
              : g_empty_atom,
          0.0,
          0.0,
          &source_window),
      initiator_type_(initiator_type),
      time_origin_(time_origin),
      cross_origin_isolated_capability_(cross_origin_isolated_capability),
      resource_timing_info_(&info),
      resource_load_timing_(info.FinalResponse().GetResourceLoadTiming()),
      server_timing_(std::move(server_timing)) {
  PopulateResourceTimingInfo(resource_timing_info_.get(), &source_window);
}
PerformanceResourceTiming::~PerformanceResourceTiming() = default;

const AtomicString& PerformanceResourceTiming::entryType() const {
  return performance_entry_names::kResource;
}

PerformanceEntryType PerformanceResourceTiming::EntryTypeEnum() const {
  return PerformanceEntry::EntryType::kResource;
}

uint64_t PerformanceResourceTiming::GetTransferSize(
    uint64_t encoded_body_size,
    mojom::blink::CacheState cache_state) {
  switch (cache_state) {
    case mojom::blink::CacheState::kLocal:
      return 0;
    case mojom::blink::CacheState::kValidated:
      return kHeaderSize;
    case mojom::blink::CacheState::kNone:
      return encoded_body_size + kHeaderSize;
  }
  NOTREACHED();
  return 0;
}

AtomicString PerformanceResourceTiming::initiatorType() const {
  return initiator_type_;
}

AtomicString PerformanceResourceTiming::deliveryType() const {
  if (!Info()->AllowTimingDetails()) {
    return g_empty_atom;
  }
  return Info()->DeliveryType();
}

AtomicString PerformanceResourceTiming::renderBlockingStatus() const {
  switch (Info()->RenderBlockingStatus()) {
    case RenderBlockingStatusType::kBlocking:
      return "blocking";
    case RenderBlockingStatusType::kNonBlocking:
      return "non-blocking";
  }
  NOTREACHED();
  return "non-blocking";
}

AtomicString PerformanceResourceTiming::contentType() const {
  return Info()->ContentType();
}

uint16_t PerformanceResourceTiming::responseStatus() const {
  return Info()->ResponseStatus();
}

namespace {
bool IsDocumentDestination(mojom::blink::RequestContextType context_type) {
  // TODO(crbug.com/889751) : Need to change using RequestDestination
  return context_type == mojom::blink::RequestContextType::IFRAME ||
         context_type == mojom::blink::RequestContextType::FRAME ||
         context_type == mojom::blink::RequestContextType::FORM ||
         context_type == mojom::blink::RequestContextType::HYPERLINK;
}

}  // namespace

AtomicString PerformanceResourceTiming::GetNextHopProtocol(
    const AtomicString& alpn_negotiated_protocol,
    const AtomicString& connection_info) const {
  // Fallback to connection_info when alpn_negotiated_protocol is unknown.
  AtomicString returnedProtocol = (alpn_negotiated_protocol == "unknown")
                                      ? connection_info
                                      : alpn_negotiated_protocol;
  // If connection_info is unknown, or if TAO didn't pass, return the empty
  // string.
  // https://fetch.spec.whatwg.org/#create-an-opaque-timing-info
  if (returnedProtocol == "unknown" || !Info()->AllowTimingDetails()) {
    returnedProtocol = "";
  }

  return returnedProtocol;
}

AtomicString PerformanceResourceTiming::nextHopProtocol() const {
  return PerformanceResourceTiming::GetNextHopProtocol(
      Info()->AlpnNegotiatedProtocol(), Info()->ConnectionInfo());
}

DOMHighResTimeStamp PerformanceResourceTiming::workerStart() const {
  if (!resource_load_timing_ ||
      resource_load_timing_->WorkerStart().is_null() ||
      (!Info()->AllowTimingDetails() &&
       IsDocumentDestination(Info()->ContextType()))) {
    return 0.0;
  }

  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), resource_load_timing_->WorkerStart(),
      Info()->AllowNegativeValue(), CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceResourceTiming::WorkerReady() const {
  if (!resource_load_timing_ ||
      resource_load_timing_->WorkerReady().is_null()) {
    return 0.0;
  }

  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), resource_load_timing_->WorkerReady(),
      Info()->AllowNegativeValue(), CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceResourceTiming::redirectStart() const {
  if (Info()->LastRedirectEndTime().is_null() ||
      !Info()->AllowRedirectDetails()) {
    return 0.0;
  }

  if (DOMHighResTimeStamp worker_ready_time = WorkerReady())
    return worker_ready_time;

  return PerformanceEntry::startTime();
}

DOMHighResTimeStamp PerformanceResourceTiming::redirectEnd() const {
  if (Info()->LastRedirectEndTime().is_null() ||
      !Info()->AllowRedirectDetails()) {
    return 0.0;
  }

  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), Info()->LastRedirectEndTime(), Info()->AllowNegativeValue(),
      CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceResourceTiming::fetchStart() const {
  if (!resource_load_timing_ || (!Info()->AllowRedirectDetails() &&
                                 !Info()->LastRedirectEndTime().is_null())) {
    return PerformanceEntry::startTime();
  }

  if (!Info()->LastRedirectEndTime().is_null()) {
    return Performance::MonotonicTimeToDOMHighResTimeStamp(
        TimeOrigin(), resource_load_timing_->RequestTime(),
        Info()->AllowNegativeValue(), CrossOriginIsolatedCapability());
  }

  if (DOMHighResTimeStamp worker_ready_time = WorkerReady())
    return worker_ready_time;

  return PerformanceEntry::startTime();
}

DOMHighResTimeStamp PerformanceResourceTiming::domainLookupStart() const {
  if (!Info()->AllowTimingDetails()) {
    return 0.0;
  }
  if (!resource_load_timing_ ||
      resource_load_timing_->DomainLookupStart().is_null()) {
    return fetchStart();
  }

  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), resource_load_timing_->DomainLookupStart(),
      Info()->AllowNegativeValue(), CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceResourceTiming::domainLookupEnd() const {
  if (!Info()->AllowTimingDetails()) {
    return 0.0;
  }
  if (!resource_load_timing_ ||
      resource_load_timing_->DomainLookupEnd().is_null()) {
    return domainLookupStart();
  }

  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), resource_load_timing_->DomainLookupEnd(),
      Info()->AllowNegativeValue(), CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceResourceTiming::connectStart() const {
  if (!Info()->AllowTimingDetails()) {
    return 0.0;
  }
  // connectStart will be zero when a network request is not made.
  if (!resource_load_timing_ ||
      resource_load_timing_->ConnectStart().is_null() ||
      Info()->DidReuseConnection()) {
    return domainLookupEnd();
  }

  // connectStart includes any DNS time, so we may need to trim that off.
  base::TimeTicks connect_start = resource_load_timing_->ConnectStart();
  if (!resource_load_timing_->DomainLookupEnd().is_null()) {
    connect_start = resource_load_timing_->DomainLookupEnd();
  }

  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), connect_start, Info()->AllowNegativeValue(),
      CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceResourceTiming::connectEnd() const {
  if (!Info()->AllowTimingDetails()) {
    return 0.0;
  }
  // connectStart will be zero when a network request is not made.
  if (!resource_load_timing_ || resource_load_timing_->ConnectEnd().is_null() ||
      Info()->DidReuseConnection()) {
    return connectStart();
  }

  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), resource_load_timing_->ConnectEnd(),
      Info()->AllowNegativeValue(), CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceResourceTiming::secureConnectionStart() const {
  if (!Info()->AllowTimingDetails() || !Info()->IsSecureTransport()) {
    return 0.0;
  }

  // Step 2 of
  // https://w3c.github.io/resource-Timing()/#dom-performanceresourceTiming()-secureconnectionstart.
  if (Info()->DidReuseConnection()) {
    return fetchStart();
  }

  if (resource_load_timing_ && !resource_load_timing_->SslStart().is_null()) {
    return Performance::MonotonicTimeToDOMHighResTimeStamp(
        TimeOrigin(), resource_load_timing_->SslStart(),
        Info()->AllowNegativeValue(), CrossOriginIsolatedCapability());
  }
  // We would add a DCHECK(false) here but this case may happen, for instance on
  // SXG where the behavior has not yet been properly defined. See
  // https://github.com/w3c/navigation-timing/issues/107. Therefore, we return
  // fetchStart() for cases where SslStart() is not provided.
  return fetchStart();
}

DOMHighResTimeStamp PerformanceResourceTiming::requestStart() const {
  if (!Info()->AllowTimingDetails()) {
    return 0.0;
  }
  if (!resource_load_timing_ || resource_load_timing_->SendStart().is_null()) {
    return connectEnd();
  }

  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), resource_load_timing_->SendStart(),
      Info()->AllowNegativeValue(), CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceResourceTiming::firstInterimResponseStart()
    const {
  DCHECK(RuntimeEnabledFeatures::ResourceTimingInterimResponseTimesEnabled());
  if (!Info()->AllowTimingDetails() || !resource_load_timing_) {
    return 0;
  }

  base::TimeTicks response_start =
      resource_load_timing_->ReceiveEarlyHintsStart();
  if (response_start.is_null()) {
    return 0;
  }

  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), response_start, Info()->AllowNegativeValue(),
      CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceResourceTiming::responseStart() const {
  if (!RuntimeEnabledFeatures::ResourceTimingInterimResponseTimesEnabled()) {
    return GetAnyFirstResponseStart();
  }

  if (!Info()->AllowTimingDetails() || !resource_load_timing_) {
    return GetAnyFirstResponseStart();
  }

  base::TimeTicks response_start =
      resource_load_timing_->ReceiveNonInformationalHeadersStart();
  if (response_start.is_null()) {
    return GetAnyFirstResponseStart();
  }

  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), response_start, Info()->AllowNegativeValue(),
      CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceResourceTiming::GetAnyFirstResponseStart()
    const {
  if (!Info()->AllowTimingDetails()) {
    return 0.0;
  }
  if (!resource_load_timing_) {
    return requestStart();
  }

  base::TimeTicks response_start = resource_load_timing_->ReceiveHeadersStart();
  if (response_start.is_null())
    response_start = resource_load_timing_->ReceiveHeadersEnd();
  if (response_start.is_null())
    return requestStart();

  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), response_start, Info()->AllowNegativeValue(),
      CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceResourceTiming::responseEnd() const {
  if (Info()->ResponseEnd().is_null()) {
    return responseStart();
  }

  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), Info()->ResponseEnd(), Info()->AllowNegativeValue(),
      CrossOriginIsolatedCapability());
}

uint64_t PerformanceResourceTiming::transferSize() const {
  if (!Info()->AllowTimingDetails()) {
    return 0;
  }

  return GetTransferSize(Info()->EncodedBodySize(), Info()->CacheState());
}

uint64_t PerformanceResourceTiming::encodedBodySize() const {
  return Info()->EncodedBodySize();
}

uint64_t PerformanceResourceTiming::decodedBodySize() const {
  return Info()->DecodedBodySize();
}

const HeapVector<Member<PerformanceServerTiming>>&
PerformanceResourceTiming::serverTiming() const {
  return server_timing_;
}

void PerformanceResourceTiming::BuildJSONValue(V8ObjectBuilder& builder) const {
  PerformanceEntry::BuildJSONValue(builder);
  ExecutionContext* execution_context =
      ExecutionContext::From(builder.GetScriptState());
  builder.AddString("initiatorType", initiatorType());
  if (RuntimeEnabledFeatures::DeliveryTypeEnabled(execution_context)) {
    builder.AddString("deliveryType", deliveryType());
  }
  builder.AddString("nextHopProtocol", nextHopProtocol());
  if (RuntimeEnabledFeatures::RenderBlockingStatusEnabled()) {
    builder.AddString("renderBlockingStatus", renderBlockingStatus());
  }
  if (RuntimeEnabledFeatures::ResourceTimingContentTypeEnabled()) {
    builder.AddString("contentType", contentType());
  }
  builder.AddNumber("workerStart", workerStart());
  builder.AddNumber("redirectStart", redirectStart());
  builder.AddNumber("redirectEnd", redirectEnd());
  builder.AddNumber("fetchStart", fetchStart());
  builder.AddNumber("domainLookupStart", domainLookupStart());
  builder.AddNumber("domainLookupEnd", domainLookupEnd());
  builder.AddNumber("connectStart", connectStart());
  builder.AddNumber("secureConnectionStart", secureConnectionStart());
  builder.AddNumber("connectEnd", connectEnd());
  builder.AddNumber("requestStart", requestStart());
  builder.AddNumber("responseStart", responseStart());

  if (RuntimeEnabledFeatures::ResourceTimingInterimResponseTimesEnabled()) {
    builder.AddNumber("firstInterimResponseStart", firstInterimResponseStart());
  }

  builder.AddNumber("responseEnd", responseEnd());
  builder.AddNumber("transferSize", transferSize());
  builder.AddNumber("encodedBodySize", encodedBodySize());
  builder.AddNumber("decodedBodySize", decodedBodySize());
  if (RuntimeEnabledFeatures::ResourceTimingResponseStatusEnabled()) {
    builder.AddNumber("responseStatus", responseStatus());
  }

  ScriptState* script_state = builder.GetScriptState();
  builder.Add("serverTiming", FreezeV8Object(ToV8(serverTiming(), script_state),
                                             script_state->GetIsolate()));
}

void PerformanceResourceTiming::Trace(Visitor* visitor) const {
  visitor->Trace(server_timing_);
  PerformanceEntry::Trace(visitor);
}

}  // namespace blink
