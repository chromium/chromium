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

#include <algorithm>
#include <limits>
#include <memory>
#include <string>

#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_info.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_timing.h"
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
  result.ReserveCapacity(src.size());
  for (const auto& timestamp : src) {
    result.push_back(timestamp.IsolatedCopy());
  }
  return result;
}

static const char kCacheControlHeader[] = "cache-control";
static const char kPragmaHeader[] = "pragma";

}  // namespace

ResourceResponse::SignedCertificateTimestamp::SignedCertificateTimestamp(
    const blink::WebURLResponse::SignedCertificateTimestamp& sct)
    : status_(sct.status),
      origin_(sct.origin),
      log_description_(sct.log_description),
      log_id_(sct.log_id),
      timestamp_(sct.timestamp),
      hash_algorithm_(sct.hash_algorithm),
      signature_algorithm_(sct.signature_algorithm),
      signature_data_(sct.signature_data) {}

ResourceResponse::SignedCertificateTimestamp
ResourceResponse::SignedCertificateTimestamp::IsolatedCopy() const {
  return SignedCertificateTimestamp(
      status_.IsolatedCopy(), origin_.IsolatedCopy(),
      log_description_.IsolatedCopy(), log_id_.IsolatedCopy(), timestamp_,
      hash_algorithm_.IsolatedCopy(), signature_algorithm_.IsolatedCopy(),
      signature_data_.IsolatedCopy());
}

ResourceResponse::ResourceResponse()
    : is_null_(true),
      response_type_(network::mojom::FetchResponseType::kDefault) {}

ResourceResponse::ResourceResponse(const KURL& current_request_url)
    : current_request_url_(current_request_url),
      is_null_(false),
      response_type_(network::mojom::FetchResponseType::kDefault) {}

ResourceResponse::ResourceResponse(const ResourceResponse&) = default;
ResourceResponse& ResourceResponse::operator=(const ResourceResponse&) =
    default;

ResourceResponse::~ResourceResponse() = default;

bool ResourceResponse::IsHTTP() const {
  return current_request_url_.ProtocolIsInHTTPFamily();
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
  if (!url_list_via_service_worker_.IsEmpty()) {
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
  return cache_storage_cache_name_.IsEmpty() &&
         !url_list_via_service_worker_.IsEmpty() &&
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
  static const char kAgeHeader[] = "age";
  static const char kDateHeader[] = "date";
  static const char kExpiresHeader[] = "expires";
  static const char kLastModifiedHeader[] = "last-modified";

  if (DeprecatedEqualIgnoringCase(name, kAgeHeader))
    have_parsed_age_header_ = false;
  else if (DeprecatedEqualIgnoringCase(name, kCacheControlHeader) ||
           DeprecatedEqualIgnoringCase(name, kPragmaHeader))
    cache_control_header_ = CacheControlHeader();
  else if (DeprecatedEqualIgnoringCase(name, kDateHeader))
    have_parsed_date_header_ = false;
  else if (DeprecatedEqualIgnoringCase(name, kExpiresHeader))
    have_parsed_expires_header_ = false;
  else if (DeprecatedEqualIgnoringCase(name, kLastModifiedHeader))
    have_parsed_last_modified_header_ = false;
}

void ResourceResponse::SetSecurityDetails(
    const String& protocol,
    const String& key_exchange,
    const String& key_exchange_group,
    const String& cipher,
    const String& mac,
    const String& subject_name,
    const Vector<String>& san_list,
    const String& issuer,
    time_t valid_from,
    time_t valid_to,
    const Vector<AtomicString>& certificate,
    const SignedCertificateTimestampList& sct_list) {
  DCHECK_NE(security_style_, SecurityStyle::kUnknown);
  DCHECK_NE(security_style_, SecurityStyle::kNeutral);
  security_details_ = SecurityDetails(
      protocol, key_exchange, key_exchange_group, cipher, mac, subject_name,
      san_list, issuer, valid_from, valid_to, certificate, sct_list);
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
  if (values.IsEmpty())
    return;

  UpdateHeaderParsedState(name);

  StringBuilder value_builder;
  const auto it = http_header_fields_.Find(name);
  if (it != http_header_fields_.end())
    value_builder.Append(it->value);
  for (const auto& value : values) {
    if (!value_builder.IsEmpty())
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
        http_header_fields_.Get(kCacheControlHeader),
        http_header_fields_.Get(kPragmaHeader));
  }
  return cache_control_header_.contains_no_cache;
}

bool ResourceResponse::CacheControlContainsNoStore() const {
  if (!cache_control_header_.parsed) {
    cache_control_header_ = ParseCacheControlDirectives(
        http_header_fields_.Get(kCacheControlHeader),
        http_header_fields_.Get(kPragmaHeader));
  }
  return cache_control_header_.contains_no_store;
}

bool ResourceResponse::CacheControlContainsMustRevalidate() const {
  if (!cache_control_header_.parsed) {
    cache_control_header_ = ParseCacheControlDirectives(
        http_header_fields_.Get(kCacheControlHeader),
        http_header_fields_.Get(kPragmaHeader));
  }
  return cache_control_header_.contains_must_revalidate;
}

bool ResourceResponse::HasCacheValidatorFields() const {
  static const char kLastModifiedHeader[] = "last-modified";
  static const char kETagHeader[] = "etag";
  return !http_header_fields_.Get(kLastModifiedHeader).IsEmpty() ||
         !http_header_fields_.Get(kETagHeader).IsEmpty();
}

base::Optional<base::TimeDelta> ResourceResponse::CacheControlMaxAge() const {
  if (!cache_control_header_.parsed) {
    cache_control_header_ = ParseCacheControlDirectives(
        http_header_fields_.Get(kCacheControlHeader),
        http_header_fields_.Get(kPragmaHeader));
  }
  return cache_control_header_.max_age;
}

base::TimeDelta ResourceResponse::CacheControlStaleWhileRevalidate() const {
  if (!cache_control_header_.parsed) {
    cache_control_header_ = ParseCacheControlDirectives(
        http_header_fields_.Get(kCacheControlHeader),
        http_header_fields_.Get(kPragmaHeader));
  }
  if (!cache_control_header_.stale_while_revalidate ||
      cache_control_header_.stale_while_revalidate.value() <
          base::TimeDelta()) {
    return base::TimeDelta();
  }
  return cache_control_header_.stale_while_revalidate.value();
}

static base::Optional<base::Time> ParseDateValueInHeader(
    const HTTPHeaderMap& headers,
    const AtomicString& header_name) {
  const AtomicString& header_value = headers.Get(header_name);
  if (header_value.IsEmpty())
    return base::nullopt;
  // This handles all date formats required by RFC2616:
  // Sun, 06 Nov 1994 08:49:37 GMT  ; RFC 822, updated by RFC 1123
  // Sunday, 06-Nov-94 08:49:37 GMT ; RFC 850, obsoleted by RFC 1036
  // Sun Nov  6 08:49:37 1994       ; ANSI C's asctime() format
  base::Optional<base::Time> date = ParseDate(header_value);
  if (date && date.value().is_max())
    return base::nullopt;
  return date;
}

base::Optional<base::Time> ResourceResponse::Date() const {
  if (!have_parsed_date_header_) {
    static const char kHeaderName[] = "date";
    date_ = ParseDateValueInHeader(http_header_fields_, kHeaderName);
    have_parsed_date_header_ = true;
  }
  return date_;
}

base::Optional<base::TimeDelta> ResourceResponse::Age() const {
  if (!have_parsed_age_header_) {
    static const char kHeaderName[] = "age";
    const AtomicString& header_value = http_header_fields_.Get(kHeaderName);
    bool ok;
    double seconds = header_value.ToDouble(&ok);
    if (!ok) {
      age_ = base::nullopt;
    } else {
      age_ = base::TimeDelta::FromSecondsD(seconds);
    }
    have_parsed_age_header_ = true;
  }
  return age_;
}

base::Optional<base::Time> ResourceResponse::Expires() const {
  if (!have_parsed_expires_header_) {
    static const char kHeaderName[] = "expires";
    expires_ = ParseDateValueInHeader(http_header_fields_, kHeaderName);
    have_parsed_expires_header_ = true;
  }
  return expires_;
}

base::Optional<base::Time> ResourceResponse::LastModified() const {
  if (!have_parsed_last_modified_header_) {
    static const char kHeaderName[] = "last-modified";
    last_modified_ = ParseDateValueInHeader(http_header_fields_, kHeaderName);
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
  return DeprecatedEqualIgnoringCase(value, kAttachmentString);
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

scoped_refptr<ResourceLoadInfo> ResourceResponse::GetResourceLoadInfo() const {
  return resource_load_info_.get();
}

void ResourceResponse::SetResourceLoadInfo(
    scoped_refptr<ResourceLoadInfo> load_info) {
  resource_load_info_ = std::move(load_info);
}

void ResourceResponse::SetCTPolicyCompliance(CTPolicyCompliance compliance) {
  ct_policy_compliance_ = compliance;
}

AtomicString ResourceResponse::ConnectionInfoString() const {
  std::string connection_info_string =
      net::HttpResponseInfo::ConnectionInfoToString(connection_info_);
  return AtomicString(
      reinterpret_cast<const LChar*>(connection_info_string.data()),
      connection_info_string.length());
}

void ResourceResponse::SetEncodedDataLength(int64_t value) {
  encoded_data_length_ = value;
}

void ResourceResponse::SetEncodedBodyLength(int64_t value) {
  encoded_body_length_ = value;
}

void ResourceResponse::SetDecodedBodyLength(int64_t value) {
  decoded_body_length_ = value;
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
