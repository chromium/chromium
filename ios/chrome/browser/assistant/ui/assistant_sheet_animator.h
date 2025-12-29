// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_SHEET_ANIMATOR_H_
#define IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_SHEET_ANIMATOR_H_

#import <UIKit/UIKit.h>

@class AssistantSheetViewController;

// Handles the presentation and dismissal animations for the Assistant Sheet.
@interface AssistantSheetAnimator : NSObject

// Animates the presentation of the assistant sheet (Slide Up from bottom).
- (void)animatePresentation:(AssistantSheetViewController*)viewController
                 completion:(void (^)(void))completion;

// Animates the dismissal of the assistant sheet (Slide Down to bottom).
- (void)animateDismissal:(AssistantSheetViewController*)viewController
              completion:(void (^)(void))completion;

@end

#endif  // IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_SHEET_ANIMATOR_H_
