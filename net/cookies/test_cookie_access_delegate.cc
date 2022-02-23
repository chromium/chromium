// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/test_cookie_access_delegate.h"

#include <set>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/ranges/algorithm.h"
#include "base/stl_util.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
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

absl::optional<FirstPartySetMetadata>
TestCookieAccessDelegate::ComputeFirstPartySetMetadataMaybeAsync(
    const SchemefulSite& site,
    const SchemefulSite* top_frame_site,
    const std::set<SchemefulSite>& party_context,
    base::OnceCallback<void(FirstPartySetMetadata)> callback) const {
  absl::optional<SchemefulSite> top_frame_owner =
      top_frame_site ? FindFirstPartySetOwnerSync(*top_frame_site)
                     : absl::nullopt;
  return RunMaybeAsync(
      FirstPartySetMetadata(
          SamePartyContext(),
          base::OptionalOrNullptr(FindFirstPartySetOwnerSync(site)),
          base::OptionalOrNullptr(top_frame_owner),
          FirstPartySetsContextType::kUnknown),
      std::move(callback));
}

absl::optional<SchemefulSite>
TestCookieAccessDelegate::FindFirstPartySetOwnerSync(
    const SchemefulSite& site) const {
  auto owner_set_iter =
      base::ranges::find_if(first_party_sets_, [&](const auto& set_iter) {
        return base::Contains(set_iter.second, site);
      });

  return owner_set_iter != first_party_sets_.end()
             ? absl::make_optional(owner_set_iter->first)
             : absl::nullopt;
}

absl::optional<absl::optional<SchemefulSite>>
TestCookieAccessDelegate::FindFirstPartySetOwner(
    const SchemefulSite& site,
    base::OnceCallback<void(absl::optional<SchemefulSite>)> callback) const {
  return RunMaybeAsync(FindFirstPartySetOwnerSync(site), std::move(callback));
}

absl::optional<base::flat_map<SchemefulSite, SchemefulSite>>
TestCookieAccessDelegate::FindFirstPartySetOwners(
    const base::flat_set<SchemefulSite>& sites,
    base::OnceCallback<void(base::flat_map<SchemefulSite, SchemefulSite>)>
        callback) const {
  std::vector<std::pair<SchemefulSite, SchemefulSite>> mapping;
  for (const SchemefulSite& site : sites) {
    absl::optional<SchemefulSite> owner = FindFirstPartySetOwnerSync(site);
    if (owner)
      mapping.emplace_back(site, *owner);
  }

  return RunMaybeAsync<base::flat_map<SchemefulSite, SchemefulSite>>(
      mapping, std::move(callback));
}

absl::optional<base::flat_map<SchemefulSite, std::set<SchemefulSite>>>
TestCookieAccessDelegate::RetrieveFirstPartySets(
    base::OnceCallback<
        void(base::flat_map<SchemefulSite, std::set<SchemefulSite>>)> callback)
    const {
  return RunMaybeAsync(first_party_sets_, std::move(callback));
}

template <class T>
absl::optional<T> TestCookieAccessDelegate::RunMaybeAsync(
    T result,
    base::OnceCallback<void(T)> callback) const {
  if (invoke_callbacks_asynchronously_) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
    return absl::nullopt;
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
    const base::flat_map<SchemefulSite, std::set<SchemefulSite>>& sets) {
  first_party_sets_ = sets;
}

}  // namespace net
