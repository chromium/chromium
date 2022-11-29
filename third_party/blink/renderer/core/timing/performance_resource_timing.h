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

#include "base/time/time.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/timing/performance_mark_or_measure.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/timing/resource_timing.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/timing/performance_entry.h"
#include "third_party/blink/renderer/core/timing/performance_server_timing.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

enum RenderBlockingStatusType { kBlocking, kNonBlocking };

class ResourceLoadTiming;

class CORE_EXPORT PerformanceResourceTiming : public PerformanceEntry {
  DEFINE_WRAPPERTYPEINFO();
  friend class PerformanceResourceTimingTest;

 public:
  // This constructor is for PerformanceNavigationTiming.
  // Related doc: https://goo.gl/uNecAj.
  PerformanceResourceTiming(
      const AtomicString& name,
      base::TimeTicks time_origin,
      bool cross_origin_isolated_capability,
      mojom::blink::CacheState cache_state,
      bool is_secure_transport,
      HeapVector<Member<PerformanceServerTiming>> server_timing,
      ExecutionContext* context,
      network::mojom::NavigationDeliveryType delivery_type);
  PerformanceResourceTiming(const mojom::blink::ResourceTimingInfo&,
                            base::TimeTicks time_origin,
                            bool cross_origin_isolated_capability,
                            const AtomicString& initiator_type,
                            ExecutionContext* context);
  ~PerformanceResourceTiming() override;

  const AtomicString& entryType() const override;
  PerformanceEntryType EntryTypeEnum() const override;

  // Related doc: https://goo.gl/uNecAj.
  virtual AtomicString initiatorType() const;
  AtomicString deliveryType() const;
  AtomicString nextHopProtocol() const;
  virtual AtomicString renderBlockingStatus() const;
  virtual AtomicString contentType() const;
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
  uint16_t responseStatus() const;
  const HeapVector<Member<PerformanceServerTiming>>& serverTiming() const;

  void Trace(Visitor*) const override;

 protected:
  void BuildJSONValue(V8ObjectBuilder&) const override;

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

  virtual AtomicString AlpnNegotiatedProtocol() const;
  virtual AtomicString ConnectionInfo() const;

  virtual ResourceLoadTiming* GetResourceLoadTiming() const;
  virtual bool AllowTimingDetails() const;
  virtual bool DidReuseConnection() const;
  virtual uint64_t GetTransferSize() const;
  virtual uint64_t GetEncodedBodySize() const;
  virtual uint64_t GetDecodedBodySize() const;

  virtual mojom::blink::RequestContextType ContextType() const;
  virtual base::TimeTicks ResponseEnd() const;
  virtual base::TimeTicks LastRedirectEndTime() const;
  virtual bool AllowRedirectDetails() const;
  virtual bool AllowNegativeValue() const;
  virtual bool IsSecureTransport() const;

  // Do not access private fields directly. Use getter methods.
  AtomicString initiator_type_;
  AtomicString delivery_type_;
  AtomicString alpn_negotiated_protocol_;
  AtomicString connection_info_;
  AtomicString content_type_;
  RenderBlockingStatusType render_blocking_status_;
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
  const uint64_t encoded_body_size_ = 0;
  const uint64_t decoded_body_size_ = 0;
  const uint16_t response_status_ = 0;
  const bool did_reuse_connection_ = false;
  const bool allow_timing_details_ = false;
  const bool allow_redirect_details_ = false;
  const bool allow_negative_value_ = false;
  const bool is_secure_transport_ = false;
  HeapVector<Member<PerformanceServerTiming>> server_timing_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_RESOURCE_TIMING_H_
