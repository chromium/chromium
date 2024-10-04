// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_CONSUMER_H_

#import <UIKit/UIKit.h>

enum class ToolbarTabGridButtonStyle;

// ToolbarConsumer sets the current appearance of the Toolbar.
@protocol ToolbarConsumer
// Updates the toolbar with the current forward navigation state.
- (void)setCanGoForward:(BOOL)canGoForward;
// Updates the toolbar with the current back navigation state.
- (void)setCanGoBack:(BOOL)canGoBack;
// Updates the toolbar with the current loading state.
- (void)setLoadingState:(BOOL)loading;
// Updates the toolbar with the current progress of the loading WebState.
- (void)setLoadingProgressFraction:(double)progress;
// Updates the toolbar with the current number of total tabs. If the tab is
// added, `addedInBackground` is set to YES if the tab is added in background.
// NO otherwise.
- (void)setTabCount:(int)tabCount addedInBackground:(BOOL)addedInBackground;
// Sets whether the voice search is enabled or not.
- (void)setVoiceSearchEnabled:(BOOL)enabled;
// Sets whether the share menu is enabled.
- (void)setShareMenuEnabled:(BOOL)enabled;
// Sets whether the toolbar is displaying for an NTP.
- (void)setIsNTP:(BOOL)isNTP;
// Sets the page theme color.
- (void)setPageThemeColor:(UIColor*)themeColor;
// Sets the under page background color.
- (void)setUnderPageBackgroundColor:(UIColor*)underPageBackgroundColor;
// Sets the Tab Grid button style.
- (void)setTabGridButtonStyle:(ToolbarTabGridButtonStyle)tabGridButtonStyle;
@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_CONSUMER_H_
