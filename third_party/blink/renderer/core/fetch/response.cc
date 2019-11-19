// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/response.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_response.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_array_buffer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_array_buffer_view.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_blob.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_form_data.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_readable_stream.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_url_search_params.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/blob_bytes_consumer.h"
#include "third_party/blink/renderer/core/fetch/body_stream_buffer.h"
#include "third_party/blink/renderer/core/fetch/form_data_bytes_consumer.h"
#include "third_party/blink/renderer/core/fetch/response_init.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/core/url/url_search_params.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/cors/cors.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_utils.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/network/http_header_map.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/network_utils.h"

namespace blink {

namespace {

template <typename CorsHeadersContainer>
FetchResponseData* FilterResponseDataInternal(
    network::mojom::FetchResponseType response_type,
    FetchResponseData* response,
    CorsHeadersContainer& headers) {
  switch (response_type) {
    case network::mojom::FetchResponseType::kBasic:
      return response->CreateBasicFilteredResponse();
      break;
    case network::mojom::FetchResponseType::kCors: {
      WebHTTPHeaderSet header_names;
      for (const auto& header : headers)
        header_names.insert(header.Ascii());
      return response->CreateCorsFilteredResponse(header_names);
      break;
    }
    case network::mojom::FetchResponseType::kOpaque:
      return response->CreateOpaqueFilteredResponse();
      break;
    case network::mojom::FetchResponseType::kOpaqueRedirect:
      return response->CreateOpaqueRedirectFilteredResponse();
      break;
    case network::mojom::FetchResponseType::kDefault:
      return response;
      break;
    case network::mojom::FetchResponseType::kError:
      DCHECK_EQ(response->GetType(), network::mojom::FetchResponseType::kError);
      return response;
      break;
  }
  return response;
}

FetchResponseData* CreateFetchResponseDataFromFetchAPIResponse(
    ScriptState* script_state,
    mojom::blink::FetchAPIResponse& fetch_api_response) {
  FetchResponseData* response =
      Response::CreateUnfilteredFetchResponseDataWithoutBody(
          script_state, fetch_api_response);

  if (fetch_api_response.blob) {
    response->ReplaceBodyStreamBuffer(MakeGarbageCollected<BodyStreamBuffer>(
        script_state,
        MakeGarbageCollected<BlobBytesConsumer>(
            ExecutionContext::From(script_state), fetch_api_response.blob),
        nullptr /* AbortSignal */));
  }

  // Filter the response according to |fetch_api_response|'s ResponseType.
  response =
      FilterResponseDataInternal(fetch_api_response.response_type, response,
                                 fetch_api_response.cors_exposed_header_names);

  return response;
}

// Checks whether |status| is a null body status.
// Spec: https://fetch.spec.whatwg.org/#null-body-status
bool IsNullBodyStatus(uint16_t status) {
  if (status == 101 || status == 204 || status == 205 || status == 304)
    return true;

  return false;
}

// Check whether |statusText| is a ByteString and
// matches the Reason-Phrase token production.
// RFC 2616: https://tools.ietf.org/html/rfc2616
// RFC 7230: https://tools.ietf.org/html/rfc7230
// "reason-phrase = *( HTAB / SP / VCHAR / obs-text )"
bool IsValidReasonPhrase(const String& status_text) {
  for (unsigned i = 0; i < status_text.length(); ++i) {
    UChar c = status_text[i];
    if (!(c == 0x09                      // HTAB
          || (0x20 <= c && c <= 0x7E)    // SP / VCHAR
          || (0x80 <= c && c <= 0xFF)))  // obs-text
      return false;
  }
  return true;
}

}  // namespace

Response* Response::Create(ScriptState* script_state,
                           ExceptionState& exception_state) {
  return Create(script_state, nullptr, String(), ResponseInit::Create(),
                exception_state);
}

Response* Response::Create(ScriptState* script_state,
                           ScriptValue body_value,
                           const ResponseInit* init,
                           ExceptionState& exception_state) {
  v8::Local<v8::Value> body = body_value.V8Value();
  v8::Isolate* isolate = script_state->GetIsolate();
  ExecutionContext* execution_context = ExecutionContext::From(script_state);

  BodyStreamBuffer* body_buffer = nullptr;
  String content_type;
  if (body_value.IsUndefined() || body_value.IsNull()) {
    // Note: The IDL processor cannot handle this situation. See
    // https://crbug.com/335871.
  } else if (V8Blob::HasInstance(body, isolate)) {
    Blob* blob = V8Blob::ToImpl(body.As<v8::Object>());
    body_buffer = MakeGarbageCollected<BodyStreamBuffer>(
        script_state,
        MakeGarbageCollected<BlobBytesConsumer>(execution_context,
                                                blob->GetBlobDataHandle()),
        nullptr /* AbortSignal */);
    content_type = blob->type();
  } else if (body->IsArrayBuffer()) {
    // Avoid calling into V8 from the following constructor parameters, which
    // is potentially unsafe.
    DOMArrayBuffer* array_buffer = V8ArrayBuffer::ToImpl(body.As<v8::Object>());
    body_buffer = MakeGarbageCollected<BodyStreamBuffer>(
        script_state, MakeGarbageCollected<FormDataBytesConsumer>(array_buffer),
        nullptr /* AbortSignal */);
  } else if (body->IsArrayBufferView()) {
    // Avoid calling into V8 from the following constructor parameters, which
    // is potentially unsafe.
    DOMArrayBufferView* array_buffer_view =
        V8ArrayBufferView::ToImpl(body.As<v8::Object>());
    body_buffer = MakeGarbageCollected<BodyStreamBuffer>(
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
    body_buffer = MakeGarbageCollected<BodyStreamBuffer>(
        script_state,
        MakeGarbageCollected<FormDataBytesConsumer>(execution_context,
                                                    std::move(form_data)),
        nullptr /* AbortSignal */);
  } else if (V8URLSearchParams::HasInstance(body, isolate)) {
    scoped_refptr<EncodedFormData> form_data =
        V8URLSearchParams::ToImpl(body.As<v8::Object>())->ToEncodedFormData();
    body_buffer = MakeGarbageCollected<BodyStreamBuffer>(
        script_state,
        MakeGarbageCollected<FormDataBytesConsumer>(execution_context,
                                                    std::move(form_data)),
        nullptr /* AbortSignal */);
    content_type = "application/x-www-form-urlencoded;charset=UTF-8";
  } else if (V8ReadableStream::HasInstance(body, isolate)) {
    UseCounter::Count(execution_context,
                      WebFeature::kFetchResponseConstructionWithStream);
    body_buffer = MakeGarbageCollected<BodyStreamBuffer>(
        script_state, V8ReadableStream::ToImpl(body.As<v8::Object>()));
  } else {
    String string = NativeValueTraits<IDLUSVString>::NativeValue(
        isolate, body, exception_state);
    if (exception_state.HadException())
      return nullptr;
    body_buffer = MakeGarbageCollected<BodyStreamBuffer>(
        script_state, MakeGarbageCollected<FormDataBytesConsumer>(string),
        nullptr /* AbortSignal */);
    content_type = "text/plain;charset=UTF-8";
  }
  return Create(script_state, body_buffer, content_type, init, exception_state);
}

Response* Response::Create(ScriptState* script_state,
                           BodyStreamBuffer* body,
                           const String& content_type,
                           const ResponseInit* init,
                           ExceptionState& exception_state) {
  uint16_t status = init->status();

  // "1. If |init|'s status member is not in the range 200 to 599, inclusive,
  // throw a RangeError."
  if (status < 200 || 599 < status) {
    exception_state.ThrowRangeError(
        ExceptionMessages::IndexOutsideRange<unsigned>(
            "status", status, 200, ExceptionMessages::kInclusiveBound, 599,
            ExceptionMessages::kInclusiveBound));
    return nullptr;
  }

  // "2. If |init|'s statusText member does not match the Reason-Phrase
  // token production, throw a TypeError."
  if (!IsValidReasonPhrase(init->statusText())) {
    exception_state.ThrowTypeError("Invalid statusText");
    return nullptr;
  }

  // "3. Let |r| be a new Response object, associated with a new response.
  // "4. Set |r|'s headers to a new Headers object whose list is
  // |r|'s response's header list, and guard is "response" "
  Response* r =
      MakeGarbageCollected<Response>(ExecutionContext::From(script_state));
  // "5. Set |r|'s response's status to |init|'s status member."
  r->response_->SetStatus(init->status());

  // "6. Set |r|'s response's status message to |init|'s statusText member."
  r->response_->SetStatusMessage(AtomicString(init->statusText()));

  // "7. If |init|'s headers exists, then fill |r|’s headers with
  // |init|'s headers"
  if (init->hasHeaders()) {
    // "1. Empty |r|'s response's header list."
    r->response_->HeaderList()->ClearList();
    // "2. Fill |r|'s Headers object with |init|'s headers member. Rethrow
    // any exceptions."
    r->headers_->FillWith(init->headers(), exception_state);
    if (exception_state.HadException())
      return nullptr;
  }
  // "8. If body is non-null, then:"
  if (body) {
    // "1. If |init|'s status is a null body status, then throw a TypeError."
    if (IsNullBodyStatus(status)) {
      exception_state.ThrowTypeError(
          "Response with null body status cannot have body");
      return nullptr;
    }
    // "2. Let |Content-Type| be null."
    // "3. Set |r|'s response's body and |Content-Type|
    // to the result of extracting body."
    // https://fetch.spec.whatwg.org/#concept-bodyinit-extract
    // Step 5, Blob:
    // "If object's type attribute is not the empty byte sequence, set
    // Content-Type to its value."
    r->response_->ReplaceBodyStreamBuffer(body);

    // https://fetch.spec.whatwg.org/#concept-bodyinit-extract
    // Step 5, ReadableStream:
    // "If object is disturbed or locked, then throw a TypeError."
    // If the BodyStreamBuffer was not constructed from a ReadableStream
    // then IsStreamLocked and IsStreamDisturbed will always be false.
    // So we don't have to check BodyStreamBuffer is a ReadableStream
    // or not.
    if (body->IsStreamLocked(exception_state).value_or(true) ||
        body->IsStreamDisturbed(exception_state).value_or(true)) {
      if (!exception_state.HadException()) {
        exception_state.ThrowTypeError(
            "Response body object should not be disturbed or locked");
      }
      return nullptr;
    }

    // "4. If |Content-Type| is non-null and |r|'s response's header list
    // contains no header named `Content-Type`, append `Content-Type`/
    // |Content-Type| to |r|'s response's header list."
    if (!content_type.IsEmpty() &&
        !r->response_->HeaderList()->Has("Content-Type"))
      r->response_->HeaderList()->Append("Content-Type", content_type);
  }

  // "9. Set |r|'s MIME type to the result of extracting a MIME type
  // from |r|'s response's header list."
  r->response_->SetMimeType(r->response_->HeaderList()->ExtractMIMEType());

  // "10. Set |r|'s response’s HTTPS state to current settings object's"
  // HTTPS state."
  // "11. Resolve |r|'s trailer promise with a new Headers object whose
  // guard is "immutable"."
  // "12. Return |r|."
  return r;
}

Response* Response::Create(ExecutionContext* context,
                           FetchResponseData* response) {
  return MakeGarbageCollected<Response>(context, response);
}

Response* Response::Create(ScriptState* script_state,
                           mojom::blink::FetchAPIResponse& response) {
  auto* fetch_response_data =
      CreateFetchResponseDataFromFetchAPIResponse(script_state, response);
  return MakeGarbageCollected<Response>(ExecutionContext::From(script_state),
                                        fetch_response_data);
}

Response* Response::error(ScriptState* script_state) {
  FetchResponseData* response_data =
      FetchResponseData::CreateNetworkErrorResponse();
  Response* r = MakeGarbageCollected<Response>(
      ExecutionContext::From(script_state), response_data);
  r->headers_->SetGuard(Headers::kImmutableGuard);
  return r;
}

Response* Response::redirect(ScriptState* script_state,
                             const String& url,
                             uint16_t status,
                             ExceptionState& exception_state) {
  KURL parsed_url = ExecutionContext::From(script_state)->CompleteURL(url);
  if (!parsed_url.IsValid()) {
    exception_state.ThrowTypeError("Failed to parse URL from " + url);
    return nullptr;
  }

  if (!network_utils::IsRedirectResponseCode(status)) {
    exception_state.ThrowRangeError("Invalid status code");
    return nullptr;
  }

  Response* r =
      MakeGarbageCollected<Response>(ExecutionContext::From(script_state));
  r->headers_->SetGuard(Headers::kImmutableGuard);
  r->response_->SetStatus(status);
  r->response_->HeaderList()->Set("Location", parsed_url);

  return r;
}

FetchResponseData* Response::CreateUnfilteredFetchResponseDataWithoutBody(
    ScriptState* script_state,
    mojom::blink::FetchAPIResponse& fetch_api_response) {
  FetchResponseData* response = nullptr;
  if (fetch_api_response.status_code > 0)
    response = FetchResponseData::Create();
  else
    response = FetchResponseData::CreateNetworkErrorResponse();

  response->SetResponseSource(fetch_api_response.response_source);
  response->SetURLList(fetch_api_response.url_list);
  response->SetStatus(fetch_api_response.status_code);
  response->SetStatusMessage(WTF::AtomicString(fetch_api_response.status_text));
  response->SetResponseTime(fetch_api_response.response_time);
  response->SetCacheStorageCacheName(
      fetch_api_response.cache_storage_cache_name);
  response->SetSideDataBlob(fetch_api_response.side_data_blob);

  for (const auto& header : fetch_api_response.headers)
    response->HeaderList()->Append(header.key, header.value);

  // TODO(wanderview): This sets the mime type of the Response based on the
  // current headers.  This should be correct for most cases, but technically
  // the mime type should really be frozen at the initial Response
  // construction.  We should plumb the value through the cache_storage
  // persistence layer and include the explicit mime type in FetchAPIResponse
  // to set here. See: crbug.com/938939
  response->SetMimeType(response->HeaderList()->ExtractMIMEType());

  return response;
}

FetchResponseData* Response::FilterResponseData(
    network::mojom::FetchResponseType response_type,
    FetchResponseData* response,
    WTF::Vector<WTF::String>& headers) {
  return FilterResponseDataInternal(response_type, response, headers);
}

String Response::type() const {
  // "The type attribute's getter must return response's type."
  switch (response_->GetType()) {
    case network::mojom::FetchResponseType::kBasic:
      return "basic";
    case network::mojom::FetchResponseType::kCors:
      return "cors";
    case network::mojom::FetchResponseType::kDefault:
      return "default";
    case network::mojom::FetchResponseType::kError:
      return "error";
    case network::mojom::FetchResponseType::kOpaque:
      return "opaque";
    case network::mojom::FetchResponseType::kOpaqueRedirect:
      return "opaqueredirect";
  }
  NOTREACHED();
  return "";
}

String Response::url() const {
  // "The url attribute's getter must return the empty string if response's
  // url is null and response's url, serialized with the exclude fragment
  // flag set, otherwise."
  const KURL* response_url = response_->Url();
  if (!response_url)
    return g_empty_string;
  if (!response_url->HasFragmentIdentifier())
    return *response_url;
  KURL url(*response_url);
  url.RemoveFragmentIdentifier();
  return url;
}

bool Response::redirected() const {
  return response_->UrlList().size() > 1;
}

uint16_t Response::status() const {
  // "The status attribute's getter must return response's status."
  return response_->Status();
}

bool Response::ok() const {
  // "The ok attribute's getter must return true
  // if response's status is in the range 200 to 299, and false otherwise."
  return cors::IsOkStatus(status());
}

String Response::statusText() const {
  // "The statusText attribute's getter must return response's status message."
  return response_->StatusMessage();
}

Headers* Response::headers() const {
  // "The headers attribute's getter must return the associated Headers object."
  return headers_;
}

Response* Response::clone(ScriptState* script_state,
                          ExceptionState& exception_state) {
  if (IsBodyLocked(exception_state) == BodyLocked::kLocked ||
      IsBodyUsed(exception_state) == BodyUsed::kUsed) {
    DCHECK(!exception_state.HadException());
    exception_state.ThrowTypeError("Response body is already used");
    return nullptr;
  }

  if (exception_state.HadException())
    return nullptr;

  FetchResponseData* response = response_->Clone(script_state, exception_state);
  if (exception_state.HadException())
    return nullptr;
  Headers* headers = Headers::Create(response->HeaderList());
  headers->SetGuard(headers_->GetGuard());
  return MakeGarbageCollected<Response>(GetExecutionContext(), response,
                                        headers);
}

bool Response::HasPendingActivity() const {
  if (!GetExecutionContext() || GetExecutionContext()->IsContextDestroyed())
    return false;
  if (!InternalBodyBuffer())
    return false;
  if (InternalBodyBuffer()->HasPendingActivity())
    return true;
  return Body::HasPendingActivity();
}

mojom::blink::FetchAPIResponsePtr Response::PopulateFetchAPIResponse(
    const KURL& request_url) {
  return response_->PopulateFetchAPIResponse(request_url);
}

Response::Response(ExecutionContext* context)
    : Response(context, FetchResponseData::Create()) {}

Response::Response(ExecutionContext* context, FetchResponseData* response)
    : Response(context, response, Headers::Create(response->HeaderList())) {
  headers_->SetGuard(Headers::kResponseGuard);
}

Response::Response(ExecutionContext* context,
                   FetchResponseData* response,
                   Headers* headers)
    : Body(context), response_(response), headers_(headers) {}

bool Response::HasBody() const {
  return response_->InternalBuffer();
}

Body::BodyUsed Response::IsBodyUsed(ExceptionState& exception_state) {
  auto* body_buffer = InternalBodyBuffer();
  if (!body_buffer)
    return BodyUsed::kUnused;
  base::Optional<bool> stream_disturbed =
      body_buffer->IsStreamDisturbed(exception_state);
  if (exception_state.HadException())
    return BodyUsed::kBroken;
  DCHECK(stream_disturbed.has_value());
  return stream_disturbed.value() ? BodyUsed::kUsed : BodyUsed::kUnused;
}

String Response::MimeType() const {
  return response_->MimeType();
}

String Response::ContentType() const {
  String result;
  response_->HeaderList()->Get(http_names::kContentType, result);
  return result;
}

String Response::InternalMIMEType() const {
  return response_->InternalMIMEType();
}

const Vector<KURL>& Response::InternalURLList() const {
  return response_->InternalURLList();
}

FetchHeaderList* Response::InternalHeaderList() const {
  return response_->InternalHeaderList();
}

void Response::Trace(blink::Visitor* visitor) {
  Body::Trace(visitor);
  visitor->Trace(response_);
  visitor->Trace(headers_);
}

bool Response::IsBodyUsedForDCheck(ExceptionState& exception_state) {
  return InternalBodyBuffer() &&
         InternalBodyBuffer()->IsStreamDisturbedForDCheck(exception_state);
}

}  // namespace blink
