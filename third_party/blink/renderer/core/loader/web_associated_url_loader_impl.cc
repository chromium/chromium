/*
 * Copyright (C) 2010, 2011, 2012 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/loader/web_associated_url_loader_impl.h"

#include <limits>
#include <memory>
#include <optional>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "services/network/public/cpp/request_destination.h"
#include "services/network/public/cpp/request_mode.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/resource_request_blocked_reason.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_http_header_visitor.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_associated_url_loader_client.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/loader/threadable_loader.h"
#include "third_party/blink/renderer/core/loader/threadable_loader_client.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_request.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_response.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/loader/cors/cors.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_utils.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/weborigin/referrer.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

class HTTPRequestHeaderValidator : public WebHTTPHeaderVisitor {
 public:
  HTTPRequestHeaderValidator() : is_safe_(true) {}
  HTTPRequestHeaderValidator(const HTTPRequestHeaderValidator&) = delete;
  HTTPRequestHeaderValidator& operator=(const HTTPRequestHeaderValidator&) =
      delete;
  ~HTTPRequestHeaderValidator() override = default;

  void VisitHeader(const WebString& name, const WebString& value) override;
  bool IsSafe() const { return is_safe_; }

 private:
  bool is_safe_;
};

void HTTPRequestHeaderValidator::VisitHeader(const WebString& name,
                                             const WebString& value) {
  is_safe_ = is_safe_ && IsValidHTTPToken(name) &&
             !cors::IsForbiddenRequestHeader(name, value) &&
             IsValidHTTPHeaderValue(value);
}

}  // namespace

// This class bridges the interface differences between WebCore and WebKit
// loader clients.
// It forwards its ThreadableLoaderClient notifications to a
// WebAssociatedURLLoaderClient.
class WebAssociatedURLLoaderImpl::ClientAdapter final
    : public GarbageCollected<ClientAdapter>,
      public ThreadableLoaderClient {
 public:
  ClientAdapter(WebAssociatedURLLoaderImpl*,
                WebAssociatedURLLoaderClient*,
                const WebAssociatedURLLoaderOptions&,
                network::mojom::RequestMode,
                network::mojom::CredentialsMode,
                scoped_refptr<base::SingleThreadTaskRunner>);
  ClientAdapter(const ClientAdapter&) = delete;
  ClientAdapter& operator=(const ClientAdapter&) = delete;

  // ThreadableLoaderClient
  void DidSendData(uint64_t /*bytesSent*/,
                   uint64_t /*totalBytesToBeSent*/) override;
  void DidReceiveResponse(uint64_t, const ResourceResponse&) override;
  void DidDownloadData(uint64_t /*dataLength*/) override;
  void DidReceiveData(base::span<const char> /*data*/) override;
  void DidFinishLoading(uint64_t /*identifier*/) override;
  void DidFail(uint64_t /*identifier*/, const ResourceError&) override;
  void DidFailRedirectCheck(uint64_t /*identifier*/) override;

  // ThreadableLoaderClient
  bool WillFollowRedirect(
      uint64_t /*identifier*/,
      const KURL& /*new_url*/,
      const ResourceResponse& /*redirect_response*/) override;

  // Sets an error to be reported back to the client, asynchronously.
  void SetDelayedError(const ResourceError&);

  // Enables forwarding of error notifications to the
  // WebAssociatedURLLoaderClient. These
  // must be deferred until after the call to
  // WebAssociatedURLLoader::loadAsynchronously() completes.
  void EnableErrorNotifications();

  // Stops loading and releases the ThreadableLoader as early as
  // possible.
  WebAssociatedURLLoaderClient* ReleaseClient() {
    WebAssociatedURLLoaderClient* client = client_;
    client_ = nullptr;
    return client;
  }

  void Trace(Visitor* visitor) const final {
    visitor->Trace(error_timer_);
    ThreadableLoaderClient::Trace(visitor);
  }

 private:
  void NotifyError(TimerBase*);

  WebAssociatedURLLoaderImpl* loader_;
  WebAssociatedURLLoaderClient* client_;
  WebAssociatedURLLoaderOptions options_;
  network::mojom::RequestMode request_mode_;
  network::mojom::CredentialsMode credentials_mode_;
  std::optional<WebURLError> error_;

  HeapTaskRunnerTimer<ClientAdapter> error_timer_;
  bool enable_error_notifications_;
  bool did_fail_;
};

WebAssociatedURLLoaderImpl::ClientAdapter::ClientAdapter(
    WebAssociatedURLLoaderImpl* loader,
    WebAssociatedURLLoaderClient* client,
    const WebAssociatedURLLoaderOptions& options,
    network::mojom::RequestMode request_mode,
    network::mojom::CredentialsMode credentials_mode,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : loader_(loader),
      client_(client),
      options_(options),
      request_mode_(request_mode),
      credentials_mode_(credentials_mode),
      error_timer_(std::move(task_runner), this, &ClientAdapter::NotifyError),
      enable_error_notifications_(false),
      did_fail_(false) {
  DCHECK(loader_);
  DCHECK(client_);
}

bool WebAssociatedURLLoaderImpl::ClientAdapter::WillFollowRedirect(
    uint64_t identifier,
    const KURL& new_url,
    const ResourceResponse& redirect_response) {
  if (!client_)
    return true;

  WebURL wrapped_new_url(new_url);
  WrappedResourceResponse wrapped_redirect_response(redirect_response);
  return client_->WillFollowRedirect(wrapped_new_url,
                                     wrapped_redirect_response);
}

void WebAssociatedURLLoaderImpl::ClientAdapter::DidSendData(
    uint64_t bytes_sent,
    uint64_t total_bytes_to_be_sent) {
  if (!client_)
    return;

  client_->DidSendData(bytes_sent, total_bytes_to_be_sent);
}

void WebAssociatedURLLoaderImpl::ClientAdapter::DidReceiveResponse(
    uint64_t,
    const ResourceResponse& response) {
  if (!client_)
    return;

  if (options_.expose_all_response_headers ||
      (request_mode_ != network::mojom::RequestMode::kCors &&
       request_mode_ !=
           network::mojom::RequestMode::kCorsWithForcedPreflight)) {
    // Use the original ResourceResponse.
    client_->DidReceiveResponse(WrappedResourceResponse(response));
    return;
  }

  HTTPHeaderSet exposed_headers =
      cors::ExtractCorsExposedHeaderNamesList(credentials_mode_, response);
  HTTPHeaderSet blocked_headers;
  for (const auto& header : response.HttpHeaderFields()) {
    if (FetchUtils::IsForbiddenResponseHeaderName(header.key) ||
        (!cors::IsCorsSafelistedResponseHeader(header.key) &&
         exposed_headers.find(header.key.Ascii()) == exposed_headers.end()))
      blocked_headers.insert(header.key.Ascii());
  }

  if (blocked_headers.empty()) {
    // Use the original ResourceResponse.
    client_->DidReceiveResponse(WrappedResourceResponse(response));
    return;
  }

  // If there are blocked headers, copy the response so we can remove them.
  WebURLResponse validated_response = WrappedResourceResponse(response);
  for (const auto& header : blocked_headers)
    validated_response.ClearHttpHeaderField(WebString::FromASCII(header));
  client_->DidReceiveResponse(validated_response);
}

void WebAssociatedURLLoaderImpl::ClientAdapter::DidDownloadData(
    uint64_t data_length) {
  if (!client_)
    return;

  client_->DidDownloadData(data_length);
}

void WebAssociatedURLLoaderImpl::ClientAdapter::DidReceiveData(
    base::span<const char> data) {
  if (!client_) {
    return;
  }

  client_->DidReceiveData(data);
}

void WebAssociatedURLLoaderImpl::ClientAdapter::DidFinishLoading(
    uint64_t identifier) {
  if (!client_)
    return;

  loader_->ClientAdapterDone();

  ReleaseClient()->DidFinishLoading();
  // |this| may be dead here.
}

void WebAssociatedURLLoaderImpl::ClientAdapter::DidFail(
    uint64_t,
    const ResourceError& error) {
  if (!client_)
    return;

  loader_->ClientAdapterDone();

  did_fail_ = true;
  error_ = static_cast<WebURLError>(error);
  if (enable_error_notifications_)
    NotifyError(&error_timer_);
}

void WebAssociatedURLLoaderImpl::ClientAdapter::DidFailRedirectCheck(
    uint64_t identifier) {
  DidFail(identifier, ResourceError::Failure(NullURL()));
}

void WebAssociatedURLLoaderImpl::ClientAdapter::EnableErrorNotifications() {
  enable_error_notifications_ = true;
  // If an error has already been received, start a timer to report it to the
  // client after WebAssociatedURLLoader::loadAsynchronously has returned to the
  // caller.
  if (did_fail_)
    error_timer_.StartOneShot(base::TimeDelta(), FROM_HERE);
}

void WebAssociatedURLLoaderImpl::ClientAdapter::NotifyError(TimerBase* timer) {
  DCHECK_EQ(timer, &error_timer_);

  if (client_) {
    DCHECK(error_);
    ReleaseClient()->DidFail(*error_);
  }
  // |this| may be dead here.
}

class WebAssociatedURLLoaderImpl::Observer final
    : public GarbageCollected<Observer>,
      public ExecutionContextLifecycleObserver {
 public:
  Observer(WebAssociatedURLLoaderImpl* parent, ExecutionContext* context)
      : ExecutionContextLifecycleObserver(context), parent_(parent) {}

  void Dispose() {
    parent_ = nullptr;
    // TODO(keishi): Remove IsIteratingOverObservers() check when
    // HeapObserverList() supports removal while iterating.
    if (!GetExecutionContext()
             ->ContextLifecycleObserverSet()
             .IsIteratingOverObservers()) {
      SetExecutionContext(nullptr);
    }
  }

  void ContextDestroyed() override {
    if (parent_)
      parent_->ContextDestroyed();
  }

  void Trace(Visitor* visitor) const override {
    ExecutionContextLifecycleObserver::Trace(visitor);
  }

  WebAssociatedURLLoaderImpl* parent_;
};

WebAssociatedURLLoaderImpl::WebAssociatedURLLoaderImpl(
    ExecutionContext* context,
    const WebAssociatedURLLoaderOptions& options)
    : client_(nullptr),
      options_(options),
      observer_(MakeGarbageCollected<Observer>(this, context)) {}

WebAssociatedURLLoaderImpl::~WebAssociatedURLLoaderImpl() {
  Cancel();
}

void WebAssociatedURLLoaderImpl::LoadAsynchronously(
    const WebURLRequest& request,
    WebAssociatedURLLoaderClient* client) {
  DCHECK(!client_);
  DCHECK(!loader_);
  DCHECK(!client_adapter_);

  DCHECK(client);
  client_ = client;

  if (!observer_) {
    ReleaseClient()->DidFail(
        WebURLError(ResourceError::CancelledError(KURL())));
    return;
  }

  bool allow_load = true;
  WebURLRequest new_request;
  new_request.CopyFrom(request);
  if (options_.untrusted_http) {
    WebString method = new_request.HttpMethod();
    allow_load =
        IsValidHTTPToken(method) && !FetchUtils::IsForbiddenMethod(method);
    if (allow_load) {
      new_request.SetHttpMethod(FetchUtils::NormalizeMethod(method));
      HTTPRequestHeaderValidator validator;
      new_request.VisitHttpHeaderFields(&validator);

      // The request's referrer string is not stored as a header, so we must
      // consult it separately, if set.
      if (request.ReferrerString() !=
          blink::WebString(Referrer::ClientReferrerString())) {
        DCHECK(cors::IsForbiddenRequestHeader("Referer", ""));
        // `Referer` is a forbidden header name, so we must disallow this to
        // load.
        allow_load = false;
      }

      allow_load = allow_load && validator.IsSafe();
    }
  }
  new_request.ToMutableResourceRequest().SetCorsPreflightPolicy(
      options_.preflight_policy);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      observer_->GetExecutionContext()->GetTaskRunner(
          TaskType::kInternalLoading);
  client_adapter_ = MakeGarbageCollected<ClientAdapter>(
      this, client, options_, request.GetMode(), request.GetCredentialsMode(),
      std::move(task_runner));

  if (allow_load) {
    ResourceLoaderOptions resource_loader_options(
        observer_->GetExecutionContext()->GetCurrentWorld());
    resource_loader_options.data_buffering_policy = kDoNotBufferData;

    if (options_.grant_universal_access) {
      const auto request_mode = new_request.GetMode();
      DCHECK(request_mode == network::mojom::RequestMode::kNoCors ||
             request_mode == network::mojom::RequestMode::kNavigate);
      // Some callers, notablly flash, with |grant_universal_access| want to
      // have an origin matching with referrer.
      KURL referrer(request.ToResourceRequest().ReferrerString());
      scoped_refptr<SecurityOrigin> origin = SecurityOrigin::Create(referrer);
      origin->GrantUniversalAccess();
      new_request.ToMutableResourceRequest().SetRequestorOrigin(origin);
    }

    ResourceRequest& webcore_request = new_request.ToMutableResourceRequest();
    mojom::blink::RequestContextType context =
        webcore_request.GetRequestContext();
    if (context == mojom::blink::RequestContextType::UNSPECIFIED) {
      // TODO(yoav): We load URLs without setting a TargetType (and therefore a
      // request context) in several places in content/
      // (P2PPortAllocatorSession::AllocateLegacyRelaySession, for example).
      // Remove this once those places are patched up.
      new_request.SetRequestContext(mojom::blink::RequestContextType::INTERNAL);
      new_request.SetRequestDestination(
          network::mojom::RequestDestination::kEmpty);
    } else if (context == mojom::blink::RequestContextType::VIDEO) {
      resource_loader_options.initiator_info.name =
          fetch_initiator_type_names::kVideo;
    } else if (context == mojom::blink::RequestContextType::AUDIO) {
      resource_loader_options.initiator_info.name =
          fetch_initiator_type_names::kAudio;
    }

    loader_ = MakeGarbageCollected<ThreadableLoader>(
        *observer_->GetExecutionContext(), client_adapter_,
        resource_loader_options);
    loader_->Start(std::move(webcore_request));
  }

  if (!loader_) {
    client_adapter_->DidFail(
        0 /* identifier */,
        ResourceError::CancelledDueToAccessCheckError(
            request.Url(), ResourceRequestBlockedReason::kOther));
  }
  client_adapter_->EnableErrorNotifications();
}

void WebAssociatedURLLoaderImpl::Cancel() {
  DisposeObserver();
  CancelLoader();
  ReleaseClient();
}

void WebAssociatedURLLoaderImpl::ClientAdapterDone() {
  DisposeObserver();
  ReleaseClient();
}

void WebAssociatedURLLoaderImpl::CancelLoader() {
  if (!client_adapter_)
    return;

  // Prevent invocation of the WebAssociatedURLLoaderClient methods.
  client_adapter_->ReleaseClient();

  if (loader_) {
    loader_->Cancel();
    loader_ = nullptr;
  }
  client_adapter_ = nullptr;
}

void WebAssociatedURLLoaderImpl::SetDefersLoading(bool defers_loading) {
  if (loader_)
    loader_->SetDefersLoading(defers_loading);
}

void WebAssociatedURLLoaderImpl::SetLoadingTaskRunner(
    base::SingleThreadTaskRunner*) {
  // TODO(alexclarke): Maybe support this one day if it proves worthwhile.
}

void WebAssociatedURLLoaderImpl::ContextDestroyed() {
  DisposeObserver();
  CancelLoader();

  if (!client_)
    return;

  ReleaseClient()->DidFail(WebURLError(ResourceError::CancelledError(KURL())));
  // |this| may be dead here.
}

void WebAssociatedURLLoaderImpl::DisposeObserver() {
  if (!observer_)
    return;

  // TODO(tyoshino): Remove this assert once Document is fixed so that
  // contextDestroyed() is invoked for all kinds of Documents.
  //
  // Currently, the method of detecting Document destruction implemented here
  // doesn't work for all kinds of Documents. In case we reached here after
  // the Oilpan is destroyed, we just crash the renderer process to prevent
  // UaF.
  //
  // We could consider just skipping the rest of code in case
  // ThreadState::current() is null. However, the fact we reached here
  // without cancelling the loader means that it's possible there're some
  // non-Blink non-on-heap objects still facing on-heap Blink objects. E.g.
  // there could be a URLLoader instance behind the ThreadableLoader instance.
  // So, for safety, we chose to just crash here.
  CHECK(ThreadState::Current());

  observer_->Dispose();
  observer_ = nullptr;
}

}  // namespace blink
