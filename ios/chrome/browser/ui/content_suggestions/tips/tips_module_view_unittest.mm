// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser//ui/content_suggestions/tips/tips_module_view.h"

#import <Foundation/Foundation.h>

#import "base/test/task_environment.h"
#import "components/segmentation_platform/embedder/home_modules/tips_manager/constants.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/icon_detail_view.h"
#import "ios/chrome/browser/ui/content_suggestions/tips/tips_module_state.h"
#import "testing/platform_test.h"

using segmentation_platform::TipIdentifier;

// Tests the `TipsModuleView` and subviews.
class TipsModuleViewTest : public PlatformTest {
 public:
  TipsModuleViewTest() {
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
// correct module state is displayed.
TEST_F(TipsModuleViewTest, DisplaysModuleWithDefaultState) {
  TipsModuleState* state = [[TipsModuleState alloc]
      initWithIdentifier:TipIdentifier::kLensTranslate];

  TipsModuleView* view = [[TipsModuleView alloc] initWithState:state];

  [_superview addSubview:view];

  // It should initially display one item, i.e. the hero-cell default layout
  // item.
  ExpectSubviewCount(1, [IconDetailView class]);

  ExpectSubview(@"kTipsModuleViewID", true);
  ExpectSubview(@"kLensTranslateAccessibilityID", true);
  ExpectSubview(@"kLensShopAccessibilityID", false);
}
