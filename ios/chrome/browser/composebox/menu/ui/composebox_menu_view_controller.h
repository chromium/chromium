// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_MENU_UI_COMPOSEBOX_MENU_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_MENU_UI_COMPOSEBOX_MENU_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_consumer.h"
#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_mutator.h"

@class ComposeboxMenuViewController;

// Delegate for a menu.
@protocol ComposeboxMenuViewControllerDelegate

// Called when the menu requests closing.
- (void)composeboxMenuViewControllerDidChangeMenuPage:
    (ComposeboxMenuViewController*)composeboxMenuViewController;

// Called when the menu requests closing.
- (void)composeboxMenuViewControllerDidRequestClose:
    (ComposeboxMenuViewController*)composeboxMenuViewController;

@end

// View controller for the composebox menu.
@interface ComposeboxMenuViewController
    : UINavigationController <ComposeboxMenuConsumer>

// The mutator for this menu UI.
@property(nonatomic, weak) id<ComposeboxMenuMutator> mutator;

// The delegate for this menu UI.
@property(nonatomic, weak) id<ComposeboxMenuViewControllerDelegate>
    menuDelegate;

@end

// Represents a page of the composebox menu.
@interface ComposeboxMenuPageViewController : UIViewController

// The collection view displaying the composebox menu.
@property(nonatomic, readonly) UICollectionView* collectionView;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_MENU_UI_COMPOSEBOX_MENU_VIEW_CONTROLLER_H_
