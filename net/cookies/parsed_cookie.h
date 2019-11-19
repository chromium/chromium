// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_PARSED_COOKIE_H_
#define NET_COOKIES_PARSED_COOKIE_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "net/base/net_export.h"
#include "net/cookies/cookie_constants.h"

namespace net {

class NET_EXPORT ParsedCookie {
 public:
  typedef std::pair<std::string, std::string> TokenValuePair;
  typedef std::vector<TokenValuePair> PairList;

  // The maximum length of a cookie string we will try to parse
  static const size_t kMaxCookieSize = 4096;

  // Construct from a cookie string like "BLAH=1; path=/; domain=.google.com"
  // Format is according to RFC 6265. Cookies with both name and value empty
  // will be considered invalid.
  ParsedCookie(const std::string& cookie_line);
  ~ParsedCookie();

  // You should not call any other methods except for SetName/SetValue on the
  // class if !IsValid.
  bool IsValid() const;

  const std::string& Name() const { return pairs_[0].first; }
  const std::string& Token() const { return Name(); }
  const std::string& Value() const { return pairs_[0].second; }

  bool HasPath() const { return path_index_ != 0; }
  const std::string& Path() const { return pairs_[path_index_].second; }
  bool HasDomain() const { return domain_index_ != 0; }
  const std::string& Domain() const { return pairs_[domain_index_].second; }
  bool HasExpires() const { return expires_index_ != 0; }
  const std::string& Expires() const { return pairs_[expires_index_].second; }
  bool HasMaxAge() const { return maxage_index_ != 0; }
  const std::string& MaxAge() const { return pairs_[maxage_index_].second; }
  bool IsSecure() const { return secure_index_ != 0; }
  bool IsHttpOnly() const { return httponly_index_ != 0; }
  // Also spits out an enum value representing the string given as the SameSite
  // attribute value, if |samesite_string| is non-null.
  CookieSameSite SameSite(
      CookieSameSiteString* samesite_string = nullptr) const;
  CookiePriority Priority() const;

  // Returns the number of attributes, for example, returning 2 for:
  //   "BLAH=hah; path=/; domain=.google.com"
  size_t NumberOfAttributes() const { return pairs_.size() - 1; }

  // These functions set the respective properties of the cookie. If the
  // parameters are empty, the respective properties are cleared.
  // The functions return false in case an error occurred.
  // The cookie needs to be assigned a name/value before setting the other
  // attributes.
  bool SetName(const std::string& name);
  bool SetValue(const std::string& value);
  bool SetPath(const std::string& path);
  bool SetDomain(const std::string& domain);
  bool SetExpires(const std::string& expires);
  bool SetMaxAge(const std::string& maxage);
  bool SetIsSecure(bool is_secure);
  bool SetIsHttpOnly(bool is_http_only);
  bool SetSameSite(const std::string& same_site);
  bool SetPriority(const std::string& priority);

  // Returns the cookie description as it appears in a HTML response header.
  std::string ToCookieLine() const;

  // Returns an iterator pointing to the first terminator character found in
  // the given string.
  static std::string::const_iterator FindFirstTerminator(const std::string& s);

  // Given iterators pointing to the beginning and end of a string segment,
  // returns as output arguments token_start and token_end to the start and end
  // positions of a cookie attribute token name parsed from the segment, and
  // updates the segment iterator to point to the next segment to be parsed.
  // If no token is found, the function returns false and the segment iterator
  // is set to end.
  static bool ParseToken(std::string::const_iterator* it,
                         const std::string::const_iterator& end,
                         std::string::const_iterator* token_start,
                         std::string::const_iterator* token_end);

  // Given iterators pointing to the beginning and end of a string segment,
  // returns as output arguments value_start and value_end to the start and end
  // positions of a cookie attribute value parsed from the segment, and updates
  // the segment iterator to point to the next segment to be parsed.
  static void ParseValue(std::string::const_iterator* it,
                         const std::string::const_iterator& end,
                         std::string::const_iterator* value_start,
                         std::string::const_iterator* value_end);

  // Same as the above functions, except the input is assumed to contain the
  // desired token/value and nothing else.
  static std::string ParseTokenString(const std::string& token);
  static std::string ParseValueString(const std::string& value);

  // Is the string valid as the value of a cookie attribute?
  static bool IsValidCookieAttributeValue(const std::string& value);

 private:
  void ParseTokenValuePairs(const std::string& cookie_line);
  void SetupAttributes();

  // Sets a key/value pair for a cookie. |index| has to point to one of the
  // |*_index_| fields in ParsedCookie and is updated to the position where
  // the key/value pair is set in |pairs_|. Accordingly, |key| has to correspond
  // to the token matching |index|. If |value| contains invalid characters, the
  // cookie parameter is not changed and the function returns false.
  // If |value| is empty/false the key/value pair is removed.
  bool SetString(size_t* index,
                 const std::string& key,
                 const std::string& value);
  bool SetBool(size_t* index, const std::string& key, bool value);

  // Helper function for SetString and SetBool handling the case that the
  // key/value pair shall not be removed.
  bool SetAttributePair(size_t* index,
                        const std::string& key,
                        const std::string& value);

  // Removes the key/value pair from a cookie that is identified by |index|.
  // |index| refers to a position in |pairs_|.
  void ClearAttributePair(size_t index);

  PairList pairs_;
  // These will default to 0, but that should never be valid since the
  // 0th index is the user supplied token/value, not an attribute.
  // We're really never going to have more than like 8 attributes, so we
  // could fit these into 3 bits each if we're worried about size...
  size_t path_index_;
  size_t domain_index_;
  size_t expires_index_;
  size_t maxage_index_;
  size_t secure_index_;
  size_t httponly_index_;
  size_t same_site_index_;
  size_t priority_index_;

  DISALLOW_COPY_AND_ASSIGN(ParsedCookie);
};

}  // namespace net

#endif  // NET_COOKIES_PARSED_COOKIE_H_
