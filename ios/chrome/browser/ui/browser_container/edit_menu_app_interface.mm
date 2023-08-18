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
NSArray* ExtractMenuElements(UIView* view,
                             CGFloat menu_offset,
                             CGFloat menu_width) {
  // The type of the edit menu cell depends on the OS version.
  bool is_edit_menu_cell = false;
  if (@available(iOS 16, *)) {
    is_edit_menu_cell =
        [view isKindOfClass:NSClassFromString(@"_UIEditMenuListViewCell")];
  } else {
    // Back and forward buttons have the same type as the actions, so exclude
    // them using the accessibility labels.
    is_edit_menu_cell =
        [view isKindOfClass:NSClassFromString(@"UICalloutBarButton")] &&
        ![view.accessibilityIdentifier
            isEqualToString:@"show.previous.items.menu.button"] &&
        ![view.accessibilityIdentifier
            isEqualToString:@"show.next.items.menu.button"];
  }

  if (is_edit_menu_cell) {
    // Exclude views that are hidden or outside of the scrollview visible part.
    if (view.hidden) {
      return @[];
    }
    // Consider the center to avoid rounding issues.
    CGFloat center = CGRectGetMidX(view.frame);
    if (center < menu_offset || center >= menu_offset + menu_width) {
      return @[];
    }
    return @[ view.accessibilityLabel ];
  }

  NSMutableArray* sub_views_elements = [NSMutableArray array];
  CGFloat new_width = std::min(menu_width, view.frame.size.width);
  if ([view isKindOfClass:[UIScrollView class]]) {
    UIScrollView* scroll_view = (UIScrollView*)view;
    menu_offset = scroll_view.contentOffset.x;
  }
  for (UIView* subView in view.subviews) {
    [sub_views_elements
        addObjectsFromArray:ExtractMenuElements(subView, menu_offset,
                                                new_width)];
  }
  return sub_views_elements;
}

}  // namespace

@implementation EditMenuAppInterface

+ (id<GREYMatcher>)editMenuMatcher {
  if (@available(iOS 16, *)) {
    return grey_kindOfClassName(@"_UIEditMenuListView");
  } else {
    return grey_kindOfClassName(@"UICalloutBar");
  }
}

+ (id<GREYMatcher>)editMenuButtonMatcher {
  if (@available(iOS 16.0, *)) {
    return grey_kindOfClass(NSClassFromString(@"_UIEditMenuListViewCell"));
  } else {
    return grey_kindOfClass(NSClassFromString(@"UICalloutBarButton"));
  }
}

+ (id<GREYMatcher>)editMenuNextButtonMatcher {
  id<GREYMatcher> editMenu = [EditMenuAppInterface editMenuMatcher];
  if (@available(iOS 16, *)) {
    id<GREYMatcher> nextButton = grey_allOf(
        grey_ancestor(editMenu), grey_kindOfClassName(@"_UIEditMenuPageButton"),
        grey_accessibilityLabel(@"Forward"), nil);
    return nextButton;
  } else {
    id<GREYMatcher> nextButton = grey_allOf(
        grey_ancestor(editMenu), grey_kindOfClassName(@"UICalloutBarButton"),
        grey_accessibilityID(@"show.next.items.menu.button"), nil);
    return nextButton;
  }
}

+ (id<GREYMatcher>)editMenuPreviousButtonMatcher {
  id<GREYMatcher> editMenu = [EditMenuAppInterface editMenuMatcher];
  if (@available(iOS 16, *)) {
    id<GREYMatcher> previousButton = grey_allOf(
        grey_ancestor(editMenu), grey_kindOfClassName(@"_UIEditMenuPageButton"),
        grey_accessibilityLabel(@"Previous"), nil);
    return previousButton;
  } else {
    id<GREYMatcher> previousButton = grey_allOf(
        grey_ancestor(editMenu), grey_kindOfClassName(@"UICalloutBarButton"),
        grey_accessibilityID(@"show.previous.items.menu.button"), nil);
    return previousButton;
  }
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
  NSMutableArray* menuElements = [NSMutableArray array];
  for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
    UIWindowScene* windowScene =
        base::apple::ObjCCastStrict<UIWindowScene>(scene);
    for (UIWindow* window in windowScene.windows) {
      if ([window isKindOfClass:NSClassFromString(@"ChromeOverlayWindow")]) {
        continue;
      }
      [menuElements
          addObjectsFromArray:ExtractMenuElements(window, 0,
                                                  window.bounds.size.width)];
    }
  }
  return menuElements;
}

@end
