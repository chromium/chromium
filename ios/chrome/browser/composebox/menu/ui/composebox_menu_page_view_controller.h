// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_MENU_UI_COMPOSEBOX_MENU_PAGE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_MENU_UI_COMPOSEBOX_MENU_PAGE_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_consumer.h"
#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_mutator.h"

enum class ComposeboxMenuPageType {
  kFullMenu = 0,
  kMainPage = 1,
  kSecondaryPage = 2,
};

@class ComposeboxMenuPageViewController;

// Delegate for the menu page view controller.
@protocol ComposeboxMenuPageViewControllerDelegate <NSObject>
// Called when the menu requests the secondary page.
- (void)composeboxMenuPageViewControllerDidRequestSecondaryMenu:
    (ComposeboxMenuPageViewController*)composeboxMenuPageViewController;

@end

@interface ComposeboxMenuPageViewController
    : UIViewController <ComposeboxMenuConsumer, UICollectionViewDelegate>

// The collection view displaying the composebox menu.
@property(nonatomic, readonly) UICollectionView* collectionView;

// The mutator for this menu UI.
@property(nonatomic, weak) id<ComposeboxMenuMutator> mutator;

// The delegate of the menu page.
@property(nonatomic, weak) id<ComposeboxMenuPageViewControllerDelegate>
    delegate;

// The type of the page, influencing the visible options.
@property(nonatomic, readonly) ComposeboxMenuPageType menuPageType;

// Creates a new instance of this type.
- (instancetype)initWithPageType:(ComposeboxMenuPageType)menuPageType;

// The height necessary to fit the content without scrolling.
- (CGFloat)contentHeightToFit;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_MENU_UI_COMPOSEBOX_MENU_PAGE_VIEW_CONTROLLER_H_
