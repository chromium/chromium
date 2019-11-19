// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/background_fetch/background_fetch_registration.h"

#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/fetch/request.h"
#include "third_party/blink/renderer/core/fetch/response.h"
#include "third_party/blink/renderer/modules/background_fetch/background_fetch_bridge.h"
#include "third_party/blink/renderer/modules/background_fetch/background_fetch_record.h"
#include "third_party/blink/renderer/modules/cache_storage/cache.h"
#include "third_party/blink/renderer/modules/cache_storage/cache_query_options.h"
#include "third_party/blink/renderer/modules/event_target_modules_names.h"
#include "third_party/blink/renderer/modules/manifest/image_resource.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_registration.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"

namespace blink {

BackgroundFetchRegistration::BackgroundFetchRegistration(
    const String& developer_id,
    uint64_t upload_total,
    uint64_t uploaded,
    uint64_t download_total,
    uint64_t downloaded,
    mojom::BackgroundFetchResult result,
    mojom::BackgroundFetchFailureReason failure_reason)
    : developer_id_(developer_id),
      upload_total_(upload_total),
      uploaded_(uploaded),
      download_total_(download_total),
      downloaded_(downloaded),
      result_(result),
      failure_reason_(failure_reason) {}

BackgroundFetchRegistration::BackgroundFetchRegistration(
    ServiceWorkerRegistration* service_worker_registration,
    mojom::blink::BackgroundFetchRegistrationPtr registration)
    : developer_id_(registration->registration_data->developer_id),
      upload_total_(registration->registration_data->upload_total),
      uploaded_(registration->registration_data->uploaded),
      download_total_(registration->registration_data->download_total),
      downloaded_(registration->registration_data->downloaded),
      result_(registration->registration_data->result),
      failure_reason_(registration->registration_data->failure_reason) {
  DCHECK(service_worker_registration);
  Initialize(service_worker_registration,
             std::move(registration->registration_interface));
}

BackgroundFetchRegistration::~BackgroundFetchRegistration() = default;

void BackgroundFetchRegistration::Initialize(
    ServiceWorkerRegistration* registration,
    mojo::PendingRemote<mojom::blink::BackgroundFetchRegistrationService>
        registration_service) {
  DCHECK(!registration_);
  DCHECK(registration);
  DCHECK(!registration_service_);
  DCHECK(registration_service);

  registration_ = registration;
  registration_service_.Bind(std::move(registration_service));

  ExecutionContext* context = GetExecutionContext();
  if (!context || context->IsContextDestroyed())
    return;

  auto task_runner = context->GetTaskRunner(TaskType::kBackgroundFetch);
  registration_service_->AddRegistrationObserver(
      observer_receiver_.BindNewPipeAndPassRemote(task_runner));
}

void BackgroundFetchRegistration::OnProgress(
    uint64_t upload_total,
    uint64_t uploaded,
    uint64_t download_total,
    uint64_t downloaded,
    mojom::BackgroundFetchResult result,
    mojom::BackgroundFetchFailureReason failure_reason) {
  upload_total_ = upload_total;
  uploaded_ = uploaded;
  download_total_ = download_total;
  downloaded_ = downloaded;
  result_ = result;
  failure_reason_ = failure_reason;

  ExecutionContext* context = GetExecutionContext();
  if (!context || context->IsContextDestroyed())
    return;

  DCHECK(context->IsContextThread());
  DispatchEvent(*Event::Create(event_type_names::kProgress));
}

void BackgroundFetchRegistration::OnRecordsUnavailable() {
  records_available_ = false;
}

void BackgroundFetchRegistration::OnRequestCompleted(
    mojom::blink::FetchAPIRequestPtr request,
    mojom::blink::FetchAPIResponsePtr response) {
  for (auto* it = observers_.begin(); it != observers_.end();) {
    BackgroundFetchRecord* observer = it->Get();
    if (observer->ObservedUrl() == request->url) {
      observer->OnRequestCompleted(response->Clone());
      it = observers_.erase(it);
    } else {
      it++;
    }
  }
}

String BackgroundFetchRegistration::id() const {
  return developer_id_;
}

uint64_t BackgroundFetchRegistration::uploadTotal() const {
  return upload_total_;
}

uint64_t BackgroundFetchRegistration::uploaded() const {
  return uploaded_;
}

uint64_t BackgroundFetchRegistration::downloadTotal() const {
  return download_total_;
}

uint64_t BackgroundFetchRegistration::downloaded() const {
  return downloaded_;
}

bool BackgroundFetchRegistration::recordsAvailable() const {
  return records_available_;
}

const AtomicString& BackgroundFetchRegistration::InterfaceName() const {
  return event_target_names::kBackgroundFetchRegistration;
}

ExecutionContext* BackgroundFetchRegistration::GetExecutionContext() const {
  DCHECK(registration_);
  return registration_->GetExecutionContext();
}

ScriptPromise BackgroundFetchRegistration::abort(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  DCHECK(registration_);
  DCHECK(registration_service_);

  registration_service_->Abort(WTF::Bind(&BackgroundFetchRegistration::DidAbort,
                                         WrapPersistent(this),
                                         WrapPersistent(resolver)));

  return promise;
}

ScriptPromise BackgroundFetchRegistration::match(
    ScriptState* script_state,
    const RequestOrUSVString& request,
    const CacheQueryOptions* options,
    ExceptionState& exception_state) {
  return MatchImpl(
      script_state, base::make_optional<RequestOrUSVString>(request),
      mojom::blink::CacheQueryOptions::From(options), exception_state,
      /* match_all = */ false);
}

ScriptPromise BackgroundFetchRegistration::matchAll(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  return MatchImpl(script_state, /* request = */ base::nullopt,
                   /* cache_query_options = */ nullptr, exception_state,
                   /* match_all = */ true);
}

ScriptPromise BackgroundFetchRegistration::matchAll(
    ScriptState* script_state,
    const RequestOrUSVString& request,
    const CacheQueryOptions* options,
    ExceptionState& exception_state) {
  return MatchImpl(
      script_state, base::make_optional<RequestOrUSVString>(request),
      mojom::blink::CacheQueryOptions::From(options), exception_state,
      /* match_all = */ true);
}

ScriptPromise BackgroundFetchRegistration::MatchImpl(
    ScriptState* script_state,
    base::Optional<RequestOrUSVString> request,
    mojom::blink::CacheQueryOptionsPtr cache_query_options,
    ExceptionState& exception_state,
    bool match_all) {
  DCHECK(script_state);
  UMA_HISTOGRAM_BOOLEAN("BackgroundFetch.MatchCalledFromDocumentScope",
                        ExecutionContext::From(script_state)->IsDocument());
  UMA_HISTOGRAM_BOOLEAN("BackgroundFetch.MatchCalledWhenFetchIsIncomplete",
                        result_ == mojom::BackgroundFetchResult::UNSET);

  if (!records_available_) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kInvalidStateError,
            "The records associated with this background fetch are no longer "
            "available."));
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  // Convert |request| to mojom::blink::FetchAPIRequestPtr.
  mojom::blink::FetchAPIRequestPtr request_to_match;
  if (request.has_value()) {
    if (request->IsRequest()) {
      request_to_match = request->GetAsRequest()->CreateFetchAPIRequest();
    } else {
      Request* new_request = Request::Create(
          script_state, request->GetAsUSVString(), exception_state);
      if (exception_state.HadException())
        return ScriptPromise();
      request_to_match = new_request->CreateFetchAPIRequest();
    }
  }

  DCHECK(registration_);
  DCHECK(registration_service_);

  registration_service_->MatchRequests(
      std::move(request_to_match), std::move(cache_query_options), match_all,
      WTF::Bind(&BackgroundFetchRegistration::DidGetMatchingRequests,
                WrapPersistent(this), WrapPersistent(resolver), match_all));

  return promise;
}

void BackgroundFetchRegistration::DidGetMatchingRequests(
    ScriptPromiseResolver* resolver,
    bool return_all,
    Vector<mojom::blink::BackgroundFetchSettledFetchPtr> settled_fetches) {
  DCHECK(resolver);

  ScriptState* script_state = resolver->GetScriptState();
  // Do not remove this, |scope| is needed for calling ToV8()
  ScriptState::Scope scope(script_state);
  HeapVector<Member<BackgroundFetchRecord>> to_return;
  to_return.ReserveInitialCapacity(settled_fetches.size());

  for (auto& fetch : settled_fetches) {
    Request* request =
        Request::Create(script_state, *(fetch->request),
                        Request::ForServiceWorkerFetchEvent::kFalse);
    auto* record =
        MakeGarbageCollected<BackgroundFetchRecord>(request, script_state);

    // If this request is incomplete, enlist this record to receive updates on
    // the request.
    if (fetch->response.is_null() && !IsAborted())
      observers_.push_back(*record);

    UpdateRecord(record, fetch->response);
    to_return.push_back(record);
  }

  if (!return_all) {
    if (settled_fetches.IsEmpty()) {
      // Nothing was matched. Resolve with `undefined`.
      resolver->Resolve();
      return;
    }

    DCHECK_EQ(settled_fetches.size(), 1u);
    DCHECK_EQ(to_return.size(), 1u);
    resolver->Resolve(to_return[0]);
    return;
  }

  resolver->Resolve(to_return);
}

void BackgroundFetchRegistration::UpdateRecord(
    BackgroundFetchRecord* record,
    mojom::blink::FetchAPIResponsePtr& response) {
  DCHECK(record);

  if (!record->IsRecordPending())
    return;

  // Per the spec, resolve with a valid response, if there is one available,
  // even if the fetch has been aborted.
  if (!response.is_null()) {
    record->SetResponseAndUpdateState(response);
    return;
  }

  if (IsAborted()) {
    record->UpdateState(BackgroundFetchRecord::State::kAborted);
    return;
  }

  if (result_ != mojom::blink::BackgroundFetchResult::UNSET)
    record->UpdateState(BackgroundFetchRecord::State::kSettled);
}

bool BackgroundFetchRegistration::IsAborted() {
  return failure_reason_ ==
             mojom::BackgroundFetchFailureReason::CANCELLED_FROM_UI ||
         failure_reason_ ==
             mojom::BackgroundFetchFailureReason::CANCELLED_BY_DEVELOPER;
}

void BackgroundFetchRegistration::DidAbort(
    ScriptPromiseResolver* resolver,
    mojom::blink::BackgroundFetchError error) {
  switch (error) {
    case mojom::blink::BackgroundFetchError::NONE:
      resolver->Resolve(/* success = */ true);
      return;
    case mojom::blink::BackgroundFetchError::INVALID_ID:
      resolver->Resolve(/* success = */ false);
      return;
    case mojom::blink::BackgroundFetchError::STORAGE_ERROR:
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kAbortError,
          "Failed to abort registration due to I/O error."));
      return;
    case mojom::blink::BackgroundFetchError::SERVICE_WORKER_UNAVAILABLE:
    case mojom::blink::BackgroundFetchError::DUPLICATED_DEVELOPER_ID:
    case mojom::blink::BackgroundFetchError::INVALID_ARGUMENT:
    case mojom::blink::BackgroundFetchError::PERMISSION_DENIED:
    case mojom::blink::BackgroundFetchError::QUOTA_EXCEEDED:
    case mojom::blink::BackgroundFetchError::REGISTRATION_LIMIT_EXCEEDED:
      // Not applicable for this callback.
      break;
  }

  NOTREACHED();
}

const String BackgroundFetchRegistration::result() const {
  switch (result_) {
    case mojom::BackgroundFetchResult::SUCCESS:
      return "success";
    case mojom::BackgroundFetchResult::FAILURE:
      return "failure";
    case mojom::BackgroundFetchResult::UNSET:
      return "";
  }
  NOTREACHED();
}

const String BackgroundFetchRegistration::failureReason() const {
  switch (failure_reason_) {
    case mojom::BackgroundFetchFailureReason::NONE:
      return "";
    case mojom::BackgroundFetchFailureReason::CANCELLED_FROM_UI:
    case mojom::BackgroundFetchFailureReason::CANCELLED_BY_DEVELOPER:
      return "aborted";
    case mojom::BackgroundFetchFailureReason::BAD_STATUS:
      return "bad-status";
    case mojom::BackgroundFetchFailureReason::SERVICE_WORKER_UNAVAILABLE:
    case mojom::BackgroundFetchFailureReason::FETCH_ERROR:
      return "fetch-error";
    case mojom::BackgroundFetchFailureReason::QUOTA_EXCEEDED:
      return "quota-exceeded";
    case mojom::BackgroundFetchFailureReason::DOWNLOAD_TOTAL_EXCEEDED:
      return "download-total-exceeded";
  }
  NOTREACHED();
}

void BackgroundFetchRegistration::Dispose() {
  observer_receiver_.reset();
}

bool BackgroundFetchRegistration::HasPendingActivity() const {
  if (!GetExecutionContext())
    return false;
  if (GetExecutionContext()->IsContextDestroyed())
    return false;

  return !observers_.IsEmpty();
}

void BackgroundFetchRegistration::UpdateUI(
    const String& in_title,
    const SkBitmap& in_icon,
    mojom::blink::BackgroundFetchRegistrationService::UpdateUICallback
        callback) {
  DCHECK(registration_service_);
  registration_service_->UpdateUI(in_title, in_icon, std::move(callback));
}

void BackgroundFetchRegistration::Trace(Visitor* visitor) {
  visitor->Trace(registration_);
  visitor->Trace(observers_);
  EventTargetWithInlineData::Trace(visitor);
  ActiveScriptWrappable::Trace(visitor);
}

}  // namespace blink
