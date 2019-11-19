// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/fetch_respond_with_observer.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/request_context_frame_type.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_response.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/body_stream_buffer.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope.h"
#include "third_party/blink/renderer/modules/service_worker/wait_until_observer.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/loader/fetch/bytes_consumer.h"
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
    case ServiceWorkerResponseError::kResponseTypeCorsForRequestModeSameOrigin:
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

// Notifies the result of FetchDataLoader to |callback_|, the other endpoint
// for which is passed to the browser process via
// blink.mojom.ServiceWorkerFetchResponseCallback.OnResponseStream().
class FetchLoaderClient final : public GarbageCollected<FetchLoaderClient>,
                                public FetchDataLoader::Client {
  USING_GARBAGE_COLLECTED_MIXIN(FetchLoaderClient);

 public:
  FetchLoaderClient(
      std::unique_ptr<ServiceWorkerTimeoutTimer::StayAwakeToken> token)
      : token_(std::move(token)) {
    // We need to make |callback_| callable in the first place because some
    // DidFetchDataLoadXXX() accessing it may be called synchronously from
    // StartLoading().
    callback_receiver_ = callback_.BindNewPipeAndPassReceiver();
  }

  void DidFetchDataStartedDataPipe(
      mojo::ScopedDataPipeConsumerHandle pipe) override {
    DCHECK(!body_stream_.is_valid());
    DCHECK(pipe.is_valid());
    body_stream_ = std::move(pipe);
  }
  void DidFetchDataLoadedDataPipe() override {
    callback_->OnCompleted();
    token_.reset();
  }
  void DidFetchDataLoadFailed() override {
    callback_->OnAborted();
    token_.reset();
  }
  void Abort() override {
    // A fetch() aborted via AbortSignal in the ServiceWorker will just look
    // like an ordinary failure to the page.
    // TODO(ricea): Should a fetch() on the page get an AbortError instead?
    callback_->OnAborted();
    token_.reset();
  }

  mojom::blink::ServiceWorkerStreamHandlePtr CreateStreamHandle() {
    if (!body_stream_.is_valid())
      return nullptr;
    return mojom::blink::ServiceWorkerStreamHandle::New(
        std::move(body_stream_), std::move(callback_receiver_));
  }

  void Trace(blink::Visitor* visitor) override {
    FetchDataLoader::Client::Trace(visitor);
  }

 private:
  mojo::ScopedDataPipeConsumerHandle body_stream_;
  mojo::PendingReceiver<mojom::blink::ServiceWorkerStreamCallback>
      callback_receiver_;

  mojo::Remote<mojom::blink::ServiceWorkerStreamCallback> callback_;
  std::unique_ptr<ServiceWorkerTimeoutTimer::StayAwakeToken> token_;

  DISALLOW_COPY_AND_ASSIGN(FetchLoaderClient);
};

}  // namespace

FetchRespondWithObserver* FetchRespondWithObserver::Create(
    ExecutionContext* context,
    int fetch_event_id,
    const KURL& request_url,
    network::mojom::RequestMode request_mode,
    network::mojom::RedirectMode redirect_mode,
    network::mojom::RequestContextFrameType frame_type,
    mojom::RequestContextType request_context,
    WaitUntilObserver* observer) {
  return MakeGarbageCollected<FetchRespondWithObserver>(
      context, fetch_event_id, request_url, request_mode, redirect_mode,
      frame_type, request_context, observer);
}

// This function may be called when an exception is scheduled. Thus, it must
// never invoke any code that might throw. In particular, it must never invoke
// JavaScript.
void FetchRespondWithObserver::OnResponseRejected(
    ServiceWorkerResponseError error) {
  DCHECK(GetExecutionContext());
  GetExecutionContext()->AddConsoleMessage(
      ConsoleMessage::Create(mojom::ConsoleMessageSource::kJavaScript,
                             mojom::ConsoleMessageLevel::kWarning,
                             GetMessageForResponseError(error, request_url_)));

  // The default value of FetchAPIResponse's status is 0, which maps to a
  // network error.
  auto response = mojom::blink::FetchAPIResponse::New();
  response->status_text = "";
  response->error = error;
  To<ServiceWorkerGlobalScope>(GetExecutionContext())
      ->RespondToFetchEvent(event_id_, request_url_, std::move(response),
                            event_dispatch_time_, base::TimeTicks::Now());
}

void FetchRespondWithObserver::OnResponseFulfilled(
    ScriptState* script_state,
    const ScriptValue& value,
    ExceptionState::ContextType context_type,
    const char* interface_name,
    const char* property_name) {
  DCHECK(GetExecutionContext());
  if (!V8Response::HasInstance(value.V8Value(), script_state->GetIsolate())) {
    OnResponseRejected(ServiceWorkerResponseError::kNoV8Instance);
    return;
  }
  Response* response = V8Response::ToImplWithTypeCheck(
      script_state->GetIsolate(), value.V8Value());
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
  if (response_type == network::mojom::FetchResponseType::kCors &&
      request_mode_ == network::mojom::RequestMode::kSameOrigin) {
    OnResponseRejected(
        ServiceWorkerResponseError::kResponseTypeCorsForRequestModeSameOrigin);
    return;
  }
  if (response_type == network::mojom::FetchResponseType::kOpaque) {
    if (request_mode_ != network::mojom::RequestMode::kNoCors) {
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
  if (redirect_mode_ != network::mojom::RedirectMode::kManual &&
      response_type == network::mojom::FetchResponseType::kOpaqueRedirect) {
    OnResponseRejected(ServiceWorkerResponseError::kResponseTypeOpaqueRedirect);
    return;
  }
  if (redirect_mode_ != network::mojom::RedirectMode::kFollow &&
      response->redirected()) {
    OnResponseRejected(
        ServiceWorkerResponseError::kRedirectedResponseForNotFollowRequest);
    return;
  }

  ExceptionState exception_state(script_state->GetIsolate(), context_type,
                                 interface_name, property_name);
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

  mojom::blink::FetchAPIResponsePtr fetch_api_response =
      response->PopulateFetchAPIResponse(request_url_);
  ServiceWorkerGlobalScope* service_worker_global_scope =
      To<ServiceWorkerGlobalScope>(GetExecutionContext());

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
      fetch_api_response->blob = blob_data_handle;
      service_worker_global_scope->RespondToFetchEvent(
          event_id_, request_url_, std::move(fetch_api_response),
          event_dispatch_time_, base::TimeTicks::Now());
      return;
    }

    // Load the Response as a Mojo DataPipe. The resulting pipe consumer
    // handle will be passed to the FetchLoaderClient on start.
    FetchLoaderClient* fetch_loader_client =
        MakeGarbageCollected<FetchLoaderClient>(
            service_worker_global_scope->CreateStayAwakeToken());
    buffer->StartLoading(FetchDataLoader::CreateLoaderAsDataPipe(task_runner_),
                         fetch_loader_client, exception_state);
    if (exception_state.HadException()) {
      OnResponseRejected(ServiceWorkerResponseError::kResponseBodyBroken);
      return;
    }

    mojom::blink::ServiceWorkerStreamHandlePtr stream_handle =
        fetch_loader_client->CreateStreamHandle();
    // We failed to allocate the Mojo DataPipe.
    if (!stream_handle) {
      OnResponseRejected(ServiceWorkerResponseError::kDataPipeCreationFailed);
      return;
    }

    service_worker_global_scope->RespondToFetchEventWithResponseStream(
        event_id_, request_url_, std::move(fetch_api_response),
        std::move(stream_handle), event_dispatch_time_, base::TimeTicks::Now());
    return;
  }
  service_worker_global_scope->RespondToFetchEvent(
      event_id_, request_url_, std::move(fetch_api_response),
      event_dispatch_time_, base::TimeTicks::Now());
}

void FetchRespondWithObserver::OnNoResponse() {
  DCHECK(GetExecutionContext());
  To<ServiceWorkerGlobalScope>(GetExecutionContext())
      ->RespondToFetchEventWithNoResponse(event_id_, request_url_,
                                          event_dispatch_time_,
                                          base::TimeTicks::Now());
}

FetchRespondWithObserver::FetchRespondWithObserver(
    ExecutionContext* context,
    int fetch_event_id,
    const KURL& request_url,
    network::mojom::RequestMode request_mode,
    network::mojom::RedirectMode redirect_mode,
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
