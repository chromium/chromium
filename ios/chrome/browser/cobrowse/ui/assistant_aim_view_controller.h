// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COBROWSE_UI_ASSISTANT_AIM_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_COBROWSE_UI_ASSISTANT_AIM_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/cobrowse/ui/assistant_aim_consumer.h"

@class AssistantAIMViewController;
@class ComposeboxInputPlateViewController;

// Delegate for the AssistantAIMViewController.
@protocol AssistantAIMViewControllerDelegate <NSObject>

// Called when the close button is tapped.
- (void)assistantAIMViewControllerDidTapClose:
    (AssistantAIMViewController*)viewController;

@end

@interface AssistantAIMViewController : UIViewController <AssistantAIMConsumer>

// The delegate for this view controller.
@property(nonatomic, weak) id<AssistantAIMViewControllerDelegate> delegate;

// Adds the input view controller to this ViewController.
- (void)addInputViewController:
    (ComposeboxInputPlateViewController*)inputViewController;

@end

#endif  // IOS_CHROME_BROWSER_COBROWSE_UI_ASSISTANT_AIM_VIEW_CONTROLLER_H_
