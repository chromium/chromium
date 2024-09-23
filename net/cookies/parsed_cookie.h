// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_PARSED_COOKIE_H_
#define NET_COOKIES_PARSED_COOKIE_H_

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "net/base/net_export.h"
#include "net/cookies/cookie_constants.h"

namespace net {

class CookieInclusionStatus;

class NET_EXPORT ParsedCookie {
 public:
  typedef std::pair<std::string, std::string> TokenValuePair;
  typedef std::vector<TokenValuePair> PairList;

  // The maximum length allowed for a cookie string's name/value pair.
  static const size_t kMaxCookieNamePlusValueSize = 4096;

  // The maximum length allowed for each attribute value in a cookie string.
  static const size_t kMaxCookieAttributeValueSize = 1024;

  // Construct from a cookie string like "BLAH=1; path=/; domain=.google.com"
  // Format is according to RFC6265bis. Cookies with both name and value empty
  // will be considered invalid.
  // `status_out` is a nullable output param which will be populated with
  // informative exclusion reasons if the resulting ParsedCookie is invalid.
  // The CookieInclusionStatus will not be altered if the resulting ParsedCookie
  // is valid.
  explicit ParsedCookie(const std::string& cookie_line,
                        CookieInclusionStatus* status_out = nullptr);

  ParsedCookie(const ParsedCookie&) = delete;
  ParsedCookie& operator=(const ParsedCookie&) = delete;

  ~ParsedCookie();

  // You should not call any other methods except for SetName/SetValue on the
  // class if !IsValid.
  bool IsValid() const;

  const std::string& Name() const { return pairs_[0].first; }
  const std::string& Token() const { return Name(); }
  const std::string& Value() const { return pairs_[0].second; }

  bool HasPath() const { return path_index_ != 0; }
  const std::string& Path() const {
    DCHECK(HasPath());
    return pairs_[path_index_].second;
  }
  // Note that Domain() may return the empty string; in the case of cookie_line
  // "domain=", HasDomain() will return true (as the empty string is an
  // acceptable domain value), so Domain() will return std::string().
  bool HasDomain() const { return domain_index_ != 0; }
  const std::string& Domain() const {
    DCHECK(HasDomain());
    return pairs_[domain_index_].second;
  }
  bool HasExpires() const { return expires_index_ != 0; }
  const std::string& Expires() const {
    DCHECK(HasExpires());
    return pairs_[expires_index_].second;
  }
  bool HasMaxAge() const { return maxage_index_ != 0; }
  const std::string& MaxAge() const {
    DCHECK(HasMaxAge());
    return pairs_[maxage_index_].second;
  }
  bool IsSecure() const { return secure_index_ != 0; }
  bool IsHttpOnly() const { return httponly_index_ != 0; }
  // Also spits out an enum value representing the string given as the SameSite
  // attribute value, if |samesite_string| is non-null.
  CookieSameSite SameSite(
      CookieSameSiteString* samesite_string = nullptr) const;
  CookiePriority Priority() const;
  bool IsPartitioned() const { return partitioned_index_ != 0; }
  bool HasInternalHtab() const { return internal_htab_; }
  // Returns the number of attributes, for example, returning 2 for:
  //   "BLAH=hah; path=/; domain=.google.com"
  size_t NumberOfAttributes() const { return pairs_.size() - 1; }

  // These functions set the respective properties of the cookie. If the
  // parameters are empty, the respective properties are cleared.
  // The functions return false in case an error occurred.
  // The cookie needs to be assigned a name/value before setting the other
  // attributes.
  //
  // These functions should only be used if you need to modify a response's
  // Set-Cookie string. The resulting ParsedCookie and its Set-Cookie string
  // should still go through the regular cookie parsing process before entering
  // the cookie jar.
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
  bool SetIsPartitioned(bool is_partitioned);

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

  // Returns |true| if the parsed version of |value| matches |value|.
  static bool ValueMatchesParsedValue(const std::string& value);

  // Is the string valid as the name of the cookie or as an attribute name?
  static bool IsValidCookieName(const std::string& name);

  // Is the string valid as the value of the cookie?
  static bool IsValidCookieValue(const std::string& value);

  // Is the string free of any characters not allowed in attribute values?
  static bool CookieAttributeValueHasValidCharSet(const std::string& value);

  // Is the string less than the size limits set for attribute values?
  static bool CookieAttributeValueHasValidSize(const std::string& value);

  // Returns `true` if the name and value combination are valid. Calls
  // IsValidCookieName() and IsValidCookieValue() on `name` and `value`
  // respectively, in addition to checking that the sum of the two doesn't
  // exceed size limits specified in RFC6265bis.
  static bool IsValidCookieNameValuePair(
      const std::string& name,
      const std::string& value,
      CookieInclusionStatus* status_out = nullptr);

 private:
  void ParseTokenValuePairs(const std::string& cookie_line,
                            CookieInclusionStatus& status_out);
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
  // 0th index is the user supplied cookie name/value, not an attribute.
  size_t path_index_ = 0;
  size_t domain_index_ = 0;
  size_t expires_index_ = 0;
  size_t maxage_index_ = 0;
  size_t secure_index_ = 0;
  size_t httponly_index_ = 0;
  size_t same_site_index_ = 0;
  size_t priority_index_ = 0;
  size_t partitioned_index_ = 0;
  // For metrics on cookie name/value internal HTABS
  bool internal_htab_ = false;
};

}  // namespace net

#endif  // NET_COOKIES_PARSED_COOKIE_H_
