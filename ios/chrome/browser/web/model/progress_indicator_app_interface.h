// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_PROGRESS_INDICATOR_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_PROGRESS_INDICATOR_APP_INTERFACE_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

@protocol GREYMatcher;

// ProgressIndicatorAppInterface contains helpers for interacting with
// MDCProgressViews. These helpers are compiled into the app binary and can be
// called from either app or test code.
@interface ProgressIndicatorAppInterface : NSObject

// Matcher for an MDCProgressView with `progress`.
+ (id<GREYMatcher>)progressViewWithProgress:(CGFloat)progress;

@end

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_PROGRESS_INDICATOR_APP_INTERFACE_H_
