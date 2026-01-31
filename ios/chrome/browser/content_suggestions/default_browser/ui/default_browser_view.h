// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_DEFAULT_BROWSER_UI_DEFAULT_BROWSER_VIEW_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_DEFAULT_BROWSER_UI_DEFAULT_BROWSER_VIEW_H_

#import <UIKit/UIKit.h>

@protocol DefaultBrowserCommands;
@class DefaultBrowserConfig;

@interface DefaultBrowserView : UIView

// The object that should handler user events.
@property(nonatomic, weak) id<DefaultBrowserCommands> defaultBrowserHandler;

// Default initializer.
- (instancetype)initWithConfig:(DefaultBrowserConfig*)config;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_DEFAULT_BROWSER_UI_DEFAULT_BROWSER_VIEW_H_
