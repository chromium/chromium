// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_UI_CONTEXTUAL_SHEET_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_UI_CONTEXTUAL_SHEET_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/contextual_panel/ui/contextual_sheet_display_controller.h"

@protocol ContextualSheetCommands;
@class ContextualSheetViewController;
@protocol TraitCollectionChangeDelegate;

// View controller for a custom sheet for the Contextual Panel.
@interface ContextualSheetViewController
    : UIViewController <ContextualSheetDisplayController>

// Command handler.
@property(nonatomic, weak) id<ContextualSheetCommands> contextualSheetHandler;

// Delegate to inform when the trait collection changes.
@property(nonatomic, weak) id<TraitCollectionChangeDelegate>
    traitCollectionDelegate;

// Animates the appearance of the sheet after the controller has been added to
// its parent.
- (void)animateAppearance;

@end

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_UI_CONTEXTUAL_SHEET_VIEW_CONTROLLER_H_
