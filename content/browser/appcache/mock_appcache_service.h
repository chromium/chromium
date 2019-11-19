// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_MOCK_APPCACHE_SERVICE_H_
#define CONTENT_BROWSER_APPCACHE_MOCK_APPCACHE_SERVICE_H_

#include "base/compiler_specific.h"
#include "content/browser/appcache/appcache_service_impl.h"
#include "content/browser/appcache/mock_appcache_storage.h"
#include "storage/browser/quota/quota_manager.h"

namespace content {

// For use by unit tests.
class MockAppCacheService : public AppCacheServiceImpl {
 public:
  explicit MockAppCacheService(base::WeakPtr<StoragePartitionImpl> partition)
      : AppCacheServiceImpl(nullptr, std::move(partition)),
        mock_delete_appcaches_for_origin_result_(net::OK),
        delete_called_count_(0) {
    storage_ = std::make_unique<MockAppCacheStorage>(this);
  }
  MockAppCacheService() : MockAppCacheService(nullptr) {}

  // Just returns a canned completion code without actually
  // removing groups and caches in our mock storage instance.
  void DeleteAppCachesForOrigin(const url::Origin& origin,
                                net::CompletionOnceCallback callback) override;

  void set_quota_manager_proxy(storage::QuotaManagerProxy* proxy) {
    quota_manager_proxy_ = proxy;
  }

  void set_mock_delete_appcaches_for_origin_result(int rv) {
    mock_delete_appcaches_for_origin_result_ = rv;
  }

  int delete_called_count() const { return delete_called_count_; }

 private:
  int mock_delete_appcaches_for_origin_result_;
  int delete_called_count_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_MOCK_APPCACHE_SERVICE_H_
