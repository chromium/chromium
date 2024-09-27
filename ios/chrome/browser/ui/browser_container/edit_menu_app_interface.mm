// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_container/edit_menu_app_interface.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/testing/earl_grey/earl_grey_app.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Recursively iterate through the `view` subviews trees and returns the
// accessibility labels of the visible edit menu actions.
// `menu_offset` and `menu_width` are used in the recursion to exclude invisible
// items.
// Items are returned as a dictionary <x_position>,<items_label> so they can
// be sorted.
// As visible items are presented horizontally, there should not be any
// collision on the X coordinate.
NSDictionary* ExtractMenuElements(UIView* view,
                                  CGFloat menu_offset,
                                  CGFloat menu_width) {
  // The type of the edit menu cell depends on the OS version.
  bool is_edit_menu_cell = false;
  is_edit_menu_cell =
      [view isKindOfClass:NSClassFromString(@"_UIEditMenuListViewCell")];

  if (is_edit_menu_cell) {
    // Exclude views that are hidden or outside of the scrollview visible part.
    if (view.hidden) {
      return @{};
    }
    // Consider the center to avoid rounding issues.
    CGFloat center = CGRectGetMidX(view.frame);
    if (center < menu_offset || center >= menu_offset + menu_width) {
      return @{};
    }
    return @{@(center) : view.accessibilityLabel};
  }

  NSMutableDictionary* sub_views_elements = [NSMutableDictionary dictionary];
  CGFloat new_width = std::min(menu_width, view.frame.size.width);
  if ([view isKindOfClass:[UIScrollView class]]) {
    UIScrollView* scroll_view = (UIScrollView*)view;
    menu_offset = scroll_view.contentOffset.x;
  }
  for (UIView* subView in view.subviews) {
    [sub_views_elements
        addEntriesFromDictionary:ExtractMenuElements(subView, menu_offset,
                                                     new_width)];
  }
  return sub_views_elements;
}

}  // namespace

@implementation EditMenuAppInterface

+ (id<GREYMatcher>)editMenuMatcher {
  return grey_kindOfClassName(@"_UIEditMenuListView");
}

+ (id<GREYMatcher>)editMenuButtonMatcher {
  return grey_kindOfClass(NSClassFromString(@"_UIEditMenuListViewCell"));
}

+ (id<GREYMatcher>)editMenuNextButtonMatcher {
  id<GREYMatcher> editMenu = [EditMenuAppInterface editMenuMatcher];
  id<GREYMatcher> nextButton = grey_allOf(
      grey_ancestor(editMenu), grey_kindOfClassName(@"_UIEditMenuPageButton"),
      grey_accessibilityLabel(@"Forward"), nil);
  return nextButton;
}

+ (id<GREYMatcher>)editMenuPreviousButtonMatcher {
  id<GREYMatcher> editMenu = [EditMenuAppInterface editMenuMatcher];
  id<GREYMatcher> previousButton = grey_allOf(
      grey_ancestor(editMenu), grey_kindOfClassName(@"_UIEditMenuPageButton"),
      grey_accessibilityLabel(@"Previous"), nil);
  return previousButton;
}

+ (id<GREYMatcher>)editMenuActionWithAccessibilityLabel:
    (NSString*)accessibilityLabel {
  id<GREYMatcher> editMenu = [EditMenuAppInterface editMenuMatcher];
  id<GREYMatcher> editMenuButton = [EditMenuAppInterface editMenuButtonMatcher];
  id<GREYMatcher> actionButton =
      grey_allOf(grey_ancestor(editMenu), editMenuButton,
                 grey_accessibilityLabel(accessibilityLabel), nil);
  return actionButton;
}

+ (id<GREYMatcher>)editMenuLinkToTextButtonMatcher {
  return [EditMenuAppInterface
      editMenuActionWithAccessibilityLabel:l10n_util::GetNSString(
                                               IDS_IOS_SHARE_LINK_TO_TEXT)];
}

+ (id<GREYMatcher>)editMenuCopyButtonMatcher {
  return [EditMenuAppInterface editMenuActionWithAccessibilityLabel:@"Copy"];
}

+ (id<GREYMatcher>)editMenuCutButtonMatcher {
  return [EditMenuAppInterface editMenuActionWithAccessibilityLabel:@"Cut"];
}

+ (id<GREYMatcher>)editMenuPasteButtonMatcher {
  return [EditMenuAppInterface editMenuActionWithAccessibilityLabel:@"Paste"];
}

+ (NSArray*)editMenuActions {
  NSMutableDictionary* menuElements = [NSMutableDictionary dictionary];
  for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
    UIWindowScene* windowScene =
        base::apple::ObjCCastStrict<UIWindowScene>(scene);
    for (UIWindow* window in windowScene.windows) {
      if ([window isKindOfClass:NSClassFromString(@"ChromeOverlayWindow")]) {
        continue;
      }
      [menuElements
          addEntriesFromDictionary:ExtractMenuElements(
                                       window, 0, window.bounds.size.width)];
    }
  }
  NSArray* sortedKeys =
      [[menuElements allKeys] sortedArrayUsingSelector:@selector(compare:)];
  NSMutableArray* sortedValues = [NSMutableArray array];
  for (NSNumber* key in sortedKeys) {
    [sortedValues addObject:[menuElements objectForKey:key]];
  }
  return sortedValues;
}

@end
