// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/cookie_store/cookie_store.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

#include "net/base/features.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "net/cookies/cookie_util.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/mojom/restricted_cookie_manager.mojom-blink.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_cookie_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_cookie_list_item.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_cookie_store_delete_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_cookie_store_get_options.h"
#include "third_party/blink/renderer/core/core_probes_inl.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/cookie_store/cookie_change_event.h"
#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_registration.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

// Returns null if and only if an exception is thrown.
network::mojom::blink::CookieManagerGetOptionsPtr ToBackendOptions(
    const CookieStoreGetOptions* options,
    ExceptionState& exception_state) {
  auto backend_options = network::mojom::blink::CookieManagerGetOptions::New();

  // TODO(crbug.com/1124499): Cleanup matchType after evaluation.
  backend_options->match_type = network::mojom::blink::CookieMatchType::EQUALS;

  if (options->hasName()) {
    backend_options->name = options->name();
  } else {
    // No name provided. Use a filter that matches all cookies. This overrides
    // a user-provided matchType.
    backend_options->match_type =
        network::mojom::blink::CookieMatchType::STARTS_WITH;
    backend_options->name = g_empty_string;
  }

  return backend_options;
}

// Returns no value if and only if an exception is thrown.
network::mojom::blink::RestrictedCanonicalCookieParamsPtr ToCookieParams(
    const KURL& cookie_url,
    const CookieInit* options,
    ExceptionState& exception_state,
    ExecutionContext* execution_context) {
  const String& name = options->name();
  const String& value = options->value();
  if (name.empty() && value.contains('=')) {
    exception_state.ThrowTypeError(
        "Cookie value cannot contain '=' if the name is empty");
    return nullptr;
  }
  if (name.empty() && value.empty()) {
    exception_state.ThrowTypeError(
        "Cookie name and value both cannot be empty");
    return nullptr;
  }
  if (name.contains('=')) {
    exception_state.ThrowTypeError("Cookie name cannot contain '='");
    return nullptr;
  }

  base::Time expiry_time;
  if (base::FeatureList::IsEnabled(blink::features::kCookieStoreAPIMaxAge) &&
      options->hasMaxAge()) {
    UseCounter::Count(execution_context, WebFeature::kCookieStoreMaxAge);
    if (options->expires().has_value()) {
      // If both maxAge and expires are provided, throw an error.
      exception_state.ThrowTypeError(
          "Cookie expires and maxAge cannot both be specified");
      return nullptr;
    }
    const int64_t max_age = options->maxAge().value();
    // "If delta-seconds is less than or equal to zero (0), let expiry-
    // time be the earliest representable date and time. Otherwise, let the
    // expiry-time be the current date and time plus delta-seconds seconds."
    expiry_time = (max_age <= 0) ? base::Time().Min()
                                 : base::Time::Now() + base::Seconds(max_age);

  } else if (options->expires().has_value()) {
    expiry_time =
        base::Time::FromMillisecondsSinceUnixEpoch(options->expires().value());
  }

  String domain;
  // Trying to set `__http-` prefixed cookie will be rejected further down by
  // CreateSanitizedCookie regardless of the condition below. Its role is to
  // provide a more meaningful exception message than "Cookie was malformed..".
  const bool is_http_prefix = name.StartsWithIgnoringAsciiCase("__http-");
  const bool is_host_http_prefix =
      name.StartsWithIgnoringAsciiCase("__host-http-");
  if (is_http_prefix || is_host_http_prefix) {
    StringBuilder builder;
    UNSAFE_TODO(builder.AppendFormat(
        "Cookies with \"%s\" prefix cannot be set using the CookieStore API.",
        is_http_prefix ? "__Http-" : "__Host-Http-"));
    exception_state.ThrowTypeError(builder.ToString());
    return nullptr;
  }
  const bool is_host_prefixed_cookie =
      name.StartsWithIgnoringAsciiCase("__host-");
  if (!options->domain().IsNull()) {
    if (is_host_prefixed_cookie) {
      exception_state.ThrowTypeError(
          "Cookies with \"__Host-\" prefix cannot have a domain");
      return nullptr;
    }
    // The leading dot (".") from the domain attribute is stripped in the
    // Set-Cookie header, for compatibility. This API doesn't have compatibility
    // constraints, so reject the edge case outright.
    if (options->domain().starts_with('.')) {
      exception_state.ThrowTypeError("Cookie domain cannot start with \".\"");
      return nullptr;
    }

    domain = StrCat({".", options->domain()}).LowerASCII();
    net::CookieInclusionStatus status;
    if (!net::cookie_util::GetCookieDomainWithString(GURL(cookie_url),
                                                     domain.Utf8(), status)) {
      exception_state.ThrowTypeError(
          "Cookie domain must domain-match current host");
      return nullptr;
    }
  }

  // If `options` has a supplied `path`, and the `path` is empty, this implies
  // the caller intentionally set this option to be the empty string.
  // We log when this happens to see how common it is for scripts to do this.
  if (options->hasPath() && options->path().empty()) {
    UseCounter::Count(execution_context, WebFeature::kCookieStoreEmptyPath);
  }
  String path = options->path();
  if (!path.empty()) {
    if (is_host_prefixed_cookie && path != "/") {
      exception_state.ThrowTypeError(
          "Cookies with \"__Host-\" prefix cannot have a non-\"/\" path");
      return nullptr;
    }
    if (!path.starts_with('/')) {
      exception_state.ThrowTypeError("Cookie path must start with \"/\"");
      return nullptr;
    }
    if (!path.ends_with('/')) {
      path = StrCat({path, "/"});
    }
  }

  // The Cookie Store API will only write secure cookies but will read insecure
  // cookies. As a result,
  // cookieStore.get("name", "value") can get an insecure cookie, but when
  // modifying a retrieved insecure cookie via the Cookie Store API, it will
  // automatically turn it into a secure cookie without any warning.
  //
  // The Cookie Store API can only set secure cookies, so it is unusable on
  // insecure origins. file:// are excluded too for consistency with
  // document.cookie.
  if (!network::IsUrlPotentiallyTrustworthy(GURL(cookie_url)) ||
      std::ranges::contains(url::GetLocalSchemes(),
                            cookie_url.Protocol().Ascii())) {
    exception_state.ThrowTypeError(
        "Cannot modify a secure cookie on insecure origin");
    return nullptr;
  }

  network::mojom::blink::CookieSameSite same_site;
  switch (options->sameSite().AsEnum()) {
    case V8CookieSameSite::Enum::kStrict:
      same_site = network::mojom::blink::CookieSameSite::STRICT_MODE;
      break;
    case V8CookieSameSite::Enum::kLax:
      same_site = network::mojom::blink::CookieSameSite::LAX_MODE;
      break;
    case V8CookieSameSite::Enum::kNone:
      same_site = network::mojom::blink::CookieSameSite::NO_RESTRICTION;
      break;
  }

  return network::mojom::blink::RestrictedCanonicalCookieParams::New(
      name, value, domain.IsNull() ? "" : domain, path,
      base::Time() /*creation*/, expiry_time, base::Time() /*last_access*/,
      true /*secure*/, false /*http_only*/, same_site,
      network::mojom::blink::CookiePriority::MEDIUM,
      options->partitioned()
          ? network::mojom::blink::RestrictedCookiePartition::PARTITIONED
          : network::mojom::blink::RestrictedCookiePartition::UNPARTITIONED);
}

const KURL DefaultCookieURL(ExecutionContext* execution_context) {
  DCHECK(execution_context);

  if (auto* window = DynamicTo<LocalDOMWindow>(execution_context))
    return window->document()->CookieURL();

  return KURL(To<ServiceWorkerGlobalScope>(execution_context)
                  ->serviceWorker()
                  ->scriptURL());
}

// Return empty KURL if and only if an exception is thrown.
KURL CookieUrlForRead(const CookieStoreGetOptions* options,
                      const KURL& default_cookie_url,
                      ScriptState* script_state,
                      ExceptionState& exception_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);

  if (!options->hasUrl())
    return default_cookie_url;

  KURL cookie_url = KURL(default_cookie_url, options->url());
  cookie_url.RemoveFragmentIdentifier();

  if (auto* window = DynamicTo<LocalDOMWindow>(context)) {
    DCHECK_EQ(default_cookie_url, window->document()->CookieURL());

    if (cookie_url.GetString() != default_cookie_url.GetString()) {
      exception_state.ThrowTypeError("URL must match the document URL");
      return KURL();
    }
  } else {
    DCHECK(context->IsServiceWorkerGlobalScope());
    DCHECK_EQ(
        default_cookie_url.GetString(),
        To<ServiceWorkerGlobalScope>(context)->serviceWorker()->scriptURL());

    if (!cookie_url.GetString().starts_with(default_cookie_url.GetString())) {
      exception_state.ThrowTypeError("URL must be within Service Worker scope");
      return KURL();
    }
  }

  return cookie_url;
}

net::SiteForCookies DefaultSiteForCookies(ExecutionContext* execution_context) {
  DCHECK(execution_context);

  if (auto* window = DynamicTo<LocalDOMWindow>(execution_context))
    return window->document()->SiteForCookies();

  auto* scope = To<ServiceWorkerGlobalScope>(execution_context);
  const blink::BlinkStorageKey& key = scope->storage_key();
  if (key.IsFirstPartyContext()) {
    return net::SiteForCookies::FromUrl(GURL(scope->Url()));
  }
  return net::SiteForCookies();
}

const scoped_refptr<const SecurityOrigin> DefaultTopFrameOrigin(
    ExecutionContext* execution_context) {
  DCHECK(execution_context);

  if (auto* window = DynamicTo<LocalDOMWindow>(execution_context)) {
    // Can we avoid the copy? TopFrameOrigin is returned as const& but we need
    // a scoped_refptr.
    return window->document()->TopFrameOrigin()->IsolatedCopy();
  }

  const BlinkStorageKey& key =
      To<ServiceWorkerGlobalScope>(execution_context)->storage_key();
  if (key.IsFirstPartyContext()) {
    return key.GetSecurityOrigin();
  }
  return SecurityOrigin::CreateFromUrlOrigin(
      url::Origin::Create(net::SchemefulSite(key.GetTopLevelSite()).GetURL()));
}

bool IsAdTagged(ExecutionContext* context) {
  if (auto* window = DynamicTo<LocalDOMWindow>(context)) {
    if (auto* local_frame = window->GetFrame()) {
      return local_frame->IsAdFrame();
    }
  }
  return false;
}

}  // namespace

CookieStore::CookieStore(
    ExecutionContext* execution_context,
    HeapMojoRemote<network::mojom::blink::RestrictedCookieManager> backend)
    : ExecutionContextClient(execution_context),
      backend_(std::move(backend)),
      change_listener_receiver_(this, execution_context),
      default_cookie_url_(DefaultCookieURL(execution_context)),
      default_site_for_cookies_(DefaultSiteForCookies(execution_context)),
      default_top_frame_origin_(DefaultTopFrameOrigin(execution_context)) {
  DCHECK(backend_);
}

CookieStore::~CookieStore() = default;

ScriptPromise<IDLSequence<CookieListItem>> CookieStore::getAll(
    ScriptState* script_state,
    const String& name,
    ExceptionState& exception_state) {
  CookieStoreGetOptions* options = CookieStoreGetOptions::Create();
  options->setName(name);
  return getAll(script_state, options, exception_state);
}

ScriptPromise<IDLSequence<CookieListItem>> CookieStore::getAll(
    ScriptState* script_state,
    const CookieStoreGetOptions* options,
    ExceptionState& exception_state) {
  UseCounter::Count(CurrentExecutionContext(script_state->GetIsolate()),
                    WebFeature::kCookieStoreAPI);

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLSequence<CookieListItem>>>(
          script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  DoRead(script_state, options,
         BindOnce(&CookieStore::GetAllForUrlToGetAllResult,
                  WrapPersistent(resolver)),
         exception_state);
  if (exception_state.HadException()) {
    resolver->Detach();
    return EmptyPromise();
  }
  return promise;
}

ScriptPromise<IDLNullable<CookieListItem>> CookieStore::get(
    ScriptState* script_state,
    const String& name,
    ExceptionState& exception_state) {
  CookieStoreGetOptions* options = CookieStoreGetOptions::Create();
  options->setName(name);
  return get(script_state, options, exception_state);
}

ScriptPromise<IDLNullable<CookieListItem>> CookieStore::get(
    ScriptState* script_state,
    const CookieStoreGetOptions* options,
    ExceptionState& exception_state) {
  UseCounter::Count(CurrentExecutionContext(script_state->GetIsolate()),
                    WebFeature::kCookieStoreAPI);

  if (!options->hasName() && !options->hasUrl()) {
    exception_state.ThrowTypeError("CookieStoreGetOptions must not be empty");
    return ScriptPromise<IDLNullable<CookieListItem>>();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLNullable<CookieListItem>>>(
          script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  DoRead(
      script_state, options,
      BindOnce(&CookieStore::GetAllForUrlToGetResult, WrapPersistent(resolver)),
      exception_state);
  if (exception_state.HadException()) {
    resolver->Detach();
    return EmptyPromise();
  }
  return promise;
}

ScriptPromise<IDLUndefined> CookieStore::set(ScriptState* script_state,
                                             const String& name,
                                             const String& value,
                                             ExceptionState& exception_state) {
  CookieInit* set_options = CookieInit::Create();
  set_options->setName(name);
  set_options->setValue(value);
  return set(script_state, set_options, exception_state);
}

ScriptPromise<IDLUndefined> CookieStore::set(ScriptState* script_state,
                                             const CookieInit* options,
                                             ExceptionState& exception_state) {
  UseCounter::Count(CurrentExecutionContext(script_state->GetIsolate()),
                    WebFeature::kCookieStoreAPI);

  return DoWrite(script_state, options, exception_state);
}

ScriptPromise<IDLUndefined> CookieStore::Delete(
    ScriptState* script_state,
    const String& name,
    ExceptionState& exception_state) {
  UseCounter::Count(CurrentExecutionContext(script_state->GetIsolate()),
                    WebFeature::kCookieStoreAPI);

  CookieInit* set_options = CookieInit::Create();
  set_options->setName(name);
  set_options->setValue(name.empty() ? "deleted" : "");
  set_options->setExpires(0);
  return DoWrite(script_state, set_options, exception_state);
}

ScriptPromise<IDLUndefined> CookieStore::Delete(
    ScriptState* script_state,
    const CookieStoreDeleteOptions* options,
    ExceptionState& exception_state) {
  CookieInit* set_options = CookieInit::Create();
  set_options->setName(options->name());
  set_options->setValue(options->name().empty() ? "deleted" : "");
  set_options->setExpires(0);
  set_options->setDomain(options->domain());
  set_options->setPath(options->path());
  set_options->setSameSite(V8CookieSameSite::Enum::kStrict);
  set_options->setPartitioned(options->partitioned());
  return DoWrite(script_state, set_options, exception_state);
}

void CookieStore::Trace(Visitor* visitor) const {
  visitor->Trace(change_listener_receiver_);
  visitor->Trace(backend_);
  EventTarget::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

const AtomicString& CookieStore::InterfaceName() const {
  return event_target_names::kCookieStore;
}

ExecutionContext* CookieStore::GetExecutionContext() const {
  return ExecutionContextClient::GetExecutionContext();
}

void CookieStore::RemoveAllEventListeners() {
  EventTarget::RemoveAllEventListeners();
  DCHECK(!HasEventListeners());
  StopObserving();
}

void CookieStore::OnCookieChange(
    network::mojom::blink::CookieChangeInfoPtr change) {
  HeapVector<Member<CookieListItem>> changed, deleted;
  CookieChangeEvent::ToEventInfo(change, changed, deleted);
  if (changed.empty() && deleted.empty()) {
    // The backend only reported OVERWRITE events, which are dropped.
    return;
  }
  DispatchEvent(*CookieChangeEvent::Create(
      event_type_names::kChange, std::move(changed), std::move(deleted)));
}

void CookieStore::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  EventTarget::AddedEventListener(event_type, registered_listener);
  StartObserving();
}

void CookieStore::RemovedEventListener(
    const AtomicString& event_type,
    const RegisteredEventListener& registered_listener) {
  EventTarget::RemovedEventListener(event_type, registered_listener);
  if (!HasEventListeners())
    StopObserving();
}

void CookieStore::DoRead(ScriptState* script_state,
                         const CookieStoreGetOptions* options,
                         GetAllForUrlCallback backend_result_converter,
                         ExceptionState& exception_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  if (!context->GetSecurityOrigin()->CanAccessCookies()) {
    exception_state.ThrowSecurityError(
        "Access to the CookieStore API is denied in this context.");
    return;
  }

  network::mojom::blink::CookieManagerGetOptionsPtr backend_options =
      ToBackendOptions(options, exception_state);
  KURL cookie_url = CookieUrlForRead(options, default_cookie_url_, script_state,
                                     exception_state);
  if (backend_options.is_null() || cookie_url.IsNull()) {
    DCHECK(exception_state.HadException());
    return;
  }

  if (!backend_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "CookieStore backend went away");
    return;
  }

  bool is_ad_tagged = IsAdTagged(context);
  bool should_apply_devtools_overrides = false;
  probe::ShouldApplyDevtoolsCookieSettingOverrides(
      GetExecutionContext(), &should_apply_devtools_overrides);

  backend_->GetAllForUrl(
      cookie_url, default_site_for_cookies_, default_top_frame_origin_,
      context->GetStorageAccessApiStatus(), std::move(backend_options),
      is_ad_tagged, should_apply_devtools_overrides,
      /*force_disable_third_party_cookies=*/false,
      std::move(backend_result_converter));
}

// static
void CookieStore::GetAllForUrlToGetAllResult(
    ScriptPromiseResolver<IDLSequence<CookieListItem>>* resolver,
    const Vector<network::mojom::blink::CookieWithAccessResultPtr>
        backend_cookies) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;
  ScriptState::Scope scope(script_state);

  HeapVector<Member<CookieListItem>> cookies;
  cookies.ReserveInitialCapacity(backend_cookies.size());
  for (const auto& backend_cookie : backend_cookies) {
    cookies.push_back(CookieChangeEvent::ToCookieListItem(
        backend_cookie->cookie,
        backend_cookie->access_result->effective_same_site,
        false /* is_deleted */));
  }

  resolver->Resolve(std::move(cookies));
}

// static
void CookieStore::GetAllForUrlToGetResult(
    ScriptPromiseResolver<IDLNullable<CookieListItem>>* resolver,
    const Vector<network::mojom::blink::CookieWithAccessResultPtr>
        backend_cookies) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;
  ScriptState::Scope scope(script_state);

  if (backend_cookies.empty()) {
    resolver->Resolve(nullptr);
    return;
  }

  const auto& backend_cookie = backend_cookies.front();
  CookieListItem* cookie = CookieChangeEvent::ToCookieListItem(
      backend_cookie->cookie,
      backend_cookie->access_result->effective_same_site,
      false /* is_deleted */);
  resolver->Resolve(cookie);
}

ScriptPromise<IDLUndefined> CookieStore::DoWrite(
    ScriptState* script_state,
    const CookieInit* options,
    ExceptionState& exception_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  if (!context->GetSecurityOrigin()->CanAccessCookies()) {
    exception_state.ThrowSecurityError(
        "Access to the CookieStore API is denied in this context.");
    return EmptyPromise();
  }

  network::mojom::blink::RestrictedCanonicalCookieParamsPtr cookie_params =
      ToCookieParams(default_cookie_url_, options, exception_state, context);

  if (!cookie_params) {
    DCHECK(exception_state.HadException());
    return EmptyPromise();
  }

  if (!backend_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "CookieStore backend went away");
    return EmptyPromise();
  }

  bool is_ad_tagged = IsAdTagged(context);
  bool should_apply_devtools_overrides = false;
  probe::ShouldApplyDevtoolsCookieSettingOverrides(
      GetExecutionContext(), &should_apply_devtools_overrides);

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  backend_->SetCanonicalCookie(
      std::move(cookie_params), default_cookie_url_, default_site_for_cookies_,
      default_top_frame_origin_, context->GetStorageAccessApiStatus(),
      is_ad_tagged, should_apply_devtools_overrides,
      BindOnce(&CookieStore::OnSetCanonicalCookieResult,
               WrapPersistent(resolver)));
  return resolver->Promise();
}

// static
void CookieStore::OnSetCanonicalCookieResult(
    ScriptPromiseResolver<IDLUndefined>* resolver,
    bool backend_success) {
  if (!backend_success) {
    resolver->RejectWithTypeError(
        "Cookie was malformed and could not be stored, due to problem(s) while "
        "parsing.");
    return;
  }
  resolver->Resolve();
}

void CookieStore::StartObserving() {
  auto* execution_context = GetExecutionContext();

  if (change_listener_receiver_.is_bound() || !backend_ ||
      /* If we don't have permission to access cookies, don't ask
         RestrictedCookieManager, since that would make it upset to us. */
      !execution_context->GetSecurityOrigin()->CanAccessCookies()) {
    return;
  }

  // See https://bit.ly/2S0zRAS for task types.
  auto task_runner =
      execution_context->GetTaskRunner(TaskType::kDOMManipulation);
  backend_->AddChangeListener(
      default_cookie_url_, default_site_for_cookies_, default_top_frame_origin_,
      execution_context->GetStorageAccessApiStatus(),
      change_listener_receiver_.BindNewPipeAndPassRemote(task_runner), {});
}

void CookieStore::StopObserving() {
  if (!change_listener_receiver_.is_bound())
    return;
  change_listener_receiver_.reset();
}

}  // namespace blink
