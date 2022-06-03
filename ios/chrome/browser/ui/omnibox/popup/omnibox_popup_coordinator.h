// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

#include <memory>

@protocol OmniboxPopupPresenterDelegate;
@protocol OmniboxCommands;
class OmniboxPopupViewIOS;

// Coordinator for the Omnibox Popup.
@interface OmniboxPopupCoordinator : ChromeCoordinator

- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                     popupView:(std::unique_ptr<OmniboxPopupViewIOS>)popupView
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Positioner for the popup.
@property(nonatomic, weak) id<OmniboxPopupPresenterDelegate> presenterDelegate;
// Whether this coordinator has results to show.
@property(nonatomic, assign, readonly) BOOL hasResults;
// Whether the popup is open.
@property(nonatomic, assign, readonly) BOOL isOpen;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_COORDINATOR_H_
