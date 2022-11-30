// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_TEST_COOKIE_ACCESS_DELEGATE_H_
#define NET_COOKIES_TEST_COOKIE_ACCESS_DELEGATE_H_

#include <map>
#include <set>
#include <string>

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/cookie_access_delegate.h"
#include "net/cookies/cookie_constants.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {

class SchemefulSite;

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
  absl::optional<FirstPartySetMetadata> ComputeFirstPartySetMetadataMaybeAsync(
      const SchemefulSite& site,
      const SchemefulSite* top_frame_site,
      const std::set<SchemefulSite>& party_context,
      base::OnceCallback<void(FirstPartySetMetadata)> callback) const override;
  absl::optional<base::flat_map<SchemefulSite, FirstPartySetEntry>>
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

 private:
  // Finds a FirstPartySetEntry for the given site, if one exists.
  absl::optional<FirstPartySetEntry> FindFirstPartySetEntry(
      const SchemefulSite& site) const;

  // Discard any leading dot in the domain string.
  std::string GetKeyForDomainValue(const std::string& domain) const;

  // Invokes the given `callback` asynchronously or returns the result
  // synchronously, depending on the configuration of this instance.
  template <class T>
  absl::optional<T> RunMaybeAsync(T result,
                                  base::OnceCallback<void(T)> callback) const;

  std::map<std::string, CookieAccessSemantics> expectations_;
  std::map<std::string, bool> ignore_samesite_restrictions_schemes_;
  base::flat_map<SchemefulSite, FirstPartySetEntry> first_party_sets_;
  bool invoke_callbacks_asynchronously_ = false;
};

}  // namespace net

#endif  // NET_COOKIES_TEST_COOKIE_ACCESS_DELEGATE_H_
