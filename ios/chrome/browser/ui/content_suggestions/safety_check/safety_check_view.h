// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SAFETY_CHECK_SAFETY_CHECK_VIEW_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SAFETY_CHECK_SAFETY_CHECK_VIEW_H_

#import <UIKit/UIKit.h>

@protocol ContentSuggestionsViewControllerAudience;
@class SafetyCheckState;

// A view that displays the Safety Check in the Magic Stack.
//
// This view shows users the current state of the Update Chrome, Password, and
// Safe Browsing check.
@interface SafetyCheckView : UIView

// Initializes the SafetyCheckView with `state`.
- (instancetype)initWithState:(SafetyCheckState*)state;

// The object that should handle user events.
@property(nonatomic, weak) id<ContentSuggestionsViewControllerAudience>
    commandhandler;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SAFETY_CHECK_SAFETY_CHECK_VIEW_H_
