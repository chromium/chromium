// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_data/same_site_data_remover_impl.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/same_site_data_remover.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_util.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

namespace content {

namespace {

const uint32_t kStoragePartitionRemovalMask =
    content::StoragePartition::REMOVE_DATA_MASK_ALL &
    ~content::StoragePartition::REMOVE_DATA_MASK_COOKIES;

void OnGetAllCookiesWithAccessSemantics(
    base::OnceClosure closure,
    network::mojom::CookieManager* cookie_manager,
    std::set<std::string>* same_site_none_domains,
    const std::vector<net::CanonicalCookie>& cookies,
    const std::vector<net::CookieAccessSemantics>& access_semantics_list) {
  DCHECK(cookies.size() == access_semantics_list.size());
  base::RepeatingClosure barrier =
      base::BarrierClosure(cookies.size(), std::move(closure));
  for (size_t i = 0; i < cookies.size(); ++i) {
    const net::CanonicalCookie& cookie = cookies[i];
    // Partitioned cookies are only available in a single top-level site (or
    // that site's First-Party Set). Since partitioned cookies cannot be used as
    // a cross-site tracking mechanism, we exclude them from this type of
    // clearing.
    if (!cookie.IsPartitioned() &&
        cookie.IsEffectivelySameSiteNone(access_semantics_list[i])) {
      same_site_none_domains->emplace(cookie.Domain());
      cookie_manager->DeleteCanonicalCookie(
          cookie, base::BindOnce([](const base::RepeatingClosure& callback,
                                    bool success) { callback.Run(); },
                                 barrier));
    } else {
      barrier.Run();
    }
  }
}

std::unique_ptr<BrowsingDataFilterBuilder> CreateBrowsingDataFilterBuilder(
    const std::set<std::string>& same_site_none_domains) {
  std::unique_ptr<BrowsingDataFilterBuilder> filter_builder =
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kDelete);
  for (const std::string& domain : same_site_none_domains) {
    std::string host_domain = net::cookie_util::CookieDomainAsHost(domain);
    std::string registrable_domain = GetDomainAndRegistry(
        host_domain,
        net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
    filter_builder->AddRegisterableDomain(
        registrable_domain.empty() ? host_domain : registrable_domain);
  }
  return filter_builder;
}

}  // namespace

SameSiteDataRemoverImpl::SameSiteDataRemoverImpl(
    BrowserContext* browser_context)
    : browser_context_(browser_context),
      storage_partition_(browser_context_->GetDefaultStoragePartition()) {
  DCHECK(browser_context_);
}

SameSiteDataRemoverImpl::~SameSiteDataRemoverImpl() {}

const std::set<std::string>&
SameSiteDataRemoverImpl::GetDeletedDomainsForTesting() {
  return same_site_none_domains_;
}

void SameSiteDataRemoverImpl::OverrideStoragePartitionForTesting(
    StoragePartition* storage_partition) {
  storage_partition_ = storage_partition;
}

void SameSiteDataRemoverImpl::DeleteSameSiteNoneCookies(
    base::OnceClosure closure) {
  same_site_none_domains_.clear();
  auto* cookie_manager =
      storage_partition_->GetCookieManagerForBrowserProcess();
  cookie_manager->GetAllCookiesWithAccessSemantics(
      base::BindOnce(&OnGetAllCookiesWithAccessSemantics, std::move(closure),
                     cookie_manager, &same_site_none_domains_));
}

void SameSiteDataRemoverImpl::ClearStoragePartitionData(
    base::OnceClosure closure) {
  // TODO(crbug.com/987177): Figure out how to handle protected storage.
  std::unique_ptr<BrowsingDataFilterBuilder> filter_builder =
      CreateBrowsingDataFilterBuilder(same_site_none_domains_);
  storage_partition_->ClearData(
      kStoragePartitionRemovalMask,
      StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL, filter_builder.get(),
      StoragePartition::StorageKeyPolicyMatcherFunction(), nullptr, false,
      base::Time(), base::Time::Max(), std::move(closure));
}

void SameSiteDataRemoverImpl::ClearStoragePartitionForOrigins(
    base::OnceClosure closure,
    std::set<url::Origin> origins) {
  // TODO(crbug.com/987177): Figure out how to handle protected storage.
  std::unique_ptr<BrowsingDataFilterBuilder> filter_builder =
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kDelete);
  for (const url::Origin& origin : origins) {
    filter_builder->AddOrigin(origin);
  }
  storage_partition_->ClearData(
      kStoragePartitionRemovalMask,
      StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL, filter_builder.get(),
      StoragePartition::StorageKeyPolicyMatcherFunction(), nullptr, false,
      base::Time(), base::Time::Max(), std::move(closure));
}

// Defines the ClearSameSiteNoneData function declared in same_site_remover.h.
// Clears cookies and associated data available in third-party contexts.
void ClearSameSiteNoneData(base::OnceClosure closure, BrowserContext* context) {
  auto same_site_remover = std::make_unique<SameSiteDataRemoverImpl>(context);
  SameSiteDataRemoverImpl* remover = same_site_remover.get();

  remover->DeleteSameSiteNoneCookies(
      base::BindOnce(&SameSiteDataRemoverImpl::ClearStoragePartitionData,
                     std::move(same_site_remover), std::move(closure)));
}

void ClearSameSiteNoneCookiesAndStorageForOrigins(
    base::OnceClosure closure,
    BrowserContext* context,
    std::set<url::Origin> origins) {
  auto same_site_remover = std::make_unique<SameSiteDataRemoverImpl>(context);
  SameSiteDataRemoverImpl* remover = same_site_remover.get();

  remover->DeleteSameSiteNoneCookies(base::BindOnce(
      &SameSiteDataRemoverImpl::ClearStoragePartitionForOrigins,
      std::move(same_site_remover), std::move(closure), std::move(origins)));
}

}  // namespace content
