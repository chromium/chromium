// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_UI_CONTEXTUAL_SHEET_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_UI_CONTEXTUAL_SHEET_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol ContextualSheetCommands;

// View controller for a custom sheet for the Contextual Panel.
@interface ContextualSheetViewController : UIViewController

// Command handler.
@property(nonatomic, weak) id<ContextualSheetCommands> contextualSheetHandler;

@end

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_UI_CONTEXTUAL_SHEET_VIEW_CONTROLLER_H_
