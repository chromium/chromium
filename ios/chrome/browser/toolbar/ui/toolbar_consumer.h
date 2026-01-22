// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_TOOLBAR_CONSUMER_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_TOOLBAR_CONSUMER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button_visibility.h"

// Protocol for implementing the toolbar view.
@protocol ToolbarConsumer <NSObject>

// Sets whether the back button is enabled.
- (void)setCanGoBack:(BOOL)canGoBack;

// Sets whether the forward button is enabled.
- (void)setCanGoForward:(BOOL)canGoForward;

// Sets whether the page is loading.
- (void)setIsLoading:(BOOL)isLoading;

// Sets whether the share button is enabled.
- (void)setShareEnabled:(BOOL)enabled;

// Sets whether the toolbar is visible.
- (void)setVisible:(BOOL)visible;

// Sets whether the location indicator should be visible.
- (void)setLocationIndicatorVisible:(BOOL)locationIndicatorVisible
                    forNotification:(NSNotification*)notification;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_TOOLBAR_CONSUMER_H_
