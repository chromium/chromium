// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COBROWSE_UI_ASSISTANT_AIM_HISTORY_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_COBROWSE_UI_ASSISTANT_AIM_HISTORY_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import <vector>

#import "ios/chrome/browser/cobrowse/ui/assistant_aim_consumer.h"

@class AssistantAIMHistoryViewController;

// Protocol to handle interactions from the AssistantAIMHistoryViewController.
@protocol AssistantAIMHistoryViewControllerDelegate <NSObject>

// Called when the user taps the close (X) button or back button.
- (void)assistantAIMHistoryViewControllerDidTapDismiss:
    (AssistantAIMHistoryViewController*)viewController;

// Called when the user selects a specific history task.
- (void)assistantAIMHistoryViewController:
            (AssistantAIMHistoryViewController*)viewController
                      didSelectTaskWithId:(NSString*)taskId;

@end

// View Controller for presenting the "AI Mode History".
@interface AssistantAIMHistoryViewController : UIViewController

// Delegate for interactions.
@property(nonatomic, weak) id<AssistantAIMHistoryViewControllerDelegate>
    delegate;

// Updates the table with a new list of historical tasks.
- (void)updateHistoryItems:(const std::vector<AssistantAIMHistoryItem>&)items;

@end

#endif  // IOS_CHROME_BROWSER_COBROWSE_UI_ASSISTANT_AIM_HISTORY_VIEW_CONTROLLER_H_
