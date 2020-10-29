// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_disk_cache.h"

#include "base/bind_helpers.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/disk_cache.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(bashi): Port tests from AppCacheDiskCacheTest.

namespace content {

class ServiceWorkerDiskCacheTest : public testing::Test {
 public:
  ServiceWorkerDiskCacheTest() = default;

  void SetUp() override { ASSERT_TRUE(directory_.CreateUniqueTempDir()); }

  void TearDown() override { FlushCacheTasks(); }

  void FlushCacheTasks() {
    disk_cache::FlushCacheThreadForTesting();
    task_environment_.RunUntilIdle();
  }

  void InitializeDiskCache(ServiceWorkerDiskCache* disk_cache) {
    base::RunLoop loop;
    disk_cache->InitWithDiskBackend(
        directory_.GetPath(), false,
        /*post_cleanup_callback=*/base::DoNothing::Once(),
        base::BindLambdaForTesting([&](int rv) {
          ASSERT_EQ(rv, net::OK);
          loop.Quit();
        }));
    loop.Run();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir directory_;
};

// Tests that callbacks of operations are invoked even when these operations are
// called at the same time for the same key.
TEST_F(ServiceWorkerDiskCacheTest, MultipleCallsForSameKey) {
  auto disk_cache = std::make_unique<ServiceWorkerDiskCache>();

  bool create_entry_called = false;
  bool open_entry_called = false;
  bool doom_entry_called = false;

  const int64_t kKey = 1;
  disk_cache->CreateEntry(
      kKey, base::BindLambdaForTesting(
                [&](int rv, std::unique_ptr<ServiceWorkerDiskCacheEntry>) {
                  create_entry_called = true;
                }));
  disk_cache->OpenEntry(
      kKey, base::BindLambdaForTesting(
                [&](int rv, std::unique_ptr<ServiceWorkerDiskCacheEntry>) {
                  open_entry_called = true;
                }));
  disk_cache->DoomEntry(kKey, base::BindLambdaForTesting(
                                  [&](int rv) { doom_entry_called = true; }));

  InitializeDiskCache(disk_cache.get());
  FlushCacheTasks();

  EXPECT_TRUE(create_entry_called);
  EXPECT_TRUE(open_entry_called);
  EXPECT_TRUE(doom_entry_called);
}

}  // namespace content
