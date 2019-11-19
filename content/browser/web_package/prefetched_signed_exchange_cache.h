// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_PREFETCHED_SIGNED_EXCHANGE_CACHE_H_
#define CONTENT_BROWSER_WEB_PACKAGE_PREFETCHED_SIGNED_EXCHANGE_CACHE_H_

#include <map>

#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/common/prefetched_signed_exchange_info.mojom.h"
#include "net/base/hash_value.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "url/gurl.h"

namespace net {
struct SHA256HashValue;
}  // namespace net

namespace network {
struct ResourceResponseHead;
struct URLLoaderCompletionStatus;
}  // namespace network

namespace storage {
class BlobDataHandle;
}  // namespace storage

namespace content {

class NavigationLoaderInterceptor;

// PrefetchedSignedExchangeCache keeps prefetched and verified signed
// exchanges.
class CONTENT_EXPORT PrefetchedSignedExchangeCache
    : public base::RefCountedThreadSafe<PrefetchedSignedExchangeCache> {
 public:
  class CONTENT_EXPORT Entry {
   public:
    Entry();
    ~Entry();

    const GURL& outer_url() const { return outer_url_; }
    const std::unique_ptr<const network::ResourceResponseHead>& outer_response()
        const {
      return outer_response_;
    }
    const std::unique_ptr<const net::SHA256HashValue>& header_integrity()
        const {
      return header_integrity_;
    }
    const GURL& inner_url() const { return inner_url_; }
    const std::unique_ptr<const network::ResourceResponseHead>& inner_response()
        const {
      return inner_response_;
    }
    const std::unique_ptr<const network::URLLoaderCompletionStatus>&
    completion_status() const {
      return completion_status_;
    }
    const std::unique_ptr<const storage::BlobDataHandle>& blob_data_handle()
        const {
      return blob_data_handle_;
    }
    base::Time signature_expire_time() const { return signature_expire_time_; }

    void SetOuterUrl(const GURL& outer_url);
    void SetOuterResponse(
        std::unique_ptr<const network::ResourceResponseHead> outer_response);
    void SetHeaderIntegrity(
        std::unique_ptr<const net::SHA256HashValue> header_integrity);
    void SetInnerUrl(const GURL& inner_url);
    void SetInnerResponse(
        std::unique_ptr<const network::ResourceResponseHead> inner_response);
    void SetCompletionStatus(
        std::unique_ptr<const network::URLLoaderCompletionStatus>
            completion_status);
    void SetBlobDataHandle(
        std::unique_ptr<const storage::BlobDataHandle> blob_data_handle);
    void SetSignatureExpireTime(const base::Time& signature_expire_time);

    std::unique_ptr<const Entry> Clone() const;

   private:
    GURL outer_url_;
    std::unique_ptr<const network::ResourceResponseHead> outer_response_;
    std::unique_ptr<const net::SHA256HashValue> header_integrity_;
    GURL inner_url_;
    std::unique_ptr<const network::ResourceResponseHead> inner_response_;
    std::unique_ptr<const network::URLLoaderCompletionStatus>
        completion_status_;
    std::unique_ptr<const storage::BlobDataHandle> blob_data_handle_;
    base::Time signature_expire_time_;

    DISALLOW_COPY_AND_ASSIGN(Entry);
  };

  // A test observer to monitor the cache entry.
  class TestObserver : public base::CheckedObserver {
   public:
    virtual void OnStored(PrefetchedSignedExchangeCache* cache,
                          const GURL& outer_url) = 0;
  };

  using EntryMap = std::map<GURL /* outer_url */, std::unique_ptr<const Entry>>;

  PrefetchedSignedExchangeCache();

  void Store(std::unique_ptr<const Entry> cached_exchange);

  void Clear();

  std::unique_ptr<NavigationLoaderInterceptor> MaybeCreateInterceptor(
      const GURL& outer_url);

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
  std::vector<mojom::PrefetchedSignedExchangeInfoPtr> GetInfoListForNavigation(
      const Entry& main_exchange,
      const base::Time& now);

  EntryMap exchanges_;

  base::ObserverList<TestObserver> test_observers_;

  DISALLOW_COPY_AND_ASSIGN(PrefetchedSignedExchangeCache);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_PREFETCHED_SIGNED_EXCHANGE_CACHE_H_
