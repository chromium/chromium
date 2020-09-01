/*
 * Copyright (C) 2009, 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013, Intel Corporation
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_THREADABLE_LOADER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_THREADABLE_LOADER_H_

#include <memory>

#include "base/macros.h"
#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/raw_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/network/http_header_map.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/weborigin/referrer.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExecutionContext;
class KURL;
class LocalFrame;
class ResourceRequest;
class SecurityOrigin;
class ThreadableLoaderClient;

// Useful for doing loader operations from any thread (not threadsafe, just able
// to run on threads other than the main thread).
//
// Can perform requests either synchronously or asynchronously. Requests are
// asynchronous by default, and this behavior can be controlled by passing
// a ResourceLoaderOptions with synchronous_policy == kRequestSynchronously to
// the constructor.
// In either case, Start() must be called to actaully begin the request.
class CORE_EXPORT ThreadableLoader final
    : public GarbageCollected<ThreadableLoader>,
      private RawResourceClient {
 public:
  // ThreadableLoaderClient methods are never called before Start() call.
  //
  // Loading is separated into the constructor and the Start() method in order
  // to:
  // - reduce work done in a constructor
  // - not to ask the users to handle failures in the constructor and other
  //   async failures separately
  //
  // Loading completes when one of the following methods are called:
  // - DidFinishLoading()
  // - DidFail()
  // - DidFailAccessControlCheck()
  // - DidFailRedirectCheck()
  // After any of these methods is called, the loader won't call any of the
  // ThreadableLoaderClient methods.
  //
  // When ThreadableLoader::Cancel() is called,
  // ThreadableLoaderClient::DidFail() is called with a ResourceError
  // with IsCancellation() returning true, if any of DidFinishLoading()
  // or DidFail.*() methods have not been called yet. (DidFail() may be
  // called with a ResourceError with IsCancellation() returning true
  // also for cancellation happened inside the loader.)
  //
  // ThreadableLoaderClient methods may call Cancel().
  //
  // The specified ResourceFetcher if non-null, or otherwise
  // ExecutionContext::Fetcher() is used.
  ThreadableLoader(ExecutionContext&,
                   ThreadableLoaderClient*,
                   const ResourceLoaderOptions&,
                   ResourceFetcher* = nullptr);
  ~ThreadableLoader() override;

  // Exposed for testing. Code outside this class should not call this function.
  static std::unique_ptr<ResourceRequest>
  CreateAccessControlPreflightRequestForTesting(const ResourceRequest&);

  // Must be called to actually begin the request.
  void Start(ResourceRequest);

  // A ThreadableLoader may have a timeout specified. It is possible, in some
  // cases, for the timeout to be overridden after the request is sent (for
  // example, XMLHttpRequests may override their timeout setting after sending).
  //
  // If the request has already started, the new timeout will be relative to the
  // time the request started.
  //
  // Passing a timeout of zero means there should be no timeout.
  void SetTimeout(const base::TimeDelta& timeout);

  void Cancel();

  // Detach the loader from the request. This ffunction is for "keepalive"
  // requests. No notification will be sent to the client, but the request
  // will be processed.
  void Detach();

  void SetDefersLoading(bool);

  void Trace(Visitor* visitor) const override;

 private:
  class AssignOnScopeExit;
  class DetachedClient;

  static std::unique_ptr<ResourceRequest> CreateAccessControlPreflightRequest(
      const ResourceRequest&,
      const SecurityOrigin*);

  void Clear();

  // ResourceClient
  void NotifyFinished(Resource*) override;

  String DebugName() const override { return "ThreadableLoader"; }

  // RawResourceClient
  void DataSent(Resource*,
                uint64_t bytes_sent,
                uint64_t total_bytes_to_be_sent) override;
  void ResponseReceived(Resource*, const ResourceResponse&) override;
  void ResponseBodyReceived(Resource*, BytesConsumer& body) override;
  void SetSerializedCachedMetadata(Resource*, const uint8_t*, size_t) override;
  void DataReceived(Resource*, const char* data, size_t data_length) override;
  bool RedirectReceived(Resource*,
                        const ResourceRequest&,
                        const ResourceResponse&) override;
  void RedirectBlocked() override;
  void DataDownloaded(Resource*, uint64_t) override;
  void DidDownloadToBlob(Resource*, scoped_refptr<BlobDataHandle>) override;

  // Notify Inspector and log to console about resource response. Use this
  // method if response is not going to be finished normally.
  void ReportResponseReceived(uint64_t identifier, const ResourceResponse&);

  void DidTimeout(TimerBase*);
  // Calls the appropriate loading method according to policy and data about
  // origin. Only for handling the initial load (including fallback after
  // consulting ServiceWorker).
  void DispatchInitialRequest(ResourceRequest&);
  void MakeCrossOriginAccessRequest(const ResourceRequest&);

  // Loads m_fallbackRequestForServiceWorker.
  void LoadFallbackRequestForServiceWorker();
  // Issues a CORS preflight.
  void LoadPreflightRequest(const ResourceRequest&,
                            const ResourceLoaderOptions&);
  // Loads actual_request_.
  void LoadActualRequest();
  // Clears actual_request_ and reports access control check failure to
  // m_client.
  void HandlePreflightFailure(const KURL&, const network::CorsErrorStatus&);
  // Investigates the response for the preflight request. If successful,
  // the actual request will be made later in NotifyFinished().
  void HandlePreflightResponse(const ResourceResponse&);

  void DispatchDidFail(const ResourceError&);

  void PrepareCrossOriginRequest(ResourceRequest&) const;

  // This method modifies the ResourceRequest by calling
  // SetAllowStoredCredentials() on it based on same-origin-ness and the
  // credentials mode.
  //
  // This method configures the ResourceLoaderOptions so that the underlying
  // ResourceFetcher doesn't perform some part of the CORS logic since this
  // class performs it by itself.
  void LoadRequest(ResourceRequest&, ResourceLoaderOptions);

  const SecurityOrigin* GetSecurityOrigin() const;

  // Returns null if the loader is not associated with a frame.
  // TODO(kinuko): Remove dependency to frame.
  LocalFrame* GetFrame() const;

  Member<ThreadableLoaderClient> client_;
  Member<ExecutionContext> execution_context_;
  Member<ResourceFetcher> resource_fetcher_;

  base::TimeDelta timeout_;
  // Some items may be overridden by m_forceDoNotAllowStoredCredentials and
  // m_securityOrigin. In such a case, build a ResourceLoaderOptions with
  // up-to-date values from them and this variable, and use it.
  const ResourceLoaderOptions resource_loader_options_;

  // Always true. TODO(1053866): Remove this flag and code hidden by this flag.
  const bool out_of_blink_cors_;

  // Corresponds to the CORS flag in the Fetch spec.
  bool cors_flag_ = false;
  scoped_refptr<const SecurityOrigin> security_origin_;
  scoped_refptr<const SecurityOrigin> original_security_origin_;

  const bool async_;

  // Holds the original request context (used for sanity checks).
  mojom::RequestContextType request_context_;

  // Saved so that we can use the original value for the modes in
  // ResponseReceived() where |resource| might be a reused one (e.g. preloaded
  // resource) which can have different modes.
  network::mojom::RequestMode request_mode_;
  network::mojom::CredentialsMode credentials_mode_;

  // Holds the original request for fallback in case the Service Worker
  // does not respond.
  ResourceRequest fallback_request_for_service_worker_;

  // Holds the original request and options for it during preflight request
  // handling phase.
  ResourceRequest actual_request_;
  ResourceLoaderOptions actual_options_{nullptr /* world */};
  network::mojom::FetchResponseType response_tainting_ =
      network::mojom::FetchResponseType::kBasic;

  KURL initial_request_url_;
  KURL last_request_url_;

  // stores request headers in case of a cross-origin redirect.
  HTTPHeaderMap request_headers_;

  TaskRunnerTimer<ThreadableLoader> timeout_timer_;
  base::TimeTicks
      request_started_;  // Time an asynchronous fetch request is started

  // Max number of times that this ThreadableLoader can follow.
  int redirect_limit_;

  network::mojom::RedirectMode redirect_mode_;

  // Holds the referrer after a redirect response was received. This referrer is
  // used to populate the HTTP Referer header when following the redirect.
  bool override_referrer_;
  bool report_upload_progress_ = false;
  Referrer referrer_after_redirect_;

  bool detached_ = false;

  RawResourceClientStateChecker checker_;

  DISALLOW_COPY_AND_ASSIGN(ThreadableLoader);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_THREADABLE_LOADER_H_
