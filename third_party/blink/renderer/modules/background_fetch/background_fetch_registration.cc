// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/background_fetch/background_fetch_registration.h"

#include "base/optional.h"
#include "third_party/blink/public/platform/modules/background_fetch/web_background_fetch_registration.h"
#include "third_party/blink/public/platform/modules/fetch/fetch_api_request.mojom-blink.h"
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

namespace blink {

BackgroundFetchRegistration::BackgroundFetchRegistration(
    const String& developer_id,
    const String& unique_id,
    unsigned long long upload_total,
    unsigned long long uploaded,
    unsigned long long download_total,
    unsigned long long downloaded,
    mojom::BackgroundFetchResult result,
    mojom::BackgroundFetchFailureReason failure_reason)
    : developer_id_(developer_id),
      unique_id_(unique_id),
      upload_total_(upload_total),
      uploaded_(uploaded),
      download_total_(download_total),
      downloaded_(downloaded),
      result_(result),
      failure_reason_(failure_reason),
      observer_binding_(this) {}

BackgroundFetchRegistration::BackgroundFetchRegistration(
    ServiceWorkerRegistration* registration,
    const WebBackgroundFetchRegistration& web_registration)
    : developer_id_(web_registration.developer_id),
      unique_id_(web_registration.unique_id),
      upload_total_(web_registration.upload_total),
      uploaded_(web_registration.uploaded),
      download_total_(web_registration.download_total),
      downloaded_(web_registration.downloaded),
      result_(web_registration.result),
      failure_reason_(web_registration.failure_reason),
      observer_binding_(this) {
  DCHECK(registration);
  Initialize(registration);
}

BackgroundFetchRegistration::~BackgroundFetchRegistration() = default;

void BackgroundFetchRegistration::Initialize(
    ServiceWorkerRegistration* registration) {
  DCHECK(!registration_);
  DCHECK(registration);

  registration_ = registration;

  mojom::blink::BackgroundFetchRegistrationObserverPtr observer;
  observer_binding_.Bind(mojo::MakeRequest(&observer));

  BackgroundFetchBridge::From(registration_)
      ->AddRegistrationObserver(unique_id_, std::move(observer));
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
  DispatchEvent(*Event::Create(EventTypeNames::progress));
}

void BackgroundFetchRegistration::OnRecordsUnavailable() {
  records_available_ = false;
}

String BackgroundFetchRegistration::id() const {
  return developer_id_;
}

unsigned long long BackgroundFetchRegistration::uploadTotal() const {
  return upload_total_;
}

unsigned long long BackgroundFetchRegistration::uploaded() const {
  return uploaded_;
}

unsigned long long BackgroundFetchRegistration::downloadTotal() const {
  return download_total_;
}

unsigned long long BackgroundFetchRegistration::downloaded() const {
  return downloaded_;
}

bool BackgroundFetchRegistration::recordsAvailable() const {
  return records_available_;
}

const AtomicString& BackgroundFetchRegistration::InterfaceName() const {
  return EventTargetNames::BackgroundFetchRegistration;
}

ExecutionContext* BackgroundFetchRegistration::GetExecutionContext() const {
  DCHECK(registration_);
  return registration_->GetExecutionContext();
}

ScriptPromise BackgroundFetchRegistration::abort(ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  DCHECK(registration_);
  BackgroundFetchBridge::From(registration_)
      ->Abort(developer_id_, unique_id_,
              WTF::Bind(&BackgroundFetchRegistration::DidAbort,
                        WrapPersistent(this), WrapPersistent(resolver)));

  return promise;
}

ScriptPromise BackgroundFetchRegistration::match(
    ScriptState* script_state,
    const RequestOrUSVString& request,
    const CacheQueryOptions& options,
    ExceptionState& exception_state) {
  return MatchImpl(
      script_state, base::make_optional<RequestOrUSVString>(request),
      Cache::ToQueryParams(options), exception_state, /* match_all = */ false);
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
    const CacheQueryOptions& options,
    ExceptionState& exception_state) {
  return MatchImpl(
      script_state, base::make_optional<RequestOrUSVString>(request),
      Cache::ToQueryParams(options), exception_state, /* match_all = */ true);
}

ScriptPromise BackgroundFetchRegistration::MatchImpl(
    ScriptState* script_state,
    base::Optional<RequestOrUSVString> request,
    mojom::blink::QueryParamsPtr cache_query_params,
    ExceptionState& exception_state,
    bool match_all) {
  // TODO(crbug.com/875201): Update this check once we support access to active
  // fetches.
  if (result_ == mojom::BackgroundFetchResult::UNSET) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(
            DOMExceptionCode::kInvalidStateError,
            "Access to records for in-progress background fetches is not yet "
            "implemented. Please see crbug.com/875201 for more details."));
  }

  if (!records_available_) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(
            DOMExceptionCode::kInvalidStateError,
            "The records associated with this background fetch are no longer "
            "available."));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  // Convert |request| to WebServiceWorkerRequest.
  base::Optional<WebServiceWorkerRequest> optional_request;
  if (request.has_value()) {
    WebServiceWorkerRequest request_to_match;
    if (request->IsRequest()) {
      request->GetAsRequest()->PopulateWebServiceWorkerRequest(
          request_to_match);
    } else {
      Request* new_request = Request::Create(
          script_state, request->GetAsUSVString(), exception_state);
      if (exception_state.HadException())
        return ScriptPromise();
      new_request->PopulateWebServiceWorkerRequest(request_to_match);
    }
    optional_request = request_to_match;
  }

  DCHECK(registration_);

  BackgroundFetchBridge::From(registration_)
      ->MatchRequests(
          developer_id_, unique_id_, optional_request,
          std::move(cache_query_params), match_all,
          WTF::Bind(&BackgroundFetchRegistration::DidGetMatchingRequests,
                    WrapPersistent(this), WrapPersistent(resolver), match_all));
  return promise;
}

void BackgroundFetchRegistration::DidGetMatchingRequests(
    ScriptPromiseResolver* resolver,
    bool return_all,
    Vector<mojom::blink::BackgroundFetchSettledFetchPtr> settled_fetches) {
  ScriptState* script_state = resolver->GetScriptState();
  // Do not remove this, |scope| is needed for calling ToV8()
  ScriptState::Scope scope(script_state);
  HeapVector<Member<BackgroundFetchRecord>> to_return;
  to_return.ReserveInitialCapacity(settled_fetches.size());
  for (const auto& fetch : settled_fetches) {
    Request* request = Request::Create(script_state, fetch->request);

    Response* response = fetch->response
                             ? Response::Create(script_state, *fetch->response)
                             : nullptr;

    bool aborted =
        failure_reason_ ==
            mojom::BackgroundFetchFailureReason::CANCELLED_FROM_UI ||
        failure_reason_ ==
            mojom::BackgroundFetchFailureReason::CANCELLED_BY_DEVELOPER;

    to_return.push_back(new BackgroundFetchRecord(request, response, aborted));
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
      resolver->Reject(DOMException::Create(
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
    case mojom::BackgroundFetchFailureReason::TOTAL_DOWNLOAD_SIZE_EXCEEDED:
      return "total-download-exceeded";
  }
  NOTREACHED();
}

void BackgroundFetchRegistration::Dispose() {
  observer_binding_.Close();
}

void BackgroundFetchRegistration::Trace(Visitor* visitor) {
  visitor->Trace(registration_);
  EventTargetWithInlineData::Trace(visitor);
}

}  // namespace blink
