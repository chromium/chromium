// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_COORDINATOR_H_

#import <UIKit/UIKit.h>

#include <memory>

@class CommandDispatcher;
@protocol OmniboxPopupPresenterDelegate;
@protocol OmniboxFocuser;
class OmniboxPopupViewIOS;

namespace ios {
class ChromeBrowserState;
}
class WebStateList;

// Coordinator for the Omnibox Popup.
@interface OmniboxPopupCoordinator : NSObject

- (instancetype)initWithPopupView:
    (std::unique_ptr<OmniboxPopupViewIOS>)popupView NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// BrowserState.
@property(nonatomic, assign) ios::ChromeBrowserState* browserState;
// Positioner for the popup.
@property(nonatomic, weak) id<OmniboxPopupPresenterDelegate> presenterDelegate;
// Whether this coordinator has results to show.
@property(nonatomic, assign, readonly) BOOL hasResults;
// Whether the popup is open.
@property(nonatomic, assign, readonly) BOOL isOpen;
// The dispatcher for this view controller.
@property(nonatomic, readwrite, weak) CommandDispatcher* dispatcher;
// The web state list this coordinator is handling.
@property(nonatomic, assign) WebStateList* webStateList;

- (void)start;
- (void)stop;

// Presents the shortcuts feature if the current page allows for it and update
// the popup.
- (void)presentShortcutsIfNecessary;

// Dismisses the shortcuts feature and update the popup.
- (void)dismissShortcuts;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_COORDINATOR_H_
