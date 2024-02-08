// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/test_cookie_access_delegate.h"

#include <set>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/ranges/algorithm.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/types/optional_util.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_util.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "net/first_party_sets/first_party_sets_cache_filter.h"

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

// Returns true if `url` has the same scheme://eTLD+1 as `trustworthy_site_`.
bool TestCookieAccessDelegate::ShouldTreatUrlAsTrustworthy(
    const GURL& url) const {
  if (SchemefulSite(url) == trustworthy_site_) {
    return true;
  }

  return false;
}

std::optional<
    std::pair<FirstPartySetMetadata, FirstPartySetsCacheFilter::MatchInfo>>
TestCookieAccessDelegate::ComputeFirstPartySetMetadataMaybeAsync(
    const SchemefulSite& site,
    const SchemefulSite* top_frame_site,
    base::OnceCallback<void(FirstPartySetMetadata,
                            FirstPartySetsCacheFilter::MatchInfo)> callback)
    const {
  std::optional<FirstPartySetEntry> top_frame_owner =
      top_frame_site ? FindFirstPartySetEntry(*top_frame_site) : std::nullopt;
  FirstPartySetMetadata metadata(
      base::OptionalToPtr(FindFirstPartySetEntry(site)),
      base::OptionalToPtr(top_frame_owner));
  FirstPartySetsCacheFilter::MatchInfo match_info(
      first_party_sets_cache_filter_.GetMatchInfo(site));

  if (invoke_callbacks_asynchronously_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::move(metadata), match_info));
    return std::nullopt;
  }
  return std::pair(std::move(metadata), match_info);
}

std::optional<FirstPartySetEntry>
TestCookieAccessDelegate::FindFirstPartySetEntry(
    const SchemefulSite& site) const {
  auto entry = first_party_sets_.find(site);

  return entry != first_party_sets_.end() ? std::make_optional(entry->second)
                                          : std::nullopt;
}

std::optional<base::flat_map<SchemefulSite, FirstPartySetEntry>>
TestCookieAccessDelegate::FindFirstPartySetEntries(
    const base::flat_set<SchemefulSite>& sites,
    base::OnceCallback<void(base::flat_map<SchemefulSite, FirstPartySetEntry>)>
        callback) const {
  std::vector<std::pair<SchemefulSite, FirstPartySetEntry>> mapping;
  for (const SchemefulSite& site : sites) {
    std::optional<FirstPartySetEntry> entry = FindFirstPartySetEntry(site);
    if (entry)
      mapping.emplace_back(site, *entry);
  }

  return RunMaybeAsync<base::flat_map<SchemefulSite, FirstPartySetEntry>>(
      mapping, std::move(callback));
}

template <class T>
std::optional<T> TestCookieAccessDelegate::RunMaybeAsync(
    T result,
    base::OnceCallback<void(T)> callback) const {
  if (invoke_callbacks_asynchronously_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
    return std::nullopt;
  }
  return result;
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
    const base::flat_map<SchemefulSite, FirstPartySetEntry>& sets) {
  first_party_sets_ = sets;
}

}  // namespace net
