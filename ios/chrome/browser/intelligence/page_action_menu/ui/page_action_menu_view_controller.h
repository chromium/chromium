// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_ACTION_MENU_UI_PAGE_ACTION_MENU_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_ACTION_MENU_UI_PAGE_ACTION_MENU_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol BWGCommands;
@protocol LensOverlayCommands;
@protocol PageActionMenuCommands;
@protocol ReaderModeCommands;

// The view controller representing the presented page action menu UI.
@interface PageActionMenuViewController : UIViewController

// Initializes the view controller adapted to whether Reader Mode is currently
// active.
- (instancetype)initWithReaderModeActive:(BOOL)readerModeActive
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

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
