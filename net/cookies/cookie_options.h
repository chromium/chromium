// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Brought to you by number 42.

#ifndef NET_COOKIES_COOKIE_OPTIONS_H_
#define NET_COOKIES_COOKIE_OPTIONS_H_

#include <ostream>
#include <string>

#include "base/check_op.h"
#include "net/base/net_export.h"

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

    // Holds metadata about the factors that went into deciding the ContextType.
    //
    // These values may be used for recording histograms or
    // CookieInclusionStatus warnings, but SHOULD NOT be relied
    // upon for cookie inclusion decisions. Use only the ContextTypes for that.
    //
    // When adding a field, also update CompleteEquivalenceForTesting.
    struct NET_EXPORT ContextMetadata {
      // Possible "downgrades" for the SameSite context type, e.g. from a more
      // trusted context to a less trusted context, as a result of some behavior
      // change affecting the same-site calculation.
      enum class ContextDowngradeType {
        // Context not downgraded.
        kNoDowngrade,
        // Context was originally strictly same-site, was downgraded to laxly
        // same-site.
        kStrictToLax,
        // Context was originally strictly same-site, was downgraded to
        // cross-site.
        kStrictToCross,
        // Context was originally laxly same-site, was downgraded to cross-site.
        kLaxToCross,
      };

      // These values are persisted to logs. Entries should not be renumbered
      // and numeric values should never be reused.
      // This enum is to help collect metrics for https://crbug.com/1221316.
      // Specifically it's to help indicate how many cookies are accessed with a
      // given redirect type in order to provide a denominator for the
      // Cookie.CrossSiteRedirectDowngradeChangesInclusion2.* metrics.
      // Note: for this enum the notation A->B->C means that a navigation from
      // A to B was redirected to C. I.e.: A is the initator of the navigation
      // and C is the final url.
      enum class ContextRedirectTypeBug1221316 {
        kUnset =
            0,  // Indicates this value was unused and shouldn't be read. E.x.:
                // A javascript access means this value is meaningless.
        kNoRedirect = 1,  // There weren't any redirects. E.x.: A->B, A->A
        kCrossSiteRedirect =
            2,  // There was a redirect but it didn't start and
                // end at the same site or the redirect was to a different site
                // than the site-for-cookies. E.x.: A->B->C or B->B->B when the
                // site-for-cookies is A.
        kPartialSameSiteRedirect =
            3,  // There was a redirect and the start and
                // end are the same site. E.x.: A->B->A. Only this one could
                // potentially set cross_site_redirect_downgrade.
        kAllSameSiteRedirect = 4,  // There was a redirect and all urls are the
                                   // same site. E.x.:, A->A->A
        kMaxValue = kAllSameSiteRedirect
      };

      // These values are persisted to logs. Entries should not be renumbered
      // and numeric values should never be reused.
      enum class HttpMethod {
        // kUnset indicates this enum wasn't applicable in the context.
        kUnset = -1,
        // kUnknown indicates we were unable to convert the method string to
        // this enum.
        kUnknown = 0,
        kGet = 1,
        kHead = 2,
        kPost = 3,
        KPut = 4,
        kDelete = 5,
        kConnect = 6,
        kOptions = 7,
        kTrace = 8,
        kPatch = 9,
        kMaxValue = kPatch
      };

      // Records the type of any context downgrade due to a cross-site redirect,
      // i.e. whether the spec change in
      // https://github.com/httpwg/http-extensions/pull/1348 changed the result
      // of the context calculation. Note that a lax-to-cross downgrade can only
      // happen for response cookies, because a laxly same-site context only
      // happens for a top-level cross-site request, which cannot be downgraded
      // due to a cross-site redirect to a non-top-level cross-site request.
      // This only records whether the context was downgraded, not whether the
      // cookie's inclusion result was changed.
      ContextDowngradeType cross_site_redirect_downgrade =
          ContextDowngradeType::kNoDowngrade;

      ContextRedirectTypeBug1221316 redirect_type_bug_1221316 =
          ContextRedirectTypeBug1221316::kUnset;

      // Records the HTTP method of requests that result in a cross-site
      // redirect downgrade. May be kUnset if there wasn't a downgrade or if the
      // cookie access wasn't due to a request.
      //
      // Note that this field is always set when there was a context
      // downgrade but the associated histrogram is only recorded when that
      // context downgrade results in a change in inclusion status.
      HttpMethod http_method_bug_1221316 = HttpMethod::kUnset;
    };

    // The following three constructors apply default values for the metadata
    // members.
    SameSiteCookieContext()
        : SameSiteCookieContext(ContextType::CROSS_SITE,
                                ContextType::CROSS_SITE) {}

    explicit SameSiteCookieContext(ContextType same_site_context)
        : SameSiteCookieContext(same_site_context, same_site_context) {}

    SameSiteCookieContext(ContextType same_site_context,
                          ContextType schemeful_same_site_context)
        : SameSiteCookieContext(same_site_context,
                                schemeful_same_site_context,
                                ContextMetadata(),
                                ContextMetadata()) {}

    // Schemeful and schemeless context types are consistency-checked against
    // each other, but the metadata is stored as-is (i.e. the values in
    // `metadata` and `schemeful_metadata` may be logically inconsistent), as
    // the metadata is not relied upon for correctness.
    SameSiteCookieContext(ContextType same_site_context,
                          ContextType schemeful_same_site_context,
                          ContextMetadata metadata,
                          ContextMetadata schemeful_metadata)
        : context_(same_site_context),
          schemeful_context_(schemeful_same_site_context),
          metadata_(metadata),
          schemeful_metadata_(schemeful_metadata) {
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

    // Returns the metadata describing how this context was calculated, under
    // the currently applicable schemeful/schemeless mode.
    // TODO(chlily): Should take the CookieAccessSemantics as well, to
    // accurately account for the context actually used for a given cookie.
    const ContextMetadata& GetMetadataForCurrentSchemefulMode() const;

    // If you're just trying to determine if a cookie is accessible you likely
    // want to use GetContextForCookieInclusion() which will return the correct
    // context regardless the status of same-site features.
    ContextType context() const { return context_; }
    ContextType schemeful_context() const { return schemeful_context_; }

    // You probably want to use GetMetadataForCurrentSchemefulMode() instead of
    // these getters, since that takes into account the applicable schemeful
    // mode.
    const ContextMetadata& metadata() const { return metadata_; }
    ContextMetadata& metadata() { return metadata_; }
    const ContextMetadata& schemeful_metadata() const {
      return schemeful_metadata_;
    }
    ContextMetadata& schemeful_metadata() { return schemeful_metadata_; }

    // Sets context types. Does not check for consistency between context and
    // schemeful context. Does not touch the metadata.
    void SetContextTypesForTesting(ContextType context_type,
                                   ContextType schemeful_context_type);

    // Returns whether the context types and all fields of the metadata structs
    // are the same.
    bool CompleteEquivalenceForTesting(
        const SameSiteCookieContext& other) const;

    // Equality operators disregard any metadata! (Only the context types are
    // compared, not how they were computed.)
    NET_EXPORT friend bool operator==(
        const CookieOptions::SameSiteCookieContext& lhs,
        const CookieOptions::SameSiteCookieContext& rhs);
    NET_EXPORT friend bool operator!=(
        const CookieOptions::SameSiteCookieContext& lhs,
        const CookieOptions::SameSiteCookieContext& rhs);

   private:
    ContextType context_;
    ContextType schemeful_context_;

    ContextMetadata metadata_;
    ContextMetadata schemeful_metadata_;
  };

  // Creates a CookieOptions object which:
  //
  // * Excludes HttpOnly cookies
  // * Excludes SameSite cookies
  // * Updates last-accessed time.
  // * Does not report excluded cookies in APIs that can do so.
  //
  // These settings can be altered by calling:
  //
  // * |set_{include,exclude}_httponly()|
  // * |set_same_site_cookie_context()|
  // * |set_do_not_update_access_time()|
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
  void set_same_site_cookie_context(const SameSiteCookieContext& context) {
    same_site_cookie_context_ = context;
  }

  const SameSiteCookieContext& same_site_cookie_context() const {
    return same_site_cookie_context_;
  }

  void set_update_access_time() { update_access_time_ = true; }
  void set_do_not_update_access_time() { update_access_time_ = false; }
  bool update_access_time() const { return update_access_time_; }

  void set_return_excluded_cookies() { return_excluded_cookies_ = true; }
  void unset_return_excluded_cookies() { return_excluded_cookies_ = false; }
  bool return_excluded_cookies() const { return return_excluded_cookies_; }

  // Convenience method for where you need a CookieOptions that will
  // work for getting/setting all types of cookies, including HttpOnly and
  // SameSite cookies. Also specifies not to update the access time, because
  // usually this is done to get all the cookies to check that they are correct,
  // including the creation time. This basically makes a CookieOptions that is
  // the opposite of the default CookieOptions.
  static CookieOptions MakeAllInclusive();

 private:
  // Keep default values in sync with
  // content/public/common/cookie_manager.mojom.
  bool exclude_httponly_ = true;
  SameSiteCookieContext same_site_cookie_context_;
  bool update_access_time_ = true;
  bool return_excluded_cookies_ = false;
};

NET_EXPORT bool operator==(
    const CookieOptions::SameSiteCookieContext::ContextMetadata& lhs,
    const CookieOptions::SameSiteCookieContext::ContextMetadata& rhs);
NET_EXPORT bool operator!=(
    const CookieOptions::SameSiteCookieContext::ContextMetadata& lhs,
    const CookieOptions::SameSiteCookieContext::ContextMetadata& rhs);

// Allows gtest to print more helpful error messages instead of printing hex.
// (No need to null-check `os` because we can assume gtest will properly pass a
// non-null pointer, and it is dereferenced immediately anyway.)
inline void PrintTo(CookieOptions::SameSiteCookieContext::ContextType ct,
                    std::ostream* os) {
  *os << static_cast<int>(ct);
}

inline void PrintTo(
    const CookieOptions::SameSiteCookieContext::ContextMetadata& m,
    std::ostream* os) {
  *os << "{";
  *os << " cross_site_redirect_downgrade: "
      << static_cast<int>(m.cross_site_redirect_downgrade);
  *os << ", redirect_type_bug_1221316: "
      << static_cast<int>(m.redirect_type_bug_1221316);
  *os << ", http_method_bug_1221316: "
      << static_cast<int>(m.http_method_bug_1221316);
  *os << " }";
}

inline void PrintTo(const CookieOptions::SameSiteCookieContext& sscc,
                    std::ostream* os) {
  *os << "{ context: ";
  PrintTo(sscc.context(), os);
  *os << ", schemeful_context: ";
  PrintTo(sscc.schemeful_context(), os);
  *os << ", metadata: ";
  PrintTo(sscc.metadata(), os);
  *os << ", schemeful_metadata: ";
  PrintTo(sscc.schemeful_metadata(), os);
  *os << " }";
}

}  // namespace net

#endif  // NET_COOKIES_COOKIE_OPTIONS_H_
