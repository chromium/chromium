// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_PRESENTER_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_PRESENTER_H_

#import <UIKit/UIKit.h>

@protocol OmniboxPopupPositioner;

// The UI Refresh implementation of the popup presenter.
@interface OmniboxPopupPresenter : NSObject

// Updates appearance depending on the content size of the presented view
// controller by changing the visible height of the popup. When the popup was
// not previously shown, it will appear with "expansion" animation.
- (void)updateHeightAndAnimateAppearanceIfNecessary;
// Call this to hide the popup with animation.
- (void)animateCollapse;

- (instancetype)initWithPopupPositioner:(id<OmniboxPopupPositioner>)positioner
                    popupViewController:(UIViewController*)viewController
                              incognito:(BOOL)incognito;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_PRESENTER_H_
