// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_PROTOTYPE_COMPOSEBOX_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_PROTOTYPE_COMPOSEBOX_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_animation_context_provider.h"
#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_composebox_consumer.h"
#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_composebox_mutator.h"

@protocol AIMPrototypeComposeboxMutator;
@class AIMPrototypeComposeboxViewController;
@protocol TextFieldViewContaining;

/// Delegate for the AIM prototype composebox view controller.
@protocol AIMPrototypeComposeboxViewControllerDelegate
/// Informs the delegate that a user did tap on the gallery button.
- (void)aimPrototypeViewControllerDidTapGalleryButton:
    (AIMPrototypeComposeboxViewController*)composeboxViewController;
/// Informs the delegate that a user did tap on the mic button.
- (void)aimPrototypeViewController:
            (AIMPrototypeComposeboxViewController*)composeboxViewController
                   didTapMicButton:(UIButton*)button;
/// Informs the delegate that a user did tap on the lens button.
- (void)aimPrototypeViewController:
            (AIMPrototypeComposeboxViewController*)composeboxViewController
                  didTapLensButton:(UIButton*)button;
/// Informs the delegate that a user did tap on the camera button.
- (void)aimPrototypeViewControllerDidTapCameraButton:
    (AIMPrototypeComposeboxViewController*)composeboxViewController;
/// Informs the delegate that a user did tap on the gallery button.
- (void)aimPrototypeViewControllerMayShowGalleryPicker:
    (AIMPrototypeComposeboxViewController*)composeboxViewController;
/// Informs the delegate that a user did tap on the file button.
- (void)aimPrototypeViewControllerDidTapFileButton:
    (AIMPrototypeComposeboxViewController*)composeboxViewController;
/// Informs the delegate that a user did tap on the attach tabs button.
- (void)aimPrototypeViewControllerDidTapAttachTabsButton:
    (AIMPrototypeComposeboxViewController*)composeboxViewController;
@end

/// View controller for the AIM prototype composebox.
@interface AIMPrototypeComposeboxViewController
    : UIViewController <AIMPrototypeAnimationContextProvider,
                        AIMPrototypeComposeboxConsumer>

@property(nonatomic, weak) id<AIMPrototypeComposeboxViewControllerDelegate>
    delegate;
@property(nonatomic, weak) id<AIMPrototypeComposeboxMutator> mutator;

/// Height of the input view.
@property(nonatomic, readonly) CGFloat inputHeight;

/// Sets the omnibox edit view.
- (void)setEditView:(UIView<TextFieldViewContaining>*)editView;

@end

#endif  // IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_PROTOTYPE_COMPOSEBOX_VIEW_CONTROLLER_H_
