// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/fetch_respond_with_observer.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "services/network/public/mojom/request_context_frame_type.mojom-blink.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_response.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_response.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/body_stream_buffer.h"
#include "third_party/blink/renderer/core/fetch/bytes_consumer.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/console_types.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope_client.h"
#include "third_party/blink/renderer/modules/service_worker/wait_until_observer.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "v8/include/v8.h"

using blink::mojom::ServiceWorkerResponseError;

namespace blink {
namespace {

// Returns the error message to let the developer know about the reason of the
// unusual failures.
const String GetMessageForResponseError(ServiceWorkerResponseError error,
                                        const KURL& request_url) {
  String error_message = "The FetchEvent for \"" + request_url.GetString() +
                         "\" resulted in a network error response: ";
  switch (error) {
    case ServiceWorkerResponseError::kPromiseRejected:
      error_message = error_message + "the promise was rejected.";
      break;
    case ServiceWorkerResponseError::kDefaultPrevented:
      error_message =
          error_message +
          "preventDefault() was called without calling respondWith().";
      break;
    case ServiceWorkerResponseError::kNoV8Instance:
      error_message =
          error_message +
          "an object that was not a Response was passed to respondWith().";
      break;
    case ServiceWorkerResponseError::kResponseTypeError:
      error_message = error_message +
                      "the promise was resolved with an error response object.";
      break;
    case ServiceWorkerResponseError::kResponseTypeOpaque:
      error_message =
          error_message +
          "an \"opaque\" response was used for a request whose type "
          "is not no-cors";
      break;
    case ServiceWorkerResponseError::kResponseTypeNotBasicOrDefault:
      NOTREACHED();
      break;
    case ServiceWorkerResponseError::kBodyUsed:
      error_message =
          error_message +
          "a Response whose \"bodyUsed\" is \"true\" cannot be used "
          "to respond to a request.";
      break;
    case ServiceWorkerResponseError::kResponseTypeOpaqueForClientRequest:
      error_message = error_message +
                      "an \"opaque\" response was used for a client request.";
      break;
    case ServiceWorkerResponseError::kResponseTypeOpaqueRedirect:
      error_message = error_message +
                      "an \"opaqueredirect\" type response was used for a "
                      "request whose redirect mode is not \"manual\".";
      break;
    case ServiceWorkerResponseError::kResponseTypeCORSForRequestModeSameOrigin:
      error_message = error_message +
                      "a \"cors\" type response was used for a request whose "
                      "mode is \"same-origin\".";
      break;
    case ServiceWorkerResponseError::kBodyLocked:
      error_message = error_message +
                      "a Response whose \"body\" is locked cannot be used to "
                      "respond to a request.";
      break;
    case ServiceWorkerResponseError::kRedirectedResponseForNotFollowRequest:
      error_message = error_message +
                      "a redirected response was used for a request whose "
                      "redirect mode is not \"follow\".";
      break;
    case ServiceWorkerResponseError::kDataPipeCreationFailed:
      error_message = error_message + "insufficient resources.";
      break;
    case ServiceWorkerResponseError::kResponseBodyBroken:
      error_message =
          error_message + "a response body's status could not be checked.";
      break;
    case ServiceWorkerResponseError::kUnknown:
    default:
      error_message = error_message + "an unexpected error occurred.";
      break;
  }
  return error_message;
}

bool IsNavigationRequest(network::mojom::RequestContextFrameType frame_type) {
  return frame_type != network::mojom::RequestContextFrameType::kNone;
}

bool IsClientRequest(network::mojom::RequestContextFrameType frame_type,
                     mojom::RequestContextType request_context) {
  return IsNavigationRequest(frame_type) ||
         request_context == mojom::RequestContextType::SHARED_WORKER ||
         request_context == mojom::RequestContextType::WORKER;
}

// Notifies the result of FetchDataLoader to |handle_|. |handle_| pass through
// the result to its observer which is outside of blink.
class FetchLoaderClient final
    : public GarbageCollectedFinalized<FetchLoaderClient>,
      public FetchDataLoader::Client {
  WTF_MAKE_NONCOPYABLE(FetchLoaderClient);
  USING_GARBAGE_COLLECTED_MIXIN(FetchLoaderClient);

 public:
  FetchLoaderClient() {}

  void DidFetchDataStartedDataPipe(
      mojo::ScopedDataPipeConsumerHandle pipe) override {
    DCHECK(!handle_);
    handle_ = std::make_unique<WebServiceWorkerStreamHandle>(std::move(pipe));
  }
  void DidFetchDataLoadedDataPipe() override {
    DCHECK(handle_);
    // If this method is called synchronously from StartLoading() then we need
    // to delay notifying the handle until after
    // RespondToFetchEventWithResponseStream() is called.
    if (!started_) {
      pending_complete_ = true;
      return;
    }
    pending_complete_ = false;
    handle_->Completed();
  }
  void DidFetchDataLoadFailed() override {
    // If this method is called synchronously from StartLoading() then we need
    // to delay notifying the handle until after
    // RespondToFetchEventWithResponseStream() is called.
    if (!started_) {
      pending_failure_ = true;
      return;
    }
    pending_failure_ = false;
    if (handle_)
      handle_->Aborted();
  }
  void Abort() override {
    // A fetch() aborted via AbortSignal in the ServiceWorker will just look
    // like an ordinary failure to the page.
    // TODO(ricea): Should a fetch() on the page get an AbortError instead?
    if (handle_)
      handle_->Aborted();
  }

  void SetStarted() {
    DCHECK(!started_);
    // Note that RespondToFetchEventWithResponseStream() has been called and
    // flush any pending operation.
    started_ = true;
    if (pending_complete_)
      DidFetchDataLoadedDataPipe();
    else if (pending_failure_)
      DidFetchDataLoadFailed();
  }

  WebServiceWorkerStreamHandle* Handle() const { return handle_.get(); }

  void Trace(blink::Visitor* visitor) override {
    FetchDataLoader::Client::Trace(visitor);
  }

 private:
  std::unique_ptr<WebServiceWorkerStreamHandle> handle_;
  bool started_ = false;
  bool pending_complete_ = false;
  bool pending_failure_ = false;
};

}  // namespace

FetchRespondWithObserver* FetchRespondWithObserver::Create(
    ExecutionContext* context,
    int fetch_event_id,
    const KURL& request_url,
    network::mojom::FetchRequestMode request_mode,
    network::mojom::FetchRedirectMode redirect_mode,
    network::mojom::RequestContextFrameType frame_type,
    mojom::RequestContextType request_context,
    WaitUntilObserver* observer) {
  return new FetchRespondWithObserver(context, fetch_event_id, request_url,
                                      request_mode, redirect_mode, frame_type,
                                      request_context, observer);
}

// This function may be called when an exception is scheduled. Thus, it must
// never invoke any code that might throw. In particular, it must never invoke
// JavaScript.
void FetchRespondWithObserver::OnResponseRejected(
    ServiceWorkerResponseError error) {
  DCHECK(GetExecutionContext());
  GetExecutionContext()->AddConsoleMessage(
      ConsoleMessage::Create(kJSMessageSource, kWarningMessageLevel,
                             GetMessageForResponseError(error, request_url_)));

  // The default value of WebServiceWorkerResponse's status is 0, which maps
  // to a network error.
  WebServiceWorkerResponse web_response;
  web_response.SetError(error);
  ServiceWorkerGlobalScopeClient::From(GetExecutionContext())
      ->RespondToFetchEvent(event_id_, web_response, event_dispatch_time_,
                            base::TimeTicks::Now());
}

void FetchRespondWithObserver::OnResponseFulfilled(
    const ScriptValue& value,
    ExceptionState::ContextType context_type,
    const char* interface_name,
    const char* property_name) {
  DCHECK(GetExecutionContext());
  if (!V8Response::hasInstance(value.V8Value(),
                               ToIsolate(GetExecutionContext()))) {
    OnResponseRejected(ServiceWorkerResponseError::kNoV8Instance);
    return;
  }
  Response* response = V8Response::ToImplWithTypeCheck(
      ToIsolate(GetExecutionContext()), value.V8Value());
  // "If one of the following conditions is true, return a network error:
  //   - |response|'s type is |error|.
  //   - |request|'s mode is |same-origin| and |response|'s type is |cors|.
  //   - |request|'s mode is not |no-cors| and response's type is |opaque|.
  //   - |request| is a client request and |response|'s type is neither
  //     |basic| nor |default|."
  const network::mojom::FetchResponseType response_type =
      response->GetResponse()->GetType();
  if (response_type == network::mojom::FetchResponseType::kError) {
    OnResponseRejected(ServiceWorkerResponseError::kResponseTypeError);
    return;
  }
  if (response_type == network::mojom::FetchResponseType::kCORS &&
      request_mode_ == network::mojom::FetchRequestMode::kSameOrigin) {
    OnResponseRejected(
        ServiceWorkerResponseError::kResponseTypeCORSForRequestModeSameOrigin);
    return;
  }
  if (response_type == network::mojom::FetchResponseType::kOpaque) {
    if (request_mode_ != network::mojom::FetchRequestMode::kNoCORS) {
      OnResponseRejected(ServiceWorkerResponseError::kResponseTypeOpaque);
      return;
    }

    // The request mode of client requests should be "same-origin" but it is
    // not explicitly stated in the spec yet. So we need to check here.
    // FIXME: Set the request mode of client requests to "same-origin" and
    // remove this check when the spec will be updated.
    // Spec issue: https://github.com/whatwg/fetch/issues/101
    if (IsClientRequest(frame_type_, request_context_)) {
      OnResponseRejected(
          ServiceWorkerResponseError::kResponseTypeOpaqueForClientRequest);
      return;
    }
  }
  if (redirect_mode_ != network::mojom::FetchRedirectMode::kManual &&
      response_type == network::mojom::FetchResponseType::kOpaqueRedirect) {
    OnResponseRejected(ServiceWorkerResponseError::kResponseTypeOpaqueRedirect);
    return;
  }
  if (redirect_mode_ != network::mojom::FetchRedirectMode::kFollow &&
      response->redirected()) {
    OnResponseRejected(
        ServiceWorkerResponseError::kRedirectedResponseForNotFollowRequest);
    return;
  }

  ExceptionState exception_state(value.GetScriptState()->GetIsolate(),
                                 context_type, interface_name, property_name);
  if (response->IsBodyLocked(exception_state) == Body::BodyLocked::kLocked) {
    DCHECK(!exception_state.HadException());
    OnResponseRejected(ServiceWorkerResponseError::kBodyLocked);
    return;
  }

  if (exception_state.HadException()) {
    OnResponseRejected(ServiceWorkerResponseError::kResponseBodyBroken);
    return;
  }

  if (response->IsBodyUsed(exception_state) == Body::BodyUsed::kUsed) {
    DCHECK(!exception_state.HadException());
    OnResponseRejected(ServiceWorkerResponseError::kBodyUsed);
    return;
  }

  if (exception_state.HadException()) {
    OnResponseRejected(ServiceWorkerResponseError::kResponseBodyBroken);
    return;
  }

  WebServiceWorkerResponse web_response;
  response->PopulateWebServiceWorkerResponse(web_response);

  BodyStreamBuffer* buffer = response->InternalBodyBuffer();
  if (buffer) {
    scoped_refptr<BlobDataHandle> blob_data_handle =
        buffer->DrainAsBlobDataHandle(
            BytesConsumer::BlobSizePolicy::kAllowBlobWithInvalidSize,
            exception_state);
    if (exception_state.HadException()) {
      OnResponseRejected(ServiceWorkerResponseError::kResponseBodyBroken);
      return;
    }
    if (blob_data_handle) {
      // Handle the blob response body.
      web_response.SetBlobDataHandle(blob_data_handle);
      ServiceWorkerGlobalScopeClient::From(GetExecutionContext())
          ->RespondToFetchEvent(event_id_, web_response, event_dispatch_time_,
                                base::TimeTicks::Now());
      return;
    }

    // Load the Response as a mojo::DataPipe.  The resulting pipe consumer
    // handle will be passed to the FetchLoaderClient on start.
    FetchLoaderClient* fetch_loader_client = new FetchLoaderClient();
    buffer->StartLoading(FetchDataLoader::CreateLoaderAsDataPipe(task_runner_),
                         fetch_loader_client, exception_state);
    if (exception_state.HadException()) {
      OnResponseRejected(ServiceWorkerResponseError::kResponseBodyBroken);
      return;
    }

    // If we failed to create the WebServiceWorkerStreamHandle then we must
    // have failed to allocate the mojo::DataPipe.
    if (!fetch_loader_client->Handle()) {
      OnResponseRejected(ServiceWorkerResponseError::kDataPipeCreationFailed);
      return;
    }

    ServiceWorkerGlobalScopeClient::From(GetExecutionContext())
        ->RespondToFetchEventWithResponseStream(
            event_id_, web_response, fetch_loader_client->Handle(),
            event_dispatch_time_, base::TimeTicks::Now());

    fetch_loader_client->SetStarted();
    return;
  }
  ServiceWorkerGlobalScopeClient::From(GetExecutionContext())
      ->RespondToFetchEvent(event_id_, web_response, event_dispatch_time_,
                            base::TimeTicks::Now());
}

void FetchRespondWithObserver::OnNoResponse() {
  ServiceWorkerGlobalScopeClient::From(GetExecutionContext())
      ->RespondToFetchEventWithNoResponse(event_id_, event_dispatch_time_,
                                          base::TimeTicks::Now());
}

FetchRespondWithObserver::FetchRespondWithObserver(
    ExecutionContext* context,
    int fetch_event_id,
    const KURL& request_url,
    network::mojom::FetchRequestMode request_mode,
    network::mojom::FetchRedirectMode redirect_mode,
    network::mojom::RequestContextFrameType frame_type,
    mojom::RequestContextType request_context,
    WaitUntilObserver* observer)
    : RespondWithObserver(context, fetch_event_id, observer),
      request_url_(request_url),
      request_mode_(request_mode),
      redirect_mode_(redirect_mode),
      frame_type_(frame_type),
      request_context_(request_context),
      task_runner_(context->GetTaskRunner(TaskType::kNetworking)) {}

void FetchRespondWithObserver::Trace(blink::Visitor* visitor) {
  RespondWithObserver::Trace(visitor);
}

}  // namespace blink
