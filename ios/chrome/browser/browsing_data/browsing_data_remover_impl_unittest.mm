// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browsing_data/browsing_data_remover_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#import "base/test/ios/wait_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/open_from_clipboard/clipboard_recent_content.h"
#include "components/open_from_clipboard/fake_clipboard_recent_content.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/browsing_data/browsing_data_remover_observer.h"
#import "ios/chrome/browser/sessions/session_service_ios.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForActionTimeout;

namespace {

// Flags passed when calling Remove(). Clear as much data as possible, avoiding
// using services that are not created for TestChromeBrowserState.
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
const char kPartialDeletionHistogram[] =
    "History.ClearBrowsingData.Duration.PartialDeletion";

// Observer used to validate that BrowsingDataRemoverImpl notifies its
// observers.
class TestBrowsingDataRemoverObserver : public BrowsingDataRemoverObserver {
 public:
  TestBrowsingDataRemoverObserver() = default;
  ~TestBrowsingDataRemoverObserver() override = default;

  // BrowsingDataRemoverObserver implementation.
  void OnBrowsingDataRemoved(BrowsingDataRemover* remover,
                             BrowsingDataRemoveMask mask) override;

  // Returns the |mask| value passed to the last call of OnBrowsingDataRemoved.
  // Returns BrowsingDataRemoveMask::REMOVE_NOTHING if it has not been called.
  BrowsingDataRemoveMask last_remove_mask() const { return last_remove_mask_; }

 private:
  BrowsingDataRemoveMask last_remove_mask_ =
      BrowsingDataRemoveMask::REMOVE_NOTHING;

  DISALLOW_COPY_AND_ASSIGN(TestBrowsingDataRemoverObserver);
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
      : browser_state_(TestChromeBrowserState::Builder().Build()),
        browsing_data_remover_(browser_state_.get(), nil) {
    DCHECK_EQ(ClipboardRecentContent::GetInstance(), nullptr);
    ClipboardRecentContent::SetInstance(
        std::make_unique<FakeClipboardRecentContent>());
  }

  ~BrowsingDataRemoverImplTest() override {
    DCHECK_NE(ClipboardRecentContent::GetInstance(), nullptr);
    ClipboardRecentContent::SetInstance(nullptr);
    browsing_data_remover_.Shutdown();
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<ios::ChromeBrowserState> browser_state_;
  BrowsingDataRemoverImpl browsing_data_remover_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowsingDataRemoverImplTest);
};

// Tests that BrowsingDataRemoverImpl::Remove() invokes the observers.
TEST_F(BrowsingDataRemoverImplTest, InvokesObservers) {
  TestBrowsingDataRemoverObserver observer;
  ASSERT_TRUE(observer.last_remove_mask() != kRemoveMask);

  ScopedObserver<BrowsingDataRemover, BrowsingDataRemoverObserver>
      scoped_observer(&observer);
  scoped_observer.Add(&browsing_data_remover_);

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
  histogram_tester.ExpectTotalCount(kPartialDeletionHistogram, 0);

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
  histogram_tester.ExpectTotalCount(kPartialDeletionHistogram, 0);
}

// Tests that BrowsingDataRemoverImpl::Remove() Logs the duration to the correct
// histogram for partial deletion.
TEST_F(BrowsingDataRemoverImplTest, LogDurationForPartialDeletion) {
  base::HistogramTester histogram_tester;
  __block int remaining_calls = 1;
  histogram_tester.ExpectTotalCount(kFullDeletionHistogram, 0);
  histogram_tester.ExpectTotalCount(kPartialDeletionHistogram, 0);

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
  histogram_tester.ExpectTotalCount(kPartialDeletionHistogram, 1);
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
