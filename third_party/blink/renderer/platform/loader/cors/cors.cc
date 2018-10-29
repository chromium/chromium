// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/cors/cors.h"

#include <memory>
#include <string>
#include <utility>

#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/cors/preflight_cache.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/loader/cors/cors_error_string.h"
#include "third_party/blink/renderer/platform/network/http_header_map.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"
#include "url/gurl.h"

namespace blink {

namespace {

base::Optional<std::string> GetHeaderValue(const HTTPHeaderMap& header_map,
                                           const AtomicString& header_name) {
  if (header_map.Contains(header_name)) {
    const AtomicString& atomic_value = header_map.Get(header_name);
    CString string_value = atomic_value.GetString().Utf8();
    return std::string(string_value.data(), string_value.length());
  }
  return base::nullopt;
}

network::cors::PreflightCache& GetPerThreadPreflightCache() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<network::cors::PreflightCache>,
                                  cache, ());
  return *cache;
}

base::Optional<std::string> GetOptionalHeaderValue(
    const HTTPHeaderMap& header_map,
    const AtomicString& header_name) {
  const AtomicString& result = header_map.Get(header_name);
  if (result.IsNull())
    return base::nullopt;

  return std::string(result.Ascii().data());
}

std::unique_ptr<net::HttpRequestHeaders> CreateNetHttpRequestHeaders(
    const HTTPHeaderMap& header_map) {
  std::unique_ptr<net::HttpRequestHeaders> request_headers =
      std::make_unique<net::HttpRequestHeaders>();
  for (HTTPHeaderMap::const_iterator i = header_map.begin(),
                                     end = header_map.end();
       i != end; ++i) {
    DCHECK(!i->key.IsNull());
    DCHECK(!i->value.IsNull());
    request_headers->SetHeader(std::string(i->key.Ascii().data()),
                               std::string(i->value.Ascii().data()));
  }
  return request_headers;
}

url::Origin AsUrlOrigin(const SecurityOrigin& origin) {
  // "file:" origin is treated like an opaque unique origin when
  // allow-file-access-from-files is not specified. Such origin is not
  // opaque (i.e., IsOpaque() returns false) but still serializes to
  // "null".
  return origin.ToString() == "null" ? url::Origin() : origin.ToUrlOrigin();
}

}  // namespace

namespace CORS {

base::Optional<network::CORSErrorStatus> CheckAccess(
    const KURL& response_url,
    const int response_status_code,
    const HTTPHeaderMap& response_header,
    network::mojom::FetchCredentialsMode credentials_mode,
    const SecurityOrigin& origin) {
  return network::cors::CheckAccess(
      response_url, response_status_code,
      GetHeaderValue(response_header, HTTPNames::Access_Control_Allow_Origin),
      GetHeaderValue(response_header,
                     HTTPNames::Access_Control_Allow_Credentials),
      credentials_mode, AsUrlOrigin(origin));
}

base::Optional<network::CORSErrorStatus> CheckPreflightAccess(
    const KURL& response_url,
    const int response_status_code,
    const HTTPHeaderMap& response_header,
    network::mojom::FetchCredentialsMode actual_credentials_mode,
    const SecurityOrigin& origin) {
  return network::cors::CheckPreflightAccess(
      response_url, response_status_code,
      GetHeaderValue(response_header, HTTPNames::Access_Control_Allow_Origin),
      GetHeaderValue(response_header,
                     HTTPNames::Access_Control_Allow_Credentials),
      actual_credentials_mode, AsUrlOrigin(origin));
}

base::Optional<network::CORSErrorStatus> CheckRedirectLocation(
    const KURL& url,
    network::mojom::FetchRequestMode request_mode,
    const SecurityOrigin* origin,
    CORSFlag cors_flag) {
  base::Optional<url::Origin> origin_to_pass;
  if (origin)
    origin_to_pass = AsUrlOrigin(*origin);

  // Blink-side implementations rewrite the origin instead of setting the
  // tainted flag.
  return network::cors::CheckRedirectLocation(
      url, request_mode, origin_to_pass, cors_flag == CORSFlag::Set, false);
}

base::Optional<network::mojom::CORSError> CheckPreflight(
    const int preflight_response_status_code) {
  return network::cors::CheckPreflight(preflight_response_status_code);
}

base::Optional<network::CORSErrorStatus> CheckExternalPreflight(
    const HTTPHeaderMap& response_header) {
  return network::cors::CheckExternalPreflight(GetHeaderValue(
      response_header, HTTPNames::Access_Control_Allow_External));
}

bool IsCORSEnabledRequestMode(network::mojom::FetchRequestMode request_mode) {
  return network::cors::IsCORSEnabledRequestMode(request_mode);
}

base::Optional<network::CORSErrorStatus> EnsurePreflightResultAndCacheOnSuccess(
    const HTTPHeaderMap& response_header_map,
    const String& origin,
    const KURL& request_url,
    const String& request_method,
    const HTTPHeaderMap& request_header_map,
    network::mojom::FetchCredentialsMode request_credentials_mode) {
  DCHECK(!origin.IsNull());
  DCHECK(!request_method.IsNull());

  base::Optional<network::mojom::CORSError> error;

  std::unique_ptr<network::cors::PreflightResult> result =
      network::cors::PreflightResult::Create(
          request_credentials_mode,
          GetOptionalHeaderValue(response_header_map,
                                 HTTPNames::Access_Control_Allow_Methods),
          GetOptionalHeaderValue(response_header_map,
                                 HTTPNames::Access_Control_Allow_Headers),
          GetOptionalHeaderValue(response_header_map,
                                 HTTPNames::Access_Control_Max_Age),
          &error);
  if (error)
    return network::CORSErrorStatus(*error);

  base::Optional<network::CORSErrorStatus> status;
  status = result->EnsureAllowedCrossOriginMethod(
      std::string(request_method.Ascii().data()));
  if (status)
    return status;

  // |is_revalidating| is not needed for blink-side CORS.
  constexpr bool is_revalidating = false;
  status = result->EnsureAllowedCrossOriginHeaders(
      *CreateNetHttpRequestHeaders(request_header_map), is_revalidating);
  if (status)
    return status;

  GetPerThreadPreflightCache().AppendEntry(std::string(origin.Ascii().data()),
                                           request_url, std::move(result));
  return base::nullopt;
}

bool CheckIfRequestCanSkipPreflight(
    const String& origin,
    const KURL& url,
    network::mojom::FetchCredentialsMode credentials_mode,
    const String& method,
    const HTTPHeaderMap& request_header_map) {
  DCHECK(!origin.IsNull());
  DCHECK(!method.IsNull());

  // |is_revalidating| is not needed for blink-side CORS.
  constexpr bool is_revalidating = false;
  return GetPerThreadPreflightCache().CheckIfRequestCanSkipPreflight(
      std::string(origin.Ascii().data()), url, credentials_mode,
      std::string(method.Ascii().data()),
      *CreateNetHttpRequestHeaders(request_header_map), is_revalidating);
}

network::mojom::FetchResponseType CalculateResponseTainting(
    const KURL& url,
    network::mojom::FetchRequestMode request_mode,
    const SecurityOrigin* origin,
    CORSFlag cors_flag) {
  base::Optional<url::Origin> origin_to_pass;
  if (origin)
    origin_to_pass = AsUrlOrigin(*origin);
  return network::cors::CalculateResponseTainting(
      url, request_mode, origin_to_pass, cors_flag == CORSFlag::Set);
}

bool CalculateCredentialsFlag(
    network::mojom::FetchCredentialsMode credentials_mode,
    network::mojom::FetchResponseType response_tainting) {
  return network::cors::CalculateCredentialsFlag(credentials_mode,
                                                 response_tainting);
}

bool IsCORSSafelistedMethod(const String& method) {
  DCHECK(!method.IsNull());
  CString utf8_method = method.Utf8();
  return network::cors::IsCORSSafelistedMethod(
      std::string(utf8_method.data(), utf8_method.length()));
}

bool IsCORSSafelistedContentType(const String& media_type) {
  CString utf8_media_type = media_type.Utf8();
  return network::cors::IsCORSSafelistedContentType(
      std::string(utf8_media_type.data(), utf8_media_type.length()));
}

bool IsCORSSafelistedHeader(const String& name, const String& value) {
  DCHECK(!name.IsNull());
  DCHECK(!value.IsNull());
  CString utf8_name = name.Utf8();
  CString utf8_value = value.Utf8();
  return network::cors::IsCORSSafelistedHeader(
      std::string(utf8_name.data(), utf8_name.length()),
      std::string(utf8_value.data(), utf8_value.length()));
}

bool IsNoCORSSafelistedHeader(const String& name, const String& value) {
  DCHECK(!name.IsNull());
  DCHECK(!value.IsNull());
  return network::cors::IsNoCORSSafelistedHeader(WebString(name).Latin1(),
                                                 WebString(value).Latin1());
}

Vector<String> CORSUnsafeRequestHeaderNames(const HTTPHeaderMap& headers) {
  net::HttpRequestHeaders::HeaderVector in;
  for (const auto& entry : headers) {
    in.push_back(net::HttpRequestHeaders::HeaderKeyValuePair(
        WebString(entry.key).Latin1(), WebString(entry.value).Latin1()));
  }

  Vector<String> header_names;
  for (const auto& name : network::cors::CORSUnsafeRequestHeaderNames(in))
    header_names.push_back(WebString::FromLatin1(name));
  return header_names;
}

bool IsForbiddenHeaderName(const String& name) {
  CString utf8_name = name.Utf8();
  return network::cors::IsForbiddenHeader(
      std::string(utf8_name.data(), utf8_name.length()));
}

bool ContainsOnlyCORSSafelistedHeaders(const HTTPHeaderMap& header_map) {
  Vector<String> header_names = CORSUnsafeRequestHeaderNames(header_map);
  return header_names.IsEmpty();
}

bool ContainsOnlyCORSSafelistedOrForbiddenHeaders(
    const HTTPHeaderMap& headers) {
  Vector<String> header_names;

  net::HttpRequestHeaders::HeaderVector in;
  for (const auto& entry : headers) {
    in.push_back(net::HttpRequestHeaders::HeaderKeyValuePair(
        WebString(entry.key).Latin1(), WebString(entry.value).Latin1()));
  }
  // |is_revalidating| is not needed for blink-side CORS.
  constexpr bool is_revalidating = false;
  return network::cors::CORSUnsafeNotForbiddenRequestHeaderNames(
             in, is_revalidating)
      .empty();
}

bool IsOkStatus(int status) {
  return network::cors::IsOkStatus(status);
}

bool CalculateCORSFlag(const KURL& url,
                       const SecurityOrigin* origin,
                       network::mojom::FetchRequestMode request_mode) {
  if (request_mode == network::mojom::FetchRequestMode::kNavigate ||
      request_mode == network::mojom::FetchRequestMode::kNoCORS) {
    return false;
  }
  // CORS needs a proper origin (including a unique opaque origin). If the
  // request doesn't have one, CORS should not work.
  DCHECK(origin);
  return !origin->CanReadContent(url);
}

}  // namespace CORS

}  // namespace blink
