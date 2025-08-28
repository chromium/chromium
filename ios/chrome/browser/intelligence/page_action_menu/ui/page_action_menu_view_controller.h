// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_ACTION_MENU_UI_PAGE_ACTION_MENU_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_ACTION_MENU_UI_PAGE_ACTION_MENU_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/intelligence/page_action_menu/ui/page_action_menu_consumer.h"
#import "ios/chrome/browser/reader_mode/ui/reader_mode_options_consumer.h"

@protocol BWGCommands;
@protocol LensOverlayCommands;
@protocol PageActionMenuCommands;
@protocol PageActionMenuMutator;
@protocol PageActionMenuViewControllerDelegate;
@protocol ReaderModeCommands;

// The view controller representing the presented page action menu UI.
@interface PageActionMenuViewController
    : UIViewController <PageActionMenuConsumer, ReaderModeOptionsConsumer>

// The delegate for this view controller.
@property(nonatomic, weak) id<PageActionMenuViewControllerDelegate> delegate;

// Returns the appropriate detent value for a sheet presentation in `context`.
- (CGFloat)resolveDetentValueForSheetPresentation:
    (id<UISheetPresentationControllerDetentResolutionContext>)context;

// The mutator for communicating with the mediator.
@property(nonatomic, weak) id<PageActionMenuMutator> mutator;

// The handler for sending BWG commands.
@property(nonatomic, weak) id<BWGCommands> BWGHandler;

// The handler for sending page action menu commands.
@property(nonatomic, weak) id<PageActionMenuCommands> pageActionMenuHandler;

// The handler for sending lens overlay commands.
@property(nonatomic, weak) id<LensOverlayCommands> lensOverlayHandler;

// The handler for sending reader mode commands.
@property(nonatomic, weak) id<ReaderModeCommands> readerModeHandler;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_ACTION_MENU_UI_PAGE_ACTION_MENU_VIEW_CONTROLLER_H_
