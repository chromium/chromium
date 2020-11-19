// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/cors/cors.h"

#include <string>

#include "net/http/http_util.h"
#include "services/network/public/cpp/cors/cors.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

namespace {

// A parser for the value of the Access-Control-Expose-Headers header.
class HTTPHeaderNameListParser {
  STACK_ALLOCATED();

 public:
  explicit HTTPHeaderNameListParser(const String& value)
      : value_(value), pos_(0) {}

  // Tries parsing |value_| expecting it to be conforming to the #field-name
  // ABNF rule defined in RFC 7230. Returns with the field-name entries stored
  // in |output| when successful. Otherwise, returns with |output| kept empty.
  //
  // |output| must be empty.
  void Parse(HTTPHeaderSet& output) {
    DCHECK(output.empty());

    while (true) {
      ConsumeSpaces();

      if (pos_ == value_.length() && !output.empty()) {
        output.insert(std::string());
        return;
      }

      size_t token_start = pos_;
      ConsumeTokenChars();
      size_t token_size = pos_ - token_start;
      if (token_size == 0) {
        output.clear();
        return;
      }

      output.insert(value_.Substring(token_start, token_size).Ascii());
      ConsumeSpaces();

      if (pos_ == value_.length())
        return;

      if (value_[pos_] == ',') {
        ++pos_;
      } else {
        output.clear();
        return;
      }
    }
  }

 private:
  // Consumes zero or more spaces (SP and HTAB) from value_.
  void ConsumeSpaces() {
    while (true) {
      if (pos_ == value_.length())
        return;

      UChar c = value_[pos_];
      if (c != ' ' && c != '\t')
        return;
      ++pos_;
    }
  }

  // Consumes zero or more tchars from value_.
  void ConsumeTokenChars() {
    while (true) {
      if (pos_ == value_.length())
        return;

      UChar c = value_[pos_];
      if (c > 0x7F || !net::HttpUtil::IsTokenChar(c))
        return;
      ++pos_;
    }
  }

  const String value_;
  size_t pos_;
};

}  // namespace

namespace cors {

bool IsCorsEnabledRequestMode(network::mojom::RequestMode request_mode) {
  return network::cors::IsCorsEnabledRequestMode(request_mode);
}

bool IsCorsSafelistedMethod(const String& method) {
  DCHECK(!method.IsNull());
  return network::cors::IsCorsSafelistedMethod(method.Latin1());
}

bool IsCorsSafelistedContentType(const String& media_type) {
  return network::cors::IsCorsSafelistedContentType(media_type.Latin1());
}

bool IsNoCorsSafelistedHeader(const String& name, const String& value) {
  DCHECK(!name.IsNull());
  DCHECK(!value.IsNull());
  return network::cors::IsNoCorsSafelistedHeader(name.Latin1(), value.Latin1());
}

bool IsPrivilegedNoCorsHeaderName(const String& name) {
  DCHECK(!name.IsNull());
  return network::cors::IsPrivilegedNoCorsHeaderName(name.Latin1());
}

bool IsNoCorsSafelistedHeaderName(const String& name) {
  DCHECK(!name.IsNull());
  return network::cors::IsNoCorsSafelistedHeaderName(name.Latin1());
}

PLATFORM_EXPORT Vector<String> PrivilegedNoCorsHeaderNames() {
  Vector<String> header_names;
  for (const auto& name : network::cors::PrivilegedNoCorsHeaderNames())
    header_names.push_back(WebString::FromLatin1(name));
  return header_names;
}

bool IsForbiddenHeaderName(const String& name) {
  return !net::HttpUtil::IsSafeHeader(name.Latin1());
}

bool ContainsOnlyCorsSafelistedHeaders(const HTTPHeaderMap& header_map) {
  net::HttpRequestHeaders::HeaderVector in;
  for (const auto& entry : header_map) {
    in.push_back(net::HttpRequestHeaders::HeaderKeyValuePair(
        entry.key.Latin1(), entry.value.Latin1()));
  }

  return network::cors::CorsUnsafeRequestHeaderNames(in).empty();
}

bool ContainsOnlyCorsSafelistedOrForbiddenHeaders(
    const HTTPHeaderMap& headers) {
  Vector<String> header_names;

  net::HttpRequestHeaders::HeaderVector in;
  for (const auto& entry : headers) {
    in.push_back(net::HttpRequestHeaders::HeaderKeyValuePair(
        entry.key.Latin1(), entry.value.Latin1()));
  }
  // |is_revalidating| is not needed for blink-side CORS.
  constexpr bool is_revalidating = false;
  return network::cors::CorsUnsafeNotForbiddenRequestHeaderNames(
             in, is_revalidating)
      .empty();
}

bool IsOkStatus(int status) {
  return network::cors::IsOkStatus(status);
}

bool CalculateCorsFlag(const KURL& url,
                       const SecurityOrigin* initiator_origin,
                       const SecurityOrigin* isolated_world_origin,
                       network::mojom::RequestMode request_mode) {
  if (request_mode == network::mojom::RequestMode::kNavigate ||
      request_mode == network::mojom::RequestMode::kNoCors) {
    return false;
  }

  // CORS needs a proper origin (including a unique opaque origin). If the
  // request doesn't have one, CORS will not work.
  DCHECK(initiator_origin);

  if (initiator_origin->CanReadContent(url))
    return false;

  if (isolated_world_origin && isolated_world_origin->CanReadContent(url))
    return false;

  return true;
}

HTTPHeaderSet ExtractCorsExposedHeaderNamesList(
    network::mojom::CredentialsMode credentials_mode,
    const ResourceResponse& response) {
  // If a response was fetched via a service worker, it will always have
  // CorsExposedHeaderNames set from the Access-Control-Expose-Headers header.
  // For requests that didn't come from a service worker, just parse the CORS
  // header.
  if (response.WasFetchedViaServiceWorker()) {
    HTTPHeaderSet header_set;
    for (const auto& header : response.CorsExposedHeaderNames())
      header_set.insert(header.Ascii());
    return header_set;
  }

  HTTPHeaderSet header_set;
  HTTPHeaderNameListParser parser(
      response.HttpHeaderField(http_names::kAccessControlExposeHeaders));
  parser.Parse(header_set);

  if (credentials_mode != network::mojom::CredentialsMode::kInclude &&
      header_set.find("*") != header_set.end()) {
    header_set.clear();
    for (const auto& header : response.HttpHeaderFields())
      header_set.insert(header.key.Ascii());
  }
  return header_set;
}

bool IsCorsSafelistedResponseHeader(const String& name) {
  // https://fetch.spec.whatwg.org/#cors-safelisted-response-header-name
  // TODO(dcheng): Consider using a flat_set here with a transparent comparator.
  DEFINE_THREAD_SAFE_STATIC_LOCAL(HTTPHeaderSet,
                                  allowed_cross_origin_response_headers,
                                  ({
                                      "cache-control",
                                      "content-language",
                                      "content-length",
                                      "content-type",
                                      "expires",
                                      "last-modified",
                                      "pragma",
                                  }));
  return allowed_cross_origin_response_headers.find(name.Ascii()) !=
         allowed_cross_origin_response_headers.end();
}

// In the spec, https://fetch.spec.whatwg.org/#ref-for-concept-request-mode,
// No-CORS mode is highly discouraged from using it for new features. Only
// legacy usages for backward compatibility are allowed except for well-designed
// usages over the fetch API.
bool IsNoCorsAllowedContext(mojom::blink::RequestContextType context) {
  switch (context) {
    case mojom::blink::RequestContextType::AUDIO:
    case mojom::blink::RequestContextType::FAVICON:
    case mojom::blink::RequestContextType::FETCH:
    case mojom::blink::RequestContextType::IMAGE:
    case mojom::blink::RequestContextType::OBJECT:
    case mojom::blink::RequestContextType::PLUGIN:
    case mojom::blink::RequestContextType::SCRIPT:
    case mojom::blink::RequestContextType::SHARED_WORKER:
    case mojom::blink::RequestContextType::VIDEO:
    case mojom::blink::RequestContextType::WORKER:
      return true;
    default:
      return false;
  }
}

}  // namespace cors

}  // namespace blink
