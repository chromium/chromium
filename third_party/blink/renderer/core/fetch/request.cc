// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/request.h"

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/cpp/request_mode.h"
#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/public/common/loader/request_destination.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
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
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/loader/threadable_loader.h"
#include "third_party/blink/renderer/core/url/url_search_params.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
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
  request->SetOrigin(ExecutionContext::From(script_state)->GetSecurityOrigin());
  // FIXME: Set client.
  DOMWrapperWorld& world = script_state->World();
  if (world.IsIsolatedWorld())
    request->SetIsolatedWorldOrigin(world.IsolatedWorldSecurityOrigin());
  // FIXME: Set ForceOriginHeaderFlag.
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
    mojo::PendingRemote<network::mojom::blink::URLLoaderFactory> factory_clone;
    original->URLLoaderFactory()->Clone(
        factory_clone.InitWithNewPipeAndPassReceiver());
    request->SetURLLoaderFactory(std::move(factory_clone));
  }
  request->SetWindowId(original->WindowId());
  return request;
}

static bool AreAnyMembersPresent(const RequestInit* init) {
  return init->hasMethod() || init->hasHeaders() || init->hasBody() ||
         init->hasReferrer() || init->hasReferrerPolicy() || init->hasMode() ||
         init->hasCredentials() || init->hasCache() || init->hasRedirect() ||
         init->hasIntegrity() || init->hasKeepalive() ||
         init->hasImportance() || init->hasSignal();
}

static BodyStreamBuffer* ExtractBody(ScriptState* script_state,
                                     ExceptionState& exception_state,
                                     v8::Local<v8::Value> body,
                                     String& content_type) {
  DCHECK(!body->IsNull());
  BodyStreamBuffer* return_buffer = nullptr;

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  v8::Isolate* isolate = script_state->GetIsolate();

  if (V8Blob::HasInstance(body, isolate)) {
    Blob* blob = V8Blob::ToImpl(body.As<v8::Object>());
    return_buffer = MakeGarbageCollected<BodyStreamBuffer>(
        script_state,
        MakeGarbageCollected<BlobBytesConsumer>(execution_context,
                                                blob->GetBlobDataHandle()),
        nullptr /* AbortSignal */);
    content_type = blob->type();
  } else if (body->IsArrayBuffer()) {
    // Avoid calling into V8 from the following constructor parameters, which
    // is potentially unsafe.
    DOMArrayBuffer* array_buffer = V8ArrayBuffer::ToImpl(body.As<v8::Object>());
    return_buffer = MakeGarbageCollected<BodyStreamBuffer>(
        script_state, MakeGarbageCollected<FormDataBytesConsumer>(array_buffer),
        nullptr /* AbortSignal */);
  } else if (body->IsArrayBufferView()) {
    // Avoid calling into V8 from the following constructor parameters, which
    // is potentially unsafe.
    DOMArrayBufferView* array_buffer_view =
        V8ArrayBufferView::ToImpl(body.As<v8::Object>());
    return_buffer = MakeGarbageCollected<BodyStreamBuffer>(
        script_state,
        MakeGarbageCollected<FormDataBytesConsumer>(array_buffer_view),
        nullptr /* AbortSignal */);
  } else if (V8FormData::HasInstance(body, isolate)) {
    scoped_refptr<EncodedFormData> form_data =
        V8FormData::ToImpl(body.As<v8::Object>())->EncodeMultiPartFormData();
    // Here we handle formData->boundary() as a C-style string. See
    // FormDataEncoder::generateUniqueBoundaryString.
    content_type = AtomicString("multipart/form-data; boundary=") +
                   form_data->Boundary().data();
    return_buffer = MakeGarbageCollected<BodyStreamBuffer>(
        script_state,
        MakeGarbageCollected<FormDataBytesConsumer>(execution_context,
                                                    std::move(form_data)),
        nullptr /* AbortSignal */);
  } else if (V8URLSearchParams::HasInstance(body, isolate)) {
    scoped_refptr<EncodedFormData> form_data =
        V8URLSearchParams::ToImpl(body.As<v8::Object>())->ToEncodedFormData();
    return_buffer = MakeGarbageCollected<BodyStreamBuffer>(
        script_state,
        MakeGarbageCollected<FormDataBytesConsumer>(execution_context,
                                                    std::move(form_data)),
        nullptr /* AbortSignal */);
    content_type = "application/x-www-form-urlencoded;charset=UTF-8";
  } else {
    String string = NativeValueTraits<IDLUSVString>::NativeValue(
        isolate, body, exception_state);
    if (exception_state.HadException())
      return nullptr;

    return_buffer = MakeGarbageCollected<BodyStreamBuffer>(
        script_state, MakeGarbageCollected<FormDataBytesConsumer>(string),
        nullptr /* AbortSignal */);
    content_type = "text/plain;charset=UTF-8";
  }

  return return_buffer;
}

Request* Request::CreateRequestWithRequestOrString(
    ScriptState* script_state,
    Request* input_request,
    const String& input_string,
    const RequestInit* init,
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

  // The spec says:
  // - "Let |window| be client."
  // - "If |request|'s window is an environment settings object and its
  //   origin is same origin with current settings object's origin, set
  //   |window| to |request|'s window."
  // - "If |init|'s window member is present and it is not null, throw a
  //   TypeError."
  // - "If |init|'s window member is present, set |window| to no-window."
  //
  // We partially do this: if |request|'s window is present, it is copied to
  // the new request in the following step. There is no same-origin check
  // because |request|'s window is implemented as |FetchRequestData.window_id_|
  // and is an opaque id that this renderer doesn't understand. It's only set on
  // |input_request| when a service worker intercepted the request from a
  // (same-origin) frame, so it must be same-origin.
  //
  // TODO(yhirano): Add support for |init.window|.

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
    if (parsed_url.ProtocolIs("blob")) {
      mojo::PendingRemote<network::mojom::blink::URLLoaderFactory>
          url_loader_factory;
      ExecutionContext::From(script_state)
          ->GetPublicURLManager()
          .Resolve(parsed_url,
                   url_loader_factory.InitWithNewPipeAndPassReceiver());
      request->SetURLLoaderFactory(std::move(url_loader_factory));
    }

    // We don't use fallback values. We set these flags directly in below.
    // - "Set |fallbackMode| to "cors"."
    // - "Set |fallbackCredentials| to "omit"."
  }

  // "If any of |init|'s members are present, then:"
  if (AreAnyMembersPresent(init)) {
    // "If |request|'s |mode| is "navigate", then set it to "same-origin".
    if (network::IsNavigationRequestMode(request->Mode()))
      request->SetMode(network::mojom::RequestMode::kSameOrigin);

    // TODO(yhirano): Implement the following substep:
    // "Unset |request|'s reload-navigation flag."

    // "Unset |request|'s history-navigation flag."
    request->SetIsHistoryNavigation(false);

    // "Set |request|’s referrer to "client"."
    request->SetReferrerString(AtomicString(Referrer::ClientReferrerString()));

    // "Set |request|’s referrer policy to the empty string."
    request->SetReferrerPolicy(network::mojom::ReferrerPolicy::kDefault);
  }

  // "If init’s referrer member is present, then:"
  if (init->hasReferrer()) {
    // Nothing to do for the step "Let |referrer| be |init|'s referrer
    // member."

    if (init->referrer().IsEmpty()) {
      // "If |referrer| is the empty string, set |request|'s referrer to
      // "no-referrer" and terminate these substeps."
      request->SetReferrerString(AtomicString(Referrer::NoReferrer()));
    } else {
      // "Let |parsedReferrer| be the result of parsing |referrer| with
      // |baseURL|."
      KURL parsed_referrer(base_url, init->referrer());
      if (!parsed_referrer.IsValid()) {
        // "If |parsedReferrer| is failure, throw a TypeError."
        exception_state.ThrowTypeError("Referrer '" + init->referrer() +
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
  if (init->hasReferrerPolicy()) {
    // In case referrerPolicy = "", the SecurityPolicy method below will not
    // actually set referrer_policy, so we'll default to
    // network::mojom::ReferrerPolicy::kDefault.
    network::mojom::ReferrerPolicy referrer_policy;
    if (!SecurityPolicy::ReferrerPolicyFromString(
            init->referrerPolicy(), kDoNotSupportReferrerPolicyLegacyKeywords,
            &referrer_policy)) {
      DCHECK(init->referrerPolicy().IsEmpty());
      referrer_policy = network::mojom::ReferrerPolicy::kDefault;
    }

    request->SetReferrerPolicy(referrer_policy);
  }

  // The following code performs the following steps:
  // - "Let |mode| be |init|'s mode member if it is present, and
  //   |fallbackMode| otherwise."
  // - "If |mode| is "navigate", throw a TypeError."
  // - "If |mode| is non-null, set |request|'s mode to |mode|."
  if (init->mode() == "navigate") {
    exception_state.ThrowTypeError(
        "Cannot construct a Request with a RequestInit whose mode member is "
        "set as 'navigate'.");
    return nullptr;
  }
  if (init->mode() == "same-origin") {
    request->SetMode(network::mojom::RequestMode::kSameOrigin);
  } else if (init->mode() == "no-cors") {
    request->SetMode(network::mojom::RequestMode::kNoCors);
  } else if (init->mode() == "cors") {
    request->SetMode(network::mojom::RequestMode::kCors);
  } else {
    // |inputRequest| is directly checked here instead of setting and
    // checking |fallbackMode| as specified in the spec.
    if (!input_request)
      request->SetMode(network::mojom::RequestMode::kCors);
  }

  // This is not yet standardized, but we can assume the following:
  // "If |init|'s importance member is present, set |request|'s importance
  // mode to it." For more information see Priority Hints at
  // https://crbug.com/821464.
  DCHECK(init->importance().IsNull() ||
         RuntimeEnabledFeatures::PriorityHintsEnabled(execution_context));
  if (!init->importance().IsNull())
    UseCounter::Count(execution_context, WebFeature::kPriorityHints);

  if (init->importance() == "low") {
    request->SetImportance(mojom::FetchImportanceMode::kImportanceLow);
  } else if (init->importance() == "high") {
    request->SetImportance(mojom::FetchImportanceMode::kImportanceHigh);
  }

  // "Let |credentials| be |init|'s credentials member if it is present, and
  // |fallbackCredentials| otherwise."
  // "If |credentials| is non-null, set |request|'s credentials mode to
  // |credentials|."

  network::mojom::CredentialsMode credentials_mode;
  if (ParseCredentialsMode(init->credentials(), &credentials_mode)) {
    request->SetCredentials(credentials_mode);
  } else if (!input_request) {
    request->SetCredentials(network::mojom::CredentialsMode::kSameOrigin);
  }

  // "If |init|'s cache member is present, set |request|'s cache mode to it."
  if (init->cache() == "default") {
    request->SetCacheMode(mojom::FetchCacheMode::kDefault);
  } else if (init->cache() == "no-store") {
    request->SetCacheMode(mojom::FetchCacheMode::kNoStore);
  } else if (init->cache() == "reload") {
    request->SetCacheMode(mojom::FetchCacheMode::kBypassCache);
  } else if (init->cache() == "no-cache") {
    request->SetCacheMode(mojom::FetchCacheMode::kValidateCache);
  } else if (init->cache() == "force-cache") {
    request->SetCacheMode(mojom::FetchCacheMode::kForceCache);
  } else if (init->cache() == "only-if-cached") {
    request->SetCacheMode(mojom::FetchCacheMode::kOnlyIfCached);
  }

  // If |request|’s cache mode is "only-if-cached" and |request|’s mode is not
  // "same-origin", then throw a TypeError.
  if (request->CacheMode() == mojom::FetchCacheMode::kOnlyIfCached &&
      request->Mode() != network::mojom::RequestMode::kSameOrigin) {
    exception_state.ThrowTypeError(
        "'only-if-cached' can be set only with 'same-origin' mode");
    return nullptr;
  }

  // "If |init|'s redirect member is present, set |request|'s redirect mode
  // to it."
  if (init->redirect() == "follow") {
    request->SetRedirect(network::mojom::RedirectMode::kFollow);
  } else if (init->redirect() == "error") {
    request->SetRedirect(network::mojom::RedirectMode::kError);
  } else if (init->redirect() == "manual") {
    request->SetRedirect(network::mojom::RedirectMode::kManual);
  }

  // "If |init|'s integrity member is present, set |request|'s
  // integrity metadata to it."
  if (init->hasIntegrity())
    request->SetIntegrity(init->integrity());

  if (init->hasKeepalive())
    request->SetKeepalive(init->keepalive());

  // "If |init|'s method member is present, let |method| be it and run these
  // substeps:"
  if (init->hasMethod()) {
    // "If |method| is not a method or method is a forbidden method, throw
    // a TypeError."
    if (!IsValidHTTPToken(init->method())) {
      exception_state.ThrowTypeError("'" + init->method() +
                                     "' is not a valid HTTP method.");
      return nullptr;
    }
    if (FetchUtils::IsForbiddenMethod(init->method())) {
      exception_state.ThrowTypeError("'" + init->method() +
                                     "' HTTP method is unsupported.");
      return nullptr;
    }
    // "Normalize |method|."
    // "Set |request|'s method to |method|."
    request->SetMethod(
        FetchUtils::NormalizeMethod(AtomicString(init->method())));
  }

  // "If |init|'s signal member is present, then set |signal| to it."
  if (init->hasSignal()) {
    signal = init->signal();
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
  if (!init->hasHeaders()) {
    headers = r->getHeaders()->Clone();
  }
  // "Empty |r|'s request's header list."
  r->request_->HeaderList()->ClearList();
  // "If |r|'s request's mode is "no-cors", run these substeps:
  if (r->GetRequest()->Mode() == network::mojom::RequestMode::kNoCors) {
    // "If |r|'s request's method is not a CORS-safelisted method, throw a
    // TypeError."
    if (!cors::IsCorsSafelistedMethod(r->GetRequest()->Method())) {
      exception_state.ThrowTypeError("'" + r->GetRequest()->Method() +
                                     "' is unsupported in no-cors mode.");
      return nullptr;
    }
    // "Set |r|'s Headers object's guard to "request-no-cors"."
    r->getHeaders()->SetGuard(Headers::kRequestNoCorsGuard);
  }
  // "If |signal| is not null, then make |r|’s signal follow |signal|."
  if (signal)
    r->signal_->Follow(signal);

  // "Fill |r|'s Headers object with |headers|. Rethrow any exceptions."
  if (init->hasHeaders()) {
    r->getHeaders()->FillWith(init->headers(), exception_state);
  } else {
    DCHECK(headers);
    r->getHeaders()->FillWith(headers, exception_state);
  }
  if (exception_state.HadException())
    return nullptr;

  // "If either |init|'s body member is present or |temporaryBody| is
  // non-null, and |request|'s method is `GET` or `HEAD`, throw a TypeError.
  v8::Local<v8::Value> init_body =
      init->hasBody() ? init->body().V8Value() : v8::Local<v8::Value>();
  if ((!init_body.IsEmpty() && !init_body->IsNull()) || temporary_body) {
    if (request->Method() == http_names::kGET ||
        request->Method() == http_names::kHEAD) {
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
        !r->getHeaders()->has(http_names::kContentType, exception_state)) {
      r->getHeaders()->append(http_names::kContentType, content_type,
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
  r->request_->SetMimeType(r->request_->HeaderList()->ExtractMIMEType());

  // "If |input| is a Request object and |input|'s request's body is
  // non-null, run these substeps:"
  if (input_request && input_request->BodyBuffer()) {
    // "Let |dummyStream| be an empty ReadableStream object."
    auto* dummy_stream = MakeGarbageCollected<BodyStreamBuffer>(
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
                         const RequestInit* init,
                         ExceptionState& exception_state) {
  DCHECK(!input.IsNull());
  if (input.IsUSVString())
    return Create(script_state, input.GetAsUSVString(), init, exception_state);
  return Create(script_state, input.GetAsRequest(), init, exception_state);
}

Request* Request::Create(ScriptState* script_state,
                         const String& input,
                         ExceptionState& exception_state) {
  return Create(script_state, input, RequestInit::Create(), exception_state);
}

Request* Request::Create(ScriptState* script_state,
                         const String& input,
                         const RequestInit* init,
                         ExceptionState& exception_state) {
  return CreateRequestWithRequestOrString(script_state, nullptr, input, init,
                                          exception_state);
}

Request* Request::Create(ScriptState* script_state,
                         Request* input,
                         ExceptionState& exception_state) {
  return Create(script_state, input, RequestInit::Create(), exception_state);
}

Request* Request::Create(ScriptState* script_state,
                         Request* input,
                         const RequestInit* init,
                         ExceptionState& exception_state) {
  return CreateRequestWithRequestOrString(script_state, input, String(), init,
                                          exception_state);
}

Request* Request::Create(ScriptState* script_state, FetchRequestData* request) {
  return MakeGarbageCollected<Request>(script_state, request);
}

Request* Request::Create(
    ScriptState* script_state,
    const mojom::blink::FetchAPIRequest& fetch_api_request,
    ForServiceWorkerFetchEvent for_service_worker_fetch_event) {
  FetchRequestData* data = FetchRequestData::Create(
      script_state, fetch_api_request, for_service_worker_fetch_event);
  return MakeGarbageCollected<Request>(script_state, data);
}

bool Request::ParseCredentialsMode(const String& credentials_mode,
                                   network::mojom::CredentialsMode* result) {
  if (credentials_mode == "omit") {
    *result = network::mojom::CredentialsMode::kOmit;
    return true;
  }
  if (credentials_mode == "same-origin") {
    *result = network::mojom::CredentialsMode::kSameOrigin;
    return true;
  }
  if (credentials_mode == "include") {
    *result = network::mojom::CredentialsMode::kInclude;
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
              MakeGarbageCollected<AbortSignal>(
                  ExecutionContext::From(script_state))) {
  headers_->SetGuard(Headers::kRequestGuard);
}

String Request::method() const {
  // "The method attribute's getter must return request's method."
  return request_->Method();
}

const KURL& Request::url() const {
  return request_->Url();
}

String Request::destination() const {
  // "The destination attribute’s getter must return request’s destination."
  return GetRequestDestinationFromContext(request_->Context());
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
    case network::mojom::ReferrerPolicy::kAlways:
      return "unsafe-url";
    case network::mojom::ReferrerPolicy::kDefault:
      return "";
    case network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade:
      return "no-referrer-when-downgrade";
    case network::mojom::ReferrerPolicy::kNever:
      return "no-referrer";
    case network::mojom::ReferrerPolicy::kOrigin:
      return "origin";
    case network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin:
      return "origin-when-cross-origin";
    case network::mojom::ReferrerPolicy::kSameOrigin:
      return "same-origin";
    case network::mojom::ReferrerPolicy::kStrictOrigin:
      return "strict-origin";
    case network::mojom::ReferrerPolicy::
        kNoReferrerWhenDowngradeOriginWhenCrossOrigin:
      return "strict-origin-when-cross-origin";
  }
  NOTREACHED();
  return String();
}

String Request::mode() const {
  // "The mode attribute's getter must return the value corresponding to the
  // first matching statement, switching on request's mode:"
  switch (request_->Mode()) {
    case network::mojom::RequestMode::kSameOrigin:
      return "same-origin";
    case network::mojom::RequestMode::kNoCors:
      return "no-cors";
    case network::mojom::RequestMode::kCors:
    case network::mojom::RequestMode::kCorsWithForcedPreflight:
      return "cors";
    case network::mojom::RequestMode::kNavigate:
    case network::mojom::RequestMode::kNavigateNestedFrame:
    case network::mojom::RequestMode::kNavigateNestedObject:
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
    case network::mojom::CredentialsMode::kOmit:
      return "omit";
    case network::mojom::CredentialsMode::kSameOrigin:
      return "same-origin";
    case network::mojom::CredentialsMode::kInclude:
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
    case network::mojom::RedirectMode::kFollow:
      return "follow";
    case network::mojom::RedirectMode::kError:
      return "error";
    case network::mojom::RedirectMode::kManual:
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
  auto* signal =
      MakeGarbageCollected<AbortSignal>(ExecutionContext::From(script_state));
  signal->Follow(signal_);
  return MakeGarbageCollected<Request>(script_state, request, headers, signal);
}

FetchRequestData* Request::PassRequestData(ScriptState* script_state,
                                           ExceptionState& exception_state) {
  DCHECK(!IsBodyUsedForDCheck(exception_state));
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

mojom::blink::FetchAPIRequestPtr Request::CreateFetchAPIRequest() const {
  auto fetch_api_request = mojom::blink::FetchAPIRequest::New();
  fetch_api_request->method = method();
  fetch_api_request->mode = request_->Mode();
  fetch_api_request->credentials_mode = request_->Credentials();
  fetch_api_request->cache_mode = request_->CacheMode();
  fetch_api_request->redirect_mode = request_->Redirect();
  fetch_api_request->integrity = request_->Integrity();
  fetch_api_request->is_history_navigation = request_->IsHistoryNavigation();
  fetch_api_request->request_context_type = request_->Context();

  // Strip off the fragment part of URL. So far, all callers expect the fragment
  // to be excluded.
  KURL url(request_->Url());
  if (request_->Url().HasFragmentIdentifier())
    url.RemoveFragmentIdentifier();
  fetch_api_request->url = url;

  HTTPHeaderMap headers;
  for (const auto& header : headers_->HeaderList()->List()) {
    if (DeprecatedEqualIgnoringCase(header.first, "referer"))
      continue;
    AtomicString key(header.first);
    AtomicString value(header.second);
    HTTPHeaderMap::AddResult result = headers.Add(key, value);
    if (!result.is_new_entry) {
      result.stored_value->value =
          result.stored_value->value + ", " + String(value);
    }
  }
  for (const auto& pair : headers)
    fetch_api_request->headers.insert(pair.key, pair.value);

  if (!request_->ReferrerString().IsEmpty()) {
    fetch_api_request->referrer =
        mojom::blink::Referrer::New(KURL(NullURL(), request_->ReferrerString()),
                                    request_->GetReferrerPolicy());
    DCHECK(fetch_api_request->referrer->url.IsValid());
  }
  // FIXME: How can we set isReload properly? What is the correct place to load
  // it in to the Request object? We should investigate the right way to plumb
  // this information in to here.
  return fetch_api_request;
}

String Request::MimeType() const {
  return request_->MimeType();
}

String Request::ContentType() const {
  String result;
  request_->HeaderList()->Get(http_names::kContentType, result);
  return result;
}

void Request::Trace(blink::Visitor* visitor) {
  Body::Trace(visitor);
  visitor->Trace(request_);
  visitor->Trace(headers_);
  visitor->Trace(signal_);
}

}  // namespace blink
