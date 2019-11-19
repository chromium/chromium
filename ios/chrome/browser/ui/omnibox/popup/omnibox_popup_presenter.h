// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_PRESENTER_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_PRESENTER_H_

#import <UIKit/UIKit.h>

@class OmniboxPopupPresenter;

@protocol OmniboxPopupPresenterDelegate

// View to which the popup view should be added as subview.
- (UIView*)popupParentViewForPresenter:(OmniboxPopupPresenter*)presenter;

// The view controller that will parent the popup.
- (UIViewController*)popupParentViewControllerForPresenter:
    (OmniboxPopupPresenter*)presenter;

// Alert the delegate that the popup opened.
- (void)popupDidOpenForPresenter:(OmniboxPopupPresenter*)presenter;

// Alert the delegate that the popup closed.
- (void)popupDidCloseForPresenter:(OmniboxPopupPresenter*)presenter;

@end

// The UI Refresh implementation of the popup presenter.
// TODO(crbug.com/936833): This class should be refactored to handle a nil
// delegate.
@interface OmniboxPopupPresenter : NSObject

// Whether the popup is open
@property(nonatomic, assign, getter=isOpen) BOOL open;

// Uses the popup's intrinsic content size to add or remove the popup view
// if necessary.
- (void)updatePopup;

- (instancetype)initWithPopupPresenterDelegate:
                    (id<OmniboxPopupPresenterDelegate>)presenterDelegate
                           popupViewController:(UIViewController*)viewController
                                     incognito:(BOOL)incognito;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_PRESENTER_H_
