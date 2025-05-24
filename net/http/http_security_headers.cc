// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>
#include <string_view>

#include "base/base64.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "net/base/parse_number.h"
#include "net/http/http_security_headers.h"
#include "net/http/http_util.h"
#include "url/gurl.h"

namespace net {

namespace {

enum MaxAgeParsing { REQUIRE_MAX_AGE, DO_NOT_REQUIRE_MAX_AGE };

// MaxAgeToLimitedInt converts a string representation of a "whole number" of
// seconds into a uint32_t. The string may contain an arbitrarily large number,
// which will be clipped to a supplied limit and which is guaranteed to fit
// within a 32-bit unsigned integer. False is returned on any parse error.
bool MaxAgeToLimitedInt(std::string_view s, uint32_t limit, uint32_t* result) {
  ParseIntError error;
  if (!ParseUint32(s, ParseIntFormat::NON_NEGATIVE, result, &error)) {
    if (error == ParseIntError::FAILED_OVERFLOW) {
      *result = limit;
    } else {
      return false;
    }
  }

  if (*result > limit)
    *result = limit;

  return true;
}

}  // namespace

// Parse the Strict-Transport-Security header, as currently defined in
// http://tools.ietf.org/html/draft-ietf-websec-strict-transport-sec-14:
//
// Strict-Transport-Security = "Strict-Transport-Security" ":"
//                             [ directive ]  *( ";" [ directive ] )
//
// directive                 = directive-name [ "=" directive-value ]
// directive-name            = token
// directive-value           = token | quoted-string
//
// 1.  The order of appearance of directives is not significant.
//
// 2.  All directives MUST appear only once in an STS header field.
//     Directives are either optional or required, as stipulated in
//     their definitions.
//
// 3.  Directive names are case-insensitive.
//
// 4.  UAs MUST ignore any STS header fields containing directives, or
//     other header field value data, that does not conform to the
//     syntax defined in this specification.
//
// 5.  If an STS header field contains directive(s) not recognized by
//     the UA, the UA MUST ignore the unrecognized directives and if the
//     STS header field otherwise satisfies the above requirements (1
//     through 4), the UA MUST process the recognized directives.
bool ParseHSTSHeader(std::string_view value,
                     base::TimeDelta* max_age,
                     bool* include_subdomains) {
  uint32_t max_age_value = 0;
  bool max_age_seen = false;
  bool include_subdomains_value = false;

  HttpUtil::NameValuePairsIterator hsts_iterator(
      value, ';', HttpUtil::NameValuePairsIterator::Values::NOT_REQUIRED,
      HttpUtil::NameValuePairsIterator::Quotes::STRICT_QUOTES);
  while (hsts_iterator.GetNext()) {
    // Process `max-age`:
    if (base::EqualsCaseInsensitiveASCII(hsts_iterator.name(), "max-age")) {
      // Reject the header if `max-age` is specified more than once.
      if (max_age_seen) {
        return false;
      }
      max_age_seen = true;

      // Reject the header if `max-age`'s value is invalid. Otherwise, store it
      // in `max_age_value`.
      if (!MaxAgeToLimitedInt(hsts_iterator.value(), kMaxHSTSAgeSecs,
                              &max_age_value)) {
        return false;
      }

      // Process `includeSubDomains`:
    } else if (base::EqualsCaseInsensitiveASCII(hsts_iterator.name(),
                                                "includeSubDomains")) {
      // Reject the header if `includeSubDomains` is specified more than once.
      if (include_subdomains_value) {
        return false;
      }
      // Reject the header if `includeSubDomains` has a value specified:
      if (!hsts_iterator.value().empty() || hsts_iterator.value_is_quoted()) {
        return false;
      }

      include_subdomains_value = true;

      // Process unknown directives.
    } else {
      // Reject the header if a directive's name or unquoted value doesn't match
      // the `token` grammar.
      if (!HttpUtil::IsToken(hsts_iterator.name()) ||
          hsts_iterator.name().empty()) {
        return false;
      }
      if (!hsts_iterator.value().empty() && !hsts_iterator.value_is_quoted() &&
          !HttpUtil::IsToken(hsts_iterator.value())) {
        return false;
      }
    }
  }

  if (!hsts_iterator.valid()) {
    return false;
  }

  // Reject the header if no `max-age` was set.
  if (!max_age_seen) {
    return false;
  }

  *max_age = base::Seconds(max_age_value);
  *include_subdomains = include_subdomains_value;
  return true;
}

}  // namespace net
