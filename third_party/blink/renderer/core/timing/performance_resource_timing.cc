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
#include "services/network/public/mojom/service_worker_router_info.mojom-blink-forward.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/timing/performance_mark_or_measure.mojom-blink.h"
#include "third_party/blink/public/mojom/timing/resource_timing.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/core/timing/performance_entry.h"
#include "third_party/blink/renderer/core/timing/performance_mark.h"
#include "third_party/blink/renderer/core/timing/performance_measure.h"
#include "third_party/blink/renderer/core/timing/performance_server_timing.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/loader/fetch/delivery_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_timing.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_timing_utils.h"
#include "third_party/blink/renderer/platform/loader/fetch/service_worker_router_info.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

using network::mojom::blink::NavigationDeliveryType;

PerformanceResourceTiming::PerformanceResourceTiming(
    mojom::blink::ResourceTimingInfoPtr info,
    const AtomicString& initiator_type,
    base::TimeTicks time_origin,
    bool cross_origin_isolated_capability,
    ExecutionContext* context)
    : PerformanceEntry(
          info->name.IsNull() ? g_empty_atom : AtomicString(info->name),
          Performance::MonotonicTimeToDOMHighResTimeStamp(
              time_origin,
              info->start_time,
              info->allow_negative_values,
              cross_origin_isolated_capability),
          Performance::MonotonicTimeToDOMHighResTimeStamp(
              time_origin,
              info->response_end,
              info->allow_negative_values,
              cross_origin_isolated_capability),
          DynamicTo<LocalDOMWindow>(context)),
      initiator_type_(initiator_type.empty() || initiator_type.IsNull()
                          ? fetch_initiator_type_names::kOther
                          : initiator_type),
      time_origin_(time_origin),
      cross_origin_isolated_capability_(cross_origin_isolated_capability),
      server_timing_(
          PerformanceServerTiming::FromParsedServerTiming(info->server_timing)),
      info_(std::move(info)) {
  if (!server_timing_.empty()) {
    UseCounter::Count(context, WebFeature::kPerformanceServerTiming);
  }
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
  NOTREACHED_IN_MIGRATION();
  return 0;
}

bool PerformanceResourceTiming::IsResponseFromCacheStorage() const {
  return info_->service_worker_response_source ==
         network::mojom::blink::FetchResponseSource::kCacheStorage;
}

AtomicString PerformanceResourceTiming::GetDeliveryType() const {
  if (RuntimeEnabledFeatures::ServiceWorkerStaticRouterTimingInfoEnabled(
          DynamicTo<LocalDOMWindow>(source())) &&
      IsResponseFromCacheStorage()) {
    return delivery_type_names::kCacheStorage;
  }
  return info_->cache_state == mojom::blink::CacheState::kNone
             ? g_empty_atom
             : delivery_type_names::kCache;
}

AtomicString PerformanceResourceTiming::deliveryType() const {
  return info_->allow_timing_details ? GetDeliveryType() : g_empty_atom;
}

AtomicString PerformanceResourceTiming::renderBlockingStatus() const {
  return AtomicString(info_->render_blocking_status ? "blocking"
                                                    : "non-blocking");
}

AtomicString PerformanceResourceTiming::contentType() const {
  return AtomicString(info_->content_type);
}

uint16_t PerformanceResourceTiming::responseStatus() const {
  return info_->response_status;
}

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
  if (returnedProtocol == "unknown" || !info_->allow_timing_details) {
    returnedProtocol = g_empty_atom;
  }

  return returnedProtocol;
}

AtomicString PerformanceResourceTiming::nextHopProtocol() const {
  return PerformanceResourceTiming::GetNextHopProtocol(
      AtomicString(info_->alpn_negotiated_protocol),
      AtomicString(info_->connection_info));
}

DOMHighResTimeStamp PerformanceResourceTiming::workerStart() const {
  if (!info_->timing || info_->timing->service_worker_start_time.is_null()) {
    return 0.0;
  }

  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), info_->timing->service_worker_start_time,
      info_->allow_negative_values, CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceResourceTiming::workerRouterEvaluationStart()
    const {
  if (!info_->timing ||
      info_->timing->service_worker_router_evaluation_start.is_null()) {
    return 0.0;
  }

  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), info_->timing->service_worker_router_evaluation_start,
      info_->allow_negative_values, CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceResourceTiming::workerCacheLookupStart() const {
  if (!info_->timing ||
      info_->timing->service_worker_cache_lookup_start.is_null()) {
    return 0.0;
  }

  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), info_->timing->service_worker_cache_lookup_start,
      info_->allow_negative_values, CrossOriginIsolatedCapability());
}

AtomicString PerformanceResourceTiming::workerMatchedSourceType() const {
  if (!info_->service_worker_router_info ||
      !info_->service_worker_router_info->matched_source_type) {
    return AtomicString();
  }

  return AtomicString(ServiceWorkerRouterInfo::GetRouterSourceTypeString(
      *info_->service_worker_router_info->matched_source_type));
}

AtomicString PerformanceResourceTiming::workerFinalSourceType() const {
  if (!info_->service_worker_router_info ||
      !info_->service_worker_router_info->actual_source_type) {
    return AtomicString();
  }

  return AtomicString(ServiceWorkerRouterInfo::GetRouterSourceTypeString(
      *info_->service_worker_router_info->actual_source_type));
}

DOMHighResTimeStamp PerformanceResourceTiming::WorkerReady() const {
  if (!info_->timing || info_->timing->service_worker_ready_time.is_null()) {
    return 0.0;
  }

  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), info_->timing->service_worker_ready_time,
      info_->allow_negative_values, CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceResourceTiming::redirectStart() const {
  if (info_->last_redirect_end_time.is_null()) {
    return 0.0;
  }

  if (DOMHighResTimeStamp worker_ready_time = WorkerReady())
    return worker_ready_time;

  return PerformanceEntry::startTime();
}

DOMHighResTimeStamp PerformanceResourceTiming::redirectEnd() const {
  if (info_->last_redirect_end_time.is_null()) {
    return 0.0;
  }

  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), info_->last_redirect_end_time, info_->allow_negative_values,
      CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceResourceTiming::fetchStart() const {
  if (!info_->timing) {
    return PerformanceEntry::startTime();
  }

  if (!info_->last_redirect_end_time.is_null()) {
    return Performance::MonotonicTimeToDOMHighResTimeStamp(
        TimeOrigin(), info_->timing->request_start,
        info_->allow_negative_values, CrossOriginIsolatedCapability());
  }

  if (DOMHighResTimeStamp worker_ready_time = WorkerReady())
    return worker_ready_time;

  // If the fetch came from service worker static routing API and the actual
  // source type is cache, we will not have a fetch start. For compatibility,
  // we set this to responseStart (as written in explainer
  // https://github.com/WICG/service-worker-static-routing-api/blob/main/resource-timing-api.md
  // ).
  if (RuntimeEnabledFeatures::ServiceWorkerStaticRouterTimingInfoEnabled(
          DynamicTo<LocalDOMWindow>(source())) &&
      info_->service_worker_router_info &&
      info_->service_worker_router_info->actual_source_type ==
          network::mojom::ServiceWorkerRouterSourceType::kCache) {
    return responseStart();
  }

  return PerformanceEntry::startTime();
}

DOMHighResTimeStamp PerformanceResourceTiming::domainLookupStart() const {
  if (!info_->allow_timing_details) {
    return 0.0;
  }
  if (!info_->timing || !info_->timing->connect_timing ||
      info_->timing->connect_timing->domain_lookup_start.is_null()) {
    return fetchStart();
  }

  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), info_->timing->connect_timing->domain_lookup_start,
      info_->allow_negative_values, CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceResourceTiming::domainLookupEnd() const {
  if (!info_->allow_timing_details) {
    return 0.0;
  }
  if (!info_->timing || !info_->timing->connect_timing ||
      info_->timing->connect_timing->domain_lookup_end.is_null()) {
    return domainLookupStart();
  }

  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), info_->timing->connect_timing->domain_lookup_end,
      info_->allow_negative_values, CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceResourceTiming::connectStart() const {
  if (!info_->allow_timing_details) {
    return 0.0;
  }
  // connectStart will be zero when a network request is not made.
  if (!info_->timing || !info_->timing->connect_timing ||
      info_->timing->connect_timing->connect_start.is_null() ||
      info_->did_reuse_connection) {
    return domainLookupEnd();
  }

  // connectStart includes any DNS time, so we may need to trim that off.
  base::TimeTicks connect_start = info_->timing->connect_timing->connect_start;
  if (!info_->timing->connect_timing->domain_lookup_end.is_null()) {
    connect_start = info_->timing->connect_timing->domain_lookup_end;
  }

  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), connect_start, info_->allow_negative_values,
      CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceResourceTiming::connectEnd() const {
  if (!info_->allow_timing_details) {
    return 0.0;
  }
  // connectStart will be zero when a network request is not made.
  if (!info_->timing || !info_->timing->connect_timing ||
      info_->timing->connect_timing->connect_end.is_null() ||
      info_->did_reuse_connection) {
    return connectStart();
  }

  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), info_->timing->connect_timing->connect_end,
      info_->allow_negative_values, CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceResourceTiming::secureConnectionStart() const {
  if (!info_->allow_timing_details || !info_->is_secure_transport) {
    return 0.0;
  }

  // Step 2 of
  // https://w3c.github.io/resource-Timing()/#dom-performanceresourceTiming()-secureconnectionstart.
  if (info_->did_reuse_connection) {
    return fetchStart();
  }

  if (info_->timing && info_->timing->connect_timing &&
      !info_->timing->connect_timing->ssl_start.is_null()) {
    return Performance::MonotonicTimeToDOMHighResTimeStamp(
        TimeOrigin(), info_->timing->connect_timing->ssl_start,
        info_->allow_negative_values, CrossOriginIsolatedCapability());
  }
  // We would add a DCHECK(false) here but this case may happen, for instance on
  // SXG where the behavior has not yet been properly defined. See
  // https://github.com/w3c/navigation-timing/issues/107. Therefore, we return
  // fetchStart() for cases where SslStart() is not provided.
  return fetchStart();
}

DOMHighResTimeStamp PerformanceResourceTiming::requestStart() const {
  if (!info_->allow_timing_details) {
    return 0.0;
  }
  if (!info_->timing || info_->timing->send_start.is_null()) {
    return connectEnd();
  }

  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), info_->timing->send_start, info_->allow_negative_values,
      CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceResourceTiming::firstInterimResponseStart()
    const {
  if (!info_->allow_timing_details || !info_->timing) {
    return 0;
  }

  base::TimeTicks response_start = info_->timing->receive_headers_start;
  if (response_start.is_null() ||
      response_start ==
          info_->timing->receive_non_informational_headers_start) {
    return 0;
  }

  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), response_start, info_->allow_negative_values,
      CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceResourceTiming::responseStart() const {
  if (!info_->allow_timing_details || !info_->timing) {
    return GetAnyFirstResponseStart();
  }

  base::TimeTicks response_start =
      info_->timing->receive_non_informational_headers_start;
  if (response_start.is_null()) {
    return GetAnyFirstResponseStart();
  }

  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), response_start, info_->allow_negative_values,
      CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceResourceTiming::GetAnyFirstResponseStart()
    const {
  if (!info_->allow_timing_details) {
    return 0.0;
  }
  if (!info_->timing) {
    return requestStart();
  }

  base::TimeTicks response_start = info_->timing->receive_headers_start;
  if (response_start.is_null())
    response_start = info_->timing->receive_headers_end;
  if (response_start.is_null())
    return requestStart();

  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), response_start, info_->allow_negative_values,
      CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceResourceTiming::responseEnd() const {
  if (info_->response_end.is_null()) {
    return responseStart();
  }

  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), info_->response_end, info_->allow_negative_values,
      CrossOriginIsolatedCapability());
}

uint64_t PerformanceResourceTiming::transferSize() const {
  if (!info_->allow_timing_details) {
    return 0;
  }

  return GetTransferSize(info_->encoded_body_size, info_->cache_state);
}

uint64_t PerformanceResourceTiming::encodedBodySize() const {
  return info_->encoded_body_size;
}

uint64_t PerformanceResourceTiming::decodedBodySize() const {
  return info_->decoded_body_size;
}

const HeapVector<Member<PerformanceServerTiming>>&
PerformanceResourceTiming::serverTiming() const {
  return server_timing_;
}

void PerformanceResourceTiming::BuildJSONValue(V8ObjectBuilder& builder) const {
  PerformanceEntry::BuildJSONValue(builder);
  builder.AddString("initiatorType", initiatorType());
  builder.AddString("deliveryType", deliveryType());
  builder.AddString("nextHopProtocol", nextHopProtocol());
  if (RuntimeEnabledFeatures::RenderBlockingStatusEnabled()) {
    builder.AddString("renderBlockingStatus", renderBlockingStatus());
  }
  if (RuntimeEnabledFeatures::ResourceTimingContentTypeEnabled()) {
    builder.AddString("contentType", contentType());
  }
  builder.AddNumber("workerStart", workerStart());
  if (RuntimeEnabledFeatures::ServiceWorkerStaticRouterTimingInfoEnabled(
          ExecutionContext::From(builder.GetScriptState()))) {
    builder.AddNumber("workerRouterEvaluationStart",
                      workerRouterEvaluationStart());
    builder.AddNumber("workerCacheLookupStart", workerCacheLookupStart());
    builder.AddString("matchedSourceType", workerMatchedSourceType());
    builder.AddString("finalSourceType", workerFinalSourceType());
  }
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
  builder.AddNumber("firstInterimResponseStart", firstInterimResponseStart());

  builder.AddNumber("responseEnd", responseEnd());
  builder.AddNumber("transferSize", transferSize());
  builder.AddNumber("encodedBodySize", encodedBodySize());
  builder.AddNumber("decodedBodySize", decodedBodySize());
  builder.AddNumber("responseStatus", responseStatus());

  builder.AddV8Value("serverTiming",
                     ToV8Traits<IDLArray<PerformanceServerTiming>>::ToV8(
                         builder.GetScriptState(), serverTiming()));
}

void PerformanceResourceTiming::Trace(Visitor* visitor) const {
  visitor->Trace(server_timing_);
  PerformanceEntry::Trace(visitor);
}

}  // namespace blink
