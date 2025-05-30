// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_ACTION_MENU_UI_PAGE_ACTION_MENU_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_ACTION_MENU_UI_PAGE_ACTION_MENU_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol BWGCommands;
@protocol LensOverlayCommands;
@protocol PageActionMenuCommands;

// The view controller representing the presented page action menu UI.
@interface PageActionMenuViewController : UIViewController

// The handler for sending BWG commands.
@property(nonatomic, weak) id<BWGCommands> BWGHandler;

// The handler for sending page action menu commands.
@property(nonatomic, weak) id<PageActionMenuCommands> pageActionMenuHandler;

// The handler for sending lens overlay commands.
@property(nonatomic, weak) id<LensOverlayCommands> lensOverlayHandler;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_ACTION_MENU_UI_PAGE_ACTION_MENU_VIEW_CONTROLLER_H_
