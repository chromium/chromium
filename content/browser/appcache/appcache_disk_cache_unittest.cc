// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_disk_cache.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/completion_repeating_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/disk_cache/disk_cache.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class AppCacheDiskCacheTest : public testing::Test {
 public:
  AppCacheDiskCacheTest() {}

  void SetUp() override {
    ASSERT_TRUE(directory_.CreateUniqueTempDir());
    completion_callback_ = base::BindRepeating(
        &AppCacheDiskCacheTest::OnComplete, base::Unretained(this));
  }

  void TearDown() override { scoped_task_environment_.RunUntilIdle(); }

  void FlushCacheTasks() {
    disk_cache::FlushCacheThreadForTesting();
    scoped_task_environment_.RunUntilIdle();
  }

  void OnComplete(int err) {
    completion_results_.push_back(err);
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  base::ScopedTempDir directory_;
  net::CompletionRepeatingCallback completion_callback_;
  std::vector<int> completion_results_;

  static const int k10MBytes = 10 * 1024 * 1024;
};

TEST_F(AppCacheDiskCacheTest, DisablePriorToInitCompletion) {
  AppCacheDiskCacheEntry* entry = nullptr;

  // Create an instance and start it initializing, queue up
  // one of each kind of "entry" function.
  std::unique_ptr<AppCacheDiskCache> disk_cache(new AppCacheDiskCache);
  EXPECT_FALSE(disk_cache->is_disabled());
  disk_cache->InitWithDiskBackend(directory_.GetPath(), k10MBytes, false,
                                  base::OnceClosure(), completion_callback_);
  disk_cache->CreateEntry(1, &entry, completion_callback_);
  disk_cache->OpenEntry(2, &entry, completion_callback_);
  disk_cache->DoomEntry(3, completion_callback_);

  // Pull the plug on all that.
  EXPECT_FALSE(disk_cache->is_disabled());
  disk_cache->Disable();
  EXPECT_TRUE(disk_cache->is_disabled());

  FlushCacheTasks();

  EXPECT_EQ(nullptr, entry);
  EXPECT_EQ(4u, completion_results_.size());
  for (const auto& result : completion_results_) {
    EXPECT_EQ(net::ERR_ABORTED, result);
  }

  // Ensure the directory can be deleted at this point.
  EXPECT_TRUE(base::DirectoryExists(directory_.GetPath()));
  EXPECT_FALSE(base::IsDirectoryEmpty(directory_.GetPath()));
  EXPECT_TRUE(base::DeleteFile(directory_.GetPath(), true));
  EXPECT_FALSE(base::DirectoryExists(directory_.GetPath()));
}

TEST_F(AppCacheDiskCacheTest, DisableAfterInitted) {
  // Create an instance and let it fully init.
  std::unique_ptr<AppCacheDiskCache> disk_cache(new AppCacheDiskCache);
  EXPECT_FALSE(disk_cache->is_disabled());
  disk_cache->InitWithDiskBackend(directory_.GetPath(), k10MBytes, false,
                                  base::OnceClosure(), completion_callback_);
  FlushCacheTasks();
  EXPECT_EQ(1u, completion_results_.size());
  EXPECT_EQ(net::OK, completion_results_[0]);

  // Pull the plug
  disk_cache->Disable();
  FlushCacheTasks();

  // Ensure the directory can be deleted at this point.
  EXPECT_TRUE(base::DirectoryExists(directory_.GetPath()));
  EXPECT_FALSE(base::IsDirectoryEmpty(directory_.GetPath()));
  EXPECT_TRUE(base::DeleteFile(directory_.GetPath(), true));
  EXPECT_FALSE(base::DirectoryExists(directory_.GetPath()));

  // Methods should return immediately when disabled and not invoke
  // the callback at all.
  AppCacheDiskCacheEntry* entry = nullptr;
  completion_results_.clear();
  EXPECT_EQ(net::ERR_ABORTED,
            disk_cache->CreateEntry(1, &entry, completion_callback_));
  EXPECT_EQ(net::ERR_ABORTED,
            disk_cache->OpenEntry(2, &entry, completion_callback_));
  EXPECT_EQ(net::ERR_ABORTED,
            disk_cache->DoomEntry(3, completion_callback_));
  FlushCacheTasks();
  EXPECT_TRUE(completion_results_.empty());
}

// Flaky on Android: http://crbug.com/339534
TEST_F(AppCacheDiskCacheTest, DISABLED_DisableWithEntriesOpen) {
  // Create an instance and let it fully init.
  std::unique_ptr<AppCacheDiskCache> disk_cache(new AppCacheDiskCache);
  EXPECT_FALSE(disk_cache->is_disabled());
  disk_cache->InitWithDiskBackend(directory_.GetPath(), k10MBytes, false,
                                  base::OnceClosure(), completion_callback_);
  FlushCacheTasks();
  EXPECT_EQ(1u, completion_results_.size());
  EXPECT_EQ(net::OK, completion_results_[0]);

  // Note: We don't have detailed expectations of the DiskCache
  // operations because on android it's really SimpleCache which
  // does behave differently.
  //
  // What matters for the corruption handling and service reinitiazation
  // is that the directory can be deleted after the calling Disable() method,
  // and we do have expectations about that.

  // Create/open some entries.
  AppCacheDiskCacheEntry* entry1 = nullptr;
  AppCacheDiskCacheEntry* entry2 = nullptr;
  disk_cache->CreateEntry(1, &entry1, completion_callback_);
  disk_cache->CreateEntry(2, &entry2, completion_callback_);
  FlushCacheTasks();
  EXPECT_TRUE(entry1);
  EXPECT_TRUE(entry2);

  // Write something to one of the entries and flush it.
  const char* kData = "Hello";
  const int kDataLen = strlen(kData) + 1;
  scoped_refptr<net::IOBuffer> write_buf =
      base::MakeRefCounted<net::WrappedIOBuffer>(kData);
  entry1->Write(0, 0, write_buf.get(), kDataLen, completion_callback_);
  FlushCacheTasks();

  // Queue up a read and a write.
  scoped_refptr<net::IOBuffer> read_buf =
      base::MakeRefCounted<net::IOBuffer>(kDataLen);
  entry1->Read(0, 0, read_buf.get(), kDataLen, completion_callback_);
  entry2->Write(0, 0, write_buf.get(), kDataLen, completion_callback_);

  // Pull the plug
  disk_cache->Disable();
  FlushCacheTasks();

  // Ensure the directory can be deleted at this point.
  EXPECT_TRUE(base::DirectoryExists(directory_.GetPath()));
  EXPECT_FALSE(base::IsDirectoryEmpty(directory_.GetPath()));
  EXPECT_TRUE(base::DeleteFile(directory_.GetPath(), true));
  EXPECT_FALSE(base::DirectoryExists(directory_.GetPath()));

  disk_cache.reset(nullptr);

  // Also, new IO operations should fail immediately.
  EXPECT_EQ(
      net::ERR_ABORTED,
      entry1->Read(0, 0, read_buf.get(), kDataLen, completion_callback_));
  entry1->Close();
  entry2->Close();

  FlushCacheTasks();
}

TEST_F(AppCacheDiskCacheTest, CleanupCallback) {
  // Test that things delete fine when we disable the cache and wait for
  // the cleanup callback.

  net::TestClosure cleanup_done;
  net::TestCompletionCallback init_done;
  std::unique_ptr<AppCacheDiskCache> disk_cache(new AppCacheDiskCache);
  EXPECT_FALSE(disk_cache->is_disabled());
  disk_cache->InitWithDiskBackend(directory_.GetPath(), k10MBytes, false,
                                  cleanup_done.closure(), init_done.callback());
  EXPECT_EQ(net::OK, init_done.WaitForResult());

  disk_cache->Disable();
  cleanup_done.WaitForResult();

  // Ensure the directory can be deleted at this point.
  EXPECT_TRUE(base::DirectoryExists(directory_.GetPath()));
  EXPECT_FALSE(base::IsDirectoryEmpty(directory_.GetPath()));
  EXPECT_TRUE(base::DeleteFile(directory_.GetPath(), true));
  EXPECT_FALSE(base::DirectoryExists(directory_.GetPath()));
}

}  // namespace content
