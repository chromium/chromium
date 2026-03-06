// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_CONSUMER_H_
#define IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_CONSUMER_H_

#import <UIKit/UIKit.h>

// The types of button for which a menu can be provided.
typedef NS_ENUM(NSUInteger, AppBarButtonType) {
  AppBarButtonTypeAssistant,
  AppBarButtonTypeNewTab,
  AppBarButtonTypeTabGrid,
};

// Consumer of the app bar.
@protocol AppBarConsumer <NSObject>

// Updates the tab count displayed in the app bar.
- (void)updateTabCount:(NSUInteger)count;

// Sets whether the tab grid is visible or not.
- (void)setTabGridVisible:(BOOL)tabGridVisible;

// Sets whether the tab groups page in the tab grid is visible.
- (void)setTabGroupsPageVisible:(BOOL)tabGroupsPageVisible;

// Sets whether a tab group is being shown in the tab grid.
- (void)setTabGroupVisible:(BOOL)tabGroupVisible;

// Sets the context menu for the App Bar button with `buttonType`.
- (void)setMenu:(UIMenu*)menu forButtonType:(AppBarButtonType)buttonType;

// Enables or disables the buttons.
- (void)setButtonsEnabled:(BOOL)enabled;

@end

#endif  // IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_CONSUMER_H_
