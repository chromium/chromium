// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/background_fetch/background_fetch_manager.h"

#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "services/network/public/mojom/ip_address_space.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_request_requestorusvstringsequence_usvstring.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_request_usvstring.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_background_fetch_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_image_resource.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/fetch/body.h"
#include "third_party/blink/renderer/core/fetch/body_stream_buffer.h"
#include "third_party/blink/renderer/core/fetch/request.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/background_fetch/background_fetch_bridge.h"
#include "third_party/blink/renderer/modules/background_fetch/background_fetch_icon_loader.h"
#include "third_party/blink/renderer/modules/background_fetch/background_fetch_registration.h"
#include "third_party/blink/renderer/modules/background_fetch/background_fetch_type_converters.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_registration.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/cors/cors.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_utils.h"
#include "third_party/blink/renderer/platform/network/network_utils.h"
#include "third_party/blink/renderer/platform/weborigin/known_ports.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/kurl_hash.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace blink {

namespace {

// Message for the TypeError thrown when an empty request sequence is seen.
const char kEmptyRequestSequenceErrorMessage[] =
    "At least one request must be given.";

ScriptPromise RejectWithTypeError(ScriptState* script_state,
                                  const KURL& request_url,
                                  const String& reason,
                                  ExceptionState& exception_state) {
  exception_state.ThrowTypeError("Refused to fetch '" +
                                 request_url.ElidedString() + "' because " +
                                 reason + ".");
  return ScriptPromise();
}

// Returns whether the |request_url| should be blocked by the CSP. Must be
// called synchronously from the background fetch call.
bool ShouldBlockDueToCSP(ExecutionContext* execution_context,
                         const KURL& request_url) {
  return !execution_context->GetContentSecurityPolicyForCurrentWorld()
              ->AllowConnectToSource(request_url, request_url,
                                     RedirectStatus::kNoRedirect);
}

bool ShouldBlockPort(const KURL& request_url) {
  // https://fetch.spec.whatwg.org/#block-bad-port
  return !IsPortAllowedForScheme(request_url);
}

bool ShouldBlockCredentials(ExecutionContext* execution_context,
                            const KURL& request_url) {
  // "If parsedURL includes credentials, then throw a TypeError."
  // https://fetch.spec.whatwg.org/#dom-request
  // (Added by https://github.com/whatwg/fetch/issues/26).
  // "A URL includes credentials if its username or password is not the empty
  // string."
  // https://url.spec.whatwg.org/#include-credentials
  return !request_url.User().empty() || !request_url.Pass().empty();
}

bool ShouldBlockScheme(const KURL& request_url) {
  // Require http(s), i.e. block data:, wss: and file:
  // https://github.com/WICG/background-fetch/issues/44
  return !request_url.ProtocolIs(WTF::g_http_atom) &&
         !request_url.ProtocolIs(WTF::g_https_atom);
}

bool ShouldBlockDanglingMarkup(const KURL& request_url) {
  // "If request's url's potentially-dangling-markup flag is set, and request's
  // url's scheme is an HTTP(S) scheme, then set response to a network error."
  // https://github.com/whatwg/fetch/pull/519
  // https://github.com/whatwg/fetch/issues/546
  return request_url.PotentiallyDanglingMarkup() &&
         request_url.ProtocolIsInHTTPFamily();
}

scoped_refptr<BlobDataHandle> ExtractBlobHandle(
    Request* request,
    ExceptionState& exception_state) {
  DCHECK(request);

  if (request->IsBodyLocked() || request->IsBodyUsed()) {
    exception_state.ThrowTypeError("Request body is already used");
    return nullptr;
  }

  BodyStreamBuffer* buffer = request->BodyBuffer();
  if (!buffer)
    return nullptr;

  auto blob_handle = buffer->DrainAsBlobDataHandle(
      BytesConsumer::BlobSizePolicy::kDisallowBlobWithInvalidSize);

  return blob_handle;
}

}  // namespace

BackgroundFetchManager::BackgroundFetchManager(
    ServiceWorkerRegistration* registration)
    : ExecutionContextLifecycleObserver(registration->GetExecutionContext()),
      registration_(registration) {
  DCHECK(registration);
  bridge_ = BackgroundFetchBridge::From(registration_);
}

ScriptPromise BackgroundFetchManager::fetch(
    ScriptState* script_state,
    const String& id,
    const V8UnionRequestInfoOrRequestOrUSVStringSequence* requests,
    const BackgroundFetchOptions* options,
    ExceptionState& exception_state) {
  if (!registration_->active()) {
    exception_state.ThrowTypeError(
        "No active registration available on the ServiceWorkerRegistration.");
    return ScriptPromise();
  }

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  if (execution_context->IsInFencedFrame()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "backgroundFetch is not allowed in fenced frames.");
    return ScriptPromise();
  }

  bool has_requests_with_body;
  Vector<mojom::blink::FetchAPIRequestPtr> fetch_api_requests =
      CreateFetchAPIRequestVector(script_state, requests, exception_state,
                                  &has_requests_with_body);
  if (exception_state.HadException())
    return ScriptPromise();

  // Record whether any requests had a body. If there were, reject the promise.
  UMA_HISTOGRAM_BOOLEAN("BackgroundFetch.HasRequestsWithBody",
                        has_requests_with_body);

  // A HashSet to find whether there are any duplicate requests within the
  // fetch. https://bugs.chromium.org/p/chromium/issues/detail?id=871174.
  HashSet<KURL> kurls;

  // Based on security steps from https://fetch.spec.whatwg.org/#main-fetch
  // TODO(crbug.com/757441): Remove all this duplicative code once Fetch (and
  // all its security checks) are implemented in the Network Service, such that
  // the Download Service in the browser process can use it without having to
  // spin up a renderer process.
  for (const mojom::blink::FetchAPIRequestPtr& request : fetch_api_requests) {
    KURL request_url(request->url);

    if (!request_url.IsValid()) {
      return RejectWithTypeError(script_state, request_url,
                                 "that URL is invalid", exception_state);
    }

    // https://wicg.github.io/background-fetch/#dom-backgroundfetchmanager-fetch
    // ""If |internalRequest|’s mode is "no-cors", then return a promise
    //   rejected with a TypeError.""
    if (request->mode == network::mojom::RequestMode::kNoCors) {
      return RejectWithTypeError(script_state, request_url,
                                 "the request mode must not be no-cors",
                                 exception_state);
    }

    // Check this before mixed content, so that if mixed content is blocked by
    // CSP they get a CSP warning rather than a mixed content failure.
    if (ShouldBlockDueToCSP(execution_context, request_url)) {
      return RejectWithTypeError(script_state, request_url,
                                 "it violates the Content Security Policy",
                                 exception_state);
    }

    if (ShouldBlockPort(request_url)) {
      return RejectWithTypeError(script_state, request_url,
                                 "that port is not allowed", exception_state);
    }

    if (ShouldBlockCredentials(execution_context, request_url)) {
      return RejectWithTypeError(script_state, request_url,
                                 "that URL contains a username/password",
                                 exception_state);
    }

    if (ShouldBlockScheme(request_url)) {
      return RejectWithTypeError(script_state, request_url,
                                 "only the https: scheme is allowed, or http: "
                                 "for loopback IPs",
                                 exception_state);
    }

    if (ShouldBlockDanglingMarkup(request_url)) {
      return RejectWithTypeError(script_state, request_url,
                                 "it contains dangling markup",
                                 exception_state);
    }

    kurls.insert(request_url);
  }

  UMA_HISTOGRAM_BOOLEAN("BackgroundFetch.HasDuplicateRequests",
                        kurls.size() != fetch_api_requests.size());

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();

  // Pick the best icon, and load it.
  // Inability to load them should not be fatal to the fetch.
  mojom::blink::BackgroundFetchOptionsPtr options_ptr =
      mojom::blink::BackgroundFetchOptions::From(options);
  if (options->icons().size()) {
    BackgroundFetchIconLoader* loader =
        MakeGarbageCollected<BackgroundFetchIconLoader>();
    loaders_.push_back(loader);
    loader->Start(bridge_.Get(), execution_context, options->icons(),
                  resolver->WrapCallbackInScriptScope(WTF::BindOnce(
                      &BackgroundFetchManager::DidLoadIcons,
                      WrapPersistent(this), id, std::move(fetch_api_requests),
                      std::move(options_ptr), WrapWeakPersistent(loader))));
    return promise;
  }

  DidLoadIcons(id, std::move(fetch_api_requests), std::move(options_ptr),
               nullptr, resolver, SkBitmap(),
               -1 /* ideal_to_chosen_icon_size */);
  return promise;
}

void BackgroundFetchManager::DidLoadIcons(
    const String& id,
    Vector<mojom::blink::FetchAPIRequestPtr> requests,
    mojom::blink::BackgroundFetchOptionsPtr options,
    BackgroundFetchIconLoader* loader,
    ScriptPromiseResolver* resolver,
    const SkBitmap& icon,
    int64_t ideal_to_chosen_icon_size) {
  if (loader)
    loaders_.erase(base::ranges::find(loaders_, loader));

  auto ukm_data = mojom::blink::BackgroundFetchUkmData::New();
  ukm_data->ideal_to_chosen_icon_size = ideal_to_chosen_icon_size;
  bridge_->Fetch(id, std::move(requests), std::move(options), icon,
                 std::move(ukm_data),
                 resolver->WrapCallbackInScriptScope(
                     WTF::BindOnce(&BackgroundFetchManager::DidFetch,
                                   WrapPersistent(this), base::Time::Now())));
}

void BackgroundFetchManager::DidFetch(
    base::Time time_started,
    ScriptPromiseResolver* resolver,
    mojom::blink::BackgroundFetchError error,
    BackgroundFetchRegistration* registration) {
  UMA_HISTOGRAM_TIMES("BackgroundFetch.Manager.FetchDuration",
                      base::Time::Now() - time_started);

  ScriptState* script_state = resolver->GetScriptState();
  ScriptState::Scope scope(script_state);

  switch (error) {
    case mojom::blink::BackgroundFetchError::NONE:
      DCHECK(registration);
      resolver->Resolve(registration);
      return;
    case mojom::blink::BackgroundFetchError::DUPLICATED_DEVELOPER_ID:
      DCHECK(!registration);
      resolver->Reject(V8ThrowException::CreateTypeError(
          script_state->GetIsolate(),
          "There already is a registration for the given id."));
      return;
    case mojom::blink::BackgroundFetchError::PERMISSION_DENIED:
      resolver->Reject(V8ThrowException::CreateTypeError(
          script_state->GetIsolate(),
          "This origin does not have permission to start a fetch."));
      return;
    case mojom::blink::BackgroundFetchError::STORAGE_ERROR:
      DCHECK(!registration);
      resolver->Reject(V8ThrowException::CreateTypeError(
          script_state->GetIsolate(),
          "Failed to store registration due to I/O error."));
      return;
    case mojom::blink::BackgroundFetchError::SERVICE_WORKER_UNAVAILABLE:
      resolver->Reject(V8ThrowException::CreateTypeError(
          script_state->GetIsolate(),
          "There is no service worker available to service the fetch."));
      return;
    case mojom::blink::BackgroundFetchError::QUOTA_EXCEEDED:
      resolver->RejectWithDOMException(DOMExceptionCode::kQuotaExceededError,
                                       "Quota exceeded.");
      return;
    case mojom::blink::BackgroundFetchError::REGISTRATION_LIMIT_EXCEEDED:
      resolver->Reject(V8ThrowException::CreateTypeError(
          script_state->GetIsolate(),
          "There are too many active fetches for this origin."));
      return;
    case mojom::blink::BackgroundFetchError::INVALID_ARGUMENT:
    case mojom::blink::BackgroundFetchError::INVALID_ID:
      // Not applicable for this callback.
      break;
  }

  NOTREACHED();
}

ScriptPromise BackgroundFetchManager::get(ScriptState* script_state,
                                          const String& id,
                                          ExceptionState& exception_state) {
  // Creating a Background Fetch registration requires an activated worker, so
  // if |registration_| has not been activated we can skip the Mojo roundtrip.
  if (!registration_->active())
    return ScriptPromise::CastUndefined(script_state);

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  if (execution_context->IsInFencedFrame()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "backgroundFetch is not allowed in fenced frames.");
    return ScriptPromise();
  }

  ScriptState::Scope scope(script_state);

  if (id.empty()) {
    exception_state.ThrowTypeError("The provided id is invalid.");
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();

  bridge_->GetRegistration(
      id, resolver->WrapCallbackInScriptScope(
              WTF::BindOnce(&BackgroundFetchManager::DidGetRegistration,
                            WrapPersistent(this), base::Time::Now())));

  return promise;
}

// static
Vector<mojom::blink::FetchAPIRequestPtr>
BackgroundFetchManager::CreateFetchAPIRequestVector(
    ScriptState* script_state,
    const V8UnionRequestInfoOrRequestOrUSVStringSequence* requests,
    ExceptionState& exception_state,
    bool* has_requests_with_body) {
  DCHECK(requests);
  DCHECK(has_requests_with_body);

  Vector<mojom::blink::FetchAPIRequestPtr> fetch_api_requests;
  *has_requests_with_body = false;

  switch (requests->GetContentType()) {
    case V8UnionRequestInfoOrRequestOrUSVStringSequence::ContentType::
        kRequestOrUSVStringSequence: {
      const HeapVector<Member<V8RequestInfo>>& request_vector =
          requests->GetAsRequestOrUSVStringSequence();

      // Throw a TypeError when the developer has passed an empty sequence.
      if (request_vector.empty()) {
        exception_state.ThrowTypeError(kEmptyRequestSequenceErrorMessage);
        return {};
      }

      fetch_api_requests.reserve(request_vector.size());
      for (const auto& request_info : request_vector) {
        Request* request = nullptr;
        switch (request_info->GetContentType()) {
          case V8RequestInfo::ContentType::kRequest:
            request = request_info->GetAsRequest();
            break;
          case V8RequestInfo::ContentType::kUSVString:
            request = Request::Create(
                script_state, request_info->GetAsUSVString(), exception_state);
            if (exception_state.HadException())
              return {};
            break;
        }
        *has_requests_with_body |= request->HasBody();
        fetch_api_requests.push_back(request->CreateFetchAPIRequest());
        fetch_api_requests.back()->blob =
            ExtractBlobHandle(request, exception_state);
        if (exception_state.HadException())
          return {};
      }
      break;
    }
    case V8UnionRequestInfoOrRequestOrUSVStringSequence::ContentType::
        kRequest: {
      Request* request = requests->GetAsRequest();
      *has_requests_with_body = request->HasBody();
      fetch_api_requests.push_back(request->CreateFetchAPIRequest());
      fetch_api_requests.back()->blob =
          ExtractBlobHandle(request, exception_state);
      if (exception_state.HadException())
        return {};
      break;
    }
    case V8UnionRequestInfoOrRequestOrUSVStringSequence::ContentType::
        kUSVString: {
      Request* request = Request::Create(
          script_state, requests->GetAsUSVString(), exception_state);
      if (exception_state.HadException())
        return {};
      *has_requests_with_body = request->HasBody();
      fetch_api_requests.push_back(request->CreateFetchAPIRequest());
      fetch_api_requests.back()->blob =
          ExtractBlobHandle(request, exception_state);
      break;
    }
  }

  return fetch_api_requests;
}

void BackgroundFetchManager::DidGetRegistration(
    base::Time time_started,
    ScriptPromiseResolver* resolver,
    mojom::blink::BackgroundFetchError error,
    BackgroundFetchRegistration* registration) {
  UMA_HISTOGRAM_TIMES("BackgroundFetch.Manager.GetDuration",
                      base::Time::Now() - time_started);

  ScriptState* script_state = resolver->GetScriptState();
  ScriptState::Scope scope(script_state);

  switch (error) {
    case mojom::blink::BackgroundFetchError::NONE:
      DCHECK(registration);
      resolver->Resolve(registration);
      return;
    case mojom::blink::BackgroundFetchError::INVALID_ID:
      DCHECK(!registration);
      resolver->Resolve(v8::Undefined(script_state->GetIsolate()));
      return;
    case mojom::blink::BackgroundFetchError::STORAGE_ERROR:
      DCHECK(!registration);
      resolver->RejectWithDOMException(
          DOMExceptionCode::kAbortError,
          "Failed to get registration due to I/O error.");
      return;
    case mojom::blink::BackgroundFetchError::SERVICE_WORKER_UNAVAILABLE:
      resolver->RejectWithDOMException(
          DOMExceptionCode::kInvalidStateError,
          "There's no service worker available to service the fetch.");
      return;
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

ScriptPromise BackgroundFetchManager::getIds(ScriptState* script_state,
                                             ExceptionState& exception_state) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  if (execution_context->IsInFencedFrame()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "backgroundFetch is not allowed in fenced frames.");
    return ScriptPromise();
  }

  // Creating a Background Fetch registration requires an activated worker, so
  // if |registration_| has not been activated we can skip the Mojo roundtrip.
  if (!registration_->active()) {
    return ScriptPromise::Cast(script_state,
                               v8::Array::New(script_state->GetIsolate()));
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();

  bridge_->GetDeveloperIds(resolver->WrapCallbackInScriptScope(
      WTF::BindOnce(&BackgroundFetchManager::DidGetDeveloperIds,
                    WrapPersistent(this), base::Time::Now())));

  return promise;
}

void BackgroundFetchManager::DidGetDeveloperIds(
    base::Time time_started,
    ScriptPromiseResolver* resolver,
    mojom::blink::BackgroundFetchError error,
    const Vector<String>& developer_ids) {
  UMA_HISTOGRAM_TIMES("BackgroundFetch.Manager.GetIdsDuration",
                      base::Time::Now() - time_started);

  ScriptState::Scope scope(resolver->GetScriptState());

  switch (error) {
    case mojom::blink::BackgroundFetchError::NONE:
      resolver->Resolve(developer_ids);
      return;
    case mojom::blink::BackgroundFetchError::STORAGE_ERROR:
      DCHECK(developer_ids.empty());
      resolver->RejectWithDOMException(
          DOMExceptionCode::kAbortError,
          "Failed to get registration IDs due to I/O error.");
      return;
    case mojom::blink::BackgroundFetchError::DUPLICATED_DEVELOPER_ID:
    case mojom::blink::BackgroundFetchError::INVALID_ARGUMENT:
    case mojom::blink::BackgroundFetchError::INVALID_ID:
    case mojom::blink::BackgroundFetchError::PERMISSION_DENIED:
    case mojom::blink::BackgroundFetchError::SERVICE_WORKER_UNAVAILABLE:
    case mojom::blink::BackgroundFetchError::QUOTA_EXCEEDED:
    case mojom::blink::BackgroundFetchError::REGISTRATION_LIMIT_EXCEEDED:
      // Not applicable for this callback.
      break;
  }

  NOTREACHED();
}

void BackgroundFetchManager::Trace(Visitor* visitor) const {
  visitor->Trace(registration_);
  visitor->Trace(bridge_);
  visitor->Trace(loaders_);
  ExecutionContextLifecycleObserver::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

void BackgroundFetchManager::ContextDestroyed() {
  for (const auto& loader : loaders_) {
    if (loader)
      loader->Stop();
  }
  loaders_.clear();
}

}  // namespace blink
