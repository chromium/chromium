// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_disk_cache.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/numerics/safe_conversions.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_file_util.h"
#include "build/build_config.h"
#include "net/base/io_buffer.h"
#include "net/base/test_completion_callback.h"
#include "net/disk_cache/disk_cache_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {
const std::string kTestKey = "test";
const std::string kTestData = "Hello world";
}  // namespace

class SharedDictionaryDiskCacheTest : public testing::Test {
 public:
  SharedDictionaryDiskCacheTest() = default;

  void SetUp() override {
    ASSERT_TRUE(tmp_directory_.CreateUniqueTempDir());
    directory_path_ =
        tmp_directory_.GetPath().Append(FILE_PATH_LITERAL("dictionaries"));
  }

  void TearDown() override { FlushCacheTasks(); }

 protected:
  std::unique_ptr<SharedDictionaryDiskCache> CreateDiskCache() {
    auto disk_cache = std::make_unique<SharedDictionaryDiskCache>();
    disk_cache->Initialize(directory_path_,
#if BUILDFLAG(IS_ANDROID)
                           disk_cache::ApplicationStatusListenerGetter(),
#endif  // BUILDFLAG(IS_ANDROID)
                           /*file_operations_factory=*/nullptr);
    return disk_cache;
  }

  void FlushCacheTasks() {
    disk_cache::FlushCacheThreadForTesting();
    task_environment_.RunUntilIdle();
  }

  disk_cache::EntryResult CreateEntry(SharedDictionaryDiskCache* disk_cache,
                                      const std::string& key) {
    TestEntryResultCompletionCallback create_callback;
    return create_callback.GetResult(disk_cache->OpenOrCreateEntry(
        key, /*create=*/true, create_callback.callback()));
  }

  disk_cache::EntryResult OpenEntry(SharedDictionaryDiskCache* disk_cache,
                                    const std::string& key) {
    TestEntryResultCompletionCallback create_callback;
    return create_callback.GetResult(disk_cache->OpenOrCreateEntry(
        key, /*create=*/false, create_callback.callback()));
  }

  int DoomEntry(SharedDictionaryDiskCache* disk_cache, const std::string& key) {
    net::TestCompletionCallback doom_entry_callback;
    return doom_entry_callback.GetResult(
        disk_cache->DoomEntry(key, doom_entry_callback.callback()));
  }

  int ClearAll(SharedDictionaryDiskCache* disk_cache) {
    net::TestCompletionCallback clear_all_callback;
    return clear_all_callback.GetResult(
        disk_cache->ClearAll(clear_all_callback.callback()));
  }

  int WriteData(disk_cache::Entry* entry, const std::string& data) {
    scoped_refptr<net::StringIOBuffer> write_buffer =
        base::MakeRefCounted<net::StringIOBuffer>(data);
    net::TestCompletionCallback write_callback;
    return write_callback.GetResult(entry->WriteData(
        /*index=*/1, /*offset=*/0, write_buffer.get(), write_buffer->size(),
        write_callback.callback(), /*truncate=*/false));
  }

  void CheckEntryDataEquals(disk_cache::Entry* entry,
                            const std::string& expected_data) {
    EXPECT_EQ(base::checked_cast<int32_t>(expected_data.size()),
              entry->GetDataSize(/*index=*/1));

    scoped_refptr<net::IOBufferWithSize> read_buffer =
        base::MakeRefCounted<net::IOBufferWithSize>(expected_data.size());
    net::TestCompletionCallback read_callback;
    EXPECT_EQ(read_buffer->size(),
              read_callback.GetResult(entry->ReadData(
                  /*index=*/1, /*offset=*/0, read_buffer.get(),
                  expected_data.size(), read_callback.callback())));
    EXPECT_EQ(expected_data,
              std::string(reinterpret_cast<const char*>(read_buffer->data()),
                          read_buffer->size()));
  }

  void PrepareDiskCacheWithTestData() {
    std::unique_ptr<SharedDictionaryDiskCache> disk_cache = CreateDiskCache();
    // Create an entry.
    disk_cache::EntryResult create_result =
        CreateEntry(disk_cache.get(), kTestKey);
    EXPECT_EQ(net::OK, create_result.net_error());
    disk_cache::ScopedEntryPtr created_entry;
    created_entry.reset(create_result.ReleaseEntry());
    ASSERT_TRUE(created_entry);

    // Write to the entry.
    EXPECT_EQ(base::checked_cast<int>(kTestData.size()),
              WriteData(created_entry.get(), kTestData));
  }

  void CorruptDiskCache() {
    PrepareDiskCacheWithTestData();

    // Corrupt the fake index file for the populated simple cache.
    const base::FilePath index_file_path =
        directory_path_.Append(FILE_PATH_LITERAL("index"));
    ASSERT_TRUE(base::WriteFile(index_file_path, "corrupted"));
    file_permissions_restorer_ = std::make_unique<base::FilePermissionRestorer>(
        tmp_directory_.GetPath());
    // Mark the parent directory unwritable, so that we can't restore the dist
    ASSERT_TRUE(base::MakeFileUnwritable(tmp_directory_.GetPath()));
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir tmp_directory_;
  base::FilePath directory_path_;
  // `file_permissions_restorer_` must be below `tmp_directory_` to restore the
  // file permission correctly.
  std::unique_ptr<base::FilePermissionRestorer> file_permissions_restorer_;
};

TEST_F(SharedDictionaryDiskCacheTest, CreateEntry) {
  std::unique_ptr<SharedDictionaryDiskCache> disk_cache = CreateDiskCache();
  FlushCacheTasks();
  EXPECT_EQ(net::OK, CreateEntry(disk_cache.get(), kTestKey).net_error());
}

TEST_F(SharedDictionaryDiskCacheTest, CreateEntryWhileInitializing) {
  std::unique_ptr<SharedDictionaryDiskCache> disk_cache = CreateDiskCache();
  EXPECT_EQ(net::OK, CreateEntry(disk_cache.get(), kTestKey).net_error());
}

TEST_F(SharedDictionaryDiskCacheTest, OpenNonExistentEntryFailure) {
  std::unique_ptr<SharedDictionaryDiskCache> disk_cache = CreateDiskCache();
  FlushCacheTasks();
  EXPECT_EQ(net::ERR_FAILED, OpenEntry(disk_cache.get(), kTestKey).net_error());
}

TEST_F(SharedDictionaryDiskCacheTest,
       OpenNonExistentEntryFailureWhileInitializing) {
  std::unique_ptr<SharedDictionaryDiskCache> disk_cache = CreateDiskCache();
  EXPECT_EQ(net::ERR_FAILED, OpenEntry(disk_cache.get(), kTestKey).net_error());
}

TEST_F(SharedDictionaryDiskCacheTest, OpenEntrySuccess) {
  {
    std::unique_ptr<SharedDictionaryDiskCache> disk_cache = CreateDiskCache();
    EXPECT_EQ(net::OK, CreateEntry(disk_cache.get(), kTestKey).net_error());
  }
  {
    std::unique_ptr<SharedDictionaryDiskCache> disk_cache = CreateDiskCache();
    EXPECT_EQ(net::OK, OpenEntry(disk_cache.get(), kTestKey).net_error());
  }
}

TEST_F(SharedDictionaryDiskCacheTest, DoomEntry) {
  std::unique_ptr<SharedDictionaryDiskCache> disk_cache = CreateDiskCache();
  FlushCacheTasks();
  EXPECT_EQ(net::OK, DoomEntry(disk_cache.get(), kTestKey));
}

TEST_F(SharedDictionaryDiskCacheTest, DoomEntryWhileInitializing) {
  std::unique_ptr<SharedDictionaryDiskCache> disk_cache = CreateDiskCache();
  EXPECT_EQ(net::OK, DoomEntry(disk_cache.get(), kTestKey));
}

TEST_F(SharedDictionaryDiskCacheTest, ClearAll) {
  PrepareDiskCacheWithTestData();

  std::unique_ptr<SharedDictionaryDiskCache> disk_cache = CreateDiskCache();
  FlushCacheTasks();
  EXPECT_EQ(net::OK, ClearAll(disk_cache.get()));

  EXPECT_EQ(net::ERR_FAILED, OpenEntry(disk_cache.get(), kTestKey).net_error());
}

TEST_F(SharedDictionaryDiskCacheTest, ClearAllWhileInitializing) {
  PrepareDiskCacheWithTestData();

  std::unique_ptr<SharedDictionaryDiskCache> disk_cache = CreateDiskCache();
  EXPECT_EQ(net::OK, ClearAll(disk_cache.get()));

  EXPECT_EQ(net::ERR_FAILED, OpenEntry(disk_cache.get(), kTestKey).net_error());
}

TEST_F(SharedDictionaryDiskCacheTest, CreateIterator) {
  PrepareDiskCacheWithTestData();

  std::unique_ptr<SharedDictionaryDiskCache> disk_cache = CreateDiskCache();
  FlushCacheTasks();

  std::unique_ptr<disk_cache::Backend::Iterator> iterator;
  disk_cache->CreateIterator(base::BindLambdaForTesting(
      [&](std::unique_ptr<disk_cache::Backend::Iterator> it) {
        iterator = std::move(it);
      }));
  EXPECT_TRUE(iterator);
}

TEST_F(SharedDictionaryDiskCacheTest, CreateIteratorWhileInitializing) {
  PrepareDiskCacheWithTestData();

  std::unique_ptr<SharedDictionaryDiskCache> disk_cache = CreateDiskCache();
  std::unique_ptr<disk_cache::Backend::Iterator> iterator;
  disk_cache->CreateIterator(base::BindLambdaForTesting(
      [&](std::unique_ptr<disk_cache::Backend::Iterator> it) {
        iterator = std::move(it);
      }));
  EXPECT_FALSE(iterator);
  FlushCacheTasks();
  EXPECT_TRUE(iterator);
}

TEST_F(SharedDictionaryDiskCacheTest, CreateWriteOpenReadDeleteReopen) {
  std::unique_ptr<SharedDictionaryDiskCache> disk_cache = CreateDiskCache();

  // Create an entry.
  disk_cache::EntryResult create_result =
      CreateEntry(disk_cache.get(), kTestKey);
  EXPECT_EQ(net::OK, create_result.net_error());
  disk_cache::ScopedEntryPtr created_entry;
  created_entry.reset(create_result.ReleaseEntry());
  ASSERT_TRUE(created_entry);

  // Write to the entry.
  EXPECT_EQ(base::checked_cast<int>(kTestData.size()),
            WriteData(created_entry.get(), kTestData));

  // Close the written entry.
  created_entry.reset();

  // Open the entry.
  disk_cache::EntryResult open_result = OpenEntry(disk_cache.get(), kTestKey);
  EXPECT_EQ(net::OK, open_result.net_error());
  disk_cache::ScopedEntryPtr opened_entry;
  opened_entry.reset(open_result.ReleaseEntry());
  ASSERT_TRUE(opened_entry);

  // Read the entry.
  CheckEntryDataEquals(opened_entry.get(), kTestData);

  // Close the opened entry.
  opened_entry.reset();

  // Doom the entry.
  net::TestCompletionCallback doom_callback;
  EXPECT_EQ(net::OK, doom_callback.GetResult(disk_cache->DoomEntry(
                         kTestKey, doom_callback.callback())));

  // Reopen the entry.
  disk_cache::EntryResult reopen_result = OpenEntry(disk_cache.get(), kTestKey);
  EXPECT_EQ(net::ERR_FAILED, reopen_result.net_error());
}

#if !BUILDFLAG(IS_FUCHSIA)
// CorruptDiskCache() doesn't work on Fuchsia. So disabling the following tests
// on Fuchsia.
TEST_F(SharedDictionaryDiskCacheTest, CreateEntryCorruptedFailure) {
  CorruptDiskCache();
  std::unique_ptr<SharedDictionaryDiskCache> disk_cache = CreateDiskCache();
  FlushCacheTasks();
  EXPECT_EQ(net::ERR_FAILED,
            CreateEntry(disk_cache.get(), kTestKey).net_error());
}

TEST_F(SharedDictionaryDiskCacheTest,
       CreateEntryWhileInitializingCorruptedFailure) {
  CorruptDiskCache();
  std::unique_ptr<SharedDictionaryDiskCache> disk_cache = CreateDiskCache();
  EXPECT_EQ(net::ERR_FAILED,
            CreateEntry(disk_cache.get(), kTestKey).net_error());
}

TEST_F(SharedDictionaryDiskCacheTest, OpenEntryCorruptedFailure) {
  CorruptDiskCache();

  std::unique_ptr<SharedDictionaryDiskCache> disk_cache = CreateDiskCache();
  FlushCacheTasks();
  EXPECT_EQ(net::ERR_FAILED, OpenEntry(disk_cache.get(), kTestKey).net_error());
}

TEST_F(SharedDictionaryDiskCacheTest,
       OpenEntryCorruptedFailureWhileInitializing) {
  CorruptDiskCache();

  std::unique_ptr<SharedDictionaryDiskCache> disk_cache = CreateDiskCache();
  EXPECT_EQ(net::ERR_FAILED, OpenEntry(disk_cache.get(), kTestKey).net_error());
}

TEST_F(SharedDictionaryDiskCacheTest, DoomEntryCorruptedFailure) {
  CorruptDiskCache();

  std::unique_ptr<SharedDictionaryDiskCache> disk_cache = CreateDiskCache();
  FlushCacheTasks();
  EXPECT_EQ(net::ERR_FAILED, DoomEntry(disk_cache.get(), kTestKey));
}

TEST_F(SharedDictionaryDiskCacheTest,
       DoomEntryCorruptedFailureWhileInitializing) {
  CorruptDiskCache();

  std::unique_ptr<SharedDictionaryDiskCache> disk_cache = CreateDiskCache();
  EXPECT_EQ(net::ERR_FAILED, DoomEntry(disk_cache.get(), kTestKey));
}

TEST_F(SharedDictionaryDiskCacheTest, ClearAllCorruptedFailure) {
  CorruptDiskCache();

  std::unique_ptr<SharedDictionaryDiskCache> disk_cache = CreateDiskCache();
  FlushCacheTasks();
  EXPECT_EQ(net::ERR_FAILED, ClearAll(disk_cache.get()));
}

TEST_F(SharedDictionaryDiskCacheTest,
       ClearAllCorruptedFailureWhileInitializing) {
  CorruptDiskCache();

  std::unique_ptr<SharedDictionaryDiskCache> disk_cache = CreateDiskCache();
  EXPECT_EQ(net::ERR_FAILED, ClearAll(disk_cache.get()));
}

TEST_F(SharedDictionaryDiskCacheTest, CreateIteratorCorruptedFailure) {
  CorruptDiskCache();

  std::unique_ptr<SharedDictionaryDiskCache> disk_cache = CreateDiskCache();
  FlushCacheTasks();

  bool callback_called = false;
  disk_cache->CreateIterator(base::BindLambdaForTesting(
      [&](std::unique_ptr<disk_cache::Backend::Iterator> it) {
        ASSERT_FALSE(it);
        callback_called = true;
      }));
  EXPECT_TRUE(callback_called);
}

TEST_F(SharedDictionaryDiskCacheTest,
       CreateIteratorCorruptedFailureWhileInitializing) {
  CorruptDiskCache();

  std::unique_ptr<SharedDictionaryDiskCache> disk_cache = CreateDiskCache();

  bool callback_called = false;
  disk_cache->CreateIterator(base::BindLambdaForTesting(
      [&](std::unique_ptr<disk_cache::Backend::Iterator> it) {
        ASSERT_FALSE(it);
        callback_called = true;
      }));
  EXPECT_FALSE(callback_called);
  FlushCacheTasks();
  EXPECT_TRUE(callback_called);
}

TEST_F(SharedDictionaryDiskCacheTest, DeletedWhileRuningDidCreateBackend) {
  // Corrupt the disk cache so that the callback is called synchronously.
  CorruptDiskCache();

  std::unique_ptr<SharedDictionaryDiskCache> disk_cache = CreateDiskCache();

  // Test that UAF doesn't happen when `disk_cache` is synchronously deleted in
  // the callback.
  disk_cache->DoomEntry(
      kTestKey, base::BindLambdaForTesting([&](int) { disk_cache.reset(); }));
  disk_cache->DoomEntry(kTestKey,
                        base::BindOnce([](int) { ASSERT_TRUE(false); }));
  FlushCacheTasks();
}

#endif  // !BUILDFLAG(IS_FUCHSIA)

}  // namespace network
