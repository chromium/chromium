// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_view.h"

#import "base/test/task_environment.h"
#import "ios/chrome/browser/ntp/set_up_list.h"
#import "ios/chrome/browser/ntp/set_up_list_item.h"
#import "ios/chrome/browser/ntp/set_up_list_item_type.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/constants.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_item_view+private.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_item_view.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_item_view_data.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

// Tests the SetUpListView and subviews.
class SetUpListViewTest : public PlatformTest {
 public:
  SetUpListViewTest() {
    SetUpListItemType types[] = {SetUpListItemType::kSignInSync,
                                 SetUpListItemType::kDefaultBrowser,
                                 SetUpListItemType::kAutofill};
    _itemsData = [[NSMutableArray alloc] init];
    for (SetUpListItemType type : types) {
      [_itemsData addObject:[[SetUpListItemViewData alloc] initWithType:type
                                                               complete:NO]];
    }
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

  // Simulates a touch on a button with the given `accessibility_id`.
  void TouchButton(NSString* accessibility_id) {
    UIButton* button = (UIButton*)FindSubview(accessibility_id);
    ASSERT_TRUE(button);
    [button sendActionsForControlEvents:UIControlEventTouchUpInside];
    // Give time for run loop to execute events.
    _task_environment.RunUntilIdle();
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
  NSMutableArray<SetUpListItemViewData*>* _itemsData;
};

// Tests that the list can be initialized, create subviews, and that the
// expand / collapse button functions as it should. Also verifies that
// the items that are expected to be in the list are there at each step.
TEST_F(SetUpListViewTest, ExpandCollapse) {
  SetUpListView* view = [[SetUpListView alloc] initWithItems:_itemsData
                                                    rootView:nil];
  [_superview addSubview:view];

  // It should initially display two items.
  ExpectSubviewCount(2, [SetUpListItemView class]);
  ExpectSubview(set_up_list::kSignInItemID, true);
  ExpectSubview(set_up_list::kDefaultBrowserItemID, true);
  ExpectSubview(set_up_list::kAutofillItemID, false);
  ExpectSubview(set_up_list::kFollowItemID, false);

  // After touching the expand button, the list should show 3 items.
  TouchButton(set_up_list::kExpandButtonID);
  ExpectSubviewCount(3, [SetUpListItemView class]);
  ExpectSubview(set_up_list::kSignInItemID, true);
  ExpectSubview(set_up_list::kDefaultBrowserItemID, true);
  ExpectSubview(set_up_list::kAutofillItemID, true);
  ExpectSubview(set_up_list::kFollowItemID, false);

  // After touching the expand button again, the list should show 2 items.
  TouchButton(set_up_list::kExpandButtonID);
  ExpectSubviewCount(2, [SetUpListItemView class]);
  ExpectSubview(set_up_list::kSignInItemID, true);
  ExpectSubview(set_up_list::kDefaultBrowserItemID, true);
  ExpectSubview(set_up_list::kAutofillItemID, false);
  ExpectSubview(set_up_list::kFollowItemID, false);
}

// Tests that a touch on a SetUpListItemView results in a call to the view\
// delegate.
TEST_F(SetUpListViewTest, TouchSetUpListItemView) {
  SetUpListView* view = [[SetUpListView alloc] initWithItems:_itemsData
                                                    rootView:nil];

  [_superview addSubview:view];

  SetUpListItemView* item_view =
      (SetUpListItemView*)FindSubview(set_up_list::kSignInItemID);
  EXPECT_TRUE(item_view != nil);

  id view_delegate = OCMProtocolMock(@protocol(SetUpListViewDelegate));
  OCMExpect(
      [view_delegate didSelectSetUpListItem:SetUpListItemType::kSignInSync]);
  view.delegate = view_delegate;

  // Simulate a tap on a SetUpListItemView.
  UIGestureRecognizer* gesture_recognizer = item_view.gestureRecognizers[0];
  EXPECT_NE(gesture_recognizer, nil);
  gesture_recognizer.state = UIGestureRecognizerStateEnded;
  [item_view handleTap:(UITapGestureRecognizer*)gesture_recognizer];
  // Give time for run loop to execute events.
  _task_environment.RunUntilIdle();

  [view_delegate verify];
}

// Tests that a tap on the menu button results in a call to the view delegate.
TEST_F(SetUpListViewTest, MenuButton) {
  SetUpListView* view = [[SetUpListView alloc] initWithItems:_itemsData
                                                    rootView:nil];
  [_superview addSubview:view];

  id view_delegate = OCMProtocolMock(@protocol(SetUpListViewDelegate));
  OCMExpect([view_delegate showSetUpListMenuWithButton:[OCMArg any]]);
  view.delegate = view_delegate;

  TouchButton(set_up_list::kMenuButtonID);

  [view_delegate verify];
}

// Tests that a SetUpListItemView can be marked complete.
TEST_F(SetUpListViewTest, SetUpListItemViewMarkComplete) {
  SetUpListView* view = [[SetUpListView alloc] initWithItems:_itemsData
                                                    rootView:nil];
  [_superview addSubview:view];

  SetUpListItemView* item_view =
      (SetUpListItemView*)FindSubview(set_up_list::kSignInItemID);
  EXPECT_TRUE(item_view != nil);
  EXPECT_FALSE(item_view.complete);

  [view markItemComplete:SetUpListItemType::kSignInSync completion:nil];
  // Give time for run loop to execute events.
  _task_environment.RunUntilIdle();

  EXPECT_TRUE(item_view.complete);
}

// Tests that with only 2 items, no expand button is present.
TEST_F(SetUpListViewTest, NoExpandButton) {
  NSArray<SetUpListItemViewData*>* first_two_items =
      [_itemsData subarrayWithRange:NSMakeRange(0, 2)];
  SetUpListView* view = [[SetUpListView alloc] initWithItems:first_two_items
                                                    rootView:nil];

  [_superview addSubview:view];

  SetUpListItemView* expand_button =
      (SetUpListItemView*)FindSubview(set_up_list::kExpandButtonID);
  EXPECT_TRUE(expand_button == nil);
}

// Tests that the "All Set" can be shown.
TEST_F(SetUpListViewTest, AllSetView) {
  SetUpListView* view = [[SetUpListView alloc] initWithItems:_itemsData
                                                    rootView:nil];
  [_superview addSubview:view];

  ExpectSubview(set_up_list::kAllSetID, false);

  [view showDoneWithAnimations:nil];
  // Give time for run loop to execute events.
  _task_environment.RunUntilIdle();

  ExpectSubview(set_up_list::kAllSetID, true);
}
