// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_UI_PANEL_CONTENT_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_UI_PANEL_CONTENT_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol ContextualSheetCommands;
@class PanelBlockData;

// A view controller to display the contents of the Contextual Panel.
@interface PanelContentViewController : UIViewController

// The handler for ContextualSheetCommands.
@property(nonatomic, weak) id<ContextualSheetCommands>
    contextualSheetCommandHandler;

// Updates the current block data.
- (void)setPanelBlocks:(NSArray<PanelBlockData*>*)panelBlocks;

@end

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_UI_PANEL_CONTENT_VIEW_CONTROLLER_H_
