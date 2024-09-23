// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSING_DATA_FILTER_BUILDER_H_
#define CONTENT_PUBLIC_BROWSER_BROWSING_DATA_FILTER_BUILDER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "content/common/content_export.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"
#include "net/cookies/cookie_partition_key_collection.h"
#include "services/network/public/mojom/clear_data_filter.mojom-forward.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

class GURL;

namespace url {
class Origin;
}  // namespace url

namespace content {

class StoragePartitionConfig;

// A class that builds Origin->bool predicates to filter browsing data. These
// filters can be of two modes - a list of items to delete or a list of items to
// preserve, deleting everything else. The filter entries can be origins or
// registrable domains.
//
// This class defines interface to build filters for various kinds of browsing
// data. |BuildStorageKeyFilter()| is useful for most browsing data storage
// backends, but some backends, such as website settings and cookies, use other
// formats of filter.
class CONTENT_EXPORT BrowsingDataFilterBuilder {
 public:
  enum class Mode {
    // Only the origins given will be deleted.
    kDelete,
    // All origins EXCEPT the origins given will be deleted.
    kPreserve
  };

  // This determines how StorageKeys will be matched given an origin that was
  // added to the filter.
  enum class OriginMatchingMode {
    // Default mode: StorageKeys are matched on origin in 1P context and on
    // top-level-site in 3P contexts. For deletion that means that the origin
    // and everything embedded on it is deleted, but the instances when the
    // origin itself is embedded are left untouched.
    kThirdPartiesIncluded,
    // Second option: StorageKeys are matched on origin only in all contexts.
    // For deletion that means that the origin is deleted in both 1P and 3P
    // contexts, but anything embedded on it is left untouched.
    kOriginInAllContexts
  };

  // Constructs a filter with the given |mode|: delete or preserve.
  // The |OriginMatchingMode| is |kThirdPartiesIncluded| in this case by
  // default.
  static std::unique_ptr<BrowsingDataFilterBuilder> Create(Mode mode);

  // Same as above, but also allows to choose how origins are matched with
  // storage keys.
  static std::unique_ptr<BrowsingDataFilterBuilder> Create(
      Mode mode,
      OriginMatchingMode origin_mode);

  virtual ~BrowsingDataFilterBuilder() = default;

  // Adds an origin to the filter. Note that this makes it impossible to
  // create cookie, channel ID, or plugin filters, as those datatypes are
  // scoped more broadly than an origin.
  virtual void AddOrigin(const url::Origin& origin) = 0;

  // Adds a registrable domain (e.g. google.com), an internal hostname
  // (e.g. localhost), or an IP address (e.g. 127.0.0.1). Other domains, such
  // as third and lower level domains (e.g. www.google.com) are not accepted.
  // Formally, it must hold that GetDomainAndRegistry(|registrable_domain|, _)
  // is |registrable_domain| itself or an empty string for this method
  // to accept it.
  virtual void AddRegisterableDomain(const std::string& registrable_domain) = 0;

  // Set the CookiePartitionKeyCollection for a CookieDeletionFilter.
  // Partitioned cookies will be not be deleted if their partition key is not in
  // the collection. If this method is not invoked, then by default this clears
  // all partitioned cookies that match the other criteria. Passing an empty
  // collection will prevent every partitioned cookie from being deleted.
  virtual void SetCookiePartitionKeyCollection(
      const net::CookiePartitionKeyCollection&
          cookie_partition_key_collection) = 0;

  // Set the StorageKey for the filter.
  // If the key is set, then only the StoragePartition that matches the key
  // exactly will be deleted. Without the key, all storage that matches the
  // other criteria is deleted.
  virtual void SetStorageKey(
      const std::optional<blink::StorageKey>& storage_key) = 0;

  // Returns whether the StorageKey is set (e.g. using the method above).
  virtual bool HasStorageKey() const = 0;

  // Returns whether the filter's StorageKey matches the given one.
  // Note: the StorageKey in the filter has to be set.
  virtual bool MatchesWithSavedStorageKey(
      const blink::StorageKey& other_key) const = 0;

  // Returns true if we're an empty preserve list, where we delete everything.
  virtual bool MatchesAllOriginsAndDomains() = 0;

  // Returns true if we're deleting everything or nearly everything -- the mode
  // is kPreserve, we're not restricted to partitioned cookies, and no
  // StorageKey is set.
  virtual bool MatchesMostOriginsAndDomains() = 0;

  // Returns true if we're an empty delete list, where we delete nothing.
  virtual bool MatchesNothing() = 0;

  // When true, this filter will exclude unpartitioned cookies, i.e. cookies
  // whose partition key is null. By default, the value is false.
  // Setting this will NOT ensure that all partitioned cookies will match, only
  // that unpartitioned cookies will not match. Partitioned cookie matching is
  // governed by SetCookiePartitionKeyCollection() and AddRegisterableDomain().
  virtual void SetPartitionedCookiesOnly(bool value) = 0;

  // Returns true if this filter is restricted to partitioned cookies (by the
  // method above). Note that partitioned cookies may be deleted whether this
  // returns true or false.
  virtual bool PartitionedCookiesOnly() const = 0;

  // When set, only data from the given StoragePartition will be removed.
  // By default, data from non-default StoragePartitions will not be removed.
  // This should not be used when removing Profile-scoped data types.
  virtual void SetStoragePartitionConfig(
      const StoragePartitionConfig& storage_partition_config) = 0;

  virtual std::optional<StoragePartitionConfig> GetStoragePartitionConfig() = 0;

  // Deprecated: Prefer `BuildStorageKeyFilter()` instead.
  // Builds a filter that matches URLs that are in the list to delete, or aren't
  // in the list to preserve.
  virtual base::RepeatingCallback<bool(const GURL&)> BuildUrlFilter() = 0;

  // Builds a filter that matches storage keys that are in the list to delete,
  // or aren't in the list to preserve. This is preferred to BuildUrlFilter() as
  // it preserves storage partitioning information.
  virtual content::StoragePartition::StorageKeyMatcherFunction
  BuildStorageKeyFilter() = 0;

  // Builds a filter that can be used with the network service. This uses a Mojo
  // struct rather than a predicate function (as used by the rest of the filters
  // built by this class) because we need to be able to pass the filter to the
  // network service via IPC. Returns nullptr if |IsEmptyPreserveList()| is
  // true.
  virtual network::mojom::ClearDataFilterPtr BuildNetworkServiceFilter() = 0;

  // Builds a CookieDeletionInfo object that matches cookies whose sources are
  // in the list to delete, or aren't in the list to preserve.
  virtual network::mojom::CookieDeletionFilterPtr
  BuildCookieDeletionFilter() = 0;

  // Builds a filter that matches the |site| of a plugin.
  virtual base::RepeatingCallback<bool(const std::string& site)>
  BuildPluginFilter() = 0;

  // A convenience method to produce an empty preserve list, a filter that
  // matches everything.
  static base::RepeatingCallback<bool(const GURL&)> BuildNoopFilter();

  // The mode of the filter.
  virtual Mode GetMode() = 0;

  // The origins targeted by the filter.
  virtual const std::set<url::Origin>& GetOrigins() const = 0;

  // The domains targeted by the filter.
  virtual const std::set<std::string>& GetRegisterableDomains() const = 0;

  // Create a new filter builder with the same set of origins, set of domains,
  // and mode.
  virtual std::unique_ptr<BrowsingDataFilterBuilder> Copy() = 0;

  // Comparison.
  bool operator==(const BrowsingDataFilterBuilder& other) const {
    return IsEqual(other);
  }

 private:
  virtual bool IsEqual(const BrowsingDataFilterBuilder& other) const = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSING_DATA_FILTER_BUILDER_H_
