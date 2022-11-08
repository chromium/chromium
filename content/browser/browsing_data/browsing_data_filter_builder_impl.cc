// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_data/browsing_data_filter_builder_impl.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/contains.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

using net::registry_controlled_domains::GetDomainAndRegistry;
using net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES;

namespace content {

namespace {

// Whether this is a subdomain of a registrable domain.
bool IsSubdomainOfARegistrableDomain(const std::string& domain) {
  std::string registrable_domain =
      GetDomainAndRegistry(domain, INCLUDE_PRIVATE_REGISTRIES);
  return registrable_domain != domain && registrable_domain != "";
}

// Note that for every domain, exactly one of the following holds:
// 1. GetDomainAndRegistry(domain, _) == ""        - e.g. localhost, 127.0.0.1
// 2. GetDomainAndRegistry(domain, _) == domain    - e.g. google.com
// 3. IsSubdomainOfARegistrableDomain(domain)      - e.g. www.google.com
// Types 1 and 2 are supported by RegistrableDomainFilterBuilder. Type 3 is not.

// True if the domain of `url` is in the deletelist, or isn't in the
// preservelist. The deletelist or preservelist is represented as `origins`,
// `registerable_domains`, and `mode`.
bool MatchesStorageKey(const std::set<url::Origin>& origins,
                       const std::set<std::string>& registerable_domains,
                       BrowsingDataFilterBuilder::Mode mode,
                       const blink::StorageKey& storage_key) {
  bool is_delete_list = mode == BrowsingDataFilterBuilder::Mode::kDelete;
  for (const auto& origin : origins) {
    if (storage_key.MatchesOriginForTrustedStorageDeletion(origin)) {
      return is_delete_list;
    }
  }

  bool found_domain = false;
  if (!registerable_domains.empty()) {
    std::string registerable_domain =
        GetDomainAndRegistry(storage_key.origin(), INCLUDE_PRIVATE_REGISTRIES);
    found_domain =
        base::Contains(registerable_domains, registerable_domain == ""
                                                 ? storage_key.origin().host()
                                                 : registerable_domain);
  }
  return found_domain == is_delete_list;
}

bool MatchesURL(const std::set<url::Origin>& origins,
                const std::set<std::string>& registerable_domains,
                BrowsingDataFilterBuilder::Mode mode,
                bool is_cross_site_clear_site_data,
                const GURL& url) {
  DCHECK(!is_cross_site_clear_site_data);
  return MatchesStorageKey(origins, registerable_domains, mode,
                           blink::StorageKey(url::Origin::Create(url)));
}

// True if none of the supplied domains matches this plugin's |site| and we're a
// preservelist, or one of them does and we're a deletelist. The deletelist or
// preservelist is represented by |domains_and_ips| and |mode|.
bool MatchesPluginSiteForRegisterableDomainsAndIPs(
    const std::set<std::string>& domains_and_ips,
    BrowsingDataFilterBuilder::Mode mode,
    const std::string& site) {
  // If |site| is a third- or lower-level domain, find the corresponding eTLD+1.
  std::string domain_or_ip =
      GetDomainAndRegistry(site, INCLUDE_PRIVATE_REGISTRIES);
  if (domain_or_ip.empty())
    domain_or_ip = site;

  return ((mode == BrowsingDataFilterBuilder::Mode::kDelete) ==
          (domains_and_ips.find(domain_or_ip) != domains_and_ips.end()));
}

template <typename T>
base::RepeatingCallback<bool(const T&)> NotReachedFilter() {
  return base::BindRepeating([](const T&) {
    NOTREACHED();
    return false;
  });
}

}  // namespace

// static
std::unique_ptr<BrowsingDataFilterBuilder>
BrowsingDataFilterBuilder::Create(Mode mode) {
  return std::make_unique<BrowsingDataFilterBuilderImpl>(mode);
}

// static
base::RepeatingCallback<bool(const GURL&)>
BrowsingDataFilterBuilder::BuildNoopFilter() {
  return base::BindRepeating([](const GURL&) { return true; });
}

BrowsingDataFilterBuilderImpl::BrowsingDataFilterBuilderImpl(Mode mode)
    : mode_(mode) {}

BrowsingDataFilterBuilderImpl::~BrowsingDataFilterBuilderImpl() {}

void BrowsingDataFilterBuilderImpl::AddOrigin(const url::Origin& origin) {
  // By limiting the filter to non-unique origins, we can guarantee that
  // origin1 < origin2 && origin1 > origin2 <=> origin1.isSameOrigin(origin2).
  // This means that std::set::find() will use the same semantics for
  // origin comparison as Origin::IsSameOriginWith(). Furthermore, this
  // means that two filters are equal iff they are equal element-wise.
  DCHECK(!origin.opaque()) << "Invalid origin passed into OriginFilter.";

  // TODO(msramek): All urls with file scheme currently map to the same
  // origin. This is currently not a problem, but if it becomes one,
  // consider recognizing the URL path.

  origins_.insert(origin);
}

void BrowsingDataFilterBuilderImpl::AddRegisterableDomain(
    const std::string& domain) {
  // We check that the domain we're given is actually a eTLD+1, an IP address,
  // or an internal hostname.
  DCHECK(!IsSubdomainOfARegistrableDomain(domain));
  domains_.insert(domain);
}

void BrowsingDataFilterBuilderImpl::SetCookiePartitionKeyCollection(
    const net::CookiePartitionKeyCollection& cookie_partition_key_collection) {
  // This method should only be called when the current
  // `cookie_partition_key_collection_` is the default value.
  DCHECK(cookie_partition_key_collection_.ContainsAllKeys());
  cookie_partition_key_collection_ = cookie_partition_key_collection;
}

bool BrowsingDataFilterBuilderImpl::IsCrossSiteClearSiteDataForCookies() const {
  if (cookie_partition_key_collection_.IsEmpty() ||
      cookie_partition_key_collection_.ContainsAllKeys()) {
    return false;
  }
  // Assumes that there is only a single domain in the filter, since C-S-D
  // requests only have one. If this method needs to be used with more than one
  // domain, the code below needs to be modified.
  DCHECK_EQ(1U, domains_.size());
  for (const auto& domain : domains_) {
    auto secure_site =
        net::SchemefulSite(url::Origin::Create(GURL("https://" + domain)));
    auto insecure_site =
        net::SchemefulSite(url::Origin::Create(GURL("http://" + domain)));
    for (const auto& key : cookie_partition_key_collection_.PartitionKeys()) {
      if (key.site() == secure_site || key.site() == insecure_site)
        return false;
    }
  }
  return true;
}

void BrowsingDataFilterBuilderImpl::SetStorageKey(
    const absl::optional<blink::StorageKey>& storage_key) {
  storage_key_ = storage_key;
}

bool BrowsingDataFilterBuilderImpl::HasStorageKey() const {
  return storage_key_.has_value();
}

bool BrowsingDataFilterBuilderImpl::MatchesWithSavedStorageKey(
    const blink::StorageKey& other_key) const {
  DCHECK(storage_key_.has_value());
  return storage_key_.value() == other_key;
}

bool BrowsingDataFilterBuilderImpl::MatchesAllOriginsAndDomains() {
  return mode_ == Mode::kPreserve && origins_.empty() && domains_.empty();
}

base::RepeatingCallback<bool(const GURL&)>
BrowsingDataFilterBuilderImpl::BuildUrlFilter() {
  if (MatchesAllOriginsAndDomains())
    return base::BindRepeating([](const GURL&) { return true; });
  return base::BindRepeating(&MatchesURL, origins_, domains_, mode_,
                             IsCrossSiteClearSiteDataForCookies());
}

content::StoragePartition::StorageKeyMatcherFunction
BrowsingDataFilterBuilderImpl::BuildStorageKeyFilter() {
  if (!cookie_partition_key_collection_.ContainsAllKeys())
    return NotReachedFilter<blink::StorageKey>();
  if (MatchesAllOriginsAndDomains())
    return base::BindRepeating([](const blink::StorageKey&) { return true; });
  // If the filter has a StorageKey set, use it to match.
  if (HasStorageKey()) {
    return base::BindRepeating(
        &BrowsingDataFilterBuilderImpl::MatchesWithSavedStorageKey,
        base::Unretained(this));
  }
  return base::BindRepeating(&MatchesStorageKey, origins_, domains_, mode_);
}

network::mojom::ClearDataFilterPtr
BrowsingDataFilterBuilderImpl::BuildNetworkServiceFilter() {
  // TODO(msramek): Optimize BrowsingDataFilterBuilder for larger filters
  // if needed.
  DCHECK_LE(origins_.size(), 10U)
      << "BrowsingDataFilterBuilder is only suitable for creating "
         "small network service filters.";

  if (MatchesAllOriginsAndDomains())
    return nullptr;

  if (!cookie_partition_key_collection_.ContainsAllKeys())
    return nullptr;

  network::mojom::ClearDataFilterPtr filter =
      network::mojom::ClearDataFilter::New();
  filter->type = (mode_ == Mode::kDelete)
                     ? network::mojom::ClearDataFilter::Type::DELETE_MATCHES
                     : network::mojom::ClearDataFilter::Type::KEEP_MATCHES;
  filter->origins.insert(filter->origins.begin(), origins_.begin(),
                         origins_.end());
  filter->domains.insert(filter->domains.begin(), domains_.begin(),
                         domains_.end());
  return filter;
}

network::mojom::CookieDeletionFilterPtr
BrowsingDataFilterBuilderImpl::BuildCookieDeletionFilter() {
  DCHECK(origins_.empty())
      << "Origin-based deletion is not suitable for cookies. Please use "
         "different scoping, such as RegistrableDomainFilterBuilder.";
  auto deletion_filter = network::mojom::CookieDeletionFilter::New();

  deletion_filter->cookie_partition_key_collection =
      cookie_partition_key_collection_;

  switch (mode_) {
    case Mode::kDelete:
      deletion_filter->including_domains.emplace(domains_.begin(),
                                                 domains_.end());
      break;
    case Mode::kPreserve:
      deletion_filter->excluding_domains.emplace(domains_.begin(),
                                                 domains_.end());
      break;
  }
  return deletion_filter;
}

base::RepeatingCallback<bool(const std::string& site)>
BrowsingDataFilterBuilderImpl::BuildPluginFilter() {
  if (!cookie_partition_key_collection_.ContainsAllKeys())
    return NotReachedFilter<std::string>();
  DCHECK(origins_.empty()) <<
      "Origin-based deletion is not suitable for plugins. Please use "
      "different scoping, such as RegistrableDomainFilterBuilder.";
  return base::BindRepeating(&MatchesPluginSiteForRegisterableDomainsAndIPs,
                             domains_, mode_);
}

BrowsingDataFilterBuilderImpl::Mode BrowsingDataFilterBuilderImpl::GetMode() {
  return mode_;
}

std::unique_ptr<BrowsingDataFilterBuilder>
BrowsingDataFilterBuilderImpl::Copy() {
  std::unique_ptr<BrowsingDataFilterBuilderImpl> copy =
      std::make_unique<BrowsingDataFilterBuilderImpl>(mode_);
  copy->origins_ = origins_;
  copy->domains_ = domains_;
  return std::move(copy);
}

bool BrowsingDataFilterBuilderImpl::IsEqual(
    const BrowsingDataFilterBuilder& other) const {
  // This is the only implementation of BrowsingDataFilterBuilder, so we can
  // downcast |other|.
  const BrowsingDataFilterBuilderImpl* other_impl =
      static_cast<const BrowsingDataFilterBuilderImpl*>(&other);

  return origins_ == other_impl->origins_ && domains_ == other_impl->domains_ &&
         mode_ == other_impl->mode_;
}

}  // namespace content
