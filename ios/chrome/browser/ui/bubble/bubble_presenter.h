// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BUBBLE_BUBBLE_PRESENTER_H_
#define IOS_CHROME_BROWSER_UI_BUBBLE_BUBBLE_PRESENTER_H_

#import <UIKit/UIKit.h>
#import "ios/chrome/browser/ui/commands/help_commands.h"

@protocol BubblePresenterDelegate;
@class BubbleViewControllerPresenter;
class ChromeBrowserState;
@class LayoutGuideCenter;
@protocol ToolbarCommands;

// Object handling the presentation of the different bubbles tips. The class is
// holding all the bubble presenters.
@interface BubblePresenter : NSObject <HelpCommands>

// Initializes a BubblePresenter whose bubbles are presented on the
// `rootViewController`.
- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Used to display the new incognito tab tip in-product help promotion bubble.
@property(nonatomic, strong, readonly)
    BubbleViewControllerPresenter* incognitoTabTipBubblePresenter;

@property(nonatomic, weak) id<BubblePresenterDelegate> delegate;
@property(nonatomic, weak) UIViewController* rootViewController;
@property(nonatomic, weak) id<ToolbarCommands> toolbarHandler;
@property(nonatomic, strong) LayoutGuideCenter* layoutGuideCenter;

- (void)stop;

// Notifies the presenter that the user entered the tab switcher.
- (void)userEnteredTabSwitcher;

// Notifies the presenter that the tools menu has been displayed.
- (void)toolsMenuDisplayed;

// Presents a bubble associated with the Discover feed header's menu button.
- (void)presentDiscoverFeedHeaderTipBubble;

// Shows a relevant Follow help bubble while browsing a site, if applicable.
- (void)presentFollowWhileBrowsingTipBubble;

// Shows a help bubble to let the user know that they can change the default
// mode (Desktop/Mobile) of the websites.
- (void)presentDefaultSiteViewTipBubble;

// Presents a help bubble for What's New, if applicable.
- (void)presentWhatsNewBottomToolbarBubble;

// Presents a help bubble to inform the user that they can track the price of
// the item on the current website.
- (void)presentPriceNotificationsWhileBrowsingTipBubble;

@end

#endif  // IOS_CHROME_BROWSER_UI_BUBBLE_BUBBLE_PRESENTER_H_
