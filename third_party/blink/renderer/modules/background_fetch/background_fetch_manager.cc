// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/background_fetch/background_fetch_manager.h"

#include "base/metrics/histogram_macros.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_request.h"
#include "third_party/blink/renderer/bindings/core/v8/request_or_usv_string.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/request_or_usv_string_or_request_or_usv_string_sequence.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/fetch/request.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/modules/background_fetch/background_fetch_bridge.h"
#include "third_party/blink/renderer/modules/background_fetch/background_fetch_icon_loader.h"
#include "third_party/blink/renderer/modules/background_fetch/background_fetch_options.h"
#include "third_party/blink/renderer/modules/background_fetch/background_fetch_registration.h"
#include "third_party/blink/renderer/modules/background_fetch/background_fetch_type_converters.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_registration.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
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

// Message for the TypeError thrown when a null request is seen.
const char kNullRequestErrorMessage[] = "Requests must not be null.";

ScriptPromise RejectWithTypeError(ScriptState* script_state,
                                  const KURL& request_url,
                                  const String& reason) {
  return ScriptPromise::Reject(
      script_state, V8ThrowException::CreateTypeError(
                        script_state->GetIsolate(),
                        "Refused to fetch '" + request_url.ElidedString() +
                            "' because " + reason + "."));
}

bool ShouldBlockDueToCSP(ExecutionContext* execution_context,
                         const KURL& request_url) {
  return !ContentSecurityPolicy::ShouldBypassMainWorld(execution_context) &&
         !execution_context->GetContentSecurityPolicy()->AllowConnectToSource(
             request_url);
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
  return !request_url.User().IsEmpty() || !request_url.Pass().IsEmpty();
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

bool ShouldBlockGateWayAttacks(ExecutionContext* execution_context,
                               const WebServiceWorkerRequest& web_request,
                               const KURL& request_url) {
  if (RuntimeEnabledFeatures::CorsRFC1918Enabled()) {
    mojom::IPAddressSpace requestor_space =
        execution_context->GetSecurityContext().AddressSpace();

    // TODO(mkwst): This only checks explicit IP addresses. We'll have to move
    // all this up to //net and //content in order to have any real impact on
    // gateway attacks. That turns out to be a TON of work (crbug.com/378566).
    mojom::IPAddressSpace target_space = mojom::IPAddressSpace::kPublic;
    if (network_utils::IsReservedIPAddress(request_url.Host()))
      target_space = mojom::IPAddressSpace::kPrivate;
    if (SecurityOrigin::Create(request_url)->IsLocalhost())
      target_space = mojom::IPAddressSpace::kLocal;

    bool is_external_request = requestor_space > target_space;
    if (is_external_request)
      return true;
  }

  return false;
}

}  // namespace

BackgroundFetchManager::BackgroundFetchManager(
    ServiceWorkerRegistration* registration)
    : ContextLifecycleObserver(registration->GetExecutionContext()),
      registration_(registration) {
  DCHECK(registration);
  bridge_ = BackgroundFetchBridge::From(registration_);
}

ScriptPromise BackgroundFetchManager::fetch(
    ScriptState* script_state,
    const String& id,
    const RequestOrUSVStringOrRequestOrUSVStringSequence& requests,
    const BackgroundFetchOptions& options,
    ExceptionState& exception_state) {
  if (!registration_->active()) {
    return ScriptPromise::Reject(
        script_state,
        V8ThrowException::CreateTypeError(script_state->GetIsolate(),
                                          "No active registration available on "
                                          "the ServiceWorkerRegistration."));
  }

  bool has_requests_with_body;
  Vector<WebServiceWorkerRequest> web_requests = CreateWebRequestVector(
      script_state, requests, exception_state, &has_requests_with_body);
  if (exception_state.HadException())
    return ScriptPromise();

  // Record whether any requests had a body. If there were, reject the promise.
  UMA_HISTOGRAM_BOOLEAN("BackgroundFetch.HasRequestsWithBody",
                        has_requests_with_body);

  if (has_requests_with_body &&
      !RuntimeEnabledFeatures::BackgroundFetchUploadsEnabled()) {
    return ScriptPromise::Reject(
        script_state, V8ThrowException::CreateTypeError(
                          script_state->GetIsolate(),
                          "Requests with a body are not yet supported. "
                          "For updates check http://crbug.com/774054"));
  }

  ExecutionContext* execution_context = ExecutionContext::From(script_state);

  // A HashSet to find whether there are any duplicate requests within the
  // fetch. https://bugs.chromium.org/p/chromium/issues/detail?id=871174.
  HashSet<KURL> kurls;

  // Based on security steps from https://fetch.spec.whatwg.org/#main-fetch
  // TODO(crbug.com/757441): Remove all this duplicative code once Fetch (and
  // all its security checks) are implemented in the Network Service, such that
  // the Download Service in the browser process can use it without having to
  // spin up a renderer process.
  for (const WebServiceWorkerRequest& web_request : web_requests) {
    KURL request_url(web_request.Url());

    if (!request_url.IsValid()) {
      return RejectWithTypeError(script_state, request_url,
                                 "that URL is invalid");
    }

    // https://wicg.github.io/background-fetch/#dom-backgroundfetchmanager-fetch
    // ""If |internalRequest|’s mode is "no-cors", then return a promise
    //   rejected with a TypeError.""
    if (web_request.Mode() == network::mojom::FetchRequestMode::kNoCORS) {
      return RejectWithTypeError(script_state, request_url,
                                 "the request mode must not be no-cors");
    }

    // Check this before mixed content, so that if mixed content is blocked by
    // CSP they get a CSP warning rather than a mixed content failure.
    if (ShouldBlockDueToCSP(execution_context, request_url)) {
      return RejectWithTypeError(script_state, request_url,
                                 "it violates the Content Security Policy");
    }

    if (ShouldBlockPort(request_url)) {
      return RejectWithTypeError(script_state, request_url,
                                 "that port is not allowed");
    }

    if (ShouldBlockCredentials(execution_context, request_url)) {
      return RejectWithTypeError(script_state, request_url,
                                 "that URL contains a username/password");
    }

    if (ShouldBlockScheme(request_url)) {
      return RejectWithTypeError(script_state, request_url,
                                 "only the https: scheme is allowed, or http: "
                                 "for loopback IPs");
    }

    if (ShouldBlockDanglingMarkup(request_url)) {
      return RejectWithTypeError(script_state, request_url,
                                 "it contains dangling markup");
    }

    if (ShouldBlockGateWayAttacks(execution_context, web_request,
                                  request_url)) {
      return RejectWithTypeError(script_state, request_url,
                                 "Requestor IP address space doesn't match the "
                                 "target address space.");
    }

    kurls.insert(request_url);
  }

  const bool has_duplicate_requests = kurls.size() != web_requests.size();

  UMA_HISTOGRAM_BOOLEAN("BackgroundFetch.HasDuplicateRequests",
                        has_duplicate_requests);

  // Note: This is a proprietary check, due to the way Chrome currently handles
  // storing background fetch records. Entries are keyed by the URL, so if two
  // requests have the same URL, and different responses, the first response
  // will be lost when the second request/response pair is stored.
  if (has_duplicate_requests) {
    return ScriptPromise::Reject(
        script_state,
        V8ThrowException::CreateTypeError(
            script_state->GetIsolate(),
            "Fetches with duplicate requests are not yet supported. "
            "Consider adding query params to make the requests unique. "
            "For updates check http://crbug.com/871174"));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  // Pick the best icon, and load it.
  // Inability to load them should not be fatal to the fetch.
  mojom::blink::BackgroundFetchOptionsPtr options_ptr =
      mojom::blink::BackgroundFetchOptions::From(options);
  if (options.icons().size()) {
    BackgroundFetchIconLoader* loader = new BackgroundFetchIconLoader();
    loaders_.push_back(loader);
    loader->Start(
        bridge_.Get(), execution_context, options.icons(),
        WTF::Bind(&BackgroundFetchManager::DidLoadIcons, WrapPersistent(this),
                  id, WTF::Passed(std::move(web_requests)),
                  std::move(options_ptr), WrapPersistent(resolver),
                  WrapWeakPersistent(loader)));
    return promise;
  }

  DidLoadIcons(id, std::move(web_requests), std::move(options_ptr), resolver,
               nullptr, SkBitmap(), -1 /* ideal_to_chosen_icon_size */);
  return promise;
}

void BackgroundFetchManager::DidLoadIcons(
    const String& id,
    Vector<WebServiceWorkerRequest> web_requests,
    mojom::blink::BackgroundFetchOptionsPtr options,
    ScriptPromiseResolver* resolver,
    BackgroundFetchIconLoader* loader,
    const SkBitmap& icon,
    int64_t ideal_to_chosen_icon_size) {
  if (loader)
    loaders_.erase(std::find(loaders_.begin(), loaders_.end(), loader));

  auto ukm_data = mojom::blink::BackgroundFetchUkmData::New();
  ukm_data->ideal_to_chosen_icon_size = ideal_to_chosen_icon_size;
  bridge_->Fetch(
      id, std::move(web_requests), std::move(options), icon,
      std::move(ukm_data),
      WTF::Bind(&BackgroundFetchManager::DidFetch, WrapPersistent(this),
                WrapPersistent(resolver), base::Time::Now()));
}

void BackgroundFetchManager::DidFetch(
    ScriptPromiseResolver* resolver,
    base::Time time_started,
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
      resolver->Reject(DOMException::Create(
          DOMExceptionCode::kQuotaExceededError, "Quota exceeded."));
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
                                          const String& id) {
  // Creating a Background Fetch registration requires an activated worker, so
  // if |registration_| has not been activated we can skip the Mojo roundtrip.
  if (!registration_->active())
    return ScriptPromise::CastUndefined(script_state);

  ScriptState::Scope scope(script_state);

  if (id.IsEmpty()) {
    return ScriptPromise::Reject(
        script_state,
        V8ThrowException::CreateTypeError(script_state->GetIsolate(),
                                          "The provided id is invalid."));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  bridge_->GetRegistration(
      id, WTF::Bind(&BackgroundFetchManager::DidGetRegistration,
                    WrapPersistent(this), WrapPersistent(resolver),
                    base::Time::Now()));

  return promise;
}

// static
Vector<WebServiceWorkerRequest> BackgroundFetchManager::CreateWebRequestVector(
    ScriptState* script_state,
    const RequestOrUSVStringOrRequestOrUSVStringSequence& requests,
    ExceptionState& exception_state,
    bool* has_requests_with_body) {
  DCHECK(has_requests_with_body);

  Vector<WebServiceWorkerRequest> web_requests;
  *has_requests_with_body = false;

  if (requests.IsRequestOrUSVStringSequence()) {
    HeapVector<RequestOrUSVString> request_vector =
        requests.GetAsRequestOrUSVStringSequence();

    // Throw a TypeError when the developer has passed an empty sequence.
    if (!request_vector.size()) {
      exception_state.ThrowTypeError(kEmptyRequestSequenceErrorMessage);
      return Vector<WebServiceWorkerRequest>();
    }

    web_requests.resize(request_vector.size());

    for (wtf_size_t i = 0; i < request_vector.size(); ++i) {
      const RequestOrUSVString& request_or_url = request_vector[i];

      Request* request = nullptr;
      if (request_or_url.IsRequest()) {
        request = request_or_url.GetAsRequest();
      } else if (request_or_url.IsUSVString()) {
        request = Request::Create(script_state, request_or_url.GetAsUSVString(),
                                  exception_state);
        if (exception_state.HadException())
          return Vector<WebServiceWorkerRequest>();
      } else {
        exception_state.ThrowTypeError(kNullRequestErrorMessage);
        return Vector<WebServiceWorkerRequest>();
      }

      DCHECK(request);
      *has_requests_with_body |= request->HasBody();
      // TODO(crbug.com/774054): Set blob data handle when adding support for
      // requests with body.
      request->PopulateWebServiceWorkerRequest(web_requests[i]);
    }
  } else if (requests.IsRequest()) {
    auto* request = requests.GetAsRequest();
    DCHECK(request);

    // TODO(crbug.com/774054): Set blob data handle when adding support for
    // requests with body.

    *has_requests_with_body = request->HasBody();
    web_requests.resize(1);
    request->PopulateWebServiceWorkerRequest(web_requests[0]);
  } else if (requests.IsUSVString()) {
    Request* request = Request::Create(script_state, requests.GetAsUSVString(),
                                       exception_state);
    if (exception_state.HadException())
      return Vector<WebServiceWorkerRequest>();

    DCHECK(request);
    *has_requests_with_body = request->HasBody();
    web_requests.resize(1);
    request->PopulateWebServiceWorkerRequest(web_requests[0]);
  } else {
    exception_state.ThrowTypeError(kNullRequestErrorMessage);
    return Vector<WebServiceWorkerRequest>();
  }

  return web_requests;
}

void BackgroundFetchManager::DidGetRegistration(
    ScriptPromiseResolver* resolver,
    base::Time time_started,
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
      resolver->Reject(
          DOMException::Create(DOMExceptionCode::kAbortError,
                               "Failed to get registration due to I/O error."));
      return;
    case mojom::blink::BackgroundFetchError::SERVICE_WORKER_UNAVAILABLE:
      resolver->Reject(DOMException::Create(
          DOMExceptionCode::kInvalidStateError,
          "There's no service worker available to service the fetch."));
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

ScriptPromise BackgroundFetchManager::getIds(ScriptState* script_state) {
  // Creating a Background Fetch registration requires an activated worker, so
  // if |registration_| has not been activated we can skip the Mojo roundtrip.
  if (!registration_->active()) {
    return ScriptPromise::Cast(script_state,
                               v8::Array::New(script_state->GetIsolate()));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  bridge_->GetDeveloperIds(WTF::Bind(
      &BackgroundFetchManager::DidGetDeveloperIds, WrapPersistent(this),
      WrapPersistent(resolver), base::Time::Now()));

  return promise;
}

void BackgroundFetchManager::DidGetDeveloperIds(
    ScriptPromiseResolver* resolver,
    base::Time time_started,
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
      DCHECK(developer_ids.IsEmpty());
      resolver->Reject(DOMException::Create(
          DOMExceptionCode::kAbortError,
          "Failed to get registration IDs due to I/O error."));
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

void BackgroundFetchManager::Trace(blink::Visitor* visitor) {
  visitor->Trace(registration_);
  visitor->Trace(bridge_);
  visitor->Trace(loaders_);
  ContextLifecycleObserver::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

void BackgroundFetchManager::ContextDestroyed(ExecutionContext* context) {
  for (const auto& loader : loaders_) {
    if (loader)
      loader->Stop();
  }
  loaders_.clear();
}

}  // namespace blink
