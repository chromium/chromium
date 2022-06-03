// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/dawn_caching_interface.h"

#include <string>
#include <tuple>
#include <unordered_map>

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "net/base/io_buffer.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/mock/mock_backend_impl.h"
#include "net/disk_cache/mock/mock_entry_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace gpu::webgpu {
namespace {

using ::testing::_;
using ::testing::ByMove;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::TestParamInfo;
using ::testing::Values;
using ::testing::WithParamInterface;

namespace internal {

// Based on RunOnceCallbackImpl in base/test/gmock_callback_support.h (see for
// more details). Modified to return a value earlier and run the callback
// on the designated runner (asynchronously) in order to test caching
// implementation. Specifically, this was necessary because
// disk_cache::EntryResult is a move-only type, and OpenOrCreateEntry can return
// early. Since this is based on the move-only type implementation of
// RunOnceCallbackImpl, it is not safe to reuse the Action or arguments.
template <size_t I, typename RetValue, typename Tuple>
auto ReturnAndRunOnceCallbackAfterImpl(RetValue&& ret, Tuple&& tuple) {
  auto ret_ptr = base::MakeRefCounted<base::RefCountedData<RetValue>>(
      std::forward<RetValue>(ret));
  auto tuple_ptr = base::MakeRefCounted<base::RefCountedData<Tuple>>(
      std::forward<Tuple>(tuple));
  return [ret_ptr = std::move(ret_ptr), tuple_ptr = std::move(tuple_ptr)](
             auto&&... args) mutable -> decltype(auto) {
    // Modified to pull the callback out from the arguments so that we can
    // create a lambda callback to send to run asynchronously.
    auto cb = std::move(base::internal::get<I>(args...));
    base::ThreadPool::PostTask(
        FROM_HERE,
        base::BindLambdaForTesting(
            [cb = std::move(cb), tuple_ptr = std::move(tuple_ptr)]() mutable {
              base::internal::RunImpl(
                  std::move(cb),
                  std::move(std::exchange(tuple_ptr, nullptr)->data));
            }));
    return std::move(std::exchange(ret_ptr, nullptr)->data);
  };
}

}  // namespace internal

// Extension of RunOnceCallback test helper in
// base/test/gmock_callback_support.h, but allows specifying the value to return
// from the call, and run the callback asynchronously after.
template <size_t I, typename RetValue, typename... RunArgs>
auto ReturnAndRunOnceCallbackAfter(RetValue&& ret, RunArgs&&... run_args) {
  return internal::ReturnAndRunOnceCallbackAfterImpl<I>(
      std::forward<RetValue>(ret),
      std::make_tuple(std::forward<RunArgs>(run_args)...));
}

TEST(DawnCachingInterfaceNoOpTest, LoadData) {
  // Without a backend, we should just passthrough return 0 for LoadData, and
  // not produce any errors.
  base::test::TaskEnvironment task_environment;
  std::unique_ptr<DawnCachingInterface> cache =
      DawnCachingInterface::CreateForTesting(base::BindLambdaForTesting(
          [](std::unique_ptr<disk_cache::Backend>* backend,
             base::WaitableEvent* signal, net::Error* error) -> void {
            backend->reset(nullptr);
            *error = net::OK;
            signal->Signal();
          }));
  ASSERT_EQ(cache->Init(), net::OK);
  EXPECT_EQ(0u, cache->LoadData(nullptr, 0, nullptr, 0));
}

TEST(DawnCachingInterfaceNoOpTest, StoreData) {
  // Without a backend, we should just passthrough for StoreData, and not
  // produce any errors.
  base::test::TaskEnvironment task_environment;
  std::unique_ptr<DawnCachingInterface> cache =
      DawnCachingInterface::CreateForTesting(base::BindLambdaForTesting(
          [](std::unique_ptr<disk_cache::Backend>* backend,
             base::WaitableEvent* signal, net::Error* error) -> void {
            backend->reset(nullptr);
            *error = net::OK;
            signal->Signal();
          }));
  ASSERT_EQ(cache->Init(), net::OK);
  cache->StoreData(nullptr, 0, nullptr, 0);
}

class DawnCachingInterfaceMockTest : public testing::Test {
 protected:
  DawnCachingInterfaceMockTest()
      : backend_mock_(new disk_cache::BackendMock(net::CacheType::DISK_CACHE)) {
    cache_ = DawnCachingInterface::CreateForTesting(base::BindLambdaForTesting(
        [this](std::unique_ptr<disk_cache::Backend>* backend,
               base::WaitableEvent* signal, net::Error* error) -> void {
          backend->reset(this->backend_mock_);
          *error = net::OK;
          signal->Signal();
        }));
    CHECK_EQ(cache_->Init(), net::OK);
  }
  ~DawnCachingInterfaceMockTest() override = default;

  // Wrapper around calling StoreData that synchronizes threads because
  // otherwise test expectations can race with the async nature of the write
  // operations.
  void StoreData(const void* key,
                 size_t key_size,
                 const void* value,
                 size_t value_size) {
    cache_->StoreData(key, key_size, value, value_size);
    task_environment_.RunUntilIdle();
  }

  // Constants that are reused throughout the tests
  static constexpr std::string_view kKey = "cache key";
  static constexpr size_t kDataSize = 10;

  // Temporary buffer used in tests.
  char buffer_[kDataSize];

  base::test::TaskEnvironment task_environment_;

  // Mocks initialized for the tests. Note that the backend mock is actually
  // owned and managed by the caching interface after initialization. We keep a
  // raw pointer to it though in order to set expectations.
  std::unique_ptr<DawnCachingInterface> cache_ = nullptr;
  // Owned by `cache_`.
  raw_ptr<disk_cache::BackendMock> backend_mock_ = nullptr;
};

TEST_F(DawnCachingInterfaceMockTest, LoadDataSizeSyncNominal) {
  std::unique_ptr<disk_cache::EntryMock> entry =
      std::make_unique<disk_cache::EntryMock>();
  EXPECT_CALL(*entry, GetDataSize(0)).WillOnce(Return(kDataSize));

  EXPECT_CALL(*backend_mock_, OpenOrCreateEntry(StrEq(kKey), _, _))
      .WillOnce(Return(
          ByMove(disk_cache::EntryResult::MakeCreated(entry.release()))));

  EXPECT_EQ(kDataSize, cache_->LoadData(kKey.data(), kKey.size(), nullptr, 0));
}

TEST_F(DawnCachingInterfaceMockTest, LoadDataSizeSyncErrorEntry) {
  // Error from entry should return 0 for data size.
  EXPECT_CALL(*backend_mock_, OpenOrCreateEntry(StrEq(kKey), _, _))
      .WillOnce(Return(
          ByMove(disk_cache::EntryResult::MakeError(net::Error::ERR_FAILED))));

  EXPECT_EQ(0u, cache_->LoadData(kKey.data(), kKey.size(), nullptr, 0));
}

TEST_F(DawnCachingInterfaceMockTest, LoadDataSizeSyncErrorRead) {
  // Error from data size read should cause us to return 0 for size.
  std::unique_ptr<disk_cache::EntryMock> entry =
      std::make_unique<disk_cache::EntryMock>();
  EXPECT_CALL(*entry, GetDataSize(0)).WillOnce(Return(net::Error::ERR_FAILED));

  EXPECT_CALL(*backend_mock_, OpenOrCreateEntry(StrEq(kKey), _, _))
      .WillOnce(Return(
          ByMove(disk_cache::EntryResult::MakeCreated(entry.release()))));

  EXPECT_EQ(0u, cache_->LoadData(kKey.data(), kKey.size(), nullptr, 0));
}

TEST_F(DawnCachingInterfaceMockTest, LoadDataSizeAsyncNominal) {
  std::unique_ptr<disk_cache::EntryMock> entry =
      std::make_unique<disk_cache::EntryMock>();
  EXPECT_CALL(*entry, GetDataSize(0)).WillOnce(Return(kDataSize));

  EXPECT_CALL(*backend_mock_, OpenOrCreateEntry(StrEq(kKey), _, _))
      .WillOnce(ReturnAndRunOnceCallbackAfter<2>(
          disk_cache::EntryResult::MakeError(net::Error::ERR_IO_PENDING),
          disk_cache::EntryResult::MakeCreated(entry.release())));

  EXPECT_EQ(kDataSize, cache_->LoadData(kKey.data(), kKey.size(), nullptr, 0));
}

TEST_F(DawnCachingInterfaceMockTest, LoadDataSizeAsyncErrorEntry) {
  // Error from entry should return 0 for data size.
  EXPECT_CALL(*backend_mock_, OpenOrCreateEntry(StrEq(kKey), _, _))
      .WillOnce(ReturnAndRunOnceCallbackAfter<2>(
          disk_cache::EntryResult::MakeError(net::Error::ERR_IO_PENDING),
          disk_cache::EntryResult::MakeError(net::Error::ERR_FAILED)));

  EXPECT_EQ(0u, cache_->LoadData(kKey.data(), kKey.size(), nullptr, 0));
}

TEST_F(DawnCachingInterfaceMockTest, LoadDataSizeAsyncTimeout) {
  EXPECT_CALL(*backend_mock_, OpenOrCreateEntry(StrEq(kKey), _, _))
      .WillOnce(Return(ByMove(
          disk_cache::EntryResult::MakeError(net::Error::ERR_IO_PENDING))));

  EXPECT_EQ(0u, cache_->LoadData(kKey.data(), kKey.size(), nullptr, 0));
}

TEST_F(DawnCachingInterfaceMockTest, LoadDataSyncNominal) {
  std::unique_ptr<disk_cache::EntryMock> entry =
      std::make_unique<disk_cache::EntryMock>();
  EXPECT_CALL(*entry, GetDataSize(0)).WillOnce(Return(kDataSize));
  EXPECT_CALL(*entry, ReadData(0, 0, _, kDataSize, _))
      .WillOnce(Return(kDataSize));

  EXPECT_CALL(*backend_mock_, OpenOrCreateEntry(StrEq(kKey), _, _))
      .WillOnce(Return(
          ByMove(disk_cache::EntryResult::MakeCreated(entry.release()))));

  EXPECT_EQ(kDataSize,
            cache_->LoadData(kKey.data(), kKey.size(), buffer_, kDataSize));
}

TEST_F(DawnCachingInterfaceMockTest, LoadDataAsyncNominal) {
  // Async on both the entry, and the read call.
  std::unique_ptr<disk_cache::EntryMock> entry =
      std::make_unique<disk_cache::EntryMock>();
  EXPECT_CALL(*entry, GetDataSize(0)).WillOnce(Return(kDataSize));
  EXPECT_CALL(*entry, ReadData(0, 0, _, kDataSize, _))
      .WillOnce(ReturnAndRunOnceCallbackAfter<4>(net::Error::ERR_IO_PENDING,
                                                 int(kDataSize)));

  EXPECT_CALL(*backend_mock_, OpenOrCreateEntry(StrEq(kKey), _, _))
      .WillOnce(ReturnAndRunOnceCallbackAfter<2>(
          disk_cache::EntryResult::MakeError(net::Error::ERR_IO_PENDING),
          disk_cache::EntryResult::MakeCreated(entry.release())));

  EXPECT_EQ(kDataSize,
            cache_->LoadData(kKey.data(), kKey.size(), buffer_, kDataSize));
}

TEST_F(DawnCachingInterfaceMockTest, LoadDataAsyncEntry) {
  // Sync return on the read, but async on the entry.
  std::unique_ptr<disk_cache::EntryMock> entry =
      std::make_unique<disk_cache::EntryMock>();
  EXPECT_CALL(*entry, GetDataSize(0)).WillOnce(Return(kDataSize));
  EXPECT_CALL(*entry, ReadData(0, 0, _, kDataSize, _))
      .WillOnce(Return(kDataSize));

  EXPECT_CALL(*backend_mock_, OpenOrCreateEntry(StrEq(kKey), _, _))
      .WillOnce(ReturnAndRunOnceCallbackAfter<2>(
          disk_cache::EntryResult::MakeError(net::Error::ERR_IO_PENDING),
          disk_cache::EntryResult::MakeCreated(entry.release())));

  EXPECT_EQ(kDataSize,
            cache_->LoadData(kKey.data(), kKey.size(), buffer_, kDataSize));
}

TEST_F(DawnCachingInterfaceMockTest, LoadDataAsyncRead) {
  // Sync return on the entry, but async on the actual read call.
  std::unique_ptr<disk_cache::EntryMock> entry =
      std::make_unique<disk_cache::EntryMock>();
  EXPECT_CALL(*entry, GetDataSize(0)).WillOnce(Return(kDataSize));
  EXPECT_CALL(*entry, ReadData(0, 0, _, kDataSize, _))
      .WillOnce(ReturnAndRunOnceCallbackAfter<4>(net::Error::ERR_IO_PENDING,
                                                 int(kDataSize)));

  EXPECT_CALL(*backend_mock_, OpenOrCreateEntry(StrEq(kKey), _, _))
      .WillOnce(Return(
          ByMove(disk_cache::EntryResult::MakeCreated(entry.release()))));

  EXPECT_EQ(kDataSize,
            cache_->LoadData(kKey.data(), kKey.size(), buffer_, kDataSize));
}

TEST_F(DawnCachingInterfaceMockTest, LoadDataAsyncTimeoutEntry) {
  // Timeout on getting the entry.
  EXPECT_CALL(*backend_mock_, OpenOrCreateEntry(StrEq(kKey), _, _))
      .WillOnce(Return(ByMove(
          disk_cache::EntryResult::MakeError(net::Error::ERR_IO_PENDING))));

  EXPECT_EQ(0u, cache_->LoadData(kKey.data(), kKey.size(), buffer_, kDataSize));
}

TEST_F(DawnCachingInterfaceMockTest, LoadDataAsyncTimeoutRead) {
  // Timeout on the read call.
  std::unique_ptr<disk_cache::EntryMock> entry =
      std::make_unique<disk_cache::EntryMock>();
  EXPECT_CALL(*entry, GetDataSize(0)).WillOnce(Return(kDataSize));
  EXPECT_CALL(*entry, ReadData(0, 0, _, kDataSize, _))
      .WillOnce(Return(net::Error::ERR_IO_PENDING));

  EXPECT_CALL(*backend_mock_, OpenOrCreateEntry(StrEq(kKey), _, _))
      .WillOnce(ReturnAndRunOnceCallbackAfter<2>(
          disk_cache::EntryResult::MakeError(net::Error::ERR_IO_PENDING),
          disk_cache::EntryResult::MakeCreated(entry.release())));

  EXPECT_EQ(0u, cache_->LoadData(kKey.data(), kKey.size(), buffer_, kDataSize));
}

TEST_F(DawnCachingInterfaceMockTest, StoreDataSyncNominal) {
  std::unique_ptr<disk_cache::EntryMock> entry =
      std::make_unique<disk_cache::EntryMock>();
  EXPECT_CALL(*entry, WriteData(0, 0, _, kDataSize, _, false))
      .WillOnce(Return(kDataSize));

  EXPECT_CALL(*backend_mock_, OpenOrCreateEntry(StrEq(kKey), _, _))
      .WillOnce(Return(
          ByMove(disk_cache::EntryResult::MakeCreated(entry.release()))));

  StoreData(kKey.data(), kKey.size(), buffer_, kDataSize);
}

TEST_F(DawnCachingInterfaceMockTest, StoreDataAsyncNominal) {
  // Async on both the entry, and the read call.
  std::unique_ptr<disk_cache::EntryMock> entry =
      std::make_unique<disk_cache::EntryMock>();
  EXPECT_CALL(*entry, WriteData(0, 0, _, kDataSize, _, false))
      .WillOnce(ReturnAndRunOnceCallbackAfter<4>(net::Error::ERR_IO_PENDING,
                                                 int(kDataSize)));

  EXPECT_CALL(*backend_mock_, OpenOrCreateEntry(StrEq(kKey), _, _))
      .WillOnce(ReturnAndRunOnceCallbackAfter<2>(
          disk_cache::EntryResult::MakeError(net::Error::ERR_IO_PENDING),
          disk_cache::EntryResult::MakeCreated(entry.release())));

  StoreData(kKey.data(), kKey.size(), buffer_, kDataSize);
}

TEST_F(DawnCachingInterfaceMockTest, StoreDataAsyncEntry) {
  // Sync return on the write, but async on the entry.
  std::unique_ptr<disk_cache::EntryMock> entry =
      std::make_unique<disk_cache::EntryMock>();
  EXPECT_CALL(*entry, WriteData(0, 0, _, kDataSize, _, false))
      .WillOnce(Return(kDataSize));

  EXPECT_CALL(*backend_mock_, OpenOrCreateEntry(StrEq(kKey), _, _))
      .WillOnce(ReturnAndRunOnceCallbackAfter<2>(
          disk_cache::EntryResult::MakeError(net::Error::ERR_IO_PENDING),
          disk_cache::EntryResult::MakeCreated(entry.release())));

  StoreData(kKey.data(), kKey.size(), buffer_, kDataSize);
}

TEST_F(DawnCachingInterfaceMockTest, StoreDataAsyncWrite) {
  // Sync return on the entry, but async on the actual write call.
  std::unique_ptr<disk_cache::EntryMock> entry =
      std::make_unique<disk_cache::EntryMock>();
  EXPECT_CALL(*entry, WriteData(0, 0, _, kDataSize, _, false))
      .WillOnce(ReturnAndRunOnceCallbackAfter<4>(net::Error::ERR_IO_PENDING,
                                                 int(kDataSize)));

  EXPECT_CALL(*backend_mock_, OpenOrCreateEntry(StrEq(kKey), _, _))
      .WillOnce(Return(
          ByMove(disk_cache::EntryResult::MakeCreated(entry.release()))));

  StoreData(kKey.data(), kKey.size(), buffer_, kDataSize);
}

TEST_F(DawnCachingInterfaceMockTest, StoreDataAsyncEntryNonblocking) {
  // Verifies that if the callback never runs for async entry, we do not block
  // running thread.
  EXPECT_CALL(*backend_mock_, OpenOrCreateEntry(StrEq(kKey), _, _))
      .WillOnce(Return(ByMove(
          disk_cache::EntryResult::MakeError(net::Error::ERR_IO_PENDING))));

  StoreData(kKey.data(), kKey.size(), buffer_, kDataSize);
}

TEST_F(DawnCachingInterfaceMockTest, StoreDataAsyncWriteNonblocking) {
  // Verifies that if the callback never runs for async write, we do not block
  // running thread.
  std::unique_ptr<disk_cache::EntryMock> entry =
      std::make_unique<disk_cache::EntryMock>();
  EXPECT_CALL(*entry, WriteData(0, 0, _, kDataSize, _, false))
      .WillOnce(Return(net::Error::ERR_IO_PENDING));

  EXPECT_CALL(*backend_mock_, OpenOrCreateEntry(StrEq(kKey), _, _))
      .WillOnce(ReturnAndRunOnceCallbackAfter<2>(
          disk_cache::EntryResult::MakeError(net::Error::ERR_IO_PENDING),
          disk_cache::EntryResult::MakeCreated(entry.release())));

  StoreData(kKey.data(), kKey.size(), buffer_, kDataSize);
}

TEST_F(DawnCachingInterfaceMockTest, StoreDataDoomSyncIncompleteWrite) {
  std::unique_ptr<disk_cache::EntryMock> entry =
      std::make_unique<disk_cache::EntryMock>();
  EXPECT_CALL(*entry, WriteData(0, 0, _, kDataSize, _, false))
      .WillOnce(Return(kDataSize - 1));
  EXPECT_CALL(*entry, Doom).Times(1);

  EXPECT_CALL(*backend_mock_, OpenOrCreateEntry(StrEq(kKey), _, _))
      .WillOnce(Return(
          ByMove(disk_cache::EntryResult::MakeCreated(entry.release()))));

  StoreData(kKey.data(), kKey.size(), buffer_, kDataSize);
}

TEST_F(DawnCachingInterfaceMockTest, StoreDataDoomSyncErrorWrite) {
  // Sync case where we get an error on the write.
  std::unique_ptr<disk_cache::EntryMock> entry =
      std::make_unique<disk_cache::EntryMock>();
  EXPECT_CALL(*entry, WriteData(0, 0, _, kDataSize, _, false))
      .WillOnce(Return(net::Error::ERR_FAILED));
  EXPECT_CALL(*entry, Doom).Times(1);

  EXPECT_CALL(*backend_mock_, OpenOrCreateEntry(StrEq(kKey), _, _))
      .WillOnce(Return(
          ByMove(disk_cache::EntryResult::MakeCreated(entry.release()))));

  StoreData(kKey.data(), kKey.size(), buffer_, kDataSize);
}

TEST_F(DawnCachingInterfaceMockTest, StoreDataDoomAsyncIncompleteWrite) {
  // Async case where we are unable to write the full data out.
  std::unique_ptr<disk_cache::EntryMock> entry =
      std::make_unique<disk_cache::EntryMock>();
  EXPECT_CALL(*entry, WriteData(0, 0, _, kDataSize, _, false))
      .WillOnce(ReturnAndRunOnceCallbackAfter<4>(net::Error::ERR_IO_PENDING,
                                                 int(kDataSize - 1)));
  EXPECT_CALL(*entry, Doom).Times(1);

  EXPECT_CALL(*backend_mock_, OpenOrCreateEntry(StrEq(kKey), _, _))
      .WillOnce(Return(
          ByMove(disk_cache::EntryResult::MakeCreated(entry.release()))));

  StoreData(kKey.data(), kKey.size(), buffer_, kDataSize);
}

TEST_F(DawnCachingInterfaceMockTest, StoreDataDoomAsyncErrorWrite) {
  // Async case where we get an error on the write.
  std::unique_ptr<disk_cache::EntryMock> entry =
      std::make_unique<disk_cache::EntryMock>();
  EXPECT_CALL(*entry, WriteData(0, 0, _, kDataSize, _, false))
      .WillOnce(ReturnAndRunOnceCallbackAfter<4>(net::Error::ERR_IO_PENDING,
                                                 net::Error::ERR_FAILED));
  EXPECT_CALL(*entry, Doom).Times(1);

  EXPECT_CALL(*backend_mock_, OpenOrCreateEntry(StrEq(kKey), _, _))
      .WillOnce(Return(
          ByMove(disk_cache::EntryResult::MakeCreated(entry.release()))));

  StoreData(kKey.data(), kKey.size(), buffer_, kDataSize);
}

class DawnCachingInterfaceTest : public PlatformTest,
                                 public WithParamInterface<net::CacheType> {
 protected:
  void SetUp() override {
    CHECK(temp_dir_.CreateUniqueTempDir());
    const base::FilePath path = temp_dir_.GetPath().AppendASCII("cache");

    const net::CacheType cache_type = GetParam();
    switch (cache_type) {
      case net::CacheType::DISK_CACHE:
      case net::CacheType::MEMORY_CACHE:
        break;
      default:
        // Other modes are not supported.
        NOTREACHED();
    }
    dawn_cache_ = std::make_unique<DawnCachingInterface>(cache_type, 0, path);
    ASSERT_EQ(dawn_cache_->Init(), net::OK);
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<DawnCachingInterface> dawn_cache_;
};

TEST_P(DawnCachingInterfaceTest, NominalUsage) {
  static constexpr std::array<size_t, 5> kKeySizes = {64, 128, 256, 512, 1024};
  static constexpr std::array<size_t, 5> kValueSizes = {256, 512, 1024, 2048,
                                                        4096};

  // Generate and save all key/value pairs in memory to validate (using strings
  // for simplicity).
  std::unordered_map<std::string, std::string> key_values;
  char id = 1;
  for (size_t key_size : kKeySizes) {
    for (size_t value_size : kValueSizes) {
      char key[key_size];
      memset(key, id, key_size);
      char value[value_size];
      memset(value, id, value_size);
      key_values[std::string(key, key_size)] = std::string(value, value_size);
      id++;
    }
  }

  // Add all pairs into the cache.
  for (const auto& [key, value] : key_values) {
    dawn_cache_->StoreData(key.data(), key.size(), value.data(), value.size());
  }

  // Since write ops can be async, we need to "flush" to make sure they occur.
  // As of writing this, it seems to work without running the loop two times,
  // however, it may become flaky without the double-flushing because the task
  // environment cannot flush the dedicated cache thread managed by the backend.
  // Because write/store operations are actually two separate tasks (open entry
  // and read/store), it could be possible that the first flush leaves lingering
  // read/store tasks that are not completed, hence the second one to confirm.
  for (int i = 0; i < 2; i++) {
    task_environment_.RunUntilIdle();
    base::RunLoop run_loop;
    disk_cache::FlushCacheThreadAsynchronouslyForTesting(
        base::BindLambdaForTesting([&run_loop]() { run_loop.Quit(); }));
    run_loop.Run();
  }

  // Verify that all pairs can be read back out from the cache.
  for (const auto& [key, value] : key_values) {
    // Verify that the size is the expected size.
    EXPECT_EQ(value.size(),
              dawn_cache_->LoadData(key.data(), key.size(), nullptr, 0));

    // Verify the actual contents.
    char buffer[value.size()];
    memset(buffer, 0, value.size());
    EXPECT_EQ(value.size(), dawn_cache_->LoadData(key.data(), key.size(),
                                                  buffer, value.size()));
    EXPECT_EQ(0, memcmp(buffer, value.data(), value.size()));
  }
}

INSTANTIATE_TEST_SUITE_P(
    DawnCachingInterfaceTests,
    DawnCachingInterfaceTest,
    Values(net::CacheType::DISK_CACHE, net::CacheType::MEMORY_CACHE),
    [](const TestParamInfo<DawnCachingInterfaceTest::ParamType>& info) {
      switch (info.param) {
        case net::CacheType::DISK_CACHE:
          return "DiskCache";
        case net::CacheType::MEMORY_CACHE:
          return "MemoryCache";
        default:
          return "UnknownCache";
      }
    });

}  // namespace
}  // namespace gpu::webgpu
