// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/fetch_response_data.h"

#include "third_party/blink/public/mojom/fetch/fetch_api_response.mojom-blink.h"
#include "third_party/blink/renderer/core/fetch/fetch_header_list.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/loader/cors/cors.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_utils.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

using Type = network::mojom::FetchResponseType;
using ResponseSource = network::mojom::FetchResponseSource;

namespace blink {

namespace {

Vector<String> HeaderSetToVector(const HTTPHeaderSet& headers) {
  Vector<String> result;
  result.ReserveInitialCapacity(SafeCast<wtf_size_t>(headers.size()));
  // HTTPHeaderSet stores headers using Latin1 encoding.
  for (const auto& header : headers)
    result.push_back(String(header.data(), header.size()));
  return result;
}

}  // namespace

FetchResponseData* FetchResponseData::Create() {
  // "Unless stated otherwise, a response's url is null, status is 200, status
  // message is the empty byte sequence, header list is an empty header list,
  // and body is null."
  return MakeGarbageCollected<FetchResponseData>(
      Type::kDefault, ResponseSource::kUnspecified, 200, g_empty_atom);
}

FetchResponseData* FetchResponseData::CreateNetworkErrorResponse() {
  // "A network error is a response whose status is always 0, status message
  // is always the empty byte sequence, header list is aways an empty list,
  // and body is always null."
  return MakeGarbageCollected<FetchResponseData>(
      Type::kError, ResponseSource::kUnspecified, 0, g_empty_atom);
}

FetchResponseData* FetchResponseData::CreateWithBuffer(
    BodyStreamBuffer* buffer) {
  FetchResponseData* response = FetchResponseData::Create();
  response->buffer_ = buffer;
  return response;
}

FetchResponseData* FetchResponseData::CreateBasicFilteredResponse() const {
  DCHECK_EQ(type_, Type::kDefault);
  // "A basic filtered response is a filtered response whose type is |basic|,
  // header list excludes any headers in internal response's header list whose
  // name is `Set-Cookie` or `Set-Cookie2`."
  FetchResponseData* response = MakeGarbageCollected<FetchResponseData>(
      Type::kBasic, response_source_, status_, status_message_);
  response->SetURLList(url_list_);
  for (const auto& header : header_list_->List()) {
    if (FetchUtils::IsForbiddenResponseHeaderName(header.first))
      continue;
    response->header_list_->Append(header.first, header.second);
  }
  response->buffer_ = buffer_;
  response->mime_type_ = mime_type_;
  response->internal_response_ = const_cast<FetchResponseData*>(this);
  return response;
}

FetchResponseData* FetchResponseData::CreateCorsFilteredResponse(
    const HTTPHeaderSet& exposed_headers) const {
  DCHECK_EQ(type_, Type::kDefault);
  // "A CORS filtered response is a filtered response whose type is |CORS|,
  // header list excludes all headers in internal response's header list,
  // except those whose name is either one of `Cache-Control`,
  // `Content-Language`, `Content-Type`, `Expires`, `Last-Modified`, and
  // `Pragma`, and except those whose name is one of the values resulting from
  // parsing `Access-Control-Expose-Headers` in internal response's header
  // list."
  FetchResponseData* response = MakeGarbageCollected<FetchResponseData>(
      Type::kCors, response_source_, status_, status_message_);
  response->SetURLList(url_list_);
  for (const auto& header : header_list_->List()) {
    const String& name = header.first;
    if (cors::IsCorsSafelistedResponseHeader(name) ||
        (exposed_headers.find(name.Ascii()) != exposed_headers.end() &&
         !FetchUtils::IsForbiddenResponseHeaderName(name))) {
      response->header_list_->Append(name, header.second);
    }
  }
  response->cors_exposed_header_names_ = exposed_headers;
  response->buffer_ = buffer_;
  response->mime_type_ = mime_type_;
  response->internal_response_ = const_cast<FetchResponseData*>(this);
  return response;
}

FetchResponseData* FetchResponseData::CreateOpaqueFilteredResponse() const {
  DCHECK_EQ(type_, Type::kDefault);
  // "An opaque filtered response is a filtered response whose type is
  // 'opaque', url list is the empty list, status is 0, status message is the
  // empty byte sequence, header list is the empty list, body is null, and
  // cache state is 'none'."
  //
  // https://fetch.spec.whatwg.org/#concept-filtered-response-opaque
  FetchResponseData* response = MakeGarbageCollected<FetchResponseData>(
      Type::kOpaque, response_source_, 0, g_empty_atom);
  response->internal_response_ = const_cast<FetchResponseData*>(this);
  return response;
}

FetchResponseData* FetchResponseData::CreateOpaqueRedirectFilteredResponse()
    const {
  DCHECK_EQ(type_, Type::kDefault);
  // "An opaque filtered response is a filtered response whose type is
  // 'opaqueredirect', status is 0, status message is the empty byte sequence,
  // header list is the empty list, body is null, and cache state is 'none'."
  //
  // https://fetch.spec.whatwg.org/#concept-filtered-response-opaque-redirect
  FetchResponseData* response = MakeGarbageCollected<FetchResponseData>(
      Type::kOpaqueRedirect, response_source_, 0, g_empty_atom);
  response->SetURLList(url_list_);
  response->internal_response_ = const_cast<FetchResponseData*>(this);
  return response;
}

const KURL* FetchResponseData::Url() const {
  // "A response has an associated url. It is a pointer to the last response URL
  // in response’s url list and null if response’s url list is the empty list."
  if (url_list_.IsEmpty())
    return nullptr;
  return &url_list_.back();
}

uint16_t FetchResponseData::InternalStatus() const {
  if (internal_response_) {
    return internal_response_->Status();
  }
  return Status();
}

FetchHeaderList* FetchResponseData::InternalHeaderList() const {
  if (internal_response_) {
    return internal_response_->HeaderList();
  }
  return HeaderList();
}

String FetchResponseData::MimeType() const {
  return mime_type_;
}

BodyStreamBuffer* FetchResponseData::InternalBuffer() const {
  if (internal_response_) {
    return internal_response_->buffer_;
  }
  return buffer_;
}

String FetchResponseData::InternalMIMEType() const {
  if (internal_response_) {
    return internal_response_->MimeType();
  }
  return mime_type_;
}

void FetchResponseData::SetURLList(const Vector<KURL>& url_list) {
  url_list_ = url_list;
}

const Vector<KURL>& FetchResponseData::InternalURLList() const {
  if (internal_response_) {
    return internal_response_->url_list_;
  }
  return url_list_;
}

FetchResponseData* FetchResponseData::Clone(ScriptState* script_state,
                                            ExceptionState& exception_state) {
  FetchResponseData* new_response = Create();
  new_response->type_ = type_;
  new_response->response_source_ = response_source_;
  if (termination_reason_) {
    new_response->termination_reason_ = std::make_unique<TerminationReason>();
    *new_response->termination_reason_ = *termination_reason_;
  }
  new_response->SetURLList(url_list_);
  new_response->status_ = status_;
  new_response->status_message_ = status_message_;
  new_response->header_list_ = header_list_->Clone();
  new_response->mime_type_ = mime_type_;
  new_response->request_method_ = request_method_;
  new_response->response_time_ = response_time_;
  new_response->cache_storage_cache_name_ = cache_storage_cache_name_;
  new_response->cors_exposed_header_names_ = cors_exposed_header_names_;
  new_response->connection_info_ = connection_info_;
  new_response->alpn_negotiated_protocol_ = alpn_negotiated_protocol_;
  new_response->loaded_with_credentials_ = loaded_with_credentials_;
  new_response->was_fetched_via_spdy_ = was_fetched_via_spdy_;
  new_response->has_range_requested_ = has_range_requested_;

  switch (type_) {
    case Type::kBasic:
    case Type::kCors:
      DCHECK(internal_response_);
      DCHECK_EQ(buffer_, internal_response_->buffer_);
      DCHECK_EQ(internal_response_->type_, Type::kDefault);
      new_response->internal_response_ =
          internal_response_->Clone(script_state, exception_state);
      if (exception_state.HadException())
        return nullptr;
      buffer_ = internal_response_->buffer_;
      new_response->buffer_ = new_response->internal_response_->buffer_;
      break;
    case Type::kDefault: {
      DCHECK(!internal_response_);
      if (buffer_) {
        BodyStreamBuffer* new1 = nullptr;
        BodyStreamBuffer* new2 = nullptr;
        buffer_->Tee(&new1, &new2, exception_state);
        if (exception_state.HadException())
          return nullptr;
        buffer_ = new1;
        new_response->buffer_ = new2;
      }
      break;
    }
    case Type::kError:
      DCHECK(!internal_response_);
      DCHECK(!buffer_);
      break;
    case Type::kOpaque:
    case Type::kOpaqueRedirect:
      DCHECK(internal_response_);
      DCHECK(!buffer_);
      DCHECK_EQ(internal_response_->type_, Type::kDefault);
      new_response->internal_response_ =
          internal_response_->Clone(script_state, exception_state);
      if (exception_state.HadException())
        return nullptr;
      break;
  }
  return new_response;
}

mojom::blink::FetchAPIResponsePtr FetchResponseData::PopulateFetchAPIResponse(
    const KURL& request_url) {
  if (internal_response_) {
    mojom::blink::FetchAPIResponsePtr response =
        internal_response_->PopulateFetchAPIResponse(request_url);
    response->response_type = type_;
    response->response_source = response_source_;
    response->cors_exposed_header_names =
        HeaderSetToVector(cors_exposed_header_names_);
    return response;
  }
  mojom::blink::FetchAPIResponsePtr response =
      mojom::blink::FetchAPIResponse::New();
  response->url_list = url_list_;
  response->status_code = status_;
  response->status_text = status_message_;
  response->response_type = type_;
  response->response_source = response_source_;
  response->mime_type = mime_type_;
  response->request_method = request_method_;
  response->response_time = response_time_;
  response->cache_storage_cache_name = cache_storage_cache_name_;
  response->cors_exposed_header_names =
      HeaderSetToVector(cors_exposed_header_names_);
  response->connection_info = connection_info_;
  response->alpn_negotiated_protocol = alpn_negotiated_protocol_;
  response->loaded_with_credentials = loaded_with_credentials_;
  response->was_fetched_via_spdy = was_fetched_via_spdy_;
  response->has_range_requested = has_range_requested_;
  for (const auto& header : HeaderList()->List())
    response->headers.insert(header.first, header.second);
  response->parsed_headers = ParseHeaders(
      HeaderList()->GetAsRawString(status_, status_message_), request_url);
  return response;
}

void FetchResponseData::InitFromResourceResponse(
    const Vector<KURL>& request_url_list,
    const AtomicString& request_method,
    network::mojom::CredentialsMode request_credentials,
    const ResourceResponse& response) {
  SetStatus(response.HttpStatusCode());
  if (response.CurrentRequestUrl().ProtocolIsAbout() ||
      response.CurrentRequestUrl().ProtocolIsData() ||
      response.CurrentRequestUrl().ProtocolIs("blob")) {
    SetStatusMessage("OK");
  } else {
    SetStatusMessage(response.HttpStatusText());
  }

  for (auto& it : response.HttpHeaderFields())
    HeaderList()->Append(it.key, it.value);

  // Corresponds to https://fetch.spec.whatwg.org/#main-fetch step:
  // "If |internalResponse|’s URL list is empty, then set it to a clone of
  // |request|’s URL list."
  if (response.UrlListViaServiceWorker().IsEmpty()) {
    // Note: |UrlListViaServiceWorker()| is empty, unless the response came from
    // a service worker, in which case it will only be empty if it was created
    // through new Response().
    SetURLList(request_url_list);
  } else {
    DCHECK(response.WasFetchedViaServiceWorker());
    SetURLList(response.UrlListViaServiceWorker());
  }

  SetMimeType(response.MimeType());
  SetRequestMethod(request_method);
  SetResponseTime(response.ResponseTime());

  if (response.WasCached()) {
    SetResponseSource(network::mojom::FetchResponseSource::kHttpCache);
  } else if (!response.WasFetchedViaServiceWorker()) {
    SetResponseSource(network::mojom::FetchResponseSource::kNetwork);
  }

  SetConnectionInfo(response.ConnectionInfo());

  // Some non-http responses, like data: url responses, will have a null
  // |alpn_negotiated_protocol|.  In these cases we leave the default
  // value of "unknown".
  if (!response.AlpnNegotiatedProtocol().IsNull())
    SetAlpnNegotiatedProtocol(response.AlpnNegotiatedProtocol());

  SetWasFetchedViaSpdy(response.WasFetchedViaSPDY());

  SetLoadedWithCredentials(
      request_credentials == network::mojom::CredentialsMode::kInclude ||
      (request_credentials == network::mojom::CredentialsMode::kSameOrigin &&
       (response.GetType() ==
            network::mojom::blink::FetchResponseType::kBasic ||
        response.GetType() ==
            network::mojom::blink::FetchResponseType::kDefault)));

  SetHasRangeRequested(response.HasRangeRequested());
}

FetchResponseData::FetchResponseData(Type type,
                                     network::mojom::FetchResponseSource source,
                                     uint16_t status,
                                     AtomicString status_message)
    : type_(type),
      response_source_(source),
      status_(status),
      status_message_(status_message),
      header_list_(MakeGarbageCollected<FetchHeaderList>()),
      response_time_(base::Time::Now()),
      connection_info_(net::HttpResponseInfo::CONNECTION_INFO_UNKNOWN),
      alpn_negotiated_protocol_("unknown"),
      loaded_with_credentials_(false),
      was_fetched_via_spdy_(false),
      has_range_requested_(false) {}

void FetchResponseData::ReplaceBodyStreamBuffer(BodyStreamBuffer* buffer) {
  if (type_ == Type::kBasic || type_ == Type::kCors) {
    DCHECK(internal_response_);
    internal_response_->buffer_ = buffer;
    buffer_ = buffer;
  } else if (type_ == Type::kDefault) {
    DCHECK(!internal_response_);
    buffer_ = buffer;
  }
}

void FetchResponseData::Trace(Visitor* visitor) const {
  visitor->Trace(header_list_);
  visitor->Trace(internal_response_);
  visitor->Trace(buffer_);
}

}  // namespace blink
