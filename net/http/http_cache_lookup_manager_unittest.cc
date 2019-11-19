// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "net/base/features.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_cache_lookup_manager.h"
#include "net/http/http_transaction_test_util.h"
#include "net/http/mock_http_cache.h"
#include "net/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsOk;

namespace net {

namespace {

class MockServerPushHelper : public ServerPushDelegate::ServerPushHelper {
 public:
  explicit MockServerPushHelper(const GURL& url) : request_url_(url) {}

  const GURL& GetURL() const override { return request_url_; }

  MOCK_METHOD0(Cancel, void());

 private:
  const GURL request_url_;
};

std::unique_ptr<MockTransaction> CreateMockTransaction(const GURL& url) {
  MockTransaction mock_trans = {
      url.spec().c_str(), "GET", base::Time(), "", LOAD_NORMAL,
      "HTTP/1.1 200 OK",
      "Date: Wed, 28 Nov 2007 09:40:09 GMT\n"
      "Last-Modified: Wed, 28 Nov 2007 00:40:09 GMT\n",
      base::Time(), "<html><body>Google Blah Blah</body></html>",
      TEST_MODE_NORMAL, nullptr, nullptr, nullptr, 0, 0, OK};
  return std::make_unique<MockTransaction>(mock_trans);
}

void PopulateCacheEntry(HttpCache* cache, const GURL& request_url) {
  TestCompletionCallback callback;

  std::unique_ptr<MockTransaction> mock_trans =
      CreateMockTransaction(request_url);
  AddMockTransaction(mock_trans.get());

  MockHttpRequest request(*(mock_trans.get()));

  std::unique_ptr<HttpTransaction> trans;
  int rv = cache->CreateTransaction(DEFAULT_PRIORITY, &trans);
  EXPECT_THAT(rv, IsOk());
  ASSERT_TRUE(trans.get());

  rv = trans->Start(&request, callback.callback(), NetLogWithSource());
  base::RunLoop().RunUntilIdle();

  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();

  ASSERT_EQ(mock_trans->start_return_code, rv);
  if (OK != rv)
    return;

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response);

  std::string content;
  rv = ReadTransaction(trans.get(), &content);

  EXPECT_THAT(rv, IsOk());
  std::string expected(mock_trans->data);
  EXPECT_EQ(expected, content);
  RemoveMockTransaction(mock_trans.get());
}

}  // namespace

TEST(HttpCacheLookupManagerTest, ServerPushMissCache) {
  base::test::TaskEnvironment task_environment;
  MockHttpCache mock_cache;
  HttpCacheLookupManager push_delegate(mock_cache.http_cache());
  GURL request_url("http://www.example.com/pushed.jpg");

  std::unique_ptr<MockServerPushHelper> push_helper =
      std::make_unique<MockServerPushHelper>(request_url);
  MockServerPushHelper* push_helper_ptr = push_helper.get();

  // Receive a server push and should not cancel the push.
  EXPECT_CALL(*push_helper_ptr, Cancel()).Times(0);
  push_delegate.OnPush(std::move(push_helper), NetLogWithSource());
  base::RunLoop().RunUntilIdle();

  // Make sure no network transaction is created.
  EXPECT_EQ(0, mock_cache.network_layer()->transaction_count());
  EXPECT_EQ(0, mock_cache.disk_cache()->open_count());
  EXPECT_EQ(0, mock_cache.disk_cache()->create_count());
}

TEST(HttpCacheLookupManagerTest, ServerPushDoNotCreateCacheEntry) {
  base::test::TaskEnvironment task_environment;
  MockHttpCache mock_cache;
  HttpCacheLookupManager push_delegate(mock_cache.http_cache());
  GURL request_url("http://www.example.com/pushed.jpg");

  std::unique_ptr<MockServerPushHelper> push_helper =
      std::make_unique<MockServerPushHelper>(request_url);
  MockServerPushHelper* push_helper_ptr = push_helper.get();

  // Receive a server push and should not cancel the push.
  EXPECT_CALL(*push_helper_ptr, Cancel()).Times(0);
  push_delegate.OnPush(std::move(push_helper), NetLogWithSource());
  base::RunLoop().RunUntilIdle();

  // Receive another server push for the same url.
  std::unique_ptr<MockServerPushHelper> push_helper2 =
      std::make_unique<MockServerPushHelper>(request_url);
  MockServerPushHelper* push_helper_ptr2 = push_helper2.get();
  EXPECT_CALL(*push_helper_ptr2, Cancel()).Times(0);
  push_delegate.OnPush(std::move(push_helper2), NetLogWithSource());
  base::RunLoop().RunUntilIdle();

  // Verify no network transaction is created.
  EXPECT_EQ(0, mock_cache.network_layer()->transaction_count());
  EXPECT_EQ(0, mock_cache.disk_cache()->open_count());
  // Verify no cache entry is created for server push lookup.
  EXPECT_EQ(0, mock_cache.disk_cache()->create_count());
}

TEST(HttpCacheLookupManagerTest, ServerPushHitCache) {
  // Skip test if split cache is enabled, as it breaks push.
  // crbug.com/1009619
  if (base::FeatureList::IsEnabled(
          net::features::kSplitCacheByNetworkIsolationKey)) {
    return;
  }

  base::test::TaskEnvironment task_environment;
  MockHttpCache mock_cache;
  HttpCacheLookupManager push_delegate(mock_cache.http_cache());
  GURL request_url("http://www.example.com/pushed.jpg");

  // Populate the cache entry so that the cache lookup for server push hits.
  PopulateCacheEntry(mock_cache.http_cache(), request_url);
  EXPECT_EQ(1, mock_cache.network_layer()->transaction_count());
  EXPECT_EQ(0, mock_cache.disk_cache()->open_count());
  EXPECT_EQ(1, mock_cache.disk_cache()->create_count());

  // Add another mock transaction since the OnPush will create a new cache
  // transaction.
  std::unique_ptr<MockTransaction> mock_trans =
      CreateMockTransaction(request_url);
  AddMockTransaction(mock_trans.get());

  std::unique_ptr<MockServerPushHelper> push_helper =
      std::make_unique<MockServerPushHelper>(request_url);
  MockServerPushHelper* push_helper_ptr = push_helper.get();

  // Receive a server push and should cancel the push.
  EXPECT_CALL(*push_helper_ptr, Cancel()).Times(1);
  push_delegate.OnPush(std::move(push_helper), NetLogWithSource());
  base::RunLoop().RunUntilIdle();

  // Make sure no new net layer transaction is created.
  EXPECT_EQ(1, mock_cache.network_layer()->transaction_count());
  EXPECT_EQ(1, mock_cache.disk_cache()->open_count());
  EXPECT_EQ(1, mock_cache.disk_cache()->create_count());

  RemoveMockTransaction(mock_trans.get());
}

// Test when a server push is received while the HttpCacheLookupManager has a
// pending lookup transaction for the same URL, the new server push will not
// send a new lookup transaction and should not be canceled.
TEST(HttpCacheLookupManagerTest, ServerPushPendingLookup) {
  // Skip test if split cache is enabled, as it breaks push.
  // crbug.com/1009619
  if (base::FeatureList::IsEnabled(
          net::features::kSplitCacheByNetworkIsolationKey)) {
    return;
  }

  base::test::TaskEnvironment task_environment;
  MockHttpCache mock_cache;
  HttpCacheLookupManager push_delegate(mock_cache.http_cache());
  GURL request_url("http://www.example.com/pushed.jpg");

  // Populate the cache entry so that the cache lookup for server push hits.
  PopulateCacheEntry(mock_cache.http_cache(), request_url);
  EXPECT_EQ(1, mock_cache.network_layer()->transaction_count());
  EXPECT_EQ(0, mock_cache.disk_cache()->open_count());
  EXPECT_EQ(1, mock_cache.disk_cache()->create_count());

  // Add another mock transaction since the OnPush will create a new cache
  // transaction.
  std::unique_ptr<MockTransaction> mock_trans =
      CreateMockTransaction(request_url);
  AddMockTransaction(mock_trans.get());

  std::unique_ptr<MockServerPushHelper> push_helper =
      std::make_unique<MockServerPushHelper>(request_url);
  MockServerPushHelper* push_helper_ptr = push_helper.get();

  // Receive a server push and should cancel the push eventually.
  EXPECT_CALL(*push_helper_ptr, Cancel()).Times(1);
  push_delegate.OnPush(std::move(push_helper), NetLogWithSource());

  std::unique_ptr<MockServerPushHelper> push_helper2 =
      std::make_unique<MockServerPushHelper>(request_url);
  MockServerPushHelper* push_helper_ptr2 = push_helper2.get();

  // Receive another server push and should not cancel the push.
  EXPECT_CALL(*push_helper_ptr2, Cancel()).Times(0);
  push_delegate.OnPush(std::move(push_helper2), NetLogWithSource());

  base::RunLoop().RunUntilIdle();

  // Make sure no new net layer transaction is created.
  EXPECT_EQ(1, mock_cache.network_layer()->transaction_count());
  EXPECT_EQ(1, mock_cache.disk_cache()->open_count());
  EXPECT_EQ(1, mock_cache.disk_cache()->create_count());

  RemoveMockTransaction(mock_trans.get());
}

// Test the server push lookup is based on the full url.
TEST(HttpCacheLookupManagerTest, ServerPushLookupOnUrl) {
  // Skip test if split cache is enabled, as it breaks push.
  // crbug.com/1009619
  if (base::FeatureList::IsEnabled(
          net::features::kSplitCacheByNetworkIsolationKey)) {
    return;
  }

  base::test::TaskEnvironment task_environment;
  MockHttpCache mock_cache;
  HttpCacheLookupManager push_delegate(mock_cache.http_cache());
  GURL request_url("http://www.example.com/pushed.jpg?u=0");
  GURL request_url2("http://www.example.com/pushed.jpg?u=1");

  // Populate the cache entry so that the cache lookup for server push hits.
  PopulateCacheEntry(mock_cache.http_cache(), request_url);
  EXPECT_EQ(1, mock_cache.network_layer()->transaction_count());
  EXPECT_EQ(0, mock_cache.disk_cache()->open_count());
  EXPECT_EQ(1, mock_cache.disk_cache()->create_count());

  // Add another mock transaction since the OnPush will create a new cache
  // transaction.
  std::unique_ptr<MockTransaction> mock_trans =
      CreateMockTransaction(request_url);
  AddMockTransaction(mock_trans.get());

  std::unique_ptr<MockServerPushHelper> push_helper =
      std::make_unique<MockServerPushHelper>(request_url);
  MockServerPushHelper* push_helper_ptr = push_helper.get();

  // Receive a server push and should cancel the push eventually.
  EXPECT_CALL(*push_helper_ptr, Cancel()).Times(1);
  push_delegate.OnPush(std::move(push_helper), NetLogWithSource());
  // Run until the lookup transaction finishes for the first server push.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, mock_cache.network_layer()->transaction_count());
  EXPECT_EQ(1, mock_cache.disk_cache()->open_count());
  EXPECT_EQ(1, mock_cache.disk_cache()->create_count());
  RemoveMockTransaction(mock_trans.get());

  AddMockTransaction(mock_trans.get());
  // Receive the second server push with same url after the first lookup
  // finishes, and should cancel the push.
  std::unique_ptr<MockServerPushHelper> push_helper2 =
      std::make_unique<MockServerPushHelper>(request_url);
  MockServerPushHelper* push_helper_ptr2 = push_helper2.get();

  EXPECT_CALL(*push_helper_ptr2, Cancel()).Times(1);
  push_delegate.OnPush(std::move(push_helper2), NetLogWithSource());
  // Run until the lookup transaction finishes for the second server push.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, mock_cache.network_layer()->transaction_count());
  EXPECT_EQ(2, mock_cache.disk_cache()->open_count());
  EXPECT_EQ(1, mock_cache.disk_cache()->create_count());
  RemoveMockTransaction(mock_trans.get());

  std::unique_ptr<MockTransaction> mock_trans3 =
      CreateMockTransaction(request_url2);
  AddMockTransaction(mock_trans3.get());
  // Receive the third server push with a different url after lookup for a
  // similar server push has been completed, should not cancel the push.
  std::unique_ptr<MockServerPushHelper> push_helper3 =
      std::make_unique<MockServerPushHelper>(request_url2);
  MockServerPushHelper* push_helper_ptr3 = push_helper3.get();

  EXPECT_CALL(*push_helper_ptr3, Cancel()).Times(0);
  push_delegate.OnPush(std::move(push_helper3), NetLogWithSource());

  base::RunLoop().RunUntilIdle();
  // Make sure no new net layer transaction is created.
  EXPECT_EQ(1, mock_cache.network_layer()->transaction_count());
  EXPECT_EQ(2, mock_cache.disk_cache()->open_count());
  EXPECT_EQ(1, mock_cache.disk_cache()->create_count());
  RemoveMockTransaction(mock_trans3.get());
}

}  // namespace net
