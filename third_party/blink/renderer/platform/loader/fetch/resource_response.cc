/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"

#include <string>

#include "base/memory/scoped_refptr.h"
#include "net/http/structured_headers.h"
#include "net/ssl/ssl_info.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/timing/resource_timing.mojom-blink.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_timing.h"
#include "third_party/blink/renderer/platform/loader/fetch/service_worker_router_info.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

template <typename Interface>
Vector<Interface> IsolatedCopy(const Vector<Interface>& src) {
  Vector<Interface> result;
  result.reserve(src.size());
  for (const auto& timestamp : src) {
    result.push_back(timestamp.IsolatedCopy());
  }
  return result;
}

}  // namespace

ResourceResponse::ResourceResponse()
    : was_cached_(false),
      connection_reused_(false),
      is_null_(true),
      have_parsed_age_header_(false),
      have_parsed_date_header_(false),
      have_parsed_expires_header_(false),
      have_parsed_last_modified_header_(false),
      has_major_certificate_errors_(false),
      has_range_requested_(false),
      timing_allow_passed_(false),
      was_fetched_via_spdy_(false),
      was_fetched_via_service_worker_(false),
      did_service_worker_navigation_preload_(false),
      did_use_shared_dictionary_(false),
      async_revalidation_requested_(false),
      is_signed_exchange_inner_response_(false),
      is_web_bundle_inner_response_(false),
      was_in_prefetch_cache_(false),
      was_cookie_in_request_(false),
      network_accessed_(false),
      from_archive_(false),
      was_alternate_protocol_available_(false),
      was_alpn_negotiated_(false),
      has_authorization_covered_by_wildcard_on_preflight_(false),
      is_validated_(false),
      request_include_credentials_(true),
      should_use_source_hash_for_js_code_cache_(false) {}

ResourceResponse::ResourceResponse(const KURL& current_request_url)
    : ResourceResponse() {
  SetCurrentRequestUrl(current_request_url);
}

ResourceResponse::ResourceResponse(const ResourceResponse&) = default;
ResourceResponse& ResourceResponse::operator=(const ResourceResponse&) =
    default;

ResourceResponse::~ResourceResponse() = default;

bool ResourceResponse::IsHTTP() const {
  return current_request_url_.ProtocolIsInHTTPFamily();
}

bool ResourceResponse::ShouldPopulateResourceTiming() const {
  return IsHTTP() || is_web_bundle_inner_response_;
}

const KURL& ResourceResponse::CurrentRequestUrl() const {
  return current_request_url_;
}

void ResourceResponse::SetCurrentRequestUrl(const KURL& url) {
  is_null_ = false;

  current_request_url_ = url;
}

KURL ResourceResponse::ResponseUrl() const {
  // Ideally ResourceResponse would have a |url_list_| to match Fetch
  // specification's URL list concept
  // (https://fetch.spec.whatwg.org/#concept-response-url-list), and its
  // last element would be returned here.
  //
  // Instead it has |url_list_via_service_worker_| which is only populated when
  // the response came from a service worker, and that response was not created
  // through `new Response()`. Use it when available.
  if (!url_list_via_service_worker_.empty()) {
    DCHECK(WasFetchedViaServiceWorker());
    return url_list_via_service_worker_.back();
  }

  // Otherwise, use the current request URL. This is OK because the Fetch
  // specification's "main fetch" algorithm[1] sets the response URL list to the
  // request's URL list when the list isn't present. That step can't be
  // implemented now because there is no |url_list_| memeber, but effectively
  // the same thing happens by returning CurrentRequestUrl() here.
  //
  // [1] "If internalResponse’s URL list is empty, then set it to a clone of
  // request’s URL list." at
  // https://fetch.spec.whatwg.org/#ref-for-concept-response-url-list%E2%91%A4
  return CurrentRequestUrl();
}

bool ResourceResponse::IsServiceWorkerPassThrough() const {
  return cache_storage_cache_name_.empty() &&
         !url_list_via_service_worker_.empty() &&
         ResponseUrl() == CurrentRequestUrl();
}

const AtomicString& ResourceResponse::MimeType() const {
  return mime_type_;
}

void ResourceResponse::SetMimeType(const AtomicString& mime_type) {
  is_null_ = false;

  // FIXME: MIME type is determined by HTTP Content-Type header. We should
  // update the header, so that it doesn't disagree with m_mimeType.
  mime_type_ = mime_type;
}

int64_t ResourceResponse::ExpectedContentLength() const {
  return expected_content_length_;
}

void ResourceResponse::SetExpectedContentLength(
    int64_t expected_content_length) {
  is_null_ = false;

  // FIXME: Content length is determined by HTTP Content-Length header. We
  // should update the header, so that it doesn't disagree with
  // m_expectedContentLength.
  expected_content_length_ = expected_content_length;
}

const AtomicString& ResourceResponse::TextEncodingName() const {
  return text_encoding_name_;
}

void ResourceResponse::SetTextEncodingName(const AtomicString& encoding_name) {
  is_null_ = false;

  // FIXME: Text encoding is determined by HTTP Content-Type header. We should
  // update the header, so that it doesn't disagree with m_textEncodingName.
  text_encoding_name_ = encoding_name;
}

int ResourceResponse::HttpStatusCode() const {
  return http_status_code_;
}

void ResourceResponse::SetHttpStatusCode(int status_code) {
  http_status_code_ = status_code;
}

const AtomicString& ResourceResponse::HttpStatusText() const {
  return http_status_text_;
}

void ResourceResponse::SetHttpStatusText(const AtomicString& status_text) {
  http_status_text_ = status_text;
}

const AtomicString& ResourceResponse::HttpHeaderField(
    const AtomicString& name) const {
  return http_header_fields_.Get(name);
}

void ResourceResponse::UpdateHeaderParsedState(const AtomicString& name) {
  if (EqualIgnoringASCIICase(name, http_names::kLowerAge)) {
    have_parsed_age_header_ = false;
  } else if (EqualIgnoringASCIICase(name, http_names::kLowerCacheControl) ||
             EqualIgnoringASCIICase(name, http_names::kLowerPragma)) {
    cache_control_header_ = CacheControlHeader();
  } else if (EqualIgnoringASCIICase(name, http_names::kLowerDate)) {
    have_parsed_date_header_ = false;
  } else if (EqualIgnoringASCIICase(name, http_names::kLowerExpires)) {
    have_parsed_expires_header_ = false;
  } else if (EqualIgnoringASCIICase(name, http_names::kLowerLastModified)) {
    have_parsed_last_modified_header_ = false;
  }
}

void ResourceResponse::SetSSLInfo(const net::SSLInfo& ssl_info) {
  DCHECK_NE(security_style_, SecurityStyle::kUnknown);
  DCHECK_NE(security_style_, SecurityStyle::kNeutral);
  ssl_info_ = ssl_info;
}

void ResourceResponse::SetServiceWorkerRouterInfo(
    scoped_refptr<ServiceWorkerRouterInfo> value) {
  service_worker_router_info_ = std::move(value);
}

bool ResourceResponse::IsCorsSameOrigin() const {
  return network::cors::IsCorsSameOriginResponseType(response_type_);
}

bool ResourceResponse::IsCorsCrossOrigin() const {
  return network::cors::IsCorsCrossOriginResponseType(response_type_);
}

void ResourceResponse::SetHttpHeaderField(const AtomicString& name,
                                          const AtomicString& value) {
  UpdateHeaderParsedState(name);

  http_header_fields_.Set(name, value);
}

void ResourceResponse::AddHttpHeaderField(const AtomicString& name,
                                          const AtomicString& value) {
  UpdateHeaderParsedState(name);

  HTTPHeaderMap::AddResult result = http_header_fields_.Add(name, value);
  if (!result.is_new_entry)
    result.stored_value->value = result.stored_value->value + ", " + value;
}

void ResourceResponse::AddHttpHeaderFieldWithMultipleValues(
    const AtomicString& name,
    const Vector<AtomicString>& values) {
  if (values.empty())
    return;

  UpdateHeaderParsedState(name);

  StringBuilder value_builder;
  const auto it = http_header_fields_.Find(name);
  if (it != http_header_fields_.end())
    value_builder.Append(it->value);
  for (const auto& value : values) {
    if (!value_builder.empty())
      value_builder.Append(", ");
    value_builder.Append(value);
  }
  http_header_fields_.Set(name, value_builder.ToAtomicString());
}

void ResourceResponse::ClearHttpHeaderField(const AtomicString& name) {
  http_header_fields_.Remove(name);
}

const HTTPHeaderMap& ResourceResponse::HttpHeaderFields() const {
  return http_header_fields_;
}

bool ResourceResponse::CacheControlContainsNoCache() const {
  if (!cache_control_header_.parsed) {
    cache_control_header_ = ParseCacheControlDirectives(
        http_header_fields_.Get(http_names::kLowerCacheControl),
        http_header_fields_.Get(http_names::kLowerPragma));
  }
  return cache_control_header_.contains_no_cache;
}

bool ResourceResponse::CacheControlContainsNoStore() const {
  if (!cache_control_header_.parsed) {
    cache_control_header_ = ParseCacheControlDirectives(
        http_header_fields_.Get(http_names::kLowerCacheControl),
        http_header_fields_.Get(http_names::kLowerPragma));
  }
  return cache_control_header_.contains_no_store;
}

bool ResourceResponse::CacheControlContainsMustRevalidate() const {
  if (!cache_control_header_.parsed) {
    cache_control_header_ = ParseCacheControlDirectives(
        http_header_fields_.Get(http_names::kLowerCacheControl),
        http_header_fields_.Get(http_names::kLowerPragma));
  }
  return cache_control_header_.contains_must_revalidate;
}

bool ResourceResponse::HasCacheValidatorFields() const {
  return !http_header_fields_.Get(http_names::kLowerLastModified).empty() ||
         !http_header_fields_.Get(http_names::kLowerETag).empty();
}

std::optional<base::TimeDelta> ResourceResponse::CacheControlMaxAge() const {
  if (!cache_control_header_.parsed) {
    cache_control_header_ = ParseCacheControlDirectives(
        http_header_fields_.Get(http_names::kLowerCacheControl),
        http_header_fields_.Get(http_names::kLowerPragma));
  }
  return cache_control_header_.max_age;
}

base::TimeDelta ResourceResponse::CacheControlStaleWhileRevalidate() const {
  if (!cache_control_header_.parsed) {
    cache_control_header_ = ParseCacheControlDirectives(
        http_header_fields_.Get(http_names::kLowerCacheControl),
        http_header_fields_.Get(http_names::kLowerPragma));
  }
  if (!cache_control_header_.stale_while_revalidate ||
      cache_control_header_.stale_while_revalidate.value() <
          base::TimeDelta()) {
    return base::TimeDelta();
  }
  return cache_control_header_.stale_while_revalidate.value();
}

static std::optional<base::Time> ParseDateValueInHeader(
    const HTTPHeaderMap& headers,
    const AtomicString& header_name) {
  const AtomicString& header_value = headers.Get(header_name);
  if (header_value.empty())
    return std::nullopt;

  // In case of parsing the Expires header value, an invalid string 0 should be
  // treated as expired according to the RFC 9111 section 5.3 as below:
  //
  // > A cache recipient MUST interpret invalid date formats, especially the
  // > value "0", as representing a time in the past (i.e., "already expired").
  if (base::FeatureList::IsEnabled(
          blink::features::kTreatHTTPExpiresHeaderValueZeroAsExpiredInBlink) &&
      header_name == http_names::kLowerExpires && header_value == "0") {
    return base::Time::Min();
  }

  // This handles all date formats required by RFC2616:
  // Sun, 06 Nov 1994 08:49:37 GMT  ; RFC 822, updated by RFC 1123
  // Sunday, 06-Nov-94 08:49:37 GMT ; RFC 850, obsoleted by RFC 1036
  // Sun Nov  6 08:49:37 1994       ; ANSI C's asctime() format
  std::optional<base::Time> date = ParseDate(header_value);

  if (date && date.value().is_max())
    return std::nullopt;
  return date;
}

std::optional<base::Time> ResourceResponse::Date() const {
  if (!have_parsed_date_header_) {
    date_ = ParseDateValueInHeader(http_header_fields_, http_names::kLowerDate);
    have_parsed_date_header_ = true;
  }
  return date_;
}

std::optional<base::TimeDelta> ResourceResponse::Age() const {
  if (!have_parsed_age_header_) {
    const AtomicString& header_value =
        http_header_fields_.Get(http_names::kLowerAge);
    bool ok;
    double seconds = header_value.ToDouble(&ok);
    if (!ok) {
      age_ = std::nullopt;
    } else {
      age_ = base::Seconds(seconds);
    }
    have_parsed_age_header_ = true;
  }
  return age_;
}

std::optional<base::Time> ResourceResponse::Expires() const {
  if (!have_parsed_expires_header_) {
    expires_ =
        ParseDateValueInHeader(http_header_fields_, http_names::kLowerExpires);
    have_parsed_expires_header_ = true;
  }
  return expires_;
}

std::optional<base::Time> ResourceResponse::LastModified() const {
  if (!have_parsed_last_modified_header_) {
    last_modified_ = ParseDateValueInHeader(http_header_fields_,
                                            http_names::kLowerLastModified);
    have_parsed_last_modified_header_ = true;
  }
  return last_modified_;
}

bool ResourceResponse::IsAttachment() const {
  static const char kAttachmentString[] = "attachment";
  String value = http_header_fields_.Get(http_names::kContentDisposition);
  wtf_size_t loc = value.find(';');
  if (loc != kNotFound)
    value = value.Left(loc);
  value = value.StripWhiteSpace();
  return EqualIgnoringASCIICase(value, kAttachmentString);
}

AtomicString ResourceResponse::HttpContentType() const {
  return ExtractMIMETypeFromMediaType(
      HttpHeaderField(http_names::kContentType).LowerASCII());
}

bool ResourceResponse::WasCached() const {
  return was_cached_;
}

void ResourceResponse::SetWasCached(bool value) {
  was_cached_ = value;
}

bool ResourceResponse::ConnectionReused() const {
  return connection_reused_;
}

void ResourceResponse::SetConnectionReused(bool connection_reused) {
  connection_reused_ = connection_reused;
}

unsigned ResourceResponse::ConnectionID() const {
  return connection_id_;
}

void ResourceResponse::SetConnectionID(unsigned connection_id) {
  connection_id_ = connection_id;
}

ResourceLoadTiming* ResourceResponse::GetResourceLoadTiming() const {
  return resource_load_timing_.get();
}

void ResourceResponse::SetResourceLoadTiming(
    scoped_refptr<ResourceLoadTiming> resource_load_timing) {
  resource_load_timing_ = std::move(resource_load_timing);
}

AtomicString ResourceResponse::ConnectionInfoString() const {
  std::string_view connection_info_string =
      net::HttpConnectionInfoToString(connection_info_);
  return AtomicString(base::as_byte_span(connection_info_string));
}

mojom::blink::CacheState ResourceResponse::CacheState() const {
  return is_validated_
             ? mojom::blink::CacheState::kValidated
             : (!encoded_data_length_ ? mojom::blink::CacheState::kLocal
                                      : mojom::blink::CacheState::kNone);
}

void ResourceResponse::SetIsValidated(bool is_validated) {
  is_validated_ = is_validated;
}

void ResourceResponse::SetEncodedDataLength(int64_t value) {
  encoded_data_length_ = value;
}

void ResourceResponse::SetEncodedBodyLength(uint64_t value) {
  encoded_body_length_ = value;
}

void ResourceResponse::SetDecodedBodyLength(int64_t value) {
  decoded_body_length_ = value;
}

network::mojom::CrossOriginEmbedderPolicyValue
ResourceResponse::GetCrossOriginEmbedderPolicy() const {
  const std::string value =
      HttpHeaderField(http_names::kLowerCrossOriginEmbedderPolicy).Utf8();
  using Item = net::structured_headers::Item;
  const auto item = net::structured_headers::ParseItem(value);
  if (!item || item->item.Type() != Item::kTokenType) {
    return network::mojom::CrossOriginEmbedderPolicyValue::kNone;
  }
  if (item->item.GetString() == "require-corp") {
    return network::mojom::CrossOriginEmbedderPolicyValue::kRequireCorp;
  } else if (item->item.GetString() == "credentialless") {
    return network::mojom::CrossOriginEmbedderPolicyValue::kCredentialless;
  } else {
    return network::mojom::CrossOriginEmbedderPolicyValue::kNone;
  }
}

STATIC_ASSERT_ENUM(WebURLResponse::kHTTPVersionUnknown,
                   ResourceResponse::kHTTPVersionUnknown);
STATIC_ASSERT_ENUM(WebURLResponse::kHTTPVersion_0_9,
                   ResourceResponse::kHTTPVersion_0_9);
STATIC_ASSERT_ENUM(WebURLResponse::kHTTPVersion_1_0,
                   ResourceResponse::kHTTPVersion_1_0);
STATIC_ASSERT_ENUM(WebURLResponse::kHTTPVersion_1_1,
                   ResourceResponse::kHTTPVersion_1_1);
STATIC_ASSERT_ENUM(WebURLResponse::kHTTPVersion_2_0,
                   ResourceResponse::kHTTPVersion_2_0);
}  // namespace blink
