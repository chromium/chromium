// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/test_cookie_access_delegate.h"

#include <set>

#include "base/callback.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/task/thread_pool.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/first_party_set_metadata.h"
#include "net/cookies/same_party_context.h"

namespace net {

TestCookieAccessDelegate::TestCookieAccessDelegate() = default;

TestCookieAccessDelegate::~TestCookieAccessDelegate() = default;

CookieAccessSemantics TestCookieAccessDelegate::GetAccessSemantics(
    const CanonicalCookie& cookie) const {
  auto it = expectations_.find(GetKeyForDomainValue(cookie.Domain()));
  if (it != expectations_.end())
    return it->second;
  return CookieAccessSemantics::UNKNOWN;
}

bool TestCookieAccessDelegate::ShouldIgnoreSameSiteRestrictions(
    const GURL& url,
    const SiteForCookies& site_for_cookies) const {
  auto it =
      ignore_samesite_restrictions_schemes_.find(site_for_cookies.scheme());
  if (it == ignore_samesite_restrictions_schemes_.end())
    return false;
  if (it->second)
    return url.SchemeIsCryptographic();
  return true;
}

void TestCookieAccessDelegate::ComputeFirstPartySetMetadataMaybeAsync(
    const net::SchemefulSite& site,
    const net::SchemefulSite* top_frame_site,
    const std::set<net::SchemefulSite>& party_context,
    base::OnceCallback<void(FirstPartySetMetadata)> callback) const {
  if (invoke_callbacks_asynchronously_) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, base::BindOnce([]() { return FirstPartySetMetadata(); }),
        std::move(callback));
    return;
  }
  std::move(callback).Run(FirstPartySetMetadata());
}

absl::optional<net::SchemefulSite>
TestCookieAccessDelegate::FindFirstPartySetOwner(
    const net::SchemefulSite& site) const {
  for (const auto& all_sets_iter : first_party_sets_) {
    if (base::Contains(all_sets_iter.second, site))
      return all_sets_iter.first;
  }
  return absl::nullopt;
}

void TestCookieAccessDelegate::RetrieveFirstPartySets(
    base::OnceCallback<
        void(base::flat_map<net::SchemefulSite, std::set<net::SchemefulSite>>)>
        callback) const {
  if (invoke_callbacks_asynchronously_) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(
            [](const base::flat_map<net::SchemefulSite,
                                    std::set<net::SchemefulSite>>& sets) {
              return sets;
            },
            first_party_sets_),
        std::move(callback));
    return;
  }
  std::move(callback).Run(first_party_sets_);
}

void TestCookieAccessDelegate::SetExpectationForCookieDomain(
    const std::string& cookie_domain,
    CookieAccessSemantics access_semantics) {
  expectations_[GetKeyForDomainValue(cookie_domain)] = access_semantics;
}

void TestCookieAccessDelegate::SetIgnoreSameSiteRestrictionsScheme(
    const std::string& site_for_cookies_scheme,
    bool require_secure_origin) {
  ignore_samesite_restrictions_schemes_[site_for_cookies_scheme] =
      require_secure_origin;
}

std::string TestCookieAccessDelegate::GetKeyForDomainValue(
    const std::string& domain) const {
  DCHECK(!domain.empty());
  return cookie_util::CookieDomainAsHost(domain);
}

void TestCookieAccessDelegate::SetFirstPartySets(
    const base::flat_map<net::SchemefulSite, std::set<net::SchemefulSite>>&
        sets) {
  first_party_sets_ = sets;
}

}  // namespace net
