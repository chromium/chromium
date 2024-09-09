// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_PRESENTER_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_PRESENTER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_omnibox_consumer.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_type.h"

@protocol ContentProviding;
@class LayoutGuideCenter;
@class OmniboxPopupPresenter;

@protocol OmniboxPopupPresenterDelegate

/// View to which the popup view should be added as subview.
- (UIView*)popupParentViewForPresenter:(OmniboxPopupPresenter*)presenter;

/// The view controller that will parent the popup.
- (UIViewController*)popupParentViewControllerForPresenter:
    (OmniboxPopupPresenter*)presenter;

/// Returns the background color for the popup to match the style of the
/// toolbar.
- (UIColor*)popupBackgroundColorForPresenter:(OmniboxPopupPresenter*)presenter;

/// Returns the layout guide name used to anchor the omnibox popup to the
/// omnibox textfield. If nil, the popup will be fully expanded inside of the
/// parent view, from `popupParentViewForPresenter`.
- (GuideName*)omniboxGuideNameForPresenter:(OmniboxPopupPresenter*)presenter;

/// Alert the delegate that the popup opened.
- (void)popupDidOpenForPresenter:(OmniboxPopupPresenter*)presenter;

/// Alert the delegate that the popup closed.
- (void)popupDidCloseForPresenter:(OmniboxPopupPresenter*)presenter;

@end

/// The UI Refresh implementation of the popup presenter.
/// TODO(crbug.com/40616000): This class should be refactored to handle a nil
/// delegate.
@interface OmniboxPopupPresenter : NSObject <ToolbarOmniboxConsumer>

/// Whether the popup is open
@property(nonatomic, assign, getter=isOpen) BOOL open;

/// Uses the popup's intrinsic content size to add or remove the popup view
/// if necessary. The animation changes depending on:
/// `isFocusingOmnibox`: Omnibox is being focused.
- (void)updatePopupOnFocus:(BOOL)isFocusingOmnibox;

/// Tells the presenter to update, following a trait collection change.
- (void)updatePopupAfterTraitCollectionChange;

- (instancetype)
    initWithPopupPresenterDelegate:
        (id<OmniboxPopupPresenterDelegate>)presenterDelegate
               popupViewController:
                   (UIViewController<ContentProviding>*)viewController
                 layoutGuideCenter:(LayoutGuideCenter*)layoutGuideCenter
                         incognito:(BOOL)incognito;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_PRESENTER_H_
