// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_ANIMATOR_H_
#define IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_ANIMATOR_H_

#import <UIKit/UIKit.h>

@class AssistantContainerViewController;

@interface AssistantContainerAnimator : NSObject

// Animates the presentation of the assistant container (Slide Up from bottom).
- (void)animatePresentation:(AssistantContainerViewController*)viewController
                 completion:(void (^)(void))completion;

// Animates the dismissal of the assistant container (Slide Down to bottom).
- (void)animateDismissal:(AssistantContainerViewController*)viewController
              completion:(void (^)(void))completion;

@end

#endif  // IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_ANIMATOR_H_
