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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_RESOURCE_TIMING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_RESOURCE_TIMING_H_

#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/timing/performance_mark_or_measure.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/timing/resource_timing.mojom-blink.h"
#include "third_party/blink/public/mojom/timing/worker_timing_container.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/timing/performance_entry.h"
#include "third_party/blink/renderer/core/timing/performance_server_timing.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class ResourceLoadTiming;

class CORE_EXPORT PerformanceResourceTiming
    : public PerformanceEntry,
      public mojom::blink::WorkerTimingContainer {
  DEFINE_WRAPPERTYPEINFO();
  friend class PerformanceResourceTimingTest;

 public:
  // This constructor is for PerformanceNavigationTiming.
  // Related doc: https://goo.gl/uNecAj.
  PerformanceResourceTiming(
      const AtomicString& name,
      base::TimeTicks time_origin,
      bool cross_origin_isolated_capability,
      bool is_secure_transport,
      HeapVector<Member<PerformanceServerTiming>> server_timing,
      ExecutionContext* context);
  PerformanceResourceTiming(
      const mojom::blink::ResourceTimingInfo&,
      base::TimeTicks time_origin,
      bool cross_origin_isolated_capability,
      const AtomicString& initiator_type,
      mojo::PendingReceiver<mojom::blink::WorkerTimingContainer>
          worker_timing_receiver,
      ExecutionContext* context);
  ~PerformanceResourceTiming() override;

  AtomicString entryType() const override;
  PerformanceEntryType EntryTypeEnum() const override;

  // Related doc: https://goo.gl/uNecAj.
  virtual AtomicString initiatorType() const;
  AtomicString nextHopProtocol() const;
  DOMHighResTimeStamp workerStart() const;
  virtual DOMHighResTimeStamp redirectStart() const;
  virtual DOMHighResTimeStamp redirectEnd() const;
  virtual DOMHighResTimeStamp fetchStart() const;
  DOMHighResTimeStamp domainLookupStart() const;
  DOMHighResTimeStamp domainLookupEnd() const;
  DOMHighResTimeStamp connectStart() const;
  DOMHighResTimeStamp connectEnd() const;
  DOMHighResTimeStamp secureConnectionStart() const;
  DOMHighResTimeStamp requestStart() const;
  DOMHighResTimeStamp responseStart() const;
  virtual DOMHighResTimeStamp responseEnd() const;
  uint64_t transferSize() const;
  uint64_t encodedBodySize() const;
  uint64_t decodedBodySize() const;
  const HeapVector<Member<PerformanceServerTiming>>& serverTiming() const;
  const HeapVector<Member<PerformanceEntry>>& workerTiming() const;

  // Implements blink::mojom::blink::WorkerTimingContainer
  void AddPerformanceEntry(
      mojom::blink::PerformanceMarkOrMeasurePtr entry) override;
  void Trace(Visitor*) const override;

 protected:
  void BuildJSONValue(V8ObjectBuilder&) const override;

  virtual AtomicString AlpnNegotiatedProtocol() const;
  virtual AtomicString ConnectionInfo() const;

  base::TimeTicks TimeOrigin() const { return time_origin_; }
  bool CrossOriginIsolatedCapability() const {
    return cross_origin_isolated_capability_;
  }
  mojom::blink::CacheState CacheState() const { return cache_state_; }
  static uint64_t GetTransferSize(uint64_t encoded_body_size,
                                  mojom::blink::CacheState cache_state);

 private:
  // https://w3c.github.io/resource-timing/#dom-performanceresourcetiming-transfersize
  static const size_t kHeaderSize = 300;

  AtomicString GetNextHopProtocol(const AtomicString& alpn_negotiated_protocol,
                                  const AtomicString& connection_info) const;

  double WorkerReady() const;

  virtual ResourceLoadTiming* GetResourceLoadTiming() const;
  virtual bool AllowTimingDetails() const;
  virtual bool DidReuseConnection() const;
  virtual uint64_t GetTransferSize() const;
  virtual uint64_t GetEncodedBodySize() const;
  virtual uint64_t GetDecodedBodySize() const;

  AtomicString initiator_type_;
  AtomicString alpn_negotiated_protocol_;
  AtomicString connection_info_;
  base::TimeTicks time_origin_;
  bool cross_origin_isolated_capability_;
  scoped_refptr<ResourceLoadTiming> timing_;
  base::TimeTicks last_redirect_end_time_;
  base::TimeTicks response_end_;
  mojom::blink::RequestContextType context_type_ =
      mojom::blink::RequestContextType::UNSPECIFIED;
  network::mojom::RequestDestination request_destination_ =
      network::mojom::RequestDestination::kEmpty;
  mojom::blink::CacheState cache_state_ = mojom::blink::CacheState::kNone;
  uint64_t encoded_body_size_ = 0;
  uint64_t decoded_body_size_ = 0;
  bool did_reuse_connection_ = false;
  // Do not access allow_timing_details_ directly.  Instead use the
  // AllowTimingDetails() method which is overridden by some sub-classes.
  bool allow_timing_details_ = false;
  bool allow_redirect_details_ = false;
  bool allow_negative_value_ = false;
  bool is_secure_transport_ = false;
  HeapVector<Member<PerformanceServerTiming>> server_timing_;
  HeapVector<Member<PerformanceEntry>> worker_timing_;

  // Used for getting entries from a service worker to add to
  // PerformanceResourceTiming#workerTiming. Null when no service worker handles
  // a request for the resource.
  HeapMojoReceiver<mojom::blink::WorkerTimingContainer,
                   PerformanceResourceTiming>
      worker_timing_receiver_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_RESOURCE_TIMING_H_
