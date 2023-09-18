// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_MEDIATOR_DELEGATE_H_

#import <UIKit/UIKit.h>

@class BaseGridMediator;
@class URLWithTitle;
namespace web {
class WebStateID;
}  // namespace web

// Delegate protocol for an object that can handle the action sheet that asks
// for confirmation from the tab grid.
// TODO(crbug.com/1457146): This delegate should be completely refactor.
@protocol GridMediatorDelegate <NSObject>

// Displays an action sheet at `anchor` confirming that selected `items` are
// going to be closed.
- (void)
    showCloseItemsConfirmationActionSheetWithBaseGridMediator:
        (BaseGridMediator*)baseGridMediator
                                                      itemIDs:
                                                          (const std::set<
                                                              web::WebStateID>&)
                                                              itemIDs
                                                       anchor:(UIBarButtonItem*)
                                                                  buttonAnchor;

// Displays a share menu for `items` at `anchor`.
- (void)baseGridMediator:(BaseGridMediator*)baseGridMediator
               shareURLs:(NSArray<URLWithTitle*>*)items
                  anchor:(UIBarButtonItem*)buttonAnchor;

// Dismisses presented popovers, if any.
- (void)dismissPopovers;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_MEDIATOR_DELEGATE_H_
