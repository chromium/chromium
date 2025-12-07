// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_PREFETCHED_SIGNED_EXCHANGE_CACHE_H_
#define CONTENT_BROWSER_WEB_PACKAGE_PREFETCHED_SIGNED_EXCHANGE_CACHE_H_

#include <map>

#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "content/browser/web_package/prefetched_signed_exchange_cache_entry.h"
#include "content/common/content_export.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "net/base/hash_value.h"
#include "third_party/blink/public/mojom/navigation/prefetched_signed_exchange_info.mojom.h"
#include "url/gurl.h"

namespace net {
class IsolationInfo;
class NetworkAnonymizationKey;
}

namespace content {

class NavigationLoaderInterceptor;

// PrefetchedSignedExchangeCache keeps prefetched and verified signed
// exchanges.
class CONTENT_EXPORT PrefetchedSignedExchangeCache
    : public base::RefCountedThreadSafe<PrefetchedSignedExchangeCache> {
 public:
  // A test observer to monitor the cache entry.
  class TestObserver : public base::CheckedObserver {
   public:
    virtual void OnStored(PrefetchedSignedExchangeCache* cache,
                          const GURL& outer_url) = 0;
  };

  using EntryMap =
      std::map<GURL /* outer_url */,
               std::unique_ptr<const PrefetchedSignedExchangeCacheEntry>>;

  PrefetchedSignedExchangeCache();

  PrefetchedSignedExchangeCache(const PrefetchedSignedExchangeCache&) = delete;
  PrefetchedSignedExchangeCache& operator=(
      const PrefetchedSignedExchangeCache&) = delete;

  void Store(std::unique_ptr<const PrefetchedSignedExchangeCacheEntry>
                 cached_exchange);

  void Clear();

  // If there is a matching entry for |outer_url| in the cache, returns a
  // NavigationLoaderInterceptor which will load the entry. Otherwise, returns
  // null.
  // |frame_tree_node_id| is used to send a NEL report when there is a mismatch
  // between the 'header-integrity' value of 'allowed-alt-sxg' link header of
  // the cached main resource and the header integrity value of the cached
  // subresource.
  std::unique_ptr<NavigationLoaderInterceptor> MaybeCreateInterceptor(
      const GURL& outer_url,
      FrameTreeNodeId frame_tree_node_id,
      const net::IsolationInfo& isolation_info);

  const EntryMap& GetExchanges();

  void RecordHistograms();

  // Adds/removes test observers.
  void AddObserverForTesting(TestObserver* observer);
  void RemoveObserverForTesting(const TestObserver* observer);

 private:
  friend class base::RefCountedThreadSafe<PrefetchedSignedExchangeCache>;

  ~PrefetchedSignedExchangeCache();

  // Returns PrefetchedSignedExchangeInfo of entries in |exchanges_| which are
  // not expired and which are declared in the "allowed-alt-sxg" link header of
  // |main_exchange|'s inner response and which outer URL's origin is same as
  // the origin of |main_exchange|'s outer URL. Note that this method erases
  // expired entries in |exchanges_|.
  std::vector<blink::mojom::PrefetchedSignedExchangeInfoPtr>
  GetInfoListForNavigation(
      const PrefetchedSignedExchangeCacheEntry& main_exchange,
      const base::Time& now,
      FrameTreeNodeId frame_tree_node_id,
      const net::NetworkAnonymizationKey& network_anonymization_key);

  EntryMap exchanges_;

  base::ObserverList<TestObserver> test_observers_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_PREFETCHED_SIGNED_EXCHANGE_CACHE_H_
