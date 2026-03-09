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

// Delegate for the content view.
@property(nonatomic, weak)
    id<MagicStackModuleContentViewDelegate> contentViewDelegate;

// Initializes the SafetyCheckView with `config`.
- (instancetype)initWithConfig:(SafetyCheckConfig*)config;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_SAFETY_CHECK_UI_SAFETY_CHECK_VIEW_H_
