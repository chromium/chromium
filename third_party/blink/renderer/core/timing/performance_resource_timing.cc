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
#include "third_party/blink/renderer/core/delivery_type_names.h"
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

using network::mojom::blink::NavigationDeliveryType;

namespace {
const AtomicString& GetDeliveryType(
    NavigationDeliveryType navigation_delivery_type,
    mojom::blink::CacheState cache_state) {
  switch (navigation_delivery_type) {
    case NavigationDeliveryType::kDefault:
      return cache_state == mojom::blink::CacheState::kNone
                 ? g_empty_atom
                 : delivery_type_names::kCache;
    case NavigationDeliveryType::kNavigationalPrefetch:
      return delivery_type_names::kNavigationalPrefetch;
    default:
      NOTREACHED();
      return g_empty_atom;
  }
}
}  // namespace

PerformanceResourceTiming::PerformanceResourceTiming(
    const mojom::blink::ResourceTimingInfo& info,
    base::TimeTicks time_origin,
    bool cross_origin_isolated_capability,
    const AtomicString& initiator_type,
    ExecutionContext* context)
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
                       PerformanceEntry::GetNavigationId(context)),
      initiator_type_(initiator_type.empty()
                          ? fetch_initiator_type_names::kOther
                          : initiator_type),
      delivery_type_(
          GetDeliveryType(NavigationDeliveryType::kDefault, info.cache_state)),
      alpn_negotiated_protocol_(
          static_cast<String>(info.alpn_negotiated_protocol)),
      connection_info_(static_cast<String>(info.connection_info)),
      content_type_(static_cast<String>(info.content_type)),
      render_blocking_status_(info.render_blocking_status
                                  ? RenderBlockingStatusType::kBlocking
                                  : RenderBlockingStatusType::kNonBlocking),
      time_origin_(time_origin),
      cross_origin_isolated_capability_(cross_origin_isolated_capability),
      timing_(ResourceLoadTiming::FromMojo(info.timing.get())),
      last_redirect_end_time_(info.last_redirect_end_time),
      response_end_(info.response_end),
      context_type_(info.context_type),
      request_destination_(info.request_destination),
      cache_state_(info.cache_state),
      encoded_body_size_(info.encoded_body_size),
      decoded_body_size_(info.decoded_body_size),
      response_status_(info.response_status),
      did_reuse_connection_(info.did_reuse_connection),
      allow_timing_details_(info.allow_timing_details),
      allow_redirect_details_(info.allow_redirect_details),
      allow_negative_value_(info.allow_negative_values),
      is_secure_transport_(info.is_secure_transport),
      server_timing_(
          PerformanceServerTiming::FromParsedServerTiming(info.server_timing)) {
  DCHECK(context);
}

// This constructor is for PerformanceNavigationTiming.
// The navigation_id for navigation timing is always 1.
PerformanceResourceTiming::PerformanceResourceTiming(
    const AtomicString& name,
    base::TimeTicks time_origin,
    bool cross_origin_isolated_capability,
    mojom::blink::CacheState cache_state,
    bool is_secure_transport,
    HeapVector<Member<PerformanceServerTiming>> server_timing,
    ExecutionContext* context,
    NavigationDeliveryType navigation_delivery_type)
    : PerformanceEntry(name, 0.0, 0.0, kNavigationIdDefaultValue),
      delivery_type_(GetDeliveryType(navigation_delivery_type, cache_state)),
      time_origin_(time_origin),
      cross_origin_isolated_capability_(cross_origin_isolated_capability),
      context_type_(mojom::blink::RequestContextType::HYPERLINK),
      request_destination_(network::mojom::RequestDestination::kDocument),
      cache_state_(cache_state),
      is_secure_transport_(is_secure_transport),
      server_timing_(std::move(server_timing)) {
  DCHECK(context);
}

PerformanceResourceTiming::~PerformanceResourceTiming() = default;

const AtomicString& PerformanceResourceTiming::entryType() const {
  return performance_entry_names::kResource;
}

PerformanceEntryType PerformanceResourceTiming::EntryTypeEnum() const {
  return PerformanceEntry::EntryType::kResource;
}

ResourceLoadTiming* PerformanceResourceTiming::GetResourceLoadTiming() const {
  return timing_.get();
}

bool PerformanceResourceTiming::AllowTimingDetails() const {
  return allow_timing_details_;
}

bool PerformanceResourceTiming::DidReuseConnection() const {
  return did_reuse_connection_;
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

uint64_t PerformanceResourceTiming::GetTransferSize() const {
  return GetTransferSize(GetEncodedBodySize(), CacheState());
}

uint64_t PerformanceResourceTiming::GetEncodedBodySize() const {
  return encoded_body_size_;
}

uint64_t PerformanceResourceTiming::GetDecodedBodySize() const {
  return decoded_body_size_;
}

AtomicString PerformanceResourceTiming::initiatorType() const {
  return initiator_type_;
}

AtomicString PerformanceResourceTiming::deliveryType() const {
  if (!AllowTimingDetails())
    return g_empty_atom;
  return delivery_type_;
}

AtomicString PerformanceResourceTiming::renderBlockingStatus() const {
  switch (render_blocking_status_) {
    case RenderBlockingStatusType::kBlocking:
      return "blocking";
    case RenderBlockingStatusType::kNonBlocking:
      return "non-blocking";
  }
  NOTREACHED();
  return "non-blocking";
}

AtomicString PerformanceResourceTiming::contentType() const {
  return content_type_;
}

uint16_t PerformanceResourceTiming::responseStatus() const {
  return response_status_;
}

AtomicString PerformanceResourceTiming::AlpnNegotiatedProtocol() const {
  return alpn_negotiated_protocol_;
}

AtomicString PerformanceResourceTiming::ConnectionInfo() const {
  return connection_info_;
}

mojom::blink::RequestContextType PerformanceResourceTiming::ContextType()
    const {
  return context_type_;
}

base::TimeTicks PerformanceResourceTiming::ResponseEnd() const {
  return response_end_;
}

base::TimeTicks PerformanceResourceTiming::LastRedirectEndTime() const {
  return last_redirect_end_time_;
}

bool PerformanceResourceTiming::AllowRedirectDetails() const {
  return allow_redirect_details_;
}

bool PerformanceResourceTiming::AllowNegativeValue() const {
  return allow_negative_value_;
}

bool PerformanceResourceTiming::IsSecureTransport() const {
  return is_secure_transport_;
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
  if (returnedProtocol == "unknown" || !AllowTimingDetails()) {
    returnedProtocol = "";
  }

  return returnedProtocol;
}

AtomicString PerformanceResourceTiming::nextHopProtocol() const {
  return PerformanceResourceTiming::GetNextHopProtocol(AlpnNegotiatedProtocol(),
                                                       ConnectionInfo());
}

DOMHighResTimeStamp PerformanceResourceTiming::workerStart() const {
  ResourceLoadTiming* timing = GetResourceLoadTiming();
  if (!timing || timing->WorkerStart().is_null() ||
      (!AllowTimingDetails() && IsDocumentDestination(ContextType()))) {
    return 0.0;
  }

  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->WorkerStart(), AllowNegativeValue(),
      CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceResourceTiming::WorkerReady() const {
  ResourceLoadTiming* timing = GetResourceLoadTiming();
  if (!timing || timing->WorkerReady().is_null())
    return 0.0;

  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->WorkerReady(), AllowNegativeValue(),
      CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceResourceTiming::redirectStart() const {
  if (LastRedirectEndTime().is_null() || !AllowRedirectDetails())
    return 0.0;

  if (DOMHighResTimeStamp worker_ready_time = WorkerReady())
    return worker_ready_time;

  return PerformanceEntry::startTime();
}

DOMHighResTimeStamp PerformanceResourceTiming::redirectEnd() const {
  if (LastRedirectEndTime().is_null() || !AllowRedirectDetails())
    return 0.0;

  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), LastRedirectEndTime(), AllowNegativeValue(),
      CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceResourceTiming::fetchStart() const {
  ResourceLoadTiming* timing = GetResourceLoadTiming();
  if (!timing ||
      (!AllowRedirectDetails() && !LastRedirectEndTime().is_null())) {
    return PerformanceEntry::startTime();
  }

  if (!LastRedirectEndTime().is_null()) {
    return Performance::MonotonicTimeToDOMHighResTimeStamp(
        TimeOrigin(), timing->RequestTime(), AllowNegativeValue(),
        CrossOriginIsolatedCapability());
  }

  if (DOMHighResTimeStamp worker_ready_time = WorkerReady())
    return worker_ready_time;

  return PerformanceEntry::startTime();
}

DOMHighResTimeStamp PerformanceResourceTiming::domainLookupStart() const {
  if (!AllowTimingDetails())
    return 0.0;
  ResourceLoadTiming* timing = GetResourceLoadTiming();
  if (!timing || timing->DomainLookupStart().is_null())
    return fetchStart();

  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->DomainLookupStart(), AllowNegativeValue(),
      CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceResourceTiming::domainLookupEnd() const {
  if (!AllowTimingDetails())
    return 0.0;
  ResourceLoadTiming* timing = GetResourceLoadTiming();
  if (!timing || timing->DomainLookupEnd().is_null())
    return domainLookupStart();

  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->DomainLookupEnd(), AllowNegativeValue(),
      CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceResourceTiming::connectStart() const {
  if (!AllowTimingDetails())
    return 0.0;
  ResourceLoadTiming* timing = GetResourceLoadTiming();
  // connectStart will be zero when a network request is not made.
  if (!timing || timing->ConnectStart().is_null() || DidReuseConnection())
    return domainLookupEnd();

  // connectStart includes any DNS time, so we may need to trim that off.
  base::TimeTicks connect_start = timing->ConnectStart();
  if (!timing->DomainLookupEnd().is_null())
    connect_start = timing->DomainLookupEnd();

  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), connect_start, AllowNegativeValue(),
      CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceResourceTiming::connectEnd() const {
  if (!AllowTimingDetails())
    return 0.0;
  ResourceLoadTiming* timing = GetResourceLoadTiming();
  // connectStart will be zero when a network request is not made.
  if (!timing || timing->ConnectEnd().is_null() || DidReuseConnection())
    return connectStart();

  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->ConnectEnd(), AllowNegativeValue(),
      CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceResourceTiming::secureConnectionStart() const {
  if (!AllowTimingDetails() || !IsSecureTransport())
    return 0.0;

  // Step 2 of
  // https://w3c.github.io/resource-timing/#dom-performanceresourcetiming-secureconnectionstart.
  if (DidReuseConnection())
    return fetchStart();

  ResourceLoadTiming* timing = GetResourceLoadTiming();
  if (timing && !timing->SslStart().is_null()) {
    return Performance::MonotonicTimeToDOMHighResTimeStamp(
        TimeOrigin(), timing->SslStart(), AllowNegativeValue(),
        CrossOriginIsolatedCapability());
  }
  // We would add a DCHECK(false) here but this case may happen, for instance on
  // SXG where the behavior has not yet been properly defined. See
  // https://github.com/w3c/navigation-timing/issues/107. Therefore, we return
  // fetchStart() for cases where SslStart() is not provided.
  return fetchStart();
}

DOMHighResTimeStamp PerformanceResourceTiming::requestStart() const {
  if (!AllowTimingDetails())
    return 0.0;
  ResourceLoadTiming* timing = GetResourceLoadTiming();
  if (!timing || timing->SendStart().is_null())
    return connectEnd();

  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->SendStart(), AllowNegativeValue(),
      CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceResourceTiming::responseStart() const {
  if (!AllowTimingDetails())
    return 0.0;
  ResourceLoadTiming* timing = GetResourceLoadTiming();
  if (!timing)
    return requestStart();

  base::TimeTicks response_start = timing->ReceiveHeadersStart();
  if (response_start.is_null())
    response_start = timing->ReceiveHeadersEnd();
  if (response_start.is_null())
    return requestStart();

  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), response_start, AllowNegativeValue(),
      CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceResourceTiming::responseEnd() const {
  if (ResponseEnd().is_null())
    return responseStart();

  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), ResponseEnd(), AllowNegativeValue(),
      CrossOriginIsolatedCapability());
}

uint64_t PerformanceResourceTiming::transferSize() const {
  if (!AllowTimingDetails())
    return 0;

  return GetTransferSize();
}

uint64_t PerformanceResourceTiming::encodedBodySize() const {
  if (!AllowTimingDetails())
    return 0;

  return GetEncodedBodySize();
}

uint64_t PerformanceResourceTiming::decodedBodySize() const {
  if (!AllowTimingDetails())
    return 0;

  return GetDecodedBodySize();
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
