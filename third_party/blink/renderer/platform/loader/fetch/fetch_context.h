/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_FETCH_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_FETCH_CONTEXT_H_

#include <memory>

#include "base/macros.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/loader/request_context_frame_type.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/timing/worker_timing_container.mojom-blink-forward.h"
#include "third_party/blink/public/platform/resource_load_info_notifier_wrapper.h"
#include "third_party/blink/public/platform/resource_request_blocked_reason.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_info.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_priority.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/network/content_security_policy_parsers.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/weborigin/reporting_disposition.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

enum class ResourceType : uint8_t;
class ClientHintsPreferences;
class FeaturePolicy;
class KURL;
struct ResourceLoaderOptions;
class ResourceTimingInfo;
class WebScopedVirtualTimePauser;

// The FetchContext is an interface for performing context specific processing
// in response to events in the ResourceFetcher. The ResourceFetcher or its job
// class, ResourceLoader, may call the methods on a FetchContext.
//
// Any processing that depends on components outside platform/loader/fetch/
// should be implemented on a subclass of this interface, and then exposed to
// the ResourceFetcher via this interface.
class PLATFORM_EXPORT FetchContext : public GarbageCollected<FetchContext> {
 public:
  FetchContext() = default;

  static FetchContext& NullInstance() {
    return *MakeGarbageCollected<FetchContext>();
  }

  virtual ~FetchContext() = default;

  virtual void Trace(Visitor*) const {}

  virtual void AddAdditionalRequestHeaders(ResourceRequest&);

  // Returns the cache policy for the resource. ResourceRequest is not passed as
  // a const reference as a header needs to be added for doc.write blocking
  // intervention.
  virtual mojom::FetchCacheMode ResourceRequestCachePolicy(
      const ResourceRequest&,
      ResourceType,
      FetchParameters::DeferOption) const;

  // This internally dispatches WebLocalFrameClient::WillSendRequest and hooks
  // request interceptors like ServiceWorker and ApplicationCache.
  // This may modify the request.
  // |virtual_time_pauser| is an output parameter. PrepareRequest may
  // create a new WebScopedVirtualTimePauser and set it to
  // |virtual_time_pauser|.
  // This is called on initial and every redirect request.
  virtual void PrepareRequest(ResourceRequest&,
                              const FetchInitiatorInfo&,
                              WebScopedVirtualTimePauser& virtual_time_pauser,
                              ResourceType);

  // WARNING: |info| can be modified by the implementation of this method
  // despite the fact that it is given as const-ref. Namely, if
  // |worker_timing_receiver_| is set implementations may take (move out) the
  // field.
  // TODO(shimazu): Fix this. Eventually ResourceTimingInfo should become a mojo
  // struct and this should take a moved-value of it.
  virtual void AddResourceTiming(const ResourceTimingInfo&);
  virtual bool AllowImage(bool, const KURL&) const { return false; }
  virtual base::Optional<ResourceRequestBlockedReason> CanRequest(
      ResourceType,
      const ResourceRequest&,
      const KURL&,
      const ResourceLoaderOptions&,
      ReportingDisposition,
      const base::Optional<ResourceRequest::RedirectInfo>& redirect_info)
      const {
    return ResourceRequestBlockedReason::kOther;
  }
  virtual base::Optional<ResourceRequestBlockedReason> CheckCSPForRequest(
      mojom::blink::RequestContextType,
      network::mojom::RequestDestination request_destination,
      const KURL&,
      const ResourceLoaderOptions&,
      ReportingDisposition,
      const KURL& url_before_redirects,
      ResourceRequest::RedirectStatus) const {
    return ResourceRequestBlockedReason::kOther;
  }

  // Populates the ResourceRequest using the given values and information
  // stored in the FetchContext implementation. Used by ResourceFetcher to
  // prepare a ResourceRequest instance at the start of resource loading.
  virtual void PopulateResourceRequest(ResourceType,
                                       const ClientHintsPreferences&,
                                       const FetchParameters::ResourceWidth&,
                                       ResourceRequest&,
                                       const ResourceLoaderOptions&);

  // Called when the underlying context is detached. Note that some
  // FetchContexts continue working after detached (e.g., for fetch() operations
  // with "keepalive" specified).
  // Returns a "detached" fetch context which cannot be null.
  virtual FetchContext* Detach() {
    return MakeGarbageCollected<FetchContext>();
  }

  virtual const FeaturePolicy* GetFeaturePolicy() const { return nullptr; }

  // Determine if the request is on behalf of an advertisement. If so, return
  // true.
  virtual bool CalculateIfAdSubresource(
      const ResourceRequest& resource_request,
      ResourceType type,
      const FetchInitiatorInfo& initiator_info) {
    return false;
  }

  virtual PreviewsState previews_state() const {
    return PreviewsTypes::kPreviewsUnspecified;
  }

  // Returns a receiver corresponding to a request with |request_id|.
  // Null if the request has not been intercepted by a service worker.
  virtual mojo::PendingReceiver<mojom::blink::WorkerTimingContainer>
  TakePendingWorkerTimingReceiver(int request_id);

  // Returns a wrapper of ResourceLoadInfoNotifier to notify loading stats.
  virtual std::unique_ptr<ResourceLoadInfoNotifierWrapper>
  CreateResourceLoadInfoNotifierWrapper() {
    return nullptr;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FetchContext);
};

}  // namespace blink

#endif
