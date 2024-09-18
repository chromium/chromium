// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/bindings/parkable_string.h"

#include <algorithm>
#include <cstring>
#include <limits>

#include "base/functional/bind.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_provider.h"
#include "base/trace_event/process_memory_dump.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/platform/bindings/parkable_string_manager.h"
#include "third_party/blink/renderer/platform/disk_data_allocator_test_utils.h"
#include "third_party/blink/renderer/platform/instrumentation/memory_pressure_listener.h"
#include "third_party/blink/renderer/platform/scheduler/public/rail_mode_observer.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"

using ThreadPoolExecutionMode =
    base::test::TaskEnvironment::ThreadPoolExecutionMode;

namespace blink {

namespace {

constexpr size_t kSizeKb = 20;

// Compressed size of the string returned by |MakeLargeString()|.
// Update if the assertion in the |CheckCompressedSize()| test fails.
constexpr size_t kCompressedSizeZlib = 55;
constexpr size_t kCompressedSizeSnappy = 944;
#if BUILDFLAG(HAS_ZSTD_COMPRESSION)
constexpr size_t kCompressedSizeZstd = 19;
#endif

String MakeLargeString(char c = 'a') {
  Vector<char> data(kSizeKb * 1000, c);
  return String(data.data(), data.size()).ReleaseImpl();
}

String MakeComplexString(size_t size) {
  Vector<char> data(size, 'a');
  // This string should not be compressed too much, but also should not
  // be compressed failed. So make only some parts of this random.
  base::RandBytes(base::as_writable_byte_span(data).first(size / 10u));
  return String(data.data(), data.size()).ReleaseImpl();
}

class LambdaThreadDelegate : public base::PlatformThread::Delegate {
 public:
  explicit LambdaThreadDelegate(base::OnceClosure f) : f_(std::move(f)) {}
  void ThreadMain() override { std::move(f_).Run(); }

 private:
  base::OnceClosure f_;
};

}  // namespace

class ParkableStringTest
    : public testing::TestWithParam<ParkableStringImpl::CompressionAlgorithm> {
 public:
  ParkableStringTest(ThreadPoolExecutionMode thread_pool_execution_mode =
                         ThreadPoolExecutionMode::DEFAULT)
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME,
                          thread_pool_execution_mode) {
    ParkableStringImpl::CompressionAlgorithm algorithm = GetParam();
    switch (algorithm) {
      case ParkableStringImpl::CompressionAlgorithm::kZlib:
        scoped_feature_list_.InitWithFeatures(
            {}, {features::kUseSnappyForParkableStrings,
                 features::kUseZstdForParkableStrings});
        break;
      case ParkableStringImpl::CompressionAlgorithm::kSnappy:
        scoped_feature_list_.InitWithFeatures(
            {features::kUseSnappyForParkableStrings},
            {features::kUseZstdForParkableStrings});
        break;
#if BUILDFLAG(HAS_ZSTD_COMPRESSION)
      case ParkableStringImpl::CompressionAlgorithm::kZstd:
        scoped_feature_list_.InitWithFeatures(
            {features::kUseZstdForParkableStrings},
            {features::kUseSnappyForParkableStrings});
        break;
#endif  // BUILDFLAG(HAS_ZSTD_COMPRESSION)
    }

    CHECK_EQ(ParkableStringImpl::GetCompressionAlgorithm(), algorithm);
  }

 protected:
  void RunPostedTasks() { task_environment_.RunUntilIdle(); }

  bool ParkAndWait(const ParkableString& string) {
    bool success =
        string.Impl()->Park(ParkableStringImpl::ParkingMode::kCompress);
    RunPostedTasks();
    return success;
  }

  void WaitForAging() {
    if (base::FeatureList::IsEnabled(features::kCompressParkableStrings)) {
      EXPECT_GT(task_environment_.GetPendingMainThreadTaskCount(), 0u);
    }

    if (!first_aging_done_) {
      task_environment_.FastForwardBy(
          ParkableStringManager::kFirstParkingDelay);
      first_aging_done_ = true;
    } else {
      task_environment_.FastForwardBy(ParkableStringManager::AgingInterval());
    }
  }

  void WaitForDelayedParking() {
    // First wait for the string to get older.
    WaitForAging();
    // Now wait for the string to get parked.
    WaitForAging();
  }

  void WaitForDiskWriting() {
    WaitForAging();
    WaitForAging();
  }

  void CheckOnlyCpuCostTaskRemains() {
    unsigned expected_count = 0;
    if (ParkableStringManager::Instance()
            .has_posted_unparking_time_accounting_task_) {
      expected_count = 1;
    }
    EXPECT_EQ(expected_count,
              task_environment_.GetPendingMainThreadTaskCount());
  }

  void SetUp() override {
    auto& manager = ParkableStringManager::Instance();
    manager.ResetForTesting();
    manager.SetTaskRunnerForTesting(
        task_environment_.GetMainThreadTaskRunner());
    manager.SetDataAllocatorForTesting(
        std::make_unique<InMemoryDataAllocator>());

    manager.SetRendererBackgrounded(true);
    // No string yet, should not post a task since there is nothing to do.
    ASSERT_EQ(0u, task_environment_.GetPendingMainThreadTaskCount());
  }

  void TearDown() override {
    // No leaks.
    CHECK_EQ(0u, ParkableStringManager::Instance().Size());
    // Delayed tasks may remain, clear the queues.
    task_environment_.FastForwardUntilNoTasksRemain();
  }

  ParkableString CreateAndParkAll() {
    auto& manager = ParkableStringManager::Instance();
    // Checking that there are no other strings, to make sure this doesn't
    // cause side-effects.
    CHECK_EQ(0u, manager.Size());
    ParkableString parkable(MakeLargeString('a').ReleaseImpl());
    EXPECT_FALSE(parkable.Impl()->is_parked());
    WaitForDelayedParking();
    EXPECT_TRUE(parkable.Impl()->is_parked());
    return parkable;
  }

  void DisableOnDiskWriting() {
    ParkableStringManager::Instance().SetDataAllocatorForTesting(nullptr);
  }

  size_t GetExpectedCompressedSize() const {
    switch (ParkableStringImpl::GetCompressionAlgorithm()) {
      case ParkableStringImpl::CompressionAlgorithm::kZlib:
        return kCompressedSizeZlib;
      case ParkableStringImpl::CompressionAlgorithm::kSnappy:
        return kCompressedSizeSnappy;
#if BUILDFLAG(HAS_ZSTD_COMPRESSION)
      case ParkableStringImpl::CompressionAlgorithm::kZstd:
        return kCompressedSizeZstd;
#endif  // BUILDFLAG(HAS_ZSTD_COMPRESSION)
    }
  }

  bool first_aging_done_ = false;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
};

INSTANTIATE_TEST_SUITE_P(
    CompressionAlgorithm,
    ParkableStringTest,
    ::testing::Values(ParkableStringImpl::CompressionAlgorithm::kZlib,
                      ParkableStringImpl::CompressionAlgorithm::kSnappy
#if BUILDFLAG(HAS_ZSTD_COMPRESSION)
                      ,
                      ParkableStringImpl::CompressionAlgorithm::kZstd
#endif  // BUILDFLAG(HAS_ZSTD_COMPRESSION)
                      ));

// The main aim of this test is to check that the compressed size of a string
// doesn't change. If it does, |kCompressedSizeZlib| and/or
// |kCompressedSizeSnappy| will need to be updated.
TEST_P(ParkableStringTest, CheckCompressedSize) {
  const size_t kCompressedSize = GetExpectedCompressedSize();

  ParkableString parkable(MakeLargeString().ReleaseImpl());
  EXPECT_TRUE(
      parkable.Impl()->Park(ParkableStringImpl::ParkingMode::kCompress));
  RunPostedTasks();
  EXPECT_TRUE(parkable.Impl()->is_parked());
  EXPECT_EQ(kCompressedSize, parkable.Impl()->compressed_size());
}

TEST_P(ParkableStringTest, DontCompressRandomString) {
  base::HistogramTester histogram_tester;
  // Make a large random string. Large to make sure it's parkable, and random to
  // ensure its compressed size is larger than the initial size (at least from
  // gzip's header). Mersenne-Twister implementation is specified, making the
  // test deterministic.
  Vector<unsigned char> data(kSizeKb * 1000);
  base::RandBytes(data);
  ParkableString parkable(String(data.data(), data.size()).ReleaseImpl());

  EXPECT_TRUE(
      parkable.Impl()->Park(ParkableStringImpl::ParkingMode::kCompress));
  RunPostedTasks();
  // Not parked because the temporary buffer wasn't large enough.
  EXPECT_FALSE(parkable.Impl()->is_parked());
}

TEST_P(ParkableStringTest, ParkUnparkIdenticalContent) {
  ParkableString parkable(MakeLargeString().ReleaseImpl());
  EXPECT_TRUE(
      parkable.Impl()->Park(ParkableStringImpl::ParkingMode::kCompress));
  RunPostedTasks();
  EXPECT_TRUE(parkable.Impl()->is_parked());

  EXPECT_EQ(MakeLargeString(), parkable.ToString());
}

TEST_P(ParkableStringTest, DecompressUtf16String) {
  UChar emoji_grinning_face[2] = {0xd83d, 0xde00};
  size_t size_in_chars = 2 * kSizeKb * 1000 / sizeof(UChar);

  Vector<UChar> data(size_in_chars);
  for (size_t i = 0; i < size_in_chars / 2; ++i) {
    data[i * 2] = emoji_grinning_face[0];
    data[i * 2 + 1] = emoji_grinning_face[1];
  }

  String large_string = String(&data[0], size_in_chars);
  String copy = String(large_string.Impl()->IsolatedCopy());
  ParkableString parkable(large_string.ReleaseImpl());
  large_string = String();
  EXPECT_FALSE(parkable.Is8Bit());
  EXPECT_EQ(size_in_chars, parkable.length());
  EXPECT_EQ(sizeof(UChar) * size_in_chars, parkable.CharactersSizeInBytes());

  EXPECT_TRUE(
      parkable.Impl()->Park(ParkableStringImpl::ParkingMode::kCompress));
  EXPECT_TRUE(parkable.Impl()->background_task_in_progress_for_testing());
  RunPostedTasks();
  EXPECT_TRUE(parkable.Impl()->is_parked());
  EXPECT_FALSE(parkable.Impl()->background_task_in_progress_for_testing());

  // Decompression checks that the size is correct.
  String unparked = parkable.ToString();
  EXPECT_FALSE(unparked.Is8Bit());
  EXPECT_EQ(size_in_chars, unparked.length());
  EXPECT_EQ(sizeof(UChar) * size_in_chars, unparked.CharactersSizeInBytes());
  EXPECT_EQ(copy, unparked);
}

TEST_P(ParkableStringTest, Simple) {
  ParkableString parkable_abc(String("abc").ReleaseImpl());

  EXPECT_TRUE(ParkableString().IsNull());
  EXPECT_FALSE(parkable_abc.IsNull());
  EXPECT_TRUE(parkable_abc.Is8Bit());
  EXPECT_EQ(3u, parkable_abc.length());
  EXPECT_EQ(3u, parkable_abc.CharactersSizeInBytes());
  EXPECT_FALSE(
      parkable_abc.may_be_parked());  // Small strings are not parkable.

  EXPECT_EQ(String("abc"), parkable_abc.ToString());
  ParkableString copy = parkable_abc;
  EXPECT_EQ(copy.Impl(), parkable_abc.Impl());
}

TEST_P(ParkableStringTest, Park) {
  {
    ParkableString parkable_a(MakeLargeString('a').ReleaseImpl());
    EXPECT_TRUE(parkable_a.may_be_parked());
    EXPECT_FALSE(parkable_a.Impl()->is_parked());
    EXPECT_TRUE(ParkAndWait(parkable_a));
    EXPECT_TRUE(parkable_a.Impl()->is_parked());
  }

  String large_string = MakeLargeString('b');
  ParkableString parkable_b(large_string.Impl());
  EXPECT_TRUE(parkable_b.may_be_parked());
  // Not the only one to have a reference to the string.
  EXPECT_FALSE(ParkAndWait(parkable_b));
  large_string = String();
  EXPECT_TRUE(ParkAndWait(parkable_b));

  {
    ParkableString parkable_c(MakeLargeString('c').ReleaseImpl());
    EXPECT_TRUE(parkable_c.may_be_parked());
    EXPECT_FALSE(parkable_c.Impl()->is_parked());
    EXPECT_TRUE(
        parkable_c.Impl()->Park(ParkableStringImpl::ParkingMode::kCompress));
    // Should not crash, it is allowed to call |Park()| twice in a row.
    EXPECT_TRUE(
        parkable_c.Impl()->Park(ParkableStringImpl::ParkingMode::kCompress));
    parkable_c = ParkableString();  // Release the reference.
    RunPostedTasks();               // Should not crash.
  }
}

TEST_P(ParkableStringTest, EqualityNoUnparking) {
  String large_string = MakeLargeString();
  String copy = String(large_string.Impl()->IsolatedCopy());
  EXPECT_NE(large_string.Impl(), copy.Impl());

  ParkableString parkable(large_string.Impl());
  large_string = String();

  EXPECT_TRUE(parkable.may_be_parked());
  EXPECT_FALSE(parkable.Impl()->is_parked());
  EXPECT_TRUE(ParkAndWait(parkable));
  EXPECT_TRUE(parkable.Impl()->is_parked());

  ParkableString parkable_copy(copy.Impl());
  EXPECT_EQ(parkable_copy.Impl(), parkable.Impl());  // De-duplicated.
  EXPECT_TRUE(parkable.Impl()->is_parked());
  EXPECT_TRUE(parkable_copy.Impl()->is_parked());

  EXPECT_EQ(1u, ParkableStringManager::Instance().Size());
}

TEST_P(ParkableStringTest, AbortParking) {
  {
    ParkableString parkable(MakeLargeString().ReleaseImpl());
    EXPECT_TRUE(parkable.may_be_parked());
    EXPECT_FALSE(parkable.Impl()->is_parked());

    // The string is locked at the end of parking, should cancel it.
    EXPECT_TRUE(
        parkable.Impl()->Park(ParkableStringImpl::ParkingMode::kCompress));
    parkable.Impl()->Lock();
    RunPostedTasks();
    EXPECT_FALSE(parkable.Impl()->is_parked());

    // Unlock, OK to park.
    parkable.Impl()->Unlock();
    EXPECT_TRUE(ParkAndWait(parkable));
  }

  {
    ParkableString parkable(MakeLargeString().ReleaseImpl());
    // |ToString()| cancels parking as |content| is kept alive.
    EXPECT_TRUE(
        parkable.Impl()->Park(ParkableStringImpl::ParkingMode::kCompress));
    {
      String content = parkable.Impl()->ToString();
      RunPostedTasks();
      EXPECT_FALSE(parkable.Impl()->is_parked());
    }
    EXPECT_TRUE(ParkAndWait(parkable));
  }

  {
    ParkableString parkable(MakeLargeString().ReleaseImpl());
    // Transient |Lock()| or |ToString()| cancel parking.
    EXPECT_TRUE(
        parkable.Impl()->Park(ParkableStringImpl::ParkingMode::kCompress));
    parkable.Impl()->Lock();
    parkable.Impl()->ToString();
    parkable.Impl()->Unlock();
    RunPostedTasks();
    EXPECT_FALSE(parkable.Impl()->is_parked());

    // In order to test synchronous parking below, need to park the string
    // first.
    EXPECT_TRUE(
        parkable.Impl()->Park(ParkableStringImpl::ParkingMode::kCompress));
    RunPostedTasks();
    EXPECT_TRUE(parkable.Impl()->is_parked());
    parkable.ToString();

    // Synchronous parking respects locking and external references.
    parkable.ToString();
    EXPECT_TRUE(parkable.Impl()->has_compressed_data());
    parkable.Lock();
    EXPECT_FALSE(
        parkable.Impl()->Park(ParkableStringImpl::ParkingMode::kCompress));
    parkable.Unlock();
    {
      String content = parkable.ToString();
      EXPECT_FALSE(
          parkable.Impl()->Park(ParkableStringImpl::ParkingMode::kCompress));
    }
    // Parking is synchronous.
    EXPECT_TRUE(
        parkable.Impl()->Park(ParkableStringImpl::ParkingMode::kCompress));
    EXPECT_TRUE(parkable.Impl()->is_parked());
  }
}

TEST_P(ParkableStringTest, AbortedParkingRetainsCompressedData) {
  ParkableString parkable(MakeLargeString().ReleaseImpl());
  EXPECT_TRUE(parkable.may_be_parked());
  EXPECT_FALSE(parkable.Impl()->is_parked());

  EXPECT_TRUE(
      parkable.Impl()->Park(ParkableStringImpl::ParkingMode::kCompress));
  parkable.ToString();  // Cancels parking.
  RunPostedTasks();
  EXPECT_FALSE(parkable.Impl()->is_parked());
  // Compressed data is not discarded.
  EXPECT_TRUE(parkable.Impl()->has_compressed_data());

  // Synchronous parking.
  EXPECT_TRUE(
      parkable.Impl()->Park(ParkableStringImpl::ParkingMode::kCompress));
  EXPECT_TRUE(parkable.Impl()->is_parked());
}

TEST_P(ParkableStringTest, Unpark) {
  ParkableString parkable(MakeLargeString().Impl());
  String unparked_copy = String(parkable.ToString().Impl()->IsolatedCopy());
  EXPECT_TRUE(parkable.may_be_parked());
  EXPECT_FALSE(parkable.Impl()->is_parked());
  EXPECT_TRUE(ParkAndWait(parkable));
  EXPECT_TRUE(parkable.Impl()->is_parked());

  String unparked = parkable.ToString();
  EXPECT_EQ(unparked_copy, unparked);
  EXPECT_FALSE(parkable.Impl()->is_parked());
}

TEST_P(ParkableStringTest, BackgroundUnparkFromMemory) {
  // Memory parked strings can be unparked on a background thread.
  ParkableString parkable(MakeLargeString().Impl());
  String unparked_copy = String(parkable.ToString().Impl()->IsolatedCopy());
  EXPECT_TRUE(ParkAndWait(parkable));
  ParkableStringImpl* impl = parkable.Impl();
  // Check that the string was added to the correct StringMap.
  auto& manager = ParkableStringManager::Instance();
  EXPECT_TRUE(manager.IsOnParkedMapForTesting(impl));

  // Post unparking task to a background thread.
  base::ThreadPool::PostTask(FROM_HERE, base::BindOnce(
                                            [](ParkableStringImpl* string) {
                                              EXPECT_FALSE(IsMainThread());
                                              string->ToString();
                                            },
                                            base::RetainedRef(impl)));

  // Wait until the background unpark task is completed.
  while (true) {
    if (!impl->is_parked()) {
      break;
    }
  }

  // The move task is already posted, calling `ToString` in the Main thread
  // doesn't move the entry to the unparked string map.
  EXPECT_TRUE(manager.IsOnParkedMapForTesting(impl));
  EXPECT_EQ(parkable.ToString(), unparked_copy);
  EXPECT_TRUE(manager.IsOnParkedMapForTesting(impl));

  // Run the pending move task.
  RunPostedTasks();
  EXPECT_FALSE(manager.IsOnParkedMapForTesting(impl));
}

TEST_P(ParkableStringTest, BackgroundUnparkFromDisk) {
  // On disk strings can be unparked on a background thread.
  ParkableString parkable(MakeLargeString().Impl());
  String unparked_copy = String(parkable.ToString().Impl()->IsolatedCopy());
  EXPECT_TRUE(ParkAndWait(parkable));
  ParkableStringImpl* impl = parkable.Impl();

  WaitForDiskWriting();
  EXPECT_TRUE(impl->is_on_disk());

  // Check that the string was added to the correct StringMap.
  auto& manager = ParkableStringManager::Instance();
  EXPECT_TRUE(manager.IsOnDiskMapForTesting(impl));

  // Post unparking task to a background thread.
  base::ThreadPool::PostTask(FROM_HERE, base::BindOnce(
                                            [](ParkableStringImpl* string) {
                                              EXPECT_FALSE(IsMainThread());
                                              string->ToString();
                                            },
                                            base::RetainedRef(impl)));

  // Wait until the background unpark task is completed.
  while (true) {
    if (!impl->is_on_disk()) {
      break;
    }
  }

  // The move task is already posted, calling `ToString` in the Main thread
  // doesn't move the entry to the on_disk string map.
  EXPECT_TRUE(manager.IsOnDiskMapForTesting(impl));
  EXPECT_EQ(parkable.ToString(), unparked_copy);
  EXPECT_TRUE(manager.IsOnDiskMapForTesting(impl));

  // Run the pending move task.
  RunPostedTasks();
  EXPECT_FALSE(manager.IsOnDiskMapForTesting(impl));
}

struct ParkableStringWrapper {
  explicit ParkableStringWrapper(scoped_refptr<StringImpl> impl)
      : string(ParkableString(std::move(impl))) {}
  ParkableString string;
};

TEST_P(ParkableStringTest, BackgroundDestruct) {
  // Wrap a ParkableString in a unique_ptr to ensure that it is owned and
  // destroyed on a background thread.
  auto parkable =
      std::make_unique<ParkableStringWrapper>(MakeLargeString().ReleaseImpl());
  EXPECT_TRUE(parkable->string.Impl()->HasOneRef());
  base::ThreadPool::PostTask(
      FROM_HERE, base::BindOnce(
                     [](std::unique_ptr<ParkableStringWrapper> parkable) {
                       EXPECT_FALSE(IsMainThread());
                       EXPECT_TRUE(parkable->string.Impl()->HasOneRef());
                     },
                     std::move(parkable)));
  RunPostedTasks();
  CHECK_EQ(0u, ParkableStringManager::Instance().Size());
}

TEST_P(ParkableStringTest, LockUnlock) {
  ParkableString parkable(MakeLargeString().Impl());
  ParkableStringImpl* impl = parkable.Impl();
  EXPECT_EQ(0, impl->lock_depth_for_testing());

  parkable.Lock();
  EXPECT_EQ(1, impl->lock_depth_for_testing());
  parkable.Lock();
  parkable.Unlock();
  EXPECT_EQ(1, impl->lock_depth_for_testing());
  parkable.Unlock();
  EXPECT_EQ(0, impl->lock_depth_for_testing());

  parkable.Lock();
  EXPECT_FALSE(ParkAndWait(parkable));
  parkable.Unlock();
  EXPECT_TRUE(ParkAndWait(parkable));

  parkable.ToString();

  LambdaThreadDelegate delegate(
      base::BindLambdaForTesting([&]() { parkable.Lock(); }));
  base::PlatformThreadHandle thread_handle;
  base::PlatformThread::Create(0, &delegate, &thread_handle);
  base::PlatformThread::Join(thread_handle);

  EXPECT_FALSE(ParkAndWait(parkable));
  parkable.Unlock();
  EXPECT_TRUE(ParkAndWait(parkable));
}

TEST_P(ParkableStringTest, LockParkedString) {
  ParkableString parkable = CreateAndParkAll();
  ParkableStringImpl* impl = parkable.Impl();

  parkable.Lock();  // Locking doesn't unpark.
  EXPECT_TRUE(impl->is_parked());
  parkable.ToString();
  EXPECT_FALSE(impl->is_parked());
  EXPECT_EQ(1, impl->lock_depth_for_testing());

  EXPECT_FALSE(ParkAndWait(parkable));

  parkable.Unlock();
  EXPECT_EQ(0, impl->lock_depth_for_testing());
  EXPECT_TRUE(ParkAndWait(parkable));
  EXPECT_TRUE(impl->is_parked());
}

TEST_P(ParkableStringTest, DelayFirstParkingOfString) {
  base::test::ScopedFeatureList features;

  auto& manager = ParkableStringManager::Instance();
  EXPECT_EQ(0u, manager.Size());

  // Create a large string that will end up parked.
  ParkableString parkable(MakeLargeString().Impl());
  ASSERT_FALSE(parkable.Impl()->is_parked());
  EXPECT_EQ(1u, manager.Size());
  // Should age after this point.
  task_environment_.FastForwardBy(ParkableStringManager::kFirstParkingDelay);

  // String is aged but not parked.
  EXPECT_FALSE(parkable.Impl()->is_parked());

  // Now that the first aging took place the next aging task will take place
  // after the normal interval.
  task_environment_.FastForwardBy(ParkableStringManager::AgingInterval());

  EXPECT_TRUE(parkable.Impl()->is_parked());
}

TEST_P(ParkableStringTest, ManagerSimple) {
  auto& manager = ParkableStringManager::Instance();
  EXPECT_EQ(0u, manager.Size());

  // Small strings are not tracked.
  ParkableString small(String("abc").ReleaseImpl());
  EXPECT_EQ(0u, manager.Size());

  // Large ones are.
  ParkableString parkable(MakeLargeString().Impl());
  ASSERT_FALSE(parkable.Impl()->is_parked());
  EXPECT_EQ(1u, manager.Size());

  WaitForDelayedParking();
  EXPECT_TRUE(parkable.Impl()->is_parked());

  // Park and unpark.
  parkable.ToString();
  EXPECT_FALSE(parkable.Impl()->is_parked());
  WaitForDelayedParking();
  EXPECT_TRUE(parkable.Impl()->is_parked());

  // More than one reference, no parking.
  String alive_unparked = parkable.ToString();
  WaitForDelayedParking();
  EXPECT_FALSE(parkable.Impl()->is_parked());

  // Since no strings are parkable, the tick stopped.
  CheckOnlyCpuCostTaskRemains();

  // Other reference is dropped, OK to park.
  alive_unparked = String();

  // Tick was not scheduled, no parking.
  WaitForDelayedParking();
  EXPECT_FALSE(parkable.Impl()->is_parked());

  // Create a temporary string to start the tick again.
  { ParkableString tmp(MakeLargeString('b').ReleaseImpl()); }
  WaitForDelayedParking();
  EXPECT_TRUE(parkable.Impl()->is_parked());
}

TEST_P(ParkableStringTest, ManagerMultipleStrings) {
  auto& manager = ParkableStringManager::Instance();
  EXPECT_EQ(0u, manager.Size());

  ParkableString parkable(MakeLargeString('a').Impl());
  ParkableString parkable2(MakeLargeString('b').Impl());
  EXPECT_EQ(2u, manager.Size());

  parkable2 = ParkableString();
  EXPECT_EQ(1u, manager.Size());

  ParkableString copy = parkable;
  parkable = ParkableString();
  EXPECT_EQ(1u, manager.Size());
  copy = ParkableString();
  EXPECT_EQ(0u, manager.Size());

  String str = MakeLargeString('c');
  ParkableString parkable3(str.Impl());
  EXPECT_EQ(1u, manager.Size());
  // De-duplicated with the same underlying StringImpl.
  ParkableString other_parkable3(str.Impl());
  EXPECT_EQ(1u, manager.Size());
  EXPECT_EQ(parkable3.Impl(), other_parkable3.Impl());

  {
    // De-duplicated with a different StringImpl but the same content.
    ParkableString other_parkable3_different_string(
        MakeLargeString('c').ReleaseImpl());
    EXPECT_EQ(1u, manager.Size());
    EXPECT_EQ(parkable3.Impl(), other_parkable3_different_string.Impl());
  }

  // If all the references to a string are internal, park it.
  str = String();
  // This string is not parkable, but should still be tracked.
  ParkableString parkable4(MakeLargeString('d').Impl());
  String parkable4_content = parkable4.ToString();

  // Only drop it from the managed strings when the last one is gone.
  parkable3 = ParkableString();
  EXPECT_EQ(2u, manager.Size());
  other_parkable3 = ParkableString();
  EXPECT_EQ(1u, manager.Size());
  parkable4 = ParkableString();
  EXPECT_EQ(0u, manager.Size());
}

TEST_P(ParkableStringTest, ShouldPark) {
  String empty_string("");
  EXPECT_FALSE(ParkableStringManager::ShouldPark(*empty_string.Impl()));
  String parkable(MakeLargeString().ReleaseImpl());
  EXPECT_TRUE(ParkableStringManager::ShouldPark(*parkable.Impl()));

  LambdaThreadDelegate delegate(base::BindLambdaForTesting([]() {
    String parkable(MakeLargeString().ReleaseImpl());
    EXPECT_FALSE(ParkableStringManager::ShouldPark(*parkable.Impl()));
  }));
  base::PlatformThreadHandle thread_handle;
  base::PlatformThread::Create(0, &delegate, &thread_handle);
  base::PlatformThread::Join(thread_handle);
}

#if defined(ADDRESS_SANITIZER)
#define EXPECT_ASAN_DEATH(statement, regex) EXPECT_DEATH(statement, regex)
#else
#define EXPECT_ASAN_DEATH(statement, regex) \
  GTEST_UNSUPPORTED_DEATH_TEST(statement, regex, )
#endif

TEST_P(ParkableStringTest, AsanPoisoningTest) {
  ParkableString parkable(MakeLargeString().ReleaseImpl());
  const LChar* data = parkable.ToString().Characters8();
  EXPECT_TRUE(ParkAndWait(parkable));
  EXPECT_ASAN_DEATH(EXPECT_NE(0, data[10]), "");
}

// Non-regression test for crbug.com/905137.
TEST_P(ParkableStringTest, CorrectAsanPoisoning) {
  ParkableString parkable(MakeLargeString().ReleaseImpl());
  EXPECT_TRUE(
      parkable.Impl()->Park(ParkableStringImpl::ParkingMode::kCompress));
  // A main thread task is posted once compression is done.
  while (task_environment_.GetPendingMainThreadTaskCount() == 0) {
    parkable.Lock();
    parkable.ToString();
    parkable.Unlock();
  }
  RunPostedTasks();
}

TEST_P(ParkableStringTest, Compression) {
  const size_t kCompressedSize = GetExpectedCompressedSize();

  base::HistogramTester histogram_tester;

  ParkableString parkable = CreateAndParkAll();
  ParkableStringImpl* impl = parkable.Impl();

  EXPECT_TRUE(impl->is_parked());
  EXPECT_TRUE(impl->has_compressed_data());
  EXPECT_EQ(kCompressedSize, impl->compressed_size());
  parkable.ToString();  // First decompression.
  EXPECT_FALSE(impl->is_parked());
  EXPECT_TRUE(impl->has_compressed_data());
  EXPECT_TRUE(impl->Park(ParkableStringImpl::ParkingMode::kSynchronousOnly));
  EXPECT_TRUE(impl->is_parked());
  parkable.ToString();  // Second decompression.

  histogram_tester.ExpectUniqueSample(
      "Memory.ParkableString.Compression.SizeKb", kSizeKb, 1);
  histogram_tester.ExpectTotalCount("Memory.ParkableString.Compression.Latency",
                                    1);
  histogram_tester.ExpectTotalCount(
      "Memory.ParkableString.Decompression.Latency", 2);
  histogram_tester.ExpectTotalCount(
      "Memory.ParkableString.Decompression.ThroughputMBps", 2);
}

TEST_P(ParkableStringTest, SynchronousCompression) {
  ParkableStringManager& manager = ParkableStringManager::Instance();
  ParkableString parkable = CreateAndParkAll();

  parkable.ToString();
  EXPECT_TRUE(parkable.Impl()->has_compressed_data());
  // No waiting, synchronous compression.
  manager.ParkAll(ParkableStringImpl::ParkingMode::kSynchronousOnly);
  EXPECT_TRUE(parkable.Impl()->is_parked());
  task_environment_.FastForwardUntilNoTasksRemain();
}

TEST_P(ParkableStringTest, CompressionFailed) {
  const size_t kSize = 20000;
  Vector<char> data(kSize);
  base::RandBytes(base::as_writable_byte_span(data));
  ParkableString parkable(String(data.data(), data.size()).ReleaseImpl());
  WaitForDelayedParking();
  EXPECT_EQ(ParkableStringImpl::Age::kOld, parkable.Impl()->age_for_testing());

  // Because input string is too complicated, parking has failed.
  EXPECT_FALSE(parkable.Impl()->is_parked());

  // Make sure there will be no additional parking trial for this string.
  EXPECT_EQ(ParkableStringImpl::AgeOrParkResult::kNonTransientFailure,
            parkable.Impl()->MaybeAgeOrParkString());

  // |Park()| should be failed as well.
  EXPECT_FALSE(
      parkable.Impl()->Park(ParkableStringImpl::ParkingMode::kCompress));
}

TEST_P(ParkableStringTest, ToAndFromDisk) {
  base::HistogramTester histogram_tester;

  ParkableString parkable(MakeLargeString('a').ReleaseImpl());
  ParkableStringImpl* impl = parkable.Impl();

  EXPECT_EQ(ParkableStringImpl::Age::kYoung, impl->age_for_testing());
  WaitForAging();
  EXPECT_EQ(ParkableStringImpl::Age::kOld, impl->age_for_testing());
  WaitForAging();
  EXPECT_EQ(ParkableStringImpl::Age::kOld, impl->age_for_testing());
  parkable.Impl()->MaybeAgeOrParkString();
  EXPECT_EQ(ParkableStringImpl::Age::kVeryOld, impl->age_for_testing());
  impl->MaybeAgeOrParkString();
  EXPECT_FALSE(impl->is_on_disk());
  RunPostedTasks();
  EXPECT_TRUE(impl->is_on_disk());
  parkable.ToString();
  EXPECT_FALSE(impl->is_on_disk());
  EXPECT_EQ(ParkableStringImpl::Age::kYoung, impl->age_for_testing());

  histogram_tester.ExpectTotalCount("Memory.ParkableString.Read.Latency", 1);
}

TEST_P(ParkableStringTest, UnparkWhileWritingToDisk) {
  base::HistogramTester histogram_tester;

  ParkableString parkable(MakeLargeString('a').ReleaseImpl());
  ParkableStringImpl* impl = parkable.Impl();

  EXPECT_EQ(ParkableStringImpl::Age::kYoung, impl->age_for_testing());
  WaitForAging();
  EXPECT_EQ(ParkableStringImpl::Age::kOld, impl->age_for_testing());
  WaitForAging();
  EXPECT_EQ(ParkableStringImpl::Age::kOld, impl->age_for_testing());
  parkable.Impl()->MaybeAgeOrParkString();
  EXPECT_EQ(ParkableStringImpl::Age::kVeryOld, impl->age_for_testing());
  impl->MaybeAgeOrParkString();
  EXPECT_FALSE(impl->is_on_disk());
  EXPECT_TRUE(impl->background_task_in_progress_for_testing());

  // Unparking cancels discarding to disk.
  EXPECT_FALSE(parkable.ToString().IsNull());
  EXPECT_TRUE(impl->background_task_in_progress_for_testing());
  RunPostedTasks();
  EXPECT_FALSE(impl->is_on_disk());
  EXPECT_TRUE(impl->has_on_disk_data());
  EXPECT_EQ(ParkableStringImpl::Age::kYoung, impl->age_for_testing());
}

TEST_P(ParkableStringTest, NoCompetingWritingToDisk) {
  ParkableString parkable(MakeLargeString('a').ReleaseImpl());
  ParkableStringImpl* impl = parkable.Impl();

  EXPECT_EQ(ParkableStringImpl::Age::kYoung, impl->age_for_testing());
  WaitForAging();
  EXPECT_EQ(ParkableStringImpl::Age::kOld, impl->age_for_testing());
  WaitForAging();
  EXPECT_EQ(ParkableStringImpl::Age::kOld, impl->age_for_testing());
  parkable.Impl()->MaybeAgeOrParkString();
  EXPECT_EQ(ParkableStringImpl::Age::kVeryOld, impl->age_for_testing());
  impl->MaybeAgeOrParkString();
  EXPECT_FALSE(impl->is_on_disk());

  // Unparking cancels discarding to disk.
  EXPECT_FALSE(parkable.ToString().IsNull());
  EXPECT_EQ(ParkableStringImpl::Age::kYoung, impl->age_for_testing());
  // Until the writing is finished, the string cannot age again.
  impl->MaybeAgeOrParkString();
  EXPECT_EQ(ParkableStringImpl::Age::kYoung, impl->age_for_testing());

  RunPostedTasks();
  EXPECT_FALSE(impl->is_on_disk());
  EXPECT_TRUE(impl->has_on_disk_data());
  EXPECT_EQ(ParkableStringImpl::Age::kYoung, impl->age_for_testing());
  // Aging is now possible again.
  impl->MaybeAgeOrParkString();
  EXPECT_EQ(ParkableStringImpl::Age::kOld, impl->age_for_testing());
}

TEST_P(ParkableStringTest, SynchronousToDisk) {
  base::HistogramTester histogram_tester;

  ParkableString parkable(MakeLargeString('a').ReleaseImpl());
  ParkableStringImpl* impl = parkable.Impl();

  WaitForDelayedParking();
  EXPECT_EQ(ParkableStringImpl::Age::kOld, impl->age_for_testing());
  WaitForAging();
  EXPECT_EQ(ParkableStringImpl::Age::kVeryOld, impl->age_for_testing());
  EXPECT_FALSE(impl->is_on_disk());

  // Writing to disk is asynchronous.
  impl->MaybeAgeOrParkString();
  EXPECT_FALSE(impl->is_on_disk());
  WaitForAging();
  EXPECT_TRUE(impl->is_on_disk());

  parkable.ToString();
  EXPECT_FALSE(impl->is_on_disk());

  impl->MaybeAgeOrParkString();
  impl->MaybeAgeOrParkString();
  impl->MaybeAgeOrParkString();

  EXPECT_FALSE(impl->is_on_disk());
  impl->MaybeAgeOrParkString();
  EXPECT_TRUE(impl->is_on_disk());  // Synchronous writing.

  parkable.ToString();
}

TEST_P(ParkableStringTest, OnPurgeMemory) {
  ParkableString parkable1 = CreateAndParkAll();
  ParkableString parkable2(MakeLargeString('b').ReleaseImpl());

  // Park everything.
  WaitForDelayedParking();
  EXPECT_TRUE(parkable1.Impl()->is_on_disk());
  EXPECT_TRUE(parkable2.Impl()->is_parked());

  // Different usage patterns:
  // 1. Parkable, will be parked synchronouly.
  // 2. Cannot be parked, compressed representation is purged.
  parkable1.ToString();
  String retained = parkable2.ToString();
  EXPECT_TRUE(parkable2.Impl()->has_compressed_data());

  MemoryPressureListenerRegistry::Instance().OnPurgeMemory();
  EXPECT_TRUE(parkable1.Impl()->is_parked());  // Parked synchronously.
  EXPECT_FALSE(parkable2.Impl()->is_parked());

  parkable1.ToString();
  EXPECT_TRUE(parkable1.Impl()->has_compressed_data());
}

TEST_P(ParkableStringTest, ReportMemoryDump) {
  const size_t kCompressedSize = GetExpectedCompressedSize();

  using base::trace_event::MemoryAllocatorDump;
  using testing::ByRef;
  using testing::Contains;
  using testing::Eq;

  constexpr size_t kActualSize =
      sizeof(ParkableStringImpl) + sizeof(ParkableStringImpl::ParkableMetadata);

  auto& manager = ParkableStringManager::Instance();
  ParkableString parkable1(MakeLargeString('a').ReleaseImpl());
  ParkableString parkable2(MakeLargeString('b').ReleaseImpl());
  // Not reported in stats below.
  ParkableString parkable3(String("short string, not parkable").ReleaseImpl());

  WaitForDelayedParking();
  parkable1.ToString();

  base::trace_event::MemoryDumpArgs args = {
      base::trace_event::MemoryDumpLevelOfDetail::kDetailed};
  base::trace_event::ProcessMemoryDump pmd(args);
  manager.OnMemoryDump(&pmd);
  base::trace_event::MemoryAllocatorDump* dump =
      pmd.GetAllocatorDump("parkable_strings");
  ASSERT_NE(nullptr, dump);

  constexpr size_t kStringSize = kSizeKb * 1000;
  MemoryAllocatorDump::Entry original("original_size", "bytes",
                                      2 * kStringSize);
  EXPECT_THAT(dump->entries(), Contains(Eq(ByRef(original))));

  // |parkable1| is unparked.
  MemoryAllocatorDump::Entry uncompressed("uncompressed_size", "bytes",
                                          kStringSize);
  EXPECT_THAT(dump->entries(), Contains(Eq(ByRef(uncompressed))));

  MemoryAllocatorDump::Entry compressed("compressed_size", "bytes",
                                        kCompressedSize);
  EXPECT_THAT(dump->entries(), Contains(Eq(ByRef(compressed))));

  // |parkable1| compressed data is overhead.
  MemoryAllocatorDump::Entry overhead("overhead_size", "bytes",
                                      kCompressedSize);
  EXPECT_THAT(dump->entries(), Contains(Eq(ByRef(overhead))));

  MemoryAllocatorDump::Entry metadata("metadata_size", "bytes",
                                      2 * kActualSize);
  EXPECT_THAT(dump->entries(), Contains(Eq(ByRef(metadata))));

  MemoryAllocatorDump::Entry savings(
      "savings_size", "bytes",
      2 * kStringSize - (kStringSize + 2 * kCompressedSize + 2 * kActualSize));
  EXPECT_THAT(dump->entries(), Contains(Eq(ByRef(savings))));

  MemoryAllocatorDump::Entry on_disk("on_disk_size", "bytes", 0);
  EXPECT_THAT(dump->entries(), Contains(Eq(ByRef(on_disk))));
  MemoryAllocatorDump::Entry on_disk_footprint("on_disk_footprint", "bytes", 0);
  EXPECT_THAT(dump->entries(), Contains(Eq(ByRef(on_disk_footprint))));

  WaitForDiskWriting();
  EXPECT_TRUE(parkable1.Impl()->has_compressed_data());
  EXPECT_TRUE(parkable2.Impl()->is_on_disk());

  pmd = base::trace_event::ProcessMemoryDump(args);
  manager.OnMemoryDump(&pmd);
  dump = pmd.GetAllocatorDump("parkable_strings");
  ASSERT_NE(nullptr, dump);
  on_disk =
      MemoryAllocatorDump::Entry("on_disk_size", "bytes", kCompressedSize);
  EXPECT_THAT(dump->entries(), Contains(Eq(ByRef(on_disk))));
  // |parkable2| is on disk.
  on_disk_footprint =
      MemoryAllocatorDump::Entry("on_disk_footprint", "bytes", kCompressedSize);
  EXPECT_THAT(dump->entries(), Contains(Eq(ByRef(on_disk_footprint))));

  MemoryAllocatorDump::Entry on_disk_free_chunks =
      MemoryAllocatorDump::Entry("on_disk_free_chunks", "bytes", 0);
  EXPECT_THAT(dump->entries(), Contains(Eq(ByRef(on_disk_free_chunks))));

  // |parkable1| is compressed.
  compressed =
      MemoryAllocatorDump::Entry("compressed_size", "bytes", kCompressedSize);
  EXPECT_THAT(dump->entries(), Contains(Eq(ByRef(compressed))));
}

TEST_P(ParkableStringTest, MemoryFootprintForDump) {
  constexpr size_t kActualSize =
      sizeof(ParkableStringImpl) + sizeof(ParkableStringImpl::ParkableMetadata);

  size_t memory_footprint;
  ParkableString parkable1(MakeLargeString('a').ReleaseImpl());
  ParkableString parkable2(MakeLargeString('b').ReleaseImpl());
  ParkableString parkable3(String("short string, not parkable").ReleaseImpl());

  WaitForDelayedParking();
  parkable1.ToString();

  // Compressed and uncompressed data.
  memory_footprint = kActualSize + parkable1.Impl()->compressed_size() +
                     parkable1.Impl()->CharactersSizeInBytes();
  EXPECT_EQ(memory_footprint, parkable1.Impl()->MemoryFootprintForDump());

  // Compressed uncompressed data only.
  memory_footprint = kActualSize + parkable2.Impl()->compressed_size();
  EXPECT_EQ(memory_footprint, parkable2.Impl()->MemoryFootprintForDump());

  // Short string, no metadata.
  memory_footprint =
      sizeof(ParkableStringImpl) + parkable3.Impl()->CharactersSizeInBytes();
  EXPECT_EQ(memory_footprint, parkable3.Impl()->MemoryFootprintForDump());
}

TEST_P(ParkableStringTest, CompressionDisabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kCompressParkableStrings);

  ParkableString parkable(MakeLargeString().ReleaseImpl());
  WaitForDelayedParking();
  EXPECT_FALSE(parkable.Impl()->may_be_parked());

  MemoryPressureListenerRegistry::Instance().OnPurgeMemory();
  EXPECT_FALSE(parkable.Impl()->may_be_parked());
}

TEST_P(ParkableStringTest, CompressionDisabledDisablesDisk) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kCompressParkableStrings);

  EXPECT_FALSE(features::IsParkableStringsToDiskEnabled());
}

TEST_P(ParkableStringTest, Aging) {
  ParkableString parkable(MakeLargeString().ReleaseImpl());
  EXPECT_EQ(ParkableStringImpl::Age::kYoung,
            parkable.Impl()->age_for_testing());
  WaitForAging();
  EXPECT_EQ(ParkableStringImpl::Age::kOld, parkable.Impl()->age_for_testing());

  parkable.Lock();
  EXPECT_EQ(ParkableStringImpl::Age::kYoung,
            parkable.Impl()->age_for_testing());
  // Locked strings don't age.
  WaitForAging();
  EXPECT_EQ(ParkableStringImpl::Age::kYoung,
            parkable.Impl()->age_for_testing());
  parkable.Unlock();
  WaitForAging();
  EXPECT_EQ(ParkableStringImpl::Age::kOld, parkable.Impl()->age_for_testing());

  parkable.ToString();
  EXPECT_EQ(ParkableStringImpl::Age::kYoung,
            parkable.Impl()->age_for_testing());
  // No external reference, can age again.
  WaitForAging();
  EXPECT_EQ(ParkableStringImpl::Age::kOld, parkable.Impl()->age_for_testing());

  // External references prevent a string from aging.
  String retained = parkable.ToString();
  EXPECT_EQ(ParkableStringImpl::Age::kYoung,
            parkable.Impl()->age_for_testing());
  WaitForAging();
  EXPECT_EQ(ParkableStringImpl::Age::kYoung,
            parkable.Impl()->age_for_testing());
}

TEST_P(ParkableStringTest, NoPrematureAging) {
  ParkableString parkable(MakeLargeString().ReleaseImpl());
  EXPECT_EQ(ParkableStringImpl::Age::kYoung,
            parkable.Impl()->age_for_testing());

  task_environment_.FastForwardBy(ParkableStringManager::AgingInterval());

  // Since not enough time elapsed not aging was done.
  EXPECT_EQ(ParkableStringImpl::Age::kYoung,
            parkable.Impl()->age_for_testing());
}

TEST_P(ParkableStringTest, OldStringsAreParked) {
  ParkableString parkable(MakeLargeString().ReleaseImpl());
  EXPECT_EQ(ParkableStringImpl::Age::kYoung,
            parkable.Impl()->age_for_testing());
  WaitForAging();
  EXPECT_EQ(ParkableStringImpl::Age::kOld, parkable.Impl()->age_for_testing());
  WaitForAging();
  EXPECT_TRUE(parkable.Impl()->is_parked());

  // Unparked, two aging cycles before parking.
  parkable.ToString();
  EXPECT_FALSE(parkable.Impl()->is_parked());
  WaitForAging();
  EXPECT_FALSE(parkable.Impl()->is_parked());
  WaitForAging();
  EXPECT_TRUE(parkable.Impl()->is_parked());

  // Unparked, two consecutive no-access aging cycles before parking.
  parkable.ToString();
  EXPECT_FALSE(parkable.Impl()->is_parked());
  WaitForAging();
  EXPECT_FALSE(parkable.Impl()->is_parked());
  parkable.ToString();
  WaitForAging();
  EXPECT_FALSE(parkable.Impl()->is_parked());
}

TEST_P(ParkableStringTest, AgingTicksStopsAndRestarts) {
  ParkableString parkable(MakeLargeString().ReleaseImpl());
  EXPECT_GT(task_environment_.GetPendingMainThreadTaskCount(), 0u);
  WaitForAging();
  EXPECT_GT(task_environment_.GetPendingMainThreadTaskCount(), 0u);
  WaitForAging();
  EXPECT_TRUE(parkable.Impl()->is_parked());
  WaitForDiskWriting();
  EXPECT_TRUE(parkable.Impl()->is_on_disk());
  WaitForAging();
  // Nothing more to do, the tick is not re-scheduled.
  CheckOnlyCpuCostTaskRemains();

  // Unparking, the tick restarts.
  parkable.ToString();
  EXPECT_GT(task_environment_.GetPendingMainThreadTaskCount(), 0u);
  WaitForDelayedParking();
  WaitForDiskWriting();
  // And stops again. 2 ticks to park the string (age, then park), and one
  // checking that there is nothing left to do.
  CheckOnlyCpuCostTaskRemains();

  // // New string, restarting the tick, temporarily.
  ParkableString parkable2(MakeLargeString().ReleaseImpl());
  WaitForDelayedParking();
  WaitForDiskWriting();
  WaitForAging();
  CheckOnlyCpuCostTaskRemains();
}

TEST_P(ParkableStringTest, AgingTicksStopsWithNoProgress) {
  ParkableString parkable(MakeLargeString('a').ReleaseImpl());
  String retained = parkable.ToString();

  EXPECT_GT(task_environment_.GetPendingMainThreadTaskCount(), 0u);
  WaitForAging();
  // The only string is referenced externally, nothing aging can change.
  CheckOnlyCpuCostTaskRemains();

  ParkableString parkable2(MakeLargeString('b').ReleaseImpl());
  WaitForAging();
  EXPECT_GT(task_environment_.GetPendingMainThreadTaskCount(), 0u);
  WaitForAging();
  EXPECT_GT(task_environment_.GetPendingMainThreadTaskCount(), 0u);
  EXPECT_TRUE(parkable2.Impl()->is_parked());
  EXPECT_GT(task_environment_.GetPendingMainThreadTaskCount(), 0u);
  WaitForAging();
  WaitForDiskWriting();
  // Once |parkable2| has been parked, back to the case where the only
  // remaining strings are referenced externally.
  CheckOnlyCpuCostTaskRemains();
}

// Flaky on a few platforms: crbug.com/1168170.
TEST_P(ParkableStringTest, DISABLED_OnlyOneAgingTask) {
  ParkableString parkable1(MakeLargeString('a').ReleaseImpl());
  ParkableString parkable2(MakeLargeString('b').ReleaseImpl());

  // Park both, and wait for the tick to stop.
  WaitForDelayedParking();
  EXPECT_TRUE(parkable1.Impl()->is_parked());
  EXPECT_TRUE(parkable2.Impl()->is_parked());
  WaitForDiskWriting();
  EXPECT_TRUE(parkable1.Impl()->is_on_disk());
  EXPECT_TRUE(parkable2.Impl()->is_on_disk());
  WaitForAging();
  CheckOnlyCpuCostTaskRemains();

  parkable1.ToString();
  parkable2.ToString();
  EXPECT_GT(task_environment_.GetPendingMainThreadTaskCount(), 0u);
  // Aging task + stats.
  EXPECT_EQ(2u, task_environment_.GetPendingMainThreadTaskCount());
}

TEST_P(ParkableStringTest, ReportTotalUnparkingTime) {
  base::ScopedMockElapsedTimersForTest mock_elapsed_timers;
  base::HistogramTester histogram_tester;

  // Disable on disk parking to keep data merely compressed, and report
  // compression metrics.
  DisableOnDiskWriting();

  ParkableString parkable(MakeLargeString().ReleaseImpl());
  ParkAndWait(parkable);

  // Iteration count: has to be low enough to end before the CPU cost task runs
  // (after 5 minutes), for both regular and less aggressive modes.
  const int kNumIterations = 4;
  for (int i = 0; i < kNumIterations; ++i) {
    parkable.ToString();
    ASSERT_FALSE(parkable.Impl()->is_parked());
    WaitForDelayedParking();
    ASSERT_TRUE(parkable.Impl()->is_parked());
    WaitForDiskWriting();
    WaitForAging();
    CheckOnlyCpuCostTaskRemains();
  }

  task_environment_.FastForwardUntilNoTasksRemain();
}

TEST_P(ParkableStringTest, ReportTotalDiskTime) {
  const size_t kCompressedSize = GetExpectedCompressedSize();

  base::ScopedMockElapsedTimersForTest mock_elapsed_timers;
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(features::IsParkableStringsToDiskEnabled());

  ParkableString parkable(MakeLargeString().ReleaseImpl());
  ParkAndWait(parkable);

  const int kNumIterations = 4;
  for (int i = 0; i < kNumIterations; ++i) {
    parkable.ToString();
    ASSERT_FALSE(parkable.Impl()->is_parked());
    WaitForDelayedParking();
    ASSERT_TRUE(parkable.Impl()->is_parked());
    WaitForDiskWriting();
    WaitForAging();
    CheckOnlyCpuCostTaskRemains();
  }

  task_environment_.FastForwardUntilNoTasksRemain();
  int64_t mock_elapsed_time_ms =
      base::ScopedMockElapsedTimersForTest::kMockElapsedTime.InMilliseconds();
  // String does not get to disk at the first iteration, hence "-1".
  histogram_tester.ExpectUniqueSample(
      "Memory.ParkableString.DiskReadTime.5min",
      mock_elapsed_time_ms * (kNumIterations - 1), 1);

  // The string is only written once despite the multiple parking/unparking
  // calls.
  histogram_tester.ExpectUniqueSample(
      "Memory.ParkableString.DiskWriteTime.5min", mock_elapsed_time_ms, 1);

  histogram_tester.ExpectUniqueSample("Memory.ParkableString.OnDiskSizeKb.5min",
                                      kCompressedSize / 1000, 1);
  histogram_tester.ExpectUniqueSample(
      "Memory.ParkableString.TotalUnparkingTime.5min",
      mock_elapsed_time_ms * kNumIterations - 1, 1);
  histogram_tester.ExpectUniqueSample(
      "Memory.ParkableString.TotalParkingThreadTime.5min", mock_elapsed_time_ms,
      1);
}

TEST_P(ParkableStringTest, EncodingAndDeduplication) {
  size_t size_in_chars = 2 * kSizeKb * 1000 / sizeof(UChar);
  Vector<UChar> data_16(size_in_chars);
  for (size_t i = 0; i < size_in_chars; ++i) {
    data_16[i] = 0x2020;
  }
  String large_string_16 = String(&data_16[0], size_in_chars);

  ParkableString parkable_16(large_string_16.Impl());
  ASSERT_TRUE(parkable_16.Impl()->digest());
  ASSERT_TRUE(parkable_16.may_be_parked());

  Vector<LChar> data_8(2 * size_in_chars);
  for (size_t i = 0; i < 2 * size_in_chars; ++i) {
    data_8[i] = 0x20;
  }
  String large_string_8 = String(&data_8[0], 2 * size_in_chars);

  ParkableString parkable_8(large_string_8.Impl());
  ASSERT_TRUE(parkable_8.Impl()->digest());
  ASSERT_TRUE(parkable_8.may_be_parked());

  // Same content, but the hash must be different because the encoding is.
  EXPECT_EQ(large_string_16.RawByteSpan(), large_string_8.RawByteSpan());
  EXPECT_NE(*parkable_16.Impl()->digest(), *parkable_8.Impl()->digest());
}

class ParkableStringTestWithQueuedThreadPool : public ParkableStringTest {
 public:
  ParkableStringTestWithQueuedThreadPool()
      : ParkableStringTest(ThreadPoolExecutionMode::QUEUED) {}
};

INSTANTIATE_TEST_SUITE_P(
    CompressionAlgorithm,
    ParkableStringTestWithQueuedThreadPool,
    ::testing::Values(ParkableStringImpl::CompressionAlgorithm::kZlib,
                      ParkableStringImpl::CompressionAlgorithm::kSnappy
#if BUILDFLAG(HAS_ZSTD_COMPRESSION)
                      ,
                      ParkableStringImpl::CompressionAlgorithm::kZstd
#endif  // BUILDFLAG(HAS_ZSTD_COMPRESSION)
                      ));

TEST_P(ParkableStringTestWithQueuedThreadPool, AgingParkingInProgress) {
  ParkableString parkable(MakeLargeString().ReleaseImpl());

  WaitForAging();
  parkable.Impl()->Park(ParkableStringImpl::ParkingMode::kCompress);

  // Advance the main thread until aging occurs. This uses RunLoop combined
  // with ThreadPoolExecutionMode::QUEUED to force the 2-seconds-delayed aging
  // task on the main thread to kick in before the immediate async compression
  // task completes.
  base::RunLoop run_loop;
  scheduler::GetSingleThreadTaskRunnerForTesting()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      ParkableStringManager::AgingInterval());
  run_loop.Run();

  // The aging task is rescheduled.
  EXPECT_EQ(2u, task_environment_.GetPendingMainThreadTaskCount());

  // Complete asynchronous work.
  RunPostedTasks();

  EXPECT_TRUE(parkable.Impl()->is_parked());
}

class ParkableStringTestWithLimitedDiskCapacity : public ParkableStringTest {
 public:
  ParkableStringTestWithLimitedDiskCapacity() {
    const std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {features::kCompressParkableStrings, {{"max_disk_capacity_mb", "1"}}}};
    features_.InitWithFeaturesAndParameters(enabled_features, {});
  }

 private:
  base::test::ScopedFeatureList features_;
};

INSTANTIATE_TEST_SUITE_P(
    CompressionAlgorithm,
    ParkableStringTestWithLimitedDiskCapacity,
    ::testing::Values(ParkableStringImpl::CompressionAlgorithm::kZlib,
                      ParkableStringImpl::CompressionAlgorithm::kSnappy
#if BUILDFLAG(HAS_ZSTD_COMPRESSION)
                      ,
                      ParkableStringImpl::CompressionAlgorithm::kZstd
#endif  // BUILDFLAG(HAS_ZSTD_COMPRESSION)
                      ));

TEST_P(ParkableStringTestWithLimitedDiskCapacity, ParkWithLimitedDiskCapacity) {
  constexpr size_t kMB = 1024 * 1024;
  {
    // Since compression rate is different, we cannot make a string for
    // same compressed data. So accumulate small compressed data until capacity
    // exceeds.
    Vector<ParkableString> strings;
    size_t total_written_compressed_data = 0;
    while (true) {
      ParkableString str(MakeComplexString(kMB).ReleaseImpl());
      WaitForDelayedParking();
      EXPECT_TRUE(str.Impl()->is_parked());

      if (total_written_compressed_data + str.Impl()->compressed_size() > kMB) {
        strings.push_back(str);
        break;
      }

      total_written_compressed_data += str.Impl()->compressed_size();
      WaitForDiskWriting();
      EXPECT_TRUE(str.Impl()->is_on_disk());
      strings.push_back(str);
    }
    WaitForDiskWriting();
    EXPECT_FALSE(strings.back().Impl()->is_on_disk());
  }

  // Since all the written data are discarded, we can write new string to disk.
  ParkableString parkable(MakeComplexString(kMB).ReleaseImpl());
  WaitForDelayedParking();
  EXPECT_TRUE(parkable.Impl()->is_parked());
  WaitForDiskWriting();
  EXPECT_TRUE(parkable.Impl()->is_on_disk());
}

class ParkableStringTestLessAggressiveMode : public ParkableStringTest {
 public:
  ParkableStringTestLessAggressiveMode()
      : features_(features::kLessAggressiveParkableString) {}

 private:
  base::test::ScopedFeatureList features_;
};

TEST_P(ParkableStringTestLessAggressiveMode, NoParkingInForeground) {
  auto& manager = ParkableStringManager::Instance();
  manager.SetRendererBackgrounded(false);

  ParkableString parkable(MakeLargeString().Impl());
  ASSERT_FALSE(parkable.Impl()->is_parked());
  EXPECT_EQ(1u, manager.Size());
  task_environment_.FastForwardBy(ParkableStringManager::kFirstParkingDelay);
  // No aging.
  EXPECT_EQ(ParkableStringImpl::Age::kYoung,
            parkable.Impl()->age_for_testing());
  EXPECT_FALSE(parkable.Impl()->is_parked());
  CheckOnlyCpuCostTaskRemains();

  manager.SetRendererBackgrounded(true);
  // A tick task has been posted.
  EXPECT_EQ(2u, task_environment_.GetPendingMainThreadTaskCount());
  // Aging restarts.
  WaitForAging();
  EXPECT_EQ(ParkableStringImpl::Age::kOld, parkable.Impl()->age_for_testing());
  manager.SetRendererBackgrounded(false);
  // Another task has been posted.
  EXPECT_EQ(2u, task_environment_.GetPendingMainThreadTaskCount());
  // But the string does not age further, since we are in foreground.
  WaitForAging();
  EXPECT_EQ(ParkableStringImpl::Age::kOld, parkable.Impl()->age_for_testing());
  EXPECT_FALSE(parkable.Impl()->is_parked());
  CheckOnlyCpuCostTaskRemains();

  // Back to foreground, pick up where we left off.
  manager.SetRendererBackgrounded(true);
  EXPECT_EQ(2u, task_environment_.GetPendingMainThreadTaskCount());
  WaitForAging();
  EXPECT_TRUE(parkable.Impl()->is_parked());
  WaitForDiskWriting();
  EXPECT_TRUE(parkable.Impl()->is_on_disk());
  // The tick eventually stops.
  WaitForAging();
  CheckOnlyCpuCostTaskRemains();
}

// Same test as the previous one, with RAIL mode transitions.
TEST_P(ParkableStringTestLessAggressiveMode, NoParkingWhileLoading) {
  auto& manager = ParkableStringManager::Instance();
  manager.OnRAILModeChanged(RAILMode::kLoad);

  ParkableString parkable(MakeLargeString().Impl());
  ASSERT_FALSE(parkable.Impl()->is_parked());
  EXPECT_EQ(1u, manager.Size());
  task_environment_.FastForwardBy(ParkableStringManager::kFirstParkingDelay);
  // No aging.
  EXPECT_EQ(ParkableStringImpl::Age::kYoung,
            parkable.Impl()->age_for_testing());
  EXPECT_FALSE(parkable.Impl()->is_parked());
  CheckOnlyCpuCostTaskRemains();

  manager.OnRAILModeChanged(RAILMode::kIdle);
  // A tick task has been posted.
  EXPECT_EQ(2u, task_environment_.GetPendingMainThreadTaskCount());
  // Aging restarts.
  WaitForAging();
  EXPECT_EQ(ParkableStringImpl::Age::kOld, parkable.Impl()->age_for_testing());
  manager.OnRAILModeChanged(RAILMode::kLoad);
  // Another task has been posted.
  EXPECT_EQ(2u, task_environment_.GetPendingMainThreadTaskCount());
  // But the string does not age further, since we are in foreground.
  WaitForAging();
  EXPECT_EQ(ParkableStringImpl::Age::kOld, parkable.Impl()->age_for_testing());
  EXPECT_FALSE(parkable.Impl()->is_parked());
  CheckOnlyCpuCostTaskRemains();

  // Back to idle, pick up where we left off.
  manager.OnRAILModeChanged(RAILMode::kIdle);
  EXPECT_EQ(2u, task_environment_.GetPendingMainThreadTaskCount());
  WaitForAging();
  EXPECT_TRUE(parkable.Impl()->is_parked());
  WaitForDiskWriting();
  EXPECT_TRUE(parkable.Impl()->is_on_disk());
  // The tick eventually stops.
  WaitForAging();
  CheckOnlyCpuCostTaskRemains();
}

// Combination of background and loading.
TEST_P(ParkableStringTestLessAggressiveMode,
       NoParkingWhileLoadingOrInForeground) {
  auto& manager = ParkableStringManager::Instance();
  // Loading in background.
  manager.OnRAILModeChanged(RAILMode::kLoad);
  manager.SetRendererBackgrounded(true);

  ParkableString parkable(MakeLargeString().Impl());
  ASSERT_FALSE(parkable.Impl()->is_parked());
  EXPECT_EQ(1u, manager.Size());
  task_environment_.FastForwardBy(ParkableStringManager::kFirstParkingDelay);
  // No aging.
  EXPECT_EQ(ParkableStringImpl::Age::kYoung,
            parkable.Impl()->age_for_testing());
  EXPECT_FALSE(parkable.Impl()->is_parked());
  CheckOnlyCpuCostTaskRemains();

  // Idle in foreground, no parking.
  manager.SetRendererBackgrounded(false);
  manager.OnRAILModeChanged(RAILMode::kIdle);
  CheckOnlyCpuCostTaskRemains();

  // Animation in foreground, no parking.
  manager.SetRendererBackgrounded(false);
  manager.OnRAILModeChanged(RAILMode::kAnimation);
  CheckOnlyCpuCostTaskRemains();

  // Not loading in background, restarting the tick.
  manager.SetRendererBackgrounded(true);
  manager.OnRAILModeChanged(RAILMode::kAnimation);
  // A tick task has been posted.
  EXPECT_EQ(2u, task_environment_.GetPendingMainThreadTaskCount());
  WaitForDelayedParking();
  EXPECT_TRUE(parkable.Impl()->is_parked());
  WaitForDiskWriting();
  EXPECT_TRUE(parkable.Impl()->is_on_disk());
  // The tick eventually stops.
  WaitForAging();
  CheckOnlyCpuCostTaskRemains();
}

INSTANTIATE_TEST_SUITE_P(
    CompressionAlgorithm,
    ParkableStringTestLessAggressiveMode,
    ::testing::Values(ParkableStringImpl::CompressionAlgorithm::kZlib,
                      ParkableStringImpl::CompressionAlgorithm::kSnappy
#if BUILDFLAG(HAS_ZSTD_COMPRESSION)
                      ,
                      ParkableStringImpl::CompressionAlgorithm::kZstd
#endif  // BUILDFLAG(HAS_ZSTD_COMPRESSION)
                      ));

}  // namespace blink
