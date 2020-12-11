// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_TEST_HELPER_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_TEST_HELPER_H_

#include <set>

#include "base/macros.h"
#include "content/browser/appcache/appcache_storage.h"

namespace content {
class AppCacheServiceImpl;
}

namespace content {

// Helper class for inserting data into a ChromeAppCacheService and reading it
// back.
class AppCacheTestHelper : public AppCacheStorage::Delegate {
 public:
  AppCacheTestHelper();
  ~AppCacheTestHelper() override;
  void AddGroupAndCache(AppCacheServiceImpl* appcache_service,
                        const GURL& manifest_url);

  void GetOriginsWithCaches(AppCacheServiceImpl* appcache_service,
                            std::set<url::Origin>* origins);

 private:
  void OnGroupAndNewestCacheStored(AppCacheGroup* group,
                                   AppCache* newest_cache,
                                   bool success,
                                   bool would_exceed_quota) override;
  void OnGotAppCacheInfo(int rv);

  int group_id_;
  int appcache_id_;
  int response_id_;
  scoped_refptr<AppCacheInfoCollection> appcache_info_;
  std::set<url::Origin>* origins_;  // not owned

  DISALLOW_COPY_AND_ASSIGN(AppCacheTestHelper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_TEST_HELPER_H_
