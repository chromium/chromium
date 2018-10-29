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

#include "third_party/blink/renderer/core/exported/web_associated_url_loader_impl.h"

#include <limits>
#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/resource_request_blocked_reason.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_cors.h"
#include "third_party/blink/public/platform/web_http_header_visitor.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_associated_url_loader_client.h"
#include "third_party/blink/renderer/core/dom/context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/loader/threadable_loader.h"
#include "third_party/blink/renderer/core/loader/threadable_loader_client.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_request.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_response.h"
#include "third_party/blink/renderer/platform/loader/cors/cors.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_utils.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

class HTTPRequestHeaderValidator : public WebHTTPHeaderVisitor {
 public:
  HTTPRequestHeaderValidator() : is_safe_(true) {}
  ~HTTPRequestHeaderValidator() override = default;

  void VisitHeader(const WebString& name, const WebString& value) override;
  bool IsSafe() const { return is_safe_; }

 private:
  bool is_safe_;

  DISALLOW_COPY_AND_ASSIGN(HTTPRequestHeaderValidator);
};

void HTTPRequestHeaderValidator::VisitHeader(const WebString& name,
                                             const WebString& value) {
  is_safe_ = is_safe_ && IsValidHTTPToken(name) &&
             !CORS::IsForbiddenHeaderName(name) &&
             IsValidHTTPHeaderValue(value);
}

}  // namespace

// This class bridges the interface differences between WebCore and WebKit
// loader clients.
// It forwards its ThreadableLoaderClient notifications to a
// WebAssociatedURLLoaderClient.
class WebAssociatedURLLoaderImpl::ClientAdapter final
    : public ThreadableLoaderClient {
 public:
  static std::unique_ptr<ClientAdapter> Create(
      WebAssociatedURLLoaderImpl*,
      WebAssociatedURLLoaderClient*,
      const WebAssociatedURLLoaderOptions&,
      network::mojom::FetchRequestMode,
      network::mojom::FetchCredentialsMode,
      scoped_refptr<base::SingleThreadTaskRunner>);

  // ThreadableLoaderClient
  void DidSendData(unsigned long long /*bytesSent*/,
                   unsigned long long /*totalBytesToBeSent*/) override;
  void DidReceiveResponse(unsigned long,
                          const ResourceResponse&,
                          std::unique_ptr<WebDataConsumerHandle>) override;
  void DidDownloadData(int /*dataLength*/) override;
  void DidReceiveData(const char*, unsigned /*dataLength*/) override;
  void DidReceiveCachedMetadata(const char*, int /*dataLength*/) override;
  void DidFinishLoading(unsigned long /*identifier*/) override;
  void DidFail(const ResourceError&) override;
  void DidFailRedirectCheck() override;

  // ThreadableLoaderClient
  bool WillFollowRedirect(
      const KURL& /*new_url*/,
      const ResourceResponse& /*redirect_response*/) override;

  // Sets an error to be reported back to the client, asychronously.
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

 private:
  ClientAdapter(WebAssociatedURLLoaderImpl*,
                WebAssociatedURLLoaderClient*,
                const WebAssociatedURLLoaderOptions&,
                network::mojom::FetchRequestMode,
                network::mojom::FetchCredentialsMode,
                scoped_refptr<base::SingleThreadTaskRunner>);

  void NotifyError(TimerBase*);

  WebAssociatedURLLoaderImpl* loader_;
  WebAssociatedURLLoaderClient* client_;
  WebAssociatedURLLoaderOptions options_;
  network::mojom::FetchRequestMode fetch_request_mode_;
  network::mojom::FetchCredentialsMode credentials_mode_;
  base::Optional<WebURLError> error_;

  TaskRunnerTimer<ClientAdapter> error_timer_;
  bool enable_error_notifications_;
  bool did_fail_;

  DISALLOW_COPY_AND_ASSIGN(ClientAdapter);
};

std::unique_ptr<WebAssociatedURLLoaderImpl::ClientAdapter>
WebAssociatedURLLoaderImpl::ClientAdapter::Create(
    WebAssociatedURLLoaderImpl* loader,
    WebAssociatedURLLoaderClient* client,
    const WebAssociatedURLLoaderOptions& options,
    network::mojom::FetchRequestMode fetch_request_mode,
    network::mojom::FetchCredentialsMode credentials_mode,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return base::WrapUnique(new ClientAdapter(loader, client, options,
                                            fetch_request_mode,
                                            credentials_mode, task_runner));
}

WebAssociatedURLLoaderImpl::ClientAdapter::ClientAdapter(
    WebAssociatedURLLoaderImpl* loader,
    WebAssociatedURLLoaderClient* client,
    const WebAssociatedURLLoaderOptions& options,
    network::mojom::FetchRequestMode fetch_request_mode,
    network::mojom::FetchCredentialsMode credentials_mode,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : loader_(loader),
      client_(client),
      options_(options),
      fetch_request_mode_(fetch_request_mode),
      credentials_mode_(credentials_mode),
      error_timer_(std::move(task_runner), this, &ClientAdapter::NotifyError),
      enable_error_notifications_(false),
      did_fail_(false) {
  DCHECK(loader_);
  DCHECK(client_);
}

bool WebAssociatedURLLoaderImpl::ClientAdapter::WillFollowRedirect(
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
    unsigned long long bytes_sent,
    unsigned long long total_bytes_to_be_sent) {
  if (!client_)
    return;

  client_->DidSendData(bytes_sent, total_bytes_to_be_sent);
}

void WebAssociatedURLLoaderImpl::ClientAdapter::DidReceiveResponse(
    unsigned long,
    const ResourceResponse& response,
    std::unique_ptr<WebDataConsumerHandle> handle) {
  ALLOW_UNUSED_LOCAL(handle);
  DCHECK(!handle);
  if (!client_)
    return;

  if (options_.expose_all_response_headers ||
      (fetch_request_mode_ != network::mojom::FetchRequestMode::kCORS &&
       fetch_request_mode_ !=
           network::mojom::FetchRequestMode::kCORSWithForcedPreflight)) {
    // Use the original ResourceResponse.
    client_->DidReceiveResponse(WrappedResourceResponse(response));
    return;
  }

  WebHTTPHeaderSet exposed_headers = WebCORS::ExtractCorsExposedHeaderNamesList(
      credentials_mode_, WrappedResourceResponse(response));
  WebHTTPHeaderSet blocked_headers;
  for (const auto& header : response.HttpHeaderFields()) {
    if (FetchUtils::IsForbiddenResponseHeaderName(header.key) ||
        (!WebCORS::IsOnAccessControlResponseHeaderWhitelist(header.key) &&
         exposed_headers.find(header.key.Ascii().data()) ==
             exposed_headers.end()))
      blocked_headers.insert(header.key.Ascii().data());
  }

  if (blocked_headers.empty()) {
    // Use the original ResourceResponse.
    client_->DidReceiveResponse(WrappedResourceResponse(response));
    return;
  }

  // If there are blocked headers, copy the response so we can remove them.
  WebURLResponse validated_response = WrappedResourceResponse(response);
  for (const auto& header : blocked_headers)
    validated_response.ClearHTTPHeaderField(WebString::FromASCII(header));
  client_->DidReceiveResponse(validated_response);
}

void WebAssociatedURLLoaderImpl::ClientAdapter::DidDownloadData(
    int data_length) {
  if (!client_)
    return;

  client_->DidDownloadData(data_length);
}

void WebAssociatedURLLoaderImpl::ClientAdapter::DidReceiveData(
    const char* data,
    unsigned data_length) {
  if (!client_)
    return;

  CHECK_LE(data_length, static_cast<unsigned>(std::numeric_limits<int>::max()));

  client_->DidReceiveData(data, data_length);
}

void WebAssociatedURLLoaderImpl::ClientAdapter::DidReceiveCachedMetadata(
    const char* data,
    int data_length) {
  if (!client_)
    return;

  client_->DidReceiveCachedMetadata(data, data_length);
}

void WebAssociatedURLLoaderImpl::ClientAdapter::DidFinishLoading(
    unsigned long identifier) {
  if (!client_)
    return;

  loader_->ClientAdapterDone();

  ReleaseClient()->DidFinishLoading();
  // |this| may be dead here.
}

void WebAssociatedURLLoaderImpl::ClientAdapter::DidFail(
    const ResourceError& error) {
  if (!client_)
    return;

  loader_->ClientAdapterDone();

  did_fail_ = true;
  error_ = static_cast<WebURLError>(error);
  if (enable_error_notifications_)
    NotifyError(&error_timer_);
}

void WebAssociatedURLLoaderImpl::ClientAdapter::DidFailRedirectCheck() {
  DidFail(ResourceError::Failure(NullURL()));
}

void WebAssociatedURLLoaderImpl::ClientAdapter::EnableErrorNotifications() {
  enable_error_notifications_ = true;
  // If an error has already been received, start a timer to report it to the
  // client after WebAssociatedURLLoader::loadAsynchronously has returned to the
  // caller.
  if (did_fail_)
    error_timer_.StartOneShot(TimeDelta(), FROM_HERE);
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
      public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(Observer);

 public:
  Observer(WebAssociatedURLLoaderImpl* parent, Document* document)
      : ContextLifecycleObserver(document), parent_(parent) {}

  void Dispose() {
    parent_ = nullptr;
    ClearContext();
  }

  void ContextDestroyed(ExecutionContext*) override {
    if (parent_)
      parent_->DocumentDestroyed();
  }

  void Trace(blink::Visitor* visitor) override {
    ContextLifecycleObserver::Trace(visitor);
  }

  WebAssociatedURLLoaderImpl* parent_;
};

WebAssociatedURLLoaderImpl::WebAssociatedURLLoaderImpl(
    Document* document,
    const WebAssociatedURLLoaderOptions& options)
    : client_(nullptr),
      options_(options),
      observer_(new Observer(this, document)) {}

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

  bool allow_load = true;
  WebURLRequest new_request(request);
  if (options_.untrusted_http) {
    WebString method = new_request.HttpMethod();
    allow_load = observer_ && IsValidHTTPToken(method) &&
                 !FetchUtils::IsForbiddenMethod(method);
    if (allow_load) {
      new_request.SetHTTPMethod(FetchUtils::NormalizeMethod(method));
      HTTPRequestHeaderValidator validator;
      new_request.VisitHTTPHeaderFields(&validator);
      allow_load = validator.IsSafe();
    }
  }
  new_request.ToMutableResourceRequest().SetCORSPreflightPolicy(
      options_.preflight_policy);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner;
  // |observer_| can be null if Cancel, DocumentDestroyed or
  // ClientAdapterDone gets called between creating the loader and
  // calling LoadAsynchronously.
  if (observer_) {
    task_runner = To<Document>(observer_->LifecycleContext())
                      ->GetTaskRunner(TaskType::kInternalLoading);
  } else {
    task_runner = Platform::Current()->CurrentThread()->GetTaskRunner();
  }
  client_ = client;
  client_adapter_ = ClientAdapter::Create(
      this, client, options_, request.GetFetchRequestMode(),
      request.GetFetchCredentialsMode(), std::move(task_runner));

  if (allow_load) {
    ResourceLoaderOptions resource_loader_options;
    resource_loader_options.data_buffering_policy = kDoNotBufferData;

    if (options_.grant_universal_access) {
      const auto mode = new_request.GetFetchRequestMode();
      DCHECK(mode == network::mojom::FetchRequestMode::kNoCORS ||
             mode == network::mojom::FetchRequestMode::kNavigate);
      scoped_refptr<SecurityOrigin> origin =
          SecurityOrigin::CreateUniqueOpaque();
      origin->GrantUniversalAccess();
      new_request.ToMutableResourceRequest().SetRequestorOrigin(origin);
    }

    const ResourceRequest& webcore_request = new_request.ToResourceRequest();
    mojom::RequestContextType context = webcore_request.GetRequestContext();
    if (context == mojom::RequestContextType::UNSPECIFIED) {
      // TODO(yoav): We load URLs without setting a TargetType (and therefore a
      // request context) in several places in content/
      // (P2PPortAllocatorSession::AllocateLegacyRelaySession, for example).
      // Remove this once those places are patched up.
      new_request.SetRequestContext(mojom::RequestContextType::INTERNAL);
    } else if (context == mojom::RequestContextType::VIDEO) {
      resource_loader_options.initiator_info.name =
          FetchInitiatorTypeNames::video;
    } else if (context == mojom::RequestContextType::AUDIO) {
      resource_loader_options.initiator_info.name =
          FetchInitiatorTypeNames::audio;
    }

    if (observer_) {
      Document& document = To<Document>(*observer_->LifecycleContext());
      loader_ = new ThreadableLoader(document, client_adapter_.get(),
                                     resource_loader_options);
      loader_->Start(webcore_request);
    }
  }

  if (!loader_) {
    client_adapter_->DidFail(ResourceError::CancelledDueToAccessCheckError(
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
  client_adapter_.reset();
}

void WebAssociatedURLLoaderImpl::SetDefersLoading(bool defers_loading) {
  if (loader_)
    loader_->SetDefersLoading(defers_loading);
}

void WebAssociatedURLLoaderImpl::SetLoadingTaskRunner(
    base::SingleThreadTaskRunner*) {
  // TODO(alexclarke): Maybe support this one day if it proves worthwhile.
}

void WebAssociatedURLLoaderImpl::DocumentDestroyed() {
  DisposeObserver();
  CancelLoader();

  if (!client_)
    return;

  ReleaseClient()->DidFail(ResourceError::CancelledError(KURL()));
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
  // there could be a WebURLLoader instance behind the
  // ThreadableLoader instance. So, for safety, we chose to just
  // crash here.
  CHECK(ThreadState::Current());

  observer_->Dispose();
  observer_ = nullptr;
}

}  // namespace blink
