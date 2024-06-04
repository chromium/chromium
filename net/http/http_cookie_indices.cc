// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_cookie_indices.h"

#include <functional>

#include "base/containers/span.h"
#include "base/pickle.h"
#include "base/ranges/algorithm.h"
#include "crypto/sha2.h"
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

  std::optional<structured_headers::List> list =
      structured_headers::ParseList(normalized_header);
  if (!list.has_value()) {
    return std::nullopt;
  }

  std::vector<std::string> cookie_names;
  cookie_names.reserve(list->size());
  for (const structured_headers::ParameterizedMember& member : *list) {
    if (member.member_is_inner_list) {
      // Inner list not permitted here.
      return std::nullopt;
    }

    const structured_headers::ParameterizedItem& item = member.member[0];
    if (!item.item.is_string()) {
      // Non-string items are not permitted here.
      return std::nullopt;
    }

    // There are basically three sets of requirements that are interesting here.
    //
    // 1. Cookie names Chromium considers valid, given by:
    //      cookie-name       = *cookie-name-octet
    //      cookie-name-octet = %x20-3A / %x3C / %x3E-7E / %x80-FF
    //                          ; octets excluding CTLs, ";", and "="
    //    See |ParsedCookie::IsValidCookieName|.
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
    // 3. Valid RFC 8941 structured field strings, whose values are given by:
    //      string-value   = *( %x20-7E )
    //
    // While all RFC 6265 valid cookie names are valid structured field strings,
    // Chromium accepts cookies whose names can nonetheless not be spelled here.
    // For example, cookie names outside 7-bit ASCII cannot be specified.
    //
    // Nor is every structured field string a valid cookie name, since it may
    // contain a ";" or "=" character (or several other characters excluded by
    // RFC 6265 in addition to Chromium). In the interest of interoperability,
    // those are expressly rejected.
    const std::string& name = item.item.GetString();
    if (name.find_first_of("()<>@,;:\\\"/[]?={} \t") != std::string::npos) {
      // This is one of those structured field strings that is not a valid
      // cookie name according to RFC 6265.
      // TODO(crbug.com/328628231): Watch mnot/I-D#346 to see if a different
      // behavior is agreed on.
      continue;
    }
    CHECK(ParsedCookie::IsValidCookieName(name))
        << "invalid cookie name \"" << name << "\"";
    cookie_names.push_back(name);
  }
  return cookie_names;
}

CookieIndicesHash HashCookieIndices(
    base::span<const std::string> cookie_indices,
    base::span<const std::pair<std::string, std::string>> cookies) {
  CHECK(base::ranges::adjacent_find(cookie_indices, std::greater_equal<>()) ==
        cookie_indices.end())
      << "cookie indices must be sorted and unique";

  std::vector<std::pair<std::string_view, std::string_view>> cookies_sorted(
      cookies.begin(), cookies.end());
  base::ranges::sort(cookies_sorted);

  base::Pickle pickle;
  auto cookies_it = cookies_sorted.begin();
  for (const std::string& cookie_name : cookie_indices) {
    while (cookies_it != cookies_sorted.end() &&
           cookies_it->first < cookie_name) {
      ++cookies_it;
    }
    while (cookies_it != cookies_sorted.end() &&
           cookies_it->first == cookie_name) {
      pickle.WriteBool(true);
      pickle.WriteString(cookies_it->second);
      ++cookies_it;
    }
    pickle.WriteBool(false);
  }
  return crypto::SHA256Hash(pickle.payload_bytes());
}

}  // namespace net
