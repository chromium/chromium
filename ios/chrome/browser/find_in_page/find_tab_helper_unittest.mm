// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/find_in_page/find_tab_helper.h"

#include "base/macros.h"
#include "base/run_loop.h"
#import "base/test/ios/wait_util.h"
#include "base/test/metrics/user_action_tester.h"
#import "ios/chrome/browser/find_in_page/features.h"
#import "ios/chrome/browser/find_in_page/find_in_page_model.h"
#import "ios/chrome/browser/web/chrome_web_test.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#import "ios/web/public/test/web_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const NSString* kTestString = @"Test string";
}

// Test fixture for the FindTabHelper class.
class FindTabHelperTest : public ChromeWebTest {
 public:
  FindTabHelperTest() = default;
  ~FindTabHelperTest() override = default;

 protected:
  void SetUp() override {
    ChromeWebTest::SetUp();
    FindTabHelper::CreateForWebState(web_state());
  }

  void TearDown() override {
    // Stop any in-progress find operations when the test completes.
    __block BOOL completion_handler_block_was_called = NO;
    FindTabHelper::FromWebState(web_state())->StopFinding(^{
      completion_handler_block_was_called = YES;
    });
    base::test::ios::WaitUntilCondition(^bool() {
      return completion_handler_block_was_called;
    });

    ChromeWebTest::TearDown();
  }

  // Loads a test html page with the given number of repetitions of
  // |kTestString}.
  void LoadTestHtml(int test_string_count) {
    NSString* html = @"<html><body>";
    for (int ii = 0; ii < test_string_count; ++ii) {
      NSString* test_string = [NSString
          stringWithFormat:@"%@ %d <br>", kTestString, test_string_count];
      html = [html stringByAppendingString:test_string];
    }
    html = [html stringByAppendingString:@"</body></html>"];

    LoadHtml(html);
  }

  base::UserActionTester user_action_tester_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FindTabHelperTest);
};

// Tests the StartFinding(), ContinueFinding(), and StopFinding() methods.
TEST_F(FindTabHelperTest, FindInPage) {
  // Tests should not run if Find in Page iFrame feature flag is on. If it is,
  // FindinPageResponseDelegate is used by FindinPageController to respond.
  if (base::FeatureList::IsEnabled(kFindInPageiFrame)) {
    return;
  }
  LoadTestHtml(5);
  auto* helper = FindTabHelper::FromWebState(web_state());
  ASSERT_TRUE(helper);

  __block BOOL completion_handler_block_was_called = NO;
  id wait_block = ^bool() {
    // Waits for |completion_handler_block_was_called| to be YES, but resets it
    // to NO before returning.
    BOOL success = completion_handler_block_was_called;
    if (success) {
      completion_handler_block_was_called = NO;
    }
    return success;
  };

  // Search for "Test string" and verify that there are five matches.
  ASSERT_EQ(0, user_action_tester_.GetActionCount(kFindActionName));
  helper->StartFinding(@"Test string", ^(FindInPageModel* model) {
    EXPECT_EQ(5U, model.matches);
    EXPECT_EQ(1U, model.currentIndex);
    completion_handler_block_was_called = YES;
  });
  base::test::ios::WaitUntilCondition(wait_block);
  EXPECT_EQ(1, user_action_tester_.GetActionCount(kFindActionName));

  // Search forward in the page for additional matches and verify that
  // |currentIndex| is updated.
  ASSERT_EQ(0, user_action_tester_.GetActionCount(kFindNextActionName));
  helper->ContinueFinding(FindTabHelper::FORWARD, ^(FindInPageModel* model) {
    EXPECT_EQ(5U, model.matches);
    EXPECT_EQ(2U, model.currentIndex);
    completion_handler_block_was_called = YES;
  });
  base::test::ios::WaitUntilCondition(wait_block);
  EXPECT_EQ(1, user_action_tester_.GetActionCount(kFindNextActionName));

  helper->ContinueFinding(FindTabHelper::FORWARD, ^(FindInPageModel* model) {
    EXPECT_EQ(5U, model.matches);
    EXPECT_EQ(3U, model.currentIndex);
    completion_handler_block_was_called = YES;
  });
  base::test::ios::WaitUntilCondition(wait_block);
  EXPECT_EQ(2, user_action_tester_.GetActionCount(kFindNextActionName));

  // Search backwards in the page for previous matches and verify that
  // |currentIndex| is updated.
  ASSERT_EQ(0, user_action_tester_.GetActionCount(kFindPreviousActionName));
  helper->ContinueFinding(FindTabHelper::REVERSE, ^(FindInPageModel* model) {
    EXPECT_EQ(5U, model.matches);
    EXPECT_EQ(2U, model.currentIndex);
    completion_handler_block_was_called = YES;
  });
  base::test::ios::WaitUntilCondition(wait_block);
  EXPECT_EQ(1, user_action_tester_.GetActionCount(kFindPreviousActionName));

  // Stop finding and verify that the completion block was called properly.
  helper->StopFinding(^{
    completion_handler_block_was_called = YES;
  });
  base::test::ios::WaitUntilCondition(wait_block);
}

// Tests that ContinueFinding() wraps around when it reaches the last match.
TEST_F(FindTabHelperTest, ContinueFindingWrapsAround) {
  // Tests should not run if Find in Page iFrame feature flag is on. If it is,
  // FindinPageResponseDelegate is used by FindinPageController to respond.
  if (base::FeatureList::IsEnabled(kFindInPageiFrame)) {
    return;
  }
  LoadTestHtml(2);
  auto* helper = FindTabHelper::FromWebState(web_state());
  ASSERT_TRUE(helper);

  __block BOOL completion_handler_block_was_called = NO;
  id wait_block = ^bool() {
    // Waits for |completion_handler_block_was_called| to be YES, but resets it
    // to NO before returning.
    BOOL success = completion_handler_block_was_called;
    if (success) {
      completion_handler_block_was_called = NO;
    }
    return success;
  };

  // Search for "Test string".
  ASSERT_EQ(0, user_action_tester_.GetActionCount(kFindActionName));
  helper->StartFinding(@"Test string", ^(FindInPageModel* model) {
    EXPECT_EQ(2U, model.matches);
    EXPECT_EQ(1U, model.currentIndex);
    completion_handler_block_was_called = YES;
  });
  base::test::ios::WaitUntilCondition(wait_block);
  EXPECT_EQ(1, user_action_tester_.GetActionCount(kFindActionName));

  // Search backwards in the page and verify that |currentIndex| wraps around to
  // the last match.
  ASSERT_EQ(0, user_action_tester_.GetActionCount(kFindPreviousActionName));
  helper->ContinueFinding(FindTabHelper::REVERSE, ^(FindInPageModel* model) {
    EXPECT_EQ(2U, model.matches);
    EXPECT_EQ(2U, model.currentIndex);
    completion_handler_block_was_called = YES;
  });
  base::test::ios::WaitUntilCondition(wait_block);
  EXPECT_EQ(1, user_action_tester_.GetActionCount(kFindPreviousActionName));

  // Search forward in the page and verify that |currentIndex| wraps around to
  // the first match.
  ASSERT_EQ(0, user_action_tester_.GetActionCount(kFindNextActionName));
  helper->ContinueFinding(FindTabHelper::FORWARD, ^(FindInPageModel* model) {
    EXPECT_EQ(2U, model.matches);
    EXPECT_EQ(1U, model.currentIndex);
    completion_handler_block_was_called = YES;
  });
  base::test::ios::WaitUntilCondition(wait_block);
  EXPECT_EQ(1, user_action_tester_.GetActionCount(kFindNextActionName));
}

// Tests that the FindInPageModel returned by GetFindResults() is updated to
// reflect the results of the latest find operation.
TEST_F(FindTabHelperTest, GetFindResults) {
  // Tests should not run if Find in Page iFrame feature flag is on. If it is,
  // FindinPageResponseDelegate is used by FindinPageController to respond.
  if (base::FeatureList::IsEnabled(kFindInPageiFrame)) {
    return;
  }
  LoadTestHtml(2);
  auto* helper = FindTabHelper::FromWebState(web_state());
  ASSERT_TRUE(helper);

  __block BOOL completion_handler_block_was_called = NO;
  id wait_block = ^bool() {
    // Waits for |completion_handler_block_was_called| to be YES, but resets it
    // to NO before returning.
    BOOL success = completion_handler_block_was_called;
    if (success) {
      completion_handler_block_was_called = NO;
    }
    return success;
  };

  // Search for "Test string".
  ASSERT_EQ(0, user_action_tester_.GetActionCount(kFindActionName));
  helper->StartFinding(@"Test string", ^(FindInPageModel* model) {
    completion_handler_block_was_called = YES;
  });
  base::test::ios::WaitUntilCondition(wait_block);
  EXPECT_EQ(1, user_action_tester_.GetActionCount(kFindActionName));
  {
    FindInPageModel* model = helper->GetFindResult();
    EXPECT_EQ(2U, model.matches);
    EXPECT_EQ(1U, model.currentIndex);
  }

  // Search forward in the page and verify that |currentIndex| wraps around to
  // the first match.
  ASSERT_EQ(0, user_action_tester_.GetActionCount(kFindNextActionName));
  helper->ContinueFinding(FindTabHelper::FORWARD, ^(FindInPageModel* model) {
    completion_handler_block_was_called = YES;
  });
  base::test::ios::WaitUntilCondition(wait_block);
  EXPECT_EQ(1, user_action_tester_.GetActionCount(kFindNextActionName));
  {
    FindInPageModel* model = helper->GetFindResult();
    EXPECT_EQ(2U, model.matches);
    EXPECT_EQ(2U, model.currentIndex);
  }
}

// Tests the IsFindUIActive() getter and setter.
TEST_F(FindTabHelperTest, IsFindUIActive) {
  // Tests should not run if Find in Page iFrame feature flag is on. If it is,
  // FindinPageResponseDelegate is used by FindinPageController to respond.
  if (base::FeatureList::IsEnabled(kFindInPageiFrame)) {
    return;
  }
  auto* helper = FindTabHelper::FromWebState(web_state());

  helper->SetFindUIActive(true);
  EXPECT_TRUE(helper->IsFindUIActive());

  helper->SetFindUIActive(false);
  EXPECT_FALSE(helper->IsFindUIActive());
}

// Tests that IsFindUIActive() is reset to false on page navigation.
TEST_F(FindTabHelperTest, FindUIActiveIsResetOnPageNavigation) {
  // Tests should not run if Find in Page iFrame feature flag is on. If it is,
  // FindinPageResponseDelegate is used by FindinPageController to respond.
  if (base::FeatureList::IsEnabled(kFindInPageiFrame)) {
    return;
  }
  LoadTestHtml(2);
  auto* helper = FindTabHelper::FromWebState(web_state());
  helper->SetFindUIActive(true);
  EXPECT_TRUE(helper->IsFindUIActive());

  LoadTestHtml(3);
  EXPECT_FALSE(helper->IsFindUIActive());
}
