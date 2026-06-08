// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COBROWSE_UI_ASSISTANT_AIM_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_COBROWSE_UI_ASSISTANT_AIM_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/cobrowse/ui/assistant_aim_consumer.h"
#import "ios/chrome/browser/shared/ui/util/ui_view_controller_with_display_tracing.h"

@protocol AssistantAIMMutator;
@class AssistantAIMViewController;
@class ComposeboxInputPlateViewController;

// Delegate for the AssistantAIMViewController.
@protocol AssistantAIMViewControllerDelegate <NSObject>

// Called when the close button is tapped.
- (void)assistantAIMViewControllerDidTapClose:
    (AssistantAIMViewController*)viewController;

// Called when the keyboard is shown.
- (void)assistantAIMViewController:(AssistantAIMViewController*)viewController
       didShowKeyboardWithDuration:(NSTimeInterval)duration
                             curve:(UIViewAnimationCurve)curve;

// Called when the keyboard is hidden.
- (void)assistantAIMViewControllerDidHideKeyboard:
    (AssistantAIMViewController*)viewController;

// Called when the UI requests ending editing.
- (void)assistantAIMViewControllerDidRequestEndEditing:
    (AssistantAIMViewController*)viewController;

// Called when the trait collection changes (e.g., orientation change).
- (void)assistantAIMViewControllerDidChangeTraits:
    (AssistantAIMViewController*)viewController;

// Called when the user requests to see the AIM SRP logs.
- (void)assistantAIMViewControllerDidRequestSRPLogs:
    (AssistantAIMViewController*)viewController;

@end

@interface AssistantAIMViewController
    : UIViewControllerWithDisplayTracing <AssistantAIMConsumer>

// The delegate for this view controller.
@property(nonatomic, weak) id<AssistantAIMViewControllerDelegate> delegate;

// The mutator for this view controller.
@property(nonatomic, weak) id<AssistantAIMMutator> mutator;

// Adds the input view controller to this ViewController.
- (void)addInputViewController:
    (ComposeboxInputPlateViewController*)inputViewController;

// Adjusts the UI based on the percentage open of the container.
- (void)adjustForContainerOpenPercentage:(CGFloat)percentage;

// Returns YES if the scroll view should be paused for the given gesture,
// transitioning tracking priority to the container.
- (BOOL)shouldPauseScrollView:(UIScrollView*)scrollView
                   forGesture:(UIGestureRecognizer*)gesture
            isInLargestDetent:(BOOL)isInLargestDetent;

@end

#endif  // IOS_CHROME_BROWSER_COBROWSE_UI_ASSISTANT_AIM_VIEW_CONTROLLER_H_
