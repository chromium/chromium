// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "base/strings/string_util.h"
#include "net/cookies/cookie_constants.h"
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

const char kTerminator[] = "\n\r\0";
const int kTerminatorLen = sizeof(kTerminator) - 1;
const char kWhitespace[] = " \t";
const char kValueSeparator[] = ";";
const char kTokenSeparator[] = ";=";

// Returns true if |c| occurs in |chars|
// TODO(erikwright): maybe make this take an iterator, could check for end also?
inline bool CharIsA(const char c, const char* chars) {
  return strchr(chars, c) != nullptr;
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

// Validate value, which may be according to RFC 6265
// cookie-value      = *cookie-octet / ( DQUOTE *cookie-octet DQUOTE )
// cookie-octet      = %x21 / %x23-2B / %x2D-3A / %x3C-5B / %x5D-7E
//                      ; US-ASCII characters excluding CTLs,
//                      ; whitespace DQUOTE, comma, semicolon,
//                      ; and backslash
bool IsValidCookieValue(const std::string& value) {
  // Number of characters to skip in validation at beginning and end of string.
  size_t skip = 0;
  if (value.size() >= 2 && *value.begin() == '"' && *(value.end() - 1) == '"')
    skip = 1;
  for (std::string::const_iterator i = value.begin() + skip;
       i != value.end() - skip; ++i) {
    bool valid_octet =
        (*i == 0x21 || (*i >= 0x23 && *i <= 0x2B) ||
         (*i >= 0x2D && *i <= 0x3A) || (*i >= 0x3C && *i <= 0x5B) ||
         (*i >= 0x5D && *i <= 0x7E));
    if (!valid_octet)
      return false;
  }
  return true;
}

bool IsControlCharacter(unsigned char c) {
  return c <= 31;
}

}  // namespace

namespace net {

ParsedCookie::ParsedCookie(const std::string& cookie_line)
    : path_index_(0),
      domain_index_(0),
      expires_index_(0),
      maxage_index_(0),
      secure_index_(0),
      httponly_index_(0),
      same_site_index_(0),
      priority_index_(0) {
  if (cookie_line.size() > kMaxCookieSize) {
    DVLOG(1) << "Not parsing cookie, too large: " << cookie_line.size();
    return;
  }

  ParseTokenValuePairs(cookie_line);
  if (!pairs_.empty())
    SetupAttributes();
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
  if (!name.empty() && !HttpUtil::IsToken(name))
    return false;
  if (pairs_.empty())
    pairs_.push_back(std::make_pair("", ""));
  pairs_[0].first = name;
  return true;
}

bool ParsedCookie::SetValue(const std::string& value) {
  if (!IsValidCookieValue(value))
    return false;
  if (pairs_.empty())
    pairs_.push_back(std::make_pair("", ""));
  pairs_[0].second = value;
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
        (it->first != kSecureTokenName && it->first != kHttpOnlyTokenName)) {
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

  // Seek past any whitespace that might in-between the token and value.
  SeekPast(it, end, kWhitespace);
  // value_start should point at the first character of the value.
  *value_start = *it;

  // Just look for ';' to terminate ('=' allowed).
  // We can hit the end, maybe they didn't terminate.
  SeekTo(it, end, kValueSeparator);

  // Will be pointed at the ; seperator or the end.
  *value_end = *it;

  // Ignore any unwanted whitespace after the value.
  if (*value_end != *value_start) {  // Could have an empty value
    --(*value_end);
    SeekBackPast(value_end, *value_start, kWhitespace);
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
  std::string::const_iterator it = value.begin();
  std::string::const_iterator end = FindFirstTerminator(value);

  std::string::const_iterator value_start, value_end;
  ParseValue(&it, end, &value_start, &value_end);
  return std::string(value_start, value_end);
}

// static
bool ParsedCookie::IsValidCookieAttributeValue(const std::string& value) {
  // The greatest common denominator of cookie attribute values is
  // <any CHAR except CTLs or ";"> according to RFC 6265.
  for (std::string::const_iterator i = value.begin(); i != value.end(); ++i) {
    if (IsControlCharacter(*i) || *i == ';')
      return false;
  }
  return true;
}

// Parse all token/value pairs and populate pairs_.
void ParsedCookie::ParseTokenValuePairs(const std::string& cookie_line) {
  pairs_.clear();

  // Ok, here we go.  We should be expecting to be starting somewhere
  // before the cookie line, not including any header name...
  std::string::const_iterator start = cookie_line.begin();
  std::string::const_iterator it = start;

  // TODO(erikwright): Make sure we're stripping \r\n in the network code.
  // Then we can log any unexpected terminators.
  std::string::const_iterator end = FindFirstTerminator(cookie_line);

  // For an empty |cookie_line|, add an empty-key with an empty value, which
  // has the effect of clearing any prior setting of the empty-key. This is done
  // to match the behavior of other browsers. See https://crbug.com/601786.
  if (it == end) {
    pairs_.push_back(TokenValuePair("", ""));
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

    // From RFC2109: "Attributes (names) (attr) are case-insensitive."
    if (pair_num != 0)
      pair.first = base::ToLowerASCII(pair.first);
    // Ignore Set-Cookie directives contaning control characters. See
    // http://crbug.com/238041.
    if (!IsValidCookieAttributeValue(pair.first) ||
        !IsValidCookieAttributeValue(pair.second)) {
      pairs_.clear();
      break;
    }

    pairs_.push_back(pair);

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
    } else if (pairs_[i].first == kDomainTokenName && pairs_[i].second != "") {
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

  // Inputs containing invalid characters should be ignored.
  if (!IsValidCookieAttributeValue(untrusted_value))
    return false;

  // Use the same whitespace trimming code as the constructor.
  const std::string parsed_value = ParseValueString(untrusted_value);
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
    pairs_.push_back(std::make_pair(key, value));
    *index = pairs_.size() - 1;
  }
  return true;
}

void ParsedCookie::ClearAttributePair(size_t index) {
  // The first pair (name/value of cookie at pairs_[0]) cannot be cleared.
  // Cookie attributes that don't have a value at the moment, are represented
  // with an index being equal to 0.
  if (index == 0)
    return;

  size_t* indexes[] = {&path_index_,      &domain_index_,  &expires_index_,
                       &maxage_index_,    &secure_index_,  &httponly_index_,
                       &same_site_index_, &priority_index_};
  for (size_t* attribute_index : indexes) {
    if (*attribute_index == index)
      *attribute_index = 0;
    else if (*attribute_index > index)
      --(*attribute_index);
  }
  pairs_.erase(pairs_.begin() + index);
}

}  // namespace net
