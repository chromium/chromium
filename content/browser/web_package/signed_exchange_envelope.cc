// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_envelope.h"

#include <string_view>
#include <utility>

#include "base/containers/fixed_flat_set.h"
#include "base/format_macros.h"
#include "base/functional/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "components/cbor/reader.h"
#include "content/browser/web_package/signed_exchange_consts.h"
#include "content/browser/web_package/signed_exchange_utils.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "url/origin.h"

namespace content {

namespace {

bool IsUncachedHeader(std::string_view name) {
  DCHECK_EQ(name, base::ToLowerASCII(name));

  constexpr auto kUncachedHeaders = base::MakeFixedFlatSet<std::string_view>({
      // "Hop-by-hop header fields listed in the Connection header field
      // (Section 6.1 of {{!RFC7230}})." [spec text]
      // Note: The Connection header field itself is banned as uncached headers,
      // so no-op.

      // https://wicg.github.io/webpackage/draft-yasskin-httpbis-origin-signed-exchanges-impl.html#stateful-headers
      // TODO(kouhei): Dedupe the list with net/http/http_response_headers.cc
      //               kChallengeResponseHeaders and kCookieResponseHeaders.
      "authentication-control",
      "authentication-info",
      "clear-site-data",
      "optional-www-authenticate",
      "proxy-authenticate",
      "proxy-authentication-info",
      "public-key-pins",
      "sec-websocket-accept",
      "set-cookie",
      "set-cookie2",
      "setprofile",
      "strict-transport-security",
      "www-authenticate",

      // Other uncached header fields:
      // https://wicg.github.io/webpackage/draft-yasskin-httpbis-origin-signed-exchanges-impl.html#uncached-headers
      // TODO(kouhei): Dedupe the list with net/http/http_response_headers.cc
      //               kHopByHopResponseHeaders.
      "connection",
      "keep-alive",
      "proxy-connection",
      "trailer",
      "transfer-encoding",
      "upgrade",
  });

  return kUncachedHeaders.contains(name);
}

// Returns if the response is cacheble by a shared cache, as per Section 3 of
// [RFC7234].
bool IsCacheableBySharedCache(const SignedExchangeEnvelope::HeaderMap& headers,
                              SignedExchangeDevToolsProxy* devtools_proxy) {
  // As we require response code 200 which is cacheable by default, these two
  // are trivially true:
  // > o  the response status code is understood by the cache, and
  // > o  the response either:
  // >    ...
  // >    *  has a status code that is defined as cacheable by default (see
  // >       Section 4.2.2), or
  // >    ...
  //
  // Also, SXG version >= b3 do not have request method and headers, so these
  // are not applicable:
  // > o  The request method is understood by the cache and defined as being
  // >    cacheable, and
  // > o  the Authorization header field (see Section 4.2 of [RFC7235]) does
  // >    not appear in the request, if the cache is shared, unless the
  // >    response explicitly allows it (see Section 3.2), and
  //
  // Hence, we have to check the two remaining clauses:
  // > o  the "no-store" cache directive (see Section 5.2) does not appear
  // >    in request or response header fields, and
  // > o  the "private" response directive (see Section 5.2.2.6) does not
  // >    appear in the response, if the cache is shared, and
  //
  // Note that this implementation does not recognize any cache control
  // extensions.
  auto found = headers.find("cache-control");
  if (found == headers.end())
    return true;
  net::HttpUtil::NameValuePairsIterator it(
      found->second, /*delimiter=*/',',
      net::HttpUtil::NameValuePairsIterator::Values::NOT_REQUIRED,
      net::HttpUtil::NameValuePairsIterator::Quotes::STRICT_QUOTES);
  while (it.GetNext()) {
    std::string_view name = it.name();
    if (name == "no-store" || name == "private") {
      signed_exchange_utils::ReportErrorAndTraceEvent(
          devtools_proxy,
          base::StringPrintf(
              "Exchange's response must be cacheable by a shared cache, but "
              "has cache-control: %s",
              found->second.c_str()));
      return false;
    }
  }
  if (!it.valid()) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy,
        base::StringPrintf(
            "Failed to parse cache-control header of the exchange. "
            "cache-control: %s",
            found->second.c_str()));
    return false;
  }
  return true;
}

bool ParseResponseMap(const cbor::Value& value,
                      SignedExchangeEnvelope* out,
                      SignedExchangeDevToolsProxy* devtools_proxy) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"), "ParseResponseMap");
  if (!value.is_map()) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy,
        base::StringPrintf(
            "Expected response map, got non-map type. Actual type: %d",
            static_cast<int>(value.type())));
    return false;
  }

  const cbor::Value::MapValue& response_map = value.GetMap();
  auto status_iter = response_map.find(
      cbor::Value(kStatusKey, cbor::Value::Type::BYTE_STRING));
  if (status_iter == response_map.end() ||
      !status_iter->second.is_bytestring()) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy, ":status is not found or not a bytestring.");
    return false;
  }
  std::string_view response_code_str =
      status_iter->second.GetBytestringAsString();
  int response_code;
  if (!base::StringToInt(response_code_str, &response_code)) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy, "Failed to parse status code to integer.");
    return false;
  }

  // TODO(kouhei): Add spec ref here once
  // https://github.com/WICG/webpackage/issues/326 is resolved.
  if (response_code != 200) {
    signed_exchange_utils::ReportErrorAndTraceEvent(devtools_proxy,
                                                    "Status code is not 200.");
    return false;
  }
  out->set_response_code(static_cast<net::HttpStatusCode>(response_code));

  for (const auto& it : response_map) {
    if (!it.first.is_bytestring() || !it.second.is_bytestring()) {
      signed_exchange_utils::ReportErrorAndTraceEvent(
          devtools_proxy, "Non-bytestring value in the response map.");
      return false;
    }
    std::string_view name_str = it.first.GetBytestringAsString();
    if (name_str == kStatusKey)
      continue;
    if (!net::HttpUtil::IsValidHeaderName(name_str)) {
      signed_exchange_utils::ReportErrorAndTraceEvent(
          devtools_proxy,
          base::StringPrintf("Invalid header name. header_name: %s",
                             std::string(name_str).c_str()));
      return false;
    }

    // https://tools.ietf.org/html/draft-yasskin-httpbis-origin-signed-exchanges-impl-02
    // Section 3.2:
    // "For each response header field in `exchange`, the header field's
    // lowercase name as a byte string to the header field's value as a byte
    // string."
    if (name_str != base::ToLowerASCII(name_str)) {
      signed_exchange_utils::ReportErrorAndTraceEvent(
          devtools_proxy,
          base::StringPrintf(
              "Response header name should be lower-cased. header_name: %s",
              std::string(name_str).c_str()));
      return false;
    }

    // 4. If exchange's headers contains an uncached header field, as defined in
    // Section 4.1, return "invalid". [spec text]
    if (IsUncachedHeader(name_str)) {
      signed_exchange_utils::ReportErrorAndTraceEvent(
          devtools_proxy,
          base::StringPrintf(
              "Exchange contains stateful response header. header_name: %s",
              std::string(name_str).c_str()));
      return false;
    }

    std::string_view value_str = it.second.GetBytestringAsString();
    if (!net::HttpUtil::IsValidHeaderValue(value_str)) {
      signed_exchange_utils::ReportErrorAndTraceEvent(devtools_proxy,
                                                      "Invalid header value.");
      return false;
    }
    if (!out->AddResponseHeader(name_str, value_str)) {
      signed_exchange_utils::ReportErrorAndTraceEvent(
          devtools_proxy,
          base::StringPrintf("Duplicate header value. header_name: %s",
                             std::string(name_str).c_str()));
      return false;
    }
  }

  // https://wicg.github.io/webpackage/draft-yasskin-http-origin-signed-responses.html#cross-origin-trust
  // Step 5. If Section 3 of [RFC7234] forbids a shared cache from storing
  // response,
  //         return “invalid”. [spec text]
  if (!IsCacheableBySharedCache(out->response_headers(), devtools_proxy))
    return false;

  // https://wicg.github.io/webpackage/loading.html#parsing-a-signed-exchange
  // Step 26. If parsedExchange’s response's status is a redirect status or the
  //          signed exchange version of parsedExchange’s response is not
  //          undefined, return a failure. [spec text]
  if (net::HttpResponseHeaders::IsRedirectResponseCode(out->response_code())) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy,
        base::StringPrintf(
            "Exchange's response status must not be a redirect status. "
            "status: %d",
            response_code));
    return false;
  }

  // https://wicg.github.io/webpackage/loading.html#parsing-b2-cbor-headers
  // 7. If responseHeaders does not contain `Content-Type`, return a failure.
  // [spec text]
  // Note: "Parsing b3 CBOR headers" algorithm should have the same step.
  // See https://github.com/WICG/webpackage/issues/555
  auto content_type_iter = out->response_headers().find("content-type");
  if (content_type_iter == out->response_headers().end()) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy,
        "Exchange's inner response must have Content-Type header.");
    return false;
  }
  // https://wicg.github.io/webpackage/loading.html#parsing-b2-cbor-headers
  // 8. Set `X-Content-Type-Options`/`nosniff` in responseHeaders. [spec text]
  // Note: "Parsing b3 CBOR headers" algorithm should have the same step.
  // See https://github.com/WICG/webpackage/issues/555
  out->SetResponseHeader("x-content-type-options", "nosniff");

  // Note: This does not reject content-type like "application/signed-exchange"
  // (no "v=" parameter). In that case, SignedExchangeRequestHandler does not
  // handle the inner response and UA just downloads it.
  // See https://github.com/WICG/webpackage/issues/299 for details.
  if (signed_exchange_utils::GetSignedExchangeVersion(content_type_iter->second)
          .has_value()) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy,
        base::StringPrintf(
            "Exchange's inner response must not be a signed-exchange. "
            "conetent-type: %s",
            content_type_iter->second.c_str()));
    return false;
  }

  return true;
}

}  // namespace

// static
std::optional<SignedExchangeEnvelope> SignedExchangeEnvelope::Parse(
    SignedExchangeVersion version,
    const signed_exchange_utils::URLWithRawString& fallback_url,
    std::string_view signature_header_field,
    base::span<const uint8_t> cbor_header,
    SignedExchangeDevToolsProxy* devtools_proxy) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "SignedExchangeEnvelope::Parse");

  const auto& request_url = fallback_url;

  cbor::Reader::DecoderError error;
  std::optional<cbor::Value> value = cbor::Reader::Read(cbor_header, &error);
  if (!value.has_value()) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy,
        base::StringPrintf("Failed to decode Value. CBOR error: %s",
                           cbor::Reader::ErrorCodeToString(error)));
    return std::nullopt;
  }

  SignedExchangeEnvelope ret;
  ret.set_cbor_header(cbor_header);
  ret.set_request_url(request_url);

  if (!ParseResponseMap(*value, &ret, devtools_proxy)) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy, "Failed to parse response map.");
    return std::nullopt;
  }

  std::optional<std::vector<SignedExchangeSignatureHeaderField::Signature>>
      signatures = SignedExchangeSignatureHeaderField::ParseSignature(
          signature_header_field, devtools_proxy);
  if (!signatures || signatures->empty()) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy, "Failed to parse signature header field.");
    return std::nullopt;
  }

  // TODO(crbug.com/40579739): Support multiple signatures.
  ret.signature_ = (*signatures)[0];

  // https://wicg.github.io/webpackage/draft-yasskin-http-origin-signed-responses.html#cross-origin-trust
  // If the signature’s “validity-url” parameter is not same-origin with
  // exchange’s effective request URI (Section 5.5 of [RFC7230]), return
  // “invalid” [spec text]
  const GURL validity_url = ret.signature().validity_url.url;
  if (!url::IsSameOriginWith(request_url.url, validity_url)) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy, "Validity URL must be same-origin with request URL.");
    return std::nullopt;
  }

  return std::move(ret);
}

SignedExchangeEnvelope::SignedExchangeEnvelope() = default;
SignedExchangeEnvelope::SignedExchangeEnvelope(const SignedExchangeEnvelope&) =
    default;
SignedExchangeEnvelope::SignedExchangeEnvelope(SignedExchangeEnvelope&&) =
    default;
SignedExchangeEnvelope::~SignedExchangeEnvelope() = default;
SignedExchangeEnvelope& SignedExchangeEnvelope::operator=(
    SignedExchangeEnvelope&&) = default;

bool SignedExchangeEnvelope::AddResponseHeader(std::string_view name,
                                               std::string_view value) {
  std::string name_str(name);
  DCHECK_EQ(name_str, base::ToLowerASCII(name))
      << "Response header names should be always lower-cased.";
  if (response_headers_.find(name_str) != response_headers_.end())
    return false;

  response_headers_.emplace(std::move(name_str), std::string(value));
  return true;
}

void SignedExchangeEnvelope::SetResponseHeader(std::string_view name,
                                               std::string_view value) {
  std::string name_str(name);
  DCHECK_EQ(name_str, base::ToLowerASCII(name))
      << "Response header names should be always lower-cased.";
  response_headers_[name_str] = std::string(value);
}

scoped_refptr<net::HttpResponseHeaders>
SignedExchangeEnvelope::BuildHttpResponseHeaders() const {
  std::string header_str("HTTP/1.1 ");
  header_str.append(base::NumberToString(response_code()));
  header_str.append(" ");
  header_str.append(net::GetHttpReasonPhrase(response_code()));
  header_str.append(" \r\n");
  for (const auto& it : response_headers()) {
    header_str.append(it.first);
    header_str.append(": ");
    header_str.append(it.second);
    header_str.append("\r\n");
  }
  header_str.append("\r\n");
  return base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(header_str));
}

void SignedExchangeEnvelope::set_cbor_header(base::span<const uint8_t> data) {
  cbor_header_ = std::vector<uint8_t>(data.begin(), data.end());
}

net::SHA256HashValue SignedExchangeEnvelope::ComputeHeaderIntegrity() const {
  net::SHA256HashValue hash;
  crypto::SHA256HashString(
      std::string_view(reinterpret_cast<const char*>(cbor_header().data()),
                       cbor_header().size()),
      &hash, sizeof(net::SHA256HashValue));
  return hash;
}

}  // namespace content
