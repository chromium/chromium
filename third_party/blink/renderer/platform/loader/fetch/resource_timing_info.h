/*
 * Copyright (C) 2013 Intel Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_RESOURCE_TIMING_INFO_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_RESOURCE_TIMING_INFO_H_

#include "base/time/time.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/timing/resource_timing.mojom-blink.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class PLATFORM_EXPORT ResourceTimingInfo
    : public RefCounted<ResourceTimingInfo> {
  USING_FAST_MALLOC(ResourceTimingInfo);

 public:
  static scoped_refptr<ResourceTimingInfo> Create(
      const AtomicString& type,
      const base::TimeTicks time,
      mojom::blink::RequestContextType context,
      network::mojom::RequestDestination destination,
      network::mojom::RequestMode mode) {
    return base::AdoptRef(
        new ResourceTimingInfo(type, time, context, destination, mode));
  }
  ResourceTimingInfo(const ResourceTimingInfo&) = delete;
  ResourceTimingInfo& operator=(const ResourceTimingInfo&) = delete;

  base::TimeTicks InitialTime() const { return initial_time_; }

  void SetInitiatorType(const AtomicString& type) { type_ = type; }
  const AtomicString& InitiatorType() const { return type_; }

  void SetRenderBlockingStatus(bool render_blocking_status) {
    render_blocking_status_ = render_blocking_status;
  }
  bool RenderBlockingStatus() const { return render_blocking_status_; }

  void SetLoadResponseEnd(base::TimeTicks time) { load_response_end_ = time; }
  base::TimeTicks LoadResponseEnd() const { return load_response_end_; }

  void SetInitialURL(const KURL& url) { initial_url_ = url; }
  const KURL& InitialURL() const { return initial_url_; }

  void SetFinalResponse(const ResourceResponse& response) {
    final_response_ = response;
  }
  const ResourceResponse& FinalResponse() const { return final_response_; }
  uint16_t responseStatus() const { return response_status_; }

  void AddRedirect(const ResourceResponse& redirect_response,
                   const KURL& new_url);
  const Vector<ResourceResponse>& RedirectChain() const {
    return redirect_chain_;
  }

  mojom::blink::CacheState CacheState() const {
    return final_response_.CacheState();
  }

  const AtomicString& ContentType() const { return content_type_; }

  // The timestamps in PerformanceResourceTiming are measured relative from the
  // time origin. In most cases these timestamps must be positive value, so we
  // use 0 for invalid negative values. But the timestamps for Service Worker
  // navigation preload requests may be negative, because these requests may
  // be started before the service worker started. We set this flag true, to
  // support such case.
  void SetNegativeAllowed(bool negative_allowed) {
    negative_allowed_ = negative_allowed;
  }
  bool NegativeAllowed() const { return negative_allowed_; }
  mojom::blink::RequestContextType ContextType() const { return context_type_; }
  network::mojom::RequestDestination RequestDestination() const {
    return request_destination_;
  }
  network::mojom::RequestMode RequestMode() const { return request_mode_; }

 private:
  ResourceTimingInfo(const AtomicString& type,
                     const base::TimeTicks time,
                     mojom::blink::RequestContextType context_type,
                     network::mojom::RequestDestination request_destination,
                     network::mojom::RequestMode request_mode)
      : type_(type),
        initial_time_(time),
        context_type_(context_type),
        request_destination_(request_destination),
        request_mode_(request_mode) {}

  AtomicString type_;
  bool render_blocking_status_ = false;
  AtomicString content_type_;
  base::TimeTicks initial_time_;
  mojom::blink::RequestContextType context_type_;
  network::mojom::RequestDestination request_destination_;
  network::mojom::RequestMode request_mode_;
  base::TimeTicks load_response_end_;
  KURL initial_url_;
  ResourceResponse final_response_;
  uint16_t response_status_ = 0;
  Vector<ResourceResponse> redirect_chain_;
  bool has_cross_origin_redirect_ = false;
  bool negative_allowed_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_RESOURCE_TIMING_INFO_H_
