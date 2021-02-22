// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Brought to you by number 42.

#ifndef NET_COOKIES_COOKIE_OPTIONS_H_
#define NET_COOKIES_COOKIE_OPTIONS_H_

#include <ostream>
#include <set>

#include "base/optional.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "url/gurl.h"

namespace net {

class NET_EXPORT CookieOptions {
 public:

  // Relation between the cookie and the navigational environment.
  class NET_EXPORT SameSiteCookieContext {
   public:
    // CROSS_SITE to SAME_SITE_STRICT are ordered from least to most trusted
    // environment. Don't renumber, used in histograms.
    enum class ContextType {
      CROSS_SITE = 0,
      // Same rules as lax but the http method is unsafe.
      SAME_SITE_LAX_METHOD_UNSAFE = 1,
      SAME_SITE_LAX = 2,
      SAME_SITE_STRICT = 3,

      // Keep last, used for histograms.
      COUNT
    };

    SameSiteCookieContext()
        : SameSiteCookieContext(ContextType::CROSS_SITE,
                                ContextType::CROSS_SITE) {}
    explicit SameSiteCookieContext(ContextType same_site_context)
        : SameSiteCookieContext(same_site_context, same_site_context) {}

    SameSiteCookieContext(ContextType same_site_context,
                          ContextType schemeful_same_site_context)
        : context_(same_site_context),
          schemeful_context_(schemeful_same_site_context) {
      DCHECK_LE(schemeful_context_, context_);
    }

    // Convenience method which returns a SameSiteCookieContext with the most
    // inclusive contexts. This allows access to all SameSite cookies.
    static SameSiteCookieContext MakeInclusive();

    // Convenience method which returns a SameSiteCookieContext with the most
    // inclusive contexts for set. This allows setting all SameSite cookies.
    static SameSiteCookieContext MakeInclusiveForSet();

    // Returns the context for determining SameSite cookie inclusion.
    ContextType GetContextForCookieInclusion() const;

    // If you're just trying to determine if a cookie is accessible you likely
    // want to use GetContextForCookieInclusion() which will return the correct
    // context regardless the status of same-site features.
    ContextType context() const { return context_; }
    void set_context(ContextType context) { context_ = context; }
    void set_context(std::pair<ContextType, bool> context) {
      context_ = context.first;
      affected_by_bugfix_1166211_ = context.second;
    }

    ContextType schemeful_context() const { return schemeful_context_; }
    void set_schemeful_context(ContextType schemeful_context) {
      schemeful_context_ = schemeful_context;
    }
    void set_schemeful_context(std::pair<ContextType, bool> schemeful_context) {
      schemeful_context_ = schemeful_context.first;
      schemeful_affected_by_bugfix_1166211_ = schemeful_context.second;
    }

    // Whether the request was affected by the bugfix, either schemefully or
    // schemelessly.
    // TODO(crbug.com/1166211): Remove once no longer needed.
    bool AffectedByBugfix1166211() const;

    // If the cookie was excluded solely due to the bugfix, this applies a
    // warning to the status that will show up in the netlog. Also logs a
    // histogram showing whether the warning was applied.
    // TODO(crbug.com/1166211): Remove once no longer needed.
    void MaybeApplyBugfix1166211WarningToStatusAndLogHistogram(
        CookieInclusionStatus& status) const;

    NET_EXPORT friend bool operator==(
        const CookieOptions::SameSiteCookieContext& lhs,
        const CookieOptions::SameSiteCookieContext& rhs);
    NET_EXPORT friend bool operator!=(
        const CookieOptions::SameSiteCookieContext& lhs,
        const CookieOptions::SameSiteCookieContext& rhs);

   private:
    ContextType context_;
    ContextType schemeful_context_;

    // Record whether the ContextType calculation was affected by the bugfix for
    // crbug.com/1166211. These are for the purpose of recording histograms and
    // adding warnings to CookieInclusionStatus.
    // Note: These are not preserved when serializing/deserializing for mojo, as
    // these are only used in URLRequestHttpJob, which does not make mojo calls
    // with this struct (it is only relevant for HTTP requests).
    // TODO(crbug.com/1166211): Remove once no longer needed.
    bool affected_by_bugfix_1166211_ = false;
    bool schemeful_affected_by_bugfix_1166211_ = false;
  };

  // Computed in URLRequestHttpJob for every cookie access attempt but is only
  // relevant for SameParty cookies.
  enum class SamePartyCookieContextType {
    // The opposite to kSameParty. Should be the default value.
    kCrossParty = 0,
    // If the request URL is in the same First-Party Sets as the top-frame site
    // and each member of the isolation_info.party_context.
    kSameParty = 1,
  };

  // Creates a CookieOptions object which:
  //
  // * Excludes HttpOnly cookies
  // * Excludes SameSite cookies
  // * Updates last-accessed time.
  // * Does not report excluded cookies in APIs that can do so.
  // * Excludes SameParty cookies.
  //
  // These settings can be altered by calling:
  //
  // * |set_{include,exclude}_httponly()|
  // * |set_same_site_cookie_context()|
  // * |set_do_not_update_access_time()|
  // * |set_same_party_cookie_context_type()|
  CookieOptions();
  CookieOptions(const CookieOptions& other);
  CookieOptions(CookieOptions&& other);
  ~CookieOptions();

  CookieOptions& operator=(const CookieOptions&);
  CookieOptions& operator=(CookieOptions&&);

  void set_exclude_httponly() { exclude_httponly_ = true; }
  void set_include_httponly() { exclude_httponly_ = false; }
  bool exclude_httponly() const { return exclude_httponly_; }

  // How trusted is the current browser environment when it comes to accessing
  // SameSite cookies. Default is not trusted, e.g. CROSS_SITE.
  void set_same_site_cookie_context(SameSiteCookieContext context) {
    same_site_cookie_context_ = context;
  }

  SameSiteCookieContext same_site_cookie_context() const {
    return same_site_cookie_context_;
  }

  void set_update_access_time() { update_access_time_ = true; }
  void set_do_not_update_access_time() { update_access_time_ = false; }
  bool update_access_time() const { return update_access_time_; }

  void set_return_excluded_cookies() { return_excluded_cookies_ = true; }
  void unset_return_excluded_cookies() { return_excluded_cookies_ = false; }
  bool return_excluded_cookies() const { return return_excluded_cookies_; }

  // How trusted is the current browser environment when it comes to accessing
  // SameParty cookies. Default is not trusted, e.g. kCrossParty.
  void set_same_party_cookie_context_type(
      SamePartyCookieContextType context_type) {
    same_party_cookie_context_type_ = context_type;
  }
  SamePartyCookieContextType same_party_cookie_context_type() const {
    return same_party_cookie_context_type_;
  }

  // Getter/setter of |full_party_context_size_| for logging purposes.
  void set_full_party_context_size(uint32_t len) {
    full_party_context_size_ = len;
  }
  uint32_t full_party_context_size() const { return full_party_context_size_; }

  void set_is_in_nontrivial_first_party_set(bool is_member) {
    is_in_nontrivial_first_party_set_ = is_member;
  }
  bool is_in_nontrivial_first_party_set() const {
    return is_in_nontrivial_first_party_set_;
  }

  // Convenience method for where you need a CookieOptions that will
  // work for getting/setting all types of cookies, including HttpOnly and
  // SameSite cookies. Also specifies not to update the access time, because
  // usually this is done to get all the cookies to check that they are correct,
  // including the creation time. This basically makes a CookieOptions that is
  // the opposite of the default CookieOptions.
  static CookieOptions MakeAllInclusive();

 private:
  bool exclude_httponly_;
  SameSiteCookieContext same_site_cookie_context_;
  bool update_access_time_;
  bool return_excluded_cookies_ = false;

  SamePartyCookieContextType same_party_cookie_context_type_ =
      SamePartyCookieContextType::kCrossParty;
  // The size of the isolation_info.party_context plus the top-frame site.
  // Stored for logging purposes.
  uint32_t full_party_context_size_ = 0;
  // Whether the site requesting cookie access (as opposed to e.g. the
  // `site_for_cookies`) is a member (or owner) of a nontrivial First-Party
  // Set.
  // This is included here temporarily, for the purpose of ignoring SameParty
  // for sites that are not participating in the Origin Trial.
  // TODO(https://crbug.com/1163990): remove this field.
  bool is_in_nontrivial_first_party_set_ = false;
};

// Allows gtest to print more helpful error messages instead of printing hex.
// (No need to null-check `os` because we can assume gtest will properly pass a
// non-null pointer, and it is dereferenced immediately anyway.)
inline void PrintTo(CookieOptions::SameSiteCookieContext::ContextType ct,
                    std::ostream* os) {
  *os << static_cast<int>(ct);
}

inline void PrintTo(const CookieOptions::SameSiteCookieContext& sscc,
                    std::ostream* os) {
  *os << "{ context: ";
  PrintTo(sscc.context(), os);
  *os << ", schemeful_context: ";
  PrintTo(sscc.schemeful_context(), os);
  *os << " }";
}

}  // namespace net

#endif  // NET_COOKIES_COOKIE_OPTIONS_H_
