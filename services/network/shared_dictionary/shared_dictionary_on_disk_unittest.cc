// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_on_disk.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/unguessable_token.h"
#include "net/base/hash_value.h"
#include "net/base/io_buffer.h"
#include "net/disk_cache/mock/mock_backend_impl.h"
#include "net/disk_cache/mock/mock_entry_impl.h"
#include "services/network/shared_dictionary/shared_dictionary_disk_cache.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

const std::string kTestData = "hello world";

class FakeSharedDictionaryDiskCache : public SharedDictionaryDiskCache {
 public:
  explicit FakeSharedDictionaryDiskCache() = default;
  ~FakeSharedDictionaryDiskCache() override = default;
  void Initialize() {
    SharedDictionaryDiskCache::Initialize(
        base::FilePath(),
#if BUILDFLAG(IS_ANDROID)
        disk_cache::ApplicationStatusListenerGetter(),
#endif  // BUILDFLAG(IS_ANDROID)
        /*file_operations_factory=*/nullptr);
  }
  disk_cache::BackendMock* backend() { return mock_cache_ptr_; }

 protected:
  disk_cache::BackendResult CreateCacheBackend(
      const base::FilePath& cache_directory_path,
#if BUILDFLAG(IS_ANDROID)
      disk_cache::ApplicationStatusListenerGetter app_status_listener_getter,
#endif  // BUILDFLAG(IS_ANDROID)
      scoped_refptr<disk_cache::BackendFileOperationsFactory>
          file_operations_factory,
      disk_cache::BackendResultCallback callback) override {
    auto mock_cache =
        std::make_unique<disk_cache::BackendMock>(net::CacheType::APP_CACHE);
    mock_cache_ptr_ = mock_cache.get();
    return disk_cache::BackendResult::Make(std::move(mock_cache));
  }

 private:
  raw_ptr<disk_cache::BackendMock> mock_cache_ptr_;
};

TEST(SharedDictionaryOnDiskTest, AsyncOpenEntryAsyncReadData) {
  size_t expected_size = kTestData.size();
  net::SHA256HashValue hash({{0x00, 0x01}});
  base::UnguessableToken disk_cache_key_token =
      base::UnguessableToken::Create();

  auto disk_cache = std::make_unique<FakeSharedDictionaryDiskCache>();
  disk_cache->Initialize();

  disk_cache::EntryResultCallback open_entry_callback;
  EXPECT_CALL(*disk_cache->backend(), OpenEntry)
      .WillOnce([&](const std::string& key, net::RequestPriority priority,
                    disk_cache::EntryResultCallback callback) {
        open_entry_callback = std::move(callback);
        return disk_cache::EntryResult::MakeError(net::ERR_IO_PENDING);
      });

  std::unique_ptr<disk_cache::EntryMock> entry =
      std::make_unique<disk_cache::EntryMock>();
  net::CompletionOnceCallback read_all_callback;
  scoped_refptr<net::IOBuffer> buffer;

  EXPECT_CALL(*entry, GetDataSize).WillOnce([&](int index) -> int32_t {
    return expected_size;
  });

  EXPECT_CALL(*entry, ReadData)
      .WillOnce([&](int index, int offset, net::IOBuffer* buf, int buf_len,
                    net::CompletionOnceCallback callback) -> int {
        EXPECT_EQ(1, index);
        EXPECT_EQ(0, offset);
        EXPECT_EQ(base::checked_cast<int>(expected_size), buf_len);
        buffer = buf;
        read_all_callback = std::move(callback);
        return net::ERR_IO_PENDING;
      });

  auto dictionary = base::MakeRefCounted<SharedDictionaryOnDisk>(
      expected_size, hash, /*id*/ "", disk_cache_key_token, *disk_cache,
      /*disk_cache_error_callback=*/base::BindOnce([]() {
        NOTREACHED_IN_MIGRATION();
      }),
      /*on_deleted_closure_runner=*/base::ScopedClosureRunner());
  EXPECT_EQ(expected_size, dictionary->size());
  EXPECT_EQ(hash, dictionary->hash());

  bool read_all_finished = false;
  EXPECT_EQ(net::ERR_IO_PENDING,
            dictionary->ReadAll(base::BindLambdaForTesting([&](int rv) {
              EXPECT_EQ(net::OK, rv);
              read_all_finished = true;
            })));

  ASSERT_TRUE(open_entry_callback);
  std::move(open_entry_callback)
      .Run(disk_cache::EntryResult::MakeOpened(entry.release()));
  ASSERT_TRUE(buffer);
  ASSERT_TRUE(read_all_callback);
  memcpy(buffer->data(), kTestData.c_str(), kTestData.size());
  std::move(read_all_callback).Run(base::checked_cast<int>(expected_size));
  EXPECT_TRUE(read_all_finished);
  EXPECT_EQ(kTestData,
            std::string(dictionary->data()->data(), dictionary->size()));

  // ReadAll() synchronously returns OK now.
  EXPECT_EQ(net::OK, dictionary->ReadAll(base::BindLambdaForTesting(
                         [&](int rv) { ASSERT_TRUE(false); })));
}

TEST(SharedDictionaryOnDiskTest, SyncOpenEntryAsyncReadData) {
  size_t expected_size = kTestData.size();
  net::SHA256HashValue hash({{0x00, 0x01}});
  base::UnguessableToken disk_cache_key_token =
      base::UnguessableToken::Create();

  auto disk_cache = std::make_unique<FakeSharedDictionaryDiskCache>();
  disk_cache->Initialize();

  std::unique_ptr<disk_cache::EntryMock> entry =
      std::make_unique<disk_cache::EntryMock>();
  net::CompletionOnceCallback read_all_callback;
  scoped_refptr<net::IOBuffer> buffer;

  EXPECT_CALL(*entry, GetDataSize).WillOnce([&](int index) -> int32_t {
    return expected_size;
  });

  EXPECT_CALL(*entry, ReadData)
      .WillOnce([&](int index, int offset, net::IOBuffer* buf, int buf_len,
                    net::CompletionOnceCallback callback) -> int {
        EXPECT_EQ(1, index);
        EXPECT_EQ(0, offset);
        EXPECT_EQ(base::checked_cast<int>(expected_size), buf_len);
        buffer = buf;
        read_all_callback = std::move(callback);
        return net::ERR_IO_PENDING;
      });

  EXPECT_CALL(*disk_cache->backend(), OpenEntry)
      .WillOnce([&](const std::string& key, net::RequestPriority priority,
                    disk_cache::EntryResultCallback callback) {
        return disk_cache::EntryResult::MakeOpened(entry.release());
      });

  auto dictionary = base::MakeRefCounted<SharedDictionaryOnDisk>(
      expected_size, hash, /*id=*/"", disk_cache_key_token, *disk_cache,
      /*disk_cache_error_callback=*/base::BindOnce([]() {
        NOTREACHED_IN_MIGRATION();
      }),
      /*on_deleted_closure_runner=*/base::ScopedClosureRunner());

  bool read_all_finished = false;
  EXPECT_EQ(net::ERR_IO_PENDING,
            dictionary->ReadAll(base::BindLambdaForTesting([&](int rv) {
              EXPECT_EQ(net::OK, rv);
              read_all_finished = true;
            })));

  ASSERT_TRUE(buffer);
  ASSERT_TRUE(read_all_callback);
  memcpy(buffer->data(), kTestData.c_str(), kTestData.size());
  std::move(read_all_callback).Run(base::checked_cast<int>(expected_size));
  EXPECT_TRUE(read_all_finished);
  EXPECT_EQ(kTestData,
            std::string(dictionary->data()->data(), dictionary->size()));

  // ReadAll() synchronously returns OK now.
  EXPECT_EQ(net::OK, dictionary->ReadAll(base::BindLambdaForTesting(
                         [&](int rv) { ASSERT_TRUE(false); })));
}

TEST(SharedDictionaryOnDiskTest, AsyncOpenEntrySyncReadData) {
  size_t expected_size = kTestData.size();
  net::SHA256HashValue hash({{0x00, 0x01}});
  base::UnguessableToken disk_cache_key_token =
      base::UnguessableToken::Create();

  auto disk_cache = std::make_unique<FakeSharedDictionaryDiskCache>();
  disk_cache->Initialize();

  disk_cache::EntryResultCallback open_entry_callback;
  EXPECT_CALL(*disk_cache->backend(), OpenEntry)
      .WillOnce([&](const std::string& key, net::RequestPriority priority,
                    disk_cache::EntryResultCallback callback) {
        open_entry_callback = std::move(callback);
        return disk_cache::EntryResult::MakeError(net::ERR_IO_PENDING);
      });

  std::unique_ptr<disk_cache::EntryMock> entry =
      std::make_unique<disk_cache::EntryMock>();

  EXPECT_CALL(*entry, GetDataSize).WillOnce([&](int index) -> int32_t {
    return expected_size;
  });

  EXPECT_CALL(*entry, ReadData)
      .WillOnce([&](int index, int offset, net::IOBuffer* buf, int buf_len,
                    net::CompletionOnceCallback callback) -> int {
        EXPECT_EQ(1, index);
        EXPECT_EQ(0, offset);
        EXPECT_EQ(base::checked_cast<int>(expected_size), buf_len);
        memcpy(buf->data(), kTestData.c_str(), kTestData.size());
        return base::checked_cast<int>(expected_size);
      });

  auto dictionary = base::MakeRefCounted<SharedDictionaryOnDisk>(
      expected_size, hash, /*id=*/"", disk_cache_key_token, *disk_cache,
      /*disk_cache_error_callback=*/base::BindOnce([]() {
        NOTREACHED_IN_MIGRATION();
      }),
      /*on_deleted_closure_runner=*/base::ScopedClosureRunner());

  bool read_all_finished = false;
  EXPECT_EQ(net::ERR_IO_PENDING,
            dictionary->ReadAll(base::BindLambdaForTesting([&](int rv) {
              EXPECT_EQ(net::OK, rv);
              read_all_finished = true;
            })));

  ASSERT_TRUE(open_entry_callback);
  std::move(open_entry_callback)
      .Run(disk_cache::EntryResult::MakeOpened(entry.release()));
  EXPECT_TRUE(read_all_finished);
  EXPECT_EQ(kTestData,
            std::string(dictionary->data()->data(), dictionary->size()));

  // ReadAll() synchronously returns OK now.
  EXPECT_EQ(net::OK, dictionary->ReadAll(base::BindLambdaForTesting(
                         [&](int rv) { ASSERT_TRUE(false); })));
}

TEST(SharedDictionaryOnDiskTest, SyncOpenEntrySyncReadData) {
  size_t expected_size = kTestData.size();
  net::SHA256HashValue hash({{0x00, 0x01}});
  base::UnguessableToken disk_cache_key_token =
      base::UnguessableToken::Create();

  auto disk_cache = std::make_unique<FakeSharedDictionaryDiskCache>();
  disk_cache->Initialize();

  std::unique_ptr<disk_cache::EntryMock> entry =
      std::make_unique<disk_cache::EntryMock>();

  EXPECT_CALL(*entry, GetDataSize).WillOnce([&](int index) -> int32_t {
    return expected_size;
  });

  EXPECT_CALL(*entry, ReadData)
      .WillOnce([&](int index, int offset, net::IOBuffer* buf, int buf_len,
                    net::CompletionOnceCallback callback) -> int {
        EXPECT_EQ(1, index);
        EXPECT_EQ(0, offset);
        EXPECT_EQ(base::checked_cast<int>(expected_size), buf_len);
        memcpy(buf->data(), kTestData.c_str(), kTestData.size());
        return base::checked_cast<int>(expected_size);
      });

  EXPECT_CALL(*disk_cache->backend(), OpenEntry)
      .WillOnce([&](const std::string& key, net::RequestPriority priority,
                    disk_cache::EntryResultCallback callback) {
        return disk_cache::EntryResult::MakeOpened(entry.release());
      });

  auto dictionary = base::MakeRefCounted<SharedDictionaryOnDisk>(
      expected_size, hash, /*id=*/"", disk_cache_key_token, *disk_cache,
      /*disk_cache_error_callback=*/base::BindOnce([]() {
        NOTREACHED_IN_MIGRATION();
      }),
      /*on_deleted_closure_runner=*/base::ScopedClosureRunner());

  // ReadAll() synchronously returns OK.
  EXPECT_EQ(net::OK, dictionary->ReadAll(base::BindLambdaForTesting(
                         [&](int rv) { ASSERT_TRUE(false); })));
  EXPECT_EQ(kTestData,
            std::string(dictionary->data()->data(), dictionary->size()));
}

TEST(SharedDictionaryOnDiskTest, AsyncOpenEntryFailure) {
  size_t expected_size = kTestData.size();
  net::SHA256HashValue hash({{0x00, 0x01}});
  base::UnguessableToken disk_cache_key_token =
      base::UnguessableToken::Create();

  auto disk_cache = std::make_unique<FakeSharedDictionaryDiskCache>();
  disk_cache->Initialize();

  disk_cache::EntryResultCallback open_entry_callback;
  EXPECT_CALL(*disk_cache->backend(), OpenEntry)
      .WillOnce([&](const std::string& key, net::RequestPriority priority,
                    disk_cache::EntryResultCallback callback) {
        open_entry_callback = std::move(callback);
        return disk_cache::EntryResult::MakeError(net::ERR_IO_PENDING);
      });

  bool disk_cache_error_callback_called = false;
  auto dictionary = base::MakeRefCounted<SharedDictionaryOnDisk>(
      expected_size, hash, /*id=*/"", disk_cache_key_token, *disk_cache,
      /*disk_cache_error_callback=*/base::BindLambdaForTesting([&]() {
        disk_cache_error_callback_called = true;
      }),
      /*on_deleted_closure_runner=*/base::ScopedClosureRunner());
  bool read_all_finished = false;
  EXPECT_EQ(net::ERR_IO_PENDING,
            dictionary->ReadAll(base::BindLambdaForTesting([&](int rv) {
              EXPECT_EQ(net::ERR_FAILED, rv);
              EXPECT_TRUE(disk_cache_error_callback_called);
              read_all_finished = true;
            })));

  ASSERT_TRUE(open_entry_callback);
  std::move(open_entry_callback)
      .Run(disk_cache::EntryResult::MakeError(net::ERR_FAILED));
  EXPECT_TRUE(read_all_finished);

  // ReadAll() synchronously returns ERR_FAILED now.
  EXPECT_EQ(net::ERR_FAILED, dictionary->ReadAll(base::BindLambdaForTesting(
                                 [&](int rv) { ASSERT_TRUE(false); })));
}

TEST(SharedDictionaryOnDiskTest, SyncOpenEntryFailure) {
  size_t expected_size = kTestData.size();
  net::SHA256HashValue hash({{0x00, 0x01}});
  base::UnguessableToken disk_cache_key_token =
      base::UnguessableToken::Create();

  auto disk_cache = std::make_unique<FakeSharedDictionaryDiskCache>();
  disk_cache->Initialize();

  EXPECT_CALL(*disk_cache->backend(), OpenEntry)
      .WillOnce([&](const std::string& key, net::RequestPriority priority,
                    disk_cache::EntryResultCallback callback) {
        return disk_cache::EntryResult::MakeError(net::ERR_FAILED);
      });

  bool disk_cache_error_callback_called = false;
  auto dictionary = base::MakeRefCounted<SharedDictionaryOnDisk>(
      expected_size, hash, /*id*/ "", disk_cache_key_token, *disk_cache,
      /*disk_cache_error_callback=*/base::BindLambdaForTesting([&]() {
        disk_cache_error_callback_called = true;
      }),
      /*on_deleted_closure_runner=*/base::ScopedClosureRunner());

  EXPECT_EQ(net::ERR_FAILED, dictionary->ReadAll(base::BindLambdaForTesting(
                                 [&](int rv) { ASSERT_TRUE(false); })));
  EXPECT_TRUE(disk_cache_error_callback_called);
}

TEST(SharedDictionaryOnDiskTest, AsyncOpenEntryAsyncReadDataFailure) {
  size_t expected_size = kTestData.size();
  net::SHA256HashValue hash({{0x00, 0x01}});
  base::UnguessableToken disk_cache_key_token =
      base::UnguessableToken::Create();

  auto disk_cache = std::make_unique<FakeSharedDictionaryDiskCache>();
  disk_cache->Initialize();

  disk_cache::EntryResultCallback open_entry_callback;
  EXPECT_CALL(*disk_cache->backend(), OpenEntry)
      .WillOnce([&](const std::string& key, net::RequestPriority priority,
                    disk_cache::EntryResultCallback callback) {
        open_entry_callback = std::move(callback);
        return disk_cache::EntryResult::MakeError(net::ERR_IO_PENDING);
      });

  std::unique_ptr<disk_cache::EntryMock> entry =
      std::make_unique<disk_cache::EntryMock>();
  net::CompletionOnceCallback read_all_callback;

  EXPECT_CALL(*entry, GetDataSize).WillOnce([&](int index) -> int32_t {
    return expected_size;
  });

  EXPECT_CALL(*entry, ReadData)
      .WillOnce([&](int index, int offset, net::IOBuffer* buf, int buf_len,
                    net::CompletionOnceCallback callback) -> int {
        EXPECT_EQ(1, index);
        EXPECT_EQ(0, offset);
        EXPECT_EQ(base::checked_cast<int>(expected_size), buf_len);
        read_all_callback = std::move(callback);
        return net::ERR_IO_PENDING;
      });

  bool disk_cache_error_callback_called = false;
  auto dictionary = base::MakeRefCounted<SharedDictionaryOnDisk>(
      expected_size, hash, /*id=*/"", disk_cache_key_token, *disk_cache,
      /*disk_cache_error_callback=*/base::BindLambdaForTesting([&]() {
        disk_cache_error_callback_called = true;
      }),
      /*on_deleted_closure_runner=*/base::ScopedClosureRunner());

  bool read_all_finished = false;
  EXPECT_EQ(net::ERR_IO_PENDING,
            dictionary->ReadAll(base::BindLambdaForTesting([&](int rv) {
              EXPECT_EQ(net::ERR_FAILED, rv);
              EXPECT_TRUE(disk_cache_error_callback_called);
              read_all_finished = true;
            })));

  ASSERT_TRUE(open_entry_callback);
  std::move(open_entry_callback)
      .Run(disk_cache::EntryResult::MakeOpened(entry.release()));
  ASSERT_TRUE(read_all_callback);
  std::move(read_all_callback).Run(net::ERR_FAILED);
  EXPECT_TRUE(read_all_finished);

  // ReadAll() synchronously returns ERR_FAILED now.
  EXPECT_EQ(net::ERR_FAILED, dictionary->ReadAll(base::BindLambdaForTesting(
                                 [&](int rv) { ASSERT_TRUE(false); })));
}

TEST(SharedDictionaryOnDiskTest, AsyncOpenEntrySyncReadDataFailure) {
  size_t expected_size = kTestData.size();
  net::SHA256HashValue hash({{0x00, 0x01}});
  base::UnguessableToken disk_cache_key_token =
      base::UnguessableToken::Create();

  auto disk_cache = std::make_unique<FakeSharedDictionaryDiskCache>();
  disk_cache->Initialize();

  disk_cache::EntryResultCallback open_entry_callback;
  EXPECT_CALL(*disk_cache->backend(), OpenEntry)
      .WillOnce([&](const std::string& key, net::RequestPriority priority,
                    disk_cache::EntryResultCallback callback) {
        open_entry_callback = std::move(callback);
        return disk_cache::EntryResult::MakeError(net::ERR_IO_PENDING);
      });

  std::unique_ptr<disk_cache::EntryMock> entry =
      std::make_unique<disk_cache::EntryMock>();

  EXPECT_CALL(*entry, GetDataSize).WillOnce([&](int index) -> int32_t {
    return expected_size;
  });

  EXPECT_CALL(*entry, ReadData)
      .WillOnce([&](int index, int offset, net::IOBuffer* buf, int buf_len,
                    net::CompletionOnceCallback callback) -> int {
        EXPECT_EQ(1, index);
        EXPECT_EQ(0, offset);
        EXPECT_EQ(base::checked_cast<int>(expected_size), buf_len);
        return net::ERR_FAILED;
      });

  bool disk_cache_error_callback_called = false;
  auto dictionary = base::MakeRefCounted<SharedDictionaryOnDisk>(
      expected_size, hash, /*id=*/"", disk_cache_key_token, *disk_cache,
      /*disk_cache_error_callback=*/base::BindLambdaForTesting([&]() {
        disk_cache_error_callback_called = true;
      }),
      /*on_deleted_closure_runner=*/base::ScopedClosureRunner());

  bool read_all_finished = false;
  EXPECT_EQ(net::ERR_IO_PENDING,
            dictionary->ReadAll(base::BindLambdaForTesting([&](int rv) {
              EXPECT_EQ(net::ERR_FAILED, rv);
              EXPECT_TRUE(disk_cache_error_callback_called);
              read_all_finished = true;
            })));

  ASSERT_TRUE(open_entry_callback);
  std::move(open_entry_callback)
      .Run(disk_cache::EntryResult::MakeOpened(entry.release()));
  EXPECT_TRUE(read_all_finished);

  // ReadAll() synchronously returns ERR_FAILED now.
  EXPECT_EQ(net::ERR_FAILED, dictionary->ReadAll(base::BindLambdaForTesting(
                                 [&](int rv) { ASSERT_TRUE(false); })));
}
TEST(SharedDictionaryOnDiskTest, SyncOpenEntryAsyncReadDataFailure) {
  size_t expected_size = kTestData.size();
  net::SHA256HashValue hash({{0x00, 0x01}});
  base::UnguessableToken disk_cache_key_token =
      base::UnguessableToken::Create();

  auto disk_cache = std::make_unique<FakeSharedDictionaryDiskCache>();
  disk_cache->Initialize();

  std::unique_ptr<disk_cache::EntryMock> entry =
      std::make_unique<disk_cache::EntryMock>();
  net::CompletionOnceCallback read_all_callback;

  EXPECT_CALL(*entry, GetDataSize).WillOnce([&](int index) -> int32_t {
    return expected_size;
  });

  EXPECT_CALL(*entry, ReadData)
      .WillOnce([&](int index, int offset, net::IOBuffer* buf, int buf_len,
                    net::CompletionOnceCallback callback) -> int {
        EXPECT_EQ(1, index);
        EXPECT_EQ(0, offset);
        EXPECT_EQ(base::checked_cast<int>(expected_size), buf_len);
        read_all_callback = std::move(callback);
        return net::ERR_IO_PENDING;
      });

  EXPECT_CALL(*disk_cache->backend(), OpenEntry)
      .WillOnce([&](const std::string& key, net::RequestPriority priority,
                    disk_cache::EntryResultCallback callback) {
        return disk_cache::EntryResult::MakeOpened(entry.release());
      });

  bool disk_cache_error_callback_called = false;
  auto dictionary = base::MakeRefCounted<SharedDictionaryOnDisk>(
      expected_size, hash, /*id=*/"", disk_cache_key_token, *disk_cache,
      /*disk_cache_error_callback=*/base::BindLambdaForTesting([&]() {
        disk_cache_error_callback_called = true;
      }),
      /*on_deleted_closure_runner=*/base::ScopedClosureRunner());

  bool read_all_finished = false;
  EXPECT_EQ(net::ERR_IO_PENDING,
            dictionary->ReadAll(base::BindLambdaForTesting([&](int rv) {
              EXPECT_EQ(net::ERR_FAILED, rv);
              EXPECT_TRUE(disk_cache_error_callback_called);
              read_all_finished = true;
            })));

  ASSERT_TRUE(read_all_callback);
  std::move(read_all_callback).Run(net::ERR_FAILED);
  EXPECT_TRUE(read_all_finished);

  // ReadAll() synchronously returns ERR_FAILED now.
  EXPECT_EQ(net::ERR_FAILED, dictionary->ReadAll(base::BindLambdaForTesting(
                                 [&](int rv) { ASSERT_TRUE(false); })));
}

TEST(SharedDictionaryOnDiskTest, SyncOpenEntrySyncReadDataFailure) {
  size_t expected_size = kTestData.size();
  net::SHA256HashValue hash({{0x00, 0x01}});
  base::UnguessableToken disk_cache_key_token =
      base::UnguessableToken::Create();

  auto disk_cache = std::make_unique<FakeSharedDictionaryDiskCache>();
  disk_cache->Initialize();

  std::unique_ptr<disk_cache::EntryMock> entry =
      std::make_unique<disk_cache::EntryMock>();

  EXPECT_CALL(*entry, GetDataSize).WillOnce([&](int index) -> int32_t {
    return expected_size;
  });

  EXPECT_CALL(*entry, ReadData)
      .WillOnce([&](int index, int offset, net::IOBuffer* buf, int buf_len,
                    net::CompletionOnceCallback callback) -> int {
        EXPECT_EQ(1, index);
        EXPECT_EQ(0, offset);
        EXPECT_EQ(base::checked_cast<int>(expected_size), buf_len);
        return net::ERR_FAILED;
      });

  EXPECT_CALL(*disk_cache->backend(), OpenEntry)
      .WillOnce([&](const std::string& key, net::RequestPriority priority,
                    disk_cache::EntryResultCallback callback) {
        return disk_cache::EntryResult::MakeOpened(entry.release());
      });

  bool disk_cache_error_callback_called = false;
  auto dictionary = base::MakeRefCounted<SharedDictionaryOnDisk>(
      expected_size, hash, /*id=*/"", disk_cache_key_token, *disk_cache,
      /*disk_cache_error_callback=*/base::BindLambdaForTesting([&]() {
        disk_cache_error_callback_called = true;
      }),
      /*on_deleted_closure_runner=*/base::ScopedClosureRunner());

  // ReadAll() synchronously returns ERR_FAILED.
  EXPECT_EQ(net::ERR_FAILED, dictionary->ReadAll(base::BindLambdaForTesting(
                                 [&](int rv) { ASSERT_TRUE(false); })));
  EXPECT_TRUE(disk_cache_error_callback_called);
}

TEST(SharedDictionaryOnDiskTest, UnexpectedDataSize) {
  size_t expected_size = kTestData.size();
  net::SHA256HashValue hash({{0x00, 0x01}});
  base::UnguessableToken disk_cache_key_token =
      base::UnguessableToken::Create();

  auto disk_cache = std::make_unique<FakeSharedDictionaryDiskCache>();
  disk_cache->Initialize();

  disk_cache::EntryResultCallback open_entry_callback;
  EXPECT_CALL(*disk_cache->backend(), OpenEntry)
      .WillOnce([&](const std::string& key, net::RequestPriority priority,
                    disk_cache::EntryResultCallback callback) {
        open_entry_callback = std::move(callback);
        return disk_cache::EntryResult::MakeError(net::ERR_IO_PENDING);
      });

  std::unique_ptr<disk_cache::EntryMock> entry =
      std::make_unique<disk_cache::EntryMock>();

  EXPECT_CALL(*entry, GetDataSize).WillOnce([&](int index) -> int32_t {
    return expected_size + 1;  // Returns wrong size.
  });

  bool disk_cache_error_callback_called = false;
  auto dictionary = base::MakeRefCounted<SharedDictionaryOnDisk>(
      expected_size, hash, /*id=*/"", disk_cache_key_token, *disk_cache,
      /*disk_cache_error_callback=*/base::BindLambdaForTesting([&]() {
        disk_cache_error_callback_called = true;
      }),
      /*on_deleted_closure_runner=*/base::ScopedClosureRunner());

  bool read_all_finished = false;
  EXPECT_EQ(net::ERR_IO_PENDING,
            dictionary->ReadAll(base::BindLambdaForTesting([&](int rv) {
              EXPECT_EQ(net::ERR_FAILED, rv);
              EXPECT_TRUE(disk_cache_error_callback_called);
              read_all_finished = true;
            })));

  ASSERT_TRUE(open_entry_callback);
  std::move(open_entry_callback)
      .Run(disk_cache::EntryResult::MakeOpened(entry.release()));
  EXPECT_TRUE(read_all_finished);
}

}  // namespace

}  // namespace network
