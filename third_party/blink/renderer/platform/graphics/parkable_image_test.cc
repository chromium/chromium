// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/graphics/parkable_image.h"

#include "base/containers/heap_array.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/disk_data_allocator_test_utils.h"
#include "third_party/blink/renderer/platform/graphics/parkable_image_manager.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

using ThreadPoolExecutionMode =
    base::test::TaskEnvironment::ThreadPoolExecutionMode;

namespace blink {

namespace {
class LambdaThreadDelegate : public base::PlatformThread::Delegate {
 public:
  explicit LambdaThreadDelegate(base::OnceCallback<void()> f)
      : f_(std::move(f)) {}
  void ThreadMain() override { std::move(f_).Run(); }

 private:
  base::OnceCallback<void()> f_;
};
}  // namespace

// Parent for ParkableImageTest and ParkableImageNoParkingTest. The only
// difference between those two is whether parking is enabled or not.
class ParkableImageBaseTest : public ::testing::Test {
 public:
  ParkableImageBaseTest()
      : task_env_(base::test::TaskEnvironment::TimeSource::MOCK_TIME,
                  ThreadPoolExecutionMode::DEFAULT) {}

  void SetUp() override {
    auto& manager = ParkableImageManager::Instance();
    manager.ResetForTesting();
    auto tmp = std::make_unique<InMemoryDataAllocator>();
    allocator_for_testing_ = tmp.get();
    manager.SetDataAllocatorForTesting(std::move(tmp));
    manager.SetTaskRunnerForTesting(task_env_.GetMainThreadTaskRunner());
  }

  void TearDown() override {
    CHECK_EQ(ParkableImageManager::Instance().Size(), 0u);
    task_env_.FastForwardUntilNoTasksRemain();
  }

 protected:
  void WaitForParking() {
    task_env_.FastForwardBy(ParkableImageManager::kDelayedParkingInterval);
  }

  void WaitForDelayedParking() { task_env_.FastForwardBy(base::Seconds(30)); }

  // To aid in testing that the "Memory.ParkableImage.*.5min" metrics are
  // correctly recorded.
  void Wait5MinForStatistics() { task_env_.FastForwardBy(base::Minutes(5)); }

  void DescribeCurrentTasks() { task_env_.DescribeCurrentTasks(); }

  void RunPostedTasks() { task_env_.RunUntilIdle(); }

  size_t GetPendingMainThreadTaskCount() {
    return task_env_.GetPendingMainThreadTaskCount();
  }

  void set_may_write(bool may_write) {
    allocator_for_testing_->set_may_write_for_testing(may_write);
  }

  bool MaybePark(scoped_refptr<ParkableImage> pi) {
    return pi->impl_->MaybePark(task_env_.GetMainThreadTaskRunner());
  }
  static void Unpark(scoped_refptr<ParkableImage> pi) {
    base::AutoLock lock(pi->impl_->lock_);
    pi->impl_->Unpark();
  }
  static void Lock(scoped_refptr<ParkableImage> pi) {
    base::AutoLock lock(pi->impl_->lock_);
    pi->LockData();
  }
  static void Unlock(scoped_refptr<ParkableImage> pi) {
    base::AutoLock lock(pi->impl_->lock_);
    pi->UnlockData();
  }
  static bool is_on_disk(scoped_refptr<ParkableImage> pi) {
    base::AutoLock lock(pi->impl_->lock_);
    return pi->is_on_disk();
  }
  static bool is_locked(scoped_refptr<ParkableImage> pi) {
    base::AutoLock lock(pi->impl_->lock_);
    return pi->impl_->is_locked();
  }
  static bool is_frozen(scoped_refptr<ParkableImage> pi) {
    return pi->impl_->is_frozen();
  }

  scoped_refptr<ParkableImage> MakeParkableImageForTesting(base::span<const char> buffer) {
    auto pi = ParkableImage::Create();

    pi->Append(WTF::SharedBuffer::Create(buffer.data(), buffer.size()).get(), 0);

    return pi;
  }

  // Checks content matches the ParkableImage returned from
  // |MakeParkableImageForTesting|.
  static bool IsSameContent(scoped_refptr<ParkableImage> pi,
                            base::span<const char> buffer) {
    if (pi->size() != buffer.size()) {
      return false;
    }

    base::AutoLock lock(pi->impl_->lock_);
    pi->LockData();

    auto ro_buffer = pi->impl_->rw_buffer_->MakeROBufferSnapshot();
    ROBuffer::Iter iter(ro_buffer.get());
    const char* cur = buffer.data();
    do {
      if (memcmp(iter.data(), cur, iter.size()) != 0) {
        pi->UnlockData();
        return false;
      }
      cur += iter.size();
    } while (iter.Next());

    pi->UnlockData();
    return true;
  }

  // This checks that the "Memory.ParkableImage.Write.*" statistics from
  // |RecordReadStatistics()| are recorded correctly, namely
  // "Memory.ParkableImage.Write.Latency" and
  // "Memory.ParkableImage.Write.Size".
  //
  // Checks the counts for all 3 metrics, but only checks the value for
  // "Memory.ParkableImage.Write.Size", since the others can't be easily tested.
  void ExpectWriteStatistics(base::HistogramBase::Sample sample,
                             base::HistogramBase::Count expected_count) {
    histogram_tester_.ExpectTotalCount("Memory.ParkableImage.Write.Latency",
                                       expected_count);
    histogram_tester_.ExpectBucketCount("Memory.ParkableImage.Write.Size",
                                        sample, expected_count);
  }

  // This checks that the "Memory.ParkableImage.Read.*" statistics from
  // |RecordReadStatistics()| are recorded correctly, namely
  // "Memory.ParkableImage.Read.Latency", and
  // "Memory.ParkableImage.Read.Throughput".
  //
  // Checks the counts for both metrics, but not their values, since they can't
  // be easily tested.
  void ExpectReadStatistics(base::HistogramBase::Sample sample,
                            base::HistogramBase::Count expected_count) {
    histogram_tester_.ExpectTotalCount("Memory.ParkableImage.Read.Latency",
                                       expected_count);
    histogram_tester_.ExpectTotalCount("Memory.ParkableImage.Read.Throughput",
                                       expected_count);
  }

  base::HistogramTester histogram_tester_;

 private:
  base::test::TaskEnvironment task_env_;
  raw_ptr<InMemoryDataAllocator> allocator_for_testing_;
};

// Parking is enabled for these tests.
class ParkableImageTest : public ParkableImageBaseTest {
 public:
  ParkableImageTest() {
    fl_.InitWithFeatures({features::kParkableImagesToDisk},
                         {kDelayParkingImages});
  }

 private:
  base::test::ScopedFeatureList fl_;
};

// Parking is delayed but enabled for these tests.
class ParkableImageDelayedTest : public ParkableImageBaseTest {
 public:
  ParkableImageDelayedTest() {
    fl_.InitWithFeatures({features::kParkableImagesToDisk, kDelayParkingImages},
                         {});
  }

 private:
  base::test::ScopedFeatureList fl_;
};

// Parking is disabled for these tests.
class ParkableImageNoParkingTest : public ParkableImageBaseTest {
 public:
  ParkableImageNoParkingTest() {
    fl_.InitAndDisableFeature(features::kParkableImagesToDisk);
  }

 private:
  base::test::ScopedFeatureList fl_;
};

class ParkableImageWithLimitedDiskCapacityTest : public ParkableImageBaseTest {
 public:
  ParkableImageWithLimitedDiskCapacityTest() {
    const std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {features::kParkableImagesToDisk, {}},
        {features::kCompressParkableStrings, {{"max_disk_capacity_mb", "1"}}}};
    fl_.InitWithFeaturesAndParameters(enabled_features, {kDelayParkingImages});
  }

 private:
  base::test::ScopedFeatureList fl_;
};

// Tests that ParkableImages are constructed with the correct size.
TEST_F(ParkableImageTest, Size) {
  auto pi = ParkableImage::Create();

  EXPECT_EQ(pi->size(), 0u);

  // This has capacity 10, not size 10; size should still be 0.
  pi = ParkableImage::Create(10);

  EXPECT_EQ(pi->size(), 0u);
}

// Tests that |Freeze|ing a ParkableImage correctly updates its state.
TEST_F(ParkableImageTest, Frozen) {
  auto pi = ParkableImage::Create();
  ASSERT_EQ(pi->size(), 0u);

  // Starts unfrozen.
  EXPECT_FALSE(is_frozen(pi));

  pi->Freeze();

  EXPECT_TRUE(is_frozen(pi));
}

TEST_F(ParkableImageTest, LockAndUnlock) {
  auto pi = ParkableImage::Create();
  ASSERT_EQ(pi->size(), 0u);

  // ParkableImage starts unlocked.
  EXPECT_FALSE(is_locked(pi));

  Lock(pi);

  // Now locked after calling |Lock|.
  EXPECT_TRUE(is_locked(pi));

  Lock(pi);

  // Still locked after locking a second time.
  EXPECT_TRUE(is_locked(pi));

  Unlock(pi);

  // Still locked, we need to unlock a second time to unlock this.
  EXPECT_TRUE(is_locked(pi));

  Unlock(pi);

  // Now unlocked because we have locked twice then unlocked twice.
  EXPECT_FALSE(is_locked(pi));
}

// Tests that |Append|ing to a ParkableImage correctly adds data to it.
TEST_F(ParkableImageTest, Append) {
  const size_t kDataSize = 3.5 * 4096;
  auto data = base::HeapArray<char>::Uninit(kDataSize);
  PrepareReferenceData(data.data(), kDataSize);

  auto pi = ParkableImage::Create();
  ASSERT_EQ(pi->size(), 0u);  // Should be empty when created.

  pi->Append(WTF::SharedBuffer::Create(data.data(), data.size()).get(), 0);

  EXPECT_TRUE(IsSameContent(pi, data));
}

// Tests that multiple |Append|s correctly add data to the end of ParkableImage.
TEST_F(ParkableImageTest, AppendMultiple) {
  const size_t kDataSize = 3.5 * 4096;
  auto data = base::HeapArray<char>::Uninit(kDataSize);
  PrepareReferenceData(data.data(), kDataSize);

  auto pi = ParkableImage::Create();
  ASSERT_EQ(pi->size(), 0u);  // Should be empty when created.

  auto sb = WTF::SharedBuffer::Create(data.data(), data.size());
  ASSERT_EQ(sb->size(), kDataSize);

  pi->Append(sb.get(), 0);

  EXPECT_TRUE(IsSameContent(pi, data));

  sb->Append(data.data(), kDataSize);
  ASSERT_EQ(sb->size(), 2 * kDataSize);

  pi->Append(sb.get(), pi->size());

  EXPECT_EQ(pi->size(), 2 * kDataSize);
}

// Tests that we can read/write to disk correctly, preserving the data.
TEST_F(ParkableImageTest, ParkAndUnpark) {
  const size_t kDataSize = 3.5 * 4096;
  auto data = base::HeapArray<char>::Uninit(kDataSize);
  PrepareReferenceData(data.data(), kDataSize);

  // We have no images currently.
  ASSERT_EQ(0u, ParkableImageManager::Instance().Size());

  auto pi = MakeParkableImageForTesting(data);

  // We now have 1 image.
  ASSERT_EQ(1u, ParkableImageManager::Instance().Size());

  // Can't park because it is not frozen.
  EXPECT_FALSE(MaybePark(pi));

  // Should _not_ be on disk now.
  EXPECT_FALSE(is_on_disk(pi));

  pi->Freeze();

  // Parkable now that it's frozen.
  EXPECT_TRUE(MaybePark(pi));

  // Run task to park image.
  RunPostedTasks();

  // Should be on disk now.
  EXPECT_TRUE(is_on_disk(pi));

  Unpark(pi);

  // Unparking blocks until it is read from disk, so we expect it to no longer
  // be on disk after unparking.
  EXPECT_FALSE(is_on_disk(pi));

  // Make sure content is the same after unparking.
  EXPECT_TRUE(IsSameContent(pi, data));

  ExpectWriteStatistics(kDataSize / 1024, 1);
  ExpectReadStatistics(kDataSize / 1024, 1);
}

// Tests that trying to park multiple times doesn't add any extra tasks.
TEST_F(ParkableImageTest, ParkTwiceAndUnpark) {
  const size_t kDataSize = 3.5 * 4096;
  auto data = base::HeapArray<char>::Uninit(kDataSize);
  PrepareReferenceData(data.data(), kDataSize);

  // We have no images currently.
  ASSERT_EQ(0u, ParkableImageManager::Instance().Size());

  auto pi = MakeParkableImageForTesting(data);

  // We now have 1 image.
  ASSERT_EQ(1u, ParkableImageManager::Instance().Size());
  pi->Freeze();

  // Attempt to park the image twice in a row. This should have the same effect
  // as trying to park it once.
  EXPECT_TRUE(MaybePark(pi));
  EXPECT_TRUE(MaybePark(pi));

  // Run task to park image.
  RunPostedTasks();

  // Should be on disk now.
  EXPECT_TRUE(is_on_disk(pi));

  Unpark(pi);

  // Unparking blocks until it is read from disk, so we expect it to no longer
  // be on disk after unparking.
  EXPECT_FALSE(is_on_disk(pi));

  // Make sure content is the same after unparking.
  EXPECT_TRUE(IsSameContent(pi, data));

  ExpectWriteStatistics(kDataSize / 1024, 1);
  ExpectReadStatistics(kDataSize / 1024, 1);
}

// Tests that we can park to disk synchronously after the data is stored on
// disk the first time.
TEST_F(ParkableImageTest, ParkAndUnparkSync) {
  const size_t kDataSize = 3.5 * 4096;
  auto data = base::HeapArray<char>::Uninit(kDataSize);
  PrepareReferenceData(data.data(), kDataSize);

  // We have no images currently.
  ASSERT_EQ(0u, ParkableImageManager::Instance().Size());

  auto pi = MakeParkableImageForTesting(data);

  // We now have 1 image.
  ASSERT_EQ(1u, ParkableImageManager::Instance().Size());

  // Can't park because it is not frozen.
  EXPECT_FALSE(MaybePark(pi));

  // Should _not_ be on disk now.
  EXPECT_FALSE(is_on_disk(pi));

  pi->Freeze();

  // Parkable now that it's frozen.
  EXPECT_TRUE(MaybePark(pi));

  // Should not be on disk yet because we haven't run the tasks to write to disk
  // yet.
  EXPECT_FALSE(is_on_disk(pi));

  // Run task to park image.
  RunPostedTasks();

  // Should be on disk now.
  EXPECT_TRUE(is_on_disk(pi));

  Unpark(pi);

  ExpectWriteStatistics(kDataSize / 1024, 1);
  ExpectReadStatistics(kDataSize / 1024, 1);

  // Unparking blocks until it is read from disk, so we expect it to no longer
  // be on disk after unparking.
  EXPECT_FALSE(is_on_disk(pi));

  // Make sure content is the same after unparking.
  EXPECT_TRUE(IsSameContent(pi, data));

  // Try to park a second time.
  EXPECT_TRUE(MaybePark(pi));

  // We already have it on disk, so this time we just need to discard the data,
  // which can be done synchronously.
  EXPECT_TRUE(is_on_disk(pi));

  Unpark(pi);

  // Unparking blocks until it is read from disk, so we expect it to no longer
  // be on disk after unparking.
  EXPECT_FALSE(is_on_disk(pi));

  // Make sure content is the same after unparking.
  EXPECT_TRUE(IsSameContent(pi, data));

  // One extra read than write. We discard the data twice, but we only need to
  // write to disk once. Because we've discarded it twice, we need to do two
  // reads.
  ExpectWriteStatistics(kDataSize / 1024, 1);
  ExpectReadStatistics(kDataSize / 1024, 2);
}

// Tests that creating a snapshot partway through writing correctly aborts
// discarding the data.
TEST_F(ParkableImageTest, ParkAndUnparkAborted) {
  const size_t kDataSize = 3.5 * 4096;
  auto data = base::HeapArray<char>::Uninit(kDataSize);
  PrepareReferenceData(data.data(), kDataSize);

  // We have no images currently.
  ASSERT_EQ(0u, ParkableImageManager::Instance().Size());

  auto pi = MakeParkableImageForTesting(data);

  // We now have 1 image.
  ASSERT_EQ(1u, ParkableImageManager::Instance().Size());

  // Should _not_ be on disk now.
  ASSERT_FALSE(is_on_disk(pi));

  pi->Freeze();

  // Parkable now that it's frozen.
  EXPECT_TRUE(MaybePark(pi));

  auto snapshot = pi->MakeROSnapshot();
  snapshot->LockData();

  // Run task to park image.
  RunPostedTasks();

  // Should have been aborted, so still not on disk.
  EXPECT_FALSE(is_on_disk(pi));

  // Unparking after aborted write is fine.
  Unpark(pi);

  EXPECT_FALSE(is_on_disk(pi));

  // Make sure content is the same.
  EXPECT_TRUE(IsSameContent(pi, data));

  // We still expect a write to be done in this case, since the only thing
  // preventing it from being parked is the snapshot. However, the data is not
  // discarded here, since we need for the snapshot.
  //
  // Since the data was never discarded, we expect 0 reads however.
  ExpectWriteStatistics(kDataSize / 1024, 1);
  ExpectReadStatistics(kDataSize / 1024, 0);

  // Since we have a snapshot alive, we can't park.
  EXPECT_FALSE(MaybePark(pi));

  // kill the old snapshot.
  snapshot->UnlockData();
  snapshot = nullptr;

  // Now that snapshot is gone, we can park.
  EXPECT_TRUE(MaybePark(pi));

  RunPostedTasks();

  // Now parking can succeed.
  EXPECT_TRUE(is_on_disk(pi));

  // Unpark after successful write should also work.
  Unpark(pi);

  EXPECT_FALSE(is_on_disk(pi));

  // Make sure content is the same.
  EXPECT_TRUE(IsSameContent(pi, data));

  ExpectWriteStatistics(kDataSize / 1024, 1);
  ExpectReadStatistics(kDataSize / 1024, 1);
}

// Tests that a frozen image will be written to disk by the manager.
TEST_F(ParkableImageTest, ManagerSimple) {
  const size_t kDataSize = 3.5 * 4096;
  auto data = base::HeapArray<char>::Uninit(kDataSize);
  PrepareReferenceData(data.data(), kDataSize);

  auto& manager = ParkableImageManager::Instance();
  EXPECT_EQ(0u, manager.Size());

  auto pi = MakeParkableImageForTesting(data);
  pi->Freeze();

  EXPECT_EQ(1u, manager.Size());

  // One of these is the delayed parking task
  // |ParkableImageManager::MaybeParkImages|, the other is the delayed
  // accounting task |ParkableImageManager::RecordStatisticsAfter5Minutes|.
  EXPECT_EQ(2u, GetPendingMainThreadTaskCount());

  WaitForParking();

  // Image should be on disk now.
  EXPECT_TRUE(is_on_disk(pi));

  Unpark(pi);
  EXPECT_FALSE(is_on_disk(pi));

  WaitForParking();

  // Even though we unparked earlier, a new delayed parking task should park the
  // image still.
  EXPECT_TRUE(is_on_disk(pi));

  ExpectWriteStatistics(kDataSize / 1024, 1);
  ExpectReadStatistics(kDataSize / 1024, 1);
}

// Tests that a small image is not kept in the manager.
TEST_F(ParkableImageTest, ManagerSmall) {
  const size_t kDataSize = ParkableImageImpl::kMinSizeToPark - 10;
  auto data = base::HeapArray<char>::Uninit(kDataSize);
  PrepareReferenceData(data.data(), kDataSize);

  auto& manager = ParkableImageManager::Instance();
  EXPECT_EQ(0u, manager.Size());

  auto pi = MakeParkableImageForTesting(data);
  EXPECT_EQ(1u, manager.Size());

  pi->Freeze();

  // Image should now be removed from the manager.
  EXPECT_EQ(0u, manager.Size());

  // One of these is the delayed parking task
  // |ParkableImageManager::MaybeParkImages|, the other is the delayed
  // accounting task |ParkableImageManager::RecordStatisticsAfter5Minutes|.
  EXPECT_EQ(2u, GetPendingMainThreadTaskCount());

  WaitForParking();

  // Image should be on disk now.
  EXPECT_FALSE(is_on_disk(pi));
}

// Tests that the manager can correctly handle multiple parking tasks being
// created at once.
TEST_F(ParkableImageTest, ManagerTwo) {
  const size_t kDataSize = 3.5 * 4096;
  auto data = base::HeapArray<char>::Uninit(kDataSize);
  PrepareReferenceData(data.data(), kDataSize);

  auto& manager = ParkableImageManager::Instance();
  EXPECT_EQ(0u, manager.Size());

  auto pi = MakeParkableImageForTesting(data);
  pi->Freeze();

  EXPECT_EQ(1u, manager.Size());

  // One of these is the delayed parking task
  // |ParkableImageManager::MaybeParkImages|, the other is the delayed
  // accounting task |ParkableImageManager::RecordStatisticsAfter5Minutes|.
  EXPECT_EQ(2u, GetPendingMainThreadTaskCount());

  WaitForParking();

  // Image should be on disk now.
  EXPECT_TRUE(is_on_disk(pi));

  Unpark(pi);
  EXPECT_FALSE(is_on_disk(pi));

  WaitForParking();

  // Even though we unparked earlier, a new delayed parking task should park the
  // image still.
  EXPECT_TRUE(is_on_disk(pi));

  ExpectWriteStatistics(kDataSize / 1024, 1);
  ExpectReadStatistics(kDataSize / 1024, 1);
}

// Test that a non-frozen image will not be written to disk.
TEST_F(ParkableImageTest, ManagerNonFrozen) {
  const size_t kDataSize = 3.5 * 4096;
  auto data = base::HeapArray<char>::Uninit(kDataSize);
  PrepareReferenceData(data.data(), kDataSize);

  auto& manager = ParkableImageManager::Instance();
  EXPECT_EQ(0u, manager.Size());

  auto pi = MakeParkableImageForTesting(data);

  EXPECT_EQ(1u, manager.Size());

  // One of these is the delayed parking task
  // |ParkableImageManager::MaybeParkImages|, the other is the delayed
  // accounting task |ParkableImageManager::RecordStatisticsAfter5Minutes|.
  EXPECT_EQ(2u, GetPendingMainThreadTaskCount());

  WaitForParking();

  // Can't park because it is not frozen.
  EXPECT_FALSE(is_on_disk(pi));

  // No read or write was done, so we expect no metrics to be recorded for
  // reading/writing.
  ExpectWriteStatistics(0, 0);
  ExpectReadStatistics(0, 0);
}

// Check that trying to unpark a ParkableImage when parking is disabled has no
// effect.
TEST_F(ParkableImageNoParkingTest, Unpark) {
  const size_t kDataSize = 3.5 * 4096;
  auto data = base::HeapArray<char>::Uninit(kDataSize);
  PrepareReferenceData(data.data(), kDataSize);

  auto pi = MakeParkableImageForTesting(data);

  pi->Freeze();

  ASSERT_FALSE(is_on_disk(pi));

  // This is a no-op when parking is disabled.
  Unpark(pi);

  EXPECT_TRUE(IsSameContent(pi, data));

  // No data should be written or read when parking is disabled.
  ExpectWriteStatistics(kDataSize / 1024, 0);
  ExpectReadStatistics(kDataSize / 1024, 0);
}

// Tests that the ParkableImageManager is correctly recording statistics after 5
// minutes.
TEST_F(ParkableImageTest, ManagerStatistics5min) {
  const size_t kDataSize = 3.5 * 4096;
  auto data = base::HeapArray<char>::Uninit(kDataSize);
  PrepareReferenceData(data.data(), kDataSize);

  auto pi = MakeParkableImageForTesting(data);
  pi->Freeze();

  Wait5MinForStatistics();

  // We expect "Memory.ParkableImage.OnDiskFootprintKb.5min" not to be emitted,
  // since we've mocked the DiskDataAllocator for testing (and therefore cannot
  // actually write to disk).
  histogram_tester_.ExpectTotalCount(
      "Memory.ParkableImage.OnDiskFootprintKb.5min", 0);
  histogram_tester_.ExpectTotalCount("Memory.ParkableImage.OnDiskSize.5min", 1);
  histogram_tester_.ExpectTotalCount("Memory.ParkableImage.TotalReadTime.5min",
                                     1);
  histogram_tester_.ExpectTotalCount("Memory.ParkableImage.TotalSize.5min", 1);
  histogram_tester_.ExpectTotalCount("Memory.ParkableImage.TotalWriteTime.5min",
                                     1);
  histogram_tester_.ExpectTotalCount("Memory.ParkableImage.UnparkedSize.5min",
                                     1);
}

// Tests that the ParkableImageManager is correctly recording statistics after 5
// minutes, even when parking is disabled. Only bookkeeping metrics should be
// recorded in this case, since no reads/writes will happen.
TEST_F(ParkableImageNoParkingTest, ManagerStatistics5min) {
  const size_t kDataSize = 3.5 * 4096;
  auto data = base::HeapArray<char>::Uninit(kDataSize);
  PrepareReferenceData(data.data(), kDataSize);

  auto pi = MakeParkableImageForTesting(data);
  pi->Freeze();

  Wait5MinForStatistics();

  // Note that we expect 0 counts of some of these metrics.
  histogram_tester_.ExpectTotalCount(
      "Memory.ParkableImage.OnDiskFootprintKb.5min", 0);
  histogram_tester_.ExpectTotalCount("Memory.ParkableImage.OnDiskSize.5min", 1);
  histogram_tester_.ExpectTotalCount("Memory.ParkableImage.TotalReadTime.5min",
                                     0);
  histogram_tester_.ExpectTotalCount("Memory.ParkableImage.TotalSize.5min", 1);
  histogram_tester_.ExpectTotalCount("Memory.ParkableImage.TotalWriteTime.5min",
                                     0);
  histogram_tester_.ExpectTotalCount("Memory.ParkableImage.UnparkedSize.5min",
                                     1);
}

// Tests that the manager doesn't try to park any images when parking is
// disabled.
TEST_F(ParkableImageNoParkingTest, ManagerSimple) {
  const size_t kDataSize = 3.5 * 4096;
  auto data = base::HeapArray<char>::Uninit(kDataSize);
  PrepareReferenceData(data.data(), kDataSize);

  auto pi = MakeParkableImageForTesting(data);

  auto& manager = ParkableImageManager::Instance();
  // The manager still keeps track of all images when parking is disabled, but
  // should not park them.
  EXPECT_EQ(1u, manager.Size());

  pi->Freeze();

  // This is the delayed
  // accounting task |ParkableImageManager::RecordStatisticsAfter5Minutes|.
  EXPECT_EQ(1u, GetPendingMainThreadTaskCount());

  // This should not do anything, since parking is disabled.
  WaitForParking();

  EXPECT_FALSE(is_on_disk(pi));

  EXPECT_TRUE(IsSameContent(pi, data));

  // No data should be written or read when parking is disabled.
  ExpectWriteStatistics(kDataSize / 1024, 0);
  ExpectReadStatistics(kDataSize / 1024, 0);
}

// Test a locked image will not be written to disk.
TEST_F(ParkableImageTest, ManagerNotUnlocked) {
  const size_t kDataSize = 3.5 * 4096;
  auto data = base::HeapArray<char>::Uninit(kDataSize);
  PrepareReferenceData(data.data(), kDataSize);

  auto& manager = ParkableImageManager::Instance();
  EXPECT_EQ(0u, manager.Size());

  auto pi = MakeParkableImageForTesting(data);

  EXPECT_EQ(1u, manager.Size());

  // Freeze, so it would be Parkable (if not for the Lock right after this
  // line).
  pi->Freeze();
  Lock(pi);

  WaitForParking();

  // Can't park because it is locked.
  EXPECT_FALSE(is_on_disk(pi));

  Unlock(pi);
}

// Tests that the manager only reschedules the parking task  when there are
// unfrozen ParkableImages.
TEST_F(ParkableImageTest, ManagerRescheduleUnfrozen) {
  const size_t kDataSize = 3.5 * 4096;
  auto data = base::HeapArray<char>::Uninit(kDataSize);
  PrepareReferenceData(data.data(), kDataSize);

  auto& manager = ParkableImageManager::Instance();
  EXPECT_EQ(0u, manager.Size());

  auto pi = MakeParkableImageForTesting(data);

  // This is the delayed
  // accounting task |ParkableImageManager::RecordStatisticsAfter5Minutes|, and
  // the parking task.
  EXPECT_EQ(2u, GetPendingMainThreadTaskCount());

  // Fast forward enough for both to run.
  Wait5MinForStatistics();
  WaitForParking();

  // Unfrozen ParkableImages are never parked.
  ASSERT_FALSE(is_on_disk(pi));

  // We have rescheduled the task because we have unfrozen ParkableImages.
  EXPECT_EQ(1u, GetPendingMainThreadTaskCount());

  pi->Freeze();
  Lock(pi);

  WaitForParking();

  // Locked ParkableImages are never parked.
  ASSERT_FALSE(is_on_disk(pi));

  // We do no reschedule because there are no un-frozen ParkableImages.
  EXPECT_EQ(0u, GetPendingMainThreadTaskCount());

  Unlock(pi);
}

// We want to test that trying to delete an image while we try to park it works
// correctly. The expected behaviour is we park it, then delete. Slightly
// inefficient, but the safest way to do it.
TEST_F(ParkableImageTest, DestroyOnSeparateThread) {
  const size_t kDataSize = 3.5 * 4096;
  auto data = base::HeapArray<char>::Uninit(kDataSize);
  PrepareReferenceData(data.data(), kDataSize);

  auto& manager = ParkableImageManager::Instance();
  EXPECT_EQ(0u, manager.Size());

  auto pi = MakeParkableImageForTesting(data);
  EXPECT_EQ(1u, manager.Size());

  Wait5MinForStatistics();

  pi->Freeze();

  // Task for parking the image.
  EXPECT_EQ(1u, GetPendingMainThreadTaskCount());

  LambdaThreadDelegate delegate{
      base::BindLambdaForTesting([parkable_image = std::move(pi)]() mutable {
        EXPECT_TRUE(!IsMainThread());
        // We destroy the ParkableImage here, on a different thread. This will
        // post a task to the main thread to actually delete it.
        parkable_image = nullptr;
      })};

  base::PlatformThreadHandle thread_handle;
  base::PlatformThread::Create(0, &delegate, &thread_handle);
  base::PlatformThread::Join(thread_handle);

  ASSERT_EQ(pi, nullptr);

  // The manager is still aware of the ParkableImage, since the task for
  // deleting it hasn't been run yet.
  EXPECT_EQ(1u, manager.Size());
  // Task for parking image, followed by task for deleting the image.
  EXPECT_EQ(2u, GetPendingMainThreadTaskCount());

  WaitForParking();

  // Now that the tasks for deleting and parking have run, the image is deleted.
  EXPECT_EQ(0u, manager.Size());
  EXPECT_EQ(0u, GetPendingMainThreadTaskCount());
}

TEST_F(ParkableImageTest, FailedWrite) {
  auto& manager = ParkableImageManager::Instance();
  set_may_write(false);

  const size_t kDataSize = 3.5 * 4096;
  auto data = base::HeapArray<char>::Uninit(kDataSize);
  PrepareReferenceData(data.data(), kDataSize);

  EXPECT_EQ(0u, manager.Size());

  WaitForParking();

  {
    auto pi = MakeParkableImageForTesting(data);
    pi->Freeze();
    manager.MaybeParkImagesForTesting();
    EXPECT_EQ(1u, manager.Size());
  }

  WaitForParking();

  EXPECT_EQ(0u, manager.Size());
}

// Test that we park only after 30 seconds, not immediately after freezing.
TEST_F(ParkableImageDelayedTest, Simple) {
  const size_t kDataSize = 3.5 * 4096;
  auto data = base::HeapArray<char>::Uninit(kDataSize);
  PrepareReferenceData(data.data(), kDataSize);

  auto& manager = ParkableImageManager::Instance();
  EXPECT_EQ(0u, manager.Size());

  auto pi = MakeParkableImageForTesting(data);
  EXPECT_EQ(1u, manager.Size());

  Wait5MinForStatistics();

  pi->Freeze();

  EXPECT_EQ(1u, GetPendingMainThreadTaskCount());

  WaitForParking();

  // We have 1 task still, since we need to wait for 30 seconds after the image
  // has been frozen.
  EXPECT_EQ(1u, GetPendingMainThreadTaskCount());
  EXPECT_FALSE(is_on_disk(pi));

  WaitForDelayedParking();

  // After waiting 30 seconds, the image is parked.
  EXPECT_EQ(0u, GetPendingMainThreadTaskCount());

  EXPECT_TRUE(is_on_disk(pi));
}

// Test that we park only after 30 seconds or once we have read the data, not
// immediately after freezing.
TEST_F(ParkableImageDelayedTest, Read) {
  const size_t kDataSize = 3.5 * 4096;
  auto data = base::HeapArray<char>::Uninit(kDataSize);
  PrepareReferenceData(data.data(), kDataSize);

  auto& manager = ParkableImageManager::Instance();
  EXPECT_EQ(0u, manager.Size());

  auto pi = MakeParkableImageForTesting(data);
  EXPECT_EQ(1u, manager.Size());

  Wait5MinForStatistics();

  pi->Freeze();

  EXPECT_EQ(1u, GetPendingMainThreadTaskCount());

  WaitForParking();

  // We have 1 task still, since we need to wait for 30 seconds after the image
  // has been frozen.
  EXPECT_EQ(1u, GetPendingMainThreadTaskCount());
  EXPECT_FALSE(is_on_disk(pi));

  // Read the data here, which allows us to park the image immediately.
  pi->Data();

  WaitForParking();

  // Image is successfully parked, even though it's been less than 30 seconds.
  EXPECT_EQ(0u, GetPendingMainThreadTaskCount());
  EXPECT_TRUE(is_on_disk(pi));
}

// 30 seconds should be counted from when we freeze, and not be affected by
// parking/unparking.
TEST_F(ParkableImageDelayedTest, ParkAndUnpark) {
  const size_t kDataSize = 3.5 * 4096;
  auto data = base::HeapArray<char>::Uninit(kDataSize);
  PrepareReferenceData(data.data(), kDataSize);

  // We have no images currently.
  ASSERT_EQ(0u, ParkableImageManager::Instance().Size());

  auto pi = MakeParkableImageForTesting(data);

  // We now have 1 image.
  ASSERT_EQ(1u, ParkableImageManager::Instance().Size());

  pi->Freeze();

  WaitForParking();

  EXPECT_FALSE(is_on_disk(pi));

  WaitForDelayedParking();

  EXPECT_TRUE(is_on_disk(pi));

  Unpark(pi);

  // Unparking blocks until it is read from disk, so we expect it to no longer
  // be on disk after unparking.
  EXPECT_FALSE(is_on_disk(pi));

  // Make sure content is the same after unparking.
  EXPECT_TRUE(IsSameContent(pi, data));

  WaitForParking();

  // No need to wait 30 more seconds, we can park immediately.
  EXPECT_TRUE(is_on_disk(pi));
}

TEST_F(ParkableImageWithLimitedDiskCapacityTest, ParkWithLimitedDiskCapacity) {
  constexpr size_t kMB = 1024 * 1024;
  constexpr size_t kDataSize = kMB;
  auto data = base::HeapArray<char>::Uninit(kDataSize);
  PrepareReferenceData(data.data(), kDataSize);

  auto pi = MakeParkableImageForTesting(data);
  pi->Freeze();
  EXPECT_TRUE(MaybePark(pi));
  RunPostedTasks();
  EXPECT_TRUE(is_on_disk(pi));

  // Create another parkable image and attempt to write to disk.
  auto pi2 = MakeParkableImageForTesting(data);
  pi2->Freeze();
  // Should be false because there is no free space.
  EXPECT_FALSE(MaybePark(pi2));

  // Remove first parkable image. Now we can park second image.
  pi = nullptr;
  EXPECT_TRUE(MaybePark(pi2));
  RunPostedTasks();
  EXPECT_TRUE(is_on_disk(pi2));
}

}  // namespace blink
