// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_data/browsing_data_filter_builder_impl.h"

#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/network/public/mojom/clear_data_filter.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

using net::registry_controlled_domains::GetDomainAndRegistry;
using net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES;

using OriginMatchingMode =
    content::BrowsingDataFilterBuilder::OriginMatchingMode;

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
                       BrowsingDataFilterBuilder::OriginMatchingMode match_mode,
                       const blink::StorageKey& storage_key) {
  bool is_delete_list = mode == BrowsingDataFilterBuilder::Mode::kDelete;
  for (const auto& origin : origins) {
    if (match_mode == OriginMatchingMode::kThirdPartiesIncluded &&
        storage_key.MatchesOriginForTrustedStorageDeletion(origin)) {
      return is_delete_list;
    }
    if (match_mode == OriginMatchingMode::kOriginInAllContexts &&
        storage_key.origin() == origin) {
      return is_delete_list;
    }
  }

  switch (match_mode) {
    case OriginMatchingMode::kThirdPartiesIncluded: {
      return is_delete_list ==
             base::ranges::any_of(
                 registerable_domains, [&](const std::string& domain) {
                   return storage_key
                       .MatchesRegistrableDomainForTrustedStorageDeletion(
                           domain);
                 });
    }

    case OriginMatchingMode::kOriginInAllContexts: {
      std::string registerable_domain = GetDomainAndRegistry(
          storage_key.origin(), INCLUDE_PRIVATE_REGISTRIES);
      if (registerable_domain.empty()) {
        registerable_domain = storage_key.origin().host();
      }

      return is_delete_list ==
             base::Contains(registerable_domains, registerable_domain);
    }
  }

  return !is_delete_list;
}

bool MatchesURL(const std::set<url::Origin>& origins,
                const std::set<std::string>& registerable_domains,
                BrowsingDataFilterBuilder::Mode mode,
                BrowsingDataFilterBuilder::OriginMatchingMode origin_mode,
                bool partitioned_cookies_only,
                const GURL& url) {
  // TODO(crbug.com/40258758): Re-enable this check when it is actually
  // a valid precondition.
  // DCHECK(!partitioned_cookies_only);
  return MatchesStorageKey(
      origins, registerable_domains, mode, origin_mode,
      blink::StorageKey::CreateFirstParty(url::Origin::Create(url)));
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
          (base::Contains(domains_and_ips, domain_or_ip)));
}

template <typename T>
base::RepeatingCallback<bool(const T&)> NotReachedFilter() {
  return base::BindRepeating([](const T&) {
    NOTREACHED_IN_MIGRATION();
    return false;
  });
}

bool StorageKeyInCookiePartitionKeyCollection(
    const blink::StorageKey& storage_key,
    const net::CookiePartitionKeyCollection& cookie_partition_key_collection) {
  std::optional<net::CookiePartitionKey> equivalent_cookie_partition_key =
      storage_key.ToCookiePartitionKey();
  // If cookie partitioning is disabled, this will be nullopt and we can just
  // return true.
  if (!equivalent_cookie_partition_key) {
    return true;
  }
  return cookie_partition_key_collection.Contains(
      *equivalent_cookie_partition_key);
}

}  // namespace

// static
std::unique_ptr<BrowsingDataFilterBuilder>
BrowsingDataFilterBuilder::Create(Mode mode) {
  return std::make_unique<BrowsingDataFilterBuilderImpl>(mode);
}

// static
std::unique_ptr<BrowsingDataFilterBuilder> BrowsingDataFilterBuilder::Create(
    Mode mode,
    OriginMatchingMode origin_mode) {
  return std::make_unique<BrowsingDataFilterBuilderImpl>(mode, origin_mode);
}

// static
base::RepeatingCallback<bool(const GURL&)>
BrowsingDataFilterBuilder::BuildNoopFilter() {
  return base::BindRepeating([](const GURL&) { return true; });
}

BrowsingDataFilterBuilderImpl::BrowsingDataFilterBuilderImpl(Mode mode)
    : mode_(mode),
      origin_mode_(BrowsingDataFilterBuilder::OriginMatchingMode::
                       kThirdPartiesIncluded) {}

BrowsingDataFilterBuilderImpl::BrowsingDataFilterBuilderImpl(
    Mode mode,
    OriginMatchingMode origin_mode)
    : mode_(mode), origin_mode_(origin_mode) {}

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

void BrowsingDataFilterBuilderImpl::SetStorageKey(
    const std::optional<blink::StorageKey>& storage_key) {
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
  return MatchesMostOriginsAndDomains() && origins_.empty() &&
         domains_.empty() && cookie_partition_key_collection_.ContainsAllKeys();
}

bool BrowsingDataFilterBuilderImpl::MatchesMostOriginsAndDomains() {
  return mode_ == Mode::kPreserve && !partitioned_cookies_only_ &&
         !HasStorageKey();
}

bool BrowsingDataFilterBuilderImpl::MatchesNothing() {
  return mode_ == Mode::kDelete && origins_.empty() && domains_.empty();
}

void BrowsingDataFilterBuilderImpl::SetPartitionedCookiesOnly(bool value) {
  partitioned_cookies_only_ = value;
}

bool BrowsingDataFilterBuilderImpl::PartitionedCookiesOnly() const {
  return partitioned_cookies_only_;
}

void BrowsingDataFilterBuilderImpl::SetStoragePartitionConfig(
    const StoragePartitionConfig& storage_partition_config) {
  storage_partition_config_ = storage_partition_config;
}

std::optional<StoragePartitionConfig>
BrowsingDataFilterBuilderImpl::GetStoragePartitionConfig() {
  return storage_partition_config_;
}

base::RepeatingCallback<bool(const GURL&)>
BrowsingDataFilterBuilderImpl::BuildUrlFilter() {
  if (MatchesAllOriginsAndDomains())
    return base::BindRepeating([](const GURL&) { return true; });
  return base::BindRepeating(&MatchesURL, origins_, domains_, mode_,
                             origin_mode_, PartitionedCookiesOnly());
}

content::StoragePartition::StorageKeyMatcherFunction
BrowsingDataFilterBuilderImpl::BuildStorageKeyFilter() {
  if (MatchesAllOriginsAndDomains())
    return base::BindRepeating([](const blink::StorageKey&) { return true; });
  // If the filter has a StorageKey set, use it to match.
  if (HasStorageKey()) {
    CHECK(StorageKeyInCookiePartitionKeyCollection(
        *storage_key_, cookie_partition_key_collection_));
    return base::BindRepeating(
        &BrowsingDataFilterBuilderImpl::MatchesWithSavedStorageKey,
        base::Unretained(this));
  }
  return base::BindRepeating(&MatchesStorageKey, origins_, domains_, mode_,
                             origin_mode_);
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

  // TODO(crbug.com/329705715) Add support for storage partitioning.

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

  deletion_filter->partitioned_state_only = partitioned_cookies_only_;

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

BrowsingDataFilterBuilderImpl::OriginMatchingMode
BrowsingDataFilterBuilderImpl::GetOriginModeForTesting() const {
  return origin_mode_;
}

const std::set<url::Origin>& BrowsingDataFilterBuilderImpl::GetOrigins() const {
  return origins_;
}

const std::set<std::string>&
BrowsingDataFilterBuilderImpl::GetRegisterableDomains() const {
  return domains_;
}

const net::CookiePartitionKeyCollection&
BrowsingDataFilterBuilderImpl::GetCookiePartitionKeyCollectionForTesting()
    const {
  return cookie_partition_key_collection_;
}
std::unique_ptr<BrowsingDataFilterBuilder>
BrowsingDataFilterBuilderImpl::Copy() {
  std::unique_ptr<BrowsingDataFilterBuilderImpl> copy =
      std::make_unique<BrowsingDataFilterBuilderImpl>(mode_);
  copy->origin_mode_ = origin_mode_;
  copy->origins_ = origins_;
  copy->domains_ = domains_;
  copy->cookie_partition_key_collection_ = cookie_partition_key_collection_;
  copy->storage_key_ = storage_key_;
  copy->partitioned_cookies_only_ = partitioned_cookies_only_;
  copy->storage_partition_config_ = storage_partition_config_;
  return std::move(copy);
}

bool BrowsingDataFilterBuilderImpl::IsEqual(
    const BrowsingDataFilterBuilder& other) const {
  // This is the only implementation of BrowsingDataFilterBuilder, so we can
  // downcast |other|.
  const BrowsingDataFilterBuilderImpl* other_impl =
      static_cast<const BrowsingDataFilterBuilderImpl*>(&other);

  return origins_ == other_impl->origins_ && domains_ == other_impl->domains_ &&
         mode_ == other_impl->mode_ &&
         origin_mode_ == other_impl->origin_mode_ &&
         cookie_partition_key_collection_ ==
             other_impl->cookie_partition_key_collection_ &&
         storage_key_ == other_impl->storage_key_ &&
         partitioned_cookies_only_ == other_impl->partitioned_cookies_only_ &&
         storage_partition_config_ == other_impl->storage_partition_config_;
}

}  // namespace content
