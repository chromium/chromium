// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_RESULT_PAGE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_RESULT_PAGE_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/lens_overlay/ui/lens_result_page_consumer.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_toolbar_consumer.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_presenter.h"

@protocol LensToolbarMutator;
@protocol LensResultPageMutator;
@protocol TextFieldViewContaining;

/// View controller of the lens result page.
@interface LensResultPageViewController
    : UIViewController <LensResultPageConsumer,
                        LensToolbarConsumer,
                        OmniboxPopupPresenterDelegate>

/// Container for the web view.
@property(nonatomic, strong, readonly) UIView* webViewContainer;
/// Mutator of the lens omnibox.
@property(nonatomic, weak) id<LensToolbarMutator> toolbarMutator;
/// Mutator of the lens result page.
@property(nonatomic, weak) id<LensResultPageMutator> mutator;

/// Sets the omnibox edit view.
- (void)setEditView:(UIView<TextFieldViewContaining>*)editView;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_RESULT_PAGE_VIEW_CONTROLLER_H_
