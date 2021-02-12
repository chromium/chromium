// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/parkable_image.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/disk_data_allocator_test_utils.h"
#include "third_party/blink/renderer/platform/graphics/parkable_image_manager.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

using ThreadPoolExecutionMode =
    base::test::TaskEnvironment::ThreadPoolExecutionMode;

namespace blink {

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
    manager.SetDataAllocatorForTesting(
        std::make_unique<InMemoryDataAllocator>());
  }

  void TearDown() override {
    CHECK_EQ(ParkableImageManager::Instance().Size(), 0u);
    task_env_.FastForwardUntilNoTasksRemain();
  }

 protected:
  void WaitForDelayedParking() {
    task_env_.FastForwardBy(ParkableImageManager::kDelayedParkingInterval);
  }

  void DescribeCurrentTasks() { task_env_.DescribeCurrentTasks(); }

  void RunPostedTasks() { task_env_.RunUntilIdle(); }

  size_t GetPendingMainThreadTaskCount() {
    return task_env_.GetPendingMainThreadTaskCount();
  }

  static bool MaybePark(scoped_refptr<ParkableImage> pi) {
    return pi->MaybePark();
  }
  static void Unpark(scoped_refptr<ParkableImage> pi) {
    MutexLocker lock(pi->lock_);
    pi->Unpark();
  }
  static bool is_on_disk(scoped_refptr<ParkableImage> pi) {
    MutexLocker lock(pi->lock_);
    return pi->is_on_disk();
  }

  scoped_refptr<ParkableImage> MakeParkableImageForTesting(const char* buffer,
                                                           size_t length) {
    auto pi = ParkableImage::Create();

    pi->Append(WTF::SharedBuffer::Create(buffer, length).get(), 0);

    return pi;
  }

  // Checks content matches the ParkableImage returned from
  // |MakeParkableImageForTesting|.
  static bool IsSameContent(scoped_refptr<ParkableImage> pi,
                            const char* buffer,
                            size_t length) {
    if (pi->size() != length) {
      return false;
    }

    MutexLocker lock(pi->lock_);
    auto ro_buffer = pi->rw_buffer_->MakeROBufferSnapshot();
    ROBuffer::Iter iter(ro_buffer.get());
    do {
      if (memcmp(iter.data(), buffer, iter.size()) != 0) {
        return false;
      }
      buffer += iter.size();
    } while (iter.Next());

    return true;
  }

 private:
  base::test::TaskEnvironment task_env_;
};

// Parking is enabled for these tests.
class ParkableImageTest : public ParkableImageBaseTest {
 public:
  ParkableImageTest() { fl_.InitAndEnableFeature(kParkableImagesToDisk); }

 private:
  base::test::ScopedFeatureList fl_;
};

// Parking is disabled for these tests.
class ParkableImageNoParkingTest : public ParkableImageBaseTest {
 public:
  ParkableImageNoParkingTest() {
    fl_.InitAndDisableFeature(kParkableImagesToDisk);
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
  EXPECT_FALSE(pi->is_frozen());

  pi->Freeze();

  EXPECT_TRUE(pi->is_frozen());
}

// Tests that |Append|ing to a ParkableImage correctly adds data to it.
TEST_F(ParkableImageTest, Append) {
  const size_t kDataSize = 3.5 * 4096;
  char data[kDataSize];
  PrepareReferenceData(data, kDataSize);

  auto pi = ParkableImage::Create();
  ASSERT_EQ(pi->size(), 0u);  // Should be empty when created.

  pi->Append(WTF::SharedBuffer::Create(data, kDataSize).get(), 0);

  EXPECT_TRUE(IsSameContent(pi, data, kDataSize));
}

// Tests that multiple |Append|s correctly add data to the end of ParkableImage.
TEST_F(ParkableImageTest, AppendMultiple) {
  const size_t kDataSize = 3.5 * 4096;
  char data[kDataSize];
  PrepareReferenceData(data, kDataSize);

  auto pi = ParkableImage::Create();
  ASSERT_EQ(pi->size(), 0u);  // Should be empty when created.

  auto sb = WTF::SharedBuffer::Create(data, kDataSize);
  ASSERT_EQ(sb->size(), kDataSize);

  pi->Append(sb.get(), 0);

  EXPECT_TRUE(IsSameContent(pi, data, kDataSize));

  sb->Append(data, kDataSize);
  ASSERT_EQ(sb->size(), 2 * kDataSize);

  pi->Append(sb.get(), pi->size());

  EXPECT_EQ(pi->size(), 2 * kDataSize);
}

// Tests that we can read/write to disk correctly, preserving the data.
TEST_F(ParkableImageTest, ParkAndUnpark) {
  const size_t kDataSize = 3.5 * 4096;
  char data[kDataSize];
  PrepareReferenceData(data, kDataSize);

  // We have no images currently.
  ASSERT_EQ(0u, ParkableImageManager::Instance().Size());

  auto pi = MakeParkableImageForTesting(data, kDataSize);

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
  EXPECT_TRUE(IsSameContent(pi, data, kDataSize));
}

// Tests that trying to park multiple times doesn't add any extra tasks.
TEST_F(ParkableImageTest, ParkTwiceAndUnpark) {
  const size_t kDataSize = 3.5 * 4096;
  char data[kDataSize];
  PrepareReferenceData(data, kDataSize);

  // We have no images currently.
  ASSERT_EQ(0u, ParkableImageManager::Instance().Size());

  auto pi = MakeParkableImageForTesting(data, kDataSize);

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
  EXPECT_TRUE(IsSameContent(pi, data, kDataSize));
}

// Tests that we can park to disk synchronously after the data is stored on
// disk the first time.
TEST_F(ParkableImageTest, ParkAndUnparkSync) {
  const size_t kDataSize = 3.5 * 4096;
  char data[kDataSize];
  PrepareReferenceData(data, kDataSize);

  // We have no images currently.
  ASSERT_EQ(0u, ParkableImageManager::Instance().Size());

  auto pi = MakeParkableImageForTesting(data, kDataSize);

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

  // Unparking blocks until it is read from disk, so we expect it to no longer
  // be on disk after unparking.
  EXPECT_FALSE(is_on_disk(pi));

  // Make sure content is the same after unparking.
  EXPECT_TRUE(IsSameContent(pi, data, kDataSize));

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
  EXPECT_TRUE(IsSameContent(pi, data, kDataSize));
}

// Tests that creating a snapshot partway through writing correctly aborts
// discarding the data.
TEST_F(ParkableImageTest, ParkAndUnparkAborted) {
  const size_t kDataSize = 3.5 * 4096;
  char data[kDataSize];
  PrepareReferenceData(data, kDataSize);

  // We have no images currently.
  ASSERT_EQ(0u, ParkableImageManager::Instance().Size());

  auto pi = MakeParkableImageForTesting(data, kDataSize);

  // We now have 1 image.
  ASSERT_EQ(1u, ParkableImageManager::Instance().Size());

  // Should _not_ be on disk now.
  ASSERT_FALSE(is_on_disk(pi));

  pi->Freeze();

  // Parkable now that it's frozen.
  EXPECT_TRUE(MaybePark(pi));

  auto snapshot = pi->MakeROSnapshot();

  // Run task to park image.
  RunPostedTasks();

  // Should have been aborted, so still not on disk.
  EXPECT_FALSE(is_on_disk(pi));

  // Unparking after aborted write is fine.
  Unpark(pi);

  EXPECT_FALSE(is_on_disk(pi));

  // Make sure content is the same.
  EXPECT_TRUE(IsSameContent(pi, data, kDataSize));

  // Since we have a snapshot alive, we can't park.
  EXPECT_FALSE(MaybePark(pi));

  // kill the old snapshot.
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
  EXPECT_TRUE(IsSameContent(pi, data, kDataSize));
}

// Tests that a frozen image will be written to disk by the manager.
TEST_F(ParkableImageTest, ManagerSimple) {
  const size_t kDataSize = 3.5 * 4096;
  char data[kDataSize];
  PrepareReferenceData(data, kDataSize);

  auto& manager = ParkableImageManager::Instance();
  EXPECT_EQ(0u, manager.Size());

  auto pi = MakeParkableImageForTesting(data, kDataSize);
  pi->Freeze();

  EXPECT_EQ(1u, manager.Size());

  // One of these is the delayed parking task
  // |ParkableImageManager::MaybeParkImages|, the other is the delayed
  // accounting task |ParkableImageManager::RecordStatisticsAfter5Minutes|.
  EXPECT_EQ(2u, GetPendingMainThreadTaskCount());

  WaitForDelayedParking();

  // Image should be on disk now.
  EXPECT_TRUE(is_on_disk(pi));

  Unpark(pi);
  EXPECT_FALSE(is_on_disk(pi));

  WaitForDelayedParking();

  // Even though we unparked earlier, a new delayed parking task should park the
  // image still.
  EXPECT_TRUE(is_on_disk(pi));
}

// Tests that the manager can correctly handle multiple parking tasks being
// created at once.
TEST_F(ParkableImageTest, ManagerTwo) {
  const size_t kDataSize = 3.5 * 4096;
  char data[kDataSize];
  PrepareReferenceData(data, kDataSize);

  auto& manager = ParkableImageManager::Instance();
  EXPECT_EQ(0u, manager.Size());

  auto pi = MakeParkableImageForTesting(data, kDataSize);
  pi->Freeze();

  EXPECT_EQ(1u, manager.Size());

  // One of these is the delayed parking task
  // |ParkableImageManager::MaybeParkImages|, the other is the delayed
  // accounting task |ParkableImageManager::RecordStatisticsAfter5Minutes|.
  EXPECT_EQ(2u, GetPendingMainThreadTaskCount());

  WaitForDelayedParking();

  // Image should be on disk now.
  EXPECT_TRUE(is_on_disk(pi));

  Unpark(pi);
  EXPECT_FALSE(is_on_disk(pi));

  WaitForDelayedParking();

  // Even though we unparked earlier, a new delayed parking task should park the
  // image still.
  EXPECT_TRUE(is_on_disk(pi));
}

// Test that a non-frozen image will not be written to disk.
TEST_F(ParkableImageTest, ManagerNonFrozen) {
  const size_t kDataSize = 3.5 * 4096;
  char data[kDataSize];
  PrepareReferenceData(data, kDataSize);

  auto& manager = ParkableImageManager::Instance();
  EXPECT_EQ(0u, manager.Size());

  auto pi = MakeParkableImageForTesting(data, kDataSize);

  EXPECT_EQ(1u, manager.Size());

  // One of these is the delayed parking task
  // |ParkableImageManager::MaybeParkImages|, the other is the delayed
  // accounting task |ParkableImageManager::RecordStatisticsAfter5Minutes|.
  EXPECT_EQ(2u, GetPendingMainThreadTaskCount());

  WaitForDelayedParking();

  // Can't park because it is not frozen.
  EXPECT_FALSE(is_on_disk(pi));
}

// Check that trying to unpark a ParkableImage when parking is disabled has no
// effect.
TEST_F(ParkableImageNoParkingTest, Unpark) {
  const size_t kDataSize = 3.5 * 4096;
  char data[kDataSize];
  PrepareReferenceData(data, kDataSize);

  auto pi = MakeParkableImageForTesting(data, kDataSize);

  pi->Freeze();

  ASSERT_FALSE(is_on_disk(pi));

  // This is a no-op when parking is disabled.
  Unpark(pi);

  EXPECT_TRUE(IsSameContent(pi, data, kDataSize));
}

// Tests that the manager doesn't try to park any images when parking is
// disabled.
TEST_F(ParkableImageNoParkingTest, ManagerSimple) {
  const size_t kDataSize = 3.5 * 4096;
  char data[kDataSize];
  PrepareReferenceData(data, kDataSize);

  auto pi = MakeParkableImageForTesting(data, kDataSize);

  auto& manager = ParkableImageManager::Instance();
  // The manager still keeps track of all images when parking is disabled, but
  // should not park them.
  EXPECT_EQ(1u, manager.Size());

  pi->Freeze();

  // This is the delayed
  // accounting task |ParkableImageManager::RecordStatisticsAfter5Minutes|.
  EXPECT_EQ(1u, GetPendingMainThreadTaskCount());

  // This should not do anything, since parking is disabled.
  WaitForDelayedParking();

  EXPECT_FALSE(is_on_disk(pi));

  EXPECT_TRUE(IsSameContent(pi, data, kDataSize));
}

}  // namespace blink
