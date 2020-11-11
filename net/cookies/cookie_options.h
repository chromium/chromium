// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Brought to you by number 42.

#ifndef NET_COOKIES_COOKIE_OPTIONS_H_
#define NET_COOKIES_COOKIE_OPTIONS_H_

#include <set>

#include "base/optional.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/cookie_constants.h"
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

    ContextType schemeful_context() const { return schemeful_context_; }
    void set_schemeful_context(ContextType schemeful_context) {
      schemeful_context_ = schemeful_context;
    }

    NET_EXPORT friend bool operator==(
        const CookieOptions::SameSiteCookieContext& lhs,
        const CookieOptions::SameSiteCookieContext& rhs);
    NET_EXPORT friend bool operator!=(
        const CookieOptions::SameSiteCookieContext& lhs,
        const CookieOptions::SameSiteCookieContext& rhs);

   private:

    ContextType context_;

    ContextType schemeful_context_;
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
  // * |set_full_party_context()|
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

  void set_full_party_context(
      const base::Optional<std::set<net::SchemefulSite>>& full_party_context) {
    full_party_context_ = full_party_context;
  }
  const base::Optional<std::set<net::SchemefulSite>>& full_party_context()
      const {
    return full_party_context_;
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
  base::Optional<std::set<net::SchemefulSite>> full_party_context_;
};

}  // namespace net

#endif  // NET_COOKIES_COOKIE_OPTIONS_H_
