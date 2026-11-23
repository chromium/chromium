// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_TOOLBAR_BUTTONS_CONSUMER_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_TOOLBAR_BUTTONS_CONSUMER_H_

#import <UIKit/UIKit.h>

// The types of the toolbar's control buttons.
typedef NS_ENUM(NSUInteger, ToolbarButtonType) {
  ToolbarButtonTypeBack,
  ToolbarButtonTypeForward,
  ToolbarButtonTypeReload,
  ToolbarButtonTypeStop,
  ToolbarButtonTypeShare,
  ToolbarButtonTypeAssistant,
  ToolbarButtonTypeTabGrid,
  ToolbarButtonTypeTools,
};

// Receives the state of the toolbar's control buttons. Implemented by every
// surface showing them, so that they all show the same state.
@protocol ToolbarButtonsConsumer <NSObject>

// Sets whether the back button is enabled.
- (void)setCanGoBack:(BOOL)canGoBack;

// Sets whether the forward button is enabled.
- (void)setCanGoForward:(BOOL)canGoForward animated:(BOOL)animated;

// Sets whether the share button is enabled.
- (void)setShareEnabled:(BOOL)enabled;

// Sets whether the current page is the NTP, if it is the Start Surface, whether
// the page is loading, and the `progress` between 0.0 and 1.0 for the loading
// progress bar.
- (void)setNTPVisible:(BOOL)ntpVisible
       isStartSurface:(BOOL)isStartSurface
            isLoading:(BOOL)isLoading
      loadingProgress:(double)progress;

// Sets the context menu for the button with `buttonType`.
- (void)setMenu:(UIMenu*)menu forButtonType:(ToolbarButtonType)buttonType;

// Sets the tab count to display.
- (void)updateTabCount:(NSUInteger)tabCount;

// Sets whether the active web state is inside a tab group.
- (void)setInTabGroup:(BOOL)inTabGroup;

// Sets the state of the assistant button.
- (void)setAssistantButtonVisible:(BOOL)visible enabled:(BOOL)enabled;

// Sets whether the active tab is currently controlled by the actor.
- (void)setActorControlled:(BOOL)actorControlled;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_TOOLBAR_BUTTONS_CONSUMER_H_
