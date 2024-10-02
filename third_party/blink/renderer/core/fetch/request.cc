// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/request.h"

#include <optional>

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/cpp/request_destination.h"
#include "services/network/public/cpp/request_mode.h"
#include "services/network/public/mojom/attribution.mojom-blink.h"
#include "services/network/public/mojom/ip_address_space.mojom-blink.h"
#include "services/network/public/mojom/trust_tokens.mojom-blink.h"
#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_abort_signal.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_blob.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_form_data.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_private_token.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_readable_stream.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_request_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_request_usvstring.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_url_search_params.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/attribution_reporting_to_mojom.h"
#include "third_party/blink/renderer/core/fetch/blob_bytes_consumer.h"
#include "third_party/blink/renderer/core/fetch/body_stream_buffer.h"
#include "third_party/blink/renderer/core/fetch/fetch_manager.h"
#include "third_party/blink/renderer/core/fetch/form_data_bytes_consumer.h"
#include "third_party/blink/renderer/core/fetch/request_util.h"
#include "third_party/blink/renderer/core/fetch/trust_token_to_mojom.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/fileapi/public_url_manager.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/loader/threadable_loader.h"
#include "third_party/blink/renderer/core/url/url_search_params.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/cors/cors.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_utils.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_or_worker_scheduler.h"
#include "third_party/blink/renderer/platform/weborigin/origin_access_entry.h"
#include "third_party/blink/renderer/platform/weborigin/referrer.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"

namespace blink {

namespace {

using network::mojom::blink::TrustTokenOperationType;

}  // namespace

FetchRequestData* CreateCopyOfFetchRequestDataForFetch(
    ScriptState* script_state,
    const FetchRequestData* original) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  auto* request = MakeGarbageCollected<FetchRequestData>(context);
  request->SetURL(original->Url());
  request->SetMethod(original->Method());
  request->SetHeaderList(original->HeaderList()->Clone());
  request->SetOrigin(original->Origin() ? original->Origin()
                                        : context->GetSecurityOrigin());
  request->SetNavigationRedirectChain(original->NavigationRedirectChain());
  // FIXME: Set client.
  DOMWrapperWorld& world = script_state->World();
  if (world.IsIsolatedWorld()) {
    request->SetIsolatedWorldOrigin(
        world.IsolatedWorldSecurityOrigin(context->GetAgentClusterID()));
  }
  // FIXME: Set ForceOriginHeaderFlag.
  request->SetReferrerString(original->ReferrerString());
  request->SetReferrerPolicy(original->GetReferrerPolicy());
  request->SetMode(original->Mode());
  request->SetTargetAddressSpace(original->TargetAddressSpace());
  request->SetCredentials(original->Credentials());
  request->SetCacheMode(original->CacheMode());
  request->SetRedirect(original->Redirect());
  request->SetIntegrity(original->Integrity());
  request->SetFetchPriorityHint(original->FetchPriorityHint());
  request->SetPriority(original->Priority());
  request->SetKeepalive(original->Keepalive());
  request->SetBrowsingTopics(original->BrowsingTopics());
  request->SetAdAuctionHeaders(original->AdAuctionHeaders());
  request->SetSharedStorageWritable(original->SharedStorageWritable());
  request->SetIsHistoryNavigation(original->IsHistoryNavigation());
  if (original->URLLoaderFactory()) {
    mojo::PendingRemote<network::mojom::blink::URLLoaderFactory> factory_clone;
    original->URLLoaderFactory()->Clone(
        factory_clone.InitWithNewPipeAndPassReceiver());
    request->SetURLLoaderFactory(std::move(factory_clone));
  }
  request->SetWindowId(original->WindowId());
  request->SetTrustTokenParams(original->TrustTokenParams());
  request->SetAttributionReportingEligibility(
      original->AttributionReportingEligibility());
  request->SetAttributionReportingSupport(original->AttributionSupport());
  request->SetServiceWorkerRaceNetworkRequestToken(
      original->ServiceWorkerRaceNetworkRequestToken());

  // When a new request is created from another the destination is always reset
  // to be `kEmpty`.  In order to facilitate some later checks when a service
  // worker forwards a navigation request we want to keep track of the
  // destination of the original request.  Therefore record the original
  // request's destination if its non-empty, otherwise just carry forward
  // whatever "original destination" value was already set.
  if (original->Destination() != network::mojom::RequestDestination::kEmpty)
    request->SetOriginalDestination(original->Destination());
  else
    request->SetOriginalDestination(original->OriginalDestination());

  return request;
}

static bool AreAnyMembersPresent(const RequestInit* init) {
  return init->hasMethod() || init->hasHeaders() || init->hasBody() ||
         init->hasReferrer() || init->hasReferrerPolicy() || init->hasMode() ||
         init->hasTargetAddressSpace() || init->hasCredentials() ||
         init->hasCache() || init->hasRedirect() || init->hasIntegrity() ||
         init->hasKeepalive() || init->hasBrowsingTopics() ||
         init->hasAdAuctionHeaders() || init->hasSharedStorageWritable() ||
         init->hasPriority() || init->hasSignal() || init->hasDuplex() ||
         init->hasPrivateToken() || init->hasAttributionReporting();
}

static BodyStreamBuffer* ExtractBody(ScriptState* script_state,
                                     ExceptionState& exception_state,
                                     v8::Local<v8::Value> body,
                                     String& content_type,
                                     uint64_t& body_byte_length) {
  DCHECK(!body->IsNull());
  BodyStreamBuffer* return_buffer = nullptr;

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  v8::Isolate* isolate = script_state->GetIsolate();

  if (Blob* blob = V8Blob::ToWrappable(isolate, body)) {
    body_byte_length = blob->size();
    return_buffer = BodyStreamBuffer::Create(
        script_state,
        MakeGarbageCollected<BlobBytesConsumer>(execution_context,
                                                blob->GetBlobDataHandle()),
        nullptr /* AbortSignal */, /*cached_metadata_handler=*/nullptr);
    content_type = blob->type();
  } else if (body->IsArrayBuffer()) {
    // Avoid calling into V8 from the following constructor parameters, which
    // is potentially unsafe.
    DOMArrayBuffer* array_buffer =
        NativeValueTraits<DOMArrayBuffer>::NativeValue(isolate, body,
                                                       exception_state);
    if (exception_state.HadException())
      return nullptr;
    if (!base::CheckedNumeric<wtf_size_t>(array_buffer->ByteLength())
             .IsValid()) {
      exception_state.ThrowRangeError(
          "The provided ArrayBuffer exceeds the maximum supported size");
      return nullptr;
    }
    body_byte_length = array_buffer->ByteLength();
    return_buffer = BodyStreamBuffer::Create(
        script_state, MakeGarbageCollected<FormDataBytesConsumer>(array_buffer),
        nullptr /* AbortSignal */, /*cached_metadata_handler=*/nullptr);
  } else if (body->IsArrayBufferView()) {
    // Avoid calling into V8 from the following constructor parameters, which
    // is potentially unsafe.
    DOMArrayBufferView* array_buffer_view =
        NativeValueTraits<MaybeShared<DOMArrayBufferView>>::NativeValue(
            isolate, body, exception_state)
            .Get();
    if (exception_state.HadException())
      return nullptr;
    if (!base::CheckedNumeric<wtf_size_t>(array_buffer_view->byteLength())
             .IsValid()) {
      exception_state.ThrowRangeError(
          "The provided ArrayBufferView exceeds the maximum supported size");
      return nullptr;
    }
    body_byte_length = array_buffer_view->byteLength();
    return_buffer = BodyStreamBuffer::Create(
        script_state,
        MakeGarbageCollected<FormDataBytesConsumer>(array_buffer_view),
        nullptr /* AbortSignal */, /*cached_metadata_handler=*/nullptr);
  } else if (FormData* form = V8FormData::ToWrappable(isolate, body)) {
    scoped_refptr<EncodedFormData> form_data = form->EncodeMultiPartFormData();
    // Here we handle formData->boundary() as a C-style string. See
    // FormDataEncoder::generateUniqueBoundaryString.
    content_type = AtomicString("multipart/form-data; boundary=") +
                   form_data->Boundary().data();
    body_byte_length = form_data->SizeInBytes();
    return_buffer = BodyStreamBuffer::Create(
        script_state,
        MakeGarbageCollected<FormDataBytesConsumer>(execution_context,
                                                    std::move(form_data)),
        nullptr /* AbortSignal */, /*cached_metadata_handler=*/nullptr);
  } else if (URLSearchParams* url_search_params =
                 V8URLSearchParams::ToWrappable(isolate, body)) {
    scoped_refptr<EncodedFormData> form_data =
        url_search_params->ToEncodedFormData();
    body_byte_length = form_data->SizeInBytes();
    return_buffer = BodyStreamBuffer::Create(
        script_state,
        MakeGarbageCollected<FormDataBytesConsumer>(execution_context,
                                                    std::move(form_data)),
        nullptr /* AbortSignal */, /*cached_metadata_handler=*/nullptr);
    content_type = "application/x-www-form-urlencoded;charset=UTF-8";
  } else if (ReadableStream* readable_stream =
                 V8ReadableStream::ToWrappable(isolate, body);
             readable_stream &&
             RuntimeEnabledFeatures::FetchUploadStreamingEnabled(
                 execution_context)) {
    // This is implemented in Request::CreateRequestWithRequestOrString():
    //   "If the |keepalive| flag is set, then throw a TypeError."

    //   "If |object| is disturbed or locked, then throw a TypeError."
    if (readable_stream->IsDisturbed()) {
      exception_state.ThrowTypeError(
          "The provided ReadableStream is disturbed");
      return nullptr;
    }
    if (readable_stream->IsLocked()) {
      exception_state.ThrowTypeError("The provided ReadableStream is locked");
      return nullptr;
    }
    //   "Set |stream| to |object|."
    return_buffer = MakeGarbageCollected<BodyStreamBuffer>(
        script_state, readable_stream, /*cached_metadata_handler=*/nullptr);
  } else {
    String string = NativeValueTraits<IDLUSVString>::NativeValue(
        isolate, body, exception_state);
    if (exception_state.HadException())
      return nullptr;

    body_byte_length = string.length();
    return_buffer = BodyStreamBuffer::Create(
        script_state, MakeGarbageCollected<FormDataBytesConsumer>(string),
        nullptr /* AbortSignal */, /*cached_metadata_handler=*/nullptr);
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
      script_state, input_request ? input_request->GetRequest()
                                  : MakeGarbageCollected<FetchRequestData>(
                                        execution_context));

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
    if (!parsed_url.User().empty() || !parsed_url.Pass().empty()) {
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
    request->SetOrigin(execution_context->GetSecurityOrigin());
    request->SetOriginalDestination(network::mojom::RequestDestination::kEmpty);
    request->SetNavigationRedirectChain(Vector<KURL>());

    // "If |request|'s |mode| is "navigate", then set it to "same-origin".
    if (request->Mode() == network::mojom::RequestMode::kNavigate)
      request->SetMode(network::mojom::RequestMode::kSameOrigin);

    // TODO(yhirano): Implement the following substep:
    // "Unset |request|'s reload-navigation flag."

    // "Unset |request|'s history-navigation flag."
    request->SetIsHistoryNavigation(false);

    // "Set |request|’s referrer to "client"."
    request->SetReferrerString(Referrer::ClientReferrerString());

    // "Set |request|’s referrer policy to the empty string."
    request->SetReferrerPolicy(network::mojom::ReferrerPolicy::kDefault);
  }

  // "If init’s referrer member is present, then:"
  if (init->hasReferrer()) {
    // Nothing to do for the step "Let |referrer| be |init|'s referrer
    // member."

    if (init->referrer().empty()) {
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
           parsed_referrer.Host().empty() &&
           parsed_referrer.GetPath() == "client") ||
          !origin->IsSameOriginWith(
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
        request->SetReferrerString(Referrer::ClientReferrerString());
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
    network::mojom::ReferrerPolicy referrer_policy =
        network::mojom::ReferrerPolicy::kDefault;
    switch (init->referrerPolicy().AsEnum()) {
      case V8ReferrerPolicy::Enum::k:
        referrer_policy = network::mojom::ReferrerPolicy::kDefault;
        break;
      case V8ReferrerPolicy::Enum::kNoReferrer:
        referrer_policy = network::mojom::ReferrerPolicy::kNever;
        break;
      case V8ReferrerPolicy::Enum::kNoReferrerWhenDowngrade:
        referrer_policy =
            network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade;
        break;
      case V8ReferrerPolicy::Enum::kSameOrigin:
        referrer_policy = network::mojom::ReferrerPolicy::kSameOrigin;
        break;
      case V8ReferrerPolicy::Enum::kOrigin:
        referrer_policy = network::mojom::ReferrerPolicy::kOrigin;
        break;
      case V8ReferrerPolicy::Enum::kStrictOrigin:
        referrer_policy = network::mojom::ReferrerPolicy::kStrictOrigin;
        break;
      case V8ReferrerPolicy::Enum::kOriginWhenCrossOrigin:
        referrer_policy =
            network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin;
        break;
      case V8ReferrerPolicy::Enum::kStrictOriginWhenCrossOrigin:
        referrer_policy =
            network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin;
        break;
      case V8ReferrerPolicy::Enum::kUnsafeUrl:
        referrer_policy = network::mojom::ReferrerPolicy::kAlways;
        break;
      default:
        NOTREACHED();
    }
    request->SetReferrerPolicy(referrer_policy);
  }

  // The following code performs the following steps:
  // - "Let |mode| be |init|'s mode member if it is present, and
  //   |fallbackMode| otherwise."
  // - "If |mode| is "navigate", throw a TypeError."
  // - "If |mode| is non-null, set |request|'s mode to |mode|."
  if (init->hasMode()) {
    network::mojom::RequestMode mode = V8RequestModeToMojom(init->mode());
    if (mode == network::mojom::RequestMode::kNavigate) {
      exception_state.ThrowTypeError(
          "Cannot construct a Request with a RequestInit whose mode member is "
          "set as 'navigate'.");
      return nullptr;
    }
    request->SetMode(mode);
  } else {
    // |inputRequest| is directly checked here instead of setting and
    // checking |fallbackMode| as specified in the spec.
    if (!input_request)
      request->SetMode(network::mojom::RequestMode::kCors);
  }

  // "If |init|'s priority member is present, set |request|'s priority
  // to it." For more information see Priority Hints at
  // https://wicg.github.io/priority-hints/#fetch-integration
  if (init->hasPriority()) {
    UseCounter::Count(execution_context, WebFeature::kPriorityHints);
    if (init->priority() == "low") {
      request->SetFetchPriorityHint(mojom::blink::FetchPriorityHint::kLow);
    } else if (init->priority() == "high") {
      request->SetFetchPriorityHint(mojom::blink::FetchPriorityHint::kHigh);
    }
  }

  // "Let |credentials| be |init|'s credentials member if it is present, and
  // |fallbackCredentials| otherwise."
  // "If |credentials| is non-null, set |request|'s credentials mode to
  // |credentials|."
  if (init->hasCredentials()) {
    request->SetCredentials(
        V8RequestCredentialsToCredentialsMode(init->credentials().AsEnum()));
  } else if (!input_request) {
    request->SetCredentials(network::mojom::CredentialsMode::kSameOrigin);
  }

  // The following code performs the following steps:
  // - "Let |targetAddressSpace| be |init|'s targetAddressSpace member if it is
  // present, and |unknown| otherwise."
  if (init->hasTargetAddressSpace()) {
    if (init->targetAddressSpace() == "local") {
      request->SetTargetAddressSpace(network::mojom::IPAddressSpace::kLocal);
    } else if (init->targetAddressSpace() == "private") {
      request->SetTargetAddressSpace(network::mojom::IPAddressSpace::kPrivate);
    } else if (init->targetAddressSpace() == "public") {
      request->SetTargetAddressSpace(network::mojom::IPAddressSpace::kPublic);
    } else if (init->targetAddressSpace() == "unknown") {
      request->SetTargetAddressSpace(network::mojom::IPAddressSpace::kUnknown);
    }
  } else {
    request->SetTargetAddressSpace(network::mojom::IPAddressSpace::kUnknown);
  }

  // "If |init|'s cache member is present, set |request|'s cache mode to it."
  if (init->hasCache()) {
    auto&& cache = init->cache();
    if (cache == "default") {
      request->SetCacheMode(mojom::blink::FetchCacheMode::kDefault);
    } else if (cache == "no-store") {
      request->SetCacheMode(mojom::blink::FetchCacheMode::kNoStore);
    } else if (cache == "reload") {
      request->SetCacheMode(mojom::blink::FetchCacheMode::kBypassCache);
    } else if (cache == "no-cache") {
      request->SetCacheMode(mojom::blink::FetchCacheMode::kValidateCache);
    } else if (cache == "force-cache") {
      request->SetCacheMode(mojom::blink::FetchCacheMode::kForceCache);
    } else if (cache == "only-if-cached") {
      request->SetCacheMode(mojom::blink::FetchCacheMode::kOnlyIfCached);
    }
  }

  // If |request|’s cache mode is "only-if-cached" and |request|’s mode is not
  // "same-origin", then throw a TypeError.
  if (request->CacheMode() == mojom::blink::FetchCacheMode::kOnlyIfCached &&
      request->Mode() != network::mojom::RequestMode::kSameOrigin) {
    exception_state.ThrowTypeError(
        "'only-if-cached' can be set only with 'same-origin' mode");
    return nullptr;
  }

  // "If |init|'s redirect member is present, set |request|'s redirect mode
  // to it."
  if (init->hasRedirect()) {
    if (init->redirect() == "follow") {
      request->SetRedirect(network::mojom::RedirectMode::kFollow);
    } else if (init->redirect() == "error") {
      request->SetRedirect(network::mojom::RedirectMode::kError);
    } else if (init->redirect() == "manual") {
      request->SetRedirect(network::mojom::RedirectMode::kManual);
    }
  }

  // "If |init|'s integrity member is present, set |request|'s
  // integrity metadata to it."
  if (init->hasIntegrity())
    request->SetIntegrity(init->integrity());

  if (init->hasKeepalive())
    request->SetKeepalive(init->keepalive());

  if (init->hasBrowsingTopics()) {
    if (!execution_context->IsSecureContext()) {
      exception_state.ThrowTypeError(
          "browsingTopics: Topics operations are only available in secure "
          "contexts.");
      return nullptr;
    }

    request->SetBrowsingTopics(init->browsingTopics());

    if (init->browsingTopics()) {
      UseCounter::Count(execution_context,
                        mojom::blink::WebFeature::kTopicsAPIFetch);
      UseCounter::Count(execution_context,
                        mojom::blink::WebFeature::kTopicsAPIAll);
    }
  }

  if (init->hasAdAuctionHeaders()) {
    if (!execution_context->IsSecureContext()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kNotAllowedError,
          "adAuctionHeaders: ad auction operations are only available in "
          "secure contexts.");
      return nullptr;
    }

    request->SetAdAuctionHeaders(init->adAuctionHeaders());
  }

  if (init->hasSharedStorageWritable()) {
    if (!execution_context->IsSecureContext()) {
      exception_state.ThrowTypeError(
          "sharedStorageWritable: sharedStorage operations are only available"
          " in secure contexts.");
      return nullptr;
    }
    if (SecurityOrigin::Create(request->Url())->IsOpaque()) {
      exception_state.ThrowTypeError(
          "sharedStorageWritable: sharedStorage operations are not available"
          " for opaque origins.");
      return nullptr;
    }
    request->SetSharedStorageWritable(init->sharedStorageWritable());
    if (init->sharedStorageWritable()) {
      UseCounter::Count(
          execution_context,
          mojom::blink::WebFeature::kSharedStorageAPI_Fetch_Attribute);
    }
  }

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

  if (init->hasPrivateToken()) {
    UseCounter::Count(ExecutionContext::From(script_state),
                      mojom::blink::WebFeature::kTrustTokenFetch);

    network::mojom::blink::TrustTokenParams params;
    if (!ConvertTrustTokenToMojomAndCheckPermissions(
            *init->privateToken(), GetPSTFeatures(*execution_context),
            &exception_state, &params)) {
      // Whenever parsing the trustToken argument fails, we expect a suitable
      // exception to be thrown.
      DCHECK(exception_state.HadException());
      return nullptr;
    }

    if (!execution_context->IsSecureContext()) {
      exception_state.ThrowTypeError(
          "trustToken: TrustTokens operations are only available in secure "
          "contexts.");
      return nullptr;
    }

    request->SetTrustTokenParams(std::move(params));
  }

  if (init->hasAttributionReporting()) {
    if (!execution_context->IsSecureContext()) {
      exception_state.ThrowTypeError(
          "attributionReporting: Attribution Reporting operations are only "
          "available in secure contexts.");
      return nullptr;
    }

    request->SetAttributionReportingEligibility(
        ConvertAttributionReportingRequestOptionsToMojom(
            *init->attributionReporting(), *execution_context,
            exception_state));
  }

  // "Let  signals  be [|signal|] if  signal  is non-null; otherwise []."
  HeapVector<Member<AbortSignal>> signals;
  if (signal) {
    signals.push_back(signal);
  }
  // "Set |r|'s signal to the result of creating a new dependent abort signal
  // from |signals|".
  auto* request_signal =
      MakeGarbageCollected<AbortSignal>(script_state, signals);

  // "Let |r| be a new Request object associated with |request| and a new
  // Headers object whose guard is "request"."
  Request* r = Request::Create(script_state, request, request_signal);

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

  if (AreAnyMembersPresent(init)) {
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

    // "Fill |r|'s Headers object with |headers|. Rethrow any exceptions."
    if (init->hasHeaders()) {
      r->getHeaders()->FillWith(script_state, init->headers(), exception_state);
    } else {
      DCHECK(headers);
      r->getHeaders()->FillWith(script_state, headers, exception_state);
    }
    if (exception_state.HadException())
      return nullptr;
  }

  // "Let |inputBody| be |input|'s request's body if |input| is a
  //   Request object, and null otherwise."
  BodyStreamBuffer* input_body =
      input_request ? input_request->BodyBuffer() : nullptr;
  uint64_t input_body_byte_length =
      input_request ? input_request->BodyBufferByteLength() : 0;

  // "If either |init|["body"] exists and is non-null or |inputBody| is
  // non-null, and |request|'s method is `GET` or `HEAD`, throw a TypeError.
  v8::Local<v8::Value> init_body =
      init->hasBody() ? init->body().V8Value() : v8::Local<v8::Value>();
  if ((!init_body.IsEmpty() && !init_body->IsNull()) || input_body) {
    if (request->Method() == http_names::kGET ||
        request->Method() == http_names::kHEAD) {
      exception_state.ThrowTypeError(
          "Request with GET/HEAD method cannot have body.");
      return nullptr;
    }
  }

  // "Let |body| be |inputBody|."
  BodyStreamBuffer* body = input_body;
  uint64_t body_byte_length = input_body_byte_length;

  // "If |init|["body"] exists and is non-null, then:"
  if (!init_body.IsEmpty() && !init_body->IsNull()) {
    // - If |init|["keepalive"] exists and is true, then set |body| and
    //   |Content-Type| to the result of extracting |init|["body"], with the
    //   |keepalive| flag set.
    // From "extract a body":
    // - If the keepalive flag is set, then throw a TypeError.
    if (init->hasKeepalive() && init->keepalive() &&
        V8ReadableStream::HasInstance(script_state->GetIsolate(), init_body)) {
      exception_state.ThrowTypeError(
          "Keepalive request cannot have a ReadableStream body.");
      return nullptr;
    }

    // "Otherwise, set |body| and |Content-Type| to the result of extracting
    //  init["body"]."
    String content_type;
    body = ExtractBody(script_state, exception_state, init_body, content_type,
                       body_byte_length);
    // "If |Content-Type| is non-null and |this|'s header's header list
    //  does not contain `Content-Type`, then append
    //   `Content-Type`/|Content-Type| to |this|'s headers object.
    if (!content_type.empty() &&
        !r->getHeaders()->has(http_names::kContentType, exception_state)) {
      r->getHeaders()->append(script_state, http_names::kContentType,
                              content_type, exception_state);
    }
    if (exception_state.HadException())
      return nullptr;
  }

  // "If `inputOrInitBody` is non-null and `inputOrInitBody`’s source is null,
  // then:"
  if (body && body->IsMadeFromReadableStream()) {
    // "If `initBody` is non-null and `init["duplex"]` does not exist, then
    // throw a TypeError."
    if (!init_body.IsEmpty() && !init_body->IsNull() && !init->hasDuplex()) {
      exception_state.ThrowTypeError(
          "The `duplex` member must be specified for a request with a "
          "streaming body");
      return nullptr;
    }

    // "If |this|’s request’s mode is neither "same-origin" nor "cors", then
    // throw a TypeError."
    if (request->Mode() != network::mojom::RequestMode::kSameOrigin &&
        request->Mode() != network::mojom::RequestMode::kCors &&
        request->Mode() !=
            network::mojom::RequestMode::kCorsWithForcedPreflight) {
      exception_state.ThrowTypeError(
          "If request is made from ReadableStream, mode should be"
          "\"same-origin\" or \"cors\"");
      return nullptr;
    }
    // "Set this’s request’s use-CORS-preflight flag."
    request->SetMode(network::mojom::RequestMode::kCorsWithForcedPreflight);
  }

  // "If |inputBody| is |body| and |input| is disturbed or locked, then throw a
  // TypeError."
  if (input_body == body && input_request &&
      (input_request->IsBodyUsed() || input_request->IsBodyLocked())) {
    exception_state.ThrowTypeError(
        "Cannot construct a Request with a Request object that has already "
        "been used.");
    return nullptr;
  }

  // "Set |this|'s request's body to |body|.
  if (body)
    r->request_->SetBuffer(body, body_byte_length);

  // "Set |r|'s MIME type to the result of extracting a MIME type from |r|'s
  // request's header list."
  r->request_->SetMimeType(r->request_->HeaderList()->ExtractMIMEType());

  // "If |input| is a Request object and |input|'s request's body is
  // non-null, run these substeps:"
  if (input_request && input_request->BodyBuffer()) {
    // "Let |dummyStream| be an empty ReadableStream object."
    auto* dummy_stream =
        BodyStreamBuffer::Create(script_state, BytesConsumer::CreateClosed(),
                                 nullptr, /*cached_metadata_handler=*/nullptr);
    // "Set |input|'s request's body to a new body whose stream is
    // |dummyStream|."
    input_request->request_->SetBuffer(dummy_stream);
    // "Let |reader| be the result of getting reader from |dummyStream|."
    // "Read all bytes from |dummyStream| with |reader|."
    input_request->BodyBuffer()->CloseAndLockAndDisturb(exception_state);
  }

  // "Return |r|."
  return r;
}

Request* Request::Create(ScriptState* script_state,
                         const V8RequestInfo* input,
                         const RequestInit* init,
                         ExceptionState& exception_state) {
  DCHECK(input);

  switch (input->GetContentType()) {
    case V8RequestInfo::ContentType::kRequest:
      return Create(script_state, input->GetAsRequest(), init, exception_state);
    case V8RequestInfo::ContentType::kUSVString:
      return Create(script_state, input->GetAsUSVString(), init,
                    exception_state);
  }

  NOTREACHED_IN_MIGRATION();
  return nullptr;
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

Request* Request::Create(ScriptState* script_state,
                         FetchRequestData* request,
                         AbortSignal* signal) {
  return MakeGarbageCollected<Request>(script_state, request, signal);
}

Request* Request::Create(
    ScriptState* script_state,
    mojom::blink::FetchAPIRequestPtr fetch_api_request,
    ForServiceWorkerFetchEvent for_service_worker_fetch_event) {
  FetchRequestData* data =
      FetchRequestData::Create(script_state, std::move(fetch_api_request),
                               for_service_worker_fetch_event);
  auto* signal =
      MakeGarbageCollected<AbortSignal>(ExecutionContext::From(script_state));
  return MakeGarbageCollected<Request>(script_state, data, signal);
}

network::mojom::CredentialsMode Request::V8RequestCredentialsToCredentialsMode(
    V8RequestCredentials::Enum credentials_mode) {
  switch (credentials_mode) {
    case V8RequestCredentials::Enum::kOmit:
      return network::mojom::CredentialsMode::kOmit;
    case V8RequestCredentials::Enum::kSameOrigin:
      return network::mojom::CredentialsMode::kSameOrigin;
    case V8RequestCredentials::Enum::kInclude:
      return network::mojom::CredentialsMode::kInclude;
  }
  NOTREACHED();
}

Request::Request(ScriptState* script_state,
                 FetchRequestData* request,
                 Headers* headers,
                 AbortSignal* signal)
    : Body(ExecutionContext::From(script_state)),
      request_(request),
      headers_(headers),
      signal_(signal) {}

Request::Request(ScriptState* script_state,
                 FetchRequestData* request,
                 AbortSignal* signal)
    : Request(script_state,
              request,
              Headers::Create(request->HeaderList()),
              signal) {
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
  return network::RequestDestinationToString(request_->Destination());
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
  return SecurityPolicy::ReferrerPolicyAsString(request_->GetReferrerPolicy());
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
      return "navigate";
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

String Request::credentials() const {
  // "The credentials attribute's getter must return the value corresponding
  // to the first matching statement, switching on request's credentials
  // mode:"
  switch (request_->Credentials()) {
    case network::mojom::CredentialsMode::kOmit:
    case network::mojom::CredentialsMode::kOmitBug_775438_Workaround:
      return "omit";
    case network::mojom::CredentialsMode::kSameOrigin:
      return "same-origin";
    case network::mojom::CredentialsMode::kInclude:
      return "include";
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

String Request::cache() const {
  // "The cache attribute's getter must return request's cache mode."
  switch (request_->CacheMode()) {
    case mojom::blink::FetchCacheMode::kDefault:
      return "default";
    case mojom::blink::FetchCacheMode::kNoStore:
      return "no-store";
    case mojom::blink::FetchCacheMode::kBypassCache:
      return "reload";
    case mojom::blink::FetchCacheMode::kValidateCache:
      return "no-cache";
    case mojom::blink::FetchCacheMode::kForceCache:
      return "force-cache";
    case mojom::blink::FetchCacheMode::kOnlyIfCached:
      return "only-if-cached";
    case mojom::blink::FetchCacheMode::kUnspecifiedOnlyIfCachedStrict:
    case mojom::blink::FetchCacheMode::kUnspecifiedForceCacheMiss:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  NOTREACHED_IN_MIGRATION();
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
  NOTREACHED_IN_MIGRATION();
  return "";
}

String Request::integrity() const {
  return request_->Integrity();
}

String Request::duplex() const {
  return "half";
}

bool Request::keepalive() const {
  return request_->Keepalive();
}
String Request::targetAddressSpace() const {
  switch (request_->TargetAddressSpace()) {
    case network::mojom::IPAddressSpace::kLocal:
      return "local";
    case network::mojom::IPAddressSpace::kPrivate:
      return "private";
    case network::mojom::IPAddressSpace::kPublic:
      return "public";
    case network::mojom::IPAddressSpace::kUnknown:
      return "unknown";
  }
  NOTREACHED_IN_MIGRATION();
  return "unknown";
}

bool Request::isHistoryNavigation() const {
  return request_->IsHistoryNavigation();
}

Request* Request::clone(ScriptState* script_state,
                        ExceptionState& exception_state) {
  if (IsBodyLocked() || IsBodyUsed()) {
    exception_state.ThrowTypeError("Request body is already used");
    return nullptr;
  }

  FetchRequestData* request = request_->Clone(script_state, exception_state);
  if (exception_state.HadException())
    return nullptr;
  Headers* headers = Headers::Create(request->HeaderList());
  headers->SetGuard(headers_->GetGuard());

  HeapVector<Member<AbortSignal>> signals;
  CHECK(signal_);
  signals.push_back(signal_);
  auto* signal = MakeGarbageCollected<AbortSignal>(script_state, signals);

  return MakeGarbageCollected<Request>(script_state, request, headers, signal);
}

FetchRequestData* Request::PassRequestData(ScriptState* script_state,
                                           ExceptionState& exception_state) {
  DCHECK(!IsBodyUsed());
  FetchRequestData* data = request_->Pass(script_state, exception_state);
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
  fetch_api_request->destination = request_->Destination();
  fetch_api_request->request_initiator = request_->Origin();
  fetch_api_request->url = KURL(request_->Url());

  HTTPHeaderMap headers;
  for (const auto& header : headers_->HeaderList()->List()) {
    if (EqualIgnoringASCIICase(header.first, "referer"))
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

  if (!request_->ReferrerString().empty()) {
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

mojom::blink::RequestContextType Request::GetRequestContextType() const {
  if (!request_) {
    return mojom::blink::RequestContextType::UNSPECIFIED;
  }
  return mojom::blink::RequestContextType::FETCH;
}

network::mojom::RequestDestination Request::GetRequestDestination() const {
  if (!request_) {
    return network::mojom::RequestDestination::kEmpty;
  }
  return request_->Destination();
}

network::mojom::RequestMode Request::GetRequestMode() const {
  return request_->Mode();
}

void Request::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  Body::Trace(visitor);
  visitor->Trace(request_);
  visitor->Trace(headers_);
  visitor->Trace(signal_);
}

}  // namespace blink
