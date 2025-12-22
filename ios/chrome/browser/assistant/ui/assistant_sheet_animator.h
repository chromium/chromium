// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_SHEET_ANIMATOR_H_
#define IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_SHEET_ANIMATOR_H_

#import <UIKit/UIKit.h>

// Handles the presentation and dismissal animations for the Assistant Sheet.
@interface AssistantSheetAnimator : NSObject

// Animates the presentation of the given view (Expand and Fade In).
- (void)animatePresentation:(UIView*)view completion:(void (^)(void))completion;

// Animates the dismissal of the given view (Shrink and Fade Out).
- (void)animateDismissal:(UIView*)view completion:(void (^)(void))completion;

@end

#endif  // IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_SHEET_ANIMATOR_H_
