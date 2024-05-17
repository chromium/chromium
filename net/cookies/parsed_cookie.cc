// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Portions of this code based on Mozilla:
//   (netwerk/cookie/src/nsCookieService.cpp)
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Witte (dwitte@stanford.edu)
 *   Michiel van Leeuwen (mvl@exedo.nl)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "net/cookies/parsed_cookie.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/checked_math.h"
#include "base/strings/string_util.h"
#include "net/base/features.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "net/http/http_util.h"

namespace {

const char kPathTokenName[] = "path";
const char kDomainTokenName[] = "domain";
const char kExpiresTokenName[] = "expires";
const char kMaxAgeTokenName[] = "max-age";
const char kSecureTokenName[] = "secure";
const char kHttpOnlyTokenName[] = "httponly";
const char kSameSiteTokenName[] = "samesite";
const char kPriorityTokenName[] = "priority";
const char kPartitionedTokenName[] = "partitioned";

const char kTerminator[] = "\n\r\0";
const int kTerminatorLen = sizeof(kTerminator) - 1;
const char kWhitespace[] = " \t";
const char kValueSeparator = ';';
const char kTokenSeparator[] = ";=";

// Returns true if |c| occurs in |chars|
// TODO(erikwright): maybe make this take an iterator, could check for end also?
inline bool CharIsA(const char c, const char* chars) {
  return strchr(chars, c) != nullptr;
}

// Seek the iterator to the first occurrence of |character|.
// Returns true if it hits the end, false otherwise.
inline bool SeekToCharacter(std::string::const_iterator* it,
                            const std::string::const_iterator& end,
                            const char character) {
  for (; *it != end && **it != character; ++(*it)) {
  }
  return *it == end;
}

// Seek the iterator to the first occurrence of a character in |chars|.
// Returns true if it hit the end, false otherwise.
inline bool SeekTo(std::string::const_iterator* it,
                   const std::string::const_iterator& end,
                   const char* chars) {
  for (; *it != end && !CharIsA(**it, chars); ++(*it)) {
  }
  return *it == end;
}
// Seek the iterator to the first occurrence of a character not in |chars|.
// Returns true if it hit the end, false otherwise.
inline bool SeekPast(std::string::const_iterator* it,
                     const std::string::const_iterator& end,
                     const char* chars) {
  for (; *it != end && CharIsA(**it, chars); ++(*it)) {
  }
  return *it == end;
}
inline bool SeekBackPast(std::string::const_iterator* it,
                         const std::string::const_iterator& end,
                         const char* chars) {
  for (; *it != end && CharIsA(**it, chars); --(*it)) {
  }
  return *it == end;
}

// Returns the string piece within |value| that is a valid cookie value.
std::string_view ValidStringPieceForValue(const std::string& value) {
  std::string::const_iterator it = value.begin();
  std::string::const_iterator end =
      net::ParsedCookie::FindFirstTerminator(value);
  std::string::const_iterator value_start;
  std::string::const_iterator value_end;

  net::ParsedCookie::ParseValue(&it, end, &value_start, &value_end);

  return base::MakeStringPiece(value_start, value_end);
}

}  // namespace

namespace net {

ParsedCookie::ParsedCookie(const std::string& cookie_line,
                           CookieInclusionStatus* status_out) {
  // Put a pointer on the stack so the rest of the function can assign to it if
  // the default nullptr is passed in.
  CookieInclusionStatus blank_status;
  if (status_out == nullptr) {
    status_out = &blank_status;
  }
  *status_out = CookieInclusionStatus();

  ParseTokenValuePairs(cookie_line, *status_out);
  if (IsValid()) {
    SetupAttributes();
  } else {
    // Status should indicate exclusion if the resulting ParsedCookie is
    // invalid.
    CHECK(!status_out->IsInclude());
  }
}

ParsedCookie::~ParsedCookie() = default;

bool ParsedCookie::IsValid() const {
  return !pairs_.empty();
}

CookieSameSite ParsedCookie::SameSite(
    CookieSameSiteString* samesite_string) const {
  CookieSameSite samesite = CookieSameSite::UNSPECIFIED;
  if (same_site_index_ != 0) {
    samesite = StringToCookieSameSite(pairs_[same_site_index_].second,
                                      samesite_string);
  } else if (samesite_string) {
    *samesite_string = CookieSameSiteString::kUnspecified;
  }
  return samesite;
}

CookiePriority ParsedCookie::Priority() const {
  return (priority_index_ == 0)
             ? COOKIE_PRIORITY_DEFAULT
             : StringToCookiePriority(pairs_[priority_index_].second);
}

bool ParsedCookie::SetName(const std::string& name) {
  const std::string& value = pairs_.empty() ? "" : pairs_[0].second;

  // Ensure there are no invalid characters in `name`. This should be done
  // before calling ParseTokenString because we want terminating characters
  // ('\r', '\n', and '\0') and '=' in `name` to cause a rejection instead of
  // truncation.
  // TODO(crbug.com/40191620) Once we change logic more broadly to reject
  // cookies containing these characters, we should be able to simplify this
  // logic since IsValidCookieNameValuePair() also calls IsValidCookieName().
  // Also, this check will currently fail if `name` has a tab character in the
  // leading or trailing whitespace, which is inconsistent with what happens
  // when parsing a cookie line in the constructor (but the old logic for
  // SetName() behaved this way as well).
  if (!IsValidCookieName(name)) {
    return false;
  }

  // Use the same whitespace trimming code as the constructor.
  const std::string& parsed_name = ParseTokenString(name);

  if (!IsValidCookieNameValuePair(parsed_name, value)) {
    return false;
  }

  if (pairs_.empty())
    pairs_.emplace_back("", "");
  pairs_[0].first = parsed_name;

  return true;
}

bool ParsedCookie::SetValue(const std::string& value) {
  const std::string& name = pairs_.empty() ? "" : pairs_[0].first;

  // Ensure there are no invalid characters in `value`. This should be done
  // before calling ParseValueString because we want terminating characters
  // ('\r', '\n', and '\0') in `value` to cause a rejection instead of
  // truncation.
  // TODO(crbug.com/40191620) Once we change logic more broadly to reject
  // cookies containing these characters, we should be able to simplify this
  // logic since IsValidCookieNameValuePair() also calls IsValidCookieValue().
  // Also, this check will currently fail if `value` has a tab character in
  // the leading or trailing whitespace, which is inconsistent with what
  // happens when parsing a cookie line in the constructor (but the old logic
  // for SetValue() behaved this way as well).
  if (!IsValidCookieValue(value)) {
    return false;
  }

  // Use the same whitespace trimming code as the constructor.
  const std::string& parsed_value = ParseValueString(value);

  if (!IsValidCookieNameValuePair(name, parsed_value)) {
    return false;
  }
  if (pairs_.empty())
    pairs_.emplace_back("", "");
  pairs_[0].second = parsed_value;

  return true;
}

bool ParsedCookie::SetPath(const std::string& path) {
  return SetString(&path_index_, kPathTokenName, path);
}

bool ParsedCookie::SetDomain(const std::string& domain) {
  return SetString(&domain_index_, kDomainTokenName, domain);
}

bool ParsedCookie::SetExpires(const std::string& expires) {
  return SetString(&expires_index_, kExpiresTokenName, expires);
}

bool ParsedCookie::SetMaxAge(const std::string& maxage) {
  return SetString(&maxage_index_, kMaxAgeTokenName, maxage);
}

bool ParsedCookie::SetIsSecure(bool is_secure) {
  return SetBool(&secure_index_, kSecureTokenName, is_secure);
}

bool ParsedCookie::SetIsHttpOnly(bool is_http_only) {
  return SetBool(&httponly_index_, kHttpOnlyTokenName, is_http_only);
}

bool ParsedCookie::SetSameSite(const std::string& same_site) {
  return SetString(&same_site_index_, kSameSiteTokenName, same_site);
}

bool ParsedCookie::SetPriority(const std::string& priority) {
  return SetString(&priority_index_, kPriorityTokenName, priority);
}

bool ParsedCookie::SetIsPartitioned(bool is_partitioned) {
  return SetBool(&partitioned_index_, kPartitionedTokenName, is_partitioned);
}

std::string ParsedCookie::ToCookieLine() const {
  std::string out;
  for (auto it = pairs_.begin(); it != pairs_.end(); ++it) {
    if (!out.empty())
      out.append("; ");
    out.append(it->first);
    // Determine whether to emit the pair's value component. We should always
    // print it for the first pair(see crbug.com/977619). After the first pair,
    // we need to consider whether the name component is a special token.
    if (it == pairs_.begin() ||
        (it->first != kSecureTokenName && it->first != kHttpOnlyTokenName &&
         it->first != kPartitionedTokenName)) {
      out.append("=");
      out.append(it->second);
    }
  }
  return out;
}

// static
std::string::const_iterator ParsedCookie::FindFirstTerminator(
    const std::string& s) {
  std::string::const_iterator end = s.end();
  size_t term_pos = s.find_first_of(std::string(kTerminator, kTerminatorLen));
  if (term_pos != std::string::npos) {
    // We found a character we should treat as an end of string.
    end = s.begin() + term_pos;
  }
  return end;
}

// static
bool ParsedCookie::ParseToken(std::string::const_iterator* it,
                              const std::string::const_iterator& end,
                              std::string::const_iterator* token_start,
                              std::string::const_iterator* token_end) {
  DCHECK(it && token_start && token_end);
  std::string::const_iterator token_real_end;

  // Seek past any whitespace before the "token" (the name).
  // token_start should point at the first character in the token
  if (SeekPast(it, end, kWhitespace))
    return false;  // No token, whitespace or empty.
  *token_start = *it;

  // Seek over the token, to the token separator.
  // token_real_end should point at the token separator, i.e. '='.
  // If it == end after the seek, we probably have a token-value.
  SeekTo(it, end, kTokenSeparator);
  token_real_end = *it;

  // Ignore any whitespace between the token and the token separator.
  // token_end should point after the last interesting token character,
  // pointing at either whitespace, or at '=' (and equal to token_real_end).
  if (*it != *token_start) {  // We could have an empty token name.
    --(*it);                  // Go back before the token separator.
    // Skip over any whitespace to the first non-whitespace character.
    SeekBackPast(it, *token_start, kWhitespace);
    // Point after it.
    ++(*it);
  }
  *token_end = *it;

  // Seek us back to the end of the token.
  *it = token_real_end;
  return true;
}

// static
void ParsedCookie::ParseValue(std::string::const_iterator* it,
                              const std::string::const_iterator& end,
                              std::string::const_iterator* value_start,
                              std::string::const_iterator* value_end) {
  DCHECK(it && value_start && value_end);

  // Seek past any whitespace that might be in-between the token and value.
  SeekPast(it, end, kWhitespace);
  // value_start should point at the first character of the value.
  *value_start = *it;

  // Just look for ';' to terminate ('=' allowed).
  // We can hit the end, maybe they didn't terminate.
  SeekToCharacter(it, end, kValueSeparator);

  // Will point at the ; separator or the end.
  *value_end = *it;

  // Ignore any unwanted whitespace after the value.
  if (*value_end != *value_start) {  // Could have an empty value
    --(*value_end);
    // Skip over any whitespace to the first non-whitespace character.
    SeekBackPast(value_end, *value_start, kWhitespace);
    // Point after it.
    ++(*value_end);
  }
}

// static
std::string ParsedCookie::ParseTokenString(const std::string& token) {
  std::string::const_iterator it = token.begin();
  std::string::const_iterator end = FindFirstTerminator(token);

  std::string::const_iterator token_start, token_end;
  if (ParseToken(&it, end, &token_start, &token_end))
    return std::string(token_start, token_end);
  return std::string();
}

// static
std::string ParsedCookie::ParseValueString(const std::string& value) {
  return std::string(ValidStringPieceForValue(value));
}

// static
bool ParsedCookie::ValueMatchesParsedValue(const std::string& value) {
  // ValidStringPieceForValue() returns a valid substring of |value|.
  // If |value| can be fully parsed the result will have the same length
  // as |value|.
  return ValidStringPieceForValue(value).length() == value.length();
}

// static
bool ParsedCookie::IsValidCookieName(const std::string& name) {
  // IsValidCookieName() returns whether a string matches the following
  // grammar:
  //
  // cookie-name       = *cookie-name-octet
  // cookie-name-octet = %x20-3A / %x3C / %x3E-7E / %x80-FF
  //                       ; octets excluding CTLs, ";", and "="
  //
  // This can be used to determine whether cookie names and cookie attribute
  // names contain any invalid characters.
  //
  // Note that RFC6265bis section 4.1.1 suggests a stricter grammar for
  // parsing cookie names, but we choose to allow a wider range of characters
  // than what's allowed by that grammar (while still conforming to the
  // requirements of the parsing algorithm defined in section 5.2).
  //
  // For reference, see:
  //  - https://crbug.com/238041
  for (char i : name) {
    if (HttpUtil::IsControlChar(i) || i == ';' || i == '=')
      return false;
  }
  return true;
}

// static
bool ParsedCookie::IsValidCookieValue(const std::string& value) {
  // IsValidCookieValue() returns whether a string matches the following
  // grammar:
  //
  // cookie-value       = *cookie-value-octet
  // cookie-value-octet = %x20-3A / %x3C-7E / %x80-FF
  //                       ; octets excluding CTLs and ";"
  //
  // This can be used to determine whether cookie values contain any invalid
  // characters.
  //
  // Note that RFC6265bis section 4.1.1 suggests a stricter grammar for
  // parsing cookie values, but we choose to allow a wider range of characters
  // than what's allowed by that grammar (while still conforming to the
  // requirements of the parsing algorithm defined in section 5.2).
  //
  // For reference, see:
  //  - https://crbug.com/238041
  for (char i : value) {
    if (HttpUtil::IsControlChar(i) || i == ';')
      return false;
  }
  return true;
}

// static
bool ParsedCookie::CookieAttributeValueHasValidCharSet(
    const std::string& value) {
  // A cookie attribute value has the same character set restrictions as cookie
  // values, so re-use the validation function for that.
  return IsValidCookieValue(value);
}

// static
bool ParsedCookie::CookieAttributeValueHasValidSize(const std::string& value) {
  return (value.size() <= kMaxCookieAttributeValueSize);
}

// static
bool ParsedCookie::IsValidCookieNameValuePair(
    const std::string& name,
    const std::string& value,
    CookieInclusionStatus* status_out) {
  // Ignore cookies with neither name nor value.
  if (name.empty() && value.empty()) {
    if (status_out != nullptr) {
      status_out->AddExclusionReason(
          CookieInclusionStatus::EXCLUDE_NO_COOKIE_CONTENT);
    }
    // TODO(crbug.com/40189703) Note - if the exclusion reasons change to no
    // longer be the same, we'll need to not return right away and evaluate all
    // of the checks.
    return false;
  }

  // Enforce a length limit for name + value per RFC6265bis.
  base::CheckedNumeric<size_t> name_value_pair_size = name.size();
  name_value_pair_size += value.size();
  if (!name_value_pair_size.IsValid() ||
      (name_value_pair_size.ValueOrDie() > kMaxCookieNamePlusValueSize)) {
    if (status_out != nullptr) {
      status_out->AddExclusionReason(
          CookieInclusionStatus::EXCLUDE_NAME_VALUE_PAIR_EXCEEDS_MAX_SIZE);
    }
    return false;
  }

  // Ignore Set-Cookie directives containing control characters. See
  // http://crbug.com/238041.
  if (!IsValidCookieName(name) || !IsValidCookieValue(value)) {
    if (status_out != nullptr) {
      status_out->AddExclusionReason(
          CookieInclusionStatus::EXCLUDE_DISALLOWED_CHARACTER);
    }
    return false;
  }
  return true;
}

// Parse all token/value pairs and populate pairs_.
void ParsedCookie::ParseTokenValuePairs(const std::string& cookie_line,
                                        CookieInclusionStatus& status_out) {
  pairs_.clear();

  // Ok, here we go.  We should be expecting to be starting somewhere
  // before the cookie line, not including any header name...
  std::string::const_iterator start = cookie_line.begin();
  std::string::const_iterator it = start;

  // TODO(erikwright): Make sure we're stripping \r\n in the network code.
  // Then we can log any unexpected terminators.
  std::string::const_iterator end = FindFirstTerminator(cookie_line);

  // Block cookies that were truncated by control characters.
  if (end < cookie_line.end()) {
    status_out.AddExclusionReason(
        CookieInclusionStatus::EXCLUDE_DISALLOWED_CHARACTER);
    return;
  }

  // Exit early for an empty cookie string.
  if (it == end) {
    status_out.AddExclusionReason(
        CookieInclusionStatus::EXCLUDE_NO_COOKIE_CONTENT);
    return;
  }

  for (int pair_num = 0; it != end; ++pair_num) {
    TokenValuePair pair;

    std::string::const_iterator token_start, token_end;
    if (!ParseToken(&it, end, &token_start, &token_end)) {
      // Allow first token to be treated as empty-key if unparsable
      if (pair_num != 0)
        break;

      // If parsing failed, start the value parsing at the very beginning.
      token_start = start;
    }

    if (it == end || *it != '=') {
      // We have a token-value, we didn't have any token name.
      if (pair_num == 0) {
        // For the first time around, we want to treat single values
        // as a value with an empty name. (Mozilla bug 169091).
        // IE seems to also have this behavior, ex "AAA", and "AAA=10" will
        // set 2 different cookies, and setting "BBB" will then replace "AAA".
        pair.first = "";
        // Rewind to the beginning of what we thought was the token name,
        // and let it get parsed as a value.
        it = token_start;
      } else {
        // Any not-first attribute we want to treat a value as a
        // name with an empty value...  This is so something like
        // "secure;" will get parsed as a Token name, and not a value.
        pair.first = std::string(token_start, token_end);
      }
    } else {
      // We have a TOKEN=VALUE.
      pair.first = std::string(token_start, token_end);
      ++it;  // Skip past the '='.
    }

    // OK, now try to parse a value.
    std::string::const_iterator value_start, value_end;
    ParseValue(&it, end, &value_start, &value_end);

    // OK, we're finished with a Token/Value.
    pair.second = std::string(value_start, value_end);

    // For metrics, check if either the name or value contain an internal HTAB
    // (0x9). That is, not leading or trailing.
    if (pair_num == 0 &&
        (pair.first.find_first_of("\t") != std::string::npos ||
         pair.second.find_first_of("\t") != std::string::npos)) {
      internal_htab_ = true;
    }

    bool ignore_pair = false;
    if (pair_num == 0) {
      if (!IsValidCookieNameValuePair(pair.first, pair.second, &status_out)) {
        pairs_.clear();
        break;
      }
    } else {
      // From RFC2109: "Attributes (names) (attr) are case-insensitive."
      pair.first = base::ToLowerASCII(pair.first);

      // Attribute names have the same character set limitations as cookie
      // names, but only a handful of values are allowed. We don't check that
      // this attribute name is one of the allowed ones here, so just re-use
      // the cookie name check.
      if (!IsValidCookieName(pair.first)) {
        status_out.AddExclusionReason(
            CookieInclusionStatus::EXCLUDE_DISALLOWED_CHARACTER);
        pairs_.clear();
        break;
      }

      if (!CookieAttributeValueHasValidCharSet(pair.second)) {
        // If the attribute value contains invalid characters, the whole
        // cookie should be ignored.
        status_out.AddExclusionReason(
            CookieInclusionStatus::EXCLUDE_DISALLOWED_CHARACTER);
        pairs_.clear();
        break;
      }

      if (!CookieAttributeValueHasValidSize(pair.second)) {
        // If the attribute value is too large, it should be ignored.
        ignore_pair = true;
        status_out.AddWarningReason(
            CookieInclusionStatus::WARN_ATTRIBUTE_VALUE_EXCEEDS_MAX_SIZE);
      }
    }

    if (!ignore_pair) {
      pairs_.push_back(pair);
    }

    // We've processed a token/value pair, we're either at the end of
    // the string or a ValueSeparator like ';', which we want to skip.
    if (it != end)
      ++it;
  }
}

void ParsedCookie::SetupAttributes() {
  // We skip over the first token/value, the user supplied one.
  for (size_t i = 1; i < pairs_.size(); ++i) {
    if (pairs_[i].first == kPathTokenName) {
      path_index_ = i;
    } else if (pairs_[i].first == kDomainTokenName) {
      domain_index_ = i;
    } else if (pairs_[i].first == kExpiresTokenName) {
      expires_index_ = i;
    } else if (pairs_[i].first == kMaxAgeTokenName) {
      maxage_index_ = i;
    } else if (pairs_[i].first == kSecureTokenName) {
      secure_index_ = i;
    } else if (pairs_[i].first == kHttpOnlyTokenName) {
      httponly_index_ = i;
    } else if (pairs_[i].first == kSameSiteTokenName) {
      same_site_index_ = i;
    } else if (pairs_[i].first == kPriorityTokenName) {
      priority_index_ = i;
    } else if (pairs_[i].first == kPartitionedTokenName) {
      partitioned_index_ = i;
    } else {
      /* some attribute we don't know or don't care about. */
    }
  }
}

bool ParsedCookie::SetString(size_t* index,
                             const std::string& key,
                             const std::string& untrusted_value) {
  // This function should do equivalent input validation to the
  // constructor. Otherwise, the Set* functions can put this ParsedCookie in a
  // state where parsing the output of ToCookieLine() produces a different
  // ParsedCookie.
  //
  // Without input validation, invoking pc.SetPath(" baz ") would result in
  // pc.ToCookieLine() == "path= baz ". Parsing the "path= baz " string would
  // produce a cookie with "path" attribute equal to "baz" (no spaces). We
  // should not produce cookie lines that parse to different key/value pairs!

  // Inputs containing invalid characters or attribute value strings that are
  // too large should be ignored. Note that we check the attribute value size
  // after removing leading and trailing whitespace.
  if (!CookieAttributeValueHasValidCharSet(untrusted_value))
    return false;

  // Use the same whitespace trimming code as the constructor.
  const std::string parsed_value = ParseValueString(untrusted_value);

  if (!CookieAttributeValueHasValidSize(parsed_value))
    return false;

  if (parsed_value.empty()) {
    ClearAttributePair(*index);
    return true;
  } else {
    return SetAttributePair(index, key, parsed_value);
  }
}

bool ParsedCookie::SetBool(size_t* index, const std::string& key, bool value) {
  if (!value) {
    ClearAttributePair(*index);
    return true;
  } else {
    return SetAttributePair(index, key, std::string());
  }
}

bool ParsedCookie::SetAttributePair(size_t* index,
                                    const std::string& key,
                                    const std::string& value) {
  if (!HttpUtil::IsToken(key))
    return false;
  if (!IsValid())
    return false;
  if (*index) {
    pairs_[*index].second = value;
  } else {
    pairs_.emplace_back(key, value);
    *index = pairs_.size() - 1;
  }
  return true;
}

void ParsedCookie::ClearAttributePair(size_t index) {
  // The first pair (name/value of cookie at pairs_[0]) cannot be cleared.
  // Cookie attributes that don't have a value at the moment, are
  // represented with an index being equal to 0.
  if (index == 0)
    return;

  size_t* indexes[] = {
      &path_index_,      &domain_index_,   &expires_index_,
      &maxage_index_,    &secure_index_,   &httponly_index_,
      &same_site_index_, &priority_index_, &partitioned_index_};
  for (size_t* attribute_index : indexes) {
    if (*attribute_index == index)
      *attribute_index = 0;
    else if (*attribute_index > index)
      --(*attribute_index);
  }
  pairs_.erase(pairs_.begin() + index);
}

}  // namespace net
