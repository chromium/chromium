// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_UI_POPUP_OMNIBOX_POPUP_PRESENTER_H_
#define IOS_CHROME_BROWSER_OMNIBOX_UI_POPUP_OMNIBOX_POPUP_PRESENTER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/omnibox/public/omnibox_presentation_context.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/toolbar/ui_bundled/public/toolbar_omnibox_consumer.h"
#import "ios/chrome/browser/toolbar/ui_bundled/public/toolbar_type.h"

@class LayoutGuideCenter;
@class OmniboxPopupPresenter;
@class OmniboxPopupViewController;

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

/// The  presenter for the omnibox popup (UI with autocomplete suggestions).
/// Positions the `popupViewController` on the screen using the
/// `presenterDelegate`. Displays the necessary chrome (backgrounds, rounded
/// corners, ...) The presentation differs between phones and tablets.
@interface OmniboxPopupPresenter : NSObject <ToolbarOmniboxConsumer>

/// Whether the popup is open
@property(nonatomic, assign, getter=isOpen) BOOL open;

/// The container view for the popup.
@property(nonatomic, readonly) UIView* popupContainerView;

/// Stores the height of the bottom omnibox with respect to the keyboard
/// height.
@property(nonatomic, assign) CGFloat keyboardAttachedBottomOmniboxHeight;

/// Whether to show the omnibox in the bottom when the popup is open.
@property(nonatomic, readonly) BOOL useBottomOmniboxInPopup;

/// Uses the popup's intrinsic content size to add or remove the popup view
/// if necessary. The animation changes depending on:
/// `isFocusingOmnibox`: Omnibox is being focused.
- (void)updatePopupOnFocus:(BOOL)isFocusingOmnibox;

/// Tells the presenter to update, following a trait collection change.
- (void)updatePopupAfterTraitCollectionChange;

/// Sets additional insets on the popup.
- (void)setAdditionalVerticalContentInset:
    (UIEdgeInsets)additionalVerticalContentInset;

- (instancetype)
    initWithPopupPresenterDelegate:
        (id<OmniboxPopupPresenterDelegate>)presenterDelegate
               popupViewController:(OmniboxPopupViewController*)viewController
                 layoutGuideCenter:(LayoutGuideCenter*)layoutGuideCenter
                         incognito:(BOOL)incognito
               presentationContext:
                   (OmniboxPresentationContext)presentationContext;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_UI_POPUP_OMNIBOX_POPUP_PRESENTER_H_
