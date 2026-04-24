// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_TOOLBAR_CONSUMER_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_TOOLBAR_CONSUMER_H_

#import <Foundation/Foundation.h>

// The types of button for which a menu can be provided.
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

// Protocol for implementing the toolbar view.
@protocol ToolbarConsumer

// Sets whether the back button is enabled.
- (void)setCanGoBack:(BOOL)canGoBack;

// Sets whether the forward button is enabled.
- (void)setCanGoForward:(BOOL)canGoForward;

// Sets whether the page is loading.
- (void)setIsLoading:(BOOL)isLoading;

// Sets the `progress` between 0.0 and 1.0 for the loading progress bar.
- (void)setLoadingProgress:(double)progress;

// Sets whether the share button is enabled.
- (void)setShareEnabled:(BOOL)enabled;

// Sets whether the toolbar is visible.
- (void)setVisible:(BOOL)visible;

// Sets whether the current page is the NTP.
- (void)setNTPVisible:(BOOL)ntpVisible;

// Sets the context menu for the Toolbar button with `buttonType`.
- (void)setMenu:(UIMenu*)menu forButtonType:(ToolbarButtonType)buttonType;

// Sets whether the location indicator should be visible.
- (void)setLocationIndicatorVisible:(BOOL)locationIndicatorVisible
                    forNotification:(NSNotification*)notification;

// Shows the banner promo view.
- (void)showBannerPromo;

// Hides the banner promo view.
- (void)hideBannerPromo;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_TOOLBAR_CONSUMER_H_
