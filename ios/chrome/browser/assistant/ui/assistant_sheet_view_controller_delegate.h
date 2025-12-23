// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_SHEET_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_SHEET_VIEW_CONTROLLER_DELEGATE_H_

#import <Foundation/Foundation.h>

@class AssistantSheetViewController;

// Delegate for the AssistantSheetViewController.
@protocol AssistantSheetViewControllerDelegate <NSObject>

// Called when the close button is tapped.
- (void)assistantSheetViewControllerDidTapClose:
    (AssistantSheetViewController*)viewController;

@end

#endif  // IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_SHEET_VIEW_CONTROLLER_DELEGATE_H_
