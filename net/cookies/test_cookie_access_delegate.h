// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_TEST_COOKIE_ACCESS_DELEGATE_H_
#define NET_COOKIES_TEST_COOKIE_ACCESS_DELEGATE_H_

#include <map>
#include <optional>
#include <set>
#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/cookie_access_delegate.h"
#include "net/cookies/cookie_constants.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "net/first_party_sets/first_party_sets_cache_filter.h"

namespace net {

// CookieAccessDelegate for testing. You can set the return value for a given
// cookie_domain (modulo any leading dot). Calling GetAccessSemantics() will
// then return the given value, or UNKNOWN if you haven't set one.
class TestCookieAccessDelegate : public CookieAccessDelegate {
 public:
  TestCookieAccessDelegate();

  TestCookieAccessDelegate(const TestCookieAccessDelegate&) = delete;
  TestCookieAccessDelegate& operator=(const TestCookieAccessDelegate&) = delete;

  ~TestCookieAccessDelegate() override;

  // CookieAccessDelegate implementation:
  CookieAccessSemantics GetAccessSemantics(
      const CanonicalCookie& cookie) const override;
  bool ShouldIgnoreSameSiteRestrictions(
      const GURL& url,
      const SiteForCookies& site_for_cookies) const override;
  bool ShouldTreatUrlAsTrustworthy(const GURL& url) const override;
  std::optional<
      std::pair<FirstPartySetMetadata, FirstPartySetsCacheFilter::MatchInfo>>
  ComputeFirstPartySetMetadataMaybeAsync(
      const SchemefulSite& site,
      const SchemefulSite* top_frame_site,
      base::OnceCallback<void(FirstPartySetMetadata,
                              FirstPartySetsCacheFilter::MatchInfo)> callback)
      const override;
  std::optional<base::flat_map<SchemefulSite, FirstPartySetEntry>>
  FindFirstPartySetEntries(
      const base::flat_set<SchemefulSite>& sites,
      base::OnceCallback<
          void(base::flat_map<SchemefulSite, FirstPartySetEntry>)> callback)
      const override;

  // Sets the expected return value for any cookie whose Domain
  // matches |cookie_domain|. Pass the value of |cookie.Domain()| and any
  // leading dot will be discarded.
  void SetExpectationForCookieDomain(const std::string& cookie_domain,
                                     CookieAccessSemantics access_semantics);

  // Sets the expected return value for ShouldAlwaysAttachSameSiteCookies.
  // Can set schemes that always attach SameSite cookies, or schemes that always
  // attach SameSite cookies if the request URL is secure.
  void SetIgnoreSameSiteRestrictionsScheme(
      const std::string& site_for_cookies_scheme,
      bool require_secure_origin);

  // Set the test delegate's First-Party Sets. The map's keys are the sites in
  // the sets. Primary sites must be included among the keys for a given set.
  void SetFirstPartySets(
      const base::flat_map<SchemefulSite, FirstPartySetEntry>& sets);

  void set_invoke_callbacks_asynchronously(bool async) {
    invoke_callbacks_asynchronously_ = async;
  }

  void set_first_party_sets_cache_filter(FirstPartySetsCacheFilter filter) {
    first_party_sets_cache_filter_ = std::move(filter);
  }

 private:
  // Finds a FirstPartySetEntry for the given site, if one exists.
  std::optional<FirstPartySetEntry> FindFirstPartySetEntry(
      const SchemefulSite& site) const;

  // Discard any leading dot in the domain string.
  std::string GetKeyForDomainValue(const std::string& domain) const;

  // Invokes the given `callback` asynchronously or returns the result
  // synchronously, depending on the configuration of this instance.
  template <class T>
  std::optional<T> RunMaybeAsync(T result,
                                 base::OnceCallback<void(T)> callback) const;

  std::map<std::string, CookieAccessSemantics> expectations_;
  std::map<std::string, bool> ignore_samesite_restrictions_schemes_;
  base::flat_map<SchemefulSite, FirstPartySetEntry> first_party_sets_;
  FirstPartySetsCacheFilter first_party_sets_cache_filter_;
  bool invoke_callbacks_asynchronously_ = false;
  SchemefulSite trustworthy_site_ =
      SchemefulSite(GURL("http://trustworthysitefortestdelegate.example"));
};

}  // namespace net

#endif  // NET_COOKIES_TEST_COOKIE_ACCESS_DELEGATE_H_
