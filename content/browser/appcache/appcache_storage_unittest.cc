// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_storage.h"

#include "content/browser/appcache/appcache.h"
#include "content/browser/appcache/appcache_group.h"
#include "content/browser/appcache/appcache_response.h"
#include "content/browser/appcache/mock_appcache_service.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace appcache_storage_unittest {

const blink::mojom::StorageType kTemp = blink::mojom::StorageType::kTemporary;

class AppCacheStorageTest : public testing::Test {
 public:
  class MockStorageDelegate : public AppCacheStorage::Delegate {
   public:
  };

 private:
  BrowserTaskEnvironment task_environment_;
};

TEST_F(AppCacheStorageTest, AddRemoveCache) {
  MockAppCacheService service;
  scoped_refptr<AppCache> cache =
      base::MakeRefCounted<AppCache>(service.storage(), 111);

  EXPECT_EQ(cache.get(),
            service.storage()->working_set()->GetCache(111));

  service.storage()->working_set()->RemoveCache(cache.get());

  EXPECT_TRUE(!service.storage()->working_set()->GetCache(111));

  // Removing non-existing cache from service should not fail.
  MockAppCacheService dummy;
  dummy.storage()->working_set()->RemoveCache(cache.get());
}

TEST_F(AppCacheStorageTest, AddRemoveGroup) {
  MockAppCacheService service;
  const GURL kManifestUrl("http://origin/");
  scoped_refptr<AppCacheGroup> group =
      base::MakeRefCounted<AppCacheGroup>(service.storage(), kManifestUrl, 111);

  EXPECT_EQ(group.get(),
            service.storage()->working_set()->GetGroup(kManifestUrl));

  service.storage()->working_set()->RemoveGroup(group.get());

  EXPECT_TRUE(!service.storage()->working_set()->GetGroup(kManifestUrl));

  // Removing non-existing group from service should not fail.
  MockAppCacheService dummy;
  dummy.storage()->working_set()->RemoveGroup(group.get());
}

TEST_F(AppCacheStorageTest, AddRemoveResponseInfo) {
  MockAppCacheService service;
  const GURL kManifestUrl("http://origin/");
  scoped_refptr<AppCacheResponseInfo> info =
      base::MakeRefCounted<AppCacheResponseInfo>(
          service.storage()->GetWeakPtr(), kManifestUrl, 111,
          std::make_unique<net::HttpResponseInfo>(), kUnknownResponseDataSize);

  EXPECT_EQ(info.get(),
            service.storage()->working_set()->GetResponseInfo(111));

  service.storage()->working_set()->RemoveResponseInfo(info.get());

  EXPECT_FALSE(service.storage()->working_set()->GetResponseInfo(111));

  // Removing non-existing info from service should not fail.
  MockAppCacheService dummy;
  dummy.storage()->working_set()->RemoveResponseInfo(info.get());
}

TEST_F(AppCacheStorageTest, ResponseInfoLifetime) {
  scoped_refptr<AppCacheResponseInfo> info;
  {
    MockAppCacheService service;
    const GURL kManifestUrl("http://origin/");
    info = base::MakeRefCounted<AppCacheResponseInfo>(
        service.storage()->GetWeakPtr(), kManifestUrl, 111,
        std::make_unique<net::HttpResponseInfo>(), kUnknownResponseDataSize);

    EXPECT_EQ(info.get(),
              service.storage()->working_set()->GetResponseInfo(111));
  }

  // Outliving the AppCacheService should not be fatal.
  info.reset();
}

TEST_F(AppCacheStorageTest, DelegateReferences) {
  using ScopedDelegateReference =
      scoped_refptr<AppCacheStorage::DelegateReference>;
  MockAppCacheService service;
  MockStorageDelegate delegate;
  ScopedDelegateReference delegate_reference1;
  ScopedDelegateReference delegate_reference2;

  EXPECT_FALSE(service.storage()->GetDelegateReference(&delegate));

  delegate_reference1 =
      service.storage()->GetOrCreateDelegateReference(&delegate);
  EXPECT_TRUE(delegate_reference1.get());
  EXPECT_TRUE(delegate_reference1->HasOneRef());
  EXPECT_TRUE(service.storage()->GetDelegateReference(&delegate));
  EXPECT_EQ(&delegate,
            service.storage()->GetDelegateReference(&delegate)->delegate);
  EXPECT_EQ(service.storage()->GetDelegateReference(&delegate),
            service.storage()->GetOrCreateDelegateReference(&delegate));
  delegate_reference1 = nullptr;
  EXPECT_FALSE(service.storage()->GetDelegateReference(&delegate));

  delegate_reference1 =
      service.storage()->GetOrCreateDelegateReference(&delegate);
  service.storage()->CancelDelegateCallbacks(&delegate);
  EXPECT_TRUE(delegate_reference1.get());
  EXPECT_TRUE(delegate_reference1->HasOneRef());
  EXPECT_FALSE(delegate_reference1->delegate);
  EXPECT_FALSE(service.storage()->GetDelegateReference(&delegate));

  delegate_reference2 =
      service.storage()->GetOrCreateDelegateReference(&delegate);
  EXPECT_TRUE(delegate_reference2.get());
  EXPECT_TRUE(delegate_reference2->HasOneRef());
  EXPECT_EQ(&delegate, delegate_reference2->delegate);
  EXPECT_NE(delegate_reference1.get(), delegate_reference2.get());
}

TEST_F(AppCacheStorageTest, UsageMap) {
  const url::Origin kOrigin(url::Origin::Create(GURL("http://origin/")));
  const url::Origin kOrigin2(url::Origin::Create(GURL("http://origin2/")));

  MockAppCacheService service;
  scoped_refptr<MockQuotaManagerProxy> mock_proxy =
      base::MakeRefCounted<MockQuotaManagerProxy>(nullptr, nullptr);
  service.set_quota_manager_proxy(mock_proxy.get());

  service.storage()->UpdateUsageMapAndNotify(kOrigin, 0);
  EXPECT_EQ(0, mock_proxy->notify_storage_modified_count());

  service.storage()->UpdateUsageMapAndNotify(kOrigin, 10);
  EXPECT_EQ(1, mock_proxy->notify_storage_modified_count());
  EXPECT_EQ(10, mock_proxy->last_notified_delta());
  EXPECT_EQ(kOrigin, mock_proxy->last_notified_origin());
  EXPECT_EQ(kTemp, mock_proxy->last_notified_type());

  service.storage()->UpdateUsageMapAndNotify(kOrigin, 100);
  EXPECT_EQ(2, mock_proxy->notify_storage_modified_count());
  EXPECT_EQ(90, mock_proxy->last_notified_delta());
  EXPECT_EQ(kOrigin, mock_proxy->last_notified_origin());
  EXPECT_EQ(kTemp, mock_proxy->last_notified_type());

  service.storage()->UpdateUsageMapAndNotify(kOrigin, 0);
  EXPECT_EQ(3, mock_proxy->notify_storage_modified_count());
  EXPECT_EQ(-100, mock_proxy->last_notified_delta());
  EXPECT_EQ(kOrigin, mock_proxy->last_notified_origin());
  EXPECT_EQ(kTemp, mock_proxy->last_notified_type());

  service.storage()->NotifyStorageAccessed(kOrigin2);
  EXPECT_EQ(0, mock_proxy->notify_storage_accessed_count());

  service.storage()->usage_map_[kOrigin2] = 1;
  service.storage()->NotifyStorageAccessed(kOrigin2);
  EXPECT_EQ(1, mock_proxy->notify_storage_accessed_count());
  EXPECT_EQ(kOrigin2, mock_proxy->last_notified_origin());
  EXPECT_EQ(kTemp, mock_proxy->last_notified_type());

  service.storage()->usage_map_.clear();
  service.storage()->usage_map_[kOrigin] = 5000;
  service.storage()->ClearUsageMapAndNotify();
  EXPECT_EQ(4, mock_proxy->notify_storage_modified_count());
  EXPECT_EQ(-5000, mock_proxy->last_notified_delta());
  EXPECT_EQ(kOrigin, mock_proxy->last_notified_origin());
  EXPECT_EQ(kTemp, mock_proxy->last_notified_type());
  EXPECT_TRUE(service.storage()->usage_map_.empty());
}

}  // namespace appcache_storage_unittest
}  // namespace content
