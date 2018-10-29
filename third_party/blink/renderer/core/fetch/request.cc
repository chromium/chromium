// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/request.h"

#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_request.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_abort_signal.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_array_buffer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_array_buffer_view.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_blob.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_form_data.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_url_search_params.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/blob_bytes_consumer.h"
#include "third_party/blink/renderer/core/fetch/body_stream_buffer.h"
#include "third_party/blink/renderer/core/fetch/fetch_manager.h"
#include "third_party/blink/renderer/core/fetch/form_data_bytes_consumer.h"
#include "third_party/blink/renderer/core/fetch/request_init.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/fileapi/public_url_manager.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/loader/threadable_loader.h"
#include "third_party/blink/renderer/core/url/url_search_params.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/loader/cors/cors.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_utils.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/origin_access_entry.h"
#include "third_party/blink/renderer/platform/weborigin/referrer.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"

namespace blink {

FetchRequestData* CreateCopyOfFetchRequestDataForFetch(
    ScriptState* script_state,
    const FetchRequestData* original) {
  FetchRequestData* request = FetchRequestData::Create();
  request->SetURL(original->Url());
  request->SetMethod(original->Method());
  request->SetHeaderList(original->HeaderList()->Clone());
  // FIXME: Set client.
  DOMWrapperWorld& world = script_state->World();
  if (world.IsIsolatedWorld()) {
    request->SetOrigin(world.IsolatedWorldSecurityOrigin());
  } else {
    request->SetOrigin(
        ExecutionContext::From(script_state)->GetSecurityOrigin());
  }
  // FIXME: Set ForceOriginHeaderFlag.
  request->SetSameOriginDataURLFlag(true);
  request->SetReferrerString(original->ReferrerString());
  request->SetReferrerPolicy(original->GetReferrerPolicy());
  request->SetMode(original->Mode());
  request->SetCredentials(original->Credentials());
  request->SetCacheMode(original->CacheMode());
  request->SetRedirect(original->Redirect());
  request->SetIntegrity(original->Integrity());
  request->SetImportance(original->Importance());
  request->SetPriority(original->Priority());
  request->SetKeepalive(original->Keepalive());
  request->SetIsHistoryNavigation(original->IsHistoryNavigation());
  if (original->URLLoaderFactory()) {
    network::mojom::blink::URLLoaderFactoryPtr factory_clone;
    original->URLLoaderFactory()->Clone(MakeRequest(&factory_clone));
    request->SetURLLoaderFactory(std::move(factory_clone));
  }
  return request;
}

static bool AreAnyMembersPresent(const RequestInit& init) {
  return init.hasMethod() || init.hasHeaders() || init.hasBody() ||
         init.hasReferrer() || init.hasReferrerPolicy() || init.hasMode() ||
         init.hasCredentials() || init.hasCache() || init.hasRedirect() ||
         init.hasIntegrity() || init.hasKeepalive() || init.hasImportance() ||
         init.hasSignal();
}

static BodyStreamBuffer* ExtractBody(ScriptState* script_state,
                                     ExceptionState& exception_state,
                                     v8::Local<v8::Value> body,
                                     String& content_type) {
  DCHECK(!body->IsNull());
  BodyStreamBuffer* return_buffer = nullptr;

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  v8::Isolate* isolate = script_state->GetIsolate();

  if (V8Blob::hasInstance(body, isolate)) {
    Blob* blob = V8Blob::ToImpl(body.As<v8::Object>());
    return_buffer = new BodyStreamBuffer(
        script_state,
        new BlobBytesConsumer(execution_context, blob->GetBlobDataHandle()),
        nullptr /* AbortSignal */);
    content_type = blob->type();
  } else if (body->IsArrayBuffer()) {
    // Avoid calling into V8 from the following constructor parameters, which
    // is potentially unsafe.
    DOMArrayBuffer* array_buffer = V8ArrayBuffer::ToImpl(body.As<v8::Object>());
    return_buffer = new BodyStreamBuffer(
        script_state, new FormDataBytesConsumer(array_buffer),
        nullptr /* AbortSignal */);
  } else if (body->IsArrayBufferView()) {
    // Avoid calling into V8 from the following constructor parameters, which
    // is potentially unsafe.
    DOMArrayBufferView* array_buffer_view =
        V8ArrayBufferView::ToImpl(body.As<v8::Object>());
    return_buffer = new BodyStreamBuffer(
        script_state, new FormDataBytesConsumer(array_buffer_view),
        nullptr /* AbortSignal */);
  } else if (V8FormData::hasInstance(body, isolate)) {
    scoped_refptr<EncodedFormData> form_data =
        V8FormData::ToImpl(body.As<v8::Object>())->EncodeMultiPartFormData();
    // Here we handle formData->boundary() as a C-style string. See
    // FormDataEncoder::generateUniqueBoundaryString.
    content_type = AtomicString("multipart/form-data; boundary=") +
                   form_data->Boundary().data();
    return_buffer = new BodyStreamBuffer(
        script_state,
        new FormDataBytesConsumer(execution_context, std::move(form_data)),
        nullptr /* AbortSignal */);
  } else if (V8URLSearchParams::hasInstance(body, isolate)) {
    scoped_refptr<EncodedFormData> form_data =
        V8URLSearchParams::ToImpl(body.As<v8::Object>())->ToEncodedFormData();
    return_buffer = new BodyStreamBuffer(
        script_state,
        new FormDataBytesConsumer(execution_context, std::move(form_data)),
        nullptr /* AbortSignal */);
    content_type = "application/x-www-form-urlencoded;charset=UTF-8";
  } else {
    String string = NativeValueTraits<IDLUSVString>::NativeValue(
        isolate, body, exception_state);
    if (exception_state.HadException())
      return nullptr;

    return_buffer =
        new BodyStreamBuffer(script_state, new FormDataBytesConsumer(string),
                             nullptr /* AbortSignal */);
    content_type = "text/plain;charset=UTF-8";
  }

  return return_buffer;
}

Request* Request::CreateRequestWithRequestOrString(
    ScriptState* script_state,
    Request* input_request,
    const String& input_string,
    const RequestInit& init,
    ExceptionState& exception_state) {
  // Setup RequestInit's body first
  // - "If |input| is a Request object and it is disturbed, throw a
  //   TypeError."
  if (input_request &&
      input_request->IsBodyUsed(exception_state) == BodyUsed::kUsed) {
    DCHECK(!exception_state.HadException());
    exception_state.ThrowTypeError(
        "Cannot construct a Request with a Request object that has already "
        "been used.");
    return nullptr;
  }
  if (exception_state.HadException())
    return nullptr;
  // - "Let |temporaryBody| be |input|'s request's body if |input| is a
  //   Request object, and null otherwise."
  BodyStreamBuffer* temporary_body =
      input_request ? input_request->BodyBuffer() : nullptr;

  // "Let |request| be |input|'s request, if |input| is a Request object,
  // and a new request otherwise."

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  scoped_refptr<const SecurityOrigin> origin =
      execution_context->GetSecurityOrigin();

  // "Let |signal| be null."
  AbortSignal* signal = nullptr;

  // TODO(yhirano): Implement the following steps:
  // - "Let |window| be client."
  // - "If |request|'s window is an environment settings object and its
  //   origin is same origin with entry settings object's origin, set
  //   |window| to |request|'s window."
  // - "If |init|'s window member is present and it is not null, throw a
  //   TypeError."
  // - "If |init|'s window member is present, set |window| to no-window."
  //
  // "Set |request| to a new request whose url is |request|'s current url,
  // method is |request|'s method, header list is a copy of |request|'s
  // header list, unsafe-request flag is set, client is entry settings object,
  // window is |window|, origin is "client", omit-Origin-header flag is
  // |request|'s omit-Origin-header flag, same-origin data-URL flag is set,
  // referrer is |request|'s referrer, referrer policy is |request|'s
  // referrer policy, destination is the empty string, mode is |request|'s
  // mode, credentials mode is |request|'s credentials mode, cache mode is
  // |request|'s cache mode, redirect mode is |request|'s redirect mode, and
  // integrity metadata is |request|'s integrity metadata."
  FetchRequestData* request = CreateCopyOfFetchRequestDataForFetch(
      script_state,
      input_request ? input_request->GetRequest() : FetchRequestData::Create());

  if (input_request) {
    // "Set |signal| to input’s signal."
    signal = input_request->signal_;
  }

  // We don't use fallback values. We set these flags directly in below.
  // - "Let |fallbackMode| be null."
  // - "Let |fallbackCredentials| be null."

  // "Let |baseURL| be entry settings object's API base URL."
  const KURL base_url = execution_context->BaseURL();

  // "If |input| is a string, run these substeps:"
  if (!input_request) {
    // "Let |parsedURL| be the result of parsing |input| with |baseURL|."
    KURL parsed_url = KURL(base_url, input_string);
    // "If |parsedURL| is failure, throw a TypeError."
    if (!parsed_url.IsValid()) {
      exception_state.ThrowTypeError("Failed to parse URL from " +
                                     input_string);
      return nullptr;
    }
    //   "If |parsedURL| includes credentials, throw a TypeError."
    if (!parsed_url.User().IsEmpty() || !parsed_url.Pass().IsEmpty()) {
      exception_state.ThrowTypeError(
          "Request cannot be constructed from a URL that includes "
          "credentials: " +
          input_string);
      return nullptr;
    }
    // "Set |request|'s url to |parsedURL| and replace |request|'s url list
    // single URL with a copy of |parsedURL|."
    request->SetURL(parsed_url);

    // Parsing URLs should also resolve blob URLs. This is important because
    // fetching of a blob URL should work even after the URL is revoked as long
    // as the request was created while the URL was still valid.
    if (parsed_url.ProtocolIs("blob") && BlobUtils::MojoBlobURLsEnabled()) {
      network::mojom::blink::URLLoaderFactoryPtr url_loader_factory;
      ExecutionContext::From(script_state)
          ->GetPublicURLManager()
          .Resolve(parsed_url, MakeRequest(&url_loader_factory));
      request->SetURLLoaderFactory(std::move(url_loader_factory));
    }

    // We don't use fallback values. We set these flags directly in below.
    // - "Set |fallbackMode| to "cors"."
    // - "Set |fallbackCredentials| to "omit"."
  }

  // "If any of |init|'s members are present, then:"
  if (AreAnyMembersPresent(init)) {
    // "If |request|'s |mode| is "navigate", then set it to "same-origin".
    if (request->Mode() == network::mojom::FetchRequestMode::kNavigate)
      request->SetMode(network::mojom::FetchRequestMode::kSameOrigin);

    // TODO(yhirano): Implement the following substep:
    // "Unset |request|'s reload-navigation flag."

    // "Unset |request|'s history-navigation flag."
    request->SetIsHistoryNavigation(false);

    // "Set |request|’s referrer to "client"."
    request->SetReferrerString(AtomicString(Referrer::ClientReferrerString()));

    // "Set |request|’s referrer policy to the empty string."
    request->SetReferrerPolicy(kReferrerPolicyDefault);
  }

  // "If init’s referrer member is present, then:"
  if (init.hasReferrer()) {
    // Nothing to do for the step "Let |referrer| be |init|'s referrer
    // member."

    if (init.referrer().IsEmpty()) {
      // "If |referrer| is the empty string, set |request|'s referrer to
      // "no-referrer" and terminate these substeps."
      request->SetReferrerString(AtomicString(Referrer::NoReferrer()));
    } else {
      // "Let |parsedReferrer| be the result of parsing |referrer| with
      // |baseURL|."
      KURL parsed_referrer(base_url, init.referrer());
      if (!parsed_referrer.IsValid()) {
        // "If |parsedReferrer| is failure, throw a TypeError."
        exception_state.ThrowTypeError("Referrer '" + init.referrer() +
                                       "' is not a valid URL.");
        return nullptr;
      }
      if ((parsed_referrer.ProtocolIsAbout() &&
           parsed_referrer.Host().IsEmpty() &&
           parsed_referrer.GetPath() == "client") ||
          !origin->IsSameSchemeHostPort(
              SecurityOrigin::Create(parsed_referrer).get())) {
        // If |parsedReferrer|'s host is empty
        // it's cannot-be-a-base-URL flag must be set

        // "If one of the following conditions is true, then set
        // request’s referrer to "client":
        //
        //     |parsedReferrer|’s cannot-be-a-base-URL flag is set,
        //     scheme is "about", and path contains a single string "client".
        //
        //     parsedReferrer’s origin is not same origin with origin"
        //
        request->SetReferrerString(
            AtomicString(Referrer::ClientReferrerString()));
      } else {
        // "Set |request|'s referrer to |parsedReferrer|."
        request->SetReferrerString(AtomicString(parsed_referrer.GetString()));
      }
    }
  }

  // "If init's referrerPolicy member is present, set request's referrer
  // policy to it."
  if (init.hasReferrerPolicy()) {
    // In case referrerPolicy = "", the SecurityPolicy method below will not
    // actually set referrer_policy, so we'll default to
    // kReferrerPolicyDefault.
    ReferrerPolicy referrer_policy;
    if (!SecurityPolicy::ReferrerPolicyFromString(
            init.referrerPolicy(), kDoNotSupportReferrerPolicyLegacyKeywords,
            &referrer_policy)) {
      DCHECK(init.referrerPolicy().IsEmpty());
      referrer_policy = kReferrerPolicyDefault;
    }

    request->SetReferrerPolicy(referrer_policy);
  }

  // The following code performs the following steps:
  // - "Let |mode| be |init|'s mode member if it is present, and
  //   |fallbackMode| otherwise."
  // - "If |mode| is "navigate", throw a TypeError."
  // - "If |mode| is non-null, set |request|'s mode to |mode|."
  if (init.mode() == "navigate") {
    exception_state.ThrowTypeError(
        "Cannot construct a Request with a RequestInit whose mode member is "
        "set as 'navigate'.");
    return nullptr;
  }
  if (init.mode() == "same-origin") {
    request->SetMode(network::mojom::FetchRequestMode::kSameOrigin);
  } else if (init.mode() == "no-cors") {
    request->SetMode(network::mojom::FetchRequestMode::kNoCORS);
  } else if (init.mode() == "cors") {
    request->SetMode(network::mojom::FetchRequestMode::kCORS);
  } else {
    // |inputRequest| is directly checked here instead of setting and
    // checking |fallbackMode| as specified in the spec.
    if (!input_request)
      request->SetMode(network::mojom::FetchRequestMode::kCORS);
  }

  // This is not yet standardized, but we can assume the following:
  // "If |init|'s importance member is present, set |request|'s importance
  // mode to it." For more information see Priority Hints at
  // https://crbug.com/821464
  DCHECK(init.importance().IsNull() ||
         RuntimeEnabledFeatures::PriorityHintsEnabled());
  if (init.importance() == "low") {
    request->SetImportance(mojom::FetchImportanceMode::kImportanceLow);
  } else if (init.importance() == "high") {
    request->SetImportance(mojom::FetchImportanceMode::kImportanceHigh);
  }

  // "Let |credentials| be |init|'s credentials member if it is present, and
  // |fallbackCredentials| otherwise."
  // "If |credentials| is non-null, set |request|'s credentials mode to
  // |credentials|."

  network::mojom::FetchCredentialsMode credentials_mode;
  if (ParseCredentialsMode(init.credentials(), &credentials_mode)) {
    request->SetCredentials(credentials_mode);
  } else if (!input_request) {
    request->SetCredentials(network::mojom::FetchCredentialsMode::kSameOrigin);
  }

  // "If |init|'s cache member is present, set |request|'s cache mode to it."
  if (init.cache() == "default") {
    request->SetCacheMode(mojom::FetchCacheMode::kDefault);
  } else if (init.cache() == "no-store") {
    request->SetCacheMode(mojom::FetchCacheMode::kNoStore);
  } else if (init.cache() == "reload") {
    request->SetCacheMode(mojom::FetchCacheMode::kBypassCache);
  } else if (init.cache() == "no-cache") {
    request->SetCacheMode(mojom::FetchCacheMode::kValidateCache);
  } else if (init.cache() == "force-cache") {
    request->SetCacheMode(mojom::FetchCacheMode::kForceCache);
  } else if (init.cache() == "only-if-cached") {
    request->SetCacheMode(mojom::FetchCacheMode::kOnlyIfCached);
  }

  // If |request|’s cache mode is "only-if-cached" and |request|’s mode is not
  // "same-origin", then throw a TypeError.
  if (request->CacheMode() == mojom::FetchCacheMode::kOnlyIfCached &&
      request->Mode() != network::mojom::FetchRequestMode::kSameOrigin) {
    exception_state.ThrowTypeError(
        "'only-if-cached' can be set only with 'same-origin' mode");
    return nullptr;
  }

  // "If |init|'s redirect member is present, set |request|'s redirect mode
  // to it."
  if (init.redirect() == "follow") {
    request->SetRedirect(network::mojom::FetchRedirectMode::kFollow);
  } else if (init.redirect() == "error") {
    request->SetRedirect(network::mojom::FetchRedirectMode::kError);
  } else if (init.redirect() == "manual") {
    request->SetRedirect(network::mojom::FetchRedirectMode::kManual);
  }

  // "If |init|'s integrity member is present, set |request|'s
  // integrity metadata to it."
  if (init.hasIntegrity())
    request->SetIntegrity(init.integrity());

  if (init.hasKeepalive())
    request->SetKeepalive(init.keepalive());

  // "If |init|'s method member is present, let |method| be it and run these
  // substeps:"
  if (init.hasMethod()) {
    // "If |method| is not a method or method is a forbidden method, throw
    // a TypeError."
    if (!IsValidHTTPToken(init.method())) {
      exception_state.ThrowTypeError("'" + init.method() +
                                     "' is not a valid HTTP method.");
      return nullptr;
    }
    if (FetchUtils::IsForbiddenMethod(init.method())) {
      exception_state.ThrowTypeError("'" + init.method() +
                                     "' HTTP method is unsupported.");
      return nullptr;
    }
    // "Normalize |method|."
    // "Set |request|'s method to |method|."
    request->SetMethod(
        FetchUtils::NormalizeMethod(AtomicString(init.method())));
  }

  // "If |init|'s signal member is present, then set |signal| to it."
  if (init.hasSignal()) {
    signal = init.signal();
  }

  // "Let |r| be a new Request object associated with |request| and a new
  // Headers object whose guard is "request"."
  Request* r = Request::Create(script_state, request);
  // Perform the following steps:
  // - "Let |headers| be a copy of |r|'s Headers object."
  // - "If |init|'s headers member is present, set |headers| to |init|'s
  //   headers member."
  //
  // We don't create a copy of r's Headers object when init's headers member
  // is present.
  Headers* headers = nullptr;
  if (!init.hasHeaders()) {
    headers = r->getHeaders()->Clone();
  }
  // "Empty |r|'s request's header list."
  r->request_->HeaderList()->ClearList();
  // "If |r|'s request's mode is "no-cors", run these substeps:
  if (r->GetRequest()->Mode() == network::mojom::FetchRequestMode::kNoCORS) {
    // "If |r|'s request's method is not a CORS-safelisted method, throw a
    // TypeError."
    if (!CORS::IsCORSSafelistedMethod(r->GetRequest()->Method())) {
      exception_state.ThrowTypeError("'" + r->GetRequest()->Method() +
                                     "' is unsupported in no-cors mode.");
      return nullptr;
    }
    // "Set |r|'s Headers object's guard to "request-no-cors"."
    r->getHeaders()->SetGuard(Headers::kRequestNoCORSGuard);
  }
  // "If |signal| is not null, then make |r|’s signal follow |signal|."
  if (signal)
    r->signal_->Follow(signal);

  // "Fill |r|'s Headers object with |headers|. Rethrow any exceptions."
  if (init.hasHeaders()) {
    r->getHeaders()->FillWith(init.headers(), exception_state);
  } else {
    DCHECK(headers);
    r->getHeaders()->FillWith(headers, exception_state);
  }
  if (exception_state.HadException())
    return nullptr;

  // "If either |init|'s body member is present or |temporaryBody| is
  // non-null, and |request|'s method is `GET` or `HEAD`, throw a TypeError.
  v8::Local<v8::Value> init_body =
      init.hasBody() ? init.body().V8Value() : v8::Local<v8::Value>();
  if ((!init_body.IsEmpty() && !init_body->IsNull()) || temporary_body) {
    if (request->Method() == HTTPNames::GET ||
        request->Method() == HTTPNames::HEAD) {
      exception_state.ThrowTypeError(
          "Request with GET/HEAD method cannot have body.");
      return nullptr;
    }
  }

  // "If |init|’s body member is present and is non-null, then:"
  if (!init_body.IsEmpty() && !init_body->IsNull()) {
    // TODO(yhirano): Throw if keepalive flag is set and body is a
    // ReadableStream. We don't support body stream setting for Request yet.

    // Perform the following steps:
    // - "Let |stream| and |Content-Type| be the result of extracting
    //   |init|'s body member."
    // - "Set |temporaryBody| to |stream|.
    // - "If |Content-Type| is non-null and |r|'s request's header list
    //   contains no header named `Content-Type`, append
    //   `Content-Type`/|Content-Type| to |r|'s Headers object. Rethrow any
    //   exception."
    String content_type;
    temporary_body =
        ExtractBody(script_state, exception_state, init_body, content_type);
    if (!content_type.IsEmpty() &&
        !r->getHeaders()->has(HTTPNames::Content_Type, exception_state)) {
      r->getHeaders()->append(HTTPNames::Content_Type, content_type,
                              exception_state);
    }
    if (exception_state.HadException())
      return nullptr;
  }

  // "Set |r|'s request's body to |temporaryBody|.
  if (temporary_body)
    r->request_->SetBuffer(temporary_body);

  // "Set |r|'s MIME type to the result of extracting a MIME type from |r|'s
  // request's header list."
  r->request_->SetMIMEType(r->request_->HeaderList()->ExtractMIMEType());

  // "If |input| is a Request object and |input|'s request's body is
  // non-null, run these substeps:"
  if (input_request && input_request->BodyBuffer()) {
    // "Let |dummyStream| be an empty ReadableStream object."
    auto* dummy_stream = new BodyStreamBuffer(
        script_state, BytesConsumer::CreateClosed(), nullptr);
    // "Set |input|'s request's body to a new body whose stream is
    // |dummyStream|."
    input_request->request_->SetBuffer(dummy_stream);
    // "Let |reader| be the result of getting reader from |dummyStream|."
    // "Read all bytes from |dummyStream| with |reader|."
    input_request->BodyBuffer()->CloseAndLockAndDisturb(exception_state);
    if (exception_state.HadException())
      return nullptr;
  }

  // "Return |r|."
  return r;
}

Request* Request::Create(ScriptState* script_state,
                         const RequestInfo& input,
                         const RequestInit& init,
                         ExceptionState& exception_state) {
  DCHECK(!input.IsNull());
  if (input.IsUSVString())
    return Create(script_state, input.GetAsUSVString(), init, exception_state);
  return Create(script_state, input.GetAsRequest(), init, exception_state);
}

Request* Request::Create(ScriptState* script_state,
                         const String& input,
                         ExceptionState& exception_state) {
  return Create(script_state, input, RequestInit(), exception_state);
}

Request* Request::Create(ScriptState* script_state,
                         const String& input,
                         const RequestInit& init,
                         ExceptionState& exception_state) {
  return CreateRequestWithRequestOrString(script_state, nullptr, input, init,
                                          exception_state);
}

Request* Request::Create(ScriptState* script_state,
                         Request* input,
                         ExceptionState& exception_state) {
  return Create(script_state, input, RequestInit(), exception_state);
}

Request* Request::Create(ScriptState* script_state,
                         Request* input,
                         const RequestInit& init,
                         ExceptionState& exception_state) {
  return CreateRequestWithRequestOrString(script_state, input, String(), init,
                                          exception_state);
}

Request* Request::Create(ScriptState* script_state, FetchRequestData* request) {
  return new Request(script_state, request);
}

Request* Request::Create(ScriptState* script_state,
                         const WebServiceWorkerRequest& web_request) {
  FetchRequestData* request =
      FetchRequestData::Create(script_state, web_request);
  return new Request(script_state, request);
}

bool Request::ParseCredentialsMode(
    const String& credentials_mode,
    network::mojom::FetchCredentialsMode* result) {
  if (credentials_mode == "omit") {
    *result = network::mojom::FetchCredentialsMode::kOmit;
    return true;
  }
  if (credentials_mode == "same-origin") {
    *result = network::mojom::FetchCredentialsMode::kSameOrigin;
    return true;
  }
  if (credentials_mode == "include") {
    *result = network::mojom::FetchCredentialsMode::kInclude;
    return true;
  }
  return false;
}

Request::Request(ScriptState* script_state,
                 FetchRequestData* request,
                 Headers* headers,
                 AbortSignal* signal)
    : Body(ExecutionContext::From(script_state)),
      request_(request),
      headers_(headers),
      signal_(signal) {
}

Request::Request(ScriptState* script_state, FetchRequestData* request)
    : Request(script_state,
              request,
              Headers::Create(request->HeaderList()),
              new AbortSignal(ExecutionContext::From(script_state))) {
  headers_->SetGuard(Headers::kRequestGuard);
}

String Request::method() const {
  // "The method attribute's getter must return request's method."
  return request_->Method();
}

KURL Request::url() const {
  return request_->Url();
}

String Request::destination() const {
  // "The destination attribute’s getter must return request’s destination."
  switch (request_->Context()) {
    case mojom::RequestContextType::UNSPECIFIED:
    case mojom::RequestContextType::BEACON:
    case mojom::RequestContextType::DOWNLOAD:
    case mojom::RequestContextType::EVENT_SOURCE:
    case mojom::RequestContextType::FETCH:
    case mojom::RequestContextType::PING:
    case mojom::RequestContextType::XML_HTTP_REQUEST:
    case mojom::RequestContextType::SUBRESOURCE:
    case mojom::RequestContextType::PREFETCH:
      return "";
    case mojom::RequestContextType::CSP_REPORT:
      return "report";
    case mojom::RequestContextType::AUDIO:
      return "audio";
    case mojom::RequestContextType::EMBED:
      return "embed";
    case mojom::RequestContextType::FONT:
      return "font";
    case mojom::RequestContextType::FRAME:
    case mojom::RequestContextType::HYPERLINK:
    case mojom::RequestContextType::IFRAME:
    case mojom::RequestContextType::LOCATION:
    case mojom::RequestContextType::FORM:
      return "document";
    case mojom::RequestContextType::IMAGE:
    case mojom::RequestContextType::FAVICON:
    case mojom::RequestContextType::IMAGE_SET:
      return "image";
    case mojom::RequestContextType::MANIFEST:
      return "manifest";
    case mojom::RequestContextType::OBJECT:
      return "object";
    case mojom::RequestContextType::SCRIPT:
      return "script";
    case mojom::RequestContextType::SHARED_WORKER:
      return "sharedworker";
    case mojom::RequestContextType::STYLE:
      return "style";
    case mojom::RequestContextType::TRACK:
      return "track";
    case mojom::RequestContextType::VIDEO:
      return "video";
    case mojom::RequestContextType::WORKER:
      return "worker";
    case mojom::RequestContextType::XSLT:
      return "xslt";
    case mojom::RequestContextType::IMPORT:
    case mojom::RequestContextType::INTERNAL:
    case mojom::RequestContextType::PLUGIN:
    case mojom::RequestContextType::SERVICE_WORKER:
      return "unknown";
  }
  NOTREACHED();
  return "";
}

String Request::referrer() const {
  // "The referrer attribute's getter must return the empty string if
  // request's referrer is no referrer, "about:client" if request's referrer
  // is client and request's referrer, serialized, otherwise."
  DCHECK_EQ(Referrer::NoReferrer(), String());
  DCHECK_EQ(Referrer::ClientReferrerString(), "about:client");
  return request_->ReferrerString();
}

String Request::getReferrerPolicy() const {
  switch (request_->GetReferrerPolicy()) {
    case kReferrerPolicyAlways:
      return "unsafe-url";
    case kReferrerPolicyDefault:
      return "";
    case kReferrerPolicyNoReferrerWhenDowngrade:
      return "no-referrer-when-downgrade";
    case kReferrerPolicyNever:
      return "no-referrer";
    case kReferrerPolicyOrigin:
      return "origin";
    case kReferrerPolicyOriginWhenCrossOrigin:
      return "origin-when-cross-origin";
    case kReferrerPolicySameOrigin:
      return "same-origin";
    case kReferrerPolicyStrictOrigin:
      return "strict-origin";
    case kReferrerPolicyStrictOriginWhenCrossOrigin:
      return "strict-origin-when-cross-origin";
  }
  NOTREACHED();
  return String();
}

String Request::mode() const {
  // "The mode attribute's getter must return the value corresponding to the
  // first matching statement, switching on request's mode:"
  switch (request_->Mode()) {
    case network::mojom::FetchRequestMode::kSameOrigin:
      return "same-origin";
    case network::mojom::FetchRequestMode::kNoCORS:
      return "no-cors";
    case network::mojom::FetchRequestMode::kCORS:
    case network::mojom::FetchRequestMode::kCORSWithForcedPreflight:
      return "cors";
    case network::mojom::FetchRequestMode::kNavigate:
      return "navigate";
  }
  NOTREACHED();
  return "";
}

String Request::credentials() const {
  // "The credentials attribute's getter must return the value corresponding
  // to the first matching statement, switching on request's credentials
  // mode:"
  switch (request_->Credentials()) {
    case network::mojom::FetchCredentialsMode::kOmit:
      return "omit";
    case network::mojom::FetchCredentialsMode::kSameOrigin:
      return "same-origin";
    case network::mojom::FetchCredentialsMode::kInclude:
      return "include";
  }
  NOTREACHED();
  return "";
}

String Request::cache() const {
  // "The cache attribute's getter must return request's cache mode."
  switch (request_->CacheMode()) {
    case mojom::FetchCacheMode::kDefault:
      return "default";
    case mojom::FetchCacheMode::kNoStore:
      return "no-store";
    case mojom::FetchCacheMode::kBypassCache:
      return "reload";
    case mojom::FetchCacheMode::kValidateCache:
      return "no-cache";
    case mojom::FetchCacheMode::kForceCache:
      return "force-cache";
    case mojom::FetchCacheMode::kOnlyIfCached:
      return "only-if-cached";
    case mojom::FetchCacheMode::kUnspecifiedOnlyIfCachedStrict:
    case mojom::FetchCacheMode::kUnspecifiedForceCacheMiss:
      NOTREACHED();
      break;
  }
  NOTREACHED();
  return "";
}

String Request::redirect() const {
  // "The redirect attribute's getter must return request's redirect mode."
  switch (request_->Redirect()) {
    case network::mojom::FetchRedirectMode::kFollow:
      return "follow";
    case network::mojom::FetchRedirectMode::kError:
      return "error";
    case network::mojom::FetchRedirectMode::kManual:
      return "manual";
  }
  NOTREACHED();
  return "";
}

String Request::integrity() const {
  return request_->Integrity();
}

bool Request::keepalive() const {
  return request_->Keepalive();
}

bool Request::isHistoryNavigation() const {
  return request_->IsHistoryNavigation();
}

Request* Request::clone(ScriptState* script_state,
                        ExceptionState& exception_state) {
  if (IsBodyLocked(exception_state) == BodyLocked::kLocked ||
      IsBodyUsed(exception_state) == BodyUsed::kUsed) {
    DCHECK(!exception_state.HadException());
    exception_state.ThrowTypeError("Request body is already used");
    return nullptr;
  }
  if (exception_state.HadException())
    return nullptr;

  FetchRequestData* request = request_->Clone(script_state, exception_state);
  if (exception_state.HadException())
    return nullptr;
  Headers* headers = Headers::Create(request->HeaderList());
  headers->SetGuard(headers_->GetGuard());
  auto* signal = new AbortSignal(ExecutionContext::From(script_state));
  signal->Follow(signal_);
  return new Request(script_state, request, headers, signal);
}

FetchRequestData* Request::PassRequestData(ScriptState* script_state,
                                           ExceptionState& exception_state) {
  DCHECK(!IsBodyUsedForDCheck());
  FetchRequestData* data = request_->Pass(script_state, exception_state);
  if (exception_state.HadException())
    return nullptr;
  // |data|'s buffer('s js wrapper) has no retainer, but it's OK because
  // the only caller is the fetch function and it uses the body buffer
  // immediately.
  return data;
}

bool Request::HasBody() const {
  return BodyBuffer();
}

void Request::PopulateWebServiceWorkerRequest(
    WebServiceWorkerRequest& web_request) const {
  web_request.SetMethod(method());
  web_request.SetMode(request_->Mode());
  web_request.SetCredentialsMode(request_->Credentials());
  web_request.SetCacheMode(request_->CacheMode());
  web_request.SetRedirectMode(request_->Redirect());
  web_request.SetIntegrity(request_->Integrity());
  web_request.SetIsHistoryNavigation(request_->IsHistoryNavigation());
  web_request.SetRequestContext(request_->Context());

  // Strip off the fragment part of URL. So far, all users of
  // WebServiceWorkerRequest expect the fragment to be excluded.
  KURL url(request_->Url());
  if (request_->Url().HasFragmentIdentifier())
    url.RemoveFragmentIdentifier();
  web_request.SetURL(url);

  const FetchHeaderList* header_list = headers_->HeaderList();
  for (const auto& header : header_list->List()) {
    web_request.AppendHeader(header.first, header.second);
  }

  web_request.SetReferrer(request_->ReferrerString(),
                          static_cast<network::mojom::ReferrerPolicy>(
                              request_->GetReferrerPolicy()));
  // FIXME: How can we set isReload properly? What is the correct place to load
  // it in to the Request object? We should investigate the right way to plumb
  // this information in to here.
}

String Request::MimeType() const {
  return request_->MimeType();
}

String Request::ContentType() const {
  String result;
  request_->HeaderList()->Get(HTTPNames::Content_Type, result);
  return result;
}

void Request::Trace(blink::Visitor* visitor) {
  Body::Trace(visitor);
  visitor->Trace(request_);
  visitor->Trace(headers_);
  visitor->Trace(signal_);
}

}  // namespace blink
