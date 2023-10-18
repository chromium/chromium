// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_writer_on_disk.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/test_file_util.h"
#include "build/build_config.h"
#include "crypto/secure_hash.h"
#include "net/base/hash_value.h"
#include "net/base/io_buffer.h"
#include "net/base/test_completion_callback.h"
#include "net/disk_cache/disk_cache_test_util.h"
#include "net/disk_cache/mock/mock_backend_impl.h"
#include "net/disk_cache/mock/mock_entry_impl.h"
#include "services/network/shared_dictionary/shared_dictionary_constants.h"
#include "services/network/shared_dictionary/shared_dictionary_disk_cache.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

const std::string kTestData = "Hello world";
const std::string kTestData1 = "Hello ";
const std::string kTestData2 = "world";

enum class CreateBackendResultType {
  kSyncSuccess,
  kSyncFailure,
  kAsyncSuccess,
  kAsyncFailure,
};

net::SHA256HashValue GetHash(const std::string& data) {
  std::unique_ptr<crypto::SecureHash> secure_hash =
      crypto::SecureHash::Create(crypto::SecureHash::SHA256);
  secure_hash->Update(data.c_str(), data.size());
  net::SHA256HashValue sha256;
  secure_hash->Finish(sha256.data, sizeof(sha256.data));
  return sha256;
}

class FakeSharedDictionaryDiskCache : public SharedDictionaryDiskCache {
 public:
  explicit FakeSharedDictionaryDiskCache(
      CreateBackendResultType create_backend_result_type)
      : create_backend_result_type_(create_backend_result_type) {}
  ~FakeSharedDictionaryDiskCache() override = default;
  void Initialize() {
    SharedDictionaryDiskCache::Initialize(
        base::FilePath(),
#if BUILDFLAG(IS_ANDROID)
        disk_cache::ApplicationStatusListenerGetter(),
#endif  // BUILDFLAG(IS_ANDROID)
        /*file_operations_factory=*/nullptr);
  }

  void RunCreateCacheBackendCallback() {
    CHECK(backend_result_callback_);
    switch (create_backend_result_type_) {
      case CreateBackendResultType::kSyncSuccess:
        EXPECT_TRUE(false);
        return;
      case CreateBackendResultType::kSyncFailure:
        EXPECT_TRUE(false);
        return;
      case CreateBackendResultType::kAsyncSuccess:
        CHECK(mock_cache_);
        CHECK(mock_cache_ptr_);
        std::move(backend_result_callback_)
            .Run(disk_cache::BackendResult::Make(std::move(mock_cache_)));
        return;
      case CreateBackendResultType::kAsyncFailure:
        std::move(backend_result_callback_)
            .Run(disk_cache::BackendResult::MakeError(net::ERR_FAILED));
        return;
    }
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
    switch (create_backend_result_type_) {
      case CreateBackendResultType::kSyncSuccess:
        mock_cache_ = std::make_unique<disk_cache::BackendMock>(
            net::CacheType::APP_CACHE);
        mock_cache_ptr_ = mock_cache_.get();
        return disk_cache::BackendResult::Make(std::move(mock_cache_));
      case CreateBackendResultType::kSyncFailure:
        return disk_cache::BackendResult::MakeError(net::ERR_FAILED);
      case CreateBackendResultType::kAsyncSuccess:
        mock_cache_ = std::make_unique<disk_cache::BackendMock>(
            net::CacheType::APP_CACHE);
        mock_cache_ptr_ = mock_cache_.get();
        backend_result_callback_ = std::move(callback);
        return disk_cache::BackendResult::MakeError(net::ERR_IO_PENDING);
      case CreateBackendResultType::kAsyncFailure:
        backend_result_callback_ = std::move(callback);
        return disk_cache::BackendResult::MakeError(net::ERR_IO_PENDING);
    }
  }

 private:
  const CreateBackendResultType create_backend_result_type_;
  std::unique_ptr<disk_cache::BackendMock> mock_cache_;
  raw_ptr<disk_cache::BackendMock> mock_cache_ptr_;
  disk_cache::BackendResultCallback backend_result_callback_;
};

class SharedDictionaryWriterOnDiskTest : public testing::Test {
 public:
  SharedDictionaryWriterOnDiskTest() = default;

 protected:
  void RunSimpleWriteTest(bool sync_create_backend,
                          bool sync_create_entry,
                          bool sync_write_data);
  void RunCreateBackendFailureTest(bool sync_create_backend);
  void RunCreateEntryFailureTest(bool sync_create_backend,
                                 bool sync_create_entry);
  void RunWriteDataFailureTest(bool sync_create_backend,
                               bool sync_create_entry,
                               bool sync_write_data);
};

void SharedDictionaryWriterOnDiskTest::RunSimpleWriteTest(
    bool sync_create_backend,
    bool sync_create_entry,
    bool sync_write_data) {
  auto disk_cache = std::make_unique<FakeSharedDictionaryDiskCache>(
      sync_create_backend ? CreateBackendResultType::kSyncSuccess
                          : CreateBackendResultType::kAsyncSuccess);
  disk_cache->Initialize();

  std::unique_ptr<disk_cache::EntryMock> entry =
      std::make_unique<disk_cache::EntryMock>();

  net::CompletionOnceCallback write_data_callback;
  EXPECT_CALL(*entry, WriteData)
      .WillOnce([&](int index, int offset, net::IOBuffer* buf, int buf_len,
                    net::CompletionOnceCallback callback,
                    bool truncate) -> int {
        EXPECT_EQ(1, index);
        EXPECT_EQ(0, offset);
        EXPECT_EQ(base::checked_cast<int>(kTestData.size()), buf_len);
        EXPECT_EQ(
            kTestData,
            std::string(reinterpret_cast<const char*>(buf->data()), buf_len));
        if (sync_write_data) {
          return base::checked_cast<int>(kTestData.size());
        }
        write_data_callback = std::move(callback);
        return net::ERR_IO_PENDING;
      });

  base::UnguessableToken cache_key_token = base::UnguessableToken::Create();
  disk_cache::EntryResultCallback create_entry_callback;

  EXPECT_CALL(*disk_cache->backend(), CreateEntry)
      .WillOnce([&](const std::string& key, net::RequestPriority priority,
                    disk_cache::EntryResultCallback callback) {
        EXPECT_EQ(cache_key_token.ToString(), key);
        if (sync_create_entry) {
          return disk_cache::EntryResult::MakeCreated(entry.release());
        }
        create_entry_callback = std::move(callback);
        return disk_cache::EntryResult::MakeError(net::ERR_IO_PENDING);
      });
  bool finish_callback_called = false;
  scoped_refptr<SharedDictionaryWriterOnDisk> writer =
      base::MakeRefCounted<SharedDictionaryWriterOnDisk>(
          cache_key_token,
          base::BindLambdaForTesting(
              [&](SharedDictionaryWriterOnDisk::Result result, size_t size,
                  const net::SHA256HashValue& hash) {
                EXPECT_EQ(SharedDictionaryWriterOnDisk::Result::kSuccess,
                          result);
                EXPECT_EQ(kTestData.size(), size);
                EXPECT_EQ(GetHash(kTestData), hash);
                finish_callback_called = true;
              }),
          disk_cache->GetWeakPtr());
  writer->Initialize();
  writer->Append(kTestData.c_str(), kTestData.size());
  writer->Finish();

  if (!sync_create_backend) {
    disk_cache->RunCreateCacheBackendCallback();
  }
  if (!sync_create_entry) {
    ASSERT_TRUE(create_entry_callback);
    std::move(create_entry_callback)
        .Run(disk_cache::EntryResult::MakeCreated(entry.release()));
  }
  if (!sync_write_data) {
    ASSERT_TRUE(write_data_callback);
    std::move(write_data_callback)
        .Run(base::checked_cast<int>(kTestData.size()));
  }

  EXPECT_TRUE(finish_callback_called);

  writer.reset();
}

TEST_F(SharedDictionaryWriterOnDiskTest, SimpleWrite) {
  for (bool sync_create_backend : {false, true}) {
    for (bool sync_create_entry : {false, true}) {
      for (bool sync_write_data : {false, true}) {
        SCOPED_TRACE(
            base::StringPrintf("sync_create_backend: %s, sync_create_entry: "
                               "%s, sync_write_data: %s",
                               (sync_create_backend ? "true" : "false"),
                               (sync_create_entry ? "true" : "false"),
                               (sync_write_data ? "true" : "false")));

        RunSimpleWriteTest(sync_create_backend, sync_create_entry,
                           sync_write_data);
      }
    }
  }
}

void SharedDictionaryWriterOnDiskTest::RunCreateBackendFailureTest(
    bool sync_create_backend) {
  auto disk_cache = std::make_unique<FakeSharedDictionaryDiskCache>(
      sync_create_backend ? CreateBackendResultType::kSyncFailure
                          : CreateBackendResultType::kAsyncFailure);
  disk_cache->Initialize();

  bool finish_callback_called = false;
  scoped_refptr<SharedDictionaryWriterOnDisk> writer =
      base::MakeRefCounted<SharedDictionaryWriterOnDisk>(
          base::UnguessableToken::Create(),
          base::BindLambdaForTesting([&](SharedDictionaryWriterOnDisk::Result
                                             result,
                                         size_t size,
                                         const net::SHA256HashValue& hash) {
            EXPECT_EQ(
                SharedDictionaryWriterOnDisk::Result::kErrorCreateEntryFailed,
                result);
            finish_callback_called = true;
          }),
          disk_cache->GetWeakPtr());

  writer->Initialize();
  writer->Append(kTestData.c_str(), kTestData.size());
  writer->Finish();

  if (!sync_create_backend) {
    disk_cache->RunCreateCacheBackendCallback();
  }

  EXPECT_TRUE(finish_callback_called);

  writer.reset();
}

TEST_F(SharedDictionaryWriterOnDiskTest, CreateBackendFailure) {
  for (bool sync_create_backend : {false, true}) {
    SCOPED_TRACE(base::StringPrintf("sync_create_backend: %s",
                                    (sync_create_backend ? "true" : "false")));
    RunCreateBackendFailureTest(sync_create_backend);
  }
}

void SharedDictionaryWriterOnDiskTest::RunCreateEntryFailureTest(
    bool sync_create_backend,
    bool sync_create_entry) {
  auto disk_cache = std::make_unique<FakeSharedDictionaryDiskCache>(
      sync_create_backend ? CreateBackendResultType::kSyncSuccess
                          : CreateBackendResultType::kAsyncSuccess);
  disk_cache->Initialize();

  disk_cache::EntryResultCallback create_entry_callback;
  EXPECT_CALL(*disk_cache->backend(), CreateEntry)
      .WillOnce([&](const std::string& key, net::RequestPriority priority,
                    disk_cache::EntryResultCallback callback) {
        if (sync_create_entry) {
          return disk_cache::EntryResult::MakeError(net::ERR_FAILED);
        }
        create_entry_callback = std::move(callback);
        return disk_cache::EntryResult::MakeError(net::ERR_IO_PENDING);
      });

  bool finish_callback_called = false;
  scoped_refptr<SharedDictionaryWriterOnDisk> writer =
      base::MakeRefCounted<SharedDictionaryWriterOnDisk>(
          base::UnguessableToken::Create(),
          base::BindLambdaForTesting([&](SharedDictionaryWriterOnDisk::Result
                                             result,
                                         size_t size,
                                         const net::SHA256HashValue& hash) {
            EXPECT_EQ(
                SharedDictionaryWriterOnDisk::Result::kErrorCreateEntryFailed,
                result);
            finish_callback_called = true;
          }),
          disk_cache->GetWeakPtr());

  writer->Initialize();
  writer->Append(kTestData.c_str(), kTestData.size());
  writer->Finish();

  if (!sync_create_backend) {
    disk_cache->RunCreateCacheBackendCallback();
  }
  if (!sync_create_entry) {
    ASSERT_TRUE(create_entry_callback);
    std::move(create_entry_callback)
        .Run(disk_cache::EntryResult::MakeError(net::ERR_FAILED));
  }
  EXPECT_TRUE(finish_callback_called);
  writer.reset();
}

TEST_F(SharedDictionaryWriterOnDiskTest, CreateEntryFailure) {
  for (bool sync_create_backend : {false, true}) {
    for (bool sync_create_entry : {false, true}) {
      SCOPED_TRACE(
          base::StringPrintf("sync_create_backend: %s, sync_create_entry: %s",
                             (sync_create_backend ? "true" : "false"),
                             (sync_create_entry ? "true" : "false")));
      RunCreateEntryFailureTest(sync_create_backend, sync_create_entry);
    }
  }
}

void SharedDictionaryWriterOnDiskTest::RunWriteDataFailureTest(
    bool sync_create_backend,
    bool sync_create_entry,
    bool sync_write_data) {
  auto disk_cache = std::make_unique<FakeSharedDictionaryDiskCache>(
      sync_create_backend ? CreateBackendResultType::kSyncSuccess
                          : CreateBackendResultType::kAsyncSuccess);
  disk_cache->Initialize();

  std::unique_ptr<disk_cache::EntryMock> entry =
      std::make_unique<disk_cache::EntryMock>();

  net::CompletionOnceCallback write_data_callback;
  EXPECT_CALL(*entry, WriteData)
      .WillOnce([&](int index, int offset, net::IOBuffer* buf, int buf_len,
                    net::CompletionOnceCallback callback,
                    bool truncate) -> int {
        EXPECT_EQ(1, index);
        EXPECT_EQ(0, offset);
        EXPECT_EQ(base::checked_cast<int>(kTestData.size()), buf_len);
        EXPECT_EQ(
            kTestData,
            std::string(reinterpret_cast<const char*>(buf->data()), buf_len));
        if (sync_write_data) {
          return net::ERR_FAILED;
        }
        write_data_callback = std::move(callback);
        return net::ERR_IO_PENDING;
      });
  bool entry_doomed = false;
  EXPECT_CALL(*entry, Doom).WillOnce([&]() { entry_doomed = true; });

  disk_cache::EntryResultCallback create_entry_callback;
  EXPECT_CALL(*disk_cache->backend(), CreateEntry)
      .WillOnce([&](const std::string& key, net::RequestPriority priority,
                    disk_cache::EntryResultCallback callback) {
        if (sync_create_entry) {
          return disk_cache::EntryResult::MakeCreated(entry.release());
        }
        create_entry_callback = std::move(callback);
        return disk_cache::EntryResult::MakeError(net::ERR_IO_PENDING);
      });

  bool finish_callback_called = false;
  scoped_refptr<SharedDictionaryWriterOnDisk> writer =
      base::MakeRefCounted<SharedDictionaryWriterOnDisk>(
          base::UnguessableToken::Create(),
          base::BindLambdaForTesting(
              [&](SharedDictionaryWriterOnDisk::Result result, size_t size,
                  const net::SHA256HashValue& hash) {
                EXPECT_EQ(
                    SharedDictionaryWriterOnDisk::Result::kErrorWriteDataFailed,
                    result);
                finish_callback_called = true;
              }),
          disk_cache->GetWeakPtr());
  writer->Initialize();
  writer->Append(kTestData.c_str(), kTestData.size());
  writer->Finish();

  if (!sync_create_backend) {
    disk_cache->RunCreateCacheBackendCallback();
  }
  if (!sync_create_entry) {
    ASSERT_TRUE(create_entry_callback);
    std::move(create_entry_callback)
        .Run(disk_cache::EntryResult::MakeCreated(entry.release()));
  }
  if (!sync_write_data) {
    ASSERT_TRUE(write_data_callback);
    std::move(write_data_callback).Run(net::ERR_FAILED);
  }

  EXPECT_TRUE(finish_callback_called);

  EXPECT_TRUE(entry_doomed);

  writer.reset();
}

TEST_F(SharedDictionaryWriterOnDiskTest, WriteDataFailure) {
  for (bool sync_create_backend : {false, true}) {
    for (bool sync_create_entry : {false, true}) {
      for (bool sync_write_data : {false, true}) {
        SCOPED_TRACE(
            base::StringPrintf("sync_create_backend: %s, sync_create_entry: "
                               "%s, sync_write_data: %s",
                               (sync_create_backend ? "true" : "false"),
                               (sync_create_entry ? "true" : "false"),
                               (sync_write_data ? "true" : "false")));

        RunWriteDataFailureTest(sync_create_backend, sync_create_entry,
                                sync_write_data);
      }
    }
  }
}

TEST_F(SharedDictionaryWriterOnDiskTest, MultipleWrite) {
  auto disk_cache = std::make_unique<FakeSharedDictionaryDiskCache>(
      CreateBackendResultType::kAsyncSuccess);
  disk_cache->Initialize();

  std::unique_ptr<disk_cache::EntryMock> entry =
      std::make_unique<disk_cache::EntryMock>();

  net::CompletionOnceCallback write_data_callback1;
  net::CompletionOnceCallback write_data_callback2;
  EXPECT_CALL(*entry, WriteData)
      .WillOnce([&](int index, int offset, net::IOBuffer* buf, int buf_len,
                    net::CompletionOnceCallback callback,
                    bool truncate) -> int {
        EXPECT_EQ(1, index);
        EXPECT_EQ(0, offset);
        EXPECT_EQ(base::checked_cast<int>(kTestData1.size()), buf_len);
        EXPECT_EQ(
            kTestData1,
            std::string(reinterpret_cast<const char*>(buf->data()), buf_len));
        write_data_callback1 = std::move(callback);
        return net::ERR_IO_PENDING;
      })
      .WillOnce([&](int index, int offset, net::IOBuffer* buf, int buf_len,
                    net::CompletionOnceCallback callback,
                    bool truncate) -> int {
        EXPECT_EQ(1, index);
        EXPECT_EQ(base::checked_cast<int>(kTestData1.size()), offset);
        EXPECT_EQ(base::checked_cast<int>(kTestData2.size()), buf_len);
        EXPECT_EQ(
            kTestData2,
            std::string(reinterpret_cast<const char*>(buf->data()), buf_len));
        write_data_callback2 = std::move(callback);
        return net::ERR_IO_PENDING;
      });

  base::UnguessableToken cache_key_token = base::UnguessableToken::Create();
  disk_cache::EntryResultCallback create_entry_callback;
  EXPECT_CALL(*disk_cache->backend(), CreateEntry)
      .WillOnce([&](const std::string& key, net::RequestPriority priority,
                    disk_cache::EntryResultCallback callback) {
        EXPECT_EQ(cache_key_token.ToString(), key);
        create_entry_callback = std::move(callback);
        return disk_cache::EntryResult::MakeError(net::ERR_IO_PENDING);
      });

  bool finish_callback_called = false;
  scoped_refptr<SharedDictionaryWriterOnDisk> writer =
      base::MakeRefCounted<SharedDictionaryWriterOnDisk>(
          cache_key_token,
          base::BindLambdaForTesting(
              [&](SharedDictionaryWriterOnDisk::Result result, size_t size,
                  const net::SHA256HashValue& hash) {
                EXPECT_EQ(SharedDictionaryWriterOnDisk::Result::kSuccess,
                          result);
                EXPECT_EQ(kTestData1.size() + kTestData2.size(), size);
                EXPECT_EQ(GetHash(kTestData1 + kTestData2), hash);
                finish_callback_called = true;
              }),
          disk_cache->GetWeakPtr());
  writer->Initialize();
  writer->Append(kTestData1.c_str(), kTestData1.size());
  writer->Append(kTestData2.c_str(), kTestData2.size());
  writer->Finish();

  disk_cache->RunCreateCacheBackendCallback();
  std::move(create_entry_callback)
      .Run(disk_cache::EntryResult::MakeCreated(entry.release()));
  std::move(write_data_callback1)
      .Run(base::checked_cast<int>(kTestData1.size()));
  EXPECT_FALSE(finish_callback_called);
  std::move(write_data_callback2)
      .Run(base::checked_cast<int>(kTestData2.size()));
  EXPECT_TRUE(finish_callback_called);

  writer.reset();
}

TEST_F(SharedDictionaryWriterOnDiskTest, MultipleWriteSyncWrite) {
  auto disk_cache = std::make_unique<FakeSharedDictionaryDiskCache>(
      CreateBackendResultType::kAsyncSuccess);
  disk_cache->Initialize();

  std::unique_ptr<disk_cache::EntryMock> entry =
      std::make_unique<disk_cache::EntryMock>();

  EXPECT_CALL(*entry, WriteData)
      .WillOnce([&](int index, int offset, net::IOBuffer* buf, int buf_len,
                    net::CompletionOnceCallback callback,
                    bool truncate) -> int {
        EXPECT_EQ(1, index);
        EXPECT_EQ(0, offset);
        EXPECT_EQ(base::checked_cast<int>(kTestData1.size()), buf_len);
        EXPECT_EQ(
            kTestData1,
            std::string(reinterpret_cast<const char*>(buf->data()), buf_len));
        return base::checked_cast<int>(kTestData1.size());
      })
      .WillOnce([&](int index, int offset, net::IOBuffer* buf, int buf_len,
                    net::CompletionOnceCallback callback,
                    bool truncate) -> int {
        EXPECT_EQ(1, index);
        EXPECT_EQ(base::checked_cast<int>(kTestData1.size()), offset);
        EXPECT_EQ(base::checked_cast<int>(kTestData2.size()), buf_len);
        EXPECT_EQ(
            kTestData2,
            std::string(reinterpret_cast<const char*>(buf->data()), buf_len));
        return base::checked_cast<int>(kTestData2.size());
      });

  base::UnguessableToken cache_key_token = base::UnguessableToken::Create();
  disk_cache::EntryResultCallback create_entry_callback;
  EXPECT_CALL(*disk_cache->backend(), CreateEntry)
      .WillOnce([&](const std::string& key, net::RequestPriority priority,
                    disk_cache::EntryResultCallback callback) {
        EXPECT_EQ(cache_key_token.ToString(), key);
        create_entry_callback = std::move(callback);
        return disk_cache::EntryResult::MakeError(net::ERR_IO_PENDING);
      });

  bool finish_callback_called = false;
  scoped_refptr<SharedDictionaryWriterOnDisk> writer =
      base::MakeRefCounted<SharedDictionaryWriterOnDisk>(
          cache_key_token,
          base::BindLambdaForTesting(
              [&](SharedDictionaryWriterOnDisk::Result result, size_t size,
                  const net::SHA256HashValue& hash) {
                EXPECT_EQ(SharedDictionaryWriterOnDisk::Result::kSuccess,
                          result);
                EXPECT_EQ(kTestData1.size() + kTestData2.size(), size);
                EXPECT_EQ(GetHash(kTestData1 + kTestData2), hash);
                finish_callback_called = true;
              }),
          disk_cache->GetWeakPtr());
  writer->Initialize();
  writer->Append(kTestData1.c_str(), kTestData1.size());
  writer->Append(kTestData2.c_str(), kTestData2.size());
  writer->Finish();

  disk_cache->RunCreateCacheBackendCallback();
  std::move(create_entry_callback)
      .Run(disk_cache::EntryResult::MakeCreated(entry.release()));
  EXPECT_TRUE(finish_callback_called);

  writer.reset();
}

TEST_F(SharedDictionaryWriterOnDiskTest, AsyncWriteFailureOnMultipleWrites) {
  auto disk_cache = std::make_unique<FakeSharedDictionaryDiskCache>(
      CreateBackendResultType::kAsyncSuccess);
  disk_cache->Initialize();

  std::unique_ptr<disk_cache::EntryMock> entry =
      std::make_unique<disk_cache::EntryMock>();

  net::CompletionOnceCallback write_data_callback1;
  net::CompletionOnceCallback write_data_callback2;
  EXPECT_CALL(*entry, WriteData)
      .WillOnce([&](int index, int offset, net::IOBuffer* buf, int buf_len,
                    net::CompletionOnceCallback callback,
                    bool truncate) -> int {
        EXPECT_EQ(1, index);
        EXPECT_EQ(0, offset);
        EXPECT_EQ(base::checked_cast<int>(kTestData1.size()), buf_len);
        EXPECT_EQ(
            kTestData1,
            std::string(reinterpret_cast<const char*>(buf->data()), buf_len));
        write_data_callback1 = std::move(callback);
        return net::ERR_IO_PENDING;
      })
      .WillOnce([&](int index, int offset, net::IOBuffer* buf, int buf_len,
                    net::CompletionOnceCallback callback,
                    bool truncate) -> int {
        EXPECT_EQ(1, index);
        EXPECT_EQ(base::checked_cast<int>(kTestData1.size()), offset);
        EXPECT_EQ(base::checked_cast<int>(kTestData2.size()), buf_len);
        EXPECT_EQ(
            kTestData2,
            std::string(reinterpret_cast<const char*>(buf->data()), buf_len));
        write_data_callback2 = std::move(callback);
        return net::ERR_IO_PENDING;
      });

  bool entry_doomed = false;
  EXPECT_CALL(*entry, Doom).WillOnce([&]() { entry_doomed = true; });

  disk_cache::EntryResultCallback create_entry_callback;
  EXPECT_CALL(*disk_cache->backend(), CreateEntry)
      .WillOnce([&](const std::string& key, net::RequestPriority priority,
                    disk_cache::EntryResultCallback callback) {
        create_entry_callback = std::move(callback);
        return disk_cache::EntryResult::MakeError(net::ERR_IO_PENDING);
      });

  bool finish_callback_called = false;
  scoped_refptr<SharedDictionaryWriterOnDisk> writer =
      base::MakeRefCounted<SharedDictionaryWriterOnDisk>(
          base::UnguessableToken::Create(),
          base::BindLambdaForTesting(
              [&](SharedDictionaryWriterOnDisk::Result result, size_t size,
                  const net::SHA256HashValue& hash) {
                EXPECT_EQ(
                    SharedDictionaryWriterOnDisk::Result::kErrorWriteDataFailed,
                    result);
                finish_callback_called = true;
              }),
          disk_cache->GetWeakPtr());
  writer->Initialize();
  writer->Append(kTestData1.c_str(), kTestData1.size());
  writer->Append(kTestData2.c_str(), kTestData2.size());
  writer->Finish();

  disk_cache->RunCreateCacheBackendCallback();
  std::move(create_entry_callback)
      .Run(disk_cache::EntryResult::MakeCreated(entry.release()));
  std::move(write_data_callback1).Run(net::ERR_FAILED);

  EXPECT_TRUE(finish_callback_called);

  EXPECT_TRUE(entry_doomed);

  std::move(write_data_callback2).Run(net::ERR_FAILED);
  writer.reset();
}

TEST_F(SharedDictionaryWriterOnDiskTest, SyncWriteFailureOnMultipleWrites) {
  auto disk_cache = std::make_unique<FakeSharedDictionaryDiskCache>(
      CreateBackendResultType::kAsyncSuccess);
  disk_cache->Initialize();

  std::unique_ptr<disk_cache::EntryMock> entry =
      std::make_unique<disk_cache::EntryMock>();

  EXPECT_CALL(*entry, WriteData)
      .WillOnce([&](int index, int offset, net::IOBuffer* buf, int buf_len,
                    net::CompletionOnceCallback callback,
                    bool truncate) -> int {
        EXPECT_EQ(1, index);
        EXPECT_EQ(0, offset);
        EXPECT_EQ(base::checked_cast<int>(kTestData1.size()), buf_len);
        EXPECT_EQ(
            kTestData1,
            std::string(reinterpret_cast<const char*>(buf->data()), buf_len));
        return net::ERR_FAILED;
      });

  bool entry_doomed = false;
  EXPECT_CALL(*entry, Doom).WillOnce([&]() { entry_doomed = true; });

  disk_cache::EntryResultCallback create_entry_callback;
  EXPECT_CALL(*disk_cache->backend(), CreateEntry)
      .WillOnce([&](const std::string& key, net::RequestPriority priority,
                    disk_cache::EntryResultCallback callback) {
        create_entry_callback = std::move(callback);
        return disk_cache::EntryResult::MakeError(net::ERR_IO_PENDING);
      });

  bool finish_callback_called = false;
  scoped_refptr<SharedDictionaryWriterOnDisk> writer =
      base::MakeRefCounted<SharedDictionaryWriterOnDisk>(
          base::UnguessableToken::Create(),
          base::BindLambdaForTesting(
              [&](SharedDictionaryWriterOnDisk::Result result, size_t size,
                  const net::SHA256HashValue& hash) {
                EXPECT_EQ(
                    SharedDictionaryWriterOnDisk::Result::kErrorWriteDataFailed,
                    result);
                finish_callback_called = true;
              }),
          disk_cache->GetWeakPtr());
  writer->Initialize();
  writer->Append(kTestData1.c_str(), kTestData1.size());
  writer->Append(kTestData2.c_str(), kTestData2.size());
  writer->Finish();

  disk_cache->RunCreateCacheBackendCallback();
  std::move(create_entry_callback)
      .Run(disk_cache::EntryResult::MakeCreated(entry.release()));

  EXPECT_TRUE(finish_callback_called);
  EXPECT_TRUE(entry_doomed);
  writer.reset();
}

TEST_F(SharedDictionaryWriterOnDiskTest, AbortedWithoutWrite) {
  auto disk_cache = std::make_unique<FakeSharedDictionaryDiskCache>(
      CreateBackendResultType::kAsyncSuccess);
  disk_cache->Initialize();

  std::unique_ptr<disk_cache::EntryMock> entry =
      std::make_unique<disk_cache::EntryMock>();

  bool entry_doomed = false;
  EXPECT_CALL(*entry, Doom).WillOnce([&]() { entry_doomed = true; });

  disk_cache::EntryResultCallback create_entry_callback;
  EXPECT_CALL(*disk_cache->backend(), CreateEntry)
      .WillOnce([&](const std::string& key, net::RequestPriority priority,
                    disk_cache::EntryResultCallback callback) {
        create_entry_callback = std::move(callback);
        return disk_cache::EntryResult::MakeError(net::ERR_IO_PENDING);
      });

  bool finish_callback_called = false;
  scoped_refptr<SharedDictionaryWriterOnDisk> writer =
      base::MakeRefCounted<SharedDictionaryWriterOnDisk>(
          base::UnguessableToken::Create(),
          base::BindLambdaForTesting(
              [&](SharedDictionaryWriterOnDisk::Result result, size_t size,
                  const net::SHA256HashValue& hash) {
                EXPECT_EQ(SharedDictionaryWriterOnDisk::Result::kErrorAborted,
                          result);
                finish_callback_called = true;
              }),
          disk_cache->GetWeakPtr());
  writer->Initialize();

  disk_cache->RunCreateCacheBackendCallback();
  std::move(create_entry_callback)
      .Run(disk_cache::EntryResult::MakeCreated(entry.release()));

  writer.reset();

  EXPECT_TRUE(entry_doomed);
  EXPECT_TRUE(finish_callback_called);
}

TEST_F(SharedDictionaryWriterOnDiskTest, AbortedAfterWrite) {
  auto disk_cache = std::make_unique<FakeSharedDictionaryDiskCache>(
      CreateBackendResultType::kAsyncSuccess);
  disk_cache->Initialize();

  std::unique_ptr<disk_cache::EntryMock> entry =
      std::make_unique<disk_cache::EntryMock>();

  net::CompletionOnceCallback write_data_callback;
  EXPECT_CALL(*entry, WriteData)
      .WillOnce([&](int index, int offset, net::IOBuffer* buf, int buf_len,
                    net::CompletionOnceCallback callback,
                    bool truncate) -> int {
        write_data_callback = std::move(callback);
        return net::ERR_IO_PENDING;
      });

  bool entry_doomed = false;
  EXPECT_CALL(*entry, Doom).WillOnce([&]() { entry_doomed = true; });

  disk_cache::EntryResultCallback create_entry_callback;
  EXPECT_CALL(*disk_cache->backend(), CreateEntry)
      .WillOnce([&](const std::string& key, net::RequestPriority priority,
                    disk_cache::EntryResultCallback callback) {
        create_entry_callback = std::move(callback);
        return disk_cache::EntryResult::MakeError(net::ERR_IO_PENDING);
      });

  bool finish_callback_called = false;
  scoped_refptr<SharedDictionaryWriterOnDisk> writer =
      base::MakeRefCounted<SharedDictionaryWriterOnDisk>(
          base::UnguessableToken::Create(),
          base::BindLambdaForTesting(
              [&](SharedDictionaryWriterOnDisk::Result result, size_t size,
                  const net::SHA256HashValue& hash) {
                EXPECT_EQ(SharedDictionaryWriterOnDisk::Result::kErrorAborted,
                          result);
                finish_callback_called = true;
              }),
          disk_cache->GetWeakPtr());
  writer->Initialize();
  writer->Append(kTestData.c_str(), kTestData.size());

  disk_cache->RunCreateCacheBackendCallback();
  std::move(create_entry_callback)
      .Run(disk_cache::EntryResult::MakeCreated(entry.release()));
  std::move(write_data_callback).Run(base::checked_cast<int>(kTestData.size()));

  writer.reset();

  EXPECT_TRUE(entry_doomed);
  EXPECT_TRUE(finish_callback_called);
}

TEST_F(SharedDictionaryWriterOnDiskTest, ErrorSizeZero) {
  auto disk_cache = std::make_unique<FakeSharedDictionaryDiskCache>(
      CreateBackendResultType::kAsyncSuccess);
  disk_cache->Initialize();

  std::unique_ptr<disk_cache::EntryMock> entry =
      std::make_unique<disk_cache::EntryMock>();

  bool entry_doomed = false;
  EXPECT_CALL(*entry, Doom).WillOnce([&]() { entry_doomed = true; });

  disk_cache::EntryResultCallback create_entry_callback;
  EXPECT_CALL(*disk_cache->backend(), CreateEntry)
      .WillOnce([&](const std::string& key, net::RequestPriority priority,
                    disk_cache::EntryResultCallback callback) {
        create_entry_callback = std::move(callback);
        return disk_cache::EntryResult::MakeError(net::ERR_IO_PENDING);
      });

  bool finish_callback_called = false;
  scoped_refptr<SharedDictionaryWriterOnDisk> writer =
      base::MakeRefCounted<SharedDictionaryWriterOnDisk>(
          base::UnguessableToken::Create(),
          base::BindLambdaForTesting(
              [&](SharedDictionaryWriterOnDisk::Result result, size_t size,
                  const net::SHA256HashValue& hash) {
                EXPECT_EQ(SharedDictionaryWriterOnDisk::Result::kErrorSizeZero,
                          result);
                finish_callback_called = true;
              }),
          disk_cache->GetWeakPtr());
  writer->Initialize();
  writer->Finish();

  disk_cache->RunCreateCacheBackendCallback();
  std::move(create_entry_callback)
      .Run(disk_cache::EntryResult::MakeCreated(entry.release()));

  writer.reset();

  EXPECT_TRUE(entry_doomed);
  EXPECT_TRUE(finish_callback_called);
}

TEST_F(SharedDictionaryWriterOnDiskTest, ErrorSizeExceedsLimitBeforeOnEntry) {
  base::ScopedClosureRunner size_limit_resetter =
      shared_dictionary::SetDictionarySizeLimitForTesting(kTestData1.size());

  auto disk_cache = std::make_unique<FakeSharedDictionaryDiskCache>(
      CreateBackendResultType::kAsyncSuccess);
  disk_cache->Initialize();

  std::unique_ptr<disk_cache::EntryMock> entry =
      std::make_unique<disk_cache::EntryMock>();

  base::UnguessableToken cache_key_token = base::UnguessableToken::Create();
  disk_cache::EntryResultCallback create_entry_callback;
  EXPECT_CALL(*disk_cache->backend(), CreateEntry)
      .WillOnce([&](const std::string& key, net::RequestPriority priority,
                    disk_cache::EntryResultCallback callback) {
        EXPECT_EQ(cache_key_token.ToString(), key);
        create_entry_callback = std::move(callback);
        return disk_cache::EntryResult::MakeError(net::ERR_IO_PENDING);
      });

  bool finish_callback_called = false;
  scoped_refptr<SharedDictionaryWriterOnDisk> writer =
      base::MakeRefCounted<SharedDictionaryWriterOnDisk>(
          cache_key_token,
          base::BindLambdaForTesting([&](SharedDictionaryWriterOnDisk::Result
                                             result,
                                         size_t size,
                                         const net::SHA256HashValue& hash) {
            EXPECT_EQ(
                SharedDictionaryWriterOnDisk::Result::kErrorSizeExceedsLimit,
                result);
            finish_callback_called = true;
          }),
          disk_cache->GetWeakPtr());
  writer->Initialize();
  writer->Append(kTestData1.c_str(), kTestData1.size());
  EXPECT_FALSE(finish_callback_called);
  writer->Append("x", 1);
  EXPECT_TRUE(finish_callback_called);
  writer->Append(kTestData2.c_str(), kTestData2.size());
  writer->Finish();

  disk_cache->RunCreateCacheBackendCallback();
  // This will call SharedDictionaryWriterOnDisk::OnEntry().
  std::move(create_entry_callback)
      .Run(disk_cache::EntryResult::MakeCreated(entry.release()));

  writer.reset();
}

TEST_F(SharedDictionaryWriterOnDiskTest, ErrorSizeExceedsLimitAfterOnEntry) {
  base::ScopedClosureRunner size_limit_resetter =
      shared_dictionary::SetDictionarySizeLimitForTesting(kTestData1.size());

  auto disk_cache = std::make_unique<FakeSharedDictionaryDiskCache>(
      CreateBackendResultType::kAsyncSuccess);
  disk_cache->Initialize();

  std::unique_ptr<disk_cache::EntryMock> entry =
      std::make_unique<disk_cache::EntryMock>();

  net::CompletionOnceCallback write_data_callback;
  EXPECT_CALL(*entry, WriteData)
      .WillOnce([&](int index, int offset, net::IOBuffer* buf, int buf_len,
                    net::CompletionOnceCallback callback,
                    bool truncate) -> int {
        EXPECT_EQ(1, index);
        EXPECT_EQ(0, offset);
        EXPECT_EQ(base::checked_cast<int>(kTestData1.size()), buf_len);
        EXPECT_EQ(
            kTestData1,
            std::string(reinterpret_cast<const char*>(buf->data()), buf_len));
        write_data_callback = std::move(callback);
        return net::ERR_IO_PENDING;
      });
  bool entry_doom_called = false;
  EXPECT_CALL(*entry, Doom).WillOnce([&]() { entry_doom_called = true; });

  base::UnguessableToken cache_key_token = base::UnguessableToken::Create();
  disk_cache::EntryResultCallback create_entry_callback;
  EXPECT_CALL(*disk_cache->backend(), CreateEntry)
      .WillOnce([&](const std::string& key, net::RequestPriority priority,
                    disk_cache::EntryResultCallback callback) {
        EXPECT_EQ(cache_key_token.ToString(), key);
        create_entry_callback = std::move(callback);
        return disk_cache::EntryResult::MakeError(net::ERR_IO_PENDING);
      });

  bool finish_callback_called = false;
  scoped_refptr<SharedDictionaryWriterOnDisk> writer =
      base::MakeRefCounted<SharedDictionaryWriterOnDisk>(
          cache_key_token,
          base::BindLambdaForTesting([&](SharedDictionaryWriterOnDisk::Result
                                             result,
                                         size_t size,
                                         const net::SHA256HashValue& hash) {
            EXPECT_EQ(
                SharedDictionaryWriterOnDisk::Result::kErrorSizeExceedsLimit,
                result);
            finish_callback_called = true;
          }),
          disk_cache->GetWeakPtr());
  writer->Initialize();

  disk_cache->RunCreateCacheBackendCallback();
  std::move(create_entry_callback)
      .Run(disk_cache::EntryResult::MakeCreated(entry.release()));

  writer->Append(kTestData1.c_str(), kTestData1.size());

  std::move(write_data_callback)
      .Run(base::checked_cast<int>(kTestData1.size()));

  EXPECT_FALSE(finish_callback_called);
  EXPECT_FALSE(entry_doom_called);
  writer->Append("x", 1);
  EXPECT_TRUE(finish_callback_called);
  EXPECT_TRUE(entry_doom_called);

  // Test that calling Append() and Finish() doesn't cause unexpected crash.
  writer->Append(kTestData2.c_str(), kTestData2.size());
  writer->Finish();
}

}  // namespace

}  // namespace network
