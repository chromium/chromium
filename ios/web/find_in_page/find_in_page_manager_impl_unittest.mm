// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/find_in_page/find_in_page_manager_impl.h"

#import "base/test/ios/wait_util.h"
#import "base/test/metrics/user_action_tester.h"
#import "ios/web/public/test/fakes/crw_fake_find_interaction.h"
#import "ios/web/public/test/fakes/crw_fake_find_session.h"
#import "ios/web/public/test/fakes/fake_find_in_page_manager_delegate.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_test.h"
#import "testing/gtest/include/gtest/gtest.h"

namespace {

// Timeout before failure if the FindInPageManager does not report to the
// delegate fast enough.
const base::TimeDelta kWaitForFindCompletionTimeout = base::Milliseconds(100);

}  // namespace

using base::test::ios::WaitUntilConditionOrTimeout;

namespace web {

// Tests FindInPageManagerImpl and verifies that the state of
// FindInPageManagerDelegate is correct depending on what the Find session
// returns. FindInPageManagerImplTest is part of an iOS 16+ feature so this
// test class only applies to iOS 16+. For the old Find in Page tests, please
// see java_script_find_in_page_manager_impl_unittest.mm
class FindInPageManagerImplTest : public WebTest {
 protected:
  FindInPageManagerImplTest() : WebTest(std::make_unique<FakeWebClient>()) {}

  void SetUp() override {
    WebTest::SetUp();

    // Skip setup if this is <iOS 16.
    if (@available(iOS 16, *)) {
      fake_web_state_ = std::make_unique<FakeWebState>();
      fake_web_state_->SetBrowserState(GetBrowserState());

      FindInPageManagerImpl::CreateForWebState(fake_web_state_.get());
      GetFindInPageManager()->SetDelegate(&fake_delegate_);
      // Sets a smaller delay between each manager's call to
      // `PollActiveFindSession()` so tests run faster.
      GetFindInPageManager()->poll_active_find_session_delay_ =
          base::Milliseconds(5);

        // Enable and set up fake Find interaction in the fake web state.
        fake_web_state_->SetFindInteractionEnabled(true);
        fake_web_state_->SetFindInteraction(
            [[CRWFakeFindInteraction alloc] init]);
    }
  }

  // Sets the fake Find session analyzed by the FindInPageManager.
  void SetFindSession(CRWFakeFindSession* find_session) API_AVAILABLE(ios(16)) {
    static_cast<CRWFakeFindInteraction*>(fake_web_state_->GetFindInteraction())
        .activeFindSession = find_session;
  }

  // Create a fake Find session which returns the appropriate match counts for
  // given queries in `result_counts_for_queries`.
  CRWFakeFindSession* CreateFindSessionWithResultCountsForQueries(
      ResultCountsForQueries* result_counts_for_queries)
      API_AVAILABLE(ios(16)) {
    CRWFakeFindSession* find_session = [[CRWFakeFindSession alloc] init];
    find_session.resultCountsForQueries = result_counts_for_queries;
    return find_session;
  }

  // Sets a fake Find session with given fake result counts for given queries in
  // `result_counts_for_queries`,
  // to be analyzed by the FindInPageManager.
  void SetFindSessionWithResultCountsForQueries(
      ResultCountsForQueries* result_counts_for_queries)
      API_AVAILABLE(ios(16)) {
    SetFindSession(
        CreateFindSessionWithResultCountsForQueries(result_counts_for_queries));
  }

  // Wait until the delegate state has been set or time is out.
  bool WaitForStateOrTimeout() API_AVAILABLE(ios(16)) {
    return WaitUntilConditionOrTimeout(kWaitForFindCompletionTimeout, ^bool {
      base::RunLoop().RunUntilIdle();
      return fake_delegate_.state();
    });
  }

  // Wait until the index verifies `index_predicate` in the delegate state.
  bool WaitForIndexOrTimeout(bool (^index_predicate)(int))
      API_AVAILABLE(ios(16)) {
    return WaitUntilConditionOrTimeout(kWaitForFindCompletionTimeout, ^bool {
      base::RunLoop().RunUntilIdle();
      return fake_delegate_.state() &&
             index_predicate(fake_delegate_.state()->index);
    });
  }

  // Wait until the index is different from -1 in the delegate state.
  bool WaitForValidIndexOrTimeout() API_AVAILABLE(ios(16)) {
    return WaitForIndexOrTimeout(^(int index) {
      return index != -1;
    });
  }

  // Returns the FindInPageManager associated with `fake_web_state_`.
  FindInPageManagerImpl* GetFindInPageManager() API_AVAILABLE(ios(16)) {
    return static_cast<FindInPageManagerImpl*>(
        FindInPageManager::FromWebState(fake_web_state_.get()));
  }

  // Get the active Find session in the FindInPageManager.
  id<CRWFindSession> GetActiveFindSession() API_AVAILABLE(ios(16)) {
    return GetFindInPageManager()->GetActiveFindSession();
  }

  std::unique_ptr<FakeWebState> fake_web_state_ API_AVAILABLE(ios(16));
  FakeFindInPageManagerDelegate fake_delegate_ API_AVAILABLE(ios(16));
  base::UserActionTester user_action_tester_ API_AVAILABLE(ios(16));
};

// Tests that Find In Page responds with a total match count of three when it
// calls Find on a query with three matches.
TEST_F(FindInPageManagerImplTest, FindThreeMatches) {
  if (@available(iOS 16, *)) {
    SetFindSessionWithResultCountsForQueries(@{@"foo" : @3});

    GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);

    ASSERT_TRUE(WaitForStateOrTimeout());
    EXPECT_EQ(3, fake_delegate_.state()->match_count);
  }
}

// Tests that Find In Page returns a total match count matching the latest find
// if two finds are called.
TEST_F(FindInPageManagerImplTest, ReturnLatestFind) {
  if (@available(iOS 16, *)) {
    SetFindSessionWithResultCountsForQueries(@{@"foo" : @3, @"bar" : @2});

    GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);
    ASSERT_TRUE(WaitForStateOrTimeout());
    fake_delegate_.Reset();

    GetFindInPageManager()->Find(@"bar", FindInPageOptions::FindInPageSearch);

    ASSERT_TRUE(WaitForStateOrTimeout());
    EXPECT_EQ(2, fake_delegate_.state()->match_count);
  }
}

// Tests that the Find In Page manager does not report to the delegate if the
// web state is destroyed during a Find operation.
TEST_F(FindInPageManagerImplTest, DestroyWebStateDuringFind) {
  if (@available(iOS 16, *)) {
    SetFindSessionWithResultCountsForQueries(@{@"foo" : @3});
    GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);

    fake_web_state_.reset();
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(fake_delegate_.state());
  }
}

// Tests that Find In Page doesn't fail when delegate is not set.
TEST_F(FindInPageManagerImplTest, DelegateNotSet) {
  if (@available(iOS 16, *)) {
    GetFindInPageManager()->SetDelegate(nullptr);
    SetFindSessionWithResultCountsForQueries(@{@"foo" : @3});
    GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);

    ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForFindCompletionTimeout, ^{
      base::RunLoop().RunUntilIdle();
      return GetActiveFindSession().resultCount == 3;
    }));
  }
}

// Tests that Find in Page manager responds with a total match count of zero
// when there are no matches in the web page. Tests that Find in Page also did
// not respond with a valid selected match index value.
TEST_F(FindInPageManagerImplTest, PageWithNoMatchNoHighlight) {
  if (@available(iOS 16, *)) {
    GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);

    ASSERT_TRUE(WaitForStateOrTimeout());
    EXPECT_EQ(0, fake_delegate_.state()->match_count);
    EXPECT_EQ(-1, fake_delegate_.state()->index);
  }
}

// Tests that Find in Page responds with index zero after a find when there are
// three matches in a page.
TEST_F(FindInPageManagerImplTest, DidHighlightFirstIndex) {
  if (@available(iOS 16, *)) {
    SetFindSessionWithResultCountsForQueries(@{@"foo" : @3});
    GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);

    ASSERT_TRUE(WaitForValidIndexOrTimeout());
    EXPECT_EQ(0, fake_delegate_.state()->index);
  }
}

// Tests that Find in Page responds with index one to a FindInPageNext find
// after a FindInPageSearch find finishes when there are two matches in a page.
TEST_F(FindInPageManagerImplTest, FindDidHighlightSecondIndexAfterNextCall) {
  if (@available(iOS 16, *)) {
    SetFindSessionWithResultCountsForQueries(@{@"foo" : @2});
    GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);
    ASSERT_TRUE(WaitForStateOrTimeout());

    GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageNext);

    ASSERT_TRUE(WaitForValidIndexOrTimeout());
    EXPECT_EQ(1, fake_delegate_.state()->index);
  }
}

// Tests that Find in Page selects all matches in a page with three matches and
// wraps when making successive FindInPageNext calls.
TEST_F(FindInPageManagerImplTest, FindDidSelectAllMatchesWithNextCall) {
  if (@available(iOS 16, *)) {
    SetFindSessionWithResultCountsForQueries(@{@"foo" : @3});
    GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);

    ASSERT_TRUE(WaitForValidIndexOrTimeout());
    EXPECT_EQ(0, fake_delegate_.state()->index);

    GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageNext);
    fake_delegate_.Reset();
    ASSERT_TRUE(WaitForValidIndexOrTimeout());
    EXPECT_EQ(1, fake_delegate_.state()->index);

    GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageNext);
    fake_delegate_.Reset();
    ASSERT_TRUE(WaitForValidIndexOrTimeout());
    EXPECT_EQ(2, fake_delegate_.state()->index);

    GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageNext);
    fake_delegate_.Reset();
    ASSERT_TRUE(WaitForValidIndexOrTimeout());
    EXPECT_EQ(0, fake_delegate_.state()->index);
  }
}

// Tests that Find in Page selects all matches in a page with three matches and
// wraps when making successive FindInPagePrevious calls.
TEST_F(FindInPageManagerImplTest,
       FindDidLoopThroughAllMatchesWithPreviousCall) {
  if (@available(iOS 16, *)) {
    SetFindSessionWithResultCountsForQueries(@{@"foo" : @3});
    GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);

    ASSERT_TRUE(WaitForValidIndexOrTimeout());
    EXPECT_EQ(0, fake_delegate_.state()->index);

    GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPagePrevious);

    fake_delegate_.Reset();
    ASSERT_TRUE(WaitForStateOrTimeout());
    EXPECT_EQ(2, fake_delegate_.state()->index);

    GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPagePrevious);

    fake_delegate_.Reset();
    ASSERT_TRUE(WaitForStateOrTimeout());
    EXPECT_EQ(1, fake_delegate_.state()->index);

    GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPagePrevious);

    fake_delegate_.Reset();
    ASSERT_TRUE(WaitForStateOrTimeout());
    EXPECT_EQ(0, fake_delegate_.state()->index);
  }
}

// Tests that Find in Page does not respond to a FindInPageNext or a
// FindInPagePrevious call if no FindInPageSearch find was executed beforehand.
TEST_F(FindInPageManagerImplTest, FindDidNotRepondToNextOrPrevIfNoSearch) {
  if (@available(iOS 16, *)) {
    SetFindSessionWithResultCountsForQueries(@{@"foo" : @3});

    GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageNext);
    base::RunLoop().RunUntilIdle();

    EXPECT_FALSE(fake_delegate_.state());

    GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPagePrevious);
    base::RunLoop().RunUntilIdle();

    EXPECT_FALSE(fake_delegate_.state());
  }
}

// Tests that Find in Page resets the match count to 0 and the query to nil
// after calling StopFinding().
TEST_F(FindInPageManagerImplTest, FindInPageCanStopFind) {
  if (@available(iOS 16, *)) {
    SetFindSessionWithResultCountsForQueries(@{@"foo" : @3});
    GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);
    ASSERT_TRUE(WaitForStateOrTimeout());

    fake_delegate_.Reset();
    GetFindInPageManager()->StopFinding();
    ASSERT_TRUE(WaitForStateOrTimeout());
    EXPECT_FALSE(fake_delegate_.state()->query);
    EXPECT_EQ(0, fake_delegate_.state()->match_count);
  }
}

// Tests that Find in Page logs correct UserActions for given API calls.
TEST_F(FindInPageManagerImplTest, FindUserActions) {
  if (@available(iOS 16, *)) {
    SetFindSessionWithResultCountsForQueries(@{@"foo" : @3});
    ASSERT_EQ(
        0, user_action_tester_.GetActionCount("IOS.FindInPage.SearchStarted"));
    GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);
    ASSERT_TRUE(WaitForValidIndexOrTimeout());

    ASSERT_EQ(0, user_action_tester_.GetActionCount("IOS.FindInPage.FindNext"));
    GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageNext);
    fake_delegate_.Reset();
    ASSERT_TRUE(WaitForIndexOrTimeout(^(int index) {
      return index == 1;
    }));
    EXPECT_EQ(1, user_action_tester_.GetActionCount("IOS.FindInPage.FindNext"));

    ASSERT_EQ(
        0, user_action_tester_.GetActionCount("IOS.FindInPage.FindPrevious"));
    GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPagePrevious);
    fake_delegate_.Reset();
    ASSERT_TRUE(WaitForIndexOrTimeout(^(int index) {
      return index == 0;
    }));
    EXPECT_EQ(
        1, user_action_tester_.GetActionCount("IOS.FindInPage.FindPrevious"));
  }
}

// Tests that the Find navigator is presented when the Find session starts and
// dismissed when the Find session stops, if a Find interaction is used.
TEST_F(FindInPageManagerImplTest, FindNavigatorPresentedAndDismissed) {
  if (@available(iOS 16, *)) {
    GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);
    ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForFindCompletionTimeout, ^{
      base::RunLoop().RunUntilIdle();
      return fake_web_state_->GetFindInteraction().findNavigatorVisible;
    }));

    GetFindInPageManager()->StopFinding();
    ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForFindCompletionTimeout, ^{
      base::RunLoop().RunUntilIdle();
      return !fake_web_state_->GetFindInteraction().findNavigatorVisible;
    }));
  }
}

// Tests that the manager reports to its delegate when it detects the Find
// navigator has been dismissed by the user, and set the query back to nil and
// the match count to 0.
TEST_F(FindInPageManagerImplTest,
       UserDismissesFindNavigatorDetectedAndStopsSearch) {
  if (@available(iOS 16, *)) {
    SetFindSessionWithResultCountsForQueries(@{@"foo" : @3});
    GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);
    ASSERT_TRUE(WaitForValidIndexOrTimeout());
    ASSERT_FALSE(fake_delegate_.state()->user_dismissed_find_navigator);
    ASSERT_EQ(3, fake_delegate_.state()->match_count);

    [fake_web_state_->GetFindInteraction() dismissFindNavigator];
    fake_delegate_.Reset();
    ASSERT_TRUE(WaitForStateOrTimeout());
    EXPECT_TRUE(fake_delegate_.state()->user_dismissed_find_navigator);
    EXPECT_EQ(0, fake_delegate_.state()->match_count);
    EXPECT_FALSE(fake_delegate_.state()->query);
  }
}

}  // namespace web
