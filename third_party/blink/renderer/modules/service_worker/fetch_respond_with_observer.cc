// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/fetch_respond_with_observer.h"

#include <memory>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_response.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/request_context_frame_type.mojom-blink.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_stream_handle.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_response.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/body_stream_buffer.h"
#include "third_party/blink/renderer/core/fetch/response.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/modules/service_worker/cross_origin_resource_policy_checker.h"
#include "third_party/blink/renderer/modules/service_worker/fetch_event.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope.h"
#include "third_party/blink/renderer/modules/service_worker/wait_until_observer.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/bytes_consumer.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/wtf/gc_plugin.h"
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
      NOTREACHED_IN_MIGRATION();
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
    case ServiceWorkerResponseError::kDisallowedByCorp:
      error_message = error_message +
                      "Cross-Origin-Resource-Policy prevented from serving the "
                      "response to the client.";
      break;
    case ServiceWorkerResponseError::kUnknown:
    default:
      error_message = error_message + "an unexpected error occurred.";
      break;
  }
  return error_message;
}

bool IsNavigationRequest(mojom::RequestContextFrameType frame_type) {
  return frame_type != mojom::RequestContextFrameType::kNone;
}

bool IsClientRequest(mojom::RequestContextFrameType frame_type,
                     network::mojom::RequestDestination destination) {
  return IsNavigationRequest(frame_type) ||
         destination == network::mojom::RequestDestination::kSharedWorker ||
         destination == network::mojom::RequestDestination::kWorker;
}

// Notifies the result of FetchDataLoader to |callback_|, the other endpoint
// for which is passed to the browser process via
// blink.mojom.ServiceWorkerFetchResponseCallback.OnResponseStream().
class FetchLoaderClient final : public GarbageCollected<FetchLoaderClient>,
                                public FetchDataLoader::Client {
 public:
  FetchLoaderClient(
      std::unique_ptr<ServiceWorkerEventQueue::StayAwakeToken> token,
      ServiceWorkerGlobalScope* service_worker,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : callback_(service_worker), token_(std::move(token)) {
    // We need to make |callback_| callable in the first place because some
    // DidFetchDataLoadXXX() accessing it may be called synchronously from
    // StartLoading().
    callback_receiver_ =
        callback_.BindNewPipeAndPassReceiver(std::move(task_runner));
  }

  FetchLoaderClient(const FetchLoaderClient&) = delete;
  FetchLoaderClient& operator=(const FetchLoaderClient&) = delete;

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

  void Trace(Visitor* visitor) const override {
    visitor->Trace(callback_);
    FetchDataLoader::Client::Trace(visitor);
  }

 private:
  mojo::ScopedDataPipeConsumerHandle body_stream_;
  mojo::PendingReceiver<mojom::blink::ServiceWorkerStreamCallback>
      callback_receiver_;

  HeapMojoRemote<mojom::blink::ServiceWorkerStreamCallback> callback_;
  std::unique_ptr<ServiceWorkerEventQueue::StayAwakeToken> token_;
};

class UploadingCompletionObserver
    : public GarbageCollected<UploadingCompletionObserver>,
      public BytesUploader::Client {
 public:
  explicit UploadingCompletionObserver(
      int fetch_event_id,
      ScriptPromiseResolver<IDLUndefined>* resolver,
      ServiceWorkerGlobalScope* service_worker_global_scope)
      : fetch_event_id_(fetch_event_id),
        resolver_(resolver),
        service_worker_global_scope_(service_worker_global_scope) {}

  ~UploadingCompletionObserver() override = default;

  void OnComplete() override {
    resolver_->Resolve();
    service_worker_global_scope_->OnStreamingUploadCompletion(fetch_event_id_);
  }

  void OnError() override {
    resolver_->Reject();
    service_worker_global_scope_->OnStreamingUploadCompletion(fetch_event_id_);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(resolver_);
    visitor->Trace(service_worker_global_scope_);
    BytesUploader::Client::Trace(visitor);
  }

 private:
  const int fetch_event_id_;
  const Member<ScriptPromiseResolver<IDLUndefined>> resolver_;
  Member<ServiceWorkerGlobalScope> service_worker_global_scope_;
};

}  // namespace

// This function may be called when an exception is scheduled. Thus, it must
// never invoke any code that might throw. In particular, it must never invoke
// JavaScript.
void FetchRespondWithObserver::OnResponseRejected(
    ServiceWorkerResponseError error) {
  DCHECK(GetExecutionContext());
  const String error_message = GetMessageForResponseError(error, request_url_);
  GetExecutionContext()->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::ConsoleMessageSource::kJavaScript,
      mojom::ConsoleMessageLevel::kWarning, error_message));

  // The default value of FetchAPIResponse's status is 0, which maps to a
  // network error.
  auto response = mojom::blink::FetchAPIResponse::New();
  response->status_text = "";
  response->error = error;
  ServiceWorkerGlobalScope* service_worker_global_scope =
      To<ServiceWorkerGlobalScope>(GetExecutionContext());
  service_worker_global_scope->RespondToFetchEvent(
      event_id_, request_url_, range_request_, std::move(response),
      event_dispatch_time_, base::TimeTicks::Now());
  event_->RejectHandledPromise(error_message);
}

void FetchRespondWithObserver::OnResponseFulfilled(ScriptState* script_state,
                                                   const ScriptValue& value) {
  DCHECK(GetExecutionContext());
  Response* response =
      V8Response::ToWrappable(script_state->GetIsolate(), value.V8Value());
  if (!response) {
    OnResponseRejected(ServiceWorkerResponseError::kNoV8Instance);
    return;
  }
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
    if (IsClientRequest(frame_type_, request_destination_)) {
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

  if (response->IsBodyLocked()) {
    OnResponseRejected(ServiceWorkerResponseError::kBodyLocked);
    return;
  }

  if (response->IsBodyUsed()) {
    OnResponseRejected(ServiceWorkerResponseError::kBodyUsed);
    return;
  }

  mojom::blink::FetchAPIResponsePtr fetch_api_response =
      response->PopulateFetchAPIResponse(request_url_);
  ServiceWorkerGlobalScope* service_worker_global_scope =
      To<ServiceWorkerGlobalScope>(GetExecutionContext());

  // If Cross-Origin-Embedder-Policy is set to require-corp,
  // Cross-Origin-Resource-Policy verification should happen before passing the
  // response to the client. The service worker script must be in the same
  // origin with the requestor, which is a client of the service worker.
  //
  // Here is in the renderer and we don't have a "trustworthy" initiator.
  // Hence we provide |initiator_origin| as |request_initiator_origin_lock|.
  auto initiator_origin =
      url::Origin::Create(GURL(service_worker_global_scope->Url()));
  // |corp_checker_| could be nullptr when the request is for a main resource
  // or the connection to the client which initiated the request is broken.
  // CORP check isn't needed in both cases because a service worker should be
  // in the same origin with the main resource, and the response to the broken
  // connection won't reach to the client.
  if (corp_checker_ &&
      corp_checker_->IsBlocked(
          url::Origin::Create(GURL(service_worker_global_scope->Url())),
          request_mode_, request_destination_, *response)) {
    OnResponseRejected(ServiceWorkerResponseError::kDisallowedByCorp);
    return;
  }

  BodyStreamBuffer* buffer = response->InternalBodyBuffer();
  if (buffer) {
    // The |side_data_blob| must be taken before the body buffer is
    // drained or loading begins.
    fetch_api_response->side_data_blob = buffer->TakeSideDataBlob();

    ExceptionState exception_state(script_state->GetIsolate());

    scoped_refptr<BlobDataHandle> blob_data_handle =
        buffer->DrainAsBlobDataHandle(
            BytesConsumer::BlobSizePolicy::kAllowBlobWithInvalidSize,
            exception_state);

    if (blob_data_handle) {
      // Handle the blob response body.
      fetch_api_response->blob = blob_data_handle;
      service_worker_global_scope->RespondToFetchEvent(
          event_id_, request_url_, range_request_,
          std::move(fetch_api_response), event_dispatch_time_,
          base::TimeTicks::Now());
      event_->ResolveHandledPromise();
      return;
    }

    // Load the Response as a Mojo DataPipe. The resulting pipe consumer
    // handle will be passed to the FetchLoaderClient on start.
    FetchLoaderClient* fetch_loader_client =
        MakeGarbageCollected<FetchLoaderClient>(
            service_worker_global_scope->CreateStayAwakeToken(),
            service_worker_global_scope, task_runner_);
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
        event_id_, request_url_, range_request_, std::move(fetch_api_response),
        std::move(stream_handle), event_dispatch_time_, base::TimeTicks::Now());
    event_->ResolveHandledPromise();
    return;
  }
  service_worker_global_scope->RespondToFetchEvent(
      event_id_, request_url_, range_request_, std::move(fetch_api_response),
      event_dispatch_time_, base::TimeTicks::Now());
  event_->ResolveHandledPromise();
}

void FetchRespondWithObserver::OnNoResponse(ScriptState* script_state) {
  DCHECK(GetExecutionContext());
  if (original_request_body_stream_ &&
      (original_request_body_stream_->IsLocked() ||
       original_request_body_stream_->IsDisturbed())) {
    GetExecutionContext()->CountUse(
        WebFeature::kFetchRespondWithNoResponseWithUsedRequestBody);
  }

  ServiceWorkerGlobalScope* service_worker_global_scope =
      To<ServiceWorkerGlobalScope>(GetExecutionContext());
  auto* body_buffer = event_->request()->BodyBuffer();
  std::optional<network::DataElementChunkedDataPipe> request_body_to_pass;
  if (body_buffer && !request_body_has_source_) {
    auto* body_stream = body_buffer->Stream();
    if (body_stream->IsLocked() || body_stream->IsDisturbed()) {
      OnResponseRejected(
          mojom::blink::ServiceWorkerResponseError::kRequestBodyUnusable);
      return;
    }

    // Keep the service worker alive as long as we are reading from the request
    // body.
    auto* resolver =
        MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
    WaitUntil(script_state, resolver->Promise(), ASSERT_NO_EXCEPTION);
    auto* observer = MakeGarbageCollected<UploadingCompletionObserver>(
        event_id_, resolver, service_worker_global_scope);
    mojo::PendingRemote<network::mojom::blink::ChunkedDataPipeGetter> remote;
    body_buffer->DrainAsChunkedDataPipeGetter(
        script_state, remote.InitWithNewPipeAndPassReceiver(), observer);
    request_body_to_pass.emplace(
        ToCrossVariantMojoType(std::move(remote)),
        network::DataElementChunkedDataPipe::ReadOnlyOnce(true));
  }

  service_worker_global_scope->RespondToFetchEventWithNoResponse(
      event_id_, event_.Get(), request_url_, range_request_,
      std::move(request_body_to_pass), event_dispatch_time_,
      base::TimeTicks::Now());
  event_->ResolveHandledPromise();
}

void FetchRespondWithObserver::SetEvent(FetchEvent* event) {
  DCHECK(!event_);
  DCHECK(!original_request_body_stream_);
  event_ = event;
  // We don't use Body::body() in order to avoid accidental CountUse calls.
  BodyStreamBuffer* body_buffer = event_->request()->BodyBuffer();
  if (body_buffer) {
    original_request_body_stream_ = body_buffer->Stream();
  }
}

FetchRespondWithObserver::FetchRespondWithObserver(
    ExecutionContext* context,
    int fetch_event_id,
    base::WeakPtr<CrossOriginResourcePolicyChecker> corp_checker,
    const mojom::blink::FetchAPIRequest& request,
    WaitUntilObserver* observer)
    : RespondWithObserver(context, fetch_event_id, observer),
      request_url_(request.url),
      request_mode_(request.mode),
      redirect_mode_(request.redirect_mode),
      frame_type_(request.frame_type),
      request_destination_(request.destination),
      request_body_has_source_(request.body.FormBody()),
      range_request_(request.headers.Contains(http_names::kRange)),
      corp_checker_(std::move(corp_checker)),
      task_runner_(context->GetTaskRunner(TaskType::kNetworking)) {}

void FetchRespondWithObserver::Trace(Visitor* visitor) const {
  visitor->Trace(event_);
  visitor->Trace(original_request_body_stream_);
  RespondWithObserver::Trace(visitor);
}

}  // namespace blink
