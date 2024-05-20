// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SAFETY_CHECK_SAFETY_CHECK_VIEW_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SAFETY_CHECK_SAFETY_CHECK_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_magic_stack_consumer.h"

@protocol SafetyCheckAudience;
@class SafetyCheckState;
@protocol MagicStackModuleContentViewDelegate;

// A view that displays the Safety Check in the Magic Stack.
//
// This view shows users the current state of the Update Chrome, Password, and
// Safe Browsing check.
@interface SafetyCheckView : UIView <SafetyCheckMagicStackConsumer>

// Initializes the SafetyCheckView with `state` and `contentViewDelegate`.
- (instancetype)initWithState:(SafetyCheckState*)state
          contentViewDelegate:
              (id<MagicStackModuleContentViewDelegate>)contentViewDelegate;

// The object that should handle user events.
@property(nonatomic, weak) id<SafetyCheckAudience> audience;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SAFETY_CHECK_SAFETY_CHECK_VIEW_H_
