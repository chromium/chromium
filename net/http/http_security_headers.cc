// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "base/base64.h"
#include "base/strings/string_piece.h"
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
bool MaxAgeToLimitedInt(base::StringPiece s, uint32_t limit, uint32_t* result) {
  ParseIntError error;
  if (!ParseUint32(s, result, &error)) {
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
bool ParseHSTSHeader(const std::string& value,
                     base::TimeDelta* max_age,
                     bool* include_subdomains) {
  uint32_t max_age_candidate = 0;
  bool include_subdomains_candidate = false;

  // We must see max-age exactly once.
  int max_age_observed = 0;
  // We must see includeSubdomains exactly 0 or 1 times.
  int include_subdomains_observed = 0;

  enum ParserState {
    START,
    AFTER_MAX_AGE_LABEL,
    AFTER_MAX_AGE_EQUALS,
    AFTER_MAX_AGE,
    AFTER_INCLUDE_SUBDOMAINS,
    AFTER_UNKNOWN_LABEL,
    DIRECTIVE_END
  } state = START;

  base::StringTokenizer tokenizer(value, " \t=;");
  tokenizer.set_options(base::StringTokenizer::RETURN_DELIMS);
  tokenizer.set_quote_chars("\"");
  std::string unquoted;
  while (tokenizer.GetNext()) {
    base::StringPiece token = tokenizer.token_piece();
    DCHECK(!tokenizer.token_is_delim() || token.length() == 1);
    switch (state) {
      case START:
      case DIRECTIVE_END:
        if (base::IsAsciiWhitespace(token[0]))
          continue;
        if (base::LowerCaseEqualsASCII(token, "max-age")) {
          state = AFTER_MAX_AGE_LABEL;
          max_age_observed++;
        } else if (base::LowerCaseEqualsASCII(token, "includesubdomains")) {
          state = AFTER_INCLUDE_SUBDOMAINS;
          include_subdomains_observed++;
          include_subdomains_candidate = true;
        } else {
          state = AFTER_UNKNOWN_LABEL;
        }
        break;

      case AFTER_MAX_AGE_LABEL:
        if (base::IsAsciiWhitespace(token[0]))
          continue;
        if (token[0] != '=')
          return false;
        DCHECK_EQ(token.length(), 1U);
        state = AFTER_MAX_AGE_EQUALS;
        break;

      case AFTER_MAX_AGE_EQUALS:
        if (base::IsAsciiWhitespace(token[0]))
          continue;
        unquoted = HttpUtil::Unquote(token);
        if (!MaxAgeToLimitedInt(unquoted, kMaxHSTSAgeSecs, &max_age_candidate))
          return false;
        state = AFTER_MAX_AGE;
        break;

      case AFTER_MAX_AGE:
      case AFTER_INCLUDE_SUBDOMAINS:
        if (base::IsAsciiWhitespace(token[0]))
          continue;
        else if (token[0] == ';')
          state = DIRECTIVE_END;
        else
          return false;
        break;

      case AFTER_UNKNOWN_LABEL:
        // Consume and ignore the post-label contents (if any).
        if (token[0] != ';')
          continue;
        state = DIRECTIVE_END;
        break;
    }
  }

  // We've consumed all the input. Let's see what state we ended up in.
  if (max_age_observed != 1 ||
      (include_subdomains_observed != 0 && include_subdomains_observed != 1)) {
    return false;
  }

  switch (state) {
    case DIRECTIVE_END:
    case AFTER_MAX_AGE:
    case AFTER_INCLUDE_SUBDOMAINS:
    case AFTER_UNKNOWN_LABEL:
      *max_age = base::TimeDelta::FromSeconds(max_age_candidate);
      *include_subdomains = include_subdomains_candidate;
      return true;
    case START:
    case AFTER_MAX_AGE_LABEL:
    case AFTER_MAX_AGE_EQUALS:
      return false;
    default:
      NOTREACHED();
      return false;
  }
}

// "Expect-CT" ":"
//     "max-age" "=" delta-seconds
//     [ "," "enforce" ]
//     [ "," "report-uri" "=" absolute-URI ]
bool ParseExpectCTHeader(const std::string& value,
                         base::TimeDelta* max_age,
                         bool* enforce,
                         GURL* report_uri) {
  bool parsed_max_age = false;
  bool enforce_candidate = false;
  bool has_report_uri = false;
  uint32_t max_age_candidate = 0;
  GURL parsed_report_uri;

  HttpUtil::NameValuePairsIterator name_value_pairs(
      value.begin(), value.end(), ',',
      HttpUtil::NameValuePairsIterator::Values::NOT_REQUIRED,
      // Use STRICT_QUOTES because "UAs must not attempt to fix malformed header
      // fields."
      HttpUtil::NameValuePairsIterator::Quotes::STRICT_QUOTES);

  while (name_value_pairs.GetNext()) {
    base::StringPiece name = name_value_pairs.name_piece();
    if (base::LowerCaseEqualsASCII(name, "max-age")) {
      // "A given directive MUST NOT appear more than once in a given header
      // field."
      if (parsed_max_age)
        return false;
      if (!MaxAgeToLimitedInt(name_value_pairs.value_piece(),
                              kMaxExpectCTAgeSecs, &max_age_candidate)) {
        return false;
      }
      parsed_max_age = true;
    } else if (base::LowerCaseEqualsASCII(name, "enforce")) {
      // "A given directive MUST NOT appear more than once in a given header
      // field."
      if (enforce_candidate)
        return false;
      if (!name_value_pairs.value_piece().empty())
        return false;
      enforce_candidate = true;
    } else if (base::LowerCaseEqualsASCII(name, "report-uri")) {
      // "A given directive MUST NOT appear more than once in a given header
      // field."
      if (has_report_uri)
        return false;

      has_report_uri = true;
      parsed_report_uri = GURL(name_value_pairs.value_piece());
      if (parsed_report_uri.is_empty() || !parsed_report_uri.is_valid())
        return false;
    } else {
      // Silently ignore unknown directives for forward compatibility.
    }
  }

  if (!name_value_pairs.valid())
    return false;

  if (!parsed_max_age)
    return false;

  *max_age = base::TimeDelta::FromSeconds(max_age_candidate);
  *enforce = enforce_candidate;
  *report_uri = parsed_report_uri;
  return true;
}

}  // namespace net
