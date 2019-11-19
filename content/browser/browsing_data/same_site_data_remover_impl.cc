// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_data/same_site_data_remover_impl.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/same_site_data_remover.h"
#include "content/public/browser/storage_partition.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_util.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "url/origin.h"

namespace content {

namespace {

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
    if (cookie.IsEffectivelySameSiteNone(access_semantics_list[i])) {
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

bool DoesOriginMatchDomain(const std::set<std::string>& same_site_none_domains,
                           const url::Origin& origin,
                           storage::SpecialStoragePolicy* policy) {
  for (const std::string& domain : same_site_none_domains) {
    if (net::cookie_util::IsDomainMatch(domain, origin.host())) {
      return true;
    }
  }
  return false;
}

void OnDeleteSameSiteNoneCookies(
    base::OnceClosure closure,
    std::unique_ptr<SameSiteDataRemoverImpl> remover,
    bool clear_storage) {
  if (clear_storage) {
    remover->ClearStoragePartitionData(std::move(closure));
  } else {
    std::move(closure).Run();
  }
}

}  // namespace

SameSiteDataRemoverImpl::SameSiteDataRemoverImpl(
    BrowserContext* browser_context)
    : browser_context_(browser_context),
      storage_partition_(
          BrowserContext::GetDefaultStoragePartition(browser_context_)) {
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
  const uint32_t storage_partition_removal_mask =
      content::StoragePartition::REMOVE_DATA_MASK_ALL &
      ~content::StoragePartition::REMOVE_DATA_MASK_COOKIES;
  const uint32_t quota_storage_removal_mask =
      StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL;
  // TODO(crbug.com/987177): Figure out how to handle protected storage.

  storage_partition_->ClearData(
      storage_partition_removal_mask, quota_storage_removal_mask,
      base::BindRepeating(&DoesOriginMatchDomain, same_site_none_domains_),
      nullptr, false, base::Time(), base::Time::Max(), std::move(closure));
}

// Defines the ClearSameSiteNoneData function declared in same_site_remover.h.
// Clears cookies and associated data available in third-party contexts.
void ClearSameSiteNoneData(base::OnceClosure closure,
                           BrowserContext* context,
                           bool clear_storage) {
  auto same_site_remover = std::make_unique<SameSiteDataRemoverImpl>(context);
  SameSiteDataRemoverImpl* remover = same_site_remover.get();

  remover->DeleteSameSiteNoneCookies(
      base::BindOnce(&OnDeleteSameSiteNoneCookies, std::move(closure),
                     std::move(same_site_remover), clear_storage));
}

}  // namespace content
