// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_view.h"

#import <Foundation/Foundation.h>

#import "base/test/task_environment.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/icon_detail_view.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/constants.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_state.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/types.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/utils.h"
#import "testing/platform_test.h"

// Tests the SafetyCheckView and subviews.
class SafetyCheckViewTest : public PlatformTest {
 public:
  SafetyCheckViewTest() {
    _superview = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 200, 200)];

    _window = [[UIWindow alloc] init];

    [_window addSubview:_superview];

    UIView.animationsEnabled = NO;
  }

  // Iterates a view's subviews recursively, calling the block with each one.
  void IterateSubviews(UIView* view, bool (^block)(UIView* subview)) {
    for (UIView* subview in view.subviews) {
      bool should_break = block(subview);

      if (should_break) {
        break;
      }

      IterateSubviews(subview, block);
    }
  }

  // Searches recursively through subviews to find one with the given
  // `accessibility_id`.
  UIView* FindSubview(NSString* accessibility_id) {
    __block UIView* found = nil;

    IterateSubviews(_superview, ^bool(UIView* subview) {
      if (subview.accessibilityIdentifier == accessibility_id) {
        found = subview;

        return true;
      }

      return false;
    });

    return found;
  }

  // Expects a subview with the given `accessibility_id` to either exist or
  // or not.
  void ExpectSubview(NSString* accessibility_id, bool exists) {
    UIView* subview = FindSubview(accessibility_id);

    if (exists) {
      EXPECT_NE(subview, nil);
    } else {
      EXPECT_EQ(subview, nil);
    }
  }

  // Returns a count of subviews of the given `klass`.
  int CountSubviewsWithClass(UIView* view, Class klass) {
    __block int count = 0;

    IterateSubviews(view, ^bool(UIView* subview) {
      if ([subview class] == klass) {
        count++;
      }

      return false;
    });

    return count;
  }

  // Expects `count` subviews of the given `klass` to exist.
  void ExpectSubviewCount(int count, Class klass) {
    int actual_count = CountSubviewsWithClass(_superview, klass);

    EXPECT_EQ(actual_count, count);
  }

 protected:
  base::test::SingleThreadTaskEnvironment _task_environment;
  UIWindow* _window;
  UIView* _superview;
};

// Tests that the module can be initialized, create subviews, and that the
// default module state is displayed.
TEST_F(SafetyCheckViewTest, DisplaysModuleWithDefaultState) {
  SafetyCheckState* state = [[SafetyCheckState alloc]
      initWithUpdateChromeState:UpdateChromeSafetyCheckState::kDefault
                  passwordState:PasswordSafetyCheckState::kDefault
              safeBrowsingState:SafeBrowsingSafetyCheckState::kDefault
                   runningState:RunningSafetyCheckState::kDefault];

  SafetyCheckView* view = [[SafetyCheckView alloc] initWithState:state
                                             contentViewDelegate:nil];

  [_superview addSubview:view];

  ExpectSubviewCount(1, [SafetyCheckView class]);

  // It should initially display one item, i.e. the hero-cell default layout
  // item.
  ExpectSubviewCount(1, [IconDetailView class]);

  ExpectSubview(safety_check::kSafetyCheckViewID, true);
  ExpectSubview(safety_check::kDefaultItemID, true);

  ExpectSubview(safety_check::kAllSafeItemID, false);
  ExpectSubview(safety_check::kRunningItemID, false);
  ExpectSubview(safety_check::kUpdateChromeItemID, false);
  ExpectSubview(safety_check::kPasswordItemID, false);
  ExpectSubview(safety_check::kSafeBrowsingItemID, false);
}

// Tests that the module can be initialized, create subviews, and that the
// running module state is displayed.
TEST_F(SafetyCheckViewTest, DisplaysModuleWithRunningState) {
  SafetyCheckState* state = [[SafetyCheckState alloc]
      initWithUpdateChromeState:UpdateChromeSafetyCheckState::kDefault
                  passwordState:PasswordSafetyCheckState::kDefault
              safeBrowsingState:SafeBrowsingSafetyCheckState::kDefault
                   runningState:RunningSafetyCheckState::kRunning];

  SafetyCheckView* view = [[SafetyCheckView alloc] initWithState:state
                                             contentViewDelegate:nil];

  [_superview addSubview:view];

  ExpectSubviewCount(1, [SafetyCheckView class]);

  // It should initially display one item, i.e. the hero-cell default layout
  // item.
  ExpectSubviewCount(1, [IconDetailView class]);

  ExpectSubview(safety_check::kSafetyCheckViewID, true);
  ExpectSubview(safety_check::kRunningItemID, true);

  ExpectSubview(safety_check::kDefaultItemID, false);
  ExpectSubview(safety_check::kAllSafeItemID, false);
  ExpectSubview(safety_check::kUpdateChromeItemID, false);
  ExpectSubview(safety_check::kPasswordItemID, false);
  ExpectSubview(safety_check::kSafeBrowsingItemID, false);
}

// Tests that the module can be initialized, create subviews, and that the
// module state for a single check issue (passwords) is displayed.
TEST_F(SafetyCheckViewTest, DisplaysModuleWithSinglePasswordsIssue) {
  SafetyCheckState* state = [[SafetyCheckState alloc]
      initWithUpdateChromeState:UpdateChromeSafetyCheckState::kDefault
                  passwordState:PasswordSafetyCheckState::
                                    kUnmutedCompromisedPasswords
              safeBrowsingState:SafeBrowsingSafetyCheckState::kDefault
                   runningState:RunningSafetyCheckState::kDefault];

  SafetyCheckView* view = [[SafetyCheckView alloc] initWithState:state
                                             contentViewDelegate:nil];

  [_superview addSubview:view];

  ExpectSubviewCount(1, [SafetyCheckView class]);

  // It should initially display one item, i.e. the hero-cell default layout
  // item.
  ExpectSubviewCount(1, [IconDetailView class]);

  ExpectSubview(safety_check::kSafetyCheckViewID, true);
  ExpectSubview(safety_check::kPasswordItemID, true);

  ExpectSubview(safety_check::kRunningItemID, false);
  ExpectSubview(safety_check::kDefaultItemID, false);
  ExpectSubview(safety_check::kAllSafeItemID, false);
  ExpectSubview(safety_check::kUpdateChromeItemID, false);
  ExpectSubview(safety_check::kSafeBrowsingItemID, false);
}

// Tests that the module can be initialized, create subviews, and that the
// module state for a single check issue (safe browsing) is displayed.
TEST_F(SafetyCheckViewTest, DisplaysModuleWithSingleSafeBrowsingIssue) {
  SafetyCheckState* state = [[SafetyCheckState alloc]
      initWithUpdateChromeState:UpdateChromeSafetyCheckState::kDefault
                  passwordState:PasswordSafetyCheckState::kDefault
              safeBrowsingState:SafeBrowsingSafetyCheckState::kUnsafe
                   runningState:RunningSafetyCheckState::kDefault];

  SafetyCheckView* view = [[SafetyCheckView alloc] initWithState:state
                                             contentViewDelegate:nil];

  [_superview addSubview:view];

  ExpectSubviewCount(1, [SafetyCheckView class]);

  // It should initially display one item, i.e. the hero-cell default layout
  // item.
  ExpectSubviewCount(1, [IconDetailView class]);

  ExpectSubview(safety_check::kSafetyCheckViewID, true);
  ExpectSubview(safety_check::kSafeBrowsingItemID, true);

  ExpectSubview(safety_check::kRunningItemID, false);
  ExpectSubview(safety_check::kDefaultItemID, false);
  ExpectSubview(safety_check::kAllSafeItemID, false);
  ExpectSubview(safety_check::kUpdateChromeItemID, false);
  ExpectSubview(safety_check::kPasswordItemID, false);
}

// Tests that the module can be initialized, create subviews, and that the
// module state for a single check issue (update chrome) is displayed.
TEST_F(SafetyCheckViewTest, DisplaysModuleWithSingleUpdateChromeIssue) {
  SafetyCheckState* state = [[SafetyCheckState alloc]
      initWithUpdateChromeState:UpdateChromeSafetyCheckState::kOutOfDate
                  passwordState:PasswordSafetyCheckState::kDefault
              safeBrowsingState:SafeBrowsingSafetyCheckState::kDefault
                   runningState:RunningSafetyCheckState::kDefault];

  SafetyCheckView* view = [[SafetyCheckView alloc] initWithState:state
                                             contentViewDelegate:nil];

  [_superview addSubview:view];

  ExpectSubviewCount(1, [SafetyCheckView class]);

  // It should initially display one item, i.e. the hero-cell default layout
  // item.
  ExpectSubviewCount(1, [IconDetailView class]);

  ExpectSubview(safety_check::kSafetyCheckViewID, true);
  ExpectSubview(safety_check::kUpdateChromeItemID, true);

  ExpectSubview(safety_check::kRunningItemID, false);
  ExpectSubview(safety_check::kDefaultItemID, false);
  ExpectSubview(safety_check::kAllSafeItemID, false);
  ExpectSubview(safety_check::kSafeBrowsingItemID, false);
  ExpectSubview(safety_check::kPasswordItemID, false);
}

// Tests that the module can be initialized, create subviews, and that the
// module state for a multiple check issues (update chrome & passwords) are
// displayed.
TEST_F(SafetyCheckViewTest, DisplaysModuleWithPasswordAndUpdateChromeIssues) {
  SafetyCheckState* state = [[SafetyCheckState alloc]
      initWithUpdateChromeState:UpdateChromeSafetyCheckState::kOutOfDate
                  passwordState:PasswordSafetyCheckState::
                                    kUnmutedCompromisedPasswords
              safeBrowsingState:SafeBrowsingSafetyCheckState::kDefault
                   runningState:RunningSafetyCheckState::kDefault];

  SafetyCheckView* view = [[SafetyCheckView alloc] initWithState:state
                                             contentViewDelegate:nil];

  [_superview addSubview:view];

  ExpectSubviewCount(1, [SafetyCheckView class]);

  // It should initially display two items, i.e. the multi-row layout.
  ExpectSubviewCount(2, [IconDetailView class]);

  ExpectSubview(safety_check::kSafetyCheckViewID, true);
  ExpectSubview(safety_check::kUpdateChromeItemID, true);
  ExpectSubview(safety_check::kPasswordItemID, true);

  ExpectSubview(safety_check::kRunningItemID, false);
  ExpectSubview(safety_check::kDefaultItemID, false);
  ExpectSubview(safety_check::kAllSafeItemID, false);
  ExpectSubview(safety_check::kSafeBrowsingItemID, false);
}

// Tests that the module can be initialized, create subviews, and that the
// module state for a multiple check issues (passwords & safe browsing) are
// displayed.
TEST_F(SafetyCheckViewTest, DisplaysModuleWithPasswordAndSafeBrowsingIssues) {
  SafetyCheckState* state = [[SafetyCheckState alloc]
      initWithUpdateChromeState:UpdateChromeSafetyCheckState::kDefault
                  passwordState:PasswordSafetyCheckState::
                                    kUnmutedCompromisedPasswords
              safeBrowsingState:SafeBrowsingSafetyCheckState::kUnsafe
                   runningState:RunningSafetyCheckState::kDefault];

  SafetyCheckView* view = [[SafetyCheckView alloc] initWithState:state
                                             contentViewDelegate:nil];

  [_superview addSubview:view];

  ExpectSubviewCount(1, [SafetyCheckView class]);

  // It should initially display two items, i.e. the multi-row layout.
  ExpectSubviewCount(2, [IconDetailView class]);

  ExpectSubview(safety_check::kSafetyCheckViewID, true);
  ExpectSubview(safety_check::kPasswordItemID, true);
  ExpectSubview(safety_check::kSafeBrowsingItemID, true);

  ExpectSubview(safety_check::kUpdateChromeItemID, false);
  ExpectSubview(safety_check::kRunningItemID, false);
  ExpectSubview(safety_check::kDefaultItemID, false);
  ExpectSubview(safety_check::kAllSafeItemID, false);
}

// Tests that the module can be initialized, create subviews, and that the
// module state for a multiple check issues (update chrome & safe browsing) are
// displayed.
TEST_F(SafetyCheckViewTest,
       DisplaysModuleWithUpdateChromeAndSafeBrowsingIssues) {
  SafetyCheckState* state = [[SafetyCheckState alloc]
      initWithUpdateChromeState:UpdateChromeSafetyCheckState::kOutOfDate
                  passwordState:PasswordSafetyCheckState::kDefault
              safeBrowsingState:SafeBrowsingSafetyCheckState::kUnsafe
                   runningState:RunningSafetyCheckState::kDefault];

  SafetyCheckView* view = [[SafetyCheckView alloc] initWithState:state
                                             contentViewDelegate:nil];

  [_superview addSubview:view];

  ExpectSubviewCount(1, [SafetyCheckView class]);

  // It should initially display two items, i.e. the multi-row layout.
  ExpectSubviewCount(2, [IconDetailView class]);

  ExpectSubview(safety_check::kSafetyCheckViewID, true);
  ExpectSubview(safety_check::kUpdateChromeItemID, true);
  ExpectSubview(safety_check::kSafeBrowsingItemID, true);

  ExpectSubview(safety_check::kPasswordItemID, false);
  ExpectSubview(safety_check::kRunningItemID, false);
  ExpectSubview(safety_check::kDefaultItemID, false);
  ExpectSubview(safety_check::kAllSafeItemID, false);
}

// Tests correctly generating a string representation of `SafetyCheckItemType`.
TEST_F(SafetyCheckViewTest, CreatesSafetyCheckItemType) {
  EXPECT_TRUE([NameForSafetyCheckItemType(SafetyCheckItemType::kAllSafe)
      isEqualToString:@"SafetyCheckItemType::kAllSafe"]);
  EXPECT_TRUE([NameForSafetyCheckItemType(SafetyCheckItemType::kRunning)
      isEqualToString:@"SafetyCheckItemType::kRunning"]);
  EXPECT_TRUE([NameForSafetyCheckItemType(SafetyCheckItemType::kUpdateChrome)
      isEqualToString:@"SafetyCheckItemType::kUpdateChrome"]);
  EXPECT_TRUE([NameForSafetyCheckItemType(SafetyCheckItemType::kPassword)
      isEqualToString:@"SafetyCheckItemType::kPassword"]);
  EXPECT_TRUE([NameForSafetyCheckItemType(SafetyCheckItemType::kSafeBrowsing)
      isEqualToString:@"SafetyCheckItemType::kSafeBrowsing"]);
  EXPECT_TRUE([NameForSafetyCheckItemType(SafetyCheckItemType::kDefault)
      isEqualToString:@"SafetyCheckItemType::kDefault"]);
}

// Tests correctly finding the corresponding `SafetyCheckItemType`
// given its string representation.
TEST_F(SafetyCheckViewTest, FindsSafetyCheckItemTypeFromName) {
  EXPECT_EQ(SafetyCheckItemTypeForName(@"SafetyCheckItemType::kAllSafe"),
            SafetyCheckItemType::kAllSafe);
  EXPECT_EQ(SafetyCheckItemTypeForName(@"SafetyCheckItemType::kRunning"),
            SafetyCheckItemType::kRunning);
  EXPECT_EQ(SafetyCheckItemTypeForName(@"SafetyCheckItemType::kUpdateChrome"),
            SafetyCheckItemType::kUpdateChrome);
  EXPECT_EQ(SafetyCheckItemTypeForName(@"SafetyCheckItemType::kPassword"),
            SafetyCheckItemType::kPassword);
  EXPECT_EQ(SafetyCheckItemTypeForName(@"SafetyCheckItemType::kSafeBrowsing"),
            SafetyCheckItemType::kSafeBrowsing);
  EXPECT_EQ(SafetyCheckItemTypeForName(@"SafetyCheckItemType::kDefault"),
            SafetyCheckItemType::kDefault);
}
