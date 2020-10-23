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

#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/timing/performance_mark_or_measure.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/core/timing/performance_mark.h"
#include "third_party/blink/renderer/core/timing/performance_measure.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_timing.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_timing_info.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"

namespace blink {

PerformanceResourceTiming::PerformanceResourceTiming(
    const mojom::blink::ResourceTimingInfo& info,
    base::TimeTicks time_origin,
    const AtomicString& initiator_type,
    mojo::PendingReceiver<mojom::blink::WorkerTimingContainer>
        worker_timing_receiver,
    ExecutionContext* context)
    : PerformanceEntry(AtomicString(info.name),
                       Performance::MonotonicTimeToDOMHighResTimeStamp(
                           time_origin,
                           info.start_time,
                           info.allow_negative_values),
                       Performance::MonotonicTimeToDOMHighResTimeStamp(
                           time_origin,
                           info.response_end,
                           info.allow_negative_values)),
      initiator_type_(initiator_type.IsEmpty()
                          ? fetch_initiator_type_names::kOther
                          : initiator_type),
      alpn_negotiated_protocol_(
          static_cast<String>(info.alpn_negotiated_protocol)),
      connection_info_(static_cast<String>(info.connection_info)),
      time_origin_(time_origin),
      timing_(ResourceLoadTiming::FromMojo(info.timing.get())),
      last_redirect_end_time_(info.last_redirect_end_time),
      response_end_(info.response_end),
      context_type_(info.context_type),
      request_destination_(info.request_destination),
      transfer_size_(info.transfer_size),
      encoded_body_size_(info.encoded_body_size),
      decoded_body_size_(info.decoded_body_size),
      did_reuse_connection_(info.did_reuse_connection),
      allow_timing_details_(info.allow_timing_details),
      allow_redirect_details_(info.allow_redirect_details),
      allow_negative_value_(info.allow_negative_values),
      is_secure_context_(info.is_secure_context),
      server_timing_(
          PerformanceServerTiming::FromParsedServerTiming(info.server_timing)),
      worker_timing_receiver_(this, context) {
  DCHECK(context);
  worker_timing_receiver_.Bind(
      std::move(worker_timing_receiver),
      context->GetTaskRunner(TaskType::kMiscPlatformAPI));
}

// This constructor is for PerformanceNavigationTiming.
// TODO(https://crbug.com/900700): Set a Mojo pending receiver for
// WorkerTimingContainer in |worker_timing_receiver_| when a service worker
// controls a page.
PerformanceResourceTiming::PerformanceResourceTiming(
    const AtomicString& name,
    base::TimeTicks time_origin,
    bool is_secure_context,
    HeapVector<Member<PerformanceServerTiming>> server_timing,
    ExecutionContext* context)
    : PerformanceEntry(name, 0.0, 0.0),
      time_origin_(time_origin),
      context_type_(mojom::blink::RequestContextType::HYPERLINK),
      request_destination_(network::mojom::RequestDestination::kDocument),
      is_secure_context_(is_secure_context),
      server_timing_(std::move(server_timing)),
      worker_timing_receiver_(this, context) {
  DCHECK(context);
  worker_timing_receiver_.Bind(
      mojo::NullReceiver(), context->GetTaskRunner(TaskType::kMiscPlatformAPI));
}

PerformanceResourceTiming::~PerformanceResourceTiming() = default;

AtomicString PerformanceResourceTiming::entryType() const {
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

uint64_t PerformanceResourceTiming::GetTransferSize() const {
  return transfer_size_;
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

AtomicString PerformanceResourceTiming::AlpnNegotiatedProtocol() const {
  return alpn_negotiated_protocol_;
}

AtomicString PerformanceResourceTiming::ConnectionInfo() const {
  return connection_info_;
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
  // If connection_info is unknown, or if this is a `document` destination and
  // TAO didn't pass, return the empty string.
  // https://github.com/w3c/navigation-timing/issues/71
  // https://github.com/w3c/resource-timing/pull/224
  if (returnedProtocol == "unknown" ||
      (!AllowTimingDetails() && IsDocumentDestination(context_type_))) {
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
      (!AllowTimingDetails() && IsDocumentDestination(context_type_))) {
    return 0.0;
  }

  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      time_origin_, timing->WorkerStart(), allow_negative_value_);
}

DOMHighResTimeStamp PerformanceResourceTiming::WorkerReady() const {
  ResourceLoadTiming* timing = GetResourceLoadTiming();
  if (!timing || timing->WorkerReady().is_null())
    return 0.0;

  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      time_origin_, timing->WorkerReady(), allow_negative_value_);
}

DOMHighResTimeStamp PerformanceResourceTiming::redirectStart() const {
  if (last_redirect_end_time_.is_null() || !allow_redirect_details_)
    return 0.0;

  if (DOMHighResTimeStamp worker_ready_time = WorkerReady())
    return worker_ready_time;

  return PerformanceEntry::startTime();
}

DOMHighResTimeStamp PerformanceResourceTiming::redirectEnd() const {
  if (last_redirect_end_time_.is_null() || !allow_redirect_details_)
    return 0.0;

  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      time_origin_, last_redirect_end_time_, allow_negative_value_);
}

DOMHighResTimeStamp PerformanceResourceTiming::fetchStart() const {
  ResourceLoadTiming* timing = GetResourceLoadTiming();
  if (!timing)
    return PerformanceEntry::startTime();

  if (!last_redirect_end_time_.is_null()) {
    return Performance::MonotonicTimeToDOMHighResTimeStamp(
        time_origin_, timing->RequestTime(), allow_negative_value_);
  }

  if (DOMHighResTimeStamp worker_ready_time = WorkerReady())
    return worker_ready_time;

  return PerformanceEntry::startTime();
}

DOMHighResTimeStamp PerformanceResourceTiming::domainLookupStart() const {
  if (!AllowTimingDetails())
    return 0.0;
  ResourceLoadTiming* timing = GetResourceLoadTiming();
  if (!timing || timing->DnsStart().is_null())
    return fetchStart();

  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      time_origin_, timing->DnsStart(), allow_negative_value_);
}

DOMHighResTimeStamp PerformanceResourceTiming::domainLookupEnd() const {
  if (!AllowTimingDetails())
    return 0.0;
  ResourceLoadTiming* timing = GetResourceLoadTiming();
  if (!timing || timing->DnsEnd().is_null())
    return domainLookupStart();

  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      time_origin_, timing->DnsEnd(), allow_negative_value_);
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
  if (!timing->DnsEnd().is_null())
    connect_start = timing->DnsEnd();

  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      time_origin_, connect_start, allow_negative_value_);
}

DOMHighResTimeStamp PerformanceResourceTiming::connectEnd() const {
  if (!AllowTimingDetails())
    return 0.0;
  ResourceLoadTiming* timing = GetResourceLoadTiming();
  // connectStart will be zero when a network request is not made.
  if (!timing || timing->ConnectEnd().is_null() || DidReuseConnection())
    return connectStart();

  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      time_origin_, timing->ConnectEnd(), allow_negative_value_);
}

DOMHighResTimeStamp PerformanceResourceTiming::secureConnectionStart() const {
  if (!AllowTimingDetails() || !is_secure_context_)
    return 0.0;

  // Step 2 of
  // https://w3c.github.io/resource-timing/#dom-performanceresourcetiming-secureconnectionstart.
  if (DidReuseConnection())
    return fetchStart();

  ResourceLoadTiming* timing = GetResourceLoadTiming();
  if (!timing || timing->SslStart().is_null()) {
    // TODO(yoav): add DCHECK or use counter to make sure this never happens.
    return 0.0;
  }

  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      time_origin_, timing->SslStart(), allow_negative_value_);
}

DOMHighResTimeStamp PerformanceResourceTiming::requestStart() const {
  if (!AllowTimingDetails())
    return 0.0;
  ResourceLoadTiming* timing = GetResourceLoadTiming();
  if (!timing)
    return connectEnd();

  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      time_origin_, timing->SendStart(), allow_negative_value_);
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
      time_origin_, response_start, allow_negative_value_);
}

DOMHighResTimeStamp PerformanceResourceTiming::responseEnd() const {
  if (response_end_.is_null())
    return responseStart();

  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      time_origin_, response_end_, allow_negative_value_);
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

const HeapVector<Member<PerformanceEntry>>&
PerformanceResourceTiming::workerTiming() const {
  return worker_timing_;
}

void PerformanceResourceTiming::BuildJSONValue(V8ObjectBuilder& builder) const {
  PerformanceEntry::BuildJSONValue(builder);
  builder.AddString("initiatorType", initiatorType());
  builder.AddString("nextHopProtocol", nextHopProtocol());
  builder.AddNumber("workerStart", workerStart());
  builder.AddNumber("redirectStart", redirectStart());
  builder.AddNumber("redirectEnd", redirectEnd());
  builder.AddNumber("fetchStart", fetchStart());
  builder.AddNumber("domainLookupStart", domainLookupStart());
  builder.AddNumber("domainLookupEnd", domainLookupEnd());
  builder.AddNumber("connectStart", connectStart());
  builder.AddNumber("connectEnd", connectEnd());
  builder.AddNumber("secureConnectionStart", secureConnectionStart());
  builder.AddNumber("requestStart", requestStart());
  builder.AddNumber("responseStart", responseStart());
  builder.AddNumber("responseEnd", responseEnd());
  builder.AddNumber("transferSize", transferSize());
  builder.AddNumber("encodedBodySize", encodedBodySize());
  builder.AddNumber("decodedBodySize", decodedBodySize());

  ScriptState* script_state = builder.GetScriptState();
  builder.Add("serverTiming", FreezeV8Object(ToV8(serverTiming(), script_state),
                                             script_state->GetIsolate()));
  builder.Add("workerTiming", FreezeV8Object(ToV8(workerTiming(), script_state),
                                             script_state->GetIsolate()));
}

void PerformanceResourceTiming::AddPerformanceEntry(
    mojom::blink::PerformanceMarkOrMeasurePtr
        mojo_performance_mark_or_measure) {
  // TODO(https://crbug.com/900700): Wait until the end of fetch event to stop
  // appearing incomplete PerformanceResourceTiming. Incomplete |workerTiming|
  // will be exposed in the case that FetchEvent#addPerformanceEntry is called
  // after PerformanceResourceTiming is constructed. This may cause different
  // results of |workerTiming| in accessing it at the different time.

  NonThrowableExceptionState exception_state;
  WTF::AtomicString name(mojo_performance_mark_or_measure->name);

  scoped_refptr<SerializedScriptValue> serialized_detail =
      SerializedScriptValue::NullValue();
  if (mojo_performance_mark_or_measure->detail) {
    serialized_detail = SerializedScriptValue::Create(
        reinterpret_cast<const char*>(
            mojo_performance_mark_or_measure->detail->data()),
        mojo_performance_mark_or_measure->detail->size());
  }

  switch (mojo_performance_mark_or_measure->entry_type) {
    case mojom::blink::PerformanceMarkOrMeasure::EntryType::kMark:
      worker_timing_.emplace_back(MakeGarbageCollected<PerformanceMark>(
          name, mojo_performance_mark_or_measure->start_time, serialized_detail,
          exception_state));
      break;
    case mojom::blink::PerformanceMarkOrMeasure::EntryType::kMeasure:
      ScriptState* script_state;
      worker_timing_.emplace_back(MakeGarbageCollected<PerformanceMeasure>(
          script_state, name, mojo_performance_mark_or_measure->start_time,
          mojo_performance_mark_or_measure->start_time +
              mojo_performance_mark_or_measure->duration,
          serialized_detail, exception_state));
      break;
  }
}

void PerformanceResourceTiming::Trace(Visitor* visitor) const {
  visitor->Trace(server_timing_);
  visitor->Trace(worker_timing_);
  visitor->Trace(worker_timing_receiver_);
  PerformanceEntry::Trace(visitor);
}

}  // namespace blink
