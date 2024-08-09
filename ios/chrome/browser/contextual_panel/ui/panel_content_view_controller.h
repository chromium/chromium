// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_UI_PANEL_CONTENT_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_UI_PANEL_CONTENT_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/contextual_panel/ui/panel_content_consumer.h"

@protocol ContextualSheetCommands;
@protocol ContextualSheetDisplayController;
@class PanelBlockData;
@protocol TraitCollectionChangeDelegate;

@protocol PanelContentViewControllerMetricsDelegate

// Returns the name of the current entrypoint info block.
- (NSString*)entrypointInfoBlockName;

// Returns whether the entrypoint was a loud entrypoint. This includes both
// the large entrypoint chip and the IPH.
- (BOOL)wasLoudEntrypoint;

@end

// A view controller to display the contents of the Contextual Panel.
@interface PanelContentViewController : UIViewController <PanelContentConsumer>

// The handler for ContextualSheetCommands.
@property(nonatomic, weak) id<ContextualSheetCommands>
    contextualSheetCommandHandler;

@property(nonatomic, weak) id<ContextualSheetDisplayController>
    sheetDisplayController;

@property(nonatomic, weak) id<PanelContentViewControllerMetricsDelegate>
    metricsDelegate;

// Delegate to inform about trait collection changes in this view controller.
@property(nonatomic, weak) id<TraitCollectionChangeDelegate>
    traitCollectionDelegate;

// Updates the current block data.
- (void)setPanelBlocks:(NSArray<PanelBlockData*>*)panelBlocks;

@end

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_UI_PANEL_CONTENT_VIEW_CONTROLLER_H_
