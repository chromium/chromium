// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_cookie_indices.h"

#include "net/cookies/parsed_cookie.h"
#include "net/http/http_response_headers.h"
#include "net/http/structured_headers.h"

namespace net {

namespace {
constexpr std::string_view kCookieIndicesHeader = "Cookie-Indices";
}  // namespace

std::optional<std::vector<std::string>> ParseCookieIndices(
    const HttpResponseHeaders& headers) {
  std::string normalized_header;
  if (!headers.GetNormalizedHeader(kCookieIndicesHeader, &normalized_header)) {
    return std::nullopt;
  }

  std::optional<net::structured_headers::List> list =
      structured_headers::ParseList(normalized_header);
  if (!list.has_value()) {
    return std::nullopt;
  }

  std::vector<std::string> cookie_names;
  cookie_names.reserve(list->size());
  for (const structured_headers::ParameterizedMember& member : *list) {
    if (member.member_is_inner_list || member.params.size()) {
      // Inner list not permitted here.
      // TODO(crbug.com/328628231): Perhaps this should be handled gracefully.
      return std::nullopt;
    }

    const structured_headers::ParameterizedItem& item = member.member[0];
    if (item.params.size() || !item.item.is_token()) {
      // Non-token items, and tokens with parameters, are not permitted here.
      // TODO(crbug.com/328628231): Perhaps this should be handled gracefully.
      return std::nullopt;
    }

    // There are basically three sets of requirements that are interesting here.
    //
    // 1. Cookie names Chromium considers valid, given by:
    //      cookie-name       = *cookie-name-octet
    //      cookie-name-octet = %x20-3A / %x3C / %x3E-7E / %x80-FF
    //                          ; octets excluding CTLs, ";", and "="
    //    See |net::ParsedCookie::IsValidCookieName|.
    //
    // 2. Cookie names RFC 6265 considers valid, given by:
    //      cookie-name = token
    //      token       = 1*<any CHAR except CTLs or separators>
    //      separators  = "(" | ")" | "<" | ">" | "@"
    //                  | "," | ";" | ":" | "\" | <">
    //                  | "/" | "[" | "]" | "?" | "="
    //                  | "{" | "}" | SP | HT
    //      CHAR        = <any US-ASCII character (octets 0 - 127)>
    //      CTL         = <any US-ASCII control character
    //                    (octets 0 - 31) and DEL (127)>
    //
    // 3. Valid RFC 8941 structured field tokens, given by:
    //      sf-token = ( ALPHA / "*" ) *( tchar / ":" / "/" )
    //      UPALPHA        = <any US-ASCII uppercase letter "A".."Z">
    //      LOALPHA        = <any US-ASCII lowercase letter "a".."z">
    //      ALPHA          = UPALPHA | LOALPHA
    //      tchar          = "!" / "#" / "$" / "%" / "&" / "'" / "*"
    //                     / "+" / "-" / "." / "^" / "_" / "`" / "|" / "~"
    //                     / DIGIT / ALPHA
    //                     ; any VCHAR, except delimiters
    //
    // This means that not all valid cookie names (whichever definition you use)
    // can be parsed here. For example, "__sessid" does not start with an
    // alphabetic character or an asterisk).
    //
    // Nor is every structured field token a valid cookie name by the RFC 6265
    // requirements, since it may contain a "/" (%x2F) or ":" (%x3A) as a
    // non-initial character. In the interest of interoperability, and since
    // there are already possible cookie names which could not be specified
    // here, those are expressly rejected.
    //
    // Aside from this, every structured field token is necessarily a valid
    // cookie name (according to both Chromium and RFC 6265).
    const std::string& token_string = item.item.GetString();
    if (token_string.find_first_of(":/") != std::string::npos) {
      // This is one of those structured tokens that is not a valid cookie name
      // according to RFC 6265.
      // TODO(crbug.com/328628231): Perhaps this should be handled gracefully.
      return std::nullopt;
    }
    CHECK(ParsedCookie::IsValidCookieName(token_string))
        << "invalid cookie name \"" << token_string << "\"";
    cookie_names.push_back(token_string);
  }
  return cookie_names;
}

}  // namespace net
