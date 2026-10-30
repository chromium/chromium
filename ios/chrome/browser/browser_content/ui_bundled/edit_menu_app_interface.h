// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_CONTENT_UI_BUNDLED_EDIT_MENU_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_BROWSER_CONTENT_UI_BUNDLED_EDIT_MENU_APP_INTERFACE_H_

#import <UIKit/UIKit.h>

@protocol GREYMatcher;

// App interface to interact with the Edit Menu.
@interface EditMenuAppInterface : NSObject

// Matcher for the full Edit Menu.
+ (id<GREYMatcher>)editMenuMatcher;

// Matcher for a generic button of the Edit menu.
+ (id<GREYMatcher>)editMenuButtonMatcher;

// Matcher for the next button in the Edit Menu.
+ (id<GREYMatcher>)editMenuNextButtonMatcher;

// Matcher for the previous button in the Edit Menu.
+ (id<GREYMatcher>)editMenuPreviousButtonMatcher;

// Matcher for the button in the Edit Menu with the accessibility label
// `accessibilityLabel`.
+ (id<GREYMatcher>)editMenuActionWithAccessibilityLabel:
    (NSString*)accessibilityLabel;

// Matcher for the button in the Edit Menu with the accessibility label
// `accessibilityLabel` when in context menu format.
+ (id<GREYMatcher>)contextMenuActionWithAccessibilityLabel:
    (NSString*)accessibilityLabel;

// Returns a matcher for the Link to Text button in the Edit Menu.
+ (id<GREYMatcher>)editMenuLinkToTextButtonMatcher;

// Returns a matcher for the Copy button in the Edit Menu.
+ (id<GREYMatcher>)editMenuCopyButtonMatcher;

// Returns a matcher for the Cut button in the Edit Menu.
+ (id<GREYMatcher>)editMenuCutButtonMatcher;

// Returns a matcher for the Paste button in the Edit Menu.
+ (id<GREYMatcher>)editMenuPasteButtonMatcher;

// Returns the accessibility IDs of menu items visible on screen.
+ (NSArray<NSString*>*)editMenuActions;

@end

#endif  // IOS_CHROME_BROWSER_BROWSER_CONTENT_UI_BUNDLED_EDIT_MENU_APP_INTERFACE_H_
