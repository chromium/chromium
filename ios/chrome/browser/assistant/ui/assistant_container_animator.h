// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_ANIMATOR_H_
#define IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_ANIMATOR_H_

#import <UIKit/UIKit.h>

@protocol AssistantContainerAnimatable;
@protocol AssistantContainerPresenter;
@class LayoutState;

@interface AssistantContainerAnimator : NSObject

// Designated initializer with layout state.
- (instancetype)initWithLayoutState:(LayoutState*)layoutState
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Animates the presentation of the assistant container (Slide Up from bottom).
- (void)animatePresentation:
            (UIViewController<AssistantContainerAnimatable>*)viewController
                   animated:(BOOL)animated
                 completion:(void (^)(void))completion;

// Animates the dismissal of the assistant container (Slide Down to bottom).
- (void)animateDismissal:
            (UIViewController<AssistantContainerAnimatable>*)viewController
                animated:(BOOL)animated
              completion:(void (^)(void))completion;

// Animates the presentation of the assistant container side panel.
- (void)animateSidePanelPresentation:
            (UIViewController<AssistantContainerAnimatable>*)viewController
                  baseViewController:
                      (UIViewController<AssistantContainerPresenter>*)
                          baseViewController
                            animated:(BOOL)animated
                          completion:(void (^)(void))completion;

// Animates the dismissal of the assistant container side panel.
- (void)animateSidePanelDismissal:
            (UIViewController<AssistantContainerAnimatable>*)viewController
               baseViewController:
                   (UIViewController<AssistantContainerPresenter>*)
                       baseViewController
                         animated:(BOOL)animated
                       completion:(void (^)(void))completion;

@end

#endif  // IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_ANIMATOR_H_
