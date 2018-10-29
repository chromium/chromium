// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/parkable_string.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/platform/bindings/parkable_string_manager.h"

#include <thread>
#include <vector>

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

constexpr size_t kSizeKb = 20;

String MakeLargeString() {
  std::vector<char> data(kSizeKb * 1000, 'a');
  return String(String(data.data(), data.size()).ReleaseImpl());
}

void RunPostedTasks() {
  base::RunLoop run_loop;
  blink::scheduler::GetSingleThreadTaskRunnerForTesting()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

bool ParkAndWait(const ParkableString& string) {
  bool return_value = string.Impl()->Park();
  RunPostedTasks();
  return return_value;
}

}  // namespace

class ParkableStringTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ParkableStringManager::Instance().SetRendererBackgrounded(false);
    scoped_feature_list_.InitAndEnableFeature(
        kCompressParkableStringsInBackground);
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ParkableStringTest, Simple) {
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

TEST_F(ParkableStringTest, Park) {
  base::HistogramTester histogram_tester;

  {
    ParkableString parkable(MakeLargeString().ReleaseImpl());
    EXPECT_TRUE(parkable.may_be_parked());
    EXPECT_FALSE(parkable.Impl()->is_parked());
    EXPECT_TRUE(ParkAndWait(parkable));
    EXPECT_TRUE(parkable.Impl()->is_parked());
  }

  String large_string = MakeLargeString();
  ParkableString parkable(large_string.Impl());
  EXPECT_TRUE(parkable.may_be_parked());
  // Not the only one to have a reference to the string.
  EXPECT_FALSE(ParkAndWait(parkable));
  large_string = String();
  EXPECT_TRUE(ParkAndWait(parkable));

  histogram_tester.ExpectBucketCount(
      "Memory.MovableStringParkingAction",
      ParkableStringImpl::ParkingAction::kParkedInBackground, 2);
  histogram_tester.ExpectTotalCount("Memory.MovableStringParkingAction", 2);

  {
    ParkableString parkable(MakeLargeString().ReleaseImpl());
    EXPECT_TRUE(parkable.may_be_parked());
    EXPECT_FALSE(parkable.Impl()->is_parked());
    EXPECT_TRUE(parkable.Impl()->Park());
    parkable = ParkableString();  // release the reference.
    RunPostedTasks();             // Should not crash.
  }
}

TEST_F(ParkableStringTest, AbortParking) {
  ParkableString parkable(MakeLargeString().ReleaseImpl());
  ParkableStringImpl* impl = parkable.Impl();

  EXPECT_TRUE(parkable.may_be_parked());
  EXPECT_FALSE(impl->is_parked());

  // The string is locked at the end of parking, should cancel it.
  EXPECT_TRUE(impl->Park());
  impl->Lock();
  RunPostedTasks();
  EXPECT_FALSE(impl->is_parked());

  // Unlock, OK to park.
  impl->Unlock();
  EXPECT_TRUE(ParkAndWait(parkable));
  impl->ToString();

  // |ToString()| cancels unparking as |content| is kept alive.
  EXPECT_TRUE(impl->Park());
  String content = impl->ToString();
  RunPostedTasks();
  EXPECT_FALSE(impl->is_parked());
  content = String();
  EXPECT_TRUE(ParkAndWait(parkable));
  impl->ToString();

  // Transient |Lock()| or |ToString()| doesn't cancel parking.
  EXPECT_TRUE(impl->Park());
  impl->Lock();
  impl->ToString();
  impl->Unlock();
  RunPostedTasks();
  EXPECT_TRUE(impl->is_parked());
}

TEST_F(ParkableStringTest, Unpark) {
  base::HistogramTester histogram_tester;

  ParkableString parkable(MakeLargeString().Impl());
  String unparked_copy = parkable.ToString().IsolatedCopy();
  EXPECT_TRUE(parkable.may_be_parked());
  EXPECT_FALSE(parkable.Impl()->is_parked());
  EXPECT_TRUE(ParkAndWait(parkable));
  EXPECT_TRUE(parkable.Impl()->is_parked());

  String unparked = parkable.ToString();
  EXPECT_EQ(unparked_copy, unparked);
  EXPECT_FALSE(parkable.Impl()->is_parked());

  histogram_tester.ExpectTotalCount("Memory.MovableStringParkingAction", 2);
  histogram_tester.ExpectBucketCount(
      "Memory.MovableStringParkingAction",
      ParkableStringImpl::ParkingAction::kParkedInBackground, 1);
  histogram_tester.ExpectBucketCount(
      "Memory.MovableStringParkingAction",
      ParkableStringImpl::ParkingAction::kUnparkedInForeground, 1);
}

TEST_F(ParkableStringTest, LockUnlock) {
  ParkableString parkable(MakeLargeString().Impl());
  ParkableStringImpl* impl = parkable.Impl();
  EXPECT_EQ(0, impl->lock_depth_);

  parkable.Lock();
  EXPECT_EQ(1, impl->lock_depth_);
  parkable.Lock();
  parkable.Unlock();
  EXPECT_EQ(1, impl->lock_depth_);
  parkable.Unlock();
  EXPECT_EQ(0, impl->lock_depth_);

  parkable.Lock();
  EXPECT_FALSE(ParkAndWait(parkable));
  parkable.Unlock();
  EXPECT_TRUE(ParkAndWait(parkable));

  parkable.ToString();
  std::thread t([&]() { parkable.Lock(); });
  t.join();
  EXPECT_FALSE(ParkAndWait(parkable));
  parkable.Unlock();
  EXPECT_TRUE(ParkAndWait(parkable));
}

TEST_F(ParkableStringTest, LockParkedString) {
  ParkableString parkable(MakeLargeString().Impl());
  ParkableStringImpl* impl = parkable.Impl();
  EXPECT_EQ(0, impl->lock_depth_);
  EXPECT_TRUE(ParkAndWait(parkable));

  parkable.Lock();  // Locking doesn't unpark.
  EXPECT_TRUE(impl->is_parked());
  parkable.ToString();
  EXPECT_FALSE(impl->is_parked());
  EXPECT_EQ(1, impl->lock_depth_);

  EXPECT_FALSE(ParkAndWait(parkable));

  parkable.Unlock();
  EXPECT_EQ(0, impl->lock_depth_);
  EXPECT_TRUE(ParkAndWait(parkable));
  EXPECT_TRUE(impl->is_parked());
}

TEST_F(ParkableStringTest, ManagerSimple) {
  base::HistogramTester histogram_tester;

  ParkableString parkable(MakeLargeString().Impl());
  ASSERT_FALSE(parkable.Impl()->is_parked());

  auto& manager = ParkableStringManager::Instance();
  EXPECT_EQ(1u, manager.Size());

  // Small strings are not tracked.
  ParkableString small(String("abc").ReleaseImpl());
  EXPECT_EQ(1u, manager.Size());

  // No parking as the current state is not "backgrounded".
  manager.SetRendererBackgrounded(false);
  ASSERT_FALSE(manager.IsRendererBackgrounded());
  manager.ParkAllIfRendererBackgrounded();
  RunPostedTasks();
  EXPECT_FALSE(parkable.Impl()->is_parked());
  histogram_tester.ExpectTotalCount("Memory.MovableStringsCount", 0);

  manager.SetRendererBackgrounded(true);
  ASSERT_TRUE(manager.IsRendererBackgrounded());
  manager.ParkAllIfRendererBackgrounded();
  RunPostedTasks();
  EXPECT_TRUE(parkable.Impl()->is_parked());
  histogram_tester.ExpectUniqueSample("Memory.MovableStringsCount", 1, 1);

  // Park and unpark.
  parkable.ToString();
  EXPECT_FALSE(parkable.Impl()->is_parked());
  manager.ParkAllIfRendererBackgrounded();
  RunPostedTasks();
  EXPECT_TRUE(parkable.Impl()->is_parked());
  histogram_tester.ExpectUniqueSample("Memory.MovableStringsCount", 1, 2);

  // More than one reference, no parking.
  manager.SetRendererBackgrounded(false);
  String alive_unparked = parkable.ToString();  // Unparked in foreground.
  manager.SetRendererBackgrounded(true);
  manager.ParkAllIfRendererBackgrounded();
  RunPostedTasks();
  EXPECT_FALSE(parkable.Impl()->is_parked());

  // Other reference is dropped, OK to park.
  alive_unparked = String();
  manager.ParkAllIfRendererBackgrounded();
  RunPostedTasks();
  EXPECT_TRUE(parkable.Impl()->is_parked());

  histogram_tester.ExpectTotalCount("Memory.MovableStringParkingAction", 5);
  histogram_tester.ExpectBucketCount(
      "Memory.MovableStringParkingAction",
      ParkableStringImpl::ParkingAction::kParkedInBackground, 3);
  histogram_tester.ExpectBucketCount(
      "Memory.MovableStringParkingAction",
      ParkableStringImpl::ParkingAction::kUnparkedInBackground, 1);
  histogram_tester.ExpectBucketCount(
      "Memory.MovableStringParkingAction",
      ParkableStringImpl::ParkingAction::kUnparkedInForeground, 1);
}

TEST_F(ParkableStringTest, ManagerMultipleStrings) {
  base::HistogramTester histogram_tester;

  ParkableString parkable(MakeLargeString().Impl());
  ParkableString parkable2(MakeLargeString().Impl());

  auto& manager = ParkableStringManager::Instance();
  EXPECT_EQ(2u, manager.Size());

  parkable2 = ParkableString();
  EXPECT_EQ(1u, manager.Size());

  ParkableString copy = parkable;
  parkable = ParkableString();
  EXPECT_EQ(1u, manager.Size());
  copy = ParkableString();
  EXPECT_EQ(0u, manager.Size());

  String str = MakeLargeString();
  ParkableString parkable3(str.Impl());
  EXPECT_EQ(1u, manager.Size());
  // De-duplicated.
  ParkableString other_parkable3(str.Impl());
  EXPECT_EQ(1u, manager.Size());
  EXPECT_EQ(parkable3.Impl(), other_parkable3.Impl());

  // If all the references to a string are internal, park it.
  str = String();
  // This string is not parkable, bur should still be in size and count
  // histograms.
  ParkableString parkable4(MakeLargeString().Impl());
  String parkable4_content = parkable4.ToString();

  manager.SetRendererBackgrounded(true);
  ASSERT_TRUE(manager.IsRendererBackgrounded());
  manager.ParkAllIfRendererBackgrounded();  // Records count and size histograms
  RunPostedTasks();
  EXPECT_TRUE(parkable3.Impl()->is_parked());
  manager.ParkAllIfRendererBackgrounded();  // Records count and size histograms
  RunPostedTasks();

  // Only drop it from the managed strings when the last one is gone.
  parkable3 = ParkableString();
  EXPECT_EQ(2u, manager.Size());
  other_parkable3 = ParkableString();
  EXPECT_EQ(1u, manager.Size());
  parkable4 = ParkableString();
  EXPECT_EQ(0u, manager.Size());

  // 1 parked, 1 unparked. Bucket count is 2 as we collected stats twice.
  histogram_tester.ExpectUniqueSample("Memory.MovableStringsCount", 2, 2);
  histogram_tester.ExpectUniqueSample("Memory.MovableStringsTotalSizeKb",
                                      2 * kSizeKb, 2);

  histogram_tester.ExpectTotalCount("Memory.MovableStringParkingAction", 1);
  histogram_tester.ExpectBucketCount(
      "Memory.MovableStringParkingAction",
      ParkableStringImpl::ParkingAction::kParkedInBackground, 1);
}

TEST_F(ParkableStringTest, ShouldPark) {
  String empty_string("");
  EXPECT_FALSE(ParkableStringManager::ShouldPark(*empty_string.Impl()));
  std::vector<char> data(20 * 1000, 'a');

  String parkable(String(data.data(), data.size()).ReleaseImpl());
  EXPECT_TRUE(ParkableStringManager::ShouldPark(*parkable.Impl()));

  std::thread t([]() {
    std::vector<char> data(20 * 1000, 'a');
    String parkable(String(data.data(), data.size()).ReleaseImpl());
    EXPECT_FALSE(ParkableStringManager::ShouldPark(*parkable.Impl()));
  });
  t.join();
}

#if defined(ADDRESS_SANITIZER)
TEST_F(ParkableStringTest, AsanPoisoningTest) {
  ParkableString parkable(MakeLargeString().ReleaseImpl());
  const LChar* data = parkable.ToString().Characters8();
  EXPECT_TRUE(ParkAndWait(parkable));
  EXPECT_DEATH(EXPECT_NE(0, data[10]), "");
}
#endif  // defined(ADDRESS_SANITIZER)

}  // namespace blink
