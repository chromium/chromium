// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browsing_data/model/browsing_data_remover_impl.h"

#import <memory>

#import "base/check_op.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/run_loop.h"
#import "base/scoped_observation.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "components/open_from_clipboard/clipboard_recent_content.h"
#import "components/open_from_clipboard/fake_clipboard_recent_content.h"
#import "ios/chrome/browser/browsing_data/model/browsing_data_remover_observer.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using base::test::ios::kWaitForActionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace {

// Flags passed when calling Remove(). Clear as much data as possible, avoiding
// using services that are not created for TestProfileIOS.
constexpr BrowsingDataRemoveMask kRemoveMask =
    BrowsingDataRemoveMask::REMOVE_APPCACHE |
    BrowsingDataRemoveMask::REMOVE_CACHE |
    BrowsingDataRemoveMask::REMOVE_COOKIES |
    BrowsingDataRemoveMask::REMOVE_FORM_DATA |
    BrowsingDataRemoveMask::REMOVE_HISTORY |
    BrowsingDataRemoveMask::REMOVE_INDEXEDDB |
    BrowsingDataRemoveMask::REMOVE_LOCAL_STORAGE |
    BrowsingDataRemoveMask::REMOVE_PASSWORDS |
    BrowsingDataRemoveMask::REMOVE_WEBSQL |
    BrowsingDataRemoveMask::REMOVE_CACHE_STORAGE |
    BrowsingDataRemoveMask::REMOVE_VISITED_LINKS |
    BrowsingDataRemoveMask::REMOVE_LAST_USER_ACCOUNT;

const char kFullDeletionHistogram[] =
    "History.ClearBrowsingData.Duration.FullDeletion";
const char kTimeRangeDeletionHistogram[] =
    "History.ClearBrowsingData.Duration.TimeRangeDeletion";

// Observer used to validate that BrowsingDataRemoverImpl notifies its
// observers.
class TestBrowsingDataRemoverObserver : public BrowsingDataRemoverObserver {
 public:
  TestBrowsingDataRemoverObserver() = default;

  TestBrowsingDataRemoverObserver(const TestBrowsingDataRemoverObserver&) =
      delete;
  TestBrowsingDataRemoverObserver& operator=(
      const TestBrowsingDataRemoverObserver&) = delete;

  ~TestBrowsingDataRemoverObserver() override = default;

  // BrowsingDataRemoverObserver implementation.
  void OnBrowsingDataRemoved(BrowsingDataRemover* remover,
                             BrowsingDataRemoveMask mask) override;

  // Returns the `mask` value passed to the last call of OnBrowsingDataRemoved.
  // Returns BrowsingDataRemoveMask::REMOVE_NOTHING if it has not been called.
  BrowsingDataRemoveMask last_remove_mask() const { return last_remove_mask_; }

 private:
  BrowsingDataRemoveMask last_remove_mask_ =
      BrowsingDataRemoveMask::REMOVE_NOTHING;
};

void TestBrowsingDataRemoverObserver::OnBrowsingDataRemoved(
    BrowsingDataRemover* remover,
    BrowsingDataRemoveMask mask) {
  DCHECK(mask != BrowsingDataRemoveMask::REMOVE_NOTHING);
  last_remove_mask_ = mask;
}

}  // namespace

class BrowsingDataRemoverImplTest : public PlatformTest {
 public:
  BrowsingDataRemoverImplTest()
      : profile_(TestProfileIOS::Builder().Build()),
        browsing_data_remover_(profile_.get()) {
    DCHECK_EQ(ClipboardRecentContent::GetInstance(), nullptr);
    ClipboardRecentContent::SetInstance(
        std::make_unique<FakeClipboardRecentContent>());
  }

  BrowsingDataRemoverImplTest(const BrowsingDataRemoverImplTest&) = delete;
  BrowsingDataRemoverImplTest& operator=(const BrowsingDataRemoverImplTest&) =
      delete;

  ~BrowsingDataRemoverImplTest() override {
    DCHECK_NE(ClipboardRecentContent::GetInstance(), nullptr);
    ClipboardRecentContent::SetInstance(nullptr);
    browsing_data_remover_.Shutdown();
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<ProfileIOS> profile_;
  BrowsingDataRemoverImpl browsing_data_remover_;
};

// Tests that BrowsingDataRemoverImpl::Remove() invokes the observers.
TEST_F(BrowsingDataRemoverImplTest, InvokesObservers) {
  TestBrowsingDataRemoverObserver observer;
  ASSERT_TRUE(observer.last_remove_mask() != kRemoveMask);

  base::ScopedObservation<BrowsingDataRemover, BrowsingDataRemoverObserver>
      scoped_observer(&observer);
  scoped_observer.Observe(&browsing_data_remover_);

  browsing_data_remover_.Remove(browsing_data::TimePeriod::ALL_TIME,
                                kRemoveMask, base::DoNothing());

  TestBrowsingDataRemoverObserver* observer_ptr = &observer;
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
    // Spin the RunLoop as WaitUntilConditionOrTimeout doesn't.
    base::RunLoop().RunUntilIdle();
    return observer_ptr->last_remove_mask() == kRemoveMask;
  }));
}

// Tests that BrowsingDataRemoverImpl::Remove() can be called multiple times.
TEST_F(BrowsingDataRemoverImplTest, SerializeRemovals) {
  __block int remaining_calls = 2;
  browsing_data_remover_.Remove(browsing_data::TimePeriod::ALL_TIME,
                                kRemoveMask, base::BindOnce(^{
                                  --remaining_calls;
                                }));
  browsing_data_remover_.Remove(browsing_data::TimePeriod::ALL_TIME,
                                kRemoveMask, base::BindOnce(^{
                                  --remaining_calls;
                                }));

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
    // Spin the RunLoop as WaitUntilConditionOrTimeout doesn't.
    base::RunLoop().RunUntilIdle();
    return remaining_calls == 0;
  }));
}

// Tests that BrowsingDataRemoverImpl::Remove() Logs the duration to the correct
// histogram for full deletion.
TEST_F(BrowsingDataRemoverImplTest, LogDurationForFullDeletion) {
  base::HistogramTester histogram_tester;
  __block int remaining_calls = 1;
  histogram_tester.ExpectTotalCount(kFullDeletionHistogram, 0);
  histogram_tester.ExpectTotalCount(kTimeRangeDeletionHistogram, 0);

  browsing_data_remover_.Remove(browsing_data::TimePeriod::ALL_TIME,
                                kRemoveMask, base::BindOnce(^{
                                  --remaining_calls;
                                }));
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
    // Spin the RunLoop as WaitUntilConditionOrTimeout doesn't.
    base::RunLoop().RunUntilIdle();
    return remaining_calls == 0;
  }));

  histogram_tester.ExpectTotalCount(kFullDeletionHistogram, 1);
  histogram_tester.ExpectTotalCount(kTimeRangeDeletionHistogram, 0);
}

// Tests that BrowsingDataRemoverImpl::Remove() Logs the duration to the correct
// histogram for partial deletion.
TEST_F(BrowsingDataRemoverImplTest, LogDurationForPartialDeletion) {
  base::HistogramTester histogram_tester;
  __block int remaining_calls = 1;
  histogram_tester.ExpectTotalCount(kFullDeletionHistogram, 0);
  histogram_tester.ExpectTotalCount(kTimeRangeDeletionHistogram, 0);

  browsing_data_remover_.Remove(browsing_data::TimePeriod::LAST_HOUR,
                                kRemoveMask, base::BindOnce(^{
                                  --remaining_calls;
                                }));
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
    // Spin the RunLoop as WaitUntilConditionOrTimeout doesn't.
    base::RunLoop().RunUntilIdle();
    return remaining_calls == 0;
  }));

  histogram_tester.ExpectTotalCount(kFullDeletionHistogram, 0);
  histogram_tester.ExpectTotalCount(kTimeRangeDeletionHistogram, 1);
}

// Tests that BrowsingDataRemoverImpl::Remove() can finish performing its
// operation even if the BrowserState is destroyed.
TEST_F(BrowsingDataRemoverImplTest, PerformAfterBrowserStateDestruction) {
  __block int remaining_calls = 1;
  browsing_data_remover_.Remove(browsing_data::TimePeriod::ALL_TIME,
                                kRemoveMask, base::BindOnce(^{
                                  --remaining_calls;
                                }));

  // Simulate destruction of BrowserState.
  browsing_data_remover_.Shutdown();

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
    // Spin the RunLoop as WaitUntilConditionOrTimeout doesn't.
    base::RunLoop().RunUntilIdle();
    return remaining_calls == 0;
  }));
}

// Tests that BrowsingDataRemoverImpl::RemoveInRange() invokes the observers.
TEST_F(BrowsingDataRemoverImplTest, InvokesObservers_RemoveInRange) {
  TestBrowsingDataRemoverObserver observer;
  ASSERT_TRUE(observer.last_remove_mask() != kRemoveMask);

  base::ScopedObservation<BrowsingDataRemover, BrowsingDataRemoverObserver>
      scoped_observer(&observer);
  scoped_observer.Observe(&browsing_data_remover_);

  base::Time delete_start_time = base::Time::Now() - base::Hours(1);
  base::Time delete_end_time = base::Time::Now();
  browsing_data_remover_.RemoveInRange(delete_start_time, delete_end_time,
                                       kRemoveMask, base::DoNothing());

  TestBrowsingDataRemoverObserver* observer_ptr = &observer;
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
    // Spin the RunLoop as WaitUntilConditionOrTimeout doesn't.
    base::RunLoop().RunUntilIdle();
    return observer_ptr->last_remove_mask() == kRemoveMask;
  }));
}

// Tests that BrowsingDataRemoverImpl::RemoveInRange() can be called multiple
// times.
TEST_F(BrowsingDataRemoverImplTest, SerializeRemovals_RemoveInRange) {
  __block int remaining_calls = 2;
  base::Time delete_start_time = base::Time::Now() - base::Hours(1);
  base::Time delete_end_time = base::Time::Now();
  browsing_data_remover_.RemoveInRange(delete_start_time, delete_end_time,
                                       kRemoveMask, base::BindOnce(^{
                                         --remaining_calls;
                                       }));

  browsing_data_remover_.RemoveInRange(delete_start_time, delete_end_time,
                                       kRemoveMask, base::BindOnce(^{
                                         --remaining_calls;
                                       }));

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
    // Spin the RunLoop as WaitUntilConditionOrTimeout doesn't.
    base::RunLoop().RunUntilIdle();
    return remaining_calls == 0;
  }));
}

// Tests that BrowsingDataRemoverImpl::RemoveInRange() can finish performing its
// operation even if the BrowserState is destroyed.
TEST_F(BrowsingDataRemoverImplTest,
       PerformAfterBrowserStateDestruction_RemoveInRange) {
  __block int remaining_calls = 1;
  base::Time delete_start_time = base::Time::Now() - base::Hours(1);
  base::Time delete_end_time = base::Time::Now();
  browsing_data_remover_.RemoveInRange(delete_start_time, delete_end_time,
                                       kRemoveMask, base::BindOnce(^{
                                         --remaining_calls;
                                       }));

  // Simulate destruction of BrowserState.
  browsing_data_remover_.Shutdown();

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
    // Spin the RunLoop as WaitUntilConditionOrTimeout doesn't.
    base::RunLoop().RunUntilIdle();
    return remaining_calls == 0;
  }));
}
