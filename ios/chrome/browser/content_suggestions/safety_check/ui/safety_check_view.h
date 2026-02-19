// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_SAFETY_CHECK_UI_SAFETY_CHECK_VIEW_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_SAFETY_CHECK_UI_SAFETY_CHECK_VIEW_H_

#import <UIKit/UIKit.h>

@class SafetyCheckConfig;
@protocol MagicStackModuleContentViewDelegate;

// A view that displays the Safety Check in the Magic Stack.
//
// This view shows users the current state of the Update Chrome, Password, and
// Safe Browsing check.
@interface SafetyCheckView : UIView

// Initializes the SafetyCheckView with `config` and `contentViewDelegate`.
// TODO(crbug.com/391617946): Refactor content view delegate and methods that
// use it out of the initializer.
- (instancetype)initWithConfig:(SafetyCheckConfig*)config
           contentViewDelegate:
               (id<MagicStackModuleContentViewDelegate>)contentViewDelegate;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_SAFETY_CHECK_UI_SAFETY_CHECK_VIEW_H_
