// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "third_party/blink/renderer/modules/service_worker/fetch_event.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/mojom/timing/performance_mark_or_measure.mojom-blink.h"
#include "third_party/blink/public/mojom/timing/worker_timing_container.mojom-blink.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_error.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/request.h"
#include "third_party/blink/renderer/core/fetch/response.h"
#include "third_party/blink/renderer/core/timing/performance_mark.h"
#include "third_party/blink/renderer/core/timing/performance_measure.h"
#include "third_party/blink/renderer/core/timing/worker_global_scope_performance.h"
#include "third_party/blink/renderer/modules/service_worker/fetch_respond_with_observer.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_error.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_timing.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_timing_info.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/network_utils.h"

namespace blink {

FetchEvent* FetchEvent::Create(ScriptState* script_state,
                               const AtomicString& type,
                               const FetchEventInit* initializer) {
  return MakeGarbageCollected<FetchEvent>(script_state, type, initializer,
                                          nullptr, nullptr, mojo::NullRemote(),
                                          false);
}

Request* FetchEvent::request() const {
  return request_;
}

String FetchEvent::clientId() const {
  return client_id_;
}

String FetchEvent::resultingClientId() const {
  return resulting_client_id_;
}

bool FetchEvent::isReload() const {
  UseCounter::Count(GetExecutionContext(), WebFeature::kFetchEventIsReload);
  return is_reload_;
}

void FetchEvent::respondWith(ScriptState* script_state,
                             ScriptPromise script_promise,
                             ExceptionState& exception_state) {
  stopImmediatePropagation();
  if (observer_)
    observer_->RespondWith(script_state, script_promise, exception_state);
}

ScriptPromise FetchEvent::preloadResponse(ScriptState* script_state) {
  return preload_response_property_->Promise(script_state->World());
}

ScriptPromise FetchEvent::handled(ScriptState* script_state) {
  return handled_property_->Promise(script_state->World());
}

void FetchEvent::ResolveHandledPromise() {
  handled_property_->ResolveWithUndefined();
}

void FetchEvent::RejectHandledPromise(const String& error_message) {
  handled_property_->Reject(ServiceWorkerError::GetException(
      nullptr, mojom::blink::ServiceWorkerErrorType::kNetwork, error_message));
}

const AtomicString& FetchEvent::InterfaceName() const {
  return event_interface_names::kFetchEvent;
}

bool FetchEvent::HasPendingActivity() const {
  // Prevent V8 from garbage collecting the wrapper object while waiting for the
  // preload response. This is in order to keep the resolver of preloadResponse
  // Promise alive. Note that |preload_response_property_| can be nullptr as
  // GC can run while running the FetchEvent constructor, before the member is
  // set. If it isn't set we treat it as a pending state.
  return !preload_response_property_ ||
         preload_response_property_->GetState() ==
             PreloadResponseProperty::kPending;
}

FetchEvent::FetchEvent(ScriptState* script_state,
                       const AtomicString& type,
                       const FetchEventInit* initializer,
                       FetchRespondWithObserver* respond_with_observer,
                       WaitUntilObserver* wait_until_observer,
                       mojo::PendingRemote<mojom::blink::WorkerTimingContainer>
                           worker_timing_remote,
                       bool navigation_preload_sent)
    : ExtendableEvent(type, initializer, wait_until_observer),
      ExecutionContextClient(ExecutionContext::From(script_state)),
      observer_(respond_with_observer),
      preload_response_property_(MakeGarbageCollected<PreloadResponseProperty>(
          ExecutionContext::From(script_state))),
      handled_property_(
          MakeGarbageCollected<ScriptPromiseProperty<ToV8UndefinedGenerator,
                                                     Member<DOMException>>>(
              ExecutionContext::From(script_state))),
      worker_timing_remote_(ExecutionContext::From(script_state)) {
  worker_timing_remote_.Bind(std::move(worker_timing_remote),
                             ExecutionContext::From(script_state)
                                 ->GetTaskRunner(TaskType::kNetworking));
  if (!navigation_preload_sent)
    preload_response_property_->ResolveWithUndefined();

  client_id_ = initializer->clientId();
  resulting_client_id_ = initializer->resultingClientId();
  is_reload_ = initializer->isReload();
  request_ = initializer->request();
}

FetchEvent::~FetchEvent() = default;

void FetchEvent::OnNavigationPreloadResponse(
    ScriptState* script_state,
    std::unique_ptr<WebURLResponse> response,
    mojo::ScopedDataPipeConsumerHandle data_pipe) {
  if (!script_state->ContextIsValid())
    return;
  DCHECK(preload_response_property_);
  DCHECK(!preload_response_);
  ScriptState::Scope scope(script_state);
  preload_response_ = std::move(response);
  DataPipeBytesConsumer* bytes_consumer = nullptr;
  if (data_pipe.is_valid()) {
    DataPipeBytesConsumer::CompletionNotifier* completion_notifier = nullptr;
    bytes_consumer = MakeGarbageCollected<DataPipeBytesConsumer>(
        ExecutionContext::From(script_state)
            ->GetTaskRunner(TaskType::kNetworking),
        std::move(data_pipe), &completion_notifier);
    body_completion_notifier_ = completion_notifier;
  }
  // TODO(ricea): Verify that this response can't be aborted from JS.
  FetchResponseData* response_data =
      bytes_consumer
          ? FetchResponseData::CreateWithBuffer(BodyStreamBuffer::Create(
                script_state, bytes_consumer,
                MakeGarbageCollected<AbortSignal>(
                    ExecutionContext::From(script_state)),
                /*cached_metadata_handler=*/nullptr))
          : FetchResponseData::Create();
  Vector<KURL> url_list(1);
  url_list[0] = preload_response_->CurrentRequestUrl();

  response_data->InitFromResourceResponse(
      url_list, http_names::kGET, network::mojom::CredentialsMode::kInclude,
      FetchRequestData::kBasicTainting,
      preload_response_->ToResourceResponse());

  FetchResponseData* tainted_response =
      network_utils::IsRedirectResponseCode(preload_response_->HttpStatusCode())
          ? response_data->CreateOpaqueRedirectFilteredResponse()
          : response_data->CreateBasicFilteredResponse();
  preload_response_property_->Resolve(
      Response::Create(ExecutionContext::From(script_state), tainted_response));
}

void FetchEvent::OnNavigationPreloadError(
    ScriptState* script_state,
    std::unique_ptr<WebServiceWorkerError> error) {
  if (!script_state->ContextIsValid())
    return;
  if (body_completion_notifier_) {
    body_completion_notifier_->SignalError(BytesConsumer::Error());
    body_completion_notifier_ = nullptr;
  }
  DCHECK(preload_response_property_);
  if (preload_response_property_->GetState() !=
      PreloadResponseProperty::kPending) {
    return;
  }
  preload_response_property_->Reject(
      ServiceWorkerError::Take(nullptr, *error.get()));
}

void FetchEvent::OnNavigationPreloadComplete(
    WorkerGlobalScope* worker_global_scope,
    base::TimeTicks completion_time,
    int64_t encoded_data_length,
    int64_t encoded_body_length,
    int64_t decoded_body_length) {
  DCHECK(preload_response_);
  if (body_completion_notifier_) {
    body_completion_notifier_->SignalComplete();
    body_completion_notifier_ = nullptr;
  }
  std::unique_ptr<WebURLResponse> response = std::move(preload_response_);
  ResourceResponse resource_response = response->ToResourceResponse();
  resource_response.SetEncodedDataLength(encoded_data_length);
  resource_response.SetEncodedBodyLength(encoded_body_length);
  resource_response.SetDecodedBodyLength(decoded_body_length);

  ResourceLoadTiming* timing = resource_response.GetResourceLoadTiming();
  // |timing| can be null, see https://crbug.com/817691.
  base::TimeTicks request_time =
      timing ? timing->RequestTime() : base::TimeTicks();
  // According to the Resource Timing spec, the initiator type of
  // navigation preload request is "navigation".
  scoped_refptr<ResourceTimingInfo> info = ResourceTimingInfo::Create(
      "navigation", request_time, request_->GetRequestContextType(),
      request_->GetRequestDestination());
  info->SetNegativeAllowed(true);
  info->SetLoadResponseEnd(completion_time);
  info->SetInitialURL(request_->url());
  info->SetFinalResponse(resource_response);
  info->AddFinalTransferSize(encoded_data_length == -1 ? 0
                                                       : encoded_data_length);
  WorkerGlobalScopePerformance::performance(*worker_global_scope)
      ->GenerateAndAddResourceTiming(*info);
}

void FetchEvent::addPerformanceEntry(PerformanceMark* performance_mark) {
  if (worker_timing_remote_.is_bound()) {
    auto mojo_performance_mark =
        performance_mark->ToMojoPerformanceMarkOrMeasure();
    worker_timing_remote_->AddPerformanceEntry(
        std::move(mojo_performance_mark));
  }
}

void FetchEvent::addPerformanceEntry(PerformanceMeasure* performance_measure) {
  if (worker_timing_remote_.is_bound()) {
    auto mojo_performance_measure =
        performance_measure->ToMojoPerformanceMarkOrMeasure();
    worker_timing_remote_->AddPerformanceEntry(
        std::move(mojo_performance_measure));
  }
}

void FetchEvent::Trace(Visitor* visitor) const {
  visitor->Trace(observer_);
  visitor->Trace(request_);
  visitor->Trace(preload_response_property_);
  visitor->Trace(body_completion_notifier_);
  visitor->Trace(handled_property_);
  visitor->Trace(worker_timing_remote_);
  ExtendableEvent::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
