// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_DEFAULT_BROWSER_UI_DEFAULT_BROWSER_VIEW_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_DEFAULT_BROWSER_UI_DEFAULT_BROWSER_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/content_suggestions/ui_bundled/default_browser/ui/default_browser_commands.h"

@class DefaultBrowserConfig;

@interface DefaultBrowserView : UIView

// Default initializer.
- (instancetype)initWithConfig:(DefaultBrowserConfig*)config;

// The object that should handler user events.
@property(nonatomic, weak) id<DefaultBrowserCommands> commandHandler;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_DEFAULT_BROWSER_UI_DEFAULT_BROWSER_VIEW_H_
