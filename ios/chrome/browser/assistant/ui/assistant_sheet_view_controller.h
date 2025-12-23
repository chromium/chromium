// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_SHEET_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_SHEET_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/assistant/ui/assistant_sheet_consumer.h"
#import "ios/chrome/browser/assistant/ui/assistant_sheet_view_controller_delegate.h"

// View Controller for the Assistant Sheet.
@interface AssistantSheetViewController
    : UIViewController <AssistantSheetConsumer>

// Delegate for handling actions.
@property(nonatomic, weak) id<AssistantSheetViewControllerDelegate> delegate;

// The view to anchor to. If nil, falls back to the bottom of the parent view.
@property(nonatomic, weak) UIView* anchorView;

// Whether to anchor to the bottom of the view (YES) or the top (NO).
// Defaults to NO.
@property(nonatomic, assign) BOOL anchorToBottom;

@end

#endif  // IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_SHEET_VIEW_CONTROLLER_H_
