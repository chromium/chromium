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

#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "services/network/public/mojom/request_context_frame_type.mojom-shared.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom-blink.h"
#include "third_party/blink/public/platform/code_cache_loader.h"
#include "third_party/blink/public/platform/modules/fetch/fetch_api_request.mojom-shared.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/resource_request_blocked_reason.h"
#include "third_party/blink/public/platform/scheduler/web_resource_loading_task_runner_handle.h"
#include "third_party/blink/public/platform/web_application_cache_host.h"
#include "third_party/blink/public/platform/web_url_loader.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_info.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_priority.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_scheduler.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/network/content_security_policy_parsers.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/weborigin/security_violation_reporting_policy.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/noncopyable.h"

namespace blink {

class ClientHintsPreferences;
class KURL;
class MHTMLArchive;
class PlatformProbeSink;
class ResourceError;
class ResourceResponse;
class ResourceTimingInfo;
enum class ResourceType : uint8_t;

enum FetchResourceType { kFetchMainResource, kFetchSubresource };

// The FetchContext is an interface for performing context specific processing
// in response to events in the ResourceFetcher. The ResourceFetcher or its job
// class, ResourceLoader, may call the methods on a FetchContext.
//
// Any processing that depends on components outside platform/loader/fetch/
// should be implemented on a subclass of this interface, and then exposed to
// the ResourceFetcher via this interface.
class PLATFORM_EXPORT FetchContext
    : public GarbageCollectedFinalized<FetchContext> {
  WTF_MAKE_NONCOPYABLE(FetchContext);

 public:
  // This enum corresponds to blink::MessageSource. We have this not to
  // introduce any dependency to core/.
  //
  // Currently only kJSMessageSource, kSecurityMessageSource and
  // kOtherMessageSource are used, but not to impress readers that
  // AddConsoleMessage() call from FetchContext() should always use them,
  // which is not true, we ask users of the Add.*ConsoleMessage() methods
  // to explicitly specify the MessageSource to use.
  //
  // Extend this when needed.
  enum LogSource { kJSSource, kSecuritySource, kOtherSource };

  static FetchContext& NullInstance(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  virtual ~FetchContext() = default;

  virtual void Trace(blink::Visitor*);

  virtual bool IsFrameFetchContext() { return false; }

  virtual void AddAdditionalRequestHeaders(ResourceRequest&, FetchResourceType);

  // Called when the ResourceFetcher observes a data: URI load that contains an
  // octothorpe ('#') character. This is a temporary method to support an Intent
  // to Deprecate for spec incompliant handling of '#' characters in data URIs.
  //
  // TODO(crbug.com/123004): Remove once we have enough data for the I2D.
  virtual void RecordDataUriWithOctothorpe() {}

  // Returns the cache policy for the resource. ResourceRequest is not passed as
  // a const reference as a header needs to be added for doc.write blocking
  // intervention.
  virtual mojom::FetchCacheMode ResourceRequestCachePolicy(
      const ResourceRequest&,
      ResourceType,
      FetchParameters::DeferOption) const;

  virtual void DispatchDidChangeResourcePriority(unsigned long identifier,
                                                 ResourceLoadPriority,
                                                 int intra_priority_value);

  // This internally dispatches WebLocalFrameClient::willSendRequest and hooks
  // request interceptors like ServiceWorker and ApplicationCache.
  // This may modify the request.
  enum class RedirectType { kForRedirect, kNotForRedirect };
  virtual void PrepareRequest(ResourceRequest&, RedirectType);

  // The last callback before a request is actually sent to the browser process.
  // TODO(https://crbug.com/632580): make this take const ResourceRequest&.
  virtual void DispatchWillSendRequest(
      unsigned long identifier,
      ResourceRequest&,
      const ResourceResponse& redirect_response,
      ResourceType,
      const FetchInitiatorInfo& = FetchInitiatorInfo());
  virtual void DispatchDidLoadResourceFromMemoryCache(unsigned long identifier,
                                                      const ResourceRequest&,
                                                      const ResourceResponse&);
  enum class ResourceResponseType { kNotFromMemoryCache, kFromMemoryCache };
  virtual void DispatchDidReceiveResponse(
      unsigned long identifier,
      const ResourceResponse&,
      network::mojom::RequestContextFrameType,
      mojom::RequestContextType,
      Resource*,
      ResourceResponseType);
  virtual void DispatchDidReceiveData(unsigned long identifier,
                                      const char* data,
                                      int data_length);
  virtual void DispatchDidReceiveEncodedData(unsigned long identifier,
                                             int encoded_data_length);
  virtual void DispatchDidDownloadToBlob(unsigned long identifier,
                                         BlobDataHandle*);
  virtual void DispatchDidFinishLoading(unsigned long identifier,
                                        TimeTicks finish_time,
                                        int64_t encoded_data_length,
                                        int64_t decoded_body_length,
                                        bool should_report_corb_blocking);
  virtual void DispatchDidFail(const KURL&,
                               unsigned long identifier,
                               const ResourceError&,
                               int64_t encoded_data_length,
                               bool is_internal_request);

  virtual bool ShouldLoadNewResource(ResourceType) const { return false; }

  // Called when a resource load is first requested, which may not be when the
  // load actually begins.
  virtual void RecordLoadingActivity(const ResourceRequest&,
                                     ResourceType,
                                     const AtomicString& fetch_initiator_name);

  virtual void DidLoadResource(Resource*);

  virtual void AddResourceTiming(const ResourceTimingInfo&);
  virtual bool AllowImage(bool, const KURL&) const { return false; }
  virtual base::Optional<ResourceRequestBlockedReason> CanRequest(
      ResourceType,
      const ResourceRequest&,
      const KURL&,
      const ResourceLoaderOptions&,
      SecurityViolationReportingPolicy,
      ResourceRequest::RedirectStatus) const {
    return ResourceRequestBlockedReason::kOther;
  }
  virtual base::Optional<ResourceRequestBlockedReason> CheckCSPForRequest(
      mojom::RequestContextType,
      const KURL&,
      const ResourceLoaderOptions&,
      SecurityViolationReportingPolicy,
      ResourceRequest::RedirectStatus) const {
    return ResourceRequestBlockedReason::kOther;
  }

  virtual blink::mojom::ControllerServiceWorkerMode
  IsControlledByServiceWorker() const {
    return blink::mojom::ControllerServiceWorkerMode::kNoController;
  }
  virtual int64_t ServiceWorkerID() const { return -1; }
  virtual int ApplicationCacheHostID() const {
    return WebApplicationCacheHost::kAppCacheNoHostId;
  }

  virtual bool IsMainFrame() const { return true; }
  virtual bool DefersLoading() const { return false; }
  virtual bool IsLoadComplete() const { return false; }
  virtual bool UpdateTimingInfoForIFrameNavigation(ResourceTimingInfo*) {
    return false;
  }

  virtual void AddInfoConsoleMessage(const String&, LogSource) const;
  virtual void AddWarningConsoleMessage(const String&, LogSource) const;
  virtual void AddErrorConsoleMessage(const String&, LogSource) const;

  virtual const SecurityOrigin* GetSecurityOrigin() const { return nullptr; }

  // Populates the ResourceRequest using the given values and information
  // stored in the FetchContext implementation. Used by ResourceFetcher to
  // prepare a ResourceRequest instance at the start of resource loading.
  virtual void PopulateResourceRequest(ResourceType,
                                       const ClientHintsPreferences&,
                                       const FetchParameters::ResourceWidth&,
                                       ResourceRequest&);

  virtual MHTMLArchive* Archive() const { return nullptr; }

  PlatformProbeSink* GetPlatformProbeSink() const {
    return platform_probe_sink_;
  }

  virtual std::unique_ptr<WebURLLoader> CreateURLLoader(
      const ResourceRequest&,
      const ResourceLoaderOptions&) {
    NOTREACHED();
    return nullptr;
  }

  // Create a default code cache loader to fetch data from code caches.
  virtual std::unique_ptr<CodeCacheLoader> CreateCodeCacheLoader() {
    return Platform::Current()->CreateCodeCacheLoader();
  }

  // Returns the initial throttling policy used by the associated
  // ResourceLoadScheduler.
  virtual ResourceLoadScheduler::ThrottlingPolicy InitialLoadThrottlingPolicy()
      const {
    return ResourceLoadScheduler::ThrottlingPolicy::kNormal;
  }

  virtual bool IsDetached() const { return false; }

  // Obtains FrameScheduler instance that is used in the attached frame.
  // May return nullptr if a frame is not attached or detached.
  virtual FrameScheduler* GetFrameScheduler() const { return nullptr; }

  // Returns a task runner intended for loading tasks. Should work even in a
  // worker context, where FrameScheduler doesn't exist, but the returned
  // base::SingleThreadTaskRunner will not work after the context detaches
  // (after Detach() is called, this will return a generic timer suitable for
  // post-detach actions like keepalive requests.
  virtual scoped_refptr<base::SingleThreadTaskRunner> GetLoadingTaskRunner() {
    return task_runner_;
  }

  // TODO(altimin): This is used when creating a URLLoader, and
  // FetchContext::GetLoadingTaskRunner is used whenever asynchronous tasks
  // around resource loading are posted. Modify the code so that all
  // the tasks related to loading a resource use the resource loader handle's
  // task runner.
  virtual std::unique_ptr<blink::scheduler::WebResourceLoadingTaskRunnerHandle>
  CreateResourceLoadingTaskRunnerHandle() {
    return nullptr;
  }

  // Called when the underlying context is detached. Note that some
  // FetchContexts continue working after detached (e.g., for fetch() operations
  // with "keepalive" specified).
  // Returns a "detached" fetch context which can be null.
  virtual FetchContext* Detach() { return nullptr; }

  // Returns the updated priority of the resource based on the experiments that
  // may be currently enabled.
  virtual ResourceLoadPriority ModifyPriorityForExperiments(
      ResourceLoadPriority priority) const {
    return priority;
  }

  // Returns if the |resource_url| is identified as ad.
  virtual bool IsAdResource(const KURL& resource_url,
                            ResourceType type,
                            mojom::RequestContextType request_context) const {
    return false;
  }

  // Called when IdlenessDetector emits its network idle signal.
  virtual void DispatchNetworkQuiet() {}

 protected:
  explicit FetchContext(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

 private:
  Member<PlatformProbeSink> platform_probe_sink_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace blink

#endif
