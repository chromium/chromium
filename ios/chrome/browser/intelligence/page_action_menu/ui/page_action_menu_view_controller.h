// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_ACTION_MENU_UI_PAGE_ACTION_MENU_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_ACTION_MENU_UI_PAGE_ACTION_MENU_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol PageActionMenuMutator;

// The view controller representing the presented page action menu UI.
@interface PageActionMenuViewController : UIViewController

// The mutator for communicating with the mediator.
@property(nonatomic, weak) id<PageActionMenuMutator> mutator;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_ACTION_MENU_UI_PAGE_ACTION_MENU_VIEW_CONTROLLER_H_
